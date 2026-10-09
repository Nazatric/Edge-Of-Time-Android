/**
 * @file    fiber_context_aarch64.h
 * @brief   Register-save block used by the Android AArch64 fiber backend
 *
 * Layout is mirrored by the OFF_* defines in fiber_switch_aarch64.S. Keep the
 * two in sync; the static_assert below catches size drift.
 *
 * @license BSD 3-Clause License (matches the ReXGlue SDK it plugs into)
 */

#pragma once

#include <cstddef>
#include <cstdint>

extern "C" {

/// Callee-saved CPU state for one suspended fiber (AAPCS64).
struct RexFiberContext {
  uint64_t x19, x20, x21, x22, x23, x24, x25, x26, x27, x28;  // +0   .. +72
  uint64_t fp;                                                // +80  (x29)
  uint64_t lr;                                                // +88  (x30)
  uint64_t sp;                                                // +96
  uint64_t d8, d9, d10, d11, d12, d13, d14, d15;              // +104 .. +160
};

static_assert(sizeof(RexFiberContext) == 168,
              "RexFiberContext layout must match fiber_switch_aarch64.S offsets");
static_assert(offsetof(RexFiberContext, fp) == 80, "OFF_FP mismatch");
static_assert(offsetof(RexFiberContext, sp) == 96, "OFF_SP mismatch");
static_assert(offsetof(RexFiberContext, d8) == 104, "OFF_D8 mismatch");

/// Save the current context into `from`, then resume `to`.
/// Implemented in fiber_switch_aarch64.S.
void rex_fiber_swap(RexFiberContext* from, RexFiberContext* to);

}  // extern "C"
