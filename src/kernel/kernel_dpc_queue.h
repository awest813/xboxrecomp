#ifndef XBOX_KERNEL_DPC_QUEUE_H
#define XBOX_KERNEL_DPC_QUEUE_H
#include <stdint.h>
typedef struct { uint32_t dpc, arg1, arg2; } XboxPendingDpc;
int xbox_DpcQueueInsert(uint32_t dpc, uint32_t arg1, uint32_t arg2);
int xbox_DpcQueueRemove(uint32_t dpc);
int xbox_DpcQueuePop(XboxPendingDpc *out);
int xbox_DpcQueueContains(uint32_t dpc);
#endif
