"""Compare lifted float LAHF/branches with actual x86 flags, including NaNs."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

import pytest

from .disasm import BasicBlock, Instruction, Operand
from .lifter import Lifter, lift_basic_block, _make_condition
from .translator import FunctionTranslator


def test_lahf_arithmetic_requests_carry_snapshot():
    decoded = [Instruction(0,2,'add','al,cl','00c8'),
               Instruction(2,1,'lahf','','9f')]
    assert FunctionTranslator._function_needs_cf(decoded)
    assert not FunctionTranslator._function_needs_cf([
        Instruction(0,3,'ucomiss','xmm0,xmm1','0f2ec1'),decoded[1]])


def test_float_lahf_native_flags():
    cc = shutil.which('clang') or shutil.which('gcc')
    if not cc and Path(r'C:\Program Files\LLVM\bin\clang.exe').exists():
        cc = r'C:\Program Files\LLVM\bin\clang.exe'
    cmake = shutil.which('cmake') if os.name == 'nt' else None
    if not cc and not cmake:
        pytest.skip('C compiler unavailable')
    ops = [Operand(type='reg', reg='xmm0'), Operand(type='reg', reg='xmm1')]
    bb = BasicBlock(0, [Instruction(0, 1, 'lahf', '', '9f')])
    source = r'''
#include <stdint.h>
#include <math.h>
#include <string.h>
#define RECOMP_FCMP(a,b) (((a)!=(a)||(b)!=(b))?2:(a)<(b)?-1:(a)>(b)?1:0)
#if defined(_MSC_VER) && !defined(__clang__)
#include <windows.h>
/* MSVC x64 has no inline assembler. Execute just the two measured instruction
 * sequences in a private page; no game or process memory is involved. */
typedef unsigned (*FlagFn)(double,double);
static FlagFn code_function(const unsigned char *code, unsigned n) {
  void *p=VirtualAlloc(0,n,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
  DWORD old;
  if(!p) ExitProcess(90);
  memcpy(p,code,n);
  if(!VirtualProtect(p,n,PAGE_EXECUTE_READ,&old)) ExitProcess(91);
  FlushInstructionCache(GetCurrentProcess(),p,n);
  return (FlagFn)p;
}
static unsigned native_sse(double a,double b) {
  static FlagFn fn;
  const unsigned char code[]={0x66,0x0F,0x2E,0xC1,0x9F,0x0F,0xB6,0xC4,0xC3};
  if(!fn) fn=code_function(code,sizeof code);
  return fn(a,b);
}
static unsigned native_x87(double a,double b) {
  static FlagFn fn;
  const unsigned char code[]={0x48,0x83,0xEC,0x10,
    0xF2,0x0F,0x11,0x0C,0x24,0xF2,0x0F,0x11,0x44,0x24,0x08,
    0xDD,0x04,0x24,0xDD,0x44,0x24,0x08,0xDF,0xE9,0xDD,0xD8,
    0x9F,0x0F,0xB6,0xC4,0x48,0x83,0xC4,0x10,0xC3};
  if(!fn) fn=code_function(code,sizeof code);
  return fn(a,b);
}
static unsigned native_add8(unsigned a,unsigned b) {
  typedef unsigned (*UintFn)(unsigned,unsigned);
  static UintFn fn;
  const unsigned char code[]={0x8B,0xC1,0x00,0xD0,0x9F,0x0F,0xB6,0xC4,0xC3};
  if(!fn) fn=(UintFn)code_function(code,sizeof code);
  return fn(a,b);
}
#else
static unsigned native_sse(double a, double b) {
  unsigned flags;
  __asm__ volatile("ucomisd %2,%1; lahf" : "=a"(flags) : "x"(a),"x"(b) : "cc");
  return (flags >> 8) & 255;
}
static unsigned native_x87(double a, double b) {
  unsigned flags;
  __asm__ volatile("fldl %2; fldl %1; fucomip %%st(1),%%st; fstp %%st(0); lahf"
                   : "=a"(flags) : "m"(a),"m"(b) : "st","cc");
  return (flags >> 8) & 255;
}
static unsigned native_add8(unsigned a,unsigned b) {
  unsigned flags;
  __asm__ volatile("movb %b1,%%al; addb %b2,%%al; lahf"
                   : "=&a"(flags) : "q"(a),"q"(b) : "cc");
  return (flags >> 8) & 255;
}
#endif
'''
    add = Instruction(0,2,'add','al,dl','00d0')
    add.operands = [Operand(type='reg',reg='al'),Operand(type='reg',reg='dl')]
    lifter = Lifter()
    lifter.needs_cf = True
    code, _ = lift_basic_block(lifter, BasicBlock(0,[add,bb.instructions[0]]))
    source += '''
#define LO8(x) ((uint8_t)(x))
#define HI8(x) ((uint8_t)((x)>>8))
#define SET_LO8(r,v) ((r)=((r)&0xFFFFFF00u)|((uint32_t)(v)&255u))
static unsigned parity8(unsigned x) {
  x^=x>>4; x^=x>>2; x^=x>>1; return !(x&1);
}
#define RECOMP_PARITY8(x) parity8(x)
static unsigned lifted_add8(unsigned a,unsigned b) {
  uint32_t eax=0xABCD0000u|a,edx=b,_fa=0,_fb=0;
  int32_t _fas=0,_fbs=0;
  int _cf=0;
''' + '\n'.join(code) + '''
  if((eax&0xFFFF0000u)!=0xABCD0000u) return 0x100;
  return (eax>>8)&255;
}
'''
    for name, kind, operands in [('sse', 'ucomisd', ops), ('x87', 'fucomip', [])]:
        code, _ = lift_basic_block(Lifter(), bb, flag_state=(kind, operands))
        assert '/* lahf (' in '\n'.join(code)
        source += f'''static unsigned lifted_{name}(double _fca,double _fcb) {{
  unsigned eax=0xABCDEF12u;
  int g_fp_cmp=RECOMP_FCMP(_fca,_fcb);
  {''.join(code)}
  if ((eax & 0xFFFF00FFu)!=0xABCD0012u) return 0x100;
  return (eax >> 8) & 255;
}}
'''
        for jcc, mask, expected in [('je', 0x40, '!=0'), ('jne', 0x40, '==0'),
                                    ('jb', 1, '!=0'), ('jae', 1, '==0'),
                                    ('jbe', 0x41, '!=0'), ('ja', 0x41, '==0'),
                                    ('jp', 4, '!=0'), ('jnp', 4, '==0')]:
            expr = _make_condition(jcc, kind, operands)[0]
            source += f'''static int branch_{name}_{jcc}(double _fca,double _fcb) {{
 int g_fp_cmp=RECOMP_FCMP(_fca,_fcb);
 return !!({expr}) == !!((native_{name}(_fca,_fcb)&{mask}){expected});
}}
'''
    source += '''
int main(void) {
  for(unsigned a=0;a<256;a++) for(unsigned b=0;b<256;b++)
    if(lifted_add8(a,b)!=native_add8(a,b)) return 4;
  const double v[]={0.0,-0.0,1.0,-1.0,1e-310,-1e-310,INFINITY,-INFINITY,NAN};
  for(unsigned i=0;i<9;i++) for(unsigned j=0;j<9;j++) {
    double a=v[i],b=v[j];
    if(lifted_sse(a,b)!=native_sse(a,b)) return 1;
    if(lifted_x87(a,b)!=native_x87(a,b)) return 2;
'''
    for name in ('sse', 'x87'):
        for jcc in ('je', 'jne', 'jb', 'jae', 'jbe', 'ja', 'jp', 'jnp'):
            source += f'    if(!branch_{name}_{jcc}(a,b)) return 3;\n'
    source += '  }\n  return 0;\n}\n'
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / 'flags.c'
        path.write_text(source)
        exe = Path(tmp) / 'flags.exe'
        if cc:
            command = [cc, '-O2', str(path), '-o', str(exe)]
            if os.name != 'nt':
                command.append('-lm')
            result = subprocess.run(command, capture_output=True, text=True)
            assert result.returncode == 0, result.stderr
        else:
            (Path(tmp)/'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.20)\nproject(flags C)\n'
                'add_executable(flags flags.c)\n')
            result = subprocess.run([cmake,'-S',tmp,'-B',str(Path(tmp)/'build'),'-A','x64'],
                                    capture_output=True,text=True)
            assert result.returncode == 0, result.stdout+result.stderr
            result = subprocess.run([cmake,'--build',str(Path(tmp)/'build'),'--config','Release'],
                                    capture_output=True,text=True)
            assert result.returncode == 0, result.stdout+result.stderr
            exe = Path(tmp)/'build/Release/flags.exe'
        result = subprocess.run([str(exe)], capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
