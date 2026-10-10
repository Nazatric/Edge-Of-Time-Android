#!/usr/bin/env bash
# Build the codegen HOST tools natively (x86_64): rexglue (from the ReXGlue SDK)
# and XenosRecomp (vendored in upstream). Neither is marked host-only in the
# SDK/upstream CMake, so cross-compiling for Android builds them for arm64 and
# they cannot run on the build machine -- hence this separate native build.
#
# No game files are needed. Run from the repository root.
set -euo pipefail

OUT_DIR="${1:-out/host-tools}"
JOBS="${JOBS:-2}"

echo "==> Building rexglue (host, native)"
cmake -S thirdparty/rexglue-sdk -B "$OUT_DIR/rexglue" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DREXGLUE_BUILD_TESTS=OFF \
    -DREXGLUE_ENABLE_TRACY=OFF
cmake --build "$OUT_DIR/rexglue" --target rexglue --parallel "$JOBS"

echo "==> Building XenosRecomp (host, native)"
cmake -S upstream/thirdparty/XenosRecomp -B "$OUT_DIR/xenosrecomp" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$OUT_DIR/xenosrecomp" --parallel "$JOBS"

echo "==> Building pkztool (host, native, via PKZLib)"
cmake -S upstream/thirdparty/PKZLib -B "$OUT_DIR/pkzlib" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DPKZLIB_BUILD_TOOLS=ON \
    -DPKZLIB_BUILD_TESTS=OFF
cmake --build "$OUT_DIR/pkzlib" --target pkztool --parallel "$JOBS"

echo "==> Building pkzprep (host, native, single translation unit)"
HOST_CXX="${HOST_CXX:-c++}"
"$HOST_CXX" -std=c++23 -O2 \
    -I upstream/thirdparty/stb \
    upstream/pkzlib/tools/pkzprep.cpp \
    -o "$OUT_DIR/pkzprep"

echo "==> Host tools:"
ls -la "$OUT_DIR/rexglue/rexglue" "$OUT_DIR/xenosrecomp/XenosRecomp" \
       "$OUT_DIR/pkzlib/pkztool" "$OUT_DIR/pkzprep"
"$OUT_DIR/rexglue/rexglue" --version
