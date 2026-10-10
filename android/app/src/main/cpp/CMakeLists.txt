# libedgeoftime.so - the Android application entry point.
#
# This library defines SDL_main, which SDLActivity (org.libsdl.app) resolves via
# dlsym and runs on the SDL thread. It links against the PREBUILT ReXGlue SDK
# libraries produced by the M1 CI job (librexruntime.so, which statically
# contains SDL3 and exports its public API, plus librexgpu-xenos.so), so the
# app shares the SDK's single SDL3 instance instead of carrying a second copy.
#
# Required CMake arguments:
#   REXGLUE_SDK_DIR    - path to the rexglue-sdk source tree (patched)
#   REXGLUE_BUILD_DIR  - SDK build output directory (librexruntime.so etc.)
#   REX_ANDROID_SUPPORT_DIR     - android/rex_android (platform layer sources)
#   REX_ANDROID_SUPPORT_INCLUDE - android/rex_android/include
#
# The compile flags mirror the SDK's own Android build (see
# .github/workflows/android-native.yml) so that the C++ ABI matches exactly:
# same libc++ (static, -fexperimental-library), same -std=c++23, same
# -fno-char8_t and -ffp-model=strict.

cmake_minimum_required(VERSION 3.22)
project(edgeoftime_android LANGUAGES CXX)

if(NOT REXGLUE_SDK_DIR OR NOT REXGLUE_BUILD_DIR)
    message(FATAL_ERROR "REXGLUE_SDK_DIR and REXGLUE_BUILD_DIR are required")
endif()

add_library(edgeoftime SHARED edgeoftime_main.cpp)

target_compile_features(edgeoftime PRIVATE cxx_std_23)

target_include_directories(edgeoftime PRIVATE
    "${REXGLUE_SDK_DIR}/include"
    "${REXGLUE_SDK_DIR}/thirdparty/sdl3/include"
    "${REXGLUE_SDK_DIR}/thirdparty/spdlog/include"
    "${REXGLUE_SDK_DIR}/thirdparty/fmt/include"
    "${REXGLUE_SDK_DIR}/thirdparty/simde"
    "${REXGLUE_SDK_DIR}/thirdparty/vulkan-headers/include"
    "${REX_ANDROID_SUPPORT_INCLUDE}"
)

target_compile_definitions(edgeoftime PRIVATE
    REX_HAS_VULKAN=1
)

# Match the SDK's Android compile flags (see android-native.yml).
target_compile_options(edgeoftime PRIVATE
    -fexperimental-library
    -fno-char8_t
    -ffp-model=strict
    -fPIC
    -O3
    -DNDEBUG
)

target_link_directories(edgeoftime PRIVATE "${REXGLUE_BUILD_DIR}")

# librexruntime.so statically contains SDL3 and exports SDL's public API
# (SDL_DECLSPEC is visibility("default")), so all SDL symbols resolve from it.
# DT_NEEDED entries make the load order explicit for the dynamic linker.
target_link_libraries(edgeoftime PRIVATE
    rexruntime
    log
    android
)

target_link_options(edgeoftime PRIVATE
    -fexperimental-library
    # Android 15+ devices may use 16 KB pages; keep LOAD segments 16 KB aligned.
    -Wl,-z,max-page-size=16384
    -Wl,-z,common-page-size=16384
)
