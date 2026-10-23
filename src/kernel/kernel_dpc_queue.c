/* A KDPC may be pending only once. Multiple ISR/guest threads can enqueue,
 * while the timer thread drains the queue. Publish entries and Inserted
 * together under one lock; never hold it while executing guest callbacks. */
#include "kernel.h"
#include "kernel_dpc_queue.h"
#include "xbox_memory_layout.h"

#define DPC_QUEUE_SIZE 64u
static XboxPendingDpc s_queue[DPC_QUEUE_SIZE];
static unsigned s_head, s_tail;
static SRWLOCK s_lock = SRWLOCK_INIT;
#define INSERTED(va) (*(volatile uint8_t *)(xbox_GetMemoryOffset() + (va) + 2))

int xbox_DpcQueueInsert(uint32_t dpc, uint32_t arg1, uint32_t arg2)
{
    unsigned next;
    if (!dpc) return 0;
    AcquireSRWLockExclusive(&s_lock);
    next = (s_tail + 1) % DPC_QUEUE_SIZE;
    if (INSERTED(dpc) || next == s_head) {
        ReleaseSRWLockExclusive(&s_lock);
        return 0;
    }
    s_queue[s_tail].dpc = dpc;
    s_queue[s_tail].arg1 = arg1;
    s_queue[s_tail].arg2 = arg2;
    INSERTED(dpc) = 1;
    s_tail = next;
    ReleaseSRWLockExclusive(&s_lock);
    return 1;
}

int xbox_DpcQueueRemove(uint32_t dpc)
{
    unsigned i;
    int found = 0;
    if (!dpc) return 0;
    AcquireSRWLockExclusive(&s_lock);
    for (i = s_head; i != s_tail; i = (i + 1) % DPC_QUEUE_SIZE)
        if (s_queue[i].dpc == dpc) {
            unsigned next = (i + 1) % DPC_QUEUE_SIZE;
            /* Reclaim the slot immediately, including at full capacity. */
            while (next != s_tail) {
                s_queue[i] = s_queue[next];
                i = next;
                next = (next + 1) % DPC_QUEUE_SIZE;
            }
            s_tail = i;
            s_queue[i].dpc = 0;
            INSERTED(dpc) = 0;
            found = 1;
            break;
        }
    ReleaseSRWLockExclusive(&s_lock);
    return found;
}

int xbox_DpcQueuePop(XboxPendingDpc *out)
{
    int found = 0;
    if (!out) return 0;
    AcquireSRWLockExclusive(&s_lock);
    while (s_head != s_tail) {
        XboxPendingDpc d = s_queue[s_head];
        s_head = (s_head + 1) % DPC_QUEUE_SIZE;
        if (d.dpc) {
            INSERTED(d.dpc) = 0;
            *out = d;
            found = 1;
            break;
        }
    }
    ReleaseSRWLockExclusive(&s_lock);
    return found;
}

int xbox_DpcQueueContains(uint32_t dpc)
{
    unsigned i;
    int found=0;
    AcquireSRWLockShared(&s_lock);
    for(i=s_head;dpc && i!=s_tail;i=(i+1)%DPC_QUEUE_SIZE)
        if(s_queue[i].dpc==dpc) { found=1;break; }
    ReleaseSRWLockShared(&s_lock);
    return found;
}
