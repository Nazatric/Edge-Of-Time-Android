# Codegen pipeline (bring-your-own-game-files)

This directory implements the pipeline that turns the user's own copy of
Spider-Man: Edge of Time into recompiled, Android-ready game code. **No game
files are required to build the host tools or to run CI up to the codegen
step** — everything except the two game files (and the optional shader
containers) is already in this repository.

Full technical trace: `docs/ANDROID_PORT_CODEGEN.md`.

## What it does

| Script | Purpose | Needs game files? |
| --- | --- | --- |
| `build_host_tools.sh` | Builds `rexglue` (codegen CLI) and `XenosRecomp` (shader recompiler) natively for the build machine | **No** |
| `run_codegen.sh <game-dir>` | Stages the game files into `upstream/assets/`, runs `rexglue codegen`, optionally generates `generated/shader_cache.cpp` | **Yes** (your own dump) |

## Legal input requirements (your own copy of the game)

1. `Default.xex` — with the **title update applied** (the function addresses
   in `upstream/config/*.toml` match the patched executable only).
2. `Data/GameLogic.dll` — with the **title update applied**.
3. `shaders/` (optional but required for graphics) — the Xenos shader
   containers extracted from your game data (`.pkz` → unpack → containers;
   see `docs/ANDROID_PORT_CODEGEN.md` §3).

The files are copied into `upstream/assets/`, which is **gitignored inside
the upstream submodule** — they can never be committed by accident, and none
of these scripts touch git.

## Running locally

```bash
# 1. Host tools (one-time, or when the SDK changes).
bash tools/codegen/build_host_tools.sh

# 2. Codegen from your game files.
bash tools/codegen/run_codegen.sh /path/to/your/game/files
```

## Running in CI (GitHub Actions)

`.github/workflows/codegen.yml` runs the same pipeline on GitHub-hosted
runners via `workflow_dispatch`. The game files are supplied as **repository
secrets** (base64-encoded), decoded onto the runner, used, and deleted at the
end of the job. They are never committed and never appear in git history.

Set these secrets on the repository (Settings → Secrets and variables →
Actions → New repository secret):

| Secret | How to create |
| --- | --- |
| `GAME_DEFAULT_XEX_B64` | `base64 -w0 Default.xex` |
| `GAME_GAMELOGIC_DLL_B64` | `base64 -w0 Data/GameLogic.dll` |
| `GAME_SHADER_DIR_B64` (optional) | `tar czf - -C /path/to/shaders . \| base64 -w0` |

Then run the **"Game Codegen Pipeline"** workflow from the Actions tab.

### What the workflow produces

- `host-tools` job: `rexglue` + `XenosRecomp` binaries (runs on every push to
  `tools/codegen/**` too, as a regression check — no secrets needed).
- `codegen` job: `upstream/generated/` (the recompiled game code) as the
  `generated-game-code` workflow artifact. **This artifact is derived from
  your game files** — it lives only in your repository's Actions artifacts
  (short retention) and is yours to download or delete.
- `shader-cache` job (if `GAME_SHADER_DIR_B64` is set): `generated/shader_cache.cpp`.
- `game-android` job: configures and builds the game for arm64-v8a against
  the patched SDK and reports exactly what compiles.

## Security notes

- The secrets exist only in GitHub's secret store and in the runner's
  environment for the duration of the job.
- The workflow never runs `git add`/`git commit`/`git push` on anything
  containing game files, and `upstream/assets/` is gitignored.
- The produced artifacts contain code derived from your game dump. Keep the
  repository private or delete the artifacts when finished. (This repository
  is currently public — the artifacts are only visible to users with write
  access, but treat the code as your own responsibility.)
