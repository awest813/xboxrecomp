#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "../../src/kernel/kernel.h"
#include "../../src/kernel/xbox_memory_layout.h"

void xbox_log(int level, const char *subsystem, const char *fmt, ...)
{ (void)level; (void)subsystem; (void)fmt; assert(!"Unexpected IRQL warning"); }
static HANDLE started, finished;
static volatile LONG inside;
static unsigned shared;
static KIRQL usb_irql;
extern void xbox_GuestWaitResume(void);
static DWORD WINAPI contender(LPVOID unused)
{
    KIRQL old;
    (void)unused;
    SetEvent(started);
    old = xbox_KfRaiseIrql(usb_irql);
    assert(old == PASSIVE_LEVEL);
    SetEvent(finished);
    xbox_KfLowerIrql(old);
    return 0;
}
static DWORD WINAPI wait_resumer(LPVOID unused)
{
    (void)unused;
    SetEvent(started);
    xbox_GuestWaitResume();
    SetEvent(finished);
    return 0;
}
static DWORD WINAPI stress(LPVOID unused)
{
    unsigned i;
    uint8_t *tib;
    g_fs_base = 0x200u + 0x40u * (uint32_t)(uintptr_t)unused;
    tib = (uint8_t *)((uintptr_t)g_fs_base + xbox_GetMemoryOffset());
    tib[0x23] = 0xA5; tib[0x25] = 0x5A;
    for (i = 0; i < 4000; i++) {
        KIRQL old = xbox_KeRaiseIrqlToDpcLevel();
        KIRQL dispatch = xbox_KfRaiseIrql(3);
        assert(old == PASSIVE_LEVEL && dispatch == DISPATCH_LEVEL);
        assert(tib[0x24] == 3);
        assert(InterlockedIncrement(&inside) == 1);
        shared++;
        assert(InterlockedDecrement(&inside) == 0);
        xbox_KfLowerIrql(dispatch);
        assert(tib[0x24] == DISPATCH_LEVEL);
        xbox_KfLowerIrql(old);
        assert(tib[0x24] == PASSIVE_LEVEL && tib[0x23] == 0xA5 && tib[0x25] == 0x5A);
    }
    return 0;
}
int main(void)
{
    HANDLE threads[4], thread;
    KIRQL old, dispatch;
    unsigned i;
    for (i = 0; i <= 27; i++) {
        KIRQL level = 0xFF;
        assert(xbox_HalGetInterruptVector(i, &level) == 0x30 + i);
        assert(level == 27 - i);
        assert(xbox_HalGetInterruptVector(i, NULL) == 0x30 + i);
    }
    usb_irql = 0xFF;
    assert(xbox_HalGetInterruptVector(28, &usb_irql) == 0 && usb_irql == 0xFF);
    assert(xbox_HalGetInterruptVector(1, &usb_irql) == 0x31 && usb_irql == 26);
    started = CreateEvent(NULL, TRUE, FALSE, NULL);
    finished = CreateEvent(NULL, TRUE, FALSE, NULL);
    assert(started && finished);
    old = xbox_KeRaiseIrqlToDpcLevel();
    dispatch = xbox_KfRaiseIrql(3);
    thread = CreateThread(NULL, 0, contender, NULL, 0, NULL);
    assert(thread);
    assert(WaitForSingleObject(started, 2000) == WAIT_OBJECT_0);
    assert(WaitForSingleObject(finished, 0) == WAIT_TIMEOUT);
    xbox_KfLowerIrql(dispatch); /* Still at DISPATCH_LEVEL: retain the gate. */
    assert(WaitForSingleObject(finished, 0) == WAIT_TIMEOUT);
    xbox_KfLowerIrql(old);
    assert(WaitForSingleObject(thread, 2000) == WAIT_OBJECT_0);
    assert(WaitForSingleObject(finished, 0) == WAIT_OBJECT_0);
    CloseHandle(thread);
    ResetEvent(started); ResetEvent(finished);
    old = xbox_KeRaiseIrqlToDpcLevel();
    thread = CreateThread(NULL, 0, wait_resumer, NULL, 0, NULL);
    assert(thread && WaitForSingleObject(started, 1000) == WAIT_OBJECT_0);
    assert(WaitForSingleObject(finished, 10) == WAIT_TIMEOUT);
    xbox_GuestWaitResume(); /* Nested DPC caller must not lock itself. */
    xbox_KfLowerIrql(old);
    assert(WaitForSingleObject(thread, 1000) == WAIT_OBJECT_0);
    assert(WaitForSingleObject(finished, 0) == WAIT_OBJECT_0);
    CloseHandle(thread); CloseHandle(started); CloseHandle(finished);
    for (i = 0; i < 4; i++) {
        threads[i] = CreateThread(NULL, 0, stress, (void *)(uintptr_t)i, 0, NULL);
        assert(threads[i]);
    }
    assert(WaitForMultipleObjects(4, threads, TRUE, 10000) == WAIT_OBJECT_0);
    assert(shared == 16000 && inside == 0);
    for (i = 0; i < 4; i++) CloseHandle(threads[i]);
    puts("PIC vectors/IRQLs, nesting, deferred USB contender and 16000 protected updates passed");
    return 0;
}
