# Template Usage Guide

This repository is meant to be renamed into a real C++ library while keeping the build, wrapper, documentation, packaging, and CI machinery reusable.

Agents tailoring a fresh project should use [`bootstrap_prompts.md`](bootstrap_prompts.md) as the interactive configuration checklist before editing.

## Fresh Library Tailoring Sequence

Use this order for a new library checkout:

1. Choose the project name, main C++ module name, optional CUDA module name, C++ namespace, and Python package name.
2. Inspect and apply the template cleanup before the broad rename pass:

   ```bash
   ./tailor_template_cleanup.sh --list
   ./tailor_template_cleanup.sh --apply --yes --project-namespace my_project
   ```

   Replace `my_project` with the chosen C++ project namespace. Add `--keep-profiling` only when the new project should keep the optional Valgrind/perf helper scripts.
3. Rename the template identifiers in tracked source files only. Exclude build trees, install trees, virtual environments, generated Python build metadata, and other generated artifacts. After cleanup succeeds, either delete `tailor_template_cleanup.sh` or exclude it from the rename pass; it is a one-shot template helper.
4. Decide which optional features the project will continue to support,
   including CUDA, OptiX, TensorRT, TBB, and OpenGL. Retained features remain
   dependency-neutral while their options are `OFF`. Keep `FindTensorRT.cmake`
   and `HandleTensorRT.cmake` when TensorRT should remain available; removing
   TensorRT entirely requires a deliberate review of root, source, package,
   test, and documentation references rather than a cleanup-script flag.
5. Remove optional skeletons that the project will not use. For example, if the
   CUDA module directory is deleted, also remove the matching
   `add_subdirectory()` entry from `src/CMakeLists.txt`.
6. Configure, build, and run CTest from a clean build directory.
7. Inspect remaining template names with
   `rg "template_project|template_src|template_src_kernels|cpp_playground"` and
   keep only intentional references in examples or documentation.

The cleanup script contains template-specific filenames and test names, so running it before a global `template_project` replacement avoids stale cleanup paths.

## Rename Checklist

Use one global replacement pass for the project name, then inspect the changed CMake package files and CMake option names.

| Template item | Replace with |
|---|---|
| `template_project` | Project/package name in snake_case |
| `template_src` | Primary C++ module directory |
| `template_src_kernels` | CUDA kernel module directory, or delete if CUDA is not used |
| `cpp_playground` | Top C++ namespace exposed to wrappers |

Set the root project metadata beside `project_name` before building or rolling
out the optional ROS overlay:

```cmake
set(project_name "my_project")
set(project_description "Reusable algorithms for my project")
set(project_homepage_url "https://example.com/my_project")
set(PROJECT_MAINTAINER_NAME "Project Maintainer" CACHE STRING "Project maintainer name")
set(PROJECT_MAINTAINER_EMAIL "maintainer@example.com" CACHE STRING "Project maintainer email")
set(PROJECT_LICENSE "Apache-2.0" CACHE STRING "Project SPDX license identifier")
```

The root `project()` call exports the description and homepage through standard
CMake metadata. The explicit maintainer and SPDX license fields also feed CPack
and ROS package manifests.

<!-- ros2-overlay-begin -->
When the optional ROS 2 overlay is kept, include these paths and identifiers in the same rename review:

| Template item | Replace with |
|---|---|
| `ros2/template_project` | `ros2/<ros_prefix>` shim directory |
| `template_project_interfaces` | `<ros_prefix>_interfaces` |
| `template_project_ros` | `<ros_prefix>_ros` |
| `template_project_spinup` | `<ros_prefix>_spinup` |

The broad `template_project` replacement also updates copied ROS launch/config names, interface package references, and workflow text. After renaming, update the EDIT-ME core-call block in `ros2/<ros_prefix>_ros/src/conversions.cpp`.

When the CMake package name is not a valid ROS package name, keep the original CMake package name for core `find_package(...)` and `<project>::<project>` target links, and use a ROS-valid package prefix for copied ROS package names. For example, `space-nav-frontend` should keep core CMake references to `space-nav-frontend` while using ROS package paths such as `ros2/space_nav_frontend_ros`.

Treat the ROS prefix as one-time package identity chosen during rename or
`add_ros2_support.sh --ros-prefix`. After that mapping is established, run
`./generate_version.sh` for recurring project metadata updates. The generator
synchronizes automatically when the supported ROS overlay helper is present;
use `--no-sync-ros2` when only the ignored `VERSION` file should change.
The command updates version, description, maintainer, license, and website but
does not rename ROS packages or their dependencies.

Remove the overlay with:

```bash
./tailor_template_cleanup.sh --apply --yes --project-namespace my_project --remove-ros2
```

`--remove-ros2` strips the fenced ROS documentation blocks and removes the overlay files. Leave the flag off when the derived project should keep ROS support.
<!-- ros2-overlay-end -->

Update these files first:

- `CMakeLists.txt`: `set(project_name "...")`
- `CMakeLists.txt`: `project_description`, `project_homepage_url`,
  `PROJECT_MAINTAINER_NAME`, `PROJECT_MAINTAINER_EMAIL`, and `PROJECT_LICENSE`
- `CMakeLists.txt`: default wrapper namespace value if wrappers are used
- `src/CMakeLists.txt`: module `add_subdirectory()` entries and status messages
- `src/cmake/template_projectConfig.cmake.in`: rename file and package references
- `src/bin/`, `examples/`, and `tests/`: include paths and starter class names
- `python/pyproject.toml.in`: package metadata
- `python/template_project/`: package directory name
- `.github/workflows/*.yml`: workflow names, artifact names, and renamed CMake option prefixes when useful
- `README.md` and `doc/main_page.md`: public project name and usage notes

## Adding C++ Code

Put public headers and compiled library sources under `src/<module>/`. The default library target exports `${PROJECT_NAME}::${PROJECT_NAME}` after installation and exposes headers from `include/<project_name>/`.

The expected pattern is:

```text
src/<module>/CMakeLists.txt
src/<module>/CMyClass.h
src/<module>/CMyClass.cpp
tests/<module>_test/testMyClass.cpp
examples/<module>_examples/exampleMyClass.cpp
```

Keep the wrapper-facing facade under `src/wrapped_impl/` when a stable Python/MATLAB API should differ from internal C++ classes.

## Library Consumers

Installed consumers should use the exported package:

```cmake
find_package(my_project REQUIRED)
target_link_libraries(my_target PRIVATE my_project::my_project)
```

Nested consumers should override the internal target namespace if they include multiple template-derived libraries:

```cmake
set(LIB_NAMESPACE_OVERRIDE nested_my_project CACHE STRING "" FORCE)
set(LIB_TARGET_NAME_OVERRIDE nested_my_project_library CACHE STRING "" FORCE)
set(my_project_METADATA_ONLY OFF CACHE BOOL "" FORCE)
set(my_project_ENABLE_CUDA OFF CACHE BOOL "" FORCE)
set(my_project_ENABLE_OPTIX OFF CACHE BOOL "" FORCE)
set(my_project_ENABLE_TENSORRT OFF CACHE BOOL "" FORCE)
add_subdirectory(path/to/my_project)
target_link_libraries(parent_target PRIVATE nested_my_project::my_project)
```

The project-qualified metadata, CUDA, OptiX, and TensorRT options are canonical
for `add_subdirectory()` consumers and cannot collide with an application's
generic cache entries. The historical `PROJECT_METADATA_ONLY`, `ENABLE_CUDA`,
`ENABLE_OPTIX`, and `ENABLE_TENSORRT` spellings remain one-config, top-level
compatibility aliases only. When supplied, a legacy alias wins for that
configure, migrates its value to the canonical project-qualified option, and is
removed from the cache. Replace `my_project` with the renamed root
`project_name`.

Only the main project configures documentation, tests, examples, wrappers, and generic `doc` targets. Nested projects keep their library target available without publishing documentation for the parent build.

## Tests

Use Catch2 for compiled tests, pytest for Python tests, and CTest as the common
runner. Put compiled tests in `test*.cpp` or `test*.cu` files and Python tests in
`test*.py` files.

The template checkout contains only runtime tests and fixtures that a derived
project should inherit. Generic tailoring, workflow, packaging, installation,
and platform conformance is owned by the standalone harness in
`cpp_cuda_template_testfield`; those verifier implementations are not part of
this source tree. A derived project should prove its own configuration, feature
matrices, installation, packaging, and external consumption with explicit fresh
out-of-tree commands in its acceptance/CI matrix. Do not make ordinary CTest
recursively configure and rebuild the same project when CI already owns that
behavioral gate.

A permanent derived-project CMake-script test is justified only when it is
lightweight, project-specific, cannot be covered by an existing runtime target
or acceptance command, and does not recursively rebuild the project.

Run focused checks during development. Prefer `ctest --test-dir <build>` so the
same command works from the repository root, local scripts, and CI jobs:

```bash
cmake -S . -B build -DENABLE_TESTS=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Test discovery is filename based:

- `test*.cpp` and `test*.cu` become Catch2 tests when Catch2 is available.
- `test*.py` becomes pytest-backed CTest tests when `ENABLE_PYTHON_TESTS=ON`.
- Use `EXCLUDED_LIST` in `tests/CMakeLists.txt` for local files that should not
  be registered by the generic helper.

Run focused subsets:

```bash
ctest --test-dir build --output-on-failure -L python
ctest --test-dir build --output-on-failure -L catch2
ctest --test-dir build --output-on-failure -R testPythonSmoke
```

Pass local CTest filters through the build helper:

```bash
./build_lib.sh --ctest-extra-args "-L python"
```

Run Python tests inside conda while keeping C++ tests native:

```bash
cmake -S . -B build -DENABLE_TESTS=ON -DPYTHON_TEST_CONDA_ENV=my_env
cmake -S . -B build -DENABLE_TESTS=ON -DPYTHON_TEST_CONDA_PREFIX=/path/to/conda/env
```

Use `PYTHON_TEST_CONDA_ENV` for a named environment and
`PYTHON_TEST_CONDA_PREFIX` for a temporary or path-pinned environment. Set only
one of them. The selected environment must provide `pytest`; CMake checks this
only when Python tests are enabled and at least one `test*.py` file is present.

## Tailoring Cleanup Script

Before broad renaming, list template-development-only files:

```bash
./tailor_template_cleanup.sh --list
```

Apply the cleanup once the list is acceptable:

```bash
./tailor_template_cleanup.sh --apply --yes --project-namespace my_project
```

By default this also removes `profiling/`. Keep those scripts only when the new project will use Valgrind/perf helpers:

```bash
./tailor_template_cleanup.sh --apply --yes --project-namespace my_project --keep-profiling
```

The script replaces `template_project::logging` with the required project
namespace, then removes agent/context notes, internal development and review
records, optional profiling scripts, and the workspace file tied to this
template checkout. It keeps reusable project infrastructure such as `cmake/`
(including the Python/MATLAB wrapper and runtime-staging modules),
`build_lib.sh`, issue forms, examples, toolchains, starter runtime tests,
MATLAB wrapper checks, `.devcontainer/`, and `.vscode/`.

Root `CMakeLists.txt`, `tests/CMakeLists.txt`, custom downstream tests, and the
non-ROS workflows are stable inputs: cleanup does not parse or rewrite them.
This boundary is intentional. Template-system conformance remains external in
TestField, while project-owned runtime checks remain with the project.

### Project workflows

The four runnable `.github/workflows/*.yml` files are directly reusable by a
derived project. There are no dormant workflow copies and no materialization
step. Normal cleanup preserves all four files byte-for-byte.

With `--remove-ros2`, cleanup removes only
`.github/workflows/build_ros2_overlay.yml` along with the optional overlay.
Without that flag, the workflow remains available. Make project-specific
runner, dependency, and deployment changes directly in the `.yml` files after
cleanup.
