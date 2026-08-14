# ROS 2 Overlay

The optional ROS 2 overlay is a colcon workspace layered on top of the C++-first template. The normal library entry point is still `./build_lib.sh`; it never needs ROS and never reads `ros2/`.

## Encapsulation contract

ROS integration lives in `ros2/` plus the root overlay helpers:

- `build_ros2.sh`
- `add_ros2_support.sh`
- the four root `COLCON_IGNORE` markers
- `.github/workflows/build_ros2_overlay.yml`
- this documentation and the ROS package runtime tests

There is no root `package.xml` and no `ENABLE_ROS2` CMake option. The shim package at `ros2/template_project/` is the only package that includes the core library. Its `CMakeLists.txt` preloads the real root `cmake/` directory, then calls `add_subdirectory()` on the repository root so the usual install/export rules publish `template_project::template_project` into the colcon install prefix.

Downstream ament packages depend on the shim package named `template_project`. After colcon builds the shim, its install prefix is on `CMAKE_PREFIX_PATH`, so `template_project_ros` can use:

```cmake
find_package(template_project REQUIRED)
target_link_libraries(my_target PRIVATE template_project::template_project)
```

## Package layout

The overlay packages are:

| Package | Role |
|---|---|
| `template_project` | Plain CMake shim around the core library. |
| `template_project_interfaces` | ROS messages and services. |
| `template_project_ros` | Bridge package with conversions, lifecycle node, component, executable, and tests. |
| `template_project_spinup` | Launch files and default node configuration. |

The `template_project_ros` package keeps a conversions-vs-node split. `template_project_ros_conversions` links the core library and interfaces but does not depend on `rclcpp`; it is safe to test without a ROS executor. `template_project_ros_component` owns lifecycle, parameters, publishers, services, and component registration.

Core C++ and CUDA unit tests remain Catch2-based. ROS package tests use
`ament_cmake_gtest` as the narrow ROS-specific exception so ament registers and
reports them through colcon.

The bridge is intentionally source-adjacent: its private include path can reach core headers under the repository `src/` tree without making those headers part of the installed public API. A derived project should adapt `conversions.cpp` to use an exported public core header whenever one exists. An installed-only bridge consumer requires the core project to install/export that header first; the overlay does not turn private source headers into a public SDK.

## Build usage

Source a ROS 2 environment, or let `build_ros2.sh` source `/opt/ros/${ROS_DISTRO:-jazzy}/setup.bash`:

```bash
./build_ros2.sh --clean
./build_ros2.sh --skip-tests
./build_ros2.sh --packages-select template_project_ros
./build_ros2.sh --debug
./build_ros2.sh --cmake-arg -DCMAKE_VERBOSE_MAKEFILE=ON
```

The script defaults `ROS_DISTRO` to `jazzy`, but it is otherwise distro-agnostic when the requested distro is installed.

The supplied standalone and composition launch files autostart the lifecycle node through `launch_ros`: they request configure, wait for the inactive state, then request activate. The service is therefore ready when either launch path finishes starting. Each launch file retains the equivalent plain `Node` or `ComposableNode` description as a commented template alternative. Uncomment that form only when an external lifecycle manager owns transitions; launching the raw executable, loading the raw component, or using those alternatives intentionally leaves the node unconfigured.

Jazzy's current `ComposableLifecycleNode` implementation resolves the loaded component's fully qualified name inconsistently during autostart. The composition launch file supplies that identity to the lifecycle event manager locally; remove the compatibility adapter after the upstream `launch_ros` fix is available in the supported ROS distro.

CUDA and OptiX flow through a workspace option facade:

| User flag | Colcon CMake argument | Shim mapping | Core CMake option |
|---|---|---|---|
| `--cuda` | `-DTEMPLATE_PROJECT_ENABLE_CUDA=ON` | cache-forces `template_project_ENABLE_CUDA` | `template_project_ENABLE_CUDA=ON` |
| `--optix` | `-DTEMPLATE_PROJECT_ENABLE_OPTIX=ON` and CUDA ON | cache-forces `template_project_ENABLE_OPTIX` | `template_project_ENABLE_OPTIX=ON` |

`TEMPLATE_PROJECT_ENABLE_CUDA` and `TEMPLATE_PROJECT_ENABLE_OPTIX` are stable
overlay facade names. They intentionally survive CMake project and ROS package
renaming so build automation has one consistent interface across derived
repositories. The shim cache-forces the core options from these facade values,
so direct `--cmake-arg -Dtemplate_project_ENABLE_CUDA=ON` or
`--cmake-arg -Dtemplate_project_ENABLE_OPTIX=ON` values are overwritten by the
shim. Use
`--cuda`, `--optix`, or set the corresponding facade variables instead.

Use a ROS 2 Jazzy environment or the ROS devcontainer for local GPU checks:

```bash
./build_ros2.sh --cuda
./build_ros2.sh --cuda --optix
```

For OptiX, provide an SDK root containing `include/optix.h` through a CMake
variable or the environment. The same contract applies when another package
consumes a core install that was built with OptiX:

```bash
export OPTIX_HOME="<optix-sdk-root>"
./build_ros2.sh --clean --cuda --optix
```

`OPTIX_ROOT`, `OptiX_ROOT`, and `OptiX_INSTALL_DIR` are equivalent CMake-side
inputs. The installed package resolves the external SDK at consumer configure
time; it does not embed the build machine's SDK path or install a private copy
of the OptiX headers.

**Last local GPU validation (2026-07-17):** ROS 2 Jazzy, CMake 3.28.3, GCC
13.3.0, CUDA 12.9.41, NVIDIA driver 580.105.08, and OptiX 8.0.0. The host had
an RTX 5090 (`sm_120`) and an RTX 4070 Ti SUPER (`sm_89`); the default
single-architecture policy selected `sm_120`. Clean CUDA and CUDA+OptiX overlay
builds each completed all four packages and reported 10 tests with zero errors
or failures. The CUDA build compiled the project
`src/template_src_kernels/placeholder.cu`; the OptiX build also generated and
embedded `placeholder_to_ptx.ptx`. GitHub ROS CI remains CPU-only, so these GPU
paths are local validation gates.

## COLCON_IGNORE policy

`COLCON_IGNORE` markers keep colcon from crawling template support trees that are not ROS packages:

- `python/COLCON_IGNORE`: required because generated `setup.py` files can be misdetected as Python packages.
- `lib/COLCON_IGNORE`: protects vendored submodules if they contain manifests.
- `examples/COLCON_IGNORE` and `tests/COLCON_IGNORE`: avoid accidental package discovery in starter project code.

There are no markers in `doc/`, `matlab/`, or `profiling/`. Runtime-generated top-level directories such as `build*`, `install`, and `template_subbuild` are handled best-effort by `build_ros2.sh` when they exist. This matters when the repository is placed inside a parent workspace: without the markers, a parent colcon crawl can discover unrelated template internals.

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

## Rollout to derived repositories

Use `add_ros2_support.sh` from this template checkout when a derived repository does not already contain a ROS overlay:

```bash
./add_ros2_support.sh --root /path/to/derived_repo --apply --yes --verify
```

The rollout script is purely additive. It refuses targets that already have `ros2/` or `build_ros2.sh`, copies the overlay files, renames copied ROS package paths and copied file contents from `template_project` to a ROS package prefix, and leaves existing target files untouched.

For CI, rollout copies the reusable
`.github/workflows/build_ros2_overlay.yml` directly into the target. The source
repository and derived projects therefore execute the same workflow definition.
Broader rollout and static-overlay conformance remains in the external
`cpp_cuda_template_testfield` harness.

By default, the ROS package prefix is derived from the target CMake package name in `set(project_name "...")`. If the CMake package name is already ROS-valid, the two names match. If the CMake package name is not ROS-valid, the script keeps core CMake references pointed at the original CMake package name while using a ROS-valid package prefix for ROS package names. For example, a target CMake package named `space-nav-frontend` keeps this core CMake shape:

```cmake
find_package(space-nav-frontend REQUIRED)
target_link_libraries(my_target PRIVATE space-nav-frontend::space-nav-frontend)
```

The copied ROS packages use paths such as `ros2/space_nav_frontend_ros`. Pass `--ros-prefix <name>` when the derived repository needs an explicit ROS package prefix.

`add_ros2_support.sh` owns this one-time package identity mapping. After the
target adopts the root metadata contract, `./generate_version.sh`
handles recurring project metadata without renaming that package identity.

After the script runs, complete the EDIT-ME core-call step in the primary adaptation seam:

```text
ros2/<ros_prefix>_ros/src/conversions.cpp
```

Update the fenced include and `EvaluateTemplateCore` body to call the derived library API. Review `ros2/<ros_prefix>_ros/src/CTemplateLifecycleNode.cpp` only when ROS node wiring, parameters, publishers, or services also need to change. Here, `<ros_prefix>` is the ROS-valid prefix reported by `add_ros2_support.sh`, which may differ from the CMake project name. Then run `./build_ros2.sh --clean`.

Supported orders:

- rename-then-overlay: tailor and rename the C++ project first, then run `add_ros2_support.sh`.
- overlay-then-rename: add the overlay to a still-template-shaped checkout, then include the `ros2/` package names in the broad rename pass.

The script does not edit README, AGENTS, CLAUDE, or other existing target docs. Link this file from target docs manually when needed.

Derived repositories that intentionally removed optional template features need one manual tailoring pass after the copy. If CUDA or OptiX support is not present in the target, update the copied `build_ros2.sh` facade, shim CMake options, docs, and CI workflow so unsupported options are not advertised.

## Removal

The overlay is kept by default during template cleanup. Remove it explicitly:

```bash
./tailor_template_cleanup.sh --apply --yes --project-namespace my_project --remove-ros2
```

`--remove-ros2` deletes `ros2/`, `build_ros2.sh`, `add_ros2_support.sh`, the colcon markers, the ROS overlay CI workflow, this file, and the ROS static pytest. It also strips `<!-- ros2-overlay-begin -->` / `<!-- ros2-overlay-end -->` fenced blocks from the template docs. `generate_version.sh` is left in place because its automatic ROS synchronization already no-ops without the supported overlay helper.

For an already-tailored repository that removed
`tailor_template_cleanup.sh`, perform manual removal: delete `ros2/`,
`build_ros2.sh`, `add_ros2_support.sh`, the ROS workflow and any remaining
`.tpl`, this document, and the ROS static pytest. Remove the four
`COLCON_IGNORE` markers only when the overlay introduced them and the derived
project does not otherwise need them. Finally, remove the complete
`<!-- ros2-overlay-begin -->` through `<!-- ros2-overlay-end -->` blocks from
`README.md`, agent guidance, `doc/bootstrap_prompts.md`,
`doc/template_usage.md`, and `doc/versioning.md`. Reject orphaned, nested, or
unclosed markers rather than deleting an ambiguous span. Keep
`generate_version.sh`; its automatic ROS synchronization is already a no-op
when the supported overlay helper is absent.

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

TestField separately owns static manifest checks, additive rollout fixtures,
identifier-boundary renaming, and default/ROS-removed tailoring conformance.

CUDA+ROS is local-only in this repository. The available self-hosted GPU runner does not provide the ROS environment, so CI intentionally avoids `build_ros2.sh --cuda`.

## Python boundary

`python/` bindings remain a separate ROS-free optional feature. The overlay does not depend on Python bindings, and the static tests check that `python/` stays free of `rclcpp`, `ament`, and `rosidl` references.
