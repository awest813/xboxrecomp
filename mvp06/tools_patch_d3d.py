"""Restore the original XDK interrupt and publication hooks after each lift.

Three definition renames, three PCRTC W1C stores and five observations change. Generated calls
and dispatch entries retain their original symbols so direct calls also use
the wrapper in src/recomp_manual.c. No game code is embedded in this script.
"""
import argparse
from pathlib import Path


def body_span(source, va):
    names = (f'void sub_{va:08X}(void)', f'void sub_{va:08X}_gen(void)')
    starts = [source.index(name) for name in names if name in source]
    assert len(starts) == 1, f'{va:08X}: expected one definition'
    start = starts[0]
    first = source.index('{', start)
    depth = 0
    for end in range(first, len(source)):
        depth += (source[end] == '{') - (source[end] == '}')
        if not depth:
            return start, end + 1
    raise AssertionError(f'{va:08X}: incomplete definition')


def patch(gen, check=False):
    stores = {
        0x57D710: [('MEM32(esi + 0x1DD0) = eax;', 'mvp06_d3d_wait_clear(esi, edi, eax);'),
                    ('MEM32(edi + 0x1C) = 5;', 'mvp06_d3d_wait_publish(edi);')],
        0x580B60: [('MEM32(edi + 0x600100) = ecx;', 'mvp06_nv2a_vblank_ack(edi, ecx);')],
        0x581310: [('MEM32(edi + 0x600100) = 1;', 'mvp06_nv2a_vblank_ack(edi, 1);')],
        0x5844BC: [('MEM32(eax + 0x600100) = ecx;', 'mvp06_nv2a_vblank_ack(eax, ecx);')],
    }
    renames = {0x57D710, 0x580B60, 0x5810D0}
    targets = {*renames, *stores}
    found = set()
    for path in sorted(gen.glob('recomp_*.c')):
        source = path.read_text()
        updated = source
        needs_decl = False
        for va in sorted(targets):
            if f'void sub_{va:08X}(void)' not in updated and f'void sub_{va:08X}_gen(void)' not in updated:
                continue
            assert va not in found, f'{va:08X}: duplicate definition'
            found.add(va)
            start, end = body_span(updated, va)
            body = updated[start:end]
            if va in renames:
                body = body.replace(f'void sub_{va:08X}(void)', f'void sub_{va:08X}_gen(void)', 1)
            if va in stores:
                for original, hook in stores[va]:
                    assert body.count(original) + body.count(hook) == 1, f'{va:08X}: hooked store changed'
                    body = body.replace(original, hook, 1)
                needs_decl = True
            if va == 0x57D710:
                for site, flags in ((0x57D759,0),(0x57D7A9,0),(0x57D7EF,1)):
                    call = f'PUSH32(esp, 0x{site:08X}u); RECOMP_ABI_CALL(0x0057D3C0u, sub_0057D3C0); /* call 0x0057D3C0 */'
                    observation = f'    mvp06_d3d_wait_get(esi, eax, {flags}); /* observe {site:08X} */'
                    assert body.count(call)==1, f'{va:08X}: GET call changed'
                    if observation not in body:
                        body = body.replace(call,call+'\n'+observation,1)
            updated = updated[:start] + body + updated[end:]
            print(f'{va:08X}: D3D hook verified in {path.name}')
        if needs_decl:
            for decl in ('extern void mvp06_nv2a_vblank_ack(uint32_t base, uint32_t value);',
                         'extern void mvp06_d3d_wait_clear(uint32_t dev, uint32_t patch, uint32_t value);',
                         'extern void mvp06_d3d_wait_publish(uint32_t patch);',
                         'extern void mvp06_d3d_wait_get(uint32_t dev, uint32_t returned, uint32_t flags);'):
                if decl not in updated:
                    updated = updated.replace('#include "recomp_funcs.h"', '#include "recomp_funcs.h"\n' + decl, 1)
        if updated != source:
            assert not check, f'{path}: hooks need reapplying'
            path.write_text(updated)
    assert found == targets, f'Missing D3D definitions: {targets - found}'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gen-dir', type=Path, default=Path(__file__).resolve().parent/'src/recomp/gen')
    parser.add_argument('--check', action='store_true', help='Verify hooks without changing generated files')
    args = parser.parse_args()
    patch(args.gen_dir, args.check)
