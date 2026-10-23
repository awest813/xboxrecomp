/* Serialize guest regions protected by raised IRQL on the Xbox's single CPU.
 * Like the reference interrupt gate, higher interrupts are deferred until the
 * current region completes. Native DMA/render/audio work remains independent;
 * this is narrower than serializing all guest execution under a GIL. */
#include "kernel.h"
#include "kernel_guest.h"
#include "xbox_memory_layout.h"

static XBOX_THREAD_LOCAL KIRQL s_irql = PASSIVE_LEVEL;
static SRWLOCK s_dispatch_gate = SRWLOCK_INIT;
KIRQL xbox_CurrentIrql(void) { return s_irql; }

/* A host event can wake a passive guest thread in the middle of the DPC
 * which signalled it. On the single Xbox CPU that thread cannot resume until
 * raised-IRQL work finishes, including its stack-request/list cleanup. */
void xbox_GuestWaitResume(void)
{
    if (s_irql < DISPATCH_LEVEL) {
        AcquireSRWLockShared(&s_dispatch_gate);
        ReleaseSRWLockShared(&s_dispatch_gate);
    }
}

/* Xbox PIC vectors start at 0x30; IRQ priority decreases with bus number.
 * USB0 (IRQ1) runs at IRQL26. Returning the bus number as IRQL left USB's
 * queue/cancellation critical sections below DISPATCH_LEVEL, so DPCs could
 * consume a request after its submitting thread had returned and reused it.
 * Mapping: CxbxKrnl.h IRQ2VECTOR/VECTOR2IRQL and HalGetInterruptVector. */
ULONG __stdcall xbox_HalGetInterruptVector(ULONG bus_level, PKIRQL irql)
{
    if (bus_level > 27u)
        return 0;
    if (irql)
        *irql = (KIRQL)(27u - bus_level);
    return 0x30u + bus_level;
}

static void irql_set(KIRQL next)
{
    KIRQL old = s_irql;
    if (old < DISPATCH_LEVEL && next >= DISPATCH_LEVEL) {
        if (!TryAcquireSRWLockExclusive(&s_dispatch_gate)) {
            /* An ISR may own the dispatch gate while waiting for the guest
             * CPU. Do not wait on that gate while retaining the CPU lock. */
            int depth = xbox_GuestSuspend();
            AcquireSRWLockExclusive(&s_dispatch_gate);
            s_irql = next;
            xbox_GuestResume(depth);
        }
    }
    s_irql = next;
    /* XDK code also reads KPCR.Irql inline through fs:[0x24]. In particular,
     * DirectSound must see DISPATCH_LEVEL inside its DPC and avoid passive
     * critical-section waits. Publish a byte in this host thread's guest TIB;
     * native tests/callers without an initialized TIB have nothing to publish. */
    if (g_fs_base && (uint64_t)g_fs_base + 0x25u <= xbox_GetMappedSize())
        *(uint8_t *)((uintptr_t)g_fs_base + xbox_GetMemoryOffset() + 0x24u) = next;
    if (old >= DISPATCH_LEVEL && next < DISPATCH_LEVEL)
        ReleaseSRWLockExclusive(&s_dispatch_gate);
}

KIRQL __fastcall xbox_KfRaiseIrql(KIRQL next)
{
    KIRQL old = s_irql;
    if (next < old)
        xbox_log(XBOX_LOG_WARN, XBOX_LOG_HAL,
                 "KfRaiseIrql: attempt to lower IRQL from %d to %d", old, next);
    irql_set(next);
    return old;
}

VOID __fastcall xbox_KfLowerIrql(KIRQL next)
{
    if (next > s_irql)
        xbox_log(XBOX_LOG_WARN, XBOX_LOG_HAL,
                 "KfLowerIrql: attempt to raise IRQL from %d to %d", s_irql, next);
    irql_set(next);
}

KIRQL __stdcall xbox_KeRaiseIrqlToDpcLevel(void)
{
    return xbox_KfRaiseIrql(DISPATCH_LEVEL);
}
