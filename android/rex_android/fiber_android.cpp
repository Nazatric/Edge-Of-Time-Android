/**
 * @file    fiber_android.cpp
 * @brief   Android backend for rex::thread::Fiber
 *
 * Replaces rexglue-sdk/src/core/fiber_posix.cpp on Android. Bionic does not
 * implement getcontext/makecontext/swapcontext, so the POSIX backend fails to
 * compile with "use of undeclared identifier 'getcontext'".
 *
 * Semantics intentionally mirror fiber_posix.cpp exactly, including the
 * convention that Trampoline reads entry_/arg_ from tls_current_ (which
 * SwitchTo sets *before* performing the switch).
 *
 * @license BSD 3-Clause License (matches the ReXGlue SDK it plugs into)
 */

#include <rex/platform.h>

#if REX_PLATFORM_ANDROID

#include <rex/thread/fiber.h>

#include <sys/mman.h>
#include <unistd.h>

#include <cassert>
#include <cstdint>
#include <new>

namespace rex::thread {

thread_local Fiber* Fiber::tls_current_ = nullptr;

namespace {

/// Entry thunk. The assembly switch seeds lr with this address, so a brand new
/// fiber begins executing here with a freshly aligned stack.
[[noreturn]] void FiberEntryThunk() {
  Fiber::TrampolinePublic();
  // entry_ must never return; if it does there is no sane context to resume.
  assert(false && "fiber entry function returned");
  for (;;) {
  }
}

}  // namespace

Fiber* Fiber::ConvertCurrentThread() {
  auto* f = new (std::nothrow) Fiber();
  if (!f) {
    return nullptr;
  }
  // The running thread's context is captured lazily: the first SwitchTo away
  // from this fiber writes the live register state into context_.
  f->is_thread_fiber_ = true;
  tls_current_ = f;
  return f;
}

Fiber* Fiber::Create(size_t stack_size, void (*entry)(void*), void* arg) {
  auto* f = new (std::nothrow) Fiber();
  if (!f) {
    return nullptr;
  }
  f->entry_ = entry;
  f->arg_ = arg;

  // Round the stack up to a page multiple so the guard page lands cleanly.
  const size_t page = static_cast<size_t>(::sysconf(_SC_PAGESIZE));
  const size_t usable = (stack_size + page - 1) & ~(page - 1);
  const size_t total = usable + page;  // + one guard page

  // Map the stack manually rather than using std::vector so a PROT_NONE guard
  // page can be placed below it: a fiber stack overflow then faults instead of
  // silently corrupting whatever the allocator put underneath.
  void* base = ::mmap(nullptr, total, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (base == MAP_FAILED) {
    delete f;
    return nullptr;
  }
  if (::mprotect(base, page, PROT_NONE) != 0) {
    ::munmap(base, total);
    delete f;
    return nullptr;
  }

  f->stack_base_ = base;
  f->stack_size_ = total;

  // AArch64 requires sp to be 16-byte aligned at all times.
  auto top = reinterpret_cast<uintptr_t>(base) + total;
  top &= ~static_cast<uintptr_t>(15);

  f->context_ = RexFiberContext{};
  f->context_.sp = static_cast<uint64_t>(top);
  f->context_.lr = reinterpret_cast<uint64_t>(&FiberEntryThunk);
  // fp = 0 terminates stack unwinding at the fiber boundary, which keeps
  // backtraces from crash handlers and profilers from walking into garbage.
  f->context_.fp = 0;
  return f;
}

/*static*/ void Fiber::TrampolinePublic() {
  Fiber* f = tls_current_;
  f->entry_(f->arg_);
}

void Fiber::SwitchTo(Fiber* target) {
  Fiber* from = tls_current_;
  if (from == target) {
    return;
  }
  tls_current_ = target;
  rex_fiber_swap(&from->context_, &target->context_);
}

void Fiber::Destroy() {
  if (is_thread_fiber_) {
    tls_current_ = nullptr;
  } else {
    assert(this != tls_current_ && "Destroy called on the currently running fiber");
  }
  if (stack_base_) {
    ::munmap(stack_base_, stack_size_);
    stack_base_ = nullptr;
    stack_size_ = 0;
  }
  delete this;
}

}  // namespace rex::thread

#endif  // REX_PLATFORM_ANDROID
