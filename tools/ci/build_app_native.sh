#!/usr/bin/env bash
# Build libedgeoftime.so - the Android application entry point (SDL_main) -
# against the PREBUILT ReXGlue SDK libraries produced by the M1 build.
#
# Run from the repository root, after the SDK build (android-native.yml does
# this in the same job). Requires ANDROID_HOME (set on GitHub runners).
set -euo pipefail

NDK_VERSION="${NDK_VERSION:-27.3.13750724}"
ABI="${ABI:-arm64-v8a}"
API="${API:-28}"

if [ -z "${ANDROID_HOME:-}" ]; then
    echo "error: ANDROID_HOME is not set" >&2
    exit 1
fi
NDK="$ANDROID_HOME/ndk/$NDK_VERSION"

# Locate the SDK build output directory (the dir containing librexruntime.so).
SDK_BUILD=""
for d in "out/android" "thirdparty/rexglue-sdk/out/linux-arm64"; do
    if [ -f "$d/librexruntime.so" ]; then
        SDK_BUILD="$PWD/$d"
        break
    fi
done
if [ -z "$SDK_BUILD" ]; then
    echo "error: librexruntime.so not found - run the SDK build first" >&2
    exit 1
fi
echo "SDK_BUILD=$SDK_BUILD"
ls -la "$SDK_BUILD"/librexruntime.so "$SDK_BUILD"/librexgpu-xenos.so

APP_OUT="out/android-app"
# 16 KB page alignment for Android 15+ devices (same as the SDK build).
LINK16K="-Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384"

cmake -S android/app/src/main/cpp -B "$APP_OUT" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_PLATFORM="android-$API" \
    -DCMAKE_BUILD_TYPE=Release \
    -DREXGLUE_SDK_DIR="$PWD/thirdparty/rexglue-sdk" \
    -DREXGLUE_BUILD_DIR="$SDK_BUILD" \
    -DREX_ANDROID_SUPPORT_DIR="$PWD/android/rex_android" \
    -DREX_ANDROID_SUPPORT_INCLUDE="$PWD/android/rex_android/include" \
    -DCMAKE_CXX_FLAGS="-fexperimental-library" \
    -DCMAKE_EXE_LINKER_FLAGS="-fexperimental-library $LINK16K" \
    -DCMAKE_SHARED_LINKER_FLAGS="-fexperimental-library $LINK16K"

cmake --build "$APP_OUT" --parallel 2

echo "--- libedgeoftime.so ---"
ls -la "$APP_OUT"/libedgeoftime.so
"$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf" -h "$APP_OUT"/libedgeoftime.so \
    | grep -E "Class|Machine"
"$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf" -d "$APP_OUT"/libedgeoftime.so \
    | grep NEEDED || true
