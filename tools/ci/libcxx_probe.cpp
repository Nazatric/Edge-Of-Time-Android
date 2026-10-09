// Probes the two libc++ features the ReXGlue SDK requires but which older
// Android NDK libc++ releases do not implement:
//
//   1. std::from_chars for floating-point types (P0067R5). libc++ only gained
//      this recently; NDK 27 (clang 18) does not have it.
//      Used by rexglue-sdk/include/rex/string/numeric.h:195,322
//
//   2. std::chrono::clock_time_conversion (C++20 <chrono>).
//      Used by rexglue-sdk/include/rex/chrono/chrono.h:131,154
//
// Compile with: $CXX -std=c++23 -c libcxx_probe.cpp
// A clean compile means that NDK can build the SDK without shims.

#include <charconv>
#include <chrono>
#include <cstdio>

int main() {
  // (1) floating-point from_chars
  float f{};
  const char* s = "1.5";
  const auto r = std::from_chars(s, s + 3, f, std::chars_format::general);
  (void)r;

  // (2) chrono clock_time_conversion
  std::chrono::clock_time_conversion<std::chrono::system_clock,
                                     std::chrono::system_clock>
      conv{};
  (void)conv;

  std::printf("ok %f\n", static_cast<double>(f));
  return 0;
}

