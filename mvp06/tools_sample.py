"""Sample every thread's RIP in a running mvp06_recomp.exe and resolve the
code addresses to function names via the linker .map file."""
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
psapi = ctypes.WinDLL("psapi")
TH32CS_SNAPTHREAD = 0x4
CONTEXT_INTEGER_FULL = 0x10000B

class THREADENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD),
                ("th32ThreadID", wt.DWORD), ("th32OwnerProcessID", wt.DWORD),
                ("tpBasePri", wt.LONG), ("tpDeltaPri", wt.LONG),
                ("dwFlags", wt.DWORD)]

def load_map():
    syms = []
    pat = re.compile(r"^ [0-9A-Fa-f]{4}:[0-9A-Fa-f]{8}\s+(\S+)\s+([0-9A-Fa-f]{8,16})\s+f\s")
    pref = None
    for line in open(MAP, errors="replace"):
        if pref is None and "Preferred load address is" in line:
            pref = int(line.split("is")[1].strip(), 16)
        m = pat.match(line)
        if m:
            syms.append((int(m.group(2), 16), m.group(1)))
    syms.sort()
    return pref, syms

def resolve(va, pref, syms, load_base):
    a = va - load_base + pref
    i = bisect.bisect_right(syms, (a, "\uffff")) - 1
    if i >= 0 and 0 <= a - syms[i][0] < 0x200000:
        return syms[i][1], a - syms[i][0]
    return None, None

def main():
    env = dict(os.environ)
    env["RECOMP_VBLANK"] = "1"
    proc = subprocess.Popen([r"build\Release\mvp06_recomp.exe"], env=env, cwd=".")
    time.sleep(float(sys.argv[1]) if len(sys.argv) > 1 else 15.0)
    pid = proc.pid
    pref, syms = load_map()
    print(f"map: {len(syms)} symbols, preferred base {pref:#x}, pid {pid}")

    h = k32.OpenProcess(0x1F0FFF, False, pid)
    mods = (ctypes.c_void_p * 1024)()
    cb = ctypes.c_ulong(1024 * ctypes.sizeof(ctypes.c_void_p))
    psapi.EnumProcessModules(h, mods, ctypes.byref(cb))
    load_base = mods[0]
    print(f"load base: {load_base:#x}")

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

    from collections import defaultdict, Counter
    hist = defaultdict(Counter)
    # CONTEXT must be 16-byte aligned for GetThreadContext
    raw = ctypes.create_string_buffer(0x4D8 + 16)
    delta = ((ctypes.addressof(raw) + 15) & ~15) - ctypes.addressof(raw)
    rounds = int(sys.argv[2]) if len(sys.argv) > 2 else 10
    for _ in range(rounds):
        for tid in tids:
            th = k32.OpenThread(0x1FFFFF, False, tid)
            if not th:
                continue
            if k32.SuspendThread(th) != -1:
                buf = ctypes.c_void_p(ctypes.addressof(raw) + delta)
                struct.pack_into("<I", raw, delta + 0x30, CONTEXT_INTEGER_FULL)
                if k32.GetThreadContext(th, buf):
                    rip = struct.unpack_from("<Q", raw, delta + 0xF8)[0]
                    rsp = struct.unpack_from("<Q", raw, delta + 0x98)[0]
                    hist[tid][(rip, rsp)] += 1
                k32.ResumeThread(th)
            k32.CloseHandle(th)
        time.sleep(0.05)

    for tid, counts in hist.items():
        print(f"tid {tid}:")
        for (rip, rsp), n in counts.most_common(4):
            name, off = resolve(rip, pref, syms, load_base)
            where = f"{name}+{off:#x}" if name else (
                "ntdll/kernel" if rip >> 40 else f"rip={rip:#x}")
            print(f"   x{n:<3} {where}   rsp={rsp:#x}")
    proc.kill()

if __name__ == "__main__":
    main()
