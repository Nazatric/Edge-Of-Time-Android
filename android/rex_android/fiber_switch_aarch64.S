/**
 * @file    fiber_switch_aarch64.S
 * @brief   AArch64 cooperative context switch for rex::thread::Fiber on Android
 *
 * Android's Bionic libc does not implement the System V ucontext fiber API
 * (getcontext/makecontext/swapcontext). Those symbols simply do not exist, so
 * rexglue-sdk/src/core/fiber_posix.cpp cannot be used on Android.
 *
 * This provides the minimal equivalent: a switch that preserves exactly the
 * registers AAPCS64 requires a function to preserve across a call.
 *
 *   - x19..x28  callee-saved general purpose registers
 *   - x29 (fp)  frame pointer
 *   - x30 (lr)  link register == the resume address
 *   - sp        stack pointer
 *   - d8..d15   low 64 bits of v8..v15 (the only callee-saved vector state)
 *
 * Caller-saved registers do not need preserving: by definition the compiler
 * already assumes they are clobbered across the call to this function.
 *
 * @license BSD 3-Clause License (matches the ReXGlue SDK it plugs into)
 */

#if defined(__aarch64__)

/*
 * Offsets must stay in lockstep with struct RexFiberContext in
 * fiber_context_aarch64.h. A static_assert there verifies the total size.
 */
#define OFF_X19   0
#define OFF_X21   16
#define OFF_X23   32
#define OFF_X25   48
#define OFF_X27   64
#define OFF_FP    80    /* x29, x30 */
#define OFF_SP    96
#define OFF_D8    104
#define OFF_D10   120
#define OFF_D12   136
#define OFF_D14   152

    .text
    .balign 16
    .global rex_fiber_swap
    .hidden rex_fiber_swap
    .type   rex_fiber_swap, %function

/*
 * void rex_fiber_swap(RexFiberContext* from /* x0 */, RexFiberContext* to /* x1 */);
 *
 * Saves the current execution context into *from, then resumes *to. Control
 * returns here (and then to the original caller) when some other fiber swaps
 * back into *from.
 */
rex_fiber_swap:
    /* --- save current context into x0 --- */
    stp     x19, x20, [x0, #OFF_X19]
    stp     x21, x22, [x0, #OFF_X21]
    stp     x23, x24, [x0, #OFF_X23]
    stp     x25, x26, [x0, #OFF_X25]
    stp     x27, x28, [x0, #OFF_X27]
    stp     x29, x30, [x0, #OFF_FP]
    mov     x2, sp
    str     x2,  [x0, #OFF_SP]
    stp     d8,  d9,  [x0, #OFF_D8]
    stp     d10, d11, [x0, #OFF_D10]
    stp     d12, d13, [x0, #OFF_D12]
    stp     d14, d15, [x0, #OFF_D14]

    /* --- restore target context from x1 --- */
    ldp     x19, x20, [x1, #OFF_X19]
    ldp     x21, x22, [x1, #OFF_X21]
    ldp     x23, x24, [x1, #OFF_X23]
    ldp     x25, x26, [x1, #OFF_X25]
    ldp     x27, x28, [x1, #OFF_X27]
    ldp     x29, x30, [x1, #OFF_FP]
    ldr     x2,  [x1, #OFF_SP]
    mov     sp, x2
    ldp     d8,  d9,  [x1, #OFF_D8]
    ldp     d10, d11, [x1, #OFF_D10]
    ldp     d12, d13, [x1, #OFF_D12]
    ldp     d14, d15, [x1, #OFF_D14]

    /*
     * Jump to the restored link register. For a fiber that was previously
     * suspended this returns into rex_fiber_swap's caller. For a freshly
     * created fiber, lr was seeded with the trampoline address, so this is
     * the fiber's first entry.
     */
    ret

    .size rex_fiber_swap, . - rex_fiber_swap

#endif /* __aarch64__ */

/* Mark the stack non-executable; required for Android W^X compliance. */
#if defined(__linux__)
    .section .note.GNU-stack, "", %progbits
#endif
