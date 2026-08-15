# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

`xbox_controller_api` is a C++20 library for reading Xbox controller input, with
optional Python bindings (via gtwrap) and an optional ROS 2 overlay. It was
derived from Pietro Califano's `cpp_cuda_template_project` CMake template.

**The controller API itself is not implemented yet.** The library currently
exposes placeholders that mark where real code goes:

- `src/xbox_controller_api/placeholder.h` / `.cpp` — core library seam
- `src/wrapped_impl/CWrapperPlaceholder.h` / `.cpp` — wrapper-facing facade
- `ros2/xbox_controller_api_ros/src/conversions.cpp` — ROS core-call seam,
  marked with `EDIT ME` comments

Keep these seams compiling. Replacing them is the actual feature work.

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

## Removed template features

CUDA, OptiX, TensorRT, and the MATLAB wrapper were deliberately removed during
tailoring. Do not reintroduce them incidentally. Two intentional exceptions
remain and are **not** oversights:

- `.devcontainer/` and `configure_devcontainer.sh` keep opt-in CUDA support
  (defaults off) as reusable container infrastructure.
- `cmake/HandleWrapper.cmake` retains gtwrap-root resolution and `matlab.h`
  include discovery, because those sit on the shared code path the Python
  wrapper uses and are inert while the option is OFF.
