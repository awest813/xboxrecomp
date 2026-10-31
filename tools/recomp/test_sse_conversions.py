"""Scalar SSE conversions against hardware rounding and integer-indefinite."""
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile

import pytest

from .disasm import Instruction, Operand
from .lifter import Lifter


def conversion(mnemonic, memory=False):
    double = 'sd' in mnemonic
    source = (Operand(type='mem', mem_base='ecx', mem_size=8 if double else 4)
              if memory else Operand(type='reg', reg='xmm0'))
    insn = Instruction(0, 4, mnemonic, '', '')
    insn.operands = [Operand(type='reg', reg='eax'), source]
    return '\n'.join(Lifter().lift_instruction(insn))


def test_scalar_sse_conversions_native():
    if platform.machine().lower() not in ('amd64', 'x86_64', 'x86', 'i386', 'i686'):
        pytest.skip('requires x86 SSE conversion reference')
    cc = shutil.which('clang') or shutil.which('gcc')
    if not cc and Path(r'C:\Program Files\LLVM\bin\clang.exe').exists():
        cc = r'C:\Program Files\LLVM\bin\clang.exe'
    cmake = shutil.which('cmake')
    if not cc and not (os.name == 'nt' and cmake):
        pytest.skip('C compiler unavailable')
    header = Path(__file__).resolve().parents[2] / 'templates/runtime/recomp_types.h'
    source = f'#define RECOMP_GENERATED_CODE\n#include "{header.as_posix()}"\n' + r'''
#include <stdio.h>
RECOMP_TLS uint32_t g_eax;
RECOMP_TLS RecompXmm g_xmm0;
#if defined(_MSC_VER) && !defined(__clang__)
#include <windows.h>
static void *code_function(unsigned char prefix,unsigned char opcode) {
  unsigned char code[]={prefix,0x0F,opcode,0xC0,0xC3};
  DWORD old;void *p=VirtualAlloc(0,sizeof code,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
  if(!p) ExitProcess(90);
  memcpy(p,code,sizeof code);
  if(!VirtualProtect(p,sizeof code,PAGE_EXECUTE_READ,&old)) ExitProcess(91);
  FlushInstructionCache(GetCurrentProcess(),p,sizeof code);return p;
}
typedef int32_t (*SingleFn)(float);
typedef int32_t (*DoubleFn)(double);
static int32_t native_ss(float v,int truncate) {
  static SingleFn fn[2];if(!fn[truncate])fn[truncate]=(SingleFn)code_function(0xF3,truncate?0x2C:0x2D);
  return fn[truncate](v);
}
static int32_t native_sd(double v,int truncate) {
  static DoubleFn fn[2];if(!fn[truncate])fn[truncate]=(DoubleFn)code_function(0xF2,truncate?0x2C:0x2D);
  return fn[truncate](v);
}
#else
static int32_t native_ss(float v,int truncate) {
  int32_t r;
  if(truncate)__asm__ volatile("cvttss2si %1,%0":"=r"(r):"x"(v):"memory");
  else __asm__ volatile("cvtss2si %1,%0":"=r"(r):"x"(v):"memory");
  return r;
}
static int32_t native_sd(double v,int truncate) {
  int32_t r;
  if(truncate)__asm__ volatile("cvttsd2si %1,%0":"=r"(r):"x"(v):"memory");
  else __asm__ volatile("cvtsd2si %1,%0":"=r"(r):"x"(v):"memory");
  return r;
}
#endif
'''
    for mnemonic in ('cvtss2si', 'cvttss2si', 'cvtsd2si', 'cvttsd2si'):
        double = 'sd' in mnemonic
        code = conversion(mnemonic)
        assert ('xmm0.d[0]' if double else 'xmm0.f[0]') in code
        mem = conversion(mnemonic, memory=True)
        assert ('MEMD(ecx)' if double else 'MEMF(ecx)') in mem
        source += (f'static int32_t lifted_{mnemonic}({"double" if double else "float"} v) {{\n'
                   f'  xmm0.{"d" if double else "f"}[0]=v;\n{code}\nreturn (int32_t)eax;\n}}\n')
    source += r'''
int main(void) {
  const double special[]={0.0,-0.0,0.5,-0.5,1.5,-1.5,2.5,-2.5,
    2147483647.0,2147483647.5,2147483648.0,-2147483648.0,-2147483648.5,
    1e-310,-1e-310,INFINITY,-INFINITY,NAN};
  unsigned saved=_mm_getcsr();
  for(unsigned mode=0;mode<4;mode++) {
    _mm_setcsr(0x1F80u|(mode<<13));
    for(unsigned i=0;i<2066;i++) {
      double v=i<18?special[i]:((int)i-1042)*0.25;
      if(lifted_cvtss2si((float)v)!=native_ss((float)v,0) ||
         lifted_cvttss2si((float)v)!=native_ss((float)v,1) ||
         lifted_cvtsd2si(v)!=native_sd(v,0) ||
         lifted_cvttsd2si(v)!=native_sd(v,1)) {
        fprintf(stderr,"conversion mismatch mode=%u i=%u v=%g\n",mode,i,v);return 1;
      }
    }
  }
  _mm_setcsr(saved);return 0;
}
'''
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / 'conversions.c'
        path.write_text(source)
        exe = Path(tmp) / 'conversions.exe'
        if cc:
            command = [cc, '-O2', '-msse2', str(path), '-o', str(exe)]
            if os.name != 'nt':
                command.append('-lm')
            result = subprocess.run(command, capture_output=True, text=True)
            assert result.returncode == 0, result.stdout + result.stderr
        else:
            (Path(tmp) / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.20)\nproject(conversions C)\n'
                'add_executable(conversions conversions.c)\n')
            result = subprocess.run([cmake, '-S', tmp, '-B', str(Path(tmp) / 'build'), '-A', 'x64'],
                                    capture_output=True, text=True)
            assert result.returncode == 0, result.stdout + result.stderr
            result = subprocess.run([cmake, '--build', str(Path(tmp) / 'build'), '--config', 'Release'],
                                    capture_output=True, text=True)
            assert result.returncode == 0, result.stdout + result.stderr
            exe = Path(tmp) / 'build/Release/conversions.exe'
        result = subprocess.run([str(exe)], capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
