# Testing, CI, and Issue Workflow

## Local Gates

Use CTest for compiled and Python runtime tests.
`ctest --test-dir <build>` is the preferred form because it works from the
repository root, from scripts, and from CI jobs without changing directories:

```bash
cmake -S . -B build -DENABLE_TESTS=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Compiled tests are discovered from `test*.cpp` and `test*.cu` files and run with
Catch2. Python tests are discovered from `test*.py` files and registered as
normal CTest entries that execute `python -m pytest -q <test-file>`.

### Template conformance versus derived-project acceptance

The template's default CTest suite is already the derived-project suite. It
contains inherited C++ logger/starter behavior, Python import smoke coverage,
optional CUDA initialization/placeholder behavior, reusable fixtures, and
target-owned MATLAB wrapper checks. Tailoring does not rewrite its
registrations.

Generic template-system conformance is implemented by the standalone harness in
`cpp_cuda_template_testfield`. That harness receives an explicit candidate
source path and owns tailoring, workflows, release metadata, packaging,
installation, consumer, nested-build, cross-compilation, CUDA/OptiX, wrapper,
and static ROS contracts. Its verifier implementations are not copied into this
repository or a derived project.

Do not reproduce them as recursive CMake tests in a derived project. In
particular, ordinary derived-project CTest must not configure and rebuild the
same project again merely to cover an option combination, headless build,
installation, package archive, or consumer build. Put those gates in explicit
fresh out-of-tree acceptance commands or CI jobs, where build ownership,
prerequisites, logs, and artifacts are visible.

Permanent CMake-script tests in a derived project are limited to lightweight,
project-owned checks that cannot be expressed through Catch2, pytest, an
existing build target, or the acceptance matrix. They must not import
`VerifyTemplateProject*` scripts or create nested full-project builds.

Catch2 remains the unit-test framework for the core C++ and CUDA project. Tests
inside ROS packages are the deliberate exception: they use
`ament_cmake_gtest` so test targets and results participate in the ament/colcon
workspace contract. This ROS-specific integration does not change the native
project's Catch2 policy.

The discovery helper is shared by starter projects and downstream projects:

- `test*.cpp` and `test*.cu`: compiled only when Catch2 is available.
- `test*.py`: registered when `ENABLE_TESTS=ON` and `ENABLE_PYTHON_TESTS=ON`.
- `EXCLUDED_LIST`: accepts either full filenames such as `testSlow.py` or stems
  such as `testSlow`.
- Catch2 tests get the default `catch2` label.
- Python tests get the `python` and `pytest` labels.

Useful local filters:

```bash
ctest --test-dir build --output-on-failure -L catch2
ctest --test-dir build --output-on-failure -L python
ctest --test-dir build --output-on-failure -R testPythonSmoke
```

Catch2 output and properties are controlled through CMake cache values:

```bash
cmake -S . -B build \
  -DCATCH2_TEST_REPORTER=compact \
  -DCATCH2_TEST_PROPERTIES="LABELS;catch2"
```

Keep CTest command-line switches such as `--output-on-failure`, `-R`, `-L`, and
`--parallel` on the `ctest` invocation. Do not put them in
`CATCH2_TEST_PROPERTIES`; that variable is only for CTest property name/value
pairs passed to `catch_discover_tests`.

Local development filters can be passed through the build helper:

```bash
./build_lib.sh --ctest-extra-args "-L python"
```

`--ctest-extra-args` is for local development only. It is useful for quickly
reusing an existing build tree through `build_lib.sh`, but CI workflows should
spell out their own `ctest` command and filters directly. The value is split on
whitespace; run `ctest` directly for filters or arguments that need shell
quoting.

Use a conda environment for Python tests without wrapping the full CTest run.
This keeps C++ tests native while Python test files run inside the requested
environment:

```bash
cmake -S . -B build -DENABLE_TESTS=ON -DPYTHON_TEST_CONDA_ENV=my_env
cmake -S . -B build -DENABLE_TESTS=ON -DPYTHON_TEST_CONDA_PREFIX=/path/to/conda/env
ctest --test-dir build --output-on-failure -L python
```

Use `PYTHON_TEST_CONDA_ENV` for a named environment and
`PYTHON_TEST_CONDA_PREFIX` for a specific environment directory. Set only one of
them. If neither is set, Python tests use `PYTHON_TEST_EXECUTABLE` when provided,
then the configured `Python3` interpreter, then `python3`/`python` from `PATH`.
The selected interpreter must have `pytest` installed, and the validation happens
during CMake configure only when at least one `test*.py` file is actually being
registered.

To disable Python tests while keeping Catch2 tests:

```bash
cmake -S . -B build -DENABLE_TESTS=ON -DENABLE_PYTHON_TESTS=OFF
```

Focused documentation build:

```bash
cmake --preset docs
cmake --build --preset docs
```

## CI Workflows

The active `.github/workflows/*.yml` files are reusable project workflows and
survive normal tailoring unchanged. There are no dormant `.tpl` copies. Native
CPU, CUDA, ROS, and Pages workflows therefore exercise the same definitions
that a derived project receives.

Template-system workflow structure and behavior is checked from TestField
against an explicitly selected candidate. The parser-backed contract validates
triggers, job topology, checkout depth, shell syntax, current Pages actions,
ROS step ordering, and metadata drift behavior without adding those checks to
ordinary project CTest.

The Linux workflows keep CPU tuning portable because build artifacts are tested in a separate job. Do not re-enable `CPU_ENABLE_NATIVE_TUNING=ON` in GitHub Actions unless build and test run on the same pinned CPU family.

The native CPU, CUDA, and ROS workflows also run for `v*.*.*` tag pushes.
Their existing `paths` filters continue to scope branch
pushes and pull requests; GitHub does not evaluate path filters for tag pushes,
so a release tag still executes the release-relevant build gates. See
[GitHub workflow syntax](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax#onpushpull_requestpull_request_targetpathspaths-ignore).

CUDA jobs require a self-hosted runner with the labels `Linux`, `X64`, `gpu`,
and `cuda`. Set the repository variable `CI_USE_SELF_HOSTED` to `true` only
while such a runner is available. When the variable is unset or has any other
value, both CUDA jobs are skipped before runner allocation, so release-tag and
manual workflow runs do not remain queued indefinitely.

The ROS workflow executes `./generate_version.sh --sync-ros2` when the helper
advertises full metadata synchronization, then rejects any tracked manifest
change with:

```bash
git diff --exit-code -- ros2/*/package.xml
```

It emits a compatibility warning instead of failing when an older derived
project has not adopted that capability yet. After workflow-owned
synchronization, CI passes
`--no-version-sync` to the build helper to avoid a second unguarded rewrite.

Project workflows install only their runtime/build prerequisites. PyYAML and
the parser-backed contract belong to TestField rather than the delivered
project. Documentation jobs install Doxygen and Graphviz; CUDA jobs validate
their host tools before configuring.

The Pages workflow is separate from the C++ build workflow. It has these stages:

1. Configure docs with CUDA, OptiX, and tests disabled.
2. Build Doxygen HTML and XML.
3. Verify `index.html` exists before upload.
4. Upload the Pages artifact.
5. Deploy only for default-branch pushes, or manual dispatch when `deploy_pages=true`.
6. Fetch the deployed Pages URL and verify that it returns non-empty content.

Tests prefer executable behavior or native parsers for YAML, JSON, XML, and
generated CMake metadata. Exact text is reserved for generated output whose
representation is itself contractual, such as synchronized metadata and
preserved XML processing instructions.

## Issue Templates

Issue forms are structured so bug reports capture:

- build mode and compiler
- operating system and runner type
- CUDA/OptiX/wrapper settings
- failing command and output excerpt
- whether the problem affects docs or Pages
- whether the project is top-level or nested through `add_subdirectory`

Feature requests should state the owning surface: C++ library, CUDA/OptiX, wrappers, versioning, docs/Pages, CI, packaging, or testfield validation.

## Stop Rule

For staged workflow changes, stop on the first critical blocker that prevents trustworthy validation. Record:

- stage name
- command
- exit code
- short output excerpt
- likely owner
- next action
