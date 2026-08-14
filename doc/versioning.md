# Versioning Guide

Version resolution is shared by CMake, CPack, generated C++ config headers, Python package metadata, and installed package metadata.

## Resolution Order

The template resolves version in this order:

1. `git describe --tags --long --dirty --always`
2. A source `VERSION` file
3. Hardcoded CMake defaults in the root `CMakeLists.txt`

Supported tags are semantic version tags:

```text
v1.2.3
v1.2.3-rc.1
```

Commits after a tag become build metadata, for example `1.2.3+4.gabc1234`. Dirty worktrees add `dirty` metadata.

## VERSION Files

CMake always writes `${PROJECT_BINARY_DIR}/VERSION` during configure and installs it to the package prefix. Source-tree writes are opt-in:

```bash
cmake -S . -B build -D WRITE_SOURCE_VERSION_FILE=ON
```

Use `generate_version.sh` when you explicitly want to refresh the ignored source `VERSION` file without building:

```bash
./generate_version.sh
```

When the supported ROS 2 overlay helper is present, the same default invocation
also synchronizes `ros2/*/package.xml` metadata. Pass `--no-sync-ros2` to update
only `VERSION`; `--sync-ros2` remains available when automation needs to request
the synchronization explicitly.

Keeping source writes opt-in prevents CI and testfield configure runs from dirtying the checkout.

Binary packages install the generated build-tree `VERSION`. Canonical source
packages instead include the source-tree `VERSION` prepared by
`generate_version.sh`. Do not configure a release package from a stale source
file or mutate release metadata between CMake configuration and CPack; prepare
the checkout again and reconfigure when metadata changes.

## C++ Access

Include the configured header:

```cpp
#include "config.h"

auto version = GetVersionString();
PrintVersion();
```

The header also exposes numeric macros such as `PROJECT_VERSION_MAJOR`.

## Python and Packages

CPack package filenames retain `FULL_VERSION` as SemVer. Python metadata uses a
separate PEP 440 projection: recognized alpha, beta, release-candidate, and
development labels become their canonical PEP 440 forms. An arbitrary SemVer
prerelease such as `1.2.3-feature.x+4.gabc1234` becomes
`1.2.3.dev0+feature.x.4.gabc1234`, preserving prerelease ordering and its label.
Keep public release tags, package uploads, and generated docs aligned by
building release artifacts from an exact
`vMAJOR.MINOR.PATCH[-PRERELEASE]` tag.

<!-- ros2-overlay-begin -->
## Release tagging with the ROS 2 overlay

A release tag must reference a commit whose four `ros2/*/package.xml`
manifests already contain that exact `X.Y.Z` release version. A tag is an
immutable source snapshot: creating the final tag first in the GitHub UI and
synchronizing the manifests afterward leaves the tagged source stale
permanently.

Prepare a release locally in this order, replacing `vX.Y.Z` and the branch
name with the intended release values:

```bash
release_tag=vX.Y.Z
release_branch="$(git branch --show-current)"
test -n "${release_branch}"

# Resolve X.Y.Z without publishing a release tag.
git tag --no-sign "${release_tag}"
./generate_version.sh
git tag -d "${release_tag}"

# Review and commit the synchronized manifests and other release metadata.
git diff -- ros2/*/package.xml
git add ros2/*/package.xml
git commit -m "Prepare ${release_tag} metadata"

# Bind the immutable release name to the synchronized commit.
git tag -a "${release_tag}" -m "Release ${release_tag}"
```

The first tag is a temporary local lightweight tag. Do not push it. After it is
deleted, the synchronized release-preparation commit is expected to fail the
strict version check because Git still resolves the previous release until the
final annotated tag exists on the new commit. Do not publish that intermediate
state. Create the final annotated tag locally, run all release gates with that
tag present, and then publish the branch and tag together:

```bash
./generate_version.sh
./build_lib.sh -B build_release --clean
./build_ros2.sh --clean

# CPack's TGZ is the canonical source release archive.
cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release
cmake -E make_directory dist
cmake -E chdir dist cpack --config ../build_release/CPackSourceConfig.cmake

git push --atomic origin "${release_branch}" "${release_tag}"
```

The atomic push prevents the release commit and its required tag from becoming
visible separately. A release source archive must contain the synchronized ROS
manifests and resolved release metadata. In particular, a no-Git source archive
must include the generated `VERSION` file for the final tag; an arbitrary tree
without Git tag context or that metadata is not a valid release input.

The TGZ produced from `CPackSourceConfig.cmake` is the canonical source release.
It is validated outside Git against the same strict core and full version as the
tagged checkout. CPack includes the prepared source-tree `VERSION` and excludes
the exact active binary directory plus deterministic generated paths such as
the root install prefix and ROS `build`, `install`, and `log` outputs. Prepare a
release from a checkout without additional generated build trees: CPack does not
scan arbitrary `CMakeCache.txt` files or infer ownership from path names.
Build-prefixed source directories and foreign child-project caches therefore
remain package input.

The template appends its deterministic exclusions to
`CPACK_SOURCE_IGNORE_FILES` and leaves caller-owned extension hooks, including
`CPACK_PROJECT_CONFIG_FILE` and CPack install scripts, unchanged. Projects may
add local package policy through those public variables without the template
replacing it.

GitHub's automatic source links are non-canonical: they are repository
snapshots and do not include the generated `VERSION` file required by this
release contract. Uploading the CPack TGZ to a GitHub release remains a
deliberate manual step; CI upload automation is not yet part of the release
workflow.

Pushes of `v*.*.*` tags run the native CPU, CUDA, and ROS workflows. The ROS
workflow regenerates metadata, derives the expected strict core version from
`VERSION`, and requires `git diff --exit-code -- ros2/*/package.xml` to remain
clean. Branch path filters remain in place, but GitHub does not evaluate them
for tag pushes, so the release gates are not skipped merely because a tag has
no changed-path list.
<!-- ros2-overlay-end -->
