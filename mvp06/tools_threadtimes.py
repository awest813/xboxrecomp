"""Per-thread CPU time + ntdll wait-site identification for mvp06_recomp."""
import ctypes
import ctypes.wintypes as wt
import os
import struct
import subprocess
import sys
import time

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
psapi = ctypes.WinDLL("psapi")
TH32CS_SNAPTHREAD = 0x4
CONTEXT_FULL = 0x10000B

class THREADENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD),
                ("th32ThreadID", wt.DWORD), ("th32OwnerProcessID", wt.DWORD),
                ("tpBasePri", wt.LONG), ("tpDeltaPri", wt.LONG),
                ("dwFlags", wt.DWORD)]

class FILETIME(ctypes.Structure):
    _fields_ = [("dwLow", wt.DWORD), ("dwHigh", wt.DWORD)]

def main():
    env = dict(os.environ)
    env["RECOMP_VBLANK"] = "1"
    proc = subprocess.Popen([r"build\Release\mvp06_recomp.exe"], env=env, cwd=".")
    time.sleep(float(sys.argv[1]) if len(sys.argv) > 1 else 15.0)
    pid = proc.pid

    h = k32.OpenProcess(0x1F0FFF, False, pid)

    # module bases for the exe and ntdll
    class MODULEENTRY32(ctypes.Structure):
        _fields_ = [("dwSize", wt.DWORD), ("th32ModuleID", wt.DWORD),
                    ("th32ProcessID", wt.DWORD), ("GlblcntUsage", wt.DWORD),
                    ("ProccntUsage", wt.DWORD), ("modBaseAddr", ctypes.c_void_p),
                    ("modBaseSize", wt.DWORD), ("hModule", wt.HMODULE),
                    ("szModule", ctypes.c_char * 256),
                    ("szExePath", ctypes.c_char * 260)]
    msnap = k32.CreateToolhelp32Snapshot(0x8, pid)  # TH32CS_SNAPMODULE
    me = MODULEENTRY32(); me.dwSize = ctypes.sizeof(me)
    mods = {}
    if k32.Module32First(msnap, ctypes.byref(me)):
        while True:
            mods[me.szModule.decode().lower()] = me.modBaseAddr
            if not k32.Module32Next(msnap, ctypes.byref(me)):
                break
    k32.CloseHandle(msnap)
    exe_base = mods.get("mvp06_recomp.exe", 0)
    ntdll_base = mods.get("ntdll.dll", 0)
    print(f"exe={exe_base:#x} ntdll={ntdll_base:#x}")

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

    times1, ctxs = {}, {}
    for tid in tids:
        th = k32.OpenThread(0x1FFFFF, False, tid)
        if not th:
            continue
        ct, et, kt, ut = FILETIME(), FILETIME(), FILETIME(), FILETIME()
        k32.GetThreadTimes(th, ctypes.byref(ct), ctypes.byref(et),
                           ctypes.byref(kt), ctypes.byref(ut))
        times1[tid] = (kt.dwLow + (kt.dwHigh << 32)) + (ut.dwLow + (ut.dwHigh << 32))
        if k32.SuspendThread(th) != -1:
            raw = ctypes.create_string_buffer(0x4D8 + 16)
            delta = ((ctypes.addressof(raw) + 15) & ~15) - ctypes.addressof(raw)
            buf = ctypes.c_void_p(ctypes.addressof(raw) + delta)
            struct.pack_into("<I", raw, delta + 0x30, CONTEXT_FULL)
            if k32.GetThreadContext(th, buf):
                ctxs[tid] = struct.unpack_from("<Q", raw, delta + 0xF8)[0]
            k32.ResumeThread(th)
        k32.CloseHandle(th)
    time.sleep(3.0)
    print(f"{'tid':>7} {'cpu-ms/3s':>10}  rip-module+offset")
    for tid in tids:
        th = k32.OpenThread(0x1FFFFF, False, tid)
        if not th:
            continue
        ct, et, kt, ut = FILETIME(), FILETIME(), FILETIME(), FILETIME()
        k32.GetThreadTimes(th, ctypes.byref(ct), ctypes.byref(et),
                           ctypes.byref(kt), ctypes.byref(ut))
        now = (kt.dwLow + (kt.dwHigh << 32)) + (ut.dwLow + (ut.dwHigh << 32))
        k32.CloseHandle(th)
        cpu_ms = (now - times1[tid]) / 10000.0
        rip = ctxs.get(tid, 0)
        where = "?"
        if exe_base and exe_base <= rip < exe_base + 0x4000000:
            where = f"exe+{rip - exe_base:#x}"
        elif ntdll_base and ntdll_base <= rip < ntdll_base + 0x200000:
            where = f"ntdll+{rip - ntdll_base:#x}"
        elif rip:
            where = f"rip={rip:#x}"
        print(f"{tid:>7} {cpu_ms:>10.1f}  {where}")
    proc.kill()

if __name__ == "__main__":
    main()
