# Upstream (goliathret/EdgeOfTimeRecomp) Android patches

Patches here adapt the upstream game code to Android (platform layer: SAF file
dialogs, user dirs, no-op desktop features, disabled update checks, crash
handling). They are applied to the pinned `upstream` submodule by
`tools/ci/apply_patches.sh upstream patches/upstream` (same idempotent,
fail-loud mechanism as the SDK patches in `patches/rexglue-sdk/`).

Upstream is BSD-3-Clause; patches are minimal and upstreamable.
