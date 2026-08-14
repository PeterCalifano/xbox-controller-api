# build_lib.sh Reference

`build_lib.sh` is a convenience wrapper around CMake configure, build, test, install, and optional wrapper generation. It keeps common project builds concise while still forwarding arbitrary CMake cache definitions through `-D`.

The script uses out-of-source builds, strict shell error handling, generator-independent `cmake --build`, and CTest for validation.

## Common Commands

```bash
# Default RelWithDebInfo shared-library build with tests
./build_lib.sh

# Clean debug build with Ninja
./build_lib.sh --clean -N -t debug

# Static release build and install
./build_lib.sh -t release -D BUILD_SHARED_LIBS=OFF -i

# Rebuild an already configured tree
./build_lib.sh -r -j 8
```

## Build Layout

| Option | Purpose |
|---|---|
| `-B, --buildpath <dir>` | Build directory relative to the script's checkout. Default: `<checkout>/build`. |
| `--clean` | Remove an owned conventional in-repository build directory before configure. Ignored with `--rebuild-only`. |
| `-N, --ninja-build` | Configure with the Ninja generator. |
| `-j, --jobs <N>` | Build/test parallelism. Default: `$JOBS`, then `nproc`, then `4`. |
| `-r, --rebuild-only` | Skip configure and build the existing cache. |

The generated CMake build exports `compile_commands.json` by default for tools such as clangd and static analyzers.

Clean removal is intentionally narrower than ordinary configuration. The
target must resolve below the checkout containing `build_lib.sh` as `build`,
`build*`, or `out/*`. If it already exists, its `CMakeCache.txt` must identify
this exact checkout through `CMAKE_HOME_DIRECTORY`. Relative build paths and
the CMake source path remain anchored to that checkout even when the helper is
invoked by absolute path from another working directory. Configure unusual or
external build layouts without `--clean` and remove them explicitly only after
independent review.

## Configure Options

| Option | Purpose |
|---|---|
| `-t, --type <name>` | CMake build type: `debug`, `release`, `relwithdebinfo`, or `minsizerel`. |
| `-f, --flagsCXX "<flags>"` | Extra C/C++ compiler flags. |
| `-D, --define <VAR=VAL>` | Forward a CMake cache definition. Repeatable. |
| `-n, --no-optim` | Set `NO_OPTIMIZATION=ON` for profiler-friendly `-O0 -g3` builds. |
| `--toolchain <file>` | Set `CMAKE_TOOLCHAIN_FILE`. |

Examples:

```bash
./build_lib.sh -D ENABLE_TBB=ON
./build_lib.sh -D CPU_ENABLE_NATIVE_TUNING=OFF
./build_lib.sh --toolchain cmake/toolchains/defaults/aarch64-linux-gnu.cmake --clean
```

## Tests

Tests are enabled by default and run through CTest after a successful build.

| Option | Purpose |
|---|---|
| `-c, --checks` | Enable tests. Kept for compatibility because tests are already enabled by default. |
| `--skip-tests`, `--no-checks` | Disable tests for non-Release builds. |
| `--ctest-extra-args "<args>"` | Append simple whitespace-split arguments to the CTest invocation. |
| `--python-test-executable <path>` | Python executable for pytest-backed CTest tests. |
| `--python-test-conda-env <name>` | Run Python tests through `conda run -n <name>`. |
| `--python-test-conda-prefix <dir>` | Run Python tests through `conda run -p <dir>`. |

Release builds force tests on before install so release artifacts are validated by default.
The script always invokes CTest with the build directory explicitly:

```bash
ctest --test-dir <buildpath> --output-on-failure -j <jobs>
```

Pass focused CTest filters during local development with:

```bash
./build_lib.sh --ctest-extra-args "-L python"
```

`--ctest-extra-args` is not part of the CI contract. Workflow files should keep
their CTest filters directly in the workflow step so the CI test selection is
visible without inspecting local helper invocations. The value is split on
whitespace; run `ctest` directly for filters or arguments that need shell
quoting.

For Python tests, prefer the dedicated environment flags instead of activating a
conda environment around the whole script:

```bash
./build_lib.sh --python-test-conda-env my_env --ctest-extra-args "-L python"
./build_lib.sh --python-test-conda-prefix /tmp/template_py312 --ctest-extra-args "-L python"
```

The conda flags only affect registered `test*.py` CTest entries. Compiled C++
and CUDA tests still run as native executables from the build tree.

## Install

Use `-i, --install` to run the install target after build and tests:

```bash
./build_lib.sh -t release -i
```

Install rules are owned by CMake. The script calls:

```bash
cmake --build <build_dir> --target install --parallel <jobs>
```

## CPU Optimization

CPU optimization is controlled through CMake definitions:

| CMake option | Purpose |
|---|---|
| `CPU_ENABLE_NATIVE_TUNING` | Adds `-march=native -mtune=native` for optimized native GNU/Clang builds. Disabled automatically while cross-compiling. |
| `CPU_ENABLE_SIMD` | Enables explicit SIMD flags from `CPU_SIMD_LEVEL`. |
| `CPU_SIMD_LEVEL` | `native`, `sse4.2`, `avx`, `avx2`, or `avx512f`. |
| `CPU_ENABLE_FMA` | Adds `-mfma` where supported. |
| `CPU_EXTRA_OPT_FLAGS` | Appends target-specific CPU flags. |
| `NO_OPTIMIZATION` | Forces profiler-friendly no-optimization flags regardless of build type. |

```bash
./build_lib.sh -D CPU_ENABLE_SIMD=ON -D CPU_SIMD_LEVEL=avx2 -D CPU_ENABLE_FMA=ON
```

## CUDA, OptiX, And TensorRT

CUDA, OptiX, and TensorRT are opt-in:

```bash
./build_lib.sh -D ENABLE_CUDA=ON
./build_lib.sh -D ENABLE_CUDA=ON -D ENABLE_OPTIX=ON
./build_lib.sh -D ENABLE_TENSORRT=ON -D TensorRT_ROOT=/opt/TensorRT
```

CUDA architecture selection order:

1. `CUDA_ARCHITECTURES`
2. `CMAKE_CUDA_ARCHITECTURES`
3. `nvidia-smi` on x86_64/amd64
4. Jetson/Tegra markers on aarch64/arm64

If detection is unavailable or ambiguous, set `CUDA_ARCHITECTURES` explicitly.

CUDA optimization options:

| CMake option | Purpose |
|---|---|
| `CUDA_ENABLE_FMAD` | Control NVCC fused multiply-add contraction. |
| `CUDA_ENABLE_EXTRA_DEVICE_VECTORIZATION` | Add `--extra-device-vectorization`. |
| `CUDA_USE_FAST_MATH` | Add `--use_fast_math` to regular CUDA compilation. |
| `CUDA_PTX_USE_FAST_MATH` | Add `--use_fast_math` to PTX generation. |
| `CUDA_NVCC_EXTRA_FLAGS` | Extra NVCC flags for CUDA and PTX compilation. |

OptiX builds require at least one compiled library source and at least one `*.ptx.cu` source under `src/`. Header-only OptiX configurations fail during configure because there is no compiled library artifact to own the generated PTX integration.

TensorRT enables CUDA automatically. `TensorRT_ROOT` and `TENSORRT_ROOT` accept
conventional SDK roots and NVIDIA `targets/<triplet>` archive layouts. The
installed package retains this external SDK dependency without changing a
consumer's `CMAKE_MODULE_PATH`.

## Python And MATLAB Wrappers

| Option | Purpose |
|---|---|
| `-p, --python-wrap` | Enable Python wrapper generation. |
| `-m, --matlab-wrap` | Enable MATLAB wrapper generation. |
| `--gtwrap-root <dir>` | Use a specific local gtwrap checkout. |
| `--wrap-update` | Explicitly update a resolved local gtwrap checkout. |
| `--no-wrap-update` | Keep the resolved local checkout unchanged (default). |
| `--wrap-submodule-init` | Explicitly initialize a declared `wrap` submodule fallback. |
| `--no-wrap-submodule-init` | Do not initialize a submodule fallback (default). |

Wrapper resolution order:

1. Explicit `--gtwrap-root`
2. Local `./wrap`, `./lib/wrap`, or adjacent `../wrap`
3. Installed `gtwrap` CMake package
4. Declared `wrap` or `lib/wrap` submodule if submodule initialization is enabled

Examples:

```bash
./build_lib.sh -p
./build_lib.sh -p -m --gtwrap-root /path/to/wrap
./build_lib.sh -p --wrap-update
./build_lib.sh -r -p
```

`--rebuild-only` with wrapper flags only works when the existing CMake cache was already configured with those wrappers enabled.
Ordinary wrapper configuration does not update, initialize, or add a wrapper
checkout. `--wrap-submodule-init` applies only to an existing declaration in
`.gitmodules`; add a new submodule with an explicit Git maintenance command.

## Project Binaries And Examples

The root project controls program and example targets through namespace-derived options:

```bash
./build_lib.sh \
  -D template_project_BUILD_PROGRAMS=OFF \
  -D template_project_BUILD_EXAMPLES=OFF
```

After tailoring, replace `template_project` with the project namespace used by the root `CMakeLists.txt`.

## Troubleshooting

- Use `--clean` after changing CMake options or wrapper settings.
- Use `--wrap-update` only when intentionally advancing a local gtwrap checkout.
- Set `CPU_ENABLE_NATIVE_TUNING=OFF` for portable binaries.
- Set `CUDA_ARCHITECTURES` explicitly on CI runners without reliable GPU discovery.
- For Python tests in conda, prefer `--python-test-conda-env` or `--python-test-conda-prefix` instead of activating conda around the whole CTest run.
