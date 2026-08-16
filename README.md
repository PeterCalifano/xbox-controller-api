# xbox_controller_api

A C++ library for reading Xbox controller input, with optional Python bindings
and ROS 2 integration. Shared builds are the default; static builds are
selectable through the standard CMake `BUILD_SHARED_LIBS`.

> **Status:** the input API is implemented. Reading an xpad-class controller
> works from C++ and from Python, through an SDL2 backend. Version 1 is input only: rumble and LED control are not
> implemented. See [`doc/controller_api.md`](doc/controller_api.md).

## Documentation Map

- [`doc/controller_api.md`](doc/controller_api.md): controller setup on Linux, axis and button conventions, deadzones, disconnect handling, and both examples.
- [`doc/cpp_build.md`](doc/cpp_build.md): C++ build modes, toolchains, and CPU tuning.
- [`doc/wrappers.md`](doc/wrappers.md): gtwrap setup, Python package workflow, and wrapper docstrings.
- [`doc/versioning.md`](doc/versioning.md): git tags, source/build/install `VERSION` files, C++ config macros, Python metadata, and packages.
- [`doc/logging.md`](doc/logging.md): dependency-free component logging, level configuration, stream routing, and capture.
- [`doc/documentation_workflow.md`](doc/documentation_workflow.md): Doxygen, CMake docs targets, XML output, GitHub Pages, and output checks.
- [`doc/testing_and_ci.md`](doc/testing_and_ci.md): CTest gates, CI workflow expectations, issue forms, and validation reports.
- [`doc/ros2_overlay.md`](doc/ros2_overlay.md): optional ROS 2 overlay architecture, build flow, and CI.

## Use the Library

### Choose an entry point

| Need | Entry point | Hardware or special dependency |
|---|---|---|
| Read an attached controller in a C++ application | `CSdlGamepadSource` | SDL2 and an xpad-class controller |
| Test or replay controller logic without hardware | `CScriptedGamepadSource`, `GamepadFilters`, and `GamepadControls` | None |
| Inspect a connected controller from the terminal | `xbox_controller_monitor` | SDL2 and a controller |
| Run the supplied C++ examples | `example_read_controller`, `example_scripted_replay` | First needs SDL2 and a controller; second needs neither |
| Link the core into another CMake project | `xbox_controller_api::xbox_controller_api` | Installed package or source checkout |
| Read a controller from Python | `xbox_controller_api.CGamepadWrapper` | gtwrap, Python 3.12+, SDL2, and a controller |
| Publish `sensor_msgs/msg/Joy` | ROS 2 lifecycle node and launch files | ROS 2, SDL2, and a controller |

`./build_lib.sh` is the native C++ entry point and never requires ROS.
`./build_ros2.sh` builds the optional ROS 2 overlay separately.

### Build and run the included native tools

Build the library, examples, programs, and tests:

```bash
./build_lib.sh
```

The build-tree executables are:

```bash
# Live diagnostic: raw normalized values, button edges, and attach status.
./build/src/bin/xbox_controller_monitor

# Live example: polls at 50 Hz for 20 seconds by default.
./build/examples/xbox_controller_api_examples/example_read_controller 20

# Hardware-free example: replay a scripted sample sequence.
./build/examples/xbox_controller_api_examples/example_scripted_replay
```

`xbox_controller_monitor` is installed to `bin/` when the library is installed.
The monitor and `example_read_controller` report an unavailable backend or a
missing controller and exit normally. The replay example uses no SDL2 or
controller hardware, so it also works after:

```bash
./build_lib.sh -D ENABLE_SDL2=OFF
./build/examples/xbox_controller_api_examples/example_scripted_replay
```

### Read a controller from C++

Use `CSdlGamepadSource` for hardware access. Call `update()` at the cadence
your application needs; it never sleeps or pumps SDL events.

```cpp
#include <xbox_controller_api/CSdlGamepadSource.h>
#include <xbox_controller_api/GamepadControls.h>
#include <xbox_controller_api/GamepadFilters.h>

#include <iostream>

int main()
{
    using namespace xbox_controller_api;

    CSdlGamepadSource objSource;
    if (!objSource.open())
    {
        std::cerr << objSource.lastError() << "\n";
        return 1;
    }

    SGamepadState strPreviousState;
    while (objSource.update())
    {
        const SGamepadState strState = ApplyStickDeadzone(objSource.state(), 0.15);
        if (ClassifyButtonEdge(strPreviousState, strState, EGamepadButton::A) ==
            EButtonEdge::Pressed)
        {
            std::cout << "A pressed\n";
        }
        strPreviousState = strState;
    }

    objSource.close();
}
```

Use the other public headers for the surrounding tasks:

| Header | Use |
|---|---|
| `SGamepadState.h` | One normalized controller sample |
| `GamepadFilters.h` | Axis normalization, deadzones, and button-edge classification |
| `GamepadControls.h` | Iterate buttons, read a generic button, and obtain stable button names |
| `CScriptedGamepadSource.h` | Queue deterministic frames and disconnects for tests or replay |

The value ranges, Y-axis convention, disconnect behavior, and multi-controller
selection are specified in [`doc/controller_api.md`](doc/controller_api.md).

### Use the Python wrapper

Build the wrapper, install its generated package into the Python environment
used for the build, then run the supplied example:

```bash
python3 -m pip install pyparsing
./build_lib.sh -p
cmake --build build --target python-install
python3 examples/python/example_read_controller.py 20
```

If gtwrap is not installed as a CMake package, pass a local checkout explicitly:

```bash
./build_lib.sh -p --gtwrap-root /path/to/wrap
```

The wrapper exposes `CGamepadWrapper`, which provides `open()`, `update()`,
axis accessors, named button accessors, and generic button iteration. Check
`HAS_WRAPPER` before using it when an application must also run without the
compiled extension. See [`doc/wrappers.md`](doc/wrappers.md) for package and
runtime-library details.

### Use the ROS 2 overlay

The overlay publishes `sensor_msgs/msg/Joy` on `/xbox_controller/joy` by
default. Build it in a ROS 2 environment, source the overlay, then choose one
autostart launch file:

```bash
./build_ros2.sh --clean
source ros2/install/setup.bash

# Option 1: standalone lifecycle-node process.
ros2 launch xbox_controller_api_spinup xbox_controller_api.launch.py
```

Or run the lifecycle component in a container:

```bash
ros2 launch xbox_controller_api_spinup xbox_controller_api_composition.launch.py
```

In a separate terminal, inspect the published messages with:

```bash
ros2 topic echo /xbox_controller/joy
```

For a lifecycle manager you control, run the executable directly and request
the transitions yourself. Parameters must be supplied before configuration:

```bash
ros2 run xbox_controller_api_ros xbox_controller_api_node --ros-args \
  -p joystick_index:=0 -p publish_rate_hz:=100.0 -p stick_deadzone:=0.15
ros2 lifecycle set /xbox_controller configure
ros2 lifecycle set /xbox_controller activate
```

After a controller disconnect, deactivate and activate the node to reconnect.
The full parameter list, `Joy` axis/button order, and composition details are
in [`doc/controller_api.md`](doc/controller_api.md#ros-2) and
[`doc/ros2_overlay.md`](doc/ros2_overlay.md).


## Requirements

| Dependency | Version | Notes |
|---|---|---|
| CMake | ≥ 3.15 | |
| C++ compiler | C++20 | GCC 11+, Clang 13+ |
| Eigen3 | ≥ 3.4 | Required |
| SDL2 | ≥ 2.0.9 | Optional but ON by default; provides the controller backend (`libsdl2-dev`) |
| oneTBB | any | Optional (`-DENABLE_TBB=ON`) |
| Catch2 | 3.x | Auto-fetched from GitHub if not found |
| pytest | any | Required when `ENABLE_PYTHON_TESTS=ON` and `test*.py` files are present |
| pyparsing | latest | Required for gtwrap Python code generation |
| libgoogle-perftools-dev | any | Optional (`-DENABLE_PROFILING=ON` / `-DENABLE_TCMALLOC=ON`) |

---

## Quick Start

```bash
git clone <repo-url> my_project && cd my_project

# Default shared build (RelWithDebInfo) + run tests
./build_lib.sh

# Static library build
./build_lib.sh -D BUILD_SHARED_LIBS=OFF

# Debug build, Ninja generator, 8 jobs
./build_lib.sh -t debug -N -j 8

# Build + install to ./install
./build_lib.sh -t release -i
```

Optimized native builds (`Release`, `RelWithDebInfo`) enable `-march=native -mtune=native` by default.
Cross builds disable native tuning automatically; use `CPU_EXTRA_OPT_FLAGS` for target-specific CPU flags.

Run tests manually from the repository root after a build:

```bash
ctest --test-dir build --output-on-failure
ctest --test-dir build --output-on-failure -R <test_name>
```

CTest is the single local test entrypoint. Compiled tests named `test*.cpp`
are built as Catch2 executables. Python tests named `test*.py` are
registered as CTest tests and run through `python -m pytest -q`.

Useful local filters:

```bash
ctest --test-dir build --output-on-failure -L python
ctest --test-dir build --output-on-failure -L catch2
ctest --test-dir build --output-on-failure -R testPythonSmoke
```

Select a conda environment for Python tests without affecting C++ tests. Use a
named environment when it is stable on the machine, or a prefix for temporary
validation environments:

```bash
./build_lib.sh --python-test-conda-env my_env
./build_lib.sh --python-test-conda-prefix /path/to/conda/env
```

Pass local CTest filters during development through the build helper:

```bash
./build_lib.sh --ctest-extra-args "-L python"
```

`--ctest-extra-args` is intentionally a local development hook. CI workflows
should keep their test selection explicit in the workflow YAML instead of
depending on this helper flag. The value is split on whitespace; run `ctest`
directly for filters or arguments that need shell quoting.

---

## Build Options

All options are passed via `build_lib.sh` flags or directly as `-D<VAR>=<VAL>` to CMake.

### `build_lib.sh` reference

```
-B, --buildpath <dir>     Build directory (default: <checkout>/build)
-t, --type <type>         debug | release | relwithdebinfo | minsizerel
-j, --jobs <N>            Parallel jobs (default: nproc or 4)
-r, --rebuild-only        Skip CMake configure; rebuild sources only
-N, --ninja-build         Use Ninja generator
-f, --flagsCXX "<flags>"  Extra compiler flags (e.g. "-march=native")
-D, --define <VAR=VAL>    Extra CMake cache definitions (repeatable)
    --clean               Safely delete an owned in-repository build before configure
    --profile             Enable profiling build (see Profiling section)
    --skip-tests          Do not run tests after build
-i, --install             Run install target after tests
-p, --python-wrap         Enable Python wrappers
    --python-test-conda-env <name>
                          Run test*.py CTest entries with conda run -n <name>
    --python-test-conda-prefix <dir>
                          Run test*.py CTest entries with conda run -p <dir>
    --python-test-executable <path>
                          Python executable for test*.py CTest entries without conda
    --ctest-extra-args <args>
                          Simple whitespace-split arguments appended to CTest
    --gtwrap-root <dir>   Path to local wrap checkout root
    --wrap-update         Explicitly update a local wrap checkout to latest master
    --no-wrap-update      Keep the local wrap checkout unchanged (default)
    --wrap-submodule-init Explicitly initialize a declared wrap submodule fallback
    --no-wrap-submodule-init
                          Do not initialize a wrap submodule (default)
    --toolchain <file>    CMake toolchain file
-h, --help                Show full help
```

See [`doc/build_script_doc.md`](doc/build_script_doc.md) for a detailed option reference.

`--clean` accepts only conventional in-repository `build`, `build*`, or
`out/*` paths. An existing directory must contain a CMake cache owned by this
checkout. Relative paths remain anchored to the checkout containing the script,
including when it is invoked from another working directory. The option is
ignored with `--rebuild-only`.

### CMake feature flags

| Option | Default | Description |
|---|---|---|
| `xbox_controller_api_METADATA_ONLY` | OFF | Configure project identity/version without compiler languages |
| `ENABLE_TBB` | OFF | Intel oneTBB support (`find_package(TBB)`) |
| `ENABLE_OPENGL` | OFF | OpenGL support |
| `ENABLE_SDL2` | ON | SDL2-backed controller input. When SDL2 is missing the build warns and the backend is stubbed out, keeping the full API surface |
| `ENABLE_SDL2_STRICT` | OFF | Turn a missing SDL2 into a configure error instead of a warning; used by CI |
| `ENABLE_TESTS` | ON | Register and run CTest tests |
| `CATCH2_TEST_REPORTER` | `compact` | Catch2 reporter passed through `catch_discover_tests` |
| `CATCH2_TEST_PROPERTIES` | `LABELS;catch2` | CTest property name/value pairs for discovered Catch2 tests |
| `ENABLE_PYTHON_TESTS` | ON | Register `test*.py` files as pytest-backed CTest tests |
| `PYTHON_TEST_EXECUTABLE` | auto | Python executable for pytest tests when conda is not selected |
| `PYTHON_TEST_CONDA_ENV` | `""` | Optional conda environment name for pytest tests |
| `PYTHON_TEST_CONDA_PREFIX` | `""` | Optional conda environment prefix for pytest tests |
| `ENABLE_PROFILING` | OFF | Profiling-friendly flags; enables `ENABLE_GPERFTOOLS` by default |
| `ENABLE_GPERFTOOLS` | `ENABLE_PROFILING` | Link gperftools `libprofiler` when found |
| `ENABLE_TCMALLOC` | OFF | Explicitly link gperftools `libtcmalloc`; keep OFF for normal builds |
| `BUILD_SHARED_LIBS` | ON | Build compiled libraries as shared (`OFF` builds static archives) |
| `xbox_controller_api_BUILD_PROGRAMS` | ON | Build root program targets when this project is the main project |
| `xbox_controller_api_BUILD_EXAMPLES` | ON | Build example targets when this project is the main project |
| `SANITIZE_BUILD` | OFF | Enable sanitizers (see `SANITIZERS` variable) |
| `SANITIZERS` | `address,undefined,leak` | Comma-separated sanitizer list |
| `CPU_ENABLE_NATIVE_TUNING` | ON for native, OFF for cross | Adds `-march=native -mtune=native` for GNU/Clang optimized native builds |
| `CPU_ENABLE_SIMD` | OFF | Adds explicit SIMD ISA flag from `CPU_SIMD_LEVEL` |
| `CPU_SIMD_LEVEL` | `native` | SIMD target: `native`, `sse4.2`, `avx`, `avx2`, `avx512f` |
| `CPU_ENABLE_FMA` | OFF | Adds `-mfma` for GNU/Clang optimized builds |
| `CPU_EXTRA_OPT_FLAGS` | `""` | Extra CPU optimization flags for optimized builds |
| `NO_OPTIMIZATION` | OFF | Force profiler-friendly `-O0 -g3`, frame pointers, and assertions regardless of build type |
| `WARNINGS_ARE_ERRORS` | OFF | Treat all warnings as errors (`-Werror`) |

The historical `PROJECT_METADATA_ONLY` spelling
remains a top-level compatibility alias; nested consumers must use the
project-qualified forms so parent cache options cannot change the library
configuration. A legacy alias supplied to a top-level configure wins for that
invocation, is copied to the canonical option, and is then removed from the
cache so later reconfigures cannot retain two conflicting sources of truth.

### Build type compiler flags

| Build type | Flags | Notes |
|---|---|---|
| `Debug` | `-Og -g` + sanitizers | Max debug info |
| `RelWithDebInfo` | `-O2 -g -DNDEBUG` + stricter warnings | **Default** |
| `Release` | `-O3 -DNDEBUG` | Tests forced on |
| `MinSizeRel` | `-Os` | |
| `NOPTIM` | `-O0 -g3` | Stricter warnings, frame pointers, no inlining/sibling-call optimization |

---

## Optional Features

### TBB

```bash
./build_lib.sh -D ENABLE_TBB=ON
```

### CPU vectorization tuning

`CPU_ENABLE_NATIVE_TUNING` is ON by default for optimized native builds and disabled automatically while cross-compiling.

```bash
# Disable native tuning for portable binaries
./build_lib.sh -D CPU_ENABLE_NATIVE_TUNING=OFF

# AArch64 cross build using bundled toolchain defaults
./build_lib.sh --toolchain cmake/toolchains/defaults/aarch64-linux-gnu.cmake --clean \
  -D xbox_controller_api_BUILD_PROGRAMS=OFF -D xbox_controller_api_BUILD_EXAMPLES=OFF

# Enable explicit AVX2 + FMA flags
./build_lib.sh -D CPU_ENABLE_SIMD=ON -D CPU_SIMD_LEVEL=avx2 -D CPU_ENABLE_FMA=ON
```

### Sanitizers

```bash
./build_lib.sh -t debug -D SANITIZE_BUILD=ON
# Custom sanitizer set:
./build_lib.sh -t debug -D SANITIZE_BUILD=ON -D SANITIZERS="address,undefined"
```

---

## Python Wrappers (gtwrap)

The project supports Python bindings via `gtwrap` in two modes:

1. Installed package mode (`find_package(gtwrap)`).
2. Local checkout mode (`--gtwrap-root /path/to/wrap` or `-D<project>_GTWRAP_ROOT_DIR=...`).

When `-p` is used, wrapper resolution follows this order:

1. Use an explicit `--gtwrap-root` or an existing local checkout at `./wrap`,
   `./lib/wrap`, or `../wrap`.
2. Fall back to an installed `gtwrap` package discoverable via `find_package(gtwrap)`.
3. If still unresolved and `GTWRAP_INIT_SUBMODULE_IF_MISSING=ON`, initialize a
   declared `wrap` or `lib/wrap` git submodule and use that checkout.

Wrapper checkout maintenance is disabled by default. Pass `--wrap-update` to
explicitly advance a resolved local checkout to `origin/master`, or
`--wrap-submodule-init` to initialize a declared submodule after local and
installed discovery fail. Direct CMake callers must grant checkout maintenance
with `GTWRAP_MAINTENANCE_UPDATE=ON` as well as requesting
`GTWRAP_SYNC_TO_MASTER=ON`. Submodule initialization applies only to a `wrap`
or `lib/wrap` entry already declared in `.gitmodules`; adding a new submodule is
a separate Git maintenance operation.

### Prerequisites

Install `pyparsing` in the same Python environment used for wrapping:

```bash
python3 -m pip install pyparsing
```

`pybind11` is provided by `gtwrap` (installed package or local checkout).

The default wrapper entrypoint is `src/wrap_interface.i`. If it is missing or the configured interface list is invalid, wrapper generation is auto-disabled during configure.

### Build examples

```bash
# Python wrapper
./build_lib.sh -p

# Force local wrap checkout
./build_lib.sh -p --gtwrap-root /path/to/wrap

# Rebuild an already-configured wrapper build
./build_lib.sh -r -p
```

`-p` enables namespaced CMake wrapper options and ensures the resolved Python wrapper target is built when that target exists in the configured cache.

`--rebuild-only` does not reconfigure CMake. If you use `./build_lib.sh -r -p`, the existing build directory must already have been configured with Python wrapping enabled.

### Generated sources

If your wrapper interface uses `gtsam::Vector`/`gtsam::Matrix` without a full GTSAM dependency, include `src/utils/wrap_adapters/GtsamAliases.h` in `src/wrap_interface.i` to alias them to Eigen types.

Wrapper generation output:

Python (pybind) generates `<build>/wrap_interface.cpp` from the top-level `wrap_interface.i`.

### Python package install workflow

Python package metadata is owned by `python/pyproject.toml.in` and configured
into `<build>/python/pyproject.toml` when Python wrapping is requested.
The optional `setup.py.in` augments installation behavior without duplicating
package name/version metadata.

The wrapper wheel automatically co-locates the main project shared library.
Additional direct project-owned shared runtime build targets can be declared
through
`<namespace>_GTWRAP_RUNTIME_DEPENDENCY_TARGETS`; the separate
`<namespace>_GTWRAP_DEPENDENCY_TARGETS` option remains build-order-only.

The checked-in `python/<project>/__init__.py` is the public package entrypoint:

- `import <project>` is the supported import path.
- `HAS_WRAPPER` is `True` when the compiled wrapper imports successfully.
- `HAS_WRAPPER` is `False` when the pure-Python package imports without the wrapper.
- `WRAPPER_IMPORT_ERROR` stores the wrapper import exception when fallback is active.

When Python wrapping is requested, CMake assembles a disposable package root
without updating the source checkout:

- generated `<build>/python/pyproject.toml`
- generated `<build>/python/setup.py`
- build-time `<build>/python/<project>/_wrapper_build.py` linking the latest
  successfully staged wrapper configuration

Install from the configured build package directory:

```bash
cd build/python
python -m pip install .
```

For convenience, the main project also provides:

```bash
cmake --build build --target python-install
```

When using Conda, activate the target environment first, then run the same command.

---

## Versioning

Version is resolved in order:

1. **Git tags** - tag format `vMAJOR.MINOR.PATCH` (e.g. `v1.2.0`)
2. **`VERSION` file** - parsed from `Project version: X.Y.Z` if git is unavailable
3. **CMake defaults** - `0.0.0` if neither source is available

The `VERSION` file is always written to the build directory during CMake configure and installed with the package. Source-tree writes are opt-in so CI and test harness configures do not dirty the checkout:

```bash
cmake -S . -B build -D WRITE_SOURCE_VERSION_FILE=ON
```

To write the ignored source `VERSION` file without building:

```bash
./generate_version.sh
```

Version is available in C++ via the generated `config.h`:

```cpp
#include "config.h"
PrintVersion();          // prints to stdout
GetVersionString();      // returns std::string
PROJECT_VERSION_MAJOR    // integer macros
```

---

## Install or Consume the C++ Library

Install the shared library to the default `./install` prefix, a custom prefix,
or as a static library:

```bash
./build_lib.sh -t release -i
./build_lib.sh -t release -i -D CMAKE_INSTALL_PREFIX=/opt/xbox-controller-api
./build_lib.sh -t release -i -D BUILD_SHARED_LIBS=OFF
```

An installed consumer finds and links the exported target as follows:

```cmake
find_package(xbox_controller_api CONFIG REQUIRED)
target_link_libraries(controller_app PRIVATE xbox_controller_api::xbox_controller_api)
```

Point CMake at the install prefix when it is not already discoverable:

```bash
cmake -S . -B build_consumer \
  -DCMAKE_PREFIX_PATH=/opt/xbox-controller-api
```

For the default prefix in this checkout, use `-DCMAKE_PREFIX_PATH="$PWD/install"`.
The installed target provides the include directories and link dependencies;
do not add the library's `src/` directory manually.

A parent project can also build the core directly from a source checkout:

```cmake
add_subdirectory(external/xbox-controller-api)
target_link_libraries(controller_app PRIVATE xbox_controller_api::xbox_controller_api)
```

The source-tree form builds only the core library as part of the parent. Use a
standalone build for the supplied programs, examples, tests, Python wrapper,
and ROS 2 overlay.

[`examples/consumer_project/`](examples/consumer_project/) contains a complete
installed-package example that exercises the hardware-free scripted source.

---

## DevContainer

The project ships a VS Code DevContainer configuration. To reconfigure it (base image, ROS):

```bash
# Interactive
./configure_devcontainer.sh

# Non-interactive
./configure_devcontainer.sh --base ubuntu-22.04 --ros noetic --ros-profile desktop
./configure_devcontainer.sh --non-interactive --base ubuntu-24.04
```

ROS 1 requires Ubuntu 18.04 (melodic) or 20.04 (noetic).

ROS 2 devcontainer example:

```bash
```

ROS 2 requires Ubuntu 22.04+.

The configure script only rewrites the keys it manages in `devcontainer.json` (features, GPU run args, ROS env); project-specific entries (e.g. `customizations`, extra `remoteEnv` variables) are preserved across reconfigurations. GPU passthrough args are selected with `--gpu-runtime auto|docker|podman` (default: `auto`, which prefers Docker when both engines are installed).

### GPU host requirements

When CUDA is enabled, generated `runArgs` match the selected container engine:

- **Docker**: generated args are `["--gpus", "all"]`; install the [NVIDIA Container Toolkit](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html).
- **Podman**: generated args are `["--device", "nvidia.com/gpu=all", "--security-opt=label=disable"]`; generate a CDI spec once, e.g. `sudo nvidia-ctk cdi generate --output=/etc/cdi/nvidia.yaml`. Rootless Podman also requires subordinate UID/GID ranges for your user in `/etc/subuid` and `/etc/subgid` (then run `podman system migrate`).

### Standalone container (without VS Code)

The image in `.devcontainer/Dockerfile` can be built and used outside the DevContainer flow. CUDA is installed by the `INSTALL_CUDA=on` build arg in that case (the DevContainer installs it via the `nvidia-cuda` feature instead):

```bash
# Build the image and run a command/binary inside it (repo mounted at /workspace)
./run_in_container.sh ./build/my_app --my-flag

# Interactive shell, force image rebuild, disable GPU
./run_in_container.sh --build --no-gpu

# Manual build
docker build --build-arg INSTALL_CUDA=on --build-arg CUDA_VERSION=12.9 -t my-dev .devcontainer
```

Command mode runs with the host numeric UID and GID and uses `/tmp` as its
writable home. Files created through the `/workspace` bind mount therefore
remain owned by the host user instead of root. Rootless Podman additionally
uses its `keep-id` user namespace.

### Attach VS Code to a launcher-managed container

Use `--vscode` to start a stable container before selecting
`Dev Containers: Attach to Running Container...`:

```bash
./run_in_container.sh --vscode --engine podman
```

Attachment mode mounts the repository under `/workspaces/<repository>`,
preserves bind-mount ownership, and forwards a live SSH-agent socket when one
is available. The launcher prints the `workspaceFolder` and `remoteUser`
values for the first attachment. This mode builds the Dockerfile directly, so
features declared only in `devcontainer.json` are not applied; use the normal
Dev Containers create/reopen workflow when those features are required.
Docker attachment mode also requires the image's `vscode` UID and GID to match
the host user; the launcher rejects a mismatch rather than creating files with
ambiguous ownership.

---

## Documentation

Doxygen documentation is auto-built when CMake finds `doxygen`:

```bash
cmake -S . -B build_docs -D BUILD_DOC_HTML=ON -D BUILD_DOC_XML=ON
cmake --build build_docs --target doc
```

Output goes to `build_docs/doc/html/index.html`; XML output for wrapper docstrings goes to `build_docs/doc/xml/`.

If your CMake version supports presets:

```bash
cmake --preset docs
cmake --build --preset docs
```

The docs target is created only for the top-level project. Nested template-derived libraries do not create generic `doc` targets and are excluded from the generated output.

---

## Project Structure

```
├── src/
│   ├── xbox_controller_api/     Core C++ library implementation
│   ├── wrapped_impl/            C wrapper layer for the Python bindings
│   ├── utils/logging/           Dependency-free component logger
│   ├── config.h.in              CMake-configured header (version, feature flags)
│   └── global_includes.h        Shared utilities (ANSI colors, precision constants)
├── cmake/                       CMake module system (Handle*.cmake)
├── python/                      Python package sources and packaging templates
├── ros2/                        Optional ROS 2 overlay (colcon workspace)
├── tests/                       Runtime tests and reusable fixtures
├── examples/
│   ├── consumer_project/               Using the library via find_package()
│   └── xbox_controller_api_examples/   Standalone usage examples
├── doc/                         Doxygen configuration and guides
├── build_lib.sh                 Primary build entry point
├── build_ros2.sh                Optional ROS 2 overlay build entry point
├── generate_version.sh          Write VERSION file without building
└── configure_devcontainer.sh    Reconfigure VS Code DevContainer
```
