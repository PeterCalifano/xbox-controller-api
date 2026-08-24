# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

`xbox_controller_api` is a C++20 library for reading Xbox controller input, with
optional Python bindings (via gtwrap) and an optional ROS 2 overlay. It was
derived from Pietro Califano's `cpp_cuda_template_project` CMake template.

**The input API is implemented** (v1 is input only; no rumble or LED). See
`doc/controller_api.md`. The layering, which changes must preserve:

- `SGamepadState.h` — normalized snapshot aggregate; sticks `[-1,1]` with
  **Y positive up**, triggers `[0,1]`, plus sequence and timestamp counters
- `CGamepadSource.h` — abstract poll-model base; private snapshot behind a
  protected `setState()`, so a backend cannot publish a partial sample
- `GamepadFilters.h` — pure scalar math; deliberately has **no dependency on
  `SGamepadState`**
- `GamepadControls.h` — snapshot-level operations and `EGamepadButton`, the
  single authority for control identity, names and ordering
- `CSdlGamepadSource.h/.cpp` — the only hardware-facing class; SDL types stay
  behind a pimpl so the installed header has a layout independent of
  `__SDL2_ENABLED__`
- `CScriptedGamepadSource` — deterministic replay source for hardware-free tests
- `src/wrapped_impl/CGamepadWrapper.*` — flat facade for the Python bindings

Backends apply no conditioning: deadzone and edge detection are explicit
consumer decisions. A disconnect is reported, never healed — recovery requires
an explicit `open()`.

## Build Commands

**Primary entry point** is `build_lib.sh` (never needs ROS):

```bash
./build_lib.sh                                # Default: RelWithDebInfo, output to ./build
./build_lib.sh -t debug -j 8                  # Debug build, 8 parallel jobs
./build_lib.sh -t release -i                  # Release build + install
./build_lib.sh -N                             # Use Ninja generator
./build_lib.sh --clean                        # Clean rebuild
./build_lib.sh -p                             # Build the Python wrapper
./build_lib.sh -r                             # Rebuild only (skip CMake configure)
```

**Tests** (after build):
```bash
ctest --test-dir build --output-on-failure
ctest --test-dir build --output-on-failure -L python    # pytest-backed only
ctest --test-dir build --output-on-failure -L catch2    # compiled only
```

**Optional ROS 2 overlay** — see `doc/ros2_overlay.md` before changing it:
```bash
./build_ros2.sh --clean
```

Keep ROS-related changes confined to `ros2/` plus the documented root helpers,
docs, tests, markers, and the single ROS overlay workflow.

## Architecture

### Build system (`cmake/`)

Modular: each optional dependency has a `Handle*.cmake` module creating INTERFACE
targets namespaced via `LIB_NAMESPACE`. Key modules:

- `HandleGitVersion.cmake` — semver from git tags, writes VERSION, populates `config.h`
- `HandleWrapper.cmake` — gtwrap discovery/orchestration; dispatches to `HandlePythonWrapper.cmake`
- `cmake_utils.cmake` — `add_tests()` and `add_examples()` macros

**`add_tests()` and `add_examples()` take positional arguments.** Passing an
undefined variable expands to zero arguments and silently shifts every later
one. Pass `""` to leave a slot empty.

### Source layout

- `src/xbox_controller_api/` — core C++ library implementation
- `src/wrapped_impl/` — C wrapper layer for the Python bindings
- `src/utils/logging/` — dependency-free component logger
- `src/config.h.in` — CMake-configured header (version macros, feature flags)
- `src/global_includes.h` — shared utilities (ANSI colors, precision constants)

### Testing

Catch2 (auto-fetched if not found) for `test*.cpp`; pytest for `test*.py`; CTest
is the common runner. Tests live in `tests/xbox_controller_api_test/`, fixtures
in `tests/xbox_controller_api_fixtures/`.

### Consumer pattern

`examples/consumer_project/` demonstrates external use via `find_package()`.
Targets export as `xbox_controller_api::xbox_controller_api`.

## Conventions

- Default build type is **RelWithDebInfo** (stricter warnings than Debug)
- Version format: `MAJOR.MINOR.PATCH+<commit_hash>`, from git tags matching `v*.*.*`
- File extensions: `.h` for headers
- `ACHTUNG!` prefix in comments marks critical warnings
- Sanitizer builds via `-DSANITIZE_BUILD=ON`
- Logger env var is `XBOX_CONTROLLER_API_LOG_LEVEL`
- `ENABLE_SDL2` defaults **ON**; a missing SDL2 warns and stubs the backend out
  while keeping the full API surface. `ENABLE_SDL2_STRICT=ON` makes it a
  configure error instead, and CI uses that. Gate downstream logic on the
  resolved `SDL2_ENABLED`, never on the `ENABLE_SDL2` request
- The ROS overlay forces `ENABLE_SDL2=ON` **and** `ENABLE_SDL2_STRICT=ON`: it
  publishes real controller data, so a missing libsdl2-dev must fail the build
  rather than yield a node that can never open a device. The overlay consumes
  only installed public headers, so a header a ROS translation unit needs must
  be installed
- Source discovery uses plain `file(GLOB ...)` without `CONFIGURE_DEPENDS`, so
  **adding or deleting a source file needs a fresh configure**; an incremental
  `./build_ros2.sh` will fail on a stale cached list until run with `--clean`

## Removed template features

CUDA, OptiX, TensorRT, and the MATLAB wrapper were deliberately removed during
tailoring. Do not reintroduce them incidentally. Two intentional exceptions
remain and are **not** oversights:

- `.devcontainer/` and `configure_devcontainer.sh` keep opt-in CUDA support
  (defaults off) as reusable container infrastructure.
- `cmake/HandleWrapper.cmake` retains gtwrap-root resolution and `matlab.h`
  include discovery, because those sit on the shared code path the Python
  wrapper uses and are inert while the option is OFF.
