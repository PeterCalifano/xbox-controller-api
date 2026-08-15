# Xbox 360 / xpad-class Controller Input — Implementation Plan

Status: **implemented and verified (2026-08-15)**. All six planned batches landed, plus one batch
added during implementation. See [Implementation record](#implementation-record) for the
batch-to-commit mapping, the deviations from this plan, and the outstanding follow-ups.

## Context

`xbox_controller_api` is a C++20 library skeleton (template-derived) whose actual feature — reading
Xbox controller input on Linux — is not implemented yet. Placeholder seams mark where real code goes:
`src/xbox_controller_api/placeholder.*`, `src/wrapped_impl/CWrapperPlaceholder.*`, and the ROS
overlay's `conversions.cpp` `EDIT ME` fences. This plan replaces those seams with a real,
input-only (v1) controller API.

**Detected case:** native C++ library as the foundation, with the Python-binding path wired as a
first-class consumer. The ROS 2 overlay and viewer/GUI integration get **no features and no
samples** this iteration — the overlay must merely keep compiling, retargeted to a real installed
header. The architecture (poll-model source + pure filter layer) is deliberately shaped so ROS and
viewer consumers can be added later without breaking contracts.

**Confirmed decisions:**

- Hardware: wired Xbox 360 USB as reference; generic design for any xpad-class pad (SDL mapping DB).
- Backend: **SDL2 GameController API** (libsdl2-dev 2.30 present on dev machine + devcontainer;
  libevdev headers absent). Kernel `xpad` assumed; no xboxdrv/xpadneo.
- v1 = input only (no rumble/LED).
- Consumers wired now: C++ dummy example + Python dummy example. ROS 2 and GUI: none.
- **Y-axis: up = +1** (backend negates SDL's raw down-positive Y; documented deviation).
- **`ENABLE_SDL2` defaults ON** (non-strict warn-and-stub when missing; `ENABLE_SDL2_STRICT` for CI).
- **Hot-unplug: explicit re-`open()`** — no auto-reconnect inside `update()`.
- **Deadzone: 0.0 pass-through default** everywhere; opt-in via filter / `setStickDeadzone()`.

## Architecture

```
Xbox pad → kernel xpad → SDL2 GameController (mapping DB)
                              ↓
              CSdlGamepadSource : CGamepadSource        (thin, pimpl, HW numbering stays here)
                              ↓
                    SGamepadState snapshot              (normalized: sticks [-1,1] up=+1, triggers [0,1])
                              ↓
              GamepadFilters (pure functions)           (deadzone, edge detection — explicit, testable)
                              ↓
        consumers: C++ app | CGamepadWrapper→Python | (later: ROS node, viewer)
```

Key contracts:

- **No silent behavior**: `open()` failure and hot-unplug are explicit (`false` + `lastError()`,
  snapshot zeroed via `MakeDisconnectedState`). `ENABLE_SDL2=OFF` builds keep the full API surface;
  `open()` fails with a fixed diagnostic string and `isBackendAvailable()` returns false.
- **Event-pump contract**: backend never pumps/consumes the SDL event queue — it polls via
  `SDL_GameControllerUpdate()`. Safe beside a host app that owns the SDL loop (`SDL_InitSubSystem`
  is refcounted; hints set only when we init SDL first; flush joystick events only when we own init).
- Lazy SDL init: ctor makes no SDL calls; `open()` inits, `close()`/dtor balances.
- Multi-pad policy v1: `open(i32JoystickIndex = -1)` = lowest-index game controller; explicit index
  is the extension seam.
- No backend filtering: raw normalized values in `SGamepadState`; deadzone/edges are consumer-side.

## New files

```
cmake/HandleSDL2.cmake
src/xbox_controller_api/SGamepadState.h            (aggregate + MakeDisconnectedState decl)
src/xbox_controller_api/CGamepadSource.h           (abstract poll-model base)
src/xbox_controller_api/GamepadFilters.h/.cpp      (pure: ApplyRescaledDeadzone, NormalizeStickAxis,
                                                    NormalizeTriggerAxis, InvertAxis,
                                                    ClassifyButtonEdge, MakeDisconnectedState impl)
src/xbox_controller_api/CScriptedGamepadSource.h/.cpp   (deterministic replay/mock source)
src/xbox_controller_api/CSdlGamepadSource.h/.cpp   (pimpl; #ifdef __SDL2_ENABLED__ real/stub split)
src/wrapped_impl/CGamepadWrapper.h/.cpp            (flat-method facade for gtwrap)
examples/xbox_controller_api_examples/example_read_controller.cpp
examples/python/example_read_controller.py         (inert to build; examples/ has COLCON_IGNORE)
tests/xbox_controller_api_test/testGamepadFilters.cpp
tests/xbox_controller_api_test/testGamepadState.cpp
tests/xbox_controller_api_test/testScriptedGamepadSource.cpp
tests/xbox_controller_api_test/testSdlGamepadSource.cpp    (invariant-style; passes with/without SDL2/pad)
tests/xbox_controller_api_test/testGamepadWrapperPython.py (skips cleanly when HAS_WRAPPER false)
doc/controller_api.md
```

**Deleted** (final batch, after retarget): `src/xbox_controller_api/placeholder.h/.cpp`,
`src/wrapped_impl/CWrapperPlaceholder.h/.cpp`. (`wrapper_placeholder.i` stays — unreferenced example.)

**Modified**: root `CMakeLists.txt`, `src/CMakeLists.txt`, `src/config.h.in`, `src/wrap_interface.i`,
`src/bin/example_program.cpp`, `examples/xbox_controller_api_examples/example_build.cpp` (renamed to
`example_scripted_replay.cpp` — the template excludes the historic `example_build` name from the
build via `EXCLUDED_LIST`, which stays untouched by user decision),
`examples/consumer_project/example_project.h/.cpp`, `ros2/xbox_controller_api/CMakeLists.txt`,
`ros2/xbox_controller_api_ros/src/conversions.cpp`,
`ros2/xbox_controller_api_ros/test/test_conversions.cpp`, `.github/workflows/build_linux.yml`,
`README.md`, `CLAUDE.md`, `doc/ros2_overlay.md`.

## API sketch

Conventions: C/S/E prefixes, Hungarian + trailing `_`, `#pragma once`, Allman 4-space, Doxygen on
all public API, `[[nodiscard]]`/`noexcept`, namespace `xbox_controller_api`.

- `SGamepadState` — aggregate (as implemented in Batch 1): `dLeftStickX_/Y_`, `dRightStickX_/Y_`
  in [-1,1] (up=+1), `dLeftTrigger_/dRightTrigger_` in [0,1]; buttons `bButtonA_/B_/X_/Y_`,
  `bLeftShoulder_`, `bRightShoulder_`, `bLeftStickClick_`, `bRightStickClick_`, `bBack_`,
  `bStart_`, `bGuide_`, `bDpadUp_/Down_/Left_/Right_`; `bConnected_`, `ui64SequenceId_` (bumps
  once per consumed sample), `ui64TimestampNs_` (steady_clock; 0=never). Free
  `MakeDisconnectedState(seq, ts)` = canonical zeroed snapshot (the unplug contract).
  Batch 5 facade getters must mirror these field semantics/names.
- `CGamepadSource` — abstract base (as implemented): pure `virtual bool update()`; the base itself
  stores the snapshot and provides non-virtual `state()`/`connected()` plus a protected
  `setState()` so a partially updated snapshot is never observable. Protected copy/move
  (anti-slicing, Core Guidelines C.67).
- `CScriptedGamepadSource` semantics (as implemented): `pushFrame()` forces `bConnected_=true`
  (disconnects only via `pushDisconnect()`); sequence ids are stamped on consumption; an
  exhausted queue returns false but leaves the last snapshot standing (script end ≠ disconnect).
- `GamepadFilters.h` — `EButtonEdge {None,Pressed,Released,Held}`;
  `ApplyRescaledDeadzone(x,d)` = `sign(x)·(|x|−d)/(1−d)` clamped, `d<=0` → clamp-only, `d>=1` → 0;
  `NormalizeStickAxis(int16)` (−32768 clamps to −1 exactly), `NormalizeTriggerAxis(int16)`,
  `InvertAxis`, `ClassifyButtonEdge(prev,curr)`. All hardware numbering stays in the SDL .cpp.
- `CScriptedGamepadSource final` — `pushFrame`, `pushDisconnect`, `pendingFrameCount`; `update()`
  pops a frame (false on empty/disconnected). Test + replay backend.
- `CSdlGamepadSource final` — non-copyable; `static bool isBackendAvailable() noexcept`;
  `bool open(int32 = -1)`, `void close() noexcept`, `deviceName()`, `const string& lastError()`;
  `struct SImpl` pimpl keeps SDL types out of the header. Single-thread contract documented.
  `-Wconversion` is ON in RelWithDebInfo: explicit casts on all Sint16/Uint8 math.
- `CGamepadWrapper` (wrapped_impl) — flat gtwrap-safe surface: `open()`, `openIndex(i32)` (no
  overloads in .i), `close`, `update`, `connected`, `deviceName`, `lastError`,
  `setStickDeadzone`/`stickDeadzone` (default 0.0, sticks only), per-axis double getters,
  per-button bool getters, `sequenceId()`. OFF-build: compiles against stub, `open()` False,
  getters return defaults — Python branches explicitly, no exception.

## CMake changes

1. **`cmake/HandleSDL2.cmake`** — follow `HandleZeroMQ.cmake` (pkg-config precedent) +
   `HandleOpenGL.cmake` (find_package precedent): `include_guard(GLOBAL)`;
   `option(ENABLE_SDL2 ... ON)`, `option(ENABLE_SDL2_STRICT ... OFF)`; `handle_sdl2(TARGET <t>)`
   via `cmake_parse_arguments`; INTERFACE target **always created** (even when OFF);
   `find_package(SDL2 CONFIG QUIET)` → `SDL2::SDL2`, fallback `pkg_check_modules(... IMPORTED_TARGET sdl2)`;
   on found: link + `__SDL2_ENABLED__=1` + `set(SDL2_ENABLED ON PARENT_SCOPE)`;
   missing: STRICT→FATAL_ERROR else WARNING + OFF.
2. **Root `CMakeLists.txt`**: namespacing block (~l.119) `set(SDL2_COMPILE_TARGET
   "${LIB_NAMESPACE}_sdl2_compile_interface")`; include+call handler next to the other handlers;
   `if(SDL2_ENABLED) list(APPEND EXPORT_TARGET_DEPS SDL2)` in the ~l.311 block (feeds
   `find_dependency` in the installed Config); status line in the summary block.
3. **`src/CMakeLists.txt`**: `set(XBOX_CONTROLLER_API_HAS_SDL2 ${SDL2_ENABLED})` before
   `configure_file` (l.4); mirror the ENABLE_OPENGL link block (l.79–81) and installable_targets
   block (l.114–116) for SDL2.
4. **`src/config.h.in`**: `#cmakedefine XBOX_CONTROLLER_API_HAS_SDL2` (the existing l.37 slot area).
5. **ROS shim `ros2/xbox_controller_api/CMakeLists.txt`**: one line next to the existing forces:
   `set(ENABLE_SDL2 OFF CACHE BOOL "..." FORCE)` — ROS CI and package.xml stay untouched.
6. No new source-dir CMakeLists: `src/xbox_controller_api/` and `src/wrapped_impl/` glob new files
   and install headers automatically (installed as `<xbox_controller_api/SGamepadState.h>` etc.).

## Seam retirement

- `conversions.cpp` include fence → `#include <xbox_controller_api/GamepadFilters.h>` (installed
  public header, per doc/ros2_overlay.md's stated intent); body fence → keep gain/bias, call
  `ApplyRescaledDeadzone(dAdaptedInput_, 0.0)`.
- `test_conversions.cpp:5–12` currently asserts the `multiplyBy2` contract
  (`EvaluateTemplateCore(3.0,2.0,1.0)==14.0`) → update to in-domain values, e.g.
  `EvaluateTemplateCore(0.2, 2.0, 0.1) == 0.5`.
- Rewrite placeholder consumers: `src/bin/example_program.cpp` (backend availability + one
  open/update attempt, exit 0), `example_build.cpp` (pure API only — identical in OFF builds),
  `examples/consumer_project/*` (installed-header consumption of pure API).
- `.i` update: replace the `CWrapperPlaceholder` mirror with `CGamepadWrapper` using gtwrap
  spellings (`string`, `int32_t`; includes INSIDE the namespace; **no semicolon after the closing
  namespace brace** — ACHTUNG). Verify `uint64_t` acceptance for `sequenceId`; fall back to
  `size_t` if generation rejects it.

## Tests (all hardware-free; auto-discovered `test*.cpp`/`test*.py`)

- `testGamepadFilters.cpp`: deadzone zero/boundary (`x=±d`)/full-range (`±1→±1`)/sign-symmetry/
  monotonicity/`d=0` clamp-only/`d>=1`→0/out-of-range clamp; stick normalize (−32768→−1 exact,
  32767→1, 0→0); trigger normalize (negatives→0); InvertAxis; all four button edges.
- `testGamepadState.cpp`: default-zero aggregate; `MakeDisconnectedState` zeroing contract.
- `testScriptedGamepadSource.cpp`: empty-queue behavior, frame echo, disconnect frame, edge
  detection across replayed frames.
- `testSdlGamepadSource.cpp`: invariants valid in every build/hardware combination — no SDL calls
  pre-open; pre-open state zeroed; `!isBackendAvailable()` ⇒ `open()` false + non-empty
  `lastError()`; open-true ⇒ connected + non-empty deviceName; `close()` idempotent. Never asserts
  a specific `open()` outcome.
- `testGamepadWrapperPython.py`: skipif not `HAS_WRAPPER`; failed-open ⇒ zeroed getters + error
  string; `setStickDeadzone` round-trip; getter types.

## Docs and CI

- `doc/controller_api.md` (style of `doc/logging.md`): Linux setup (`xpad`, `/dev/input/by-id/`,
  `evtest`, permissions note — no broad `input`-group advice), SDL coexistence contract, mapping
  table (field ↔ SDL_CONTROLLER_* ↔ physical control), axis conventions incl. the Y-inversion,
  deadzone math + suggested 0.10–0.20, disconnect/re-open contract, OFF-build behavior, running
  both examples, flag table.
- README.md + CLAUDE.md: `ENABLE_SDL2`/`ENABLE_SDL2_STRICT` rows; README controller quickstart
  pointer. `doc/ros2_overlay.md`: seam paragraph now points at the exported public header.
- `.github/workflows/build_linux.yml`: add `libsdl2-dev` to both apt lists; add
  `-DENABLE_SDL2=ON -DENABLE_SDL2_STRICT=ON` to configure so missing dep fails loudly. ROS CI
  untouched (shim forces OFF). Devcontainer already installs libsdl2-dev.

## Staging

Each batch leaves everything compiling; the repository batch workflow applies (no commits without
explicit authorization, no Conventional-Commits prefixes, no AI trailers).

| # | Batch | Verification | Status |
|---|-------|--------------|--------|
| 1 | Pure core: SGamepadState, CGamepadSource, GamepadFilters, CScriptedGamepadSource + their 3 test files | `./build_lib.sh` + `ctest --test-dir build --output-on-failure` | done — `ed7c959` |
| 2 | CMake SDL2 plumbing (HandleSDL2, root, src/, config.h.in, ROS shim force-OFF) | `./build_lib.sh` (log shows SDL2 ON); `-DENABLE_SDL2=OFF` configure+build; `./build_ros2.sh` | done — `cdfa00a` |
| 3 | SDL backend (CSdlGamepadSource + testSdlGamepadSource) | both ON and OFF builds + ctest green; optional manual pad smoke | done — `943691f` |
| 4 | Examples/bin/consumer rewrite + example_read_controller (50 Hz poll, edge-printed buttons, bounded runtime, quit-on-Start, graceful no-pad) | `./build_lib.sh`; run example with/without pad | done — `26665d8`, `b098e00` |
| 5 | Wrapper: CGamepadWrapper, wrap_interface.i, Python sample, pytest | `./build_lib.sh -p`; `ctest -L python`; run Python sample | done — `c5061e7` |
| 5b | **Added during implementation.** Control identity API (GamepadControls) + removal of three duplicated control tables | `./build_lib.sh -p`; ON and OFF builds; `./build_ros2.sh` | done — `b5aaf68` |
| 6 | Seam retirement (conversions retarget + ROS test values, delete placeholders), docs, CI | `./build_lib.sh && ./build_lib.sh -p && ./build_ros2.sh`; grep confirms no load-bearing "placeholder" | done — staged, awaiting commit |

## Repo-specific hazards to respect during implementation

- `add_tests()`/`add_examples()` are **positional** — pass `""` for empty slots (existing call sites
  already correct; don't disturb them).
- `-Wconversion` active in default RelWithDebInfo: explicit casts on all int16/uint8 → double math.
- Don't include `src/global_includes.h` in new code (bare RESET/RED macros collide with SDL).
- Use `CLogger` (instance-based, `XBOX_CONTROLLER_API_LOG_LEVEL`) for diagnostics; no per-frame spam.
- New ROS-visible headers must be reachable through the **installed** export, not the private
  source-tree include path.
- AGENTS.md governs commits: imperative subjects, bullet bodies with blank lines, optional
  `[MAJOR]` tag, **never** Co-Authored-By trailers; staging/commit only on explicit authorization.

## Implementation record

Completed on `feature/implement-first-prototype`, 2026-08-15. This section records what actually
happened, so the plan above stays readable as the original design rather than being rewritten.

### Deviations from the plan, with rationale

1. **`-Wfloat-equal` was missing from the hazard list.** It sits in the same `_safety_warnings`
   group as `-Wconversion` (`cmake/HandleCompilerFlags.cmake:42`) and is equally active by default,
   which makes `REQUIRE(x == 1.0)` unusable. Every floating-point assertion goes through Catch2's
   `WithinAbs(expected, tolerance)`, with a zero tolerance where bit-exactness is the requirement.
2. **SDL2 export safety.** The plan's unconditional `list(APPEND EXPORT_TARGET_DEPS SDL2)` combined
   with a `pkg_check_modules(... IMPORTED_TARGET sdl2)` fallback would have broken installed
   consumers: `PkgConfig::SDL2_PC` cannot be recreated by `find_dependency(SDL2)`. The fallback
   branch links the resolved flags instead, and `SDL2` is advertised only when discovery went
   through `find_package(SDL2 CONFIG)`. Verified by installing the fallback build and confirming the
   exported target file carries a concrete `libSDL2.so` path and zero `PkgConfig::` references.
3. **`SDL2_ENABLED` is the single downstream authority**, mirroring the `ENABLE_ZEROMQ`/
   `ZEROMQ_ENABLED` precedent: `ENABLE_SDL2` stays the request, `SDL2_ENABLED` the resolved outcome.
   Nothing force-writes the cache, so installing `libsdl2-dev` later is picked up on reconfigure.
4. **Batch 5b was added.** Three copies of the control mapping had accumulated (C++ example, the
   monitor, the Python example). `GamepadControls.h/.cpp` now owns `EGamepadButton`,
   `AllGamepadButtons`, `GetButton`, `GetGamepadButtonName`, a snapshot overload of
   `ClassifyButtonEdge`, and `ApplyStickDeadzone`. It is a new header rather than an addition to
   `GamepadFilters.h`, which deliberately keeps no dependency on `SGamepadState`.
5. **`example_build.cpp` renamed to `example_scripted_replay.cpp`.** The old name was matched by
   `examples/CMakeLists.txt`'s `EXCLUDED_LIST "example_build"`, so that example had never been
   compiled. The rename also makes it a real, verified build target.
6. **`example_program.cpp` renamed to `xbox_controller_monitor.cpp`** and reshaped from a one-shot
   probe into an installed live diagnostic: 50 Hz paced polling, report-on-change with a heartbeat,
   raw unconditioned values so drift stays visible, and `sigaction`-based SIGINT/SIGTERM shutdown.
7. **`CGamepadWrapper` caches one conditioned snapshot per poll** instead of re-running the deadzone
   inside every accessor, and gained indexed accessors (`buttonCount`, `buttonName`,
   `buttonPressed`) so a script can iterate controls generically.
8. **The ROS contract change reached further than the plan predicted.** Besides
   `test_conversions.cpp`, the launch integration test `test_spinup_launch.py` also asserted the old
   `multiplyBy2` result; it produced 5 failures until its request/response values were moved
   in-domain (input `-0.25` with the configured gain 2.0 and bias 1.0 yields exactly 0.5).
9. **`CSdlGamepadSource::update()` flushes queued joystick and controller events.** The poll-only
   contract removes the obligation to read the SDL event queue but not the consequence of
   generating entries in it, and that queue is capped.

### Verification performed

| Command | Result |
|---|---|
| `./build_lib.sh -p` | 37/37 tests, zero warnings under `-Wall -Wextra -Wpedantic -Wconversion -Wfloat-equal -Wnon-virtual-dtor -Wnull-dereference -Wsuggest-override` |
| `-DENABLE_SDL2=OFF` build (+ wrapper) | 37/37 tests, zero warnings; `open()` false with the fixed diagnostic; getters at rest |
| `ENABLE_SDL2_STRICT` matrix | available → ON; hidden + strict → configure error, exit 1; hidden + non-strict → warning and stub |
| pkg-config fallback install | export self-contained, `find_dependency` free of SDL2 |
| `./build_ros2.sh` | 4 packages, 11 tests, 0 failures |
| Fresh install + consumer project | builds warning-free against the installed package and runs |
| Live hardware | SDL reports "X360 Controller" at index 0; C++ and Python samples produce identical output at a measured 50 Hz |

### Outstanding follow-ups

- `examples/CMakeLists.txt:3` still sets `EXCLUDED_LIST "example_build"`, which after the rename
  matches no file. Dead configuration, and a trap if that filename ever returns.
- Source discovery uses plain `file(GLOB ...)` without `CONFIGURE_DEPENDS`, so adding or removing a
  source file requires a fresh configure. An incremental `./build_ros2.sh` fails on the stale cached
  list until it is run with `--clean`; this bit the placeholder deletion in batch 6.
- Pre-existing and untouched: `CMakeLists.txt` appends `OpenGL::GL` to `EXPORT_TARGET_DEPS`, which
  becomes `find_dependency(OpenGL::GL)` — not a package name. Latent until `ENABLE_OPENGL=ON`.
- `install/` is not pruned by `cmake --install`, so a stale tree can retain headers that are no
  longer produced. Remove the directory before trusting an installed-header listing.
- Deferred to v2 as designed: rumble and LED output, multi-pad enumeration beyond an explicit index,
  and ROS or viewer feature work.
