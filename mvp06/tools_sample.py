"""Sample every thread's RIP in a running mvp06_recomp.exe and resolve the
code addresses to function names via the linker .map file."""
import argparse
import bisect
import ctypes
import ctypes.wintypes as wt
import os
import re
import struct
import subprocess
import time

MAP = r"build\Release\mvp06_recomp.map"

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
psapi = ctypes.WinDLL("psapi")
TH32CS_SNAPTHREAD = 0x4
CONTEXT_INTEGER_FULL = 0x10000B
k32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
k32.OpenProcess.restype = wt.HANDLE
k32.OpenThread.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
k32.OpenThread.restype = wt.HANDLE
k32.CloseHandle.argtypes = [wt.HANDLE]
k32.CreateToolhelp32Snapshot.argtypes = [wt.DWORD, wt.DWORD]
k32.CreateToolhelp32Snapshot.restype = wt.HANDLE
k32.SuspendThread.argtypes = [wt.HANDLE]
k32.SuspendThread.restype = wt.DWORD
k32.ResumeThread.argtypes = [wt.HANDLE]
k32.GetThreadContext.argtypes = [wt.HANDLE, ctypes.c_void_p]
psapi.EnumProcessModules.argtypes = [wt.HANDLE, ctypes.c_void_p,
                                    wt.DWORD, ctypes.POINTER(wt.DWORD)]
k32.GetThreadTimes.argtypes = [wt.HANDLE] + [ctypes.POINTER(wt.FILETIME)] * 4

class MODULEINFO(ctypes.Structure):
    _fields_ = [("base", ctypes.c_void_p), ("size", wt.DWORD),
                ("entry", ctypes.c_void_p)]

psapi.GetModuleInformation.argtypes = [wt.HANDLE, ctypes.c_void_p,
    ctypes.POINTER(MODULEINFO), wt.DWORD]
psapi.GetModuleFileNameExW.argtypes = [wt.HANDLE, ctypes.c_void_p, wt.LPWSTR, wt.DWORD]

def thread_cpu(th):
    created, exited, kernel, user = (wt.FILETIME() for _ in range(4))
    if not k32.GetThreadTimes(th, ctypes.byref(created), ctypes.byref(exited),
                             ctypes.byref(kernel), ctypes.byref(user)):
        return None
    return sum((v.dwHighDateTime << 32) | v.dwLowDateTime for v in (kernel, user))

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

def system_syscalls(module):
    """Read local system-DLL exports; attribute only exact syscall stubs.

    This is RVA matching, not stack unwinding. The target and sampler use
    the same installed Windows DLLs. Other internal DLL addresses stay grouped
    under the module rather than acquiring a guessed function name.
    """
    dll = ctypes.WinDLL(module)
    base = dll._handle
    def u32(at):
        return struct.unpack("<I", ctypes.string_at(base + at, 4))[0]
    nt = u32(0x3C)
    if ctypes.string_at(base + nt, 4) != b"PE\0\0":
        return []
    magic = struct.unpack("<H", ctypes.string_at(base + nt + 24, 2))[0]
    if magic != 0x20B:  # The sampler/context layout is x64.
        return []
    directory = u32(nt + 24 + 112)
    functions, names, ordinals = (u32(directory + offset) for offset in (28, 32, 36))
    exports = {}
    for i in range(u32(directory + 24)):
        name = ctypes.string_at(base + u32(names + i * 4)).decode("ascii")
        if not name.startswith(("Nt", "Zw")):
            continue
        ordinal = struct.unpack("<H", ctypes.string_at(base + ordinals + i * 2, 2))[0]
        address = u32(functions + ordinal * 4)
        exports[address] = "Nt" + name[2:]  # Nt/Zw aliases share the stub.
    return sorted(exports.items())

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("delay", type=float, nargs="?", help="seconds before sampling")
    parser.add_argument("rounds", type=int, nargs="?", default=10)
    parser.add_argument("--pid", type=int, help="attach to an existing process; leave it running")
    parser.add_argument("--module-summary", action="store_true",
                        help="group RIP samples by function/DLL and report thread CPU time")
    parser.add_argument("--system-exports", action="store_true",
                        help="attribute ntdll/win32u syscall stubs using installed DLL exports")
    args = parser.parse_args()
    proc = None
    if args.pid:
        pid = args.pid
    else:
        env = dict(os.environ)
        env["RECOMP_VBLANK"] = "1"
        proc = subprocess.Popen([r"build\Release\mvp06_recomp.exe"], env=env, cwd=".")
        pid = proc.pid
    time.sleep(args.delay if args.delay is not None else (0 if args.pid else 15.0))
    pref, syms = load_map()
    print(f"map: {len(syms)} symbols, preferred base {pref:#x}, pid {pid}")

    h = k32.OpenProcess(0x1F0FFF, False, pid)
    if not h:
        raise ctypes.WinError(ctypes.get_last_error())
    mods = (ctypes.c_void_p * 1024)()
    cb = ctypes.c_ulong(1024 * ctypes.sizeof(ctypes.c_void_p))
    if not psapi.EnumProcessModules(h, mods, ctypes.sizeof(mods), ctypes.byref(cb)):
        k32.CloseHandle(h)
        raise ctypes.WinError(ctypes.get_last_error())
    load_base = mods[0]
    print(f"load base: {load_base:#x}")
    modules = []
    for mod in mods[:min(len(mods), cb.value // ctypes.sizeof(ctypes.c_void_p))]:
        info = MODULEINFO(); path = ctypes.create_unicode_buffer(32768)
        if psapi.GetModuleInformation(h, mod, ctypes.byref(info), ctypes.sizeof(info)):
            psapi.GetModuleFileNameExW(h, mod, path, len(path))
            modules.append((info.base, info.base + info.size, os.path.basename(path.value)))

    syscall_maps = {}
    if args.system_exports:
        for _, _, dll in modules:
            if dll.lower() in ("ntdll.dll", "win32u.dll"):
                rows = system_syscalls(dll)
                syscall_maps[dll.lower()] = ([row[0] for row in rows], rows)

    def location(rip):
        name, off = resolve(rip, pref, syms, load_base)
        if name:
            return name, f"{name}+{off:#x}"
        for base, end, dll in modules:
            if base <= rip < end:
                if dll.lower() in syscall_maps:
                    addresses, rows = syscall_maps[dll.lower()]
                    index = bisect.bisect_right(addresses, rip-base)-1
                    if index >= 0 and rip-base-rows[index][0] < 0x20:
                        label = f"{dll}!{rows[index][1]}"
                        return label, f"{label}+{rip-base-rows[index][0]:#x}"
                return dll, f"{dll}+{rip-base:#x}"
        return "unmapped", f"rip={rip:#x}"

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
    cpu_before = {}
    # CONTEXT must be 16-byte aligned for GetThreadContext
    raw = ctypes.create_string_buffer(0x4D8 + 16)
    delta = ((ctypes.addressof(raw) + 15) & ~15) - ctypes.addressof(raw)
    for _ in range(args.rounds):
        for tid in tids:
            th = k32.OpenThread(0x1FFFFF, False, tid)
            if not th:
                continue
            if tid not in cpu_before:
                cpu_before[tid] = thread_cpu(th)
            if k32.SuspendThread(th) != 0xFFFFFFFF:
                try:
                    buf = ctypes.c_void_p(ctypes.addressof(raw) + delta)
                    struct.pack_into("<I", raw, delta + 0x30, CONTEXT_INTEGER_FULL)
                    if k32.GetThreadContext(th, buf):
                        rip = struct.unpack_from("<Q", raw, delta + 0xF8)[0]
                        rsp = struct.unpack_from("<Q", raw, delta + 0x98)[0]
                        hist[tid][(rip, rsp)] += 1
                finally:
                    k32.ResumeThread(th)
            k32.CloseHandle(th)
        time.sleep(0.05)

    for tid, counts in hist.items():
        print(f"tid {tid}:")
        if args.module_summary:
            grouped = Counter()
            for (rip, rsp), n in counts.items():
                grouped[location(rip)[0]] += n
            th = k32.OpenThread(0x40, False, tid)
            end_cpu = thread_cpu(th) if th else None
            if th: k32.CloseHandle(th)
            start_cpu = cpu_before.get(tid)
            elapsed_cpu = (end_cpu-start_cpu)/10000 if end_cpu is not None and start_cpu is not None else None
            print(f"   CPU ms={elapsed_cpu}; grouped={dict(grouped.most_common(8))}")
        for (rip, rsp), n in counts.most_common(4):
            where = location(rip)[1]
            print(f"   x{n:<3} {where}   rsp={rsp:#x}")
    k32.CloseHandle(h)
    if proc is not None:
        proc.kill()

if __name__ == "__main__":
    main()
