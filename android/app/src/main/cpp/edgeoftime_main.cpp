/**
 * @file    edgeoftime_main.cpp
 * @brief   Android application entry point (SDL_main) for Edge of Time
 *
 * SDLActivity (org.libsdl.app, vendored from SDL3's android-project) resolves
 * "SDL_main" in this library via dlsym and runs it on the SDL thread through
 * SDL_RunApp. Everything below runs on that thread, which the ReXGlue SDK
 * treats as the UI thread.
 *
 * The launcher mirrors the real game boot sequence from the SDK
 * (src/ui/windowed_app_main_sdl.cpp + src/ui/rex_app.cpp):
 *
 *   cvars/logging -> platform Android init -> SDLWindowedAppContext ->
 *   rex::ui::Window (WindowSDL) -> VulkanInstance -> VulkanDevice ->
 *   UISamplers -> VulkanPresenter -> window->SetPresenter() -> message loop
 *
 * What is NOT here yet: the recompiled game code. That is produced by
 * `rexglue codegen` from the user's own game files (see
 * docs/ANDROID_PORT_CODEGEN.md) and plugged in where marked
 * "GAME BOOTSTRAP HOOK" below. Until then the launcher presents through the
 * real SDK Vulkan presenter (a black letterbox frame) and reports the missing
 * game files through the Java status overlay - it does not fake gameplay.
 *
 * @license BSD 3-Clause License (matches the repository it ships in)
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <jni.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <rex/android_jni.h>
#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/platform.h>
#include <rex/thread.h>
#include <rex/ui/presenter.h>
#include <rex/ui/vulkan/device.h>
#include <rex/ui/vulkan/instance.h>
#include <rex/ui/vulkan/presenter.h>
#include <rex/ui/vulkan/ui_samplers.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context_sdl.h>

#include <spdlog/sinks/android_sink.h>

namespace {

constexpr const char* kLogTag = "EdgeOfTime";

// Kept alive for the whole run; the presenter must outlive the window's
// presenter attachment, and the device/instance must outlive the presenter.
std::unique_ptr<rex::ui::vulkan::VulkanPresenter> g_presenter;
std::unique_ptr<rex::ui::vulkan::UISamplers> g_ui_samplers;
std::unique_ptr<rex::ui::vulkan::VulkanDevice> g_device;
std::unique_ptr<rex::ui::vulkan::VulkanInstance> g_instance;
std::unique_ptr<rex::ui::Window> g_window;
rex::ui::SDLWindowedAppContext* g_app_context = nullptr;
SDL_TimerID g_frame_pump_timer = 0;
std::atomic<bool> g_tearing_down{false};

// ---------------------------------------------------------------------------
// Java bridge
// ---------------------------------------------------------------------------

/// Reports a status line to the Java overlay (NativeBridge.onNativeStatus).
/// Safe to call from any thread.
void ReportStatus(const std::string& message) {
  JNIEnv* env = rex::android::GetThreadEnv();
  if (!env) {
    return;
  }
  jclass cls = env->FindClass("com/nazatric/edgeoftime/NativeBridge");
  if (!cls) {
    env->ExceptionClear();
    return;
  }
  jmethodID mid = env->GetStaticMethodID(cls, "onNativeStatus", "(Ljava/lang/String;)V");
  if (!mid) {
    env->ExceptionClear();
    env->DeleteLocalRef(cls);
    return;
  }
  jstring jmsg = env->NewStringUTF(message.c_str());
  env->CallStaticVoidMethod(cls, mid, jmsg);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
  }
  env->DeleteLocalRef(jmsg);
  env->DeleteLocalRef(cls);
}

/// Calls the Java helper NativeBridge.callStringStatic(name, signature, arg).
/// Returns the string result (empty if null/failed).
std::string CallJavaStringHelper(const char* method, const char* signature, const char* arg) {
  JNIEnv* env = rex::android::GetThreadEnv();
  if (!env) {
    return {};
  }
  jclass cls = env->FindClass("com/nazatric/edgeoftime/NativeBridge");
  if (!cls) {
    env->ExceptionClear();
    return {};
  }
  jmethodID mid = env->GetStaticMethodID(cls, method, signature);
  if (!mid) {
    env->ExceptionClear();
    env->DeleteLocalRef(cls);
    return {};
  }
  jstring jarg = arg ? env->NewStringUTF(arg) : nullptr;
  auto* result = reinterpret_cast<jstring>(
      env->CallStaticObjectMethod(cls, mid, jarg));
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    result = nullptr;
  }
  std::string out;
  if (result) {
    const char* chars = env->GetStringUTFChars(result, nullptr);
    if (chars) {
      out = chars;
      env->ReleaseStringUTFChars(result, chars);
    }
    env->DeleteLocalRef(result);
  }
  if (jarg) {
    env->DeleteLocalRef(jarg);
  }
  env->DeleteLocalRef(cls);
  return out;
}

std::string GetGameFolderUri() {
  return CallJavaStringHelper("getGameFolderUri", "()Ljava/lang/String;", nullptr);
}

/// Asks Java (ContentResolver + DocumentsContract) to resolve a '/'-separated
/// relative path inside the selected SAF tree to a full content:// document
/// URI. Returns an empty string when not found.
std::string ProbeGameFile(const std::string& relative_path) {
  return CallJavaStringHelper("probeGameFile",
                              "(Ljava/lang/String;)Ljava/lang/String;",
                              relative_path.c_str());
}

/// Verifies from native code that a content URI is actually openable, by
/// resolving it through the ContentResolver (rex::filesystem's Android layer).
bool ContentUriIsReadable(const std::string& uri) {
  if (uri.empty() || !rex::filesystem::IsAndroidContentUri(uri)) {
    return false;
  }
  return rex::filesystem::OpenAndroidContentFileDescriptor(uri, "r") >= 0;
}

// ---------------------------------------------------------------------------
// Game files probe
// ---------------------------------------------------------------------------

struct GameFilesStatus {
  bool folder_selected = false;
  bool default_xex = false;
  bool gamelogic_dll = false;
};

GameFilesStatus ProbeGameFiles() {
  GameFilesStatus status;
  const std::string folder = GetGameFolderUri();
  status.folder_selected = !folder.empty();
  if (!status.folder_selected) {
    return status;
  }
  // The folder mirrors the upstream layout: Default.xex at the root and
  // Data/GameLogic.dll one level down (see upstream/reeot_manifest.toml).
  const std::string xex_uri = ProbeGameFile("Default.xex");
  status.default_xex = ContentUriIsReadable(xex_uri);
  const std::string dll_uri = ProbeGameFile("Data/GameLogic.dll");
  status.gamelogic_dll = ContentUriIsReadable(dll_uri);
  return status;
}

std::string FormatGameFilesStatus(const GameFilesStatus& status) {
  if (!status.folder_selected) {
    return "Game files required: select the folder containing your copy of the game "
           "(Default.xex + Data/GameLogic.dll, title update applied).";
  }
  std::string message = "Game folder selected. Found: ";
  message += status.default_xex ? "Default.xex" : "(missing Default.xex)";
  message += status.gamelogic_dll ? " + Data/GameLogic.dll" : " + (missing Data/GameLogic.dll)";
  message += ". The recompiled game code is not built into this APK yet - see "
             "docs/ANDROID_PORT_CODEGEN.md.";
  return message;
}

void ReprobeGameFiles() {
  const GameFilesStatus status = ProbeGameFiles();
  ReportStatus(FormatGameFilesStatus(status));
}

// ---------------------------------------------------------------------------
// Frame pump
// ---------------------------------------------------------------------------

/// Drives forced presenter paints at ~30 Hz from a timer thread.
///
/// The SDK's own loop only paints on platform paint events (resize, expose).
/// On Android the surface can be destroyed and recreated underneath us
/// (rotation, app switch) without a paint event following, so the launcher
/// keeps the real presenter's swapchain alive with a periodic forced paint.
/// CallInUIThreadDeferred is thread-safe and wakes the UI loop via its wakeup
/// event; PaintFromUIThread is then executed on the UI thread as required.
/// The paint is a cheap no-op while the surface is invalid (backgrounded).
Uint32 SDLCALL FramePumpTimerCallback(void* userdata, SDL_TimerID timer_id, Uint32 interval) {
  (void)userdata;
  (void)timer_id;
  (void)interval;
  if (g_tearing_down.load(std::memory_order_acquire)) {
    return 0;  // stop the repeating timer
  }
  rex::ui::SDLWindowedAppContext* ctx = g_app_context;
  if (ctx && g_presenter) {
    ctx->CallInUIThreadDeferred([]() {
      if (g_presenter && !g_tearing_down.load(std::memory_order_acquire)) {
        g_presenter->PaintFromUIThread(true);
      }
    });
  }
  return 33;  // repeat (~30 Hz)
}

// ---------------------------------------------------------------------------
// Vulkan bring-up (the real SDK presenter path used by the game)
// ---------------------------------------------------------------------------

bool BringUpVulkan(rex::ui::Window& window) {
  ReportStatus("Initializing Vulkan...");

  g_instance = rex::ui::vulkan::VulkanInstance::Create(true, false);
  if (!g_instance) {
    ReportStatus("Vulkan instance creation failed - this device has no Vulkan driver.");
    return false;
  }

  std::vector<VkPhysicalDevice> physical_devices;
  g_instance->EnumeratePhysicalDevices(physical_devices);
  if (physical_devices.empty()) {
    ReportStatus("Vulkan instance created but no physical devices were enumerated.");
    return false;
  }

  // Prefer a discrete GPU, fall back to the first device (phones are UMA).
  VkPhysicalDevice chosen = physical_devices.front();
  VkPhysicalDeviceProperties chosen_props{};
  g_instance->functions().vkGetPhysicalDeviceProperties(chosen, &chosen_props);
  for (VkPhysicalDevice candidate : physical_devices) {
    VkPhysicalDeviceProperties props{};
    g_instance->functions().vkGetPhysicalDeviceProperties(candidate, &props);
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      chosen = candidate;
      chosen_props = props;
      break;
    }
  }

  g_device = rex::ui::vulkan::VulkanDevice::CreateIfSupported(g_instance.get(), chosen,
                                                              /*with_gpu_emulation=*/false,
                                                              /*with_swapchain=*/true);
  if (!g_device) {
    ReportStatus(std::string("No suitable Vulkan device (") + chosen_props.deviceName +
                 " does not support the required features).");
    return false;
  }

  g_ui_samplers = rex::ui::vulkan::UISamplers::Create(g_device.get());
  if (!g_ui_samplers) {
    ReportStatus("UI sampler creation failed.");
    return false;
  }

  g_presenter = rex::ui::vulkan::VulkanPresenter::Create(
      [](bool is_responsible, bool statically_from_ui_thread) {
        (void)statically_from_ui_thread;
        ReportStatus(is_responsible ? "Host GPU device lost (responsible) - restart the app."
                                    : "Host GPU device lost (external) - restart the app.");
      },
      g_device.get(), g_ui_samplers.get());
  if (!g_presenter) {
    ReportStatus("Vulkan presenter creation failed.");
    return false;
  }

  // This creates the AndroidNativeWindowSurface from the SDL window's
  // ANativeWindow and connects the presenter to it - the same call the game
  // makes in rex_app.cpp (SetupPresentation).
  window.SetPresenter(g_presenter.get());

  ReportStatus(std::string("Vulkan ready: ") + chosen_props.deviceName);
  return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// JNI entry points called from Java (com.nazatric.edgeoftime.NativeBridge)
// ---------------------------------------------------------------------------

extern "C" {

JNIEXPORT void JNICALL
Java_com_nazatric_edgeoftime_NativeBridge_nativeNotifyGameFolderChanged(JNIEnv* env,
                                                                        jclass clazz) {
  (void)env;
  (void)clazz;
  ReprobeGameFiles();
}

JNIEXPORT void JNICALL
Java_com_nazatric_edgeoftime_NativeBridge_nativeRequestQuit(JNIEnv* env, jclass clazz) {
  (void)env;
  (void)clazz;
  // Thread-safe: wakes RunMainMessageLoop, which processes the quit request.
  SDL_Event event{};
  event.type = SDL_EVENT_QUIT;
  SDL_PushEvent(&event);
}

}  // extern "C"

// ---------------------------------------------------------------------------
// SDL_main - resolved by SDLActivity.nativeRunMain via dlsym
// ---------------------------------------------------------------------------

extern "C" SDL_DECLSPEC int SDLCALL SDL_main(int argc, char* argv[]) {
  // --- Logging: mirror the SDK's own early-init + add a logcat sink. ---
  rex::cvar::Init(argc, argv);
  rex::cvar::ApplyEnvironment();
  rex::InitLoggingEarly();
  rex::LogConfig log_config = rex::BuildLogConfig(nullptr, "info", {});
  rex::AddSink(std::make_shared<spdlog::sinks::android_sink_mt>(kLogTag));
  rex::InitLogging(log_config);
  REXLOG_INFO("Edge of Time Android launcher starting (SDL_main).");

  // --- JNI registration (needs the Activity + JavaVM from SDL). ---
  JNIEnv* jni_env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
  JavaVM* java_vm = nullptr;
  if (jni_env) {
    jni_env->GetJavaVM(&java_vm);
  }
  jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
  if (!java_vm || !activity) {
    ReportStatus("Fatal: could not obtain the Android JNI environment.");
    return 1;
  }
  if (!rex::android::RegisterJni(java_vm, activity)) {
    ReportStatus("Fatal: JNI registration failed.");
    return 1;
  }
  ReportStatus("Platform layer initializing...");

  // --- SDK Android platform bring-up (memory + threading + filesystem). ---
  rex::filesystem::AndroidInitialize();
  rex::memory::AndroidInitialize();
  rex::thread::AndroidInitialize();

  // --- SDL subsystems. Video is initialized by SDLWindowedAppContext below;
  // audio (AAudio on Android) is initialized here so the audio path is live
  // before the game boots. ---
  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
      REXLOG_WARN("SDL audio init failed: {}", SDL_GetError());
      ReportStatus(std::string("Audio init failed: ") + SDL_GetError());
    } else {
      int device_count = 0;
      SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&device_count);
      if (devices) {
        SDL_free(devices);
      }
      REXLOG_INFO("SDL audio initialized ({} playback devices).", device_count);
    }
  }

  // --- UI context + window (same calls the game's RunWindowedApp makes). ---
  rex::ui::SDLWindowedAppContext app_context;
  g_app_context = &app_context;
  if (!app_context.Initialize()) {
    ReportStatus(std::string("SDL video init failed: ") + SDL_GetError());
    return 1;
  }

  g_window = rex::ui::Window::Create(app_context, "Spider-Man: Edge of Time", 1280, 720);
  if (!g_window || !g_window->Open()) {
    ReportStatus("Window creation failed.");
    REXLOG_ERROR("Window creation failed.");
    return 1;
  }

  // --- Vulkan presenter through the real SDK path. ---
  const bool vulkan_ok = BringUpVulkan(*g_window);
  if (vulkan_ok) {
    g_frame_pump_timer = SDL_AddTimer(33, FramePumpTimerCallback, nullptr);
  }

  // --- Game files probe. ---
  ReprobeGameFiles();

  // =========================================================================
  // GAME BOOTSTRAP HOOK
  //
  // When the recompiled game code is available (generated by `rexglue codegen`
  // from the user's own game files - see docs/ANDROID_PORT_CODEGEN.md), it is
  // built as libreeot_game.so which:
  //   1. defines rex::ui::GetWindowedAppCreator() (REX_DEFINE_APP(reeot, ...)),
  //   2. links librexruntime.so + librexgpu-xenos.so, and
  //   3. is loaded here with SDL_LoadObject, after which the upstream
  //      RunWindowedApp flow (ReeotApp : rex::ReXApp) takes over: runtime
  //      construction, XEX load, shader cache, guest GPU init.
  // Until then the launcher keeps presenting through the real Vulkan
  // presenter and reports the missing files above. No placeholder gameplay
  // is substituted.
  // =========================================================================

  // --- Main message loop (blocks until SDL_EVENT_QUIT). ---
  const int result = app_context.RunMainMessageLoop();

  // --- Teardown (reverse order of bring-up). ---
  g_tearing_down.store(true, std::memory_order_release);
  if (g_frame_pump_timer) {
    SDL_RemoveTimer(g_frame_pump_timer);
    g_frame_pump_timer = 0;
  }
  if (g_window) {
    g_window->SetPresenter(nullptr);
  }
  g_presenter.reset();
  g_ui_samplers.reset();
  g_device.reset();
  g_instance.reset();
  g_window->RequestClose();
  g_window.reset();
  g_app_context = nullptr;

  rex::thread::AndroidShutdown();
  rex::memory::AndroidShutdown();
  rex::filesystem::AndroidShutdown();
  rex::android::UnregisterJni();

  REXLOG_INFO("Edge of Time Android launcher exiting (code {}).", result);
  return result;
}
