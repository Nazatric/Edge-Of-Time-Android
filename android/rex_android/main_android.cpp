/**
 * @file    main_android.cpp
 * @brief   Implementation of the Android helpers the ReXGlue SDK expects
 *
 * rexglue-sdk/src/core/threading_posix.cpp includes <rex/main_android.h> and
 * both it and memory_posix.cpp call rex::GetAndroidApiLevel(), but no such
 * header or symbol exists anywhere in the published SDK. The Xenia lineage of
 * this code is visible in memory_posix.cpp:57, which still carries the
 * commented-out original include:
 *
 *     // #include "xenia/base/main_android.h"
 *
 * so the Android support was started and left incomplete. This supplies the
 * missing piece.
 *
 * @license BSD 3-Clause License (matches the ReXGlue SDK it plugs into)
 */

#include <rex/main_android.h>

#if defined(__ANDROID__)

#include <android/api-level.h>
#include <sys/system_properties.h>

#include <atomic>
#include <cstdlib>

namespace rex {
namespace {

/// Resolve the running device's API level exactly once.
int32_t QueryAndroidApiLevel() {
#if __ANDROID_API__ >= 24
  // Bionic provides this from API 24 onwards and it reports the *device* level,
  // which is what we want -- __ANDROID_API__ is only the compile-time minimum.
  const int level = android_get_device_api_level();
  if (level > 0) {
    return static_cast<int32_t>(level);
  }
#endif
  // Fallback for very old devices or unusual images.
  char value[PROP_VALUE_MAX] = {};
  if (__system_property_get("ro.build.version.sdk", value) > 0) {
    const int parsed = std::atoi(value);
    if (parsed > 0) {
      return static_cast<int32_t>(parsed);
    }
  }
  // Conservative floor: the SDK only gates API 26+ features on this value, so
  // reporting a low level disables them rather than crashing.
  return 1;
}

std::atomic<int32_t> g_api_level{0};

}  // namespace

int32_t GetAndroidApiLevel() {
  int32_t cached = g_api_level.load(std::memory_order_acquire);
  if (cached != 0) {
    return cached;
  }
  const int32_t level = QueryAndroidApiLevel();
  // Benign race: every racing thread computes the same value.
  g_api_level.store(level, std::memory_order_release);
  return level;
}

}  // namespace rex

#endif  // __ANDROID__
