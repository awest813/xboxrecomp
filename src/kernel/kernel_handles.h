/* Guest-sized handle tokens for native host handles. */
#ifndef XBOX_KERNEL_HANDLES_H
#define XBOX_KERNEL_HANDLES_H
#include "kernel.h"
uint32_t xbox_HandleToken(HANDLE handle);
HANDLE xbox_HandleResolve(uint32_t token);
HANDLE xbox_HandleTake(uint32_t token);
#endif
