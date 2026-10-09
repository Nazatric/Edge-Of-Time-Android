# Android Port Audit — Spider-Man: Edge of Time Recompiled

> Status: living document. Every claim here was verified by inspecting actual
> source in the pinned submodules, or by reading real CI logs. Anything not yet
> verified is marked **UNVERIFIED**.

Upstream: <https://github.com/goliathret/EdgeOfTimeRecomp> (BSD-3-Clause)
SDK: <https://github.com/rexglue/rexglue-sdk>

---

## 1. Executive summary

The port is **more tractable than expected**. The three things that usually kill
a console-recomp Android port are already solved upstream:

| Risk | Expected | Actual finding |
| --- | --- | --- |
| x86 AVX/SSE in the PPC interpreter context | Hard blocker on ARM64 | **Solved** — the SDK uses [`simde`](https://github.com/simd-everywhere/simde) for all x86 intrinsics (`include/rex/ppc/intrinsics.h`, `include/rex/ppc/context.h`). simde lowers SSE/AVX to NEON on `__aarch64__`. |
| No Vulkan backend / D3D12-only | Would require a new renderer | **Solved** — `src/graphics/vulkan` exists in the SDK and `thirdparty/plume` provides the RHI used by the game's own renderer. |
| No windowing/audio abstraction | Would require a new platform layer | **Solved** — the SDK already uses **SDL3** (`thirdparty/sdl3`, `src/ui/window_sdl.cpp`, `src/ui/windowed_app_main_sdl.cpp`, `src/audio/sdl`). SDL3 has first-class Android support. |

Additionally, `include/rex/platform.h` **already defines `REX_PLATFORM_ANDROID`**:

```c
#elif defined(__ANDROID__)
#define REX_PLATFORM_ANDROID 1
#define REX_PLATFORM_LINUX   1
```

So Android is a recognised platform in the SDK, and ARM64 is already a first-class
target (`include/rex/platform.h:79`, `include/rex/platform/fpscr.h:53`; upstream
ships `linux-arm64-*` and `mac-arm64-*` CMake presets).

### The one blocker that cannot be engineered away

**There is no game code in any public repository.** `upstream/generated/` contains
only `rexglue.cmake`. The recompiled PowerPC C++ is produced at build time by:

```
rexglue codegen reeot_manifest.toml
```

which reads, from the builder's own machine:

- `assets/Default.xex`
- `assets/Data/GameLogic.dll`

**with the Xbox 360 title update applied** (upstream `docs/BUILDING.md`: a clean
disc copy "won't match the function addresses in the config files").

Consequences, stated plainly:

- No CI system — GitHub Actions included — can produce a *playable* APK, because
  CI has no legal copy of the game. GitHub Actions supplies a **build
  environment**, not missing game source.
- Any APK built in CI contains the engine, renderer, platform layer and UI, but
  **not the game**. It is a real binary, not a placeholder, but it cannot reach
  gameplay until codegen is run against the builder's own dump.
- The port must therefore be architected so the game target links in cleanly
  when a user with the files runs codegen locally.

### Second-order limitation: no runtime verification available to the agent

The development sandbox has no Android device, no emulator, and no `adb`.
Therefore **no runtime claim in this repository is agent-verified**: not frame
presentation, not audio, not input, not FPS. All such claims must come from a
human running the artifact on real hardware. No benchmark numbers will be
published here that were not measured on a named physical device.

---

## 2. Environment facts (measured, not assumed)

### Development sandbox

| Item | Value |
| --- | --- |
| CPU / RAM / disk | 2 cores / 3 GB / 20 GB |
| Toolchain | `gcc`, `g++`, `python3`, `git`, `gh` only |
| Android SDK/NDK/JDK/Gradle/CMake/Ninja | **absent** |
| Network egress | `github.com`, `codeload.github.com`, `api.github.com`, `registry.npmjs.org`, `pypi.org` **only** |
| `dl.google.com`, `developer.android.com`, `services.gradle.org` | **unreachable** (connection refused) |

Hence the NDK cannot be installed locally, and **all native building is done on
GitHub Actions runners**.

### GitHub Actions runner (measured — issue #1, run 37988643348)

| Item | Value |
| --- | --- |
| `ANDROID_HOME` | `/usr/local/lib/android/sdk` |
| NDKs installed | `27.3.13750724`, `29.0.14206865`, `30.0.16248370` |
| CMake / Ninja | 3.31.6 / 1.13.2 |
| JDK | OpenJDK 17.0.20.1 |
| Host clang | 18.1.3 |
| CPU / RAM / disk | 4 cores / 15 GB / 87 GB free |

This is a viable Android build machine.

### Agent GitHub permissions (verified)

| Capability | Status |
| --- | --- |
| Push to `Nazatric/Edge-Of-Time-Android` | ✅ verified (`admin: true`) |
| Push files under `.github/workflows/` | ✅ verified |
| Trigger / read workflow runs | ✅ verified (run 37988538096 executed) |
| Read raw Actions log archives | ❌ `results-receiver.actions.githubusercontent.com` is firewalled |
| Read repo Actions *settings* API | ❌ 403 "Resource not accessible by integration" |

**Workaround in use:** every CI job writes its configure/build output into a
comment on **issue #1**, which is readable through `api.github.com`. This is how
the agent reads real build errors.

---

## 3. Upstream architecture

```
EdgeOfTimeRecomp/
  src/
    core/        engine glue
    gamelogic/   game-specific patches, input, UI  (hooks into recompiled code)
    goliath/     front-end: controller binds, text, loading, UI
    gpu/         *** the native HLE renderer ***   (~40k LOC across src/)
    installer/   game-file acquisition + validation
    mods/        mod loader
    platform/    desktop platform layer  <-- must be replaced for Android
    ui/
  config/        per-function recompilation configs + hook tables
  generated/     ONLY rexglue.cmake (codegen output is gitignored)
  thirdparty/    XenosRecomp (shader translation), plume (RHI), PKZLib, stb
```

### Renderer status — the `BUILDING.md` warning is STALE

`upstream/docs/BUILDING.md` opens with:

> "The public repository doesn't contain the native renderer yet. A build from it
> recompiles the game code but **won't display a picture**."

**This is out of date.** `src/gpu/` in the current `main` (commit `340144e`,
2026-10-08) contains a substantial, complete-looking renderer:

| File | Lines | Role |
| --- | --- | --- |
| `src/gpu/device.cpp` | 1319 | device/adapter creation |
| `src/gpu/present.cpp` | 1057 | swapchain + presentation |
| `src/gpu/draw.cpp` | — | draw submission |
| `src/gpu/render_thread.cpp` | — | dedicated render thread |
| `src/gpu/resolve.cpp` | — | Xenos EDRAM resolve emulation |
| `src/gpu/pipeline/pipeline_cache.cpp` | — | PSO cache |
| `src/gpu/pipeline/pso_precache.cpp`, `pso_predictor.cpp` | — | **precompilation to avoid shader stutter — directly valuable on mobile** |
| `src/gpu/taa.cpp`, `patches/post_effects.cpp` | — | post-processing |
| `src/gpu/shaders/hlsl/*.hlsl` | — | host blit/copy/depth-derive shaders |

All of these are referenced from `upstream/CMakeLists.txt` (lines ~255–294), so
they are really built, not dead files.

Two backends exist: **D3D12** (Windows default) and **Vulkan** (Linux/macOS).
Android will use the Vulkan path. `REEOT_D3D12` must be forced OFF.

### Shader pipeline

Xbox 360 shaders are translated ahead-of-time by the `XenosRecomp` fork into
`generated/shader_cache.cpp`. Per upstream `BUILDING.md`, this takes ~10 minutes
and requires decompressing every `.pkz` (>7 GB of game data). If the file is
absent, CMake builds a stub (`src/gpu/shaders/shader_cache_empty.cpp`, referenced
at `CMakeLists.txt:228`) **and the game cannot draw**.

→ So even with codegen done, a user must *also* run the shader-cache target
against their own game data to get a picture. This must be documented in the
Android setup flow.

---

## 4. Dependency triage

Legend: ✅ works as-is on Android arm64 · 🟡 needs config/patch · 🔴 needs replacement · ⬜ unverified

### ReXGlue SDK vendored dependencies

| Dependency | Verdict | Notes |
| --- | --- | --- |
| `simde` | ✅ | The reason AVX is a non-issue. Lowers SSE/AVX→NEON. |
| `sdl3` | ✅ | First-class Android backend: window, GL/Vulkan surface, audio, input, sensors, lifecycle. The single most important asset for this port. |
| `vulkan-headers`, `vulkan-memory-allocator` | ✅ | Android ships a Vulkan loader. |
| `fmt`, `spdlog`, `tomlplusplus`, `utfcpp`, `xxHash`, `o1heap`, `cli11`, `stb` | ✅ | Portable C++. |
| `imgui` | ✅ | SDL3 + Vulkan backends; used for the in-game overlay. |
| `vulkan-loader` | 🟡 | Should **not** be built; link Android's system `libvulkan.so` instead. |
| `glslang`, `spirv-tools`, `spirv-headers`, `dxc`, `dxbc` | 🟡 | Shader *translation* tooling. Belongs on the **build host**, not the device. Must be host-built or excluded from the Android target. |
| `FFmpeg` | 🟡 | Used for XMA audio decode. Needs an arm64-v8a cross-build; FFmpeg supports Android officially. |
| `libmspack` | ⬜ | LZX decompression for XEX. Plain C, expected portable. |
| `tracy` | 🟡 | Disable on Android initially (`REXGLUE_ENABLE_TRACY=OFF`). |
| `moltenvk` | ✅ n/a | macOS only; excluded. |
| `renderdoc` | 🟡 | Desktop capture only; exclude from device build. |
| `catch2` | ✅ n/a | Tests, host-only. |

### Upstream desktop-only platform layer — `src/platform/` (must be addressed)

| File | Verdict | Android plan |
| --- | --- | --- |
| `file_dialog.cpp` | 🔴 | GTK3 on Linux. Replace with **Storage Access Framework** via JNI. |
| `user_dirs.cpp` | 🔴 | XDG/`%APPDATA%`. Replace with `getFilesDir()`/`getExternalFilesDir()`. |
| `desktop_shortcut.cpp` | 🔴 | Meaningless on Android — compile out. |
| `update_check.cpp` | 🟡 | Network self-update; disable for the Android build. |
| `fatal_dialog.cpp` | 🔴 | Replace with an Android dialog / logcat + crash screen. |
| `crash_handler.cpp` | 🟡 | POSIX signal handler should largely work under Bionic; must not fight Android's own tombstone handler. |
| `moltenvk.cpp` | ✅ n/a | macOS only. |
| `process.cpp`, `display.cpp` | 🟡 | Audit for X11/Win32 assumptions. |

### Android-specific concerns not present upstream at all

| Concern | Status |
| --- | --- |
| Activity lifecycle (pause/resume/surface destroy) | 🔴 to implement |
| Surface recreation on rotation / app switch | 🔴 to implement |
| Touchscreen controls | 🔴 to implement (Phase Four) |
| Audio focus / interruption | 🔴 to implement |
| 16 KB memory page size (Android 15+) | 🔴 must verify all `.so`s are 16 KB-aligned |
| Scoped storage | 🔴 to implement |
| `mapped_memory_posix.cpp` / guest 512 MB address space reservation | ⬜ **highest technical risk** — see below |

### Highest remaining technical risk: guest memory mapping

`src/core/mapped_memory_posix.cpp` and `src/core/memory_posix.cpp` reserve the
Xbox 360 guest address space. Xenia-lineage runtimes typically want a large
fixed-address reservation (the 360's physical/virtual layout). On 64-bit Android
this is usually fine (`mmap` with `MAP_NORESERVE`), but:

- Android may not honour a specific fixed base address.
- Per-process memory limits and the low-memory killer are far stricter than desktop.

This is flagged as the top item to validate once the core compiles.

---

## 5. Build strategy

Because the NDK is unreachable from the sandbox, the loop is:

```
edit in sandbox → commit → push → GitHub Actions builds with the real NDK
  → job posts configure/build logs to issue #1 → agent reads logs via api.github.com
  → diagnose → fix → push → repeat
```

Staged milestones, each gated on real CI output:

1. **M1** — configure + compile ReXGlue SDK core libs for `arm64-v8a`. ✅ **done**
   — full compile *and link*, run `37994751899`; arm64 ELF + 16 KB alignment
   verified in run `37996301874`. See `docs/ANDROID_PORT_PROGRESS.md`.
2. **M2** — Gradle project producing an installable `arm64-v8a` APK.
3. **M3** — Android platform layer (storage, lifecycle, surface) replacing `src/platform/`.
4. **M4** — SDL3 + Vulkan device creation on an Android surface.
5. **M5** — touchscreen controller derived from real upstream pad mappings.
6. **M6** — game-file setup flow (SAF) + codegen handoff documentation.

Milestones 1–4 are verifiable in CI as *builds*. Milestones 4–6 cannot be
verified as *working* without a device and a user-supplied dump.

---

## 6. Reproducible build commands

### Native core, Android arm64 (what CI runs)

```bash
NDK="$ANDROID_HOME/ndk/27.3.13750724"
cmake -S thirdparty/rexglue-sdk -B out/android -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DCMAKE_BUILD_TYPE=Release \
  -DREXGLUE_BUILD_TESTS=OFF \
  -DREXGLUE_ENABLE_TRACY=OFF
cmake --build out/android --parallel
```

### Full game build (requires YOUR OWN legally dumped game files)

Not runnable in CI. Documented for users who own the game:

```bash
# 1. Place Default.xex and Data/GameLogic.dll (title update applied) in upstream/assets/
# 2. Generate the recompiled PowerPC C++:
rexglue codegen upstream/reeot_manifest.toml
# 3. Generate the shader cache from your game's .pkz archives (~10 min):
cmake --build <builddir> --target reeot_shader_cache
# 4. Build the APK (see docs/BUILDING-ANDROID.md once M2 lands)
```

---

## 7. Licensing / attribution

- Upstream EdgeOfTimeRecomp: **BSD-3-Clause** — fork permitted with attribution;
  `upstream/LICENSE` is retained via submodule.
- Submodules are referenced, **not vendored by copy**, so upstream licences and
  history stay intact.
- **No game assets, dumps, executables, title updates, or derived game data are
  present in this repository, and none will be added.**

---

## 8. Open questions / unverified items

- ✅ RESOLVED: Android's libc++ gaps (`from_chars`, `clock_time_conversion`,
  `jthread`) — fixed via patches 0002/0003 and `-fexperimental-library`.
- ✅ RESOLVED: Bionic has no ucontext fibers — AArch64 fiber backend written.
- ✅ RESOLVED: SDK's Android support was incomplete (`main_android.h`,
  `GetAndroidApiLevel`, `surface_android.h`, `OpenAndroidContentFileDescriptor`
  all referenced but never shipped) — all now implemented in `android/rex_android/`.
- ✅ RESOLVED: 16 KB page alignment — `-Wl,-z,max-page-size=16384`, verified.
- ✅ RESOLVED: FFmpeg cross-builds for arm64-v8a (libavcodec/libavutil produced).
- ⬜ Does `mapped_memory_posix.cpp` reserve an address range Android will grant?
  **Still the top runtime risk** — compiles, but untestable without a device.
- ⬜ Does FFmpeg cross-build cleanly for arm64-v8a within this tree?
- ⬜ Is `plume`'s Vulkan backend free of desktop-only extension assumptions?
- ⬜ Do the host HLSL shaders in `src/gpu/shaders/hlsl/` compile to SPIR-V for mobile?
- ⬜ Are all produced `.so`s 16 KB-page aligned?
- ⬜ Real Mali and Adreno behaviour — **requires hardware, currently unavailable.**
