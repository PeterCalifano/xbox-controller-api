# cpp_cuda_template_project

A CMake template for building GPU-accelerated C++ libraries with optional CUDA,
OptiX, TensorRT, Python/MATLAB bindings, and profiling support. Shared builds
are the default, and static builds are selectable through standard CMake
`BUILD_SHARED_LIBS`. Designed to be cloned and renamed into a real project.

## Documentation Map

- [`doc/template_usage.md`](doc/template_usage.md): cloning, renaming, source layout, nested consumers, and test placement.
- [`doc/bootstrap_prompts.md`](doc/bootstrap_prompts.md): interactive agent prompt for tailoring the template into a fresh library.
- [`doc/cpp_cuda_build.md`](doc/cpp_cuda_build.md): C++ build modes, CUDA,
  OptiX, TensorRT, toolchains, CPU tuning, and profiling toggles.
- [`doc/wrappers.md`](doc/wrappers.md): gtwrap setup, Python package workflow, MATLAB wrappers, and wrapper docstrings.
- [`doc/versioning.md`](doc/versioning.md): git tags, source/build/install `VERSION` files, C++ config macros, Python metadata, and packages.
- [`doc/logging.md`](doc/logging.md): dependency-free component logging, level configuration, stream routing, and capture.
- [`doc/documentation_workflow.md`](doc/documentation_workflow.md): Doxygen, CMake docs targets, XML output, GitHub Pages, and output checks.
- [`doc/testing_and_ci.md`](doc/testing_and_ci.md): CTest gates, CI workflow expectations, issue forms, and validation reports.

Tailoring helper:

```bash
./tailor_template_cleanup.sh --list
./tailor_template_cleanup.sh --apply --yes --project-namespace my_project
```

The required namespace option replaces `template_project::logging` in the
reusable logger sources and examples. Run the cleanup before a broad
`template_project` replacement, because the script contains template-specific
cleanup paths. Root/test CMake files, starter tests, and project workflows
remain unchanged. After cleanup succeeds, delete `tailor_template_cleanup.sh`
or exclude it from the rename pass. `profiling/` is removed by default. Add
`--keep-profiling` when the new project should keep the Valgrind/perf helper
scripts. TensorRT discovery and integration remain available to tailored
projects but stay dependency-neutral while `template_project_ENABLE_TENSORRT`
is `OFF`.

<!-- ros2-overlay-begin -->
## Optional ROS 2 Overlay

See [`doc/ros2_overlay.md`](doc/ros2_overlay.md) for the optional ROS 2 overlay architecture, build flow, CI, rollout, and removal policy.

- `./build_lib.sh`: C++-first library entry point; it never needs ROS.
- `./build_ros2.sh`: optional ROS 2 overlay build and test entry point.

Use `./tailor_template_cleanup.sh --apply --yes --project-namespace my_project --remove-ros2` when a derived project should not carry the overlay.
<!-- ros2-overlay-end -->

## Requirements

| Dependency | Version | Notes |
|---|---|---|
| CMake | ≥ 3.15 | |
| C++ compiler | C++20 | GCC 11+, Clang 13+ |
| Eigen3 | ≥ 3.4 | Required |
| CUDA Toolkit | ≥ 12.0 | Optional (`-DENABLE_CUDA=ON`) |
| OptiX SDK | any | Optional (`-DENABLE_OPTIX=ON`), requires CUDA |
| TensorRT SDK | any | Optional (`-DENABLE_TENSORRT=ON`), requires CUDA |
| oneTBB | any | Optional (`-DENABLE_TBB=ON`) |
| Catch2 | 3.x | Auto-fetched from GitHub if not found |
| pytest | any | Required when `ENABLE_PYTHON_TESTS=ON` and `test*.py` files are present |
| pyparsing | latest | Required for gtwrap Python/MATLAB code generation |
| Valgrind / perf | any | Optional, for profiling scripts |
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

CTest is the single local test entrypoint. Compiled tests named `test*.cpp` or
`test*.cu` are built as Catch2 executables. Python tests named `test*.py` are
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

## Using as a Template

For a fresh library, first run the tailoring cleanup helper above, then perform the rename pass.

To start a new project from this template, rename the following (all in one pass with your editor's global find-and-replace):

| Placeholder | Replace with |
|---|---|
| `template_project` | your project name (snake_case) |
| `template_src` | your library module name |
| `template_src_kernels` | your CUDA module name (or delete if no CUDA) |

**Files/directories to rename:**

```
src/template_src/            --> src/<your_lib>/
src/template_src_kernels/    --> src/<your_lib>_kernels/    (if using CUDA)
src/cmake/template_projectConfig.cmake.in  --> src/cmake/<your_project>Config.cmake.in
```

**CMakeLists.txt** (root project definition):

```cmake
set(project_name "your_project_name")
set(project_description "Short project description")
set(project_homepage_url "https://example.com/your_project_name")
set(PROJECT_MAINTAINER_NAME "Project Maintainer" CACHE STRING "Project maintainer name")
set(PROJECT_MAINTAINER_EMAIL "maintainer@example.com" CACHE STRING "Project maintainer email")
set(PROJECT_LICENSE "Apache-2.0" CACHE STRING "Project SPDX license identifier")
```

**What to keep as-is:** the entire `cmake/` module system, `build_lib.sh`, `configure_devcontainer.sh`, and `generate_version.sh`. Keep `profiling/` only when the project needs the optional Valgrind/perf helper scripts.

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
-m, --matlab-wrap         Enable MATLAB wrappers
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
| `template_project_ENABLE_CUDA` | OFF | CUDA GPU acceleration |
| `template_project_ENABLE_OPTIX` | OFF | NVIDIA OptiX (enables CUDA automatically) |
| `template_project_ENABLE_TENSORRT` | OFF | NVIDIA TensorRT (enables CUDA automatically) |
| `template_project_METADATA_ONLY` | OFF | Configure project identity/version without compiler languages |
| `ENABLE_TBB` | OFF | Intel oneTBB support (`find_package(TBB)`) |
| `ENABLE_OPENGL` | OFF | OpenGL support |
| `ENABLE_TESTS` | ON | Register and run CTest tests |
| `CATCH2_TEST_REPORTER` | `compact` | Catch2 reporter passed through `catch_discover_tests` |
| `CATCH2_TEST_PROPERTIES` | `LABELS;catch2` | CTest property name/value pairs for discovered Catch2 tests |
| `ENABLE_PYTHON_TESTS` | ON | Register `test*.py` files as pytest-backed CTest tests |
| `PYTHON_TEST_EXECUTABLE` | auto | Python executable for pytest tests when conda is not selected |
| `PYTHON_TEST_CONDA_ENV` | `""` | Optional conda environment name for pytest tests |
| `PYTHON_TEST_CONDA_PREFIX` | `""` | Optional conda environment prefix for pytest tests |
| `ENABLE_PROFILING` | OFF | Profiling-friendly flags; enables `ENABLE_GPERFTOOLS` by default |
| `ENABLE_GPERFTOOLS` | `ENABLE_PROFILING` | Link gperftools `libprofiler` when found |
| `ENABLE_TCMALLOC` | OFF | Explicitly link gperftools `libtcmalloc`; keep OFF for normal MATLAB MEX builds |
| `BUILD_SHARED_LIBS` | ON | Build compiled libraries as shared (`OFF` builds static archives) |
| `template_project_BUILD_PROGRAMS` | ON | Build root program targets when this project is the main project |
| `template_project_BUILD_EXAMPLES` | ON | Build example targets when this project is the main project |
| `SANITIZE_BUILD` | OFF | Enable sanitizers (see `SANITIZERS` variable) |
| `SANITIZERS` | `address,undefined,leak` | Comma-separated sanitizer list |
| `CPU_ENABLE_NATIVE_TUNING` | ON for native, OFF for cross | Adds `-march=native -mtune=native` for GNU/Clang optimized native builds |
| `CPU_ENABLE_SIMD` | OFF | Adds explicit SIMD ISA flag from `CPU_SIMD_LEVEL` |
| `CPU_SIMD_LEVEL` | `native` | SIMD target: `native`, `sse4.2`, `avx`, `avx2`, `avx512f` |
| `CPU_ENABLE_FMA` | OFF | Adds `-mfma` for GNU/Clang optimized builds |
| `CPU_EXTRA_OPT_FLAGS` | `""` | Extra CPU optimization flags for optimized builds |
| `CUDA_ENABLE_FMAD` | ON | NVCC fused multiply-add control (`--fmad=true/false`) |
| `CUDA_ENABLE_EXTRA_DEVICE_VECTORIZATION` | OFF | Adds NVCC `--extra-device-vectorization` |
| `CUDA_USE_FAST_MATH` | OFF | Adds NVCC `--use_fast_math` to regular CUDA builds |
| `CUDA_PTX_USE_FAST_MATH` | ON | Adds NVCC `--use_fast_math` to PTX generation path |
| `CUDA_NVCC_EXTRA_FLAGS` | `""` | Extra NVCC flags for CUDA and PTX compilation |
| `NO_OPTIMIZATION` | OFF | Force profiler-friendly `-O0 -g3`, frame pointers, and assertions regardless of build type |
| `WARNINGS_ARE_ERRORS` | OFF | Treat all warnings as errors (`-Werror`) |

Replace the `template_project` prefix during tailoring. The historical
`ENABLE_CUDA`, `ENABLE_OPTIX`, `ENABLE_TENSORRT`, and `PROJECT_METADATA_ONLY`
remain top-level compatibility aliases; nested consumers must use the
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

### CUDA / OptiX

```bash
./build_lib.sh -D ENABLE_CUDA=ON
./build_lib.sh -D ENABLE_CUDA=ON -D ENABLE_OPTIX=ON
```

GPU architecture is auto-detected via `nvidia-smi`. CUDA kernels live in `src/template_src_kernels/`:

- `.cu` files - standard CUDA kernels
- `.ptx.cu` files - compiled to embedded `const char[]` arrays for OptiX modules

Auto-detection is intentionally strict:

- On `x86_64`/`amd64`, a working `nvidia-smi` is required unless you set `CUDA_ARCHITECTURES` or `CMAKE_CUDA_ARCHITECTURES` explicitly.
- On `aarch64`/`arm64`, the template first tries `nvidia-smi`, then falls back to native Jetson/Tegra markers for Xavier (`72`), Orin (`87`), and Thor (`101`).
- If detection is unavailable or ambiguous, configure fails with guidance to set `CUDA_ARCHITECTURES` or `CMAKE_CUDA_ARCHITECTURES` explicitly.

Example with explicit CUDA optimization toggles:

```bash
./build_lib.sh -D ENABLE_CUDA=ON \
  -D CUDA_ARCHITECTURES=87 \
  -D CUDA_ENABLE_FMAD=ON \
  -D CUDA_ENABLE_EXTRA_DEVICE_VECTORIZATION=ON \
  -D CUDA_NVCC_EXTRA_FLAGS="--maxrregcount=128"
```

When `ENABLE_OPTIX=ON`, configuration also fails fast unless the project contains:

- at least one compilable library source under `src/` (`*.cpp` or `*.cu`, excluding `*.ptx.cu`, and excluding `src/bin/`)
- at least one PTX kernel source (`*.ptx.cu`)

This template treats OptiX on a header-only library as a configuration error.

### TensorRT

TensorRT is opt-in and enables the CUDA feature automatically. Point either
`TensorRT_ROOT` or the compatibility spelling `TENSORRT_ROOT` at an SDK root
containing `include/` and `lib/`, or at an NVIDIA archive layout containing
`targets/<triplet>/include` and `targets/<triplet>/lib`:

```bash
./build_lib.sh -D ENABLE_TENSORRT=ON \
  -D TensorRT_ROOT=/opt/TensorRT \
  -D CUDA_ARCHITECTURES=87
```

The project target propagates `TensorRT::nvinfer`,
`TensorRT::nvinfer_plugin`, `CUDA::cudart`, and
`__TENSORRT_ENABLED__=1`. Installed consumers of a TensorRT-enabled build must
make a compatible SDK discoverable through the same root hints. The installed
package resolves its own finder directly and does not modify the consumer's
`CMAKE_MODULE_PATH`.

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
  -D template_project_BUILD_PROGRAMS=OFF -D template_project_BUILD_EXAMPLES=OFF

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

## Python and MATLAB Wrappers (gtwrap)

This template supports wrappers via `gtwrap` in two modes:

1. Installed package mode (`find_package(gtwrap)`).
2. Local checkout mode (`--gtwrap-root /path/to/wrap` or `-D<project>_GTWRAP_ROOT_DIR=...`).

When `-p` and/or `-m` is used, wrapper resolution now follows this order:

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
# Python wrapper only
./build_lib.sh -p

# Python + MATLAB wrappers
./build_lib.sh -p -m

# Force local wrap checkout
./build_lib.sh -p --gtwrap-root /path/to/wrap

# Rebuild an already-configured wrapper build
./build_lib.sh -r -p
```

`-p` enables namespaced CMake wrapper options and ensures the resolved Python wrapper target is built when that target exists in the configured cache.

`--rebuild-only` does not reconfigure CMake. If you use `./build_lib.sh -r -p`, the existing build directory must already have been configured with Python wrapping enabled.

### Generated sources

If your wrapper interface uses `gtsam::Vector`/`gtsam::Matrix` without a full GTSAM dependency, include `src/utils/wrap_adapters/GtsamAliases.h` in `src/wrap_interface.i` to alias them to Eigen types.

Wrapper generators produce different C++ files by design:

1. Python (pybind): `<build>/wrap_interface.cpp` (from top-level `wrap_interface.i`).
2. MATLAB: `<build>/wrap/<project>/<project>_wrapper.cpp`.

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

## Installation and Consuming as a Library

Install to the default prefix (`./install`) or a custom one:

```bash
./build_lib.sh -t release -i
# or with custom prefix:
./build_lib.sh -t release -i -D CMAKE_INSTALL_PREFIX=/opt/my_project
# or install a static library package:
./build_lib.sh -t release -i -D BUILD_SHARED_LIBS=OFF
```

In a downstream CMake project:

```cmake
# Option 1: set the path explicitly
set(my_project_DIR "/path/to/install/lib/cmake/my_project")
find_package(my_project REQUIRED)

# Option 2: via CMAKE_PREFIX_PATH
cmake -DCMAKE_PREFIX_PATH=/path/to/install ...
```

Then link:

```cmake
target_link_libraries(my_target PRIVATE my_project::my_project)
```

See [`examples/template_consumer_project/`](examples/template_consumer_project/) for a complete working example.

---

## Profiling

### Profiling-friendly build

`--profile` adds `-fno-omit-frame-pointer -fno-inline-functions` to all build types - required for `perf` and `callgrind` to produce accurate call stacks even in optimized builds. Optionally links `gperftools` if found.

```bash
./build_lib.sh --profile -t relwithdebinfo
```

### Profiling scripts

Three wrapper scripts live in `profiling/`. All share common options:
`-e <executable>`, `-o <output_dir>`, `-a "<args>"`, `-t <trials>`, `-i <start_index>`.

```bash
# Call graph analysis (valgrind callgrind)
./profiling/run_call_profiling.sh -e ./build/my_exe -o prof_results -t 3

# Heap memory profiling (valgrind massif)
./profiling/run_mem_complexity.sh -e ./build/my_exe -o prof_results

# CPU cycles / instruction count (perf)
./profiling/run_ops_profiling.sh  -e ./build/my_exe -o prof_results
```

Scripts auto-detect whether `sudo` is needed (skipped when running as root, e.g. inside a devcontainer).

Output files are written to `<output_dir>/` and are gitignored by default.

---

## DevContainer

The project ships a VS Code DevContainer configuration. To reconfigure it (base image, ROS, CUDA):

```bash
# Interactive
./configure_devcontainer.sh

# Non-interactive
./configure_devcontainer.sh --cuda --gpu-runtime podman --base ubuntu-24.04
./configure_devcontainer.sh --base ubuntu-22.04 --ros noetic --ros-profile desktop
./configure_devcontainer.sh --non-interactive --base ubuntu-24.04
```

ROS 1 requires Ubuntu 18.04 (melodic) or 20.04 (noetic).

<!-- ros2-overlay-begin -->
ROS 2 devcontainer example:

```bash
./configure_devcontainer.sh --cuda --base ubuntu-22.04 --ros2 humble
```

ROS 2 requires Ubuntu 22.04+.
<!-- ros2-overlay-end -->

The configure script only rewrites the keys it manages in `devcontainer.json` (features, GPU run args, CUDA/ROS env); project-specific entries (e.g. `customizations`, extra `remoteEnv` variables) are preserved across reconfigurations. CUDA toolkit version is selected with `--cuda-version <v>` (default 12.9). GPU passthrough args are selected with `--gpu-runtime auto|docker|podman` (default: `auto`, which prefers Docker when both engines are installed).

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

Expose a host MATLAB installation to wrapper configuration with:

```bash
./run_in_container.sh --vscode --engine podman \
    --matlab-root /usr/local/MATLAB/R2024b
```

The installation is mounted read-only at the same absolute path and exported
as `MATLAB_ROOT_DIR`. Because mounts are fixed at container creation, stop and
recreate an existing attachment container before changing the MATLAB root.

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
│   ├── template_src/            Core C++ library implementation
│   ├── template_src_kernels/    CUDA kernels (.cu) and PTX sources (.ptx.cu)
│   ├── wrapped_impl/            C wrapper layer for Python/MATLAB bindings
│   ├── config.h.in              CMake-configured header (version, feature flags)
│   └── global_includes.h        Shared utilities (ANSI colors, precision constants)
├── cmake/                       CMake module system (Handle*.cmake)
├── profiling/                   Optional Valgrind/perf wrapper scripts
├── tests/                       Inherited runtime tests and reusable fixtures
├── examples/
│   ├── template_consumer_project/   Using the library via find_package()
│   └── template_examples/           Standalone usage examples
├── doc/                         Doxygen configuration
├── build_lib.sh                 Primary build entry point
├── generate_version.sh          Write VERSION file without building
└── configure_devcontainer.sh    Reconfigure VS Code DevContainer
```
