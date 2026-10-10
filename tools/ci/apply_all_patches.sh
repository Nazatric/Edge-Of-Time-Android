#!/usr/bin/env bash
# Apply both patch sets: the ReXGlue SDK and the upstream game code.
set -euo pipefail
cd "$(dirname "$0")/../.."
./tools/ci/apply_patches.sh thirdparty/rexglue-sdk patches/rexglue-sdk
if compgen -G "patches/upstream/*.patch" > /dev/null; then
    ./tools/ci/apply_patches.sh upstream patches/upstream
else
    echo "(no upstream patches yet)"
fi
