/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* 32 x 32 -> 64-bit multiplies, out of line. On the FM-1 such a multiply inlined among 16-bit loads now and then
 * leaves a register one bit off, and the firmware crashes (felucca-bsp docs/CPU.md: fm1-radio's AAC decoder, October
 * 2026; fm1-base's build lists the functions where it happens). A helper of its own uses the product at once, so it is
 * complete before any load that follows. The same arithmetic: the results are bit for bit the inline ones. */
#pragma once
#include <stdint.h>

__attribute__((noinline)) static int64_t mul64(int64_t a, int64_t b) { return a * b; }
__attribute__((noinline)) static uint64_t umul64(uint64_t a, uint64_t b) { return a * b; }
