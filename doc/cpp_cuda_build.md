# C++ and CUDA Build Guide

## C++ Library Build

The default build is a shared C++20 library in `RelWithDebInfo`.

```bash
./build_lib.sh
./build_lib.sh -t release -i
./build_lib.sh -D BUILD_SHARED_LIBS=OFF
```

Use `NO_OPTIMIZATION=ON` for profiler-friendly debug behavior across build types. It forces `-O0`, keeps assertions, preserves frame pointers, and disables inlining/sibling-call optimization where the compiler supports it.

```bash
./build_lib.sh -D NO_OPTIMIZATION=ON
```

## Toolchains and Cross Builds

Cross builds use CMake toolchain files under `cmake/toolchains/defaults/`. Native CPU tuning is disabled automatically while cross-compiling so build artifacts stay portable across runner CPUs.

```bash
./build_lib.sh --toolchain cmake/toolchains/defaults/aarch64-linux-gnu.cmake --clean \
  -D template_project_BUILD_PROGRAMS=OFF \
  -D template_project_BUILD_EXAMPLES=OFF
```

Use `CPU_EXTRA_OPT_FLAGS` for target-specific flags that are safe for the destination CPU.

## CUDA

CUDA is optional and enabled explicitly:

```bash
./build_lib.sh -D ENABLE_CUDA=ON
```

CUDA source files live under `src/template_src_kernels/`.

- `*.cu`: regular CUDA translation units
- `*.ptx.cu`: PTX inputs compiled into embedded C arrays for OptiX modules

GPU architecture is detected from `nvidia-smi` on x86_64. On Jetson/Tegra aarch64 systems, the template falls back to device markers for Xavier, Orin, and Thor. If detection is unavailable or ambiguous, configure with `CUDA_ARCHITECTURES` or `CMAKE_CUDA_ARCHITECTURES`.

```bash
./build_lib.sh -D ENABLE_CUDA=ON \
  -D CUDA_ARCHITECTURES=87 \
  -D CUDA_ENABLE_FMAD=ON \
  -D CUDA_USE_FAST_MATH=OFF \
  -D CUDA_PTX_USE_FAST_MATH=ON
```

## OptiX

`ENABLE_OPTIX=ON` enables CUDA and requires at least one compiled library source plus one `*.ptx.cu` source. Header-only OptiX configurations fail at configure time because there is no library artifact that can own the generated PTX integration.

```bash
./build_lib.sh -D ENABLE_OPTIX=ON -D CUDA_ARCHITECTURES=87
```

Set `OPTIX_HOME` or use the system OptiX SDK layout expected by `cmake/HandleOptiX.cmake`.

## TensorRT

`ENABLE_TENSORRT=ON` enables CUDA and links the project through the
`HandleTensorRT.cmake` interface target. Provide `TensorRT_ROOT` or
`TENSORRT_ROOT`; both conventional `include`/`lib` roots and NVIDIA
`targets/<triplet>` archive layouts are supported.

```bash
./build_lib.sh -D ENABLE_TENSORRT=ON \
  -D TensorRT_ROOT=/opt/TensorRT \
  -D CUDA_ARCHITECTURES=87
```

The build and installed package expose `TensorRT::nvinfer`,
`TensorRT::nvinfer_plugin`, and `CUDA::cudart` through the project target.
Consumers of a TensorRT-enabled install must provide a compatible SDK root;
consumers of the default disabled build do not acquire a TensorRT dependency.

## Optional Runtime Libraries

Enable oneTBB when parallel CPU code needs it:

```bash
./build_lib.sh -D ENABLE_TBB=ON
```

Enable profiling only for diagnostic builds:

```bash
./build_lib.sh --profile
```

`ENABLE_TCMALLOC` stays OFF by default because MATLAB MEX and plugin-style consumers are sensitive to allocator dependencies.
