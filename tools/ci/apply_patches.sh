#!/usr/bin/env bash
# Apply the Android port's patches to the pinned ReXGlue SDK submodule.
# Patches live in patches/rexglue-sdk/ and are kept minimal and upstreamable.
set -euo pipefail

SDK_DIR="${1:-thirdparty/rexglue-sdk}"
PATCH_DIR="${2:-patches/rexglue-sdk}"

if [ ! -d "$SDK_DIR" ]; then
  echo "error: SDK dir '$SDK_DIR' not found" >&2
  exit 1
fi

shopt -s nullglob
patches=("$PATCH_DIR"/*.patch)
if [ ${#patches[@]} -eq 0 ]; then
  echo "no patches to apply"
  exit 0
fi

for p in "${patches[@]}"; do
  abs="$(realpath "$p")"
  if git -C "$SDK_DIR" apply --check "$abs" 2>/dev/null; then
    git -C "$SDK_DIR" apply "$abs"
    echo "APPLIED  $(basename "$p")"
  elif git -C "$SDK_DIR" apply --reverse --check "$abs" 2>/dev/null; then
    echo "ALREADY  $(basename "$p")"
  else
    echo "FAILED   $(basename "$p") -- patch does not apply to pinned SDK revision" >&2
    exit 1
  fi
done
