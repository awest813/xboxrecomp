/* Separate guest TIB storage for native IRQL/scheduler regressions. */
#include "../../src/kernel/xbox_memory_layout.h"
static uint8_t ram[0x10000];
RECOMP_TLS uint32_t g_fs_base;
ptrdiff_t xbox_GetMemoryOffset(void) { return (ptrdiff_t)(uintptr_t)ram; }
size_t xbox_GetMappedSize(void) { return sizeof ram; }
