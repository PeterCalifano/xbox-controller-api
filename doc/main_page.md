# xbox_controller_api {#mainpage}

A C++ library for reading Xbox controller input, with optional Python bindings
and ROS 2 integration.

@note The controller API itself is not implemented yet. The library currently
exposes placeholder types (`placeholder.h`, `CWrapperPlaceholder`) that mark
where the real implementation goes. The build, packaging, wrapper,
documentation, and CI machinery around them is complete.

See the [README](README.md) for the quick start, then use the focused guides below:

- [C++ build guide](cpp_build.md)
- [Python wrappers](wrappers.md)
- [Versioning](versioning.md)
- [Dependency-free logging](logging.md)
- [Documentation workflow](documentation_workflow.md)
- [Testing, CI, and issue workflow](testing_and_ci.md)
- [Optional ROS 2 overlay](ros2_overlay.md)

## Installation

```bash
git clone https://github.com/PeterCalifano/xbox-controller-api.git
cd xbox-controller-api
./build_lib.sh -t release -i      # build + install to ./install
```

## Common Build Toggles

```bash
# Enable oneTBB and explicit SIMD/FMA
./build_lib.sh -D ENABLE_TBB=ON -D CPU_ENABLE_SIMD=ON -D CPU_SIMD_LEVEL=avx2 -D CPU_ENABLE_FMA=ON

# Native tuning is disabled automatically for cross builds
./build_lib.sh --toolchain cmake/toolchains/defaults/aarch64-linux-gnu.cmake --clean
```

## Wrapper Build

```bash
# Python wrapper
./build_lib.sh -p

# Use a local wrap checkout instead of installed gtwrap
./build_lib.sh -p --gtwrap-root /path/to/wrap
```

Install the Python package manually from the source Python package:

```bash
cd python
python -m pip install .
```

## Example usage (assuming installation worked)

```cmake
set(xbox_controller_api_DIR "/path/to/install/lib/cmake/xbox_controller_api")
find_package(xbox_controller_api REQUIRED)
target_link_libraries(my_target PRIVATE xbox_controller_api::xbox_controller_api)
```

See `examples/consumer_project/` for a complete downstream CMake project.

## Optional ROS 2 Overlay

The standalone C++ library builds with `./build_lib.sh` and never requires ROS.
The optional overlay in `ros2/` builds separately:

```bash
./build_ros2.sh --clean
```

The core-call seam lives in `ros2/xbox_controller_api_ros/src/conversions.cpp`,
marked with `EDIT ME` comments. It currently calls the placeholder so the
overlay keeps building; swap it for the real entry point when the library gains
one.
