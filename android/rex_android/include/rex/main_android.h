/**
 * @file    rex/main_android.h
 * @brief   Android platform helpers required by the ReXGlue SDK POSIX backends
 *
 * Installed into the SDK include tree as <rex/main_android.h>, which
 * src/core/threading_posix.cpp includes unconditionally on Android but which
 * the published SDK does not ship.
 *
 * @license BSD 3-Clause License (matches the ReXGlue SDK it plugs into)
 */

#pragma once

#include <cstdint>

namespace rex {

/// Returns the API level of the *device* the process is running on (e.g. 34
/// for Android 14), not the compile-time target. Result is cached.
///
/// The SDK uses this to gate API 26+ facilities such as ASharedMemory_create
/// (see src/core/memory_posix.cpp) and thread-name/affinity behaviour (see
/// src/core/threading_posix.cpp).
///
/// Returns 1 if the level genuinely cannot be determined, which disables
/// version-gated features rather than crashing.
int32_t GetAndroidApiLevel();

}  // namespace rex
