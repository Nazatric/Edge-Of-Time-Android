// Isolated probe for std::jthread / std::stop_token, which libc++ gates behind
// -fexperimental-library. Kept in its own translation unit so its result is not
// masked by the unrelated <charconv> gap probed in libcxx_probe.cpp.
// Used by rexglue-sdk/src/core/timer_queue.cpp:43,47
#include <stop_token>
#include <thread>

int main() {
  std::jthread t([](std::stop_token st) { (void)st; });
  t.request_stop();
  return 0;
}
