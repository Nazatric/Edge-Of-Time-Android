#pragma once
/**
 * @file    rex/ui/surface_android.h
 * @brief   ANativeWindow-backed rex::ui::Surface for Android
 *
 * rexglue-sdk/src/ui/vulkan/vulkan_presenter.cpp includes this header on
 * Android and already implements the whole presentation path against it --
 * Surface::kTypeIndex_AndroidNativeWindow is the first entry of the surface
 * type enum, and the VK_KHR_android_surface branch calls
 * vkCreateAndroidSurfaceKHR() with AndroidNativeWindowSurface::window().
 *
 * Only the class itself was missing from the published SDK. This supplies it,
 * matching the shape of the existing surface_gnulinux.h / surface_win.h /
 * surface_mac.h implementations.
 *
 * @license BSD 3-Clause License (matches the ReXGlue SDK it plugs into)
 */

#include <rex/ui/surface.h>

struct ANativeWindow;

namespace rex {
namespace ui {

/// Presentation target backed by an Android ANativeWindow.
///
/// The window is owned by the Activity/SurfaceView (or by SDL), not by this
/// object. Android destroys it on surfaceDestroyed(), so the owner must tear
/// the surface down and recreate it on surfaceCreated() rather than caching it
/// across the lifecycle.
class AndroidNativeWindowSurface final : public Surface {
 public:
  explicit AndroidNativeWindowSurface(ANativeWindow* window) : window_(window) {}

  TypeIndex GetType() const override { return kTypeIndex_AndroidNativeWindow; }

  ANativeWindow* window() const { return window_; }

  /// Called when the platform hands over a new native window (for example
  /// after a rotation or an app switch). Passing nullptr marks the surface as
  /// not currently presentable; GetSize() then reports false and the presenter
  /// skips presentation instead of submitting to a dead swapchain.
  void set_window(ANativeWindow* window) { window_ = window; }

 protected:
  bool GetSizeImpl(uint32_t& width_out, uint32_t& height_out) const override;

 private:
  ANativeWindow* window_;
};

}  // namespace ui
}  // namespace rex
