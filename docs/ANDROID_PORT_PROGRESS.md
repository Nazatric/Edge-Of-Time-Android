# Android Port Progress Log

Every entry below is backed by a real GitHub Actions run. Run IDs are given so
any claim can be checked. Build logs are mirrored into
[issue #1](https://github.com/Nazatric/Edge-Of-Time-Android/issues/1) because
the raw Actions log archive host is unreachable from the dev sandbox.

**Nothing in this file has been executed on an Android device.** No device or
emulator is available to the agent. Every claim here is a *build-time* result.

---

## Current status

| Milestone | State | Evidence |
| --- | --- | --- |
| M0 Repo + dependency audit | ✅ done | `docs/ANDROID_PORT_AUDIT.md` |
| M1 ReXGlue SDK compiles **and links** for `arm64-v8a` | ✅ **done** | run `37994751899` (`BUILD_EXIT=0`) |
| M1a arm64 ELF verified + 16 KB page aligned | ✅ **done** | run `37996301874` (`LOAD align 0x4000`) |
| M2 Gradle project producing an installable APK | ✅ **done** | runs `38025918515` + `38026377028` |
| M2a APK verified (badging, arch, 16 KB, SHA-256) + prerelease published | ✅ **done** | see "APK delivery" below |
| M2b Upstream game code compiles for arm64-v8a | ✅ **done** | probe run `38030105699`: **56 passed, 0 failed** |
| M3 Android lifecycle / surface recreation | 🟡 largely done | patches 0010/0011 + surface acquire/release; **device-untested** |
| M5 Touchscreen controls (virtual XInput pad) | 🟡 implemented, **device-untested** | `patches/upstream/0004` (`touch_input.cpp`); registered in `ReeotApp::OnPreSetup` |
| M7 Game as loadable `libreeot_game.so` + launcher hook | 🟡 implemented, build **needs game files** | `android/game/CMakeLists.txt`, patch 0003 (creator export), launcher `TryBootGame` |
| M4 Vulkan device creation on a device | 🔒 blocked | needs hardware |
| M5 Touchscreen controls | ⬜ not started | — |
| M6 Game-file setup flow (SAF) | 🟡 native half done | `filesystem_android.cpp` + Java picker + probe |
| M7 Game-code integration (`libreeot_game.so` into the launcher) | ⬜ next | needs codegen outputs; pipeline ready (`codegen.yml`) |
| Gameplay / rendering / audio / FPS | 🔒 blocked | needs game dump **and** hardware |

### APK delivery (M2 evidence)

- **CI run:** https://github.com/Nazatric/Edge-Of-Time-Android/actions/runs/38026377028
  (APK job; the earlier run `38025918515` produced the identical verified APK)
- **Workflow artifact:** `edge-of-time-android-debug-apk` on that run
  (Actions → run → Artifacts; requires repo access; 30-day retention)
- **Prerelease (permanent):** https://github.com/Nazatric/Edge-Of-Time-Android/releases/tag/v0.1.0-android-alpha
  - asset: `app-debug.apk` — 41,009,575 bytes
  - https://github.com/Nazatric/Edge-Of-Time-Android/releases/download/v0.1.0-android-alpha/app-debug.apk
  - SHA-256: `78e1814ce1afaab0f1ddfa9c020ecbf5c9d825aa6fcb76b8bc3a4683685e834a`
- **Verified in CI** (`aapt2` / `unzip` / `llvm-readelf` on the packaged libs):
  `package: com.nazatric.edgeoftime.debug`, `versionName 0.1.0-android-alpha-debug`,
  `targetSdkVersion 35`, `application-isGame`,
  **`native-code: 'arm64-v8a'`**, and all three packaged libraries
  (`librexruntime.so` 91 MB, `librexgpu-xenos.so` 52 MB, `libedgeoftime.so` 2.7 MB)
  are **ELF64 AArch64 with 16 KB-aligned LOAD segments (0x4000)**.
- **Not verified:** install/launch on a device (none attached to the agent),
  rendering, audio, input, stability, FPS. The APK contains the real runtime
  and the real renderer-initialization path, but **no recompiled game code** —
  it reports the missing game files instead of faking gameplay.
- Note: the dev sandbox cannot download workflow artifacts (the Azure blob
  host is unreachable from it), so the runner publishes the release itself.

### Build output actually produced (run `37996301874`)

```
librexruntime.so     91,081,176 bytes   ELF64 AArch64   LOAD align 0x4000
librexgpu-xenos.so   52,416,248 bytes   ELF64 AArch64   LOAD align 0x4000
libSDL3.a            18,429,826 bytes
librexcodegen.a      70,272,804 bytes
liblibavcodec.a       4,950,362 bytes
libglslang.a / libSPIRV.a / libMachineIndependent.a ...
```

---

## What was fixed, in order, each from a real CI failure

| # | Symptom (from CI) | Root cause | Fix | Commit |
| --- | --- | --- | --- | --- |
| 1 | `pkg_check_modules(X11_XCB REQUIRED x11-xcb)` fails | `if(UNIX AND NOT APPLE)` matches Android | Added an `if(ANDROID)` branch linking `android`+`log` | `9303adb` |
| 2 | `no matching function for call to 'from_chars'` | Android libc++ has no floating-point `std::from_chars` (P0067R5) | Widened upstream's existing macOS fallback `portable_float_from_chars` to Android via `__cpp_lib_to_chars` | patch 0002 |
| 3 | `explicit specialization of undeclared template 'clock_time_conversion'` | Android libc++ has no `std::chrono::clock_time_conversion` | Widened upstream's `__APPLE__` shim to `__ANDROID__` | patch 0003 |
| 4 | `no member named 'jthread' in namespace 'std'` | libc++ gates `<stop_token>`/`jthread` behind `-fexperimental-library` | Added the flag | — |
| 5 | `use of undeclared identifier 'getcontext'` | **Bionic has no ucontext fiber API at all** | Wrote an AArch64 fiber backend with a hand-written AAPCS64 context switch | `bb4b...` |
| 6 | `no member named 'GetAndroidApiLevel' in namespace 'rex'` | SDK calls it but never ships it (Xenia leftover) | Implemented `rex::GetAndroidApiLevel()` | — |
| 7 | `'rex/main_android.h' file not found` | Header referenced by SDK, never published | Wrote it | — |
| 8 | `CMAKE_ASM_COMPILE_OBJECT` not set | `.S` needs `enable_language(ASM)` | Added | — |
| 9 | asm `unexpected token at start of statement` | My own bug: nested `/* */` in a block comment | Fixed | — |
| 10 | `PTHREAD_MUTEX_ROBUST` undeclared | Bionic has no robust mutexes | Gated robust-mutex recovery on glibc | patch 0008 |
| 11 | `'X11/Xlib-xcb.h' file not found` | `window_sdl.cpp` / `surface_gnulinux.cpp` built on Android | Added Android branches, swapped the platform surface source | patches 0001/0007 |
| 12 | `'rex/ui/surface_android.h' file not found` | SDK's Vulkan presenter needs it; never published | Implemented `AndroidNativeWindowSurface` | — |
| 13 | `unable to find library -lpthread` / `-lrt` | Bionic folds both into libc | Link `log`+`android` instead | — |
| 14 | `undefined symbol: OpenAndroidContentFileDescriptor` | Declared + called by SDK, never implemented | Implemented SAF resolution via `ContentResolver.openFileDescriptor` + `detachFd` | — |
| 15 | `LOAD align 0x1000` | Default 4 KB page layout | `-Wl,-z,max-page-size=16384`; verified `0x4000` | — |

### Headline finding

The published ReXGlue SDK contains **half-finished Android support**: it defines
`REX_PLATFORM_ANDROID`, reserves `kTypeIndex_AndroidNativeWindow` as the *first*
surface type, has a complete `vkCreateAndroidSurfaceKHR` presenter path, and
calls `rex::GetAndroidApiLevel()` and `OpenAndroidContentFileDescriptor()` —
but ships none of the files those depend on. `memory_posix.cpp:57` still carries
the commented-out `// #include "xenia/base/main_android.h"` it was derived from.

Items 5, 6, 7, 12 and 14 above are the missing pieces, now implemented in
`android/rex_android/`.

---

## Infrastructure notes

- **Builds run on GitHub Actions**, because the dev sandbox cannot reach
  `dl.google.com` and so cannot install the NDK. Runner has NDK 27/29/30,
  CMake 3.31.6, Ninja, JDK 17, 4 cores, 15 GB RAM.
- **NDK matrix tested** (run `37989832873`): the `from_chars`/`chrono` gaps are
  present on NDK 27 (clang 18), 29 and 30 (clang 21) alike, so they are a libc++
  limitation and not fixable by bumping the toolchain. Build is pinned to NDK 27.
- **Build parallelism capped at 2.** At `-j4` with PCH, clang was OOM-killed and
  ninja reported `subcommand failed` with no diagnostic at all.
- **SDK is patched, not forked.** All changes live in `patches/rexglue-sdk/` and
  are applied by `tools/ci/apply_patches.sh`, which is idempotent and fails loudly
  if a patch stops applying to the pinned submodule revision. This keeps upstream
  BSD-3-Clause attribution and history intact and keeps the changes upstreamable.

---

## Hard blockers (cannot be engineered around here)

1. **No game code exists publicly.** `upstream/generated/` holds only
   `rexglue.cmake`. The recompiled PowerPC C++ is produced by
   `rexglue codegen reeot_manifest.toml` from the builder's own
   `Default.xex` + `Data/GameLogic.dll` **with the title update applied**.
   CI cannot do this and neither can I — no legal copy exists here.
   *Therefore no APK produced by this repository can reach gameplay yet.*
2. **A shader cache is also required.** Without `generated/shader_cache.cpp`
   (≈10 min to generate from the user's own `.pkz` archives) CMake builds
   `shader_cache_empty.cpp` and the game cannot draw, by upstream's own design.
3. **No Android device or emulator is available to the agent.** No `adb`, no
   logcat, no tombstones, no GPU. Rendering, audio, input, stability and FPS are
   therefore all **unverified**, and no benchmark numbers will be invented.

---

## Patch inventory (16, all CI-verified to apply)

**SDK (`patches/rexglue-sdk/`)** — 0001 X11/Wayland off on Android · 0002/0003
libc++ shims · 0004 AArch64 fiber backend · 0005 core CMake Android sources ·
0006 global Android include dir · 0007 SDL window Android surface · 0008 no
robust mutex on Bionic · 0009 main_android include · 0010 SDL: clear the
ANativeWindow property on surfaceDestroyed · 0011 SDL window: refresh the
presenter surface on Android resize · **0012 `GetExecutableFolder()` resolves
the app `nativeLibraryDir` via JNI on Android** (so the guest `reeot_GameLogic`
module preload works).

**Upstream (`patches/upstream/`)** — 0001 `std::atomic_ref` → `__atomic`
builtins fallback (NDK libc++ lacks `atomic_ref`) · 0002 crash handler uses
libunwind on Android (Bionic has no `execinfo.h`) · 0003 `main.cpp` exports
`reeot_windowed_app_creator` (C symbol) on Android · 0004 on-screen touch
controls (`src/goliath/controller/touch_input.cpp`, virtual XInput gamepad,
registered in `ReeotApp::OnPreSetup`) · 0005 CMake host-tool overrides for
cross builds (`REEOT_HOST_REXGLUE/PKZTOOL/PKZPREP/XENOSRECOMP`).

## Regression gates (all green in CI)

- `native-core`: SDK compiles+links arm64; app lib builds; artifacts listed;
  readelf arch + 16 KB alignment gate.
- `upstream-android-probe`: **56/56** upstream sources compile for arm64-v8a
  (only `main.cpp`/`reeot_app.cpp` skipped — they need codegen output).
- `game-android-configure`: `android/game` configures for arm64-v8a (validates
  the game CMake + the patches/upstream series).
- `apk`: real debug APK, aapt2 badging, packaged-lib arch/alignment, SHA-256,
  prerelease publish — all gated.

## Next highest-priority action

**M2 is done** (APK + prerelease published — see "APK delivery" above).
**M7 is implemented**: the launcher boots `libreeot_game.so` when it is
packaged (GAME BOOTSTRAP HOOK in `edgeoftime_main.cpp`), and
`android/game/CMakeLists.txt` builds it from upstream sources + codegen
output. The remaining step to gameplay is running the secrets-gated
`game-android` job (`.github/workflows/codegen.yml`) with the user's game
files, then packaging `libreeot_game.so` + `reeot_GameLogic` into the APK.

Then, in order:

1. **First game build + APK with game code** — `workflow_dispatch` the
   "Game Codegen Pipeline" with `GAME_DEFAULT_XEX_B64`,
   `GAME_GAMELOGIC_DLL_B64`, `GAME_SHADER_DIR_B64` (+ `GAME_REFERENCE_PAKS_B64`),
   stage the produced libraries into `android/app/src/main/jniLibs/arm64-v8a/`,
   rebuild the APK.
2. **M3/M4/M5 device validation** — install/launch on a real device or
   emulator once one is available; capture logcat/tombstones; verify the
   renderer, audio, touch controls, and stability. Nothing about
   rendering/audio/FPS is verified yet.
