"""Arm a hardware write watchpoint (DR0/DR7) on a guest VA in every thread
of a running mvp06_recomp.exe, so the VEH names the writer when the address
is written. Usage: py -3 tools_watchpoint.py <guest_va_hex> [wait_seconds]
"""
import bisect
import ctypes
import ctypes.wintypes as wt
import os
import struct
import subprocess
import sys
import time

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
k32.ReadProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                  ctypes.c_void_p, ctypes.c_size_t,
                                  ctypes.POINTER(ctypes.c_size_t)]
k32.ReadProcessMemory.restype = ctypes.c_bool
k32.GetThreadContext = k32.GetThreadContext
k32.SetThreadContext = k32.SetThreadContext

import tools_stackscan as ts

class THREADENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD),
                ("th32ThreadID", wt.DWORD), ("th32OwnerProcessID", wt.DWORD),
                ("tpBasePri", wt.LONG), ("tpDeltaPri", wt.LONG),
                ("dwFlags", wt.DWORD)]

def main():
    guest_va = int(sys.argv[1], 16)
    wait_s = float(sys.argv[2]) if len(sys.argv) > 2 else 12.0

    env = dict(os.environ); env["RECOMP_VBLANK"] = "1"
    proc = subprocess.Popen([r"build\Release\mvp06_recomp.exe"], env=env, cwd=".")
    time.sleep(wait_s)
    pid = proc.pid
    print(f"child alive: {proc.poll() is None}")

    # exe load base for host address computation
    msnap = k32.CreateToolhelp32Snapshot(8, pid)
    me = ts.MODULEENTRY32(); me.dwSize = ctypes.sizeof(me)
    exe_base = 0
    if k32.Module32First(msnap, ctypes.byref(me)):
        while True:
            if me.szModule.decode().lower() == "mvp06_recomp.exe":
                exe_base = me.modBaseAddr; break
            if not k32.Module32Next(msnap, ctypes.byref(me)): break
    k32.CloseHandle(msnap)

    # The 0x80000000 contiguous window is a mapping inside our own process
    # (the game IS this process) -- resolve the guest VA through the exe's
    # own g_xbox_mem_offset by reading it out of... simplest: the watchpoint
    # needs a HOST address; compute it from the runtime's mapping instead by
    # reading the guest memory through the runtime's own mechanism is not
    # available here, so use the known window base: VirtualQuery walk.
    # Instead: the game maps the window at a fixed hint; find it by scanning
    # the exe's modules is wrong -- the window is a private mapping. Use the
    # VEH-free path: EnumProcessModulesEx won't see it. Fall back: read
    # g_xbox_mem_offset from the exe's data via the .map file.
    # The runtime maps every guest window at va + g_memory_offset, and the
    # boot log prints that offset ("offset +65536" => 0x10000). Take it from
    # the child's stderr when possible; default to 0x10000.
    mem_off = 0x10000
    host_va = (guest_va + mem_off) & 0xFFFFFFFFFFFF
    print(f"guest {guest_va:#x} -> host {host_va:#x} (offset {mem_off:#x})")

    snap = k32.CreateToolhelp32Snapshot(4, 0)
    te = THREADENTRY32(); te.dwSize = ctypes.sizeof(te)
    tids = []
    if k32.Thread32First(snap, ctypes.byref(te)):
        while True:
            if te.th32OwnerProcessID == pid: tids.append(te.th32ThreadID)
            if not k32.Thread32Next(snap, ctypes.byref(te)): break
    k32.CloseHandle(snap)

    armed = 0
    for tid in tids:
        th = k32.OpenThread(0x1FFFFF, False, tid)
        if not th: continue
        if k32.SuspendThread(th) != -1:
            raw = ctypes.create_string_buffer(0x4D8 + 16)
            delta = ((ctypes.addressof(raw) + 15) & ~15) - ctypes.addressof(raw)
            ctx = ctypes.c_void_p(ctypes.addressof(raw) + delta)
            struct.pack_into("<I", raw, delta + 0x30, 0x100017)  # FULL|DEBUG_REGISTERS
            if k32.GetThreadContext(th, ctx):
                # Dr0 @0x308, Dr7 @0x320 in x64 CONTEXT
                struct.pack_into("<Q", raw, delta + 0x308, host_va)
                # DR7: L0=1, RW0=01b (write), LEN0=11b (4 bytes)
                struct.pack_into("<Q", raw, delta + 0x320, 0xD0001)
                if k32.SetThreadContext(th, ctx):
                    armed += 1
            k32.ResumeThread(th)
        k32.CloseHandle(th)
    print(f"armed {armed}/{len(tids)} threads; waiting for the write...")

    # stream the child's stderr so the watchpoint line shows up here
    try:
        proc.wait(timeout=90)
    except Exception:
        proc.kill()
    print(f"child exited with {proc.returncode}")

if __name__ == "__main__":
    main()
