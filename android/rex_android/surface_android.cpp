/**
 * @file    surface_android.cpp
 * @brief   ANativeWindow-backed Surface implementation for Android
 *
 * @license BSD 3-Clause License (matches the ReXGlue SDK it plugs into)
 */

#include <rex/platform.h>

#if REX_PLATFORM_ANDROID

#include <rex/ui/surface_android.h>

#include <android/native_window.h>

namespace rex {
namespace ui {

bool AndroidNativeWindowSurface::GetSizeImpl(uint32_t& width_out, uint32_t& height_out) const {
  if (!window_) {
    // No native window right now (between surfaceDestroyed and surfaceCreated).
    // Reporting zero tells Presenter not to open a presentation connection.
    width_out = 0;
    height_out = 0;
    return false;
  }

  const int32_t width = ANativeWindow_getWidth(window_);
  const int32_t height = ANativeWindow_getHeight(window_);
  if (width <= 0 || height <= 0) {
    width_out = 0;
    height_out = 0;
    return false;
  }

  // ANativeWindow reports physical pixels already, which is exactly what
  // Surface::GetSize is specified to return, so no density scaling is applied.
  width_out = static_cast<uint32_t>(width);
  height_out = static_cast<uint32_t>(height);
  return true;
}

}  // namespace ui
}  // namespace rex

#endif  // REX_PLATFORM_ANDROID
