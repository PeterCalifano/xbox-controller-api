# C++ Build Guide

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
  -D xbox_controller_api_BUILD_PROGRAMS=OFF \
  -D xbox_controller_api_BUILD_EXAMPLES=OFF
```

Use `CPU_EXTRA_OPT_FLAGS` for target-specific flags that are safe for the destination CPU.

## Optional Runtime Libraries

Enable oneTBB when parallel CPU code needs it:

```bash
./build_lib.sh -D ENABLE_TBB=ON
```

Enable profiling only for diagnostic builds:

```bash
./build_lib.sh --profile
```

`ENABLE_TCMALLOC` stays OFF by default because plugin-style consumers are sensitive to allocator dependencies.
