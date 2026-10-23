/* Guest KTHREAD views for object-taking kernel exports. */
#ifndef XBOX_KERNEL_THREAD_OBJECT_H
#define XBOX_KERNEL_THREAD_OBJECT_H

#include "kernel.h"

NTSTATUS xbox_ThreadObjectReference(HANDLE handle, uint32_t *object_va);
void xbox_ThreadObjectDereference(uint32_t object_va);
void xbox_ThreadObjectAddReference(uint32_t object_va);
void xbox_ThreadObjectHandleClosed(HANDLE handle);
HANDLE xbox_ThreadObjectHost(uint32_t object_va);

#endif
