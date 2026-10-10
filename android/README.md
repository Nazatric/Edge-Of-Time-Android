# Edge of Time — Android application

This directory contains the Android application that packages the ReXGlue
runtime into an installable APK.

```
android/
  settings.gradle, build.gradle, gradle.properties   Gradle project (AGP 8.7.3)
  gradle/wrapper/gradle-wrapper.properties          pinned Gradle 8.10.2
  app/
    build.gradle                                    app module (com.nazatric.edgeoftime)
    src/main/
      AndroidManifest.xml                           no permissions; Vulkan features optional
      java/
        org/libsdl/app/                             SDL3's Android glue, vendored verbatim
                                                  (zlib — see NOTICE.md there)
        com/nazatric/edgeoftime/                    MainActivity, NativeBridge, StatusOverlay,
                                                  CrashReporter, EdgeOfTimeApp
      res/                                          strings, theme, launcher icon
      cpp/
        CMakeLists.txt                              builds libedgeoftime.so
        edgeoftime_main.cpp                         SDL_main entry point (the launcher)
      jniLibs/arm64-v8a/                            populated by CI with the prebuilt
                                                  librexruntime.so, librexgpu-xenos.so,
                                                  libedgeoftime.so (gitignored)
```

## How it fits together

1. **CI job `native-core`** builds the ReXGlue SDK for `arm64-v8a`
   (`librexruntime.so`, which statically contains SDL3; `librexgpu-xenos.so`)
   and then builds `libedgeoftime.so` against those prebuilt libraries
   (`tools/ci/build_app_native.sh`).
2. **CI job `apk`** stages the three `.so` files into `app/src/main/jniLibs/`,
   runs `gradle :app:assembleDebug`, verifies the APK (aapt2 badging, packaged
   library architecture and 16 KB alignment via `llvm-readelf`, SHA-256) and
   uploads it as the `edge-of-time-android-debug-apk` workflow artifact.
3. **On device**, `MainActivity` extends SDL3's `SDLActivity`. It loads
   `librexruntime.so` first (its `JNI_OnLoad` registers all of SDL's JNI entry
   points and it statically contains SDL3), then `libedgeoftime.so`, whose
   exported `SDL_main` SDLActivity resolves via `dlsym` and runs on the SDL
   thread.

## What the app does today

`libedgeoftime.so` mirrors the real game boot sequence from the SDK
(`SDLWindowedAppContext` → `rex::ui::Window` → `VulkanInstance` →
`VulkanDevice` → `UISamplers` → `VulkanPresenter` → `window->SetPresenter()` →
message loop), so the genuine renderer initialization path runs on the device.
What it does **not** do yet is boot the game: the recompiled game code is
generated from the user's own game files (see `docs/ANDROID_PORT_CODEGEN.md`)
and is not present in this repository. The launcher probes the user-selected
Storage Access Framework folder for `Default.xex` and `Data/GameLogic.dll`,
verifies them natively through `ContentResolver.openFileDescriptor`, and
reports the state on a status overlay. No placeholder gameplay is substituted.

## Building locally

You need the Android SDK (platform 35, build-tools 35.0.0), JDK 17, and the
prebuilt native libraries staged into `app/src/main/jniLibs/arm64-v8a/`
(run the `native-core` CI job, or build the SDK yourself with the commands in
`docs/ANDROID_PORT_PROGRESS.md` plus `tools/ci/build_app_native.sh`).

```bash
cd android
gradle :app:assembleDebug        # or: gradle wrapper && ./gradlew :app:assembleDebug
```

The debug APK is signed with the automatically generated debug keystore.

## Installing

```bash
adb install app/build/outputs/apk/debug/app-debug.apk
adb logcat -s EdgeOfTime EdgeOfTimeBridge SDL AndroidRuntime
```

The app needs no permissions; game files are provided through the Storage
Access Framework folder picker (`ACTION_OPEN_DOCUMENT_TREE`).
