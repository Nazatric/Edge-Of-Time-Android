#!/usr/bin/env bash
# Compile-probe upstream's game sources for Android arm64-v8a, WITHOUT
# needing the generated game code (no game dump, no codegen, no device).
#
# It compiles exactly the source list upstream's CMakeLists puts into the
# `reeot` target (REEOT_SOURCES), with the NDK clang for arm64, except:
#   - src/main.cpp and src/reeot_app.cpp, which #include generated headers
#     (reported separately as "needs codegen" - expected failures);
#   - platform/moltenvk.cpp (macOS-only, only added on APPLE).
#
# This is the fast, no-secrets regression gate for the upstream Android port
# (patches/upstream/): "does the game code compile for Android". Failures here
# are real porting work; the two codegen-dependent files are documented.
#
# Run from the repository root. Requires ANDROID_HOME (GitHub runners set it).
set -uo pipefail

NDK_VERSION="${NDK_VERSION:-27.3.13750724}"
API="${API:-28}"
NDK="${ANDROID_HOME:?ANDROID_HOME not set}/ndk/$NDK_VERSION"
CXX="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android${API}-clang++"

SDK="thirdparty/rexglue-sdk"
UP="upstream"
GEN="out/probe-gen"
mkdir -p "$GEN/core" out

# Minimal generated headers the sources include.
cat > "$GEN/core/build_info.h" <<'HDR'
#pragma once
#define REEOT_VERSION_STRING  "0.0.0-probe"
#define REEOT_GIT_COMMIT      "probe"
#define REEOT_GIT_BRANCH      "probe"
#define REEOT_GIT_DIRTY       0
#define REEOT_BUILD_PLATFORM  "android-arm64"
#define REEOT_BUILD_COMPILER  "probe"
#define REEOT_BUILD_TIMESTAMP "00000000_0000"
HDR

INCLUDES=(
    -I"$UP/src"
    -I"$GEN"
    -I"$SDK/include"
    -I"$SDK/thirdparty/sdl3/include"
    -I"$SDK/thirdparty/spdlog/include"
    -I"$SDK/thirdparty/fmt/include"
    -I"$SDK/thirdparty/simde"
    -I"$SDK/thirdparty/vulkan-headers/include"
    -I"$SDK/thirdparty/imgui"
    -I"$UP/thirdparty/renderdoc"
    -I"$UP/thirdparty/XenosRecomp/thirdparty/smol-v/source"
    -I"$UP/thirdparty/XenosRecomp/XenosRecomp"
    -I"$PWD/android/rex_android/include"
)
# plume (Vulkan renderer interface) and imgui live in different places.
for d in "$UP/thirdparty/plume" "$SDK/thirdparty/plume" "$SDK/thirdparty/imgui"; do
    [ -d "$d" ] && INCLUDES+=(-I"$d")
done

FLAGS=(
    -std=c++23
    -fexperimental-library
    -fno-char8_t
    -ffp-model=strict
    -DREX_HAS_VULKAN=1
    -DREEOT_VERSION_STRING='"0.0.0-probe"'
    -c
)

# The exact REEOT_SOURCES list from upstream/CMakeLists.txt.
SOURCES=$(awk '/^set\(REEOT_SOURCES/,/^\)/' "$UP/CMakeLists.txt" \
    | grep -oE 'src/[A-Za-z0-9_/]+\.cpp' | sort -u)

pass=0
fail=0
failed_files=()
for src in $SOURCES; do
    case "$src" in
        src/main.cpp|src/reeot_app.cpp)
            echo "SKIP  $src  (needs codegen output: generated/default/reeot_init.h)"
            continue
            ;;
        src/platform/moltenvk.cpp)
            echo "SKIP  $src  (macOS-only)"
            continue
            ;;
    esac
    [ -f "$UP/$src" ] || { echo "MISS  $src (listed but absent)"; continue; }
    name="$(echo "$src" | tr '/' '_')"
    if "$CXX" "${FLAGS[@]}" "${INCLUDES[@]}" "$UP/$src" -o /dev/null 2> "out/probe-$name.err"; then
        echo "PASS  $src"
        pass=$((pass + 1))
    else
        echo "FAIL  $src"
        head -8 "out/probe-$name.err" | sed 's/^/      /'
        fail=$((fail + 1))
        failed_files+=("$src")
    fi
done

echo ""
echo "upstream Android compile probe: $pass passed, $fail failed"
if [ "$fail" -gt 0 ]; then
    echo "failed files:"
    printf '  %s\n' "${failed_files[@]}"
    exit 1
fi
exit 0
