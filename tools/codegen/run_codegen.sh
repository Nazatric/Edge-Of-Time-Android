#!/usr/bin/env bash
# Run the recompiled-code generation pipeline locally.
#
# Usage: tools/codegen/run_codegen.sh <game-files-dir> [host-tools-dir]
#
# <game-files-dir> must contain, from YOUR OWN copy of the game (title update
# applied):
#   Default.xex          (at the top level, or under the dir directly)
#   Data/GameLogic.dll
# and may contain:
#   shaders/             (extracted Xenos shader containers -> assets/shaders/)
#
# The files are copied into upstream/assets/ (gitignored upstream -- they are
# NEVER committed). Nothing in this script touches git.
#
# See docs/ANDROID_PORT_CODEGEN.md for the full trace.
set -euo pipefail

GAME_DIR="${1:?usage: run_codegen.sh <game-files-dir> [host-tools-dir]}"
TOOLS_DIR="${2:-out/host-tools}"

if [ ! -f "$GAME_DIR/Default.xex" ]; then
    echo "error: $GAME_DIR/Default.xex not found (title update applied is required)" >&2
    exit 1
fi
if [ ! -f "$GAME_DIR/Data/GameLogic.dll" ]; then
    echo "error: $GAME_DIR/Data/GameLogic.dll not found (title update applied is required)" >&2
    exit 1
fi

REXGLUE="$TOOLS_DIR/rexglue/rexglue"
XENOS="$TOOLS_DIR/xenosrecomp/XenosRecomp"
if [ ! -x "$REXGLUE" ] || [ ! -x "$XENOS" ]; then
    echo "==> Host tools not found; building them first (tools/codegen/build_host_tools.sh)"
    bash tools/codegen/build_host_tools.sh "$TOOLS_DIR"
fi

echo "==> Staging game files into upstream/assets/ (gitignored, never committed)"
mkdir -p upstream/assets/Data
cp "$GAME_DIR/Default.xex" upstream/assets/Default.xex
cp "$GAME_DIR/Data/GameLogic.dll" upstream/assets/Data/GameLogic.dll
if [ -d "$GAME_DIR/shaders" ]; then
    mkdir -p upstream/assets/shaders
    cp -r "$GAME_DIR/shaders/." upstream/assets/shaders/
fi

echo "==> Running rexglue codegen (this is the exact invocation the SDK's CMake uses)"
(cd upstream && "$OLDPWD/$REXGLUE" codegen "$OLDPWD/upstream/reeot_manifest.toml")

echo "==> Verifying codegen outputs"
test -f upstream/generated/default/sources.cmake
test -f upstream/generated/default/reeot_init.h
test -f upstream/generated/default/reeot_pch.h
ls upstream/generated/gamelogic | head -5
echo "codegen OK: generated/default + generated/gamelogic"

if [ -d upstream/assets/shaders ] && [ -n "$(ls -A upstream/assets/shaders 2>/dev/null)" ]; then
    echo "==> Generating the shader cache with XenosRecomp (~10 min)"
    mkdir -p upstream/generated
    "$XENOS" upstream/assets/shaders upstream/generated/shader_cache.cpp \
        upstream/thirdparty/XenosRecomp/XenosRecomp/shader_common.h
    test -s upstream/generated/shader_cache.cpp
    echo "shader cache OK: generated/shader_cache.cpp"
else
    echo "==> No assets/shaders/ provided: the game build will use the empty"
    echo "    shader-cache stub and the game will not draw. See"
    echo "    docs/ANDROID_PORT_CODEGEN.md section 3."
fi

echo "==> Done. Next: configure and build the game for Android (see"
echo "    docs/ANDROID_PORT_CODEGEN.md section 5, or the codegen.yml workflow)."
