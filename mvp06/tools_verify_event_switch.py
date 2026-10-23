"""Generate an original-x86 comparison for the idle-demo event switch.

Run after tools_patch_boundaries.py. No game bytes are stored in this source;
the private harness loads the user's XBE and tests controlled callees.
"""
import argparse
import json
from pathlib import Path
from tools.recomp import config
from tools.recomp.translator import FunctionTranslator


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--install', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    project = root / 'mvp06'
    xbe = project / 'game/default.xbe'
    config.configure_from_xbe(str(xbe))
    db = {int(f['start'], 16): dict(f, end=int(f['end'], 16))
          for f in json.loads((root / 'tools/disasm/output/functions.json').read_text())}
    start, end = 0x3DD40, 0x3DDF6
    assert db[start]['end'] == end
    translator = FunctionTranslator(xbe.read_bytes(), db)
    instructions, _ = translator.decode_function(start, end)
    assert all(a.end_address <= b.address for a, b in zip(instructions, instructions[1:]))
    assert instructions[-1].address == 0x3DDF3 and instructions[-1].is_ret
    body = translator.translate_function(start, db[start])
    for target in (0x3DD72, 0x3DD81, 0x3DDB1, 0x3DDC0, 0x3DDEC, 0x3DDF2):
        assert f'goto loc_{target:08X};' in body, hex(target)
        assert f'loc_{target:08X}: ;' in body, hex(target)
    assert 'sub_0003DDF2()' not in body
    assert 'dead code' not in body and 'TODO' not in body
    build = project / 'build'
    (build / 'event_switch_body.c').write_text(body)
    template = (project / 'tests/event_switch.c.in').read_text()
    harness_body = body.replace('0x3DE10', '0x230000D0').replace('0x3DDF8', '0x230000B8')
    harness_body = harness_body.replace('0x5F7C78', '0x23001000')
    template = template.replace('@BODY@', harness_body)
    template = template.replace('@OFFSET@', str(config.va_to_file_offset(start)))
    template = template.replace('@CONSTANT@', str(config.va_to_file_offset(0x5F7C78)))
    (build / 'event_switch_native.c').write_text(template)
    if args.install:
        path = project / 'src/recomp/gen/recomp_0001.c'
        source = path.read_text()
        first = source.index('/**\n * sub_0003DD40\n')
        last = source.index('/**', first + 3)
        (build / 'event_switch_before.c').write_text(source[first:last])
        path.write_text(source[:first] + body.rstrip() + '\n\n' + source[last:])
    print(f'{len(instructions)} nonoverlapping instructions; all six switch arms and RET 16 retained')


if __name__ == '__main__':
    main()
