"""Scan each thread's host stack for return addresses inside the exe,
resolve them to recomp function names via the .map file, and print the
reconstructed call chain (innermost frames first)."""
import bisect
import ctypes
import ctypes.wintypes as wt
import os
import re
import struct
import subprocess
import sys
import time

MAP = r"build\Release\mvp06_recomp.map"
k32 = ctypes.WinDLL("kernel32", use_last_error=True)
k32.ReadProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                  ctypes.c_void_p, ctypes.c_size_t,
                                  ctypes.POINTER(ctypes.c_size_t)]
k32.ReadProcessMemory.restype = ctypes.c_bool
TH32CS_SNAPTHREAD = 0x4
TH32CS_SNAPMODULE = 0x8
CONTEXT_FULL = 0x10000B

class THREADENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD),
                ("th32ThreadID", wt.DWORD), ("th32OwnerProcessID", wt.DWORD),
                ("tpBasePri", wt.LONG), ("tpDeltaPri", wt.LONG),
                ("dwFlags", wt.DWORD)]

class MODULEENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("th32ModuleID", wt.DWORD),
                ("th32ProcessID", wt.DWORD), ("GlblcntUsage", wt.DWORD),
                ("ProccntUsage", wt.DWORD), ("modBaseAddr", ctypes.c_void_p),
                ("modBaseSize", wt.DWORD), ("hModule", wt.HMODULE),
                ("szModule", ctypes.c_char * 256),
                ("szExePath", ctypes.c_char * 260)]

def load_map():
    syms, pref = [], None
    pat = re.compile(r"^ [0-9A-Fa-f]{4}:[0-9A-Fa-f]{8}\s+(\S+)\s+([0-9A-Fa-f]{8,16})\s+f\s")
    for line in open(MAP, errors="replace"):
        if pref is None and "Preferred load address is" in line:
            pref = int(line.split("is")[1].strip(), 16)
        m = pat.match(line)
        if m:
            syms.append((int(m.group(2), 16), m.group(1)))
    syms.sort()
    return pref, syms

def resolve(host_rip, pref, syms, load_base):
    a = host_rip - load_base + pref
    i = bisect.bisect_right(syms, (a, "\uffff")) - 1
    if i >= 0 and 0 <= a - syms[i][0] < 0x4000:
        return syms[i][1], a - syms[i][0]
    return None, None

def main():
    env = dict(os.environ); env["RECOMP_VBLANK"] = "1"
    proc = subprocess.Popen([r"build\Release\mvp06_recomp.exe"], env=env, cwd=".")
    time.sleep(float(sys.argv[1]) if len(sys.argv) > 1 else 15.0)
    pid = proc.pid
    pref, syms = load_map()

    h = k32.OpenProcess(0x1F0FFF, False, pid)
    msnap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid)
    me = MODULEENTRY32(); me.dwSize = ctypes.sizeof(me)
    exe_base = None
    if k32.Module32First(msnap, ctypes.byref(me)):
        while True:
            if me.szModule.decode().lower() == "mvp06_recomp.exe":
                exe_base = me.modBaseAddr
                break
            if not k32.Module32Next(msnap, ctypes.byref(me)):
                break
    k32.CloseHandle(msnap)
    print(f"exe base {exe_base:#x}")

    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
    te = THREADENTRY32(); te.dwSize = ctypes.sizeof(te)
    tids = []
    if k32.Thread32First(snap, ctypes.byref(te)):
        while True:
            if te.th32OwnerProcessID == pid:
                tids.append(te.th32ThreadID)
            if not k32.Thread32Next(snap, ctypes.byref(te)):
                break
    k32.CloseHandle(snap)

    for tid in tids:
        th = k32.OpenThread(0x1FFFFF, False, tid)
        if not th:
            continue
        frames = []
        if k32.SuspendThread(th) != -1:
            raw = ctypes.create_string_buffer(0x4D8 + 16)
            delta = ((ctypes.addressof(raw) + 15) & ~15) - ctypes.addressof(raw)
            buf = ctypes.c_void_p(ctypes.addressof(raw) + delta)
            struct.pack_into("<I", raw, delta + 0x30, CONTEXT_FULL)
            if k32.GetThreadContext(th, buf):
                rip = struct.unpack_from("<Q", raw, delta + 0xF8)[0]
                rsp = struct.unpack_from("<Q", raw, delta + 0x98)[0]
                name, off = resolve(rip, pref, syms, exe_base)
                frames.append((name or f"rip={rip:#x}", off or 0))
                # scan the stack for exe return addresses
                read = ctypes.c_size_t(0)
                stack = (ctypes.c_ulonglong * 512)()
                if k32.ReadProcessMemory(h, ctypes.c_void_p(rsp), stack,
                                         ctypes.sizeof(stack),
                                         ctypes.byref(read)):
                    n = read.value // 8
                    found = 0
                    for i in range(n):
                        v = stack[i]
                        if exe_base <= v < exe_base + 0x3200000:
                            nm, of = resolve(v, pref, syms, exe_base)
                            if nm:
                                frames.append((nm, of))
                                found += 1
                                if found >= 10:
                                    break
            k32.ResumeThread(th)
        k32.CloseHandle(th)
        if frames:
            print(f"tid {tid}:")
            for nm, of in frames:
                print(f"    {nm}+{of:#x}")
    proc.kill()

if __name__ == "__main__":
    main()
