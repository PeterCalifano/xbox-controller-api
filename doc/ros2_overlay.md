# ROS 2 Overlay

The optional ROS 2 overlay is a colcon workspace layered on top of the C++-first template. The normal library entry point is still `./build_lib.sh`; it never needs ROS and never reads `ros2/`.

## Encapsulation contract

ROS integration lives in `ros2/` plus the root overlay helpers:

- `build_ros2.sh`
- the four root `COLCON_IGNORE` markers
- `.github/workflows/build_ros2_overlay.yml`
- this documentation and the ROS package runtime tests

There is no root `package.xml` and no `ENABLE_ROS2` CMake option. The shim package at `ros2/xbox_controller_api/` is the only package that includes the core library. Its `CMakeLists.txt` preloads the real root `cmake/` directory, then calls `add_subdirectory()` on the repository root so the usual install/export rules publish `xbox_controller_api::xbox_controller_api` into the colcon install prefix.

Downstream ament packages depend on the shim package named `xbox_controller_api`. After colcon builds the shim, its install prefix is on `CMAKE_PREFIX_PATH`, so `xbox_controller_api_ros` can use:

```cmake
find_package(xbox_controller_api REQUIRED)
target_link_libraries(my_target PRIVATE xbox_controller_api::xbox_controller_api)
```

## Package layout

The overlay packages are:

| Package | Role |
|---|---|
| `xbox_controller_api` | Plain CMake shim around the core library. |
| `xbox_controller_api_interfaces` | ROS messages and services. |
| `xbox_controller_api_ros` | Bridge package with conversions, lifecycle node, component, executable, and tests. |
| `xbox_controller_api_spinup` | Launch files and default node configuration. |

The `xbox_controller_api_ros` package keeps a conversions-vs-node split. `xbox_controller_api_ros_conversions` links the core library and interfaces but does not depend on `rclcpp`; it is safe to test without a ROS executor. `xbox_controller_api_ros_component` owns lifecycle, parameters, publishers, services, and component registration.

Core C++ unit tests remain Catch2-based. ROS package tests use
`ament_cmake_gtest` as the narrow ROS-specific exception so ament registers and
reports them through colcon.

The bridge is intentionally source-adjacent: its private include path can reach core headers under the repository `src/` tree without making those headers part of the installed public API. A derived project should adapt `conversions.cpp` to use an exported public core header whenever one exists. An installed-only bridge consumer requires the core project to install/export that header first; the overlay does not turn private source headers into a public SDK.

## Build usage

Source a ROS 2 environment, or let `build_ros2.sh` source `/opt/ros/${ROS_DISTRO:-jazzy}/setup.bash`:

```bash
./build_ros2.sh --clean
./build_ros2.sh --skip-tests
./build_ros2.sh --packages-select xbox_controller_api_ros
./build_ros2.sh --debug
./build_ros2.sh --cmake-arg -DCMAKE_VERBOSE_MAKEFILE=ON
```

The script defaults `ROS_DISTRO` to `jazzy`, but it is otherwise distro-agnostic when the requested distro is installed.

The supplied standalone and composition launch files autostart the lifecycle node through `launch_ros`: they request configure, wait for the inactive state, then request activate. The service is therefore ready when either launch path finishes starting. Each launch file retains the equivalent plain `Node` or `ComposableNode` description as a commented template alternative. Uncomment that form only when an external lifecycle manager owns transitions; launching the raw executable, loading the raw component, or using those alternatives intentionally leaves the node unconfigured.

Jazzy's current `ComposableLifecycleNode` implementation resolves the loaded component's fully qualified name inconsistently during autostart. The composition launch file supplies that identity to the lifecycle event manager locally; remove the compatibility adapter after the upstream `launch_ros` fix is available in the supported ROS distro.

## COLCON_IGNORE policy

`COLCON_IGNORE` markers keep colcon from crawling template support trees that are not ROS packages:

- `python/COLCON_IGNORE`: required because generated `setup.py` files can be misdetected as Python packages.
- `lib/COLCON_IGNORE`: protects vendored submodules if they contain manifests.
- `examples/COLCON_IGNORE` and `tests/COLCON_IGNORE`: avoid accidental package discovery in starter project code.

There are no markers in `doc/`. Runtime-generated top-level directories such as `build*`, `install`, and `xbox_controller_api_subbuild` are handled best-effort by `build_ros2.sh` when they exist. This matters when the repository is placed inside a parent workspace: without the markers, a parent colcon crawl can discover unrelated template internals.

## Project metadata sync

The root CMake project is the source of truth for overlay metadata. Standard
`project(DESCRIPTION ... HOMEPAGE_URL ...)` fields export
`CMAKE_PROJECT_DESCRIPTION` and `CMAKE_PROJECT_HOMEPAGE_URL`; cache-backed
`PROJECT_MAINTAINER_NAME`, `PROJECT_MAINTAINER_EMAIL`, and `PROJECT_LICENSE`
fields provide the remaining manifest identity. `PROJECT_METADATA_ONLY=ON`
configures these values and the resolved version with `LANGUAGES NONE`, then
returns before compilers, dependencies, targets, wrappers, tests, docs, or
packaging are configured.

ROS package manifests require strict `X.Y.Z` versions. Before each overlay
build, `build_ros2.sh` runs:

```bash
./generate_version.sh --sync-ros2
```

unless `--no-version-sync` is passed. The flag keeps its legacy spelling for
compatibility, but now disables the complete metadata synchronization for that
overlay build. A normal `./generate_version.sh` invocation performs the same
synchronization automatically when the supported helper is present;
`--no-sync-ros2` is the standalone opt-out and `--sync-ros2` remains the
explicit compatibility form. The command invokes
`ros2/tools/sync_package_metadata.py`, which updates each
immediate `ros2/*/package.xml` version, role-specific description, maintainer,
license, and website URL from the root CMake cache. It preserves the established
ROS package names, XML processing instructions, non-website URLs, dependencies,
and file modes.

Package names are intentionally outside recurring synchronization. They are a
one-time rollout or tailoring decision and may differ from the CMake project
name. This prevents an explicit `--ros-prefix` from being replaced by a later
metadata refresh.

The command no-ops when `ros2/` is absent, so keeping `generate_version.sh` in
tailored non-ROS projects is safe. It also retains the existing hardcoded-version
fallback guard. An older derived repository must adopt the root metadata fields
and metadata-only configure branch before using the new helper; additive overlay
rollout does not modify its root `CMakeLists.txt`. Copied build and CI helpers
also require the `ROS2_PROJECT_METADATA_SYNC=1` capability marker, so the older
version-only implementation of `--sync-ros2` is skipped instead of being
misreported as a complete metadata refresh.

Run `./generate_version.sh` manually after changing root project metadata or tags, and
before packaging source archives. Releases need the tag-safe preparation order
described in [Release tagging with the ROS 2 overlay](versioning.md#release-tagging-with-the-ros-2-overlay).

## Adaptation seam

The primary adaptation seam is:

```text
ros2/xbox_controller_api_ros/src/conversions.cpp
```

Update the fenced include and the `EvaluateTemplateCore` body to call the real
library API. It currently calls the placeholder so the overlay keeps building.
Review `ros2/xbox_controller_api_ros/src/CXboxControllerLifecycleNode.cpp` only
when ROS node wiring, parameters, publishers, or services also need to change.
Then run `./build_ros2.sh --clean`.

## Removal

To drop the overlay, delete `ros2/`, `build_ros2.sh`, the ROS overlay CI
workflow (`.github/workflows/build_ros2_overlay.yml`), this document, and the
ROS static pytest. Remove the four `COLCON_IGNORE` markers only if nothing else
needs them. Keep `generate_version.sh`; its ROS synchronization is already a
no-op when the overlay is absent.

## CI

The reusable `.github/workflows/build_ros2_overlay.yml` runs one
`overlay-build` job in the `ros:jazzy` container. It installs dependencies,
uses the metadata helper when the checkout advertises full synchronization,
runs `rosdep install --from-paths ros2 -i -r -y --rosdistro jazzy`, then builds
and tests the overlay. Each supported synchronization is followed by:

```bash
git diff --exit-code -- ros2/*/package.xml
```

The workflow watches the project source and overlay paths and runs for
`v*.*.*` tag pushes as well as its branch events. It warns and continues with
existing manifests when an older derived project lacks the full metadata
marker; when synchronization is supported, manifest drift is a hard failure.


## Python boundary

`python/` bindings remain a separate ROS-free optional feature. The overlay does not depend on Python bindings, and the static tests check that `python/` stays free of `rclcpp`, `ament`, and `rosidl` references.
