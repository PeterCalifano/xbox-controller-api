# Optional ROS 2 Overlay Upgrade Plan

## Context

`cpp_cuda_template_project` is and remains a **C++/CMake-first** library template, meant to be consumed by other projects. Several derived repos need an **optional** ROS 2 layer (package configuration, build, bridge/adapter code). The sibling `ros2_cpp_cuda_template_project` stays the ROS-first, standalone template; this upgrade adds a fully-encapsulated `ros2/` overlay to the C++-first template, plus a zero-regression rollout path for already-derived repos.

## Fixed design decisions

- **colcon-only enablement**: the root CMake build never touches `ros2/`; a new `build_ros2.sh` (env-guarded) drives colcon. No `ENABLE_ROS2` CMake option.
- **Full encapsulation in `ros2/`**: the core joins the colcon workspace via a shim package `ros2/template_project/` that `add_subdirectory()`s the repo root. No root `package.xml` (it would hide the nested packages from colcon's crawler). Only files outside `ros2/`: `build_ros2.sh`, `add_ros2_support.sh`, a few one-line `COLCON_IGNORE` markers, docs, tests, one CI workflow.
- **Overlay packages**: shim + `template_project_interfaces` + `template_project_ros` (bridge) + `template_project_spinup` (launch/config). Naming: `spinup`, not `bringup`; no "composable_nodes"-style names except standard ROS API names (`ComposableNodeContainer`, `rclcpp_components`); pass-through options are a "workspace option facade", never "dummy"-flavored names.
- **ROS distro**: Jazzy for CI (`ros:jazzy` container); scripts distro-agnostic via `ROS_DISTRO`.
- **`python/` stays ROS-free**: bindings remain a separate optional feature; the overlay never depends on it (enforced by test).
- **Agent-facing rollout docs**: `doc/ros2_overlay.md` + prompt section in `doc/bootstrap_prompts.md`, referenced from `AGENTS.md`/`CLAUDE.md`.

## Verified facts the design relies on

- Root `CMakeLists.txt` already supports nested inclusion: `BUILD_AS_MAIN_PROJECT` flips OFF under `add_subdirectory` (`CMakeLists.txt:71-75`), gating examples/tests/docs/wrappers/bin. Proven by `tests/cmake/VerifyTemplateProjectCrossCompile.cmake` (nested consumer case).
- Wrinkle: `CMakeLists.txt:41` sets `CMAKE_MODULE_PATH` from `PROJECT_SOURCE_DIR` *before* `project()` → wrong dir when wrapped. The shim pre-appends the real `cmake/` dir, so **no root CMake change is needed** (keeps derived-repo rollout purely additive).
- Config package exported at `lib/cmake/template_project/` with namespace `template_project::` (`src/CMakeLists.txt:219-247`); downstream ament packages `find_package(template_project REQUIRED)` — colcon puts the shim's install prefix on `CMAKE_PREFIX_PATH`.
- `package.xml` requires strict `X.Y.Z`; `HandleGitVersion.cmake` provides `PROJECT_VERSION_CORE`.
- Working checkouts contain a generated `python/setup.py` → colcon would misdetect it as a package when the repo sits in a parent workspace → `python/COLCON_IGNORE` is mandatory.
- Donor skeletons (read-only): `ros2_cpp_cuda_template_project/ros2_ws/src/template_project_{interfaces,nodes,spinup}`, `build_ros2_ws.sh` (env-guard pattern), `.github/workflows/build_ros2.yml` (ros:jazzy CI pattern).

## Assumptions (validated empirically in Stages 1-2, fallbacks noted there)

1. colcon forwards `--cmake-args` to plain-`cmake` build-type packages (standard vendor-package pattern).
2. Dependent ament packages inherit the shim's install prefix via declared `package.xml` deps.

---

## Stage 0 - Plan doc + colcon hygiene (no ROS needed)

- [x] Write this plan to `doc/developments/ros2_overlay_upgrade_plan.md`; tick boxes as stages land.
- [x] Create one-line `COLCON_IGNORE` markers: `python/COLCON_IGNORE` (mandatory — generated `setup.py`), `lib/COLCON_IGNORE` (submodules may ship manifests), `examples/COLCON_IGNORE`, `tests/COLCON_IGNORE`. No markers in `doc/`, `matlab/`, `profiling/` (cannot contain package manifests). Untracked generated dirs (`build*`, `install`, `template_subbuild`) are handled at runtime by `build_ros2.sh` (Stage 1) best-effort touching markers into them; documented caveat in `doc/ros2_overlay.md`.
- [x] `.gitignore`: add `/ros2/build/`, `/ros2/install/`, `/ros2/log/` (root `/*build*/` patterns don't cover subdirs).
- [x] Validate: `./build_lib.sh -B build_stage0 --skip-tests` untouched; `git status --short` shows only intended additions.

## Stage 1 - `ros2/` workspace: core shim + interfaces + `build_ros2.sh`

- [x] `ros2/template_project/package.xml` (shim): format 3, name `template_project`, version matching current core, `<buildtool_depend>cmake</buildtool_depend>`, `<depend>eigen</depend>`, `<export><build_type>cmake</build_type></export>`.
- [x] `ros2/template_project/CMakeLists.txt` (shim strategy):
  - `project(template_project_colcon_shim LANGUAGES NONE)` — nested `project()` enables CXX/CUDA.
  - **Workspace option facade**: `option(TEMPLATE_PROJECT_ENABLE_CUDA ...)` / `option(TEMPLATE_PROJECT_ENABLE_OPTIX ...)` mapped onto core `ENABLE_CUDA`/`ENABLE_OPTIX` cache vars (so one workspace-wide `--cmake-args` works for every package; packages that don't consume a flag still accept it).
  - Library-only defaults under colcon: `ENABLE_TESTS OFF`, `ENABLE_FETCH_CATCH2 OFF` (cache, FORCE); `BUILD_AS_MAIN_PROJECT` gates the rest automatically.
  - Pre-append `<repo_root>/cmake` to `CMAKE_MODULE_PATH` (neutralizes root `:41` without editing it), then `add_subdirectory(<repo_root> ${CMAKE_CURRENT_BINARY_DIR}/template_project_core)` — **no** `EXCLUDE_FROM_ALL` (install rules needed); **no** `LIB_NAMESPACE_OVERRIDE` (defaults already export `template_project::template_project`).
- [x] `ros2/template_project_interfaces/`: adapt donor package — `package.xml`, `CMakeLists.txt` with `rosidl_generate_interfaces` + the option facade (no `TEMPLATE_PROJECT_PROFILE_CORE` — no such flag in this overlay), `msg/AlgorithmStatus.msg`, `srv/RunAlgorithm.srv` copied verbatim.
- [x] `build_ros2.sh` at repo root (house conventions: `set -Eeuo pipefail`, colored `info/warn/die`; adapted from donor `build_ros2_ws.sh`):
  - Env guard: `ROS_DISTRO="${ROS_DISTRO:-jazzy}"`; source `/opt/ros/$ROS_DISTRO/setup.bash` or `die` with remediation message ("the C++ library itself builds with ./build_lib.sh and never needs ROS").
  - Flags: `--clean`, `--skip-tests`, `--debug/--release/--relwithdebinfo/--build-type <t>`, `--packages-select <pkg...>`, `--cuda`, `--optix` (implies `--cuda`), `--cmake-arg <a>`/`--colcon-arg <a>` (repeatable), `--no-version-sync`, `-h`.
  - Flow: from `ros2/`: `colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=<t> -DTEMPLATE_PROJECT_ENABLE_CUDA=<..> -DTEMPLATE_PROJECT_ENABLE_OPTIX=<..>` then `colcon test --event-handlers console_direct+` + `colcon test-result --verbose` unless `--skip-tests`.
  - **Automatic version sync before building**: if `./generate_version.sh` exists, run `./generate_version.sh --sync-ros2` (warn-and-continue on failure, e.g. no git/tags; `--no-version-sync` opts out). Keeps `ros2/*/package.xml` versions current on every overlay build with no manual step. (The `--sync-ros2` mode itself lands in Stage 3; wire the call there if implementing strictly in order.)
  - Best-effort `touch` `COLCON_IGNORE` into existing root-level `build*`/`install`/`template_subbuild` dirs.
- [x] Validate: `bash -n build_ros2.sh`; `./build_ros2.sh --clean`; grep shim `CMakeCache.txt` for `ENABLE_CUDA:BOOL=OFF` (assumption 1); config package installed at `ros2/install/template_project/lib/cmake/template_project/`; `./build_lib.sh -B build_stage1 --skip-tests` still green. Fallback if assumption 1 fails: pass core options per-package via two-phase `--packages-select` build (not expected).

## Stage 2 - Bridge (`template_project_ros`) + spinup packages

- [x] `ros2/template_project_ros/` (adapt donor nodes pkg; `_nodes` → `_ros`):
  - `package.xml`: deps `rclcpp`, `rclcpp_components`, `rclcpp_lifecycle`, `lifecycle_msgs`, `template_project` (the shim — gives colcon build order), `template_project_interfaces`; test dep `ament_cmake_gtest`.
  - **Conversions library** (node-free, executor-free — the reason it's a separate target): `include/template_project_ros/conversions.h` + `src/conversions.cpp` → SHARED target `template_project_ros_conversions`; message↔core-type helpers; links `template_project::template_project` + interfaces; **no rclcpp**.
  - **Lifecycle node**: `CTemplateLifecycleNode.{h,cpp}` → SHARED target `template_project_ros_component` (standard `rclcpp_components` terminology); gain/bias params, `~/status` lifecycle pub, `~/run_algorithm` srv, `RCLCPP_COMPONENTS_REGISTER_NODE`; delegates math/message-building to conversions. The fenced core-call adaptation seam lives in `src/conversions.cpp`.
  - `src/template_project_node_main.cpp` → executable `template_project_node`.
  - `CMakeLists.txt`: option facade; `find_package(template_project REQUIRED)`; export targets/includes/deps per donor pattern (`ament_export_targets(... HAS_LIBRARY_TARGET)`).
  - Tests: `test/test_conversions.cpp` (gtest, no `rclcpp::init` — proves executor-free) + `test/test_node_construction.cpp` (donor pattern).
- [x] `ros2/template_project_spinup/` (adapt donor spinup pkg): `package.xml` (exec_depends `launch`, `launch_ros`, `template_project_ros`); `CMakeLists.txt` installs `launch/` + `config/` to `share/`; `launch/template_project.launch.py` (standalone node + params from share config); `launch/template_project_composition.launch.py` (`ComposableNodeContainer` loading `template_project_ros::CTemplateLifecycleNode` — standard ROS composition naming); `config/template_project.yaml` (node `template_algorithm`: gain/bias).
- [x] Validate: `./build_ros2.sh --clean` (all 4 packages + gtests via colcon test); launch smoke: `source ros2/install/setup.bash && ros2 launch template_project_spinup template_project.launch.py` (then kill); standalone `./build_lib.sh -B build_stage2 --skip-tests` + tailoring test still green.

### Δ Stage 2 review deltas (2026-07-07, confirmed in review — debug & fix pass)

- [x] **EDIT-ME fence lives in the wrong file**: the actual core coupling (`#include "wrapped_impl/CWrapperPlaceholder.h"` + `cpp_playground::CWrapperPlaceholder::multiplyBy2()`) is in `ros2/template_project_ros/src/conversions.cpp` (`EvaluateTemplateCore`), while the fenced `EDIT ME` block is only in `CTemplateLifecycleNode.cpp:66-68` around the call to `EvaluateTemplateCore`. A derived-repo user who edits only the node file still fails to compile. Fix: add the fenced `// --- template-core call site (EDIT ME ...) ---` block around the include + `EvaluateTemplateCore` body in `conversions.cpp` (that is the intended adaptation seam); keep the node fence as a cross-reference or drop it. Extend `VerifyTemplateProjectRos2Overlay.cmake` to assert the fence exists in `conversions.cpp` so the pointer cannot rot.
- [x] **Explain the compat include**: the PRIVATE `${TEMPLATE_PROJECT_REPOSITORY_ROOT}/src` include on `template_project_ros_conversions` and `template_project_ros_component` (`ros2/template_project_ros/CMakeLists.txt:32-33,49-50`) bypasses the exported include interface; it exists as compat for older derived repos whose installed header layout differs. Add a short *why* comment so it is not mistaken for the intended consumption path (the exported `template_project::template_project` interface remains primary).
- [x] Validate: `./build_ros2.sh --clean` still green; `ctest -R ros2_overlay` green after the verifier extension.

## Stage 3 - Version sync + template validation tests

- [x] `generate_version.sh`: add `--sync-ros2` (default behavior unchanged) — after writing `VERSION`, if `ros2/` exists rewrite the first `<version>` line of every `ros2/*/package.xml` to the strict `X.Y.Z` core version (grep-guarded awk; warn-and-skip per file if no tag); run once to set all packages. No-ops when `ros2/` is absent (safe in tailored repos).
- [x] **Automatic sync at build time**: wire `build_ros2.sh` (Stage 1) to invoke `./generate_version.sh --sync-ros2` before `colcon build` — warn-and-continue if the script is missing or git metadata is unavailable (fallback chain mirrors `generate_version.sh`: git describe → existing `VERSION` file → skip with warning); `--no-version-sync` opts out. Rationale for not syncing from CMake configure: source-tree writes during configure are an opt-in side effect in this repo (`WRITE_SOURCE_VERSION_FILE` pattern, guarded by the existing version-side-effects template-dev test); the build script is the right owner. The copied `build_ros2.sh` in derived repos keeps the same guarded call, so rollout targets get automatic sync too when they ship `generate_version.sh`.
- [x] `tests/cmake/VerifyTemplateProjectRos2Overlay.cmake` (template-dev, static, **no ROS required**), registered in `tests/CMakeLists.txt` template-dev section (above the `# Exclude EXCLUDED_LIST` marker) with `-DEXPECTED_VERSION=${PROJECT_VERSION_CORE}`, labels `ros2;template`:
  - Overlay files + `build_ros2.sh` exist; `bash -n build_ros2.sh` (extend to `add_ros2_support.sh` when it lands in Stage 5); 4 COLCON_IGNORE markers exist.
  - Each `ros2/*/package.xml` `<version>` equals `EXPECTED_VERSION`.
  - Shim contains the `CMAKE_MODULE_PATH` pre-set and `ENABLE_TESTS OFF` lines (encapsulation contract) and never references `python/`.
  - (Stages 5/6/7 extend it: `add_ros2_support.sh` behavior fixture, workflow existence, doc fences.)
- [x] `tests/template_test/testRos2OverlayStatic.py` (pytest, survives tailoring, auto-registered by `add_tests()`; skips everything if `ros2/` absent): package.xml versions mutually equal (+ equal to `VERSION` core when the gitignored file exists); markers present; `python/` tree free of `rclcpp|ament|rosidl` references; `build_ros2.sh` contains the env-guard `die` path.
- [x] Validate: `./generate_version.sh --sync-ros2`; `./build_lib.sh -B build_stage3`; `ctest --test-dir build_stage3 -R "ros2" --output-on-failure`.

## Stage 4 - Tailoring: overlay kept by default, `--remove-ros2`

- [x] `tailor_template_cleanup.sh`:
  - Append `tests/cmake/VerifyTemplateProjectRos2Overlay.cmake` to `template_development_paths`.
  - New `--remove-ros2` flag with removal set: `ros2/`, `build_ros2.sh`, `add_ros2_support.sh`, the 4 COLCON_IGNORE markers, `.github/workflows/build_ros2_overlay.yml`, `doc/ros2_overlay.md`, `tests/template_test/testRos2OverlayStatic.py`.
  - Grep-guarded patchers strip `<!-- ros2-overlay-begin --> ... <!-- ros2-overlay-end -->` fenced blocks (added in Stage 7) from `README.md`, `AGENTS.md`, `CLAUDE.md`, `doc/bootstrap_prompts.md`, `doc/template_usage.md`. Leave `generate_version.sh` untouched (`--sync-ros2` already no-ops without `ros2/`).
  - Update `--list` output: "ROS 2 overlay KEPT by default; pass --remove-ros2 to strip it".
- [x] Extend `tests/cmake/VerifyTemplateProjectTailoringScript.cmake` fixture: minimal `ros2/` tree + markers + fenced doc lines; assert default `--apply` keeps the overlay, `--apply --remove-ros2` removes all paths and strips fences, `--list` mentions the policy.
- [x] Validate: `ctest -R tailoring`; end-to-end on a scratch copy: `--apply --yes --remove-ros2` then `./build_lib.sh --skip-tests` green.

## Stage 5 - `add_ros2_support.sh` (zero-regression rollout to derived repos)

- [x] Create `add_ros2_support.sh` at repo root (house conventions; `--list` dry-run default, `--apply [--yes]`, `--root <dir>`, `--verify`, `--no-ci`):
  - Source = the template checkout containing the script (must contain `ros2/` + `build_ros2.sh`; `die` otherwise). Target = `--root`.
  - Preconditions: target has `set(project_name "<name>")` (scraped like `build_lib.sh` does); name matches `[a-z][a-z0-9_]*`; **refuse** if `<target>/ros2` or `<target>/build_ros2.sh` exists.
  - **Purely additive**: copies `ros2/`, `build_ros2.sh`, `doc/ros2_overlay.md`, `.github/workflows/build_ros2_overlay.yml` (skipped with `--no-ci`), and COLCON_IGNORE markers only into target dirs that exist and lack them. Copy-if-exists for artifacts landing in later stages (`doc/ros2_overlay.md` is Stage 7, the workflow is Stage 6) so this stage stays independently executable and testable. **Never edits an existing target file** — the shim's `CMAKE_MODULE_PATH` pre-set is what makes root-CMake patching unnecessary in derived repos. Consequence to document: an overlaid derived repo gets the new files but no fenced doc sections in its own README/AGENTS (the script never edits existing docs) — the post-apply checklist points users at `doc/ros2_overlay.md` instead.
  - Rename pass on the copied tree only: `template_project` → `<name>` in contents and file/dir names, word-boundary-aware (refuse names that would collide). Print a post-apply checklist: (1) adapt the fenced core call in `ros2/<name>_ros/src/conversions.cpp` to a real API of the target library, (2) `./build_ros2.sh --clean`, (3) `./generate_version.sh --sync-ros2` (if the target ships it).
  - `--verify`: configure-only `cmake -S <target> -B <mktemp scratch> -DENABLE_TESTS=OFF`; `die` on failure — proves the standalone build is untouched.
  - Never copies `add_ros2_support.sh` itself or template-dev Verify scripts into targets.
- [x] Extend `VerifyTemplateProjectRos2Overlay.cmake`: fixture-run `--list` against a fake minimal target; assert refusal when `ros2/` pre-exists, correct name detection, and word-boundary rename safety (fixture name like `my_template_project_x`).
- [x] Validate: `ctest -R ros2_overlay`; local validation: scratch copy → `tailor --apply --yes --remove-ros2` → `./add_ros2_support.sh --root <copy> --apply --yes --verify` → `(cd <copy> && ./build_ros2.sh --clean)` (placeholder API survives tailoring, so this must build).

### Δ Stage 5 review deltas (2026-07-07, confirmed in review — debug & fix pass)

- [x] **Latent `set -e` abort in the rename pass**: `add_ros2_support.sh` `replace_placeholder_in_file()` uses `grep -Iq . "${file_path_}" || return`, which propagates grep's exit 1 for empty/binary files and kills the whole script under `set -Eeuo pipefail` (empirically confirmed). Unreachable today (no empty/binary files in the copied set — the empty COLCON_IGNORE markers are not in `copied_roots`), but one new empty file in `ros2/` would break rollout silently. Fix: `|| return 0`.
- [x] **Post-apply checklist points at the wrong file** (same root cause as the Stage 2 delta): step 1 of `print_post_apply_checklist` names `ros2/<name>_ros/src/CTemplateLifecycleNode.cpp`, but the include + core symbol to adapt live in `ros2/<name>_ros/src/conversions.cpp`. Fix the checklist (and `--list` output if it repeats it) to name `conversions.cpp` (primary) and the node file (secondary).
- [x] **Readability**: `target_has_conflicts()` returns success when there are *no* conflicts — inverted semantics vs its name (functionally correct at both call sites). Rename (e.g. `target_is_clean`) or invert the return convention, and adjust callers.
- [x] **Scratch hygiene**: `run_verify()` leaves its `mktemp -d` scratch behind; add a `trap ... EXIT` cleanup (or explicit `rm -rf` on both success and `die` paths).
- [x] Validate: `bash -n add_ros2_support.sh`; `ctest -R ros2_overlay`; rerun the local validation loop (strip → re-add → `./build_ros2.sh --clean` → standalone build) green; add an empty file to a scratch copy's `ros2/` tree and confirm rollout survives it.

## Stage 6 - CI: `.github/workflows/build_ros2_overlay.yml`

- [x] Create workflow (adapted from donor `build_ros2.yml`; Jazzy-only):
  - Triggers: push/PR path-filtered to `ros2/**`, `build_ros2.sh`, `add_ros2_support.sh`, `tailor_template_cleanup.sh`, the workflow itself + `workflow_dispatch` (core `src/**` deliberately not a trigger — `build_linux.yml` covers the library; dispatch covers on-demand cross-checks).
  - Job `overlay-build` (`ubuntu-24.04`, container `ros:jazzy`): checkout `fetch-depth: 0`; apt `build-essential cmake libeigen3-dev python3-colcon-common-extensions python3-pytest ros-dev-tools`; `rosdep update && rosdep install --from-paths ros2 -i -r -y --rosdistro jazzy`; `./build_ros2.sh --clean`; `python3 -m pytest -q tests/template_test/testRos2OverlayStatic.py`.
  - Job `rollout-rehearsal` (same container — the no-regression proof): copy checkout → `tailor_template_cleanup.sh --apply --yes --remove-ros2` in the copy → `./add_ros2_support.sh --root <copy> --apply --yes --verify` from the original → `(cd <copy> && ./build_ros2.sh --clean)` → plain `cmake -S . -B build_plain -DENABLE_TESTS=OFF && cmake --build build_plain -j2` in the copy (standalone build of an overlaid derived repo).
- [x] CUDA+ROS: no CI job (self-hosted GPU runner lacks ROS); document `./build_ros2.sh --cuda [--optix]` locally / in the ROS devcontainer (`./configure_devcontainer.sh --ros2 jazzy` already exists) in `doc/ros2_overlay.md`.
- [x] Leave `build_linux.yml`, `build_linux_cuda.yml`, `docs_pages.yml` untouched.
- [x] Validate: rehearse both jobs locally in `docker run --rm -v "$PWD":/ws -w /ws ros:jazzy ...`; YAML sanity check.

### Δ Stage 6 review deltas (2026-07-07, confirmed in review — debug & fix pass)

- [x] **Missing trigger path**: `generate_version.sh` is coupled to the overlay build (`build_ros2.sh` invokes `--sync-ros2` before every colcon build) but is absent from both `push.paths` and `pull_request.paths` in `.github/workflows/build_ros2_overlay.yml`. A regression in the sync logic would not trigger overlay CI. Fix: add `generate_version.sh` to both path filters.
- [x] Validate: YAML sanity parse; confirm path lists in both trigger blocks match.

## Stage 7 - Documentation + agent-facing rollout instructions

- [x] `doc/ros2_overlay.md`: architecture (encapsulation contract; shim `add_subdirectory` + exported config package; conversions-vs-node split rationale); `build_ros2.sh` usage + option-flow table (`--cuda` → `-DTEMPLATE_PROJECT_ENABLE_CUDA=ON` → shim → `ENABLE_CUDA`); COLCON_IGNORE policy + parent-workspace caveat; version sync (automatic at build + manual `--sync-ros2`); rollout via `add_ros2_support.sh` (incl. EDIT-ME core-call step and both supported orders: rename-then-overlay vs overlay-then-rename); removal via `--remove-ros2`; Jazzy CI / distro-agnostic scripts; CUDA+ROS local-only; explicit "python/ bindings remain a separate ROS-free optional feature".
- [x] `doc/bootstrap_prompts.md`: "ROS 2 Overlay Rollout Prompt" section (agent-driven rollout: asks keep/remove, script vs manual, node/topic names, distro), inside `<!-- ros2-overlay-begin/end -->` fences.
- [x] `doc/template_usage.md`: extend rename map (`template_project_ros`, `_interfaces`, `_spinup`, shim dir) + tailoring `--remove-ros2` note, fenced.
- [x] `README.md`, `AGENTS.md`, `CLAUDE.md`: short fenced sections pointing at `doc/ros2_overlay.md` and the two entry points (`build_lib.sh` = C++-first, never needs ROS; `build_ros2.sh` = optional overlay).
- [x] Extend the ros2 Verify script (or `VerifyTemplateProjectDocsStatic.cmake` if a better fit) to assert fences exist in all five docs so the `--remove-ros2` patcher can never silently miss.
- [x] Validate: `ctest -L "ros2|docs"`; rerun the Stage 4 scratch `--remove-ros2` rehearsal and grep that no ros2 references remain in README/AGENTS/CLAUDE.

### Δ Stage 7 review deltas (2026-07-07, confirmed in review — debug & fix pass)

- [x] **Rollout doc names the wrong EDIT-ME file** (same root cause as the Stage 2/5 deltas): `doc/ros2_overlay.md` "Rollout to derived repositories" instructs completing the EDIT-ME step in `ros2/<project_name>_ros/src/CTemplateLifecycleNode.cpp`; update it to name `conversions.cpp` as the primary adaptation seam (include + `EvaluateTemplateCore` body), with the node fence as secondary. Keep wording consistent with the fixed `add_ros2_support.sh` checklist.
- [x] Validate: `ctest -L "ros2|docs"` green (the extended verifier from the Stage 2 delta guards the fence location).

## Stage 8 - Testfield mirroring (`../cpp_cuda_template_testfield`)

- [x] Run `./add_ros2_support.sh --root ../cpp_cuda_template_testfield --apply --yes --verify` (script detects the testfield's project name and renames the overlay).
- [x] Adapt the single EDIT-ME core-call block to a real testfield API symbol — intended validation of the derived-repo experience; record friction in `doc/developments/`.
- [x] Add the testfield's `build_ros2_overlay.yml` following its submodule/`TEMPLATE_PROJECT_SOURCE_DIR` checkout convention (mirror its `build_linux.yml` checkout steps).
- [x] Validate: `(cd ../cpp_cuda_template_testfield && ./build_ros2.sh --clean)`; its plain `./build_lib.sh` still green.

Stage 8 friction note, 2026-07-07:

- Testfield rollout exposed two additive-overlay compatibility gaps before the ROS build passed:
  - the testfield's older `write_build_VERSION_file()` writes `VERSION` to `CMAKE_BINARY_DIR`, while its root install rule expects `${PROJECT_BINARY_DIR}/VERSION` under the nested shim build;
  - a derived-repo edit to the fenced lifecycle-node core-call block may include target source headers, so the component target needs the same private source include path as the conversions target plus a direct core target link.
- The source overlay now handles both cases without editing target root CMake files. Helper variable names were kept neutral so `add_ros2_support.sh` does not leave `template_project` placeholders in renamed derived overlays.
- The Stage 2 review-delta seam relocation required one testfield follow-up: the adapted `multiplyBy2` call now lives in the fenced `ros2/template_project_ros/src/conversions.cpp` seam, and `CTemplateLifecycleNode.cpp` delegates to `EvaluateTemplateCore` so it remains ROS-only.
- The testfield overlay workflow follows the testfield's sibling-template checkout convention (`TEMPLATE_PROJECT_SOURCE_DIR`) and includes `generate_version.sh` in both path filters.
- The external blockers were cleared before closure: the conda `test_env` now has `pytest`, the remaining tracked pybind11 conflict markers were removed from both `../wrap` and the testfield `lib/wrap` copy, and the clean testfield plain gate now passes.

## Stage 9 - Final audit + minimization/readability pass

- [x] **Precondition**: all `Δ` review-delta items in Stages 2/5/6/7 are closed (debug & fix pass) before the final gate.
- [x] Audit: `rg -n "template_project_nodes|bringup|PROFILE_CORE|dummy" ros2/ doc/ --glob '!doc/developments/ros2_overlay_upgrade_plan.md'` → no donor leftovers or banned naming outside this source-of-truth plan; `git diff --stat CMakeLists.txt src/ python/` empty (root build + python untouched); `shellcheck` both new scripts.
- [x] Minimization/readability pass over new bash + CMake + C++ + docs: comments state the *why* (e.g. shim's module-path pre-set), node stays donor-sized; simplification cleanup over the full diff, then a correctness review pass.
- [x] Update `CONTEXT.md` (per AGENTS.md convention) with the overlay architecture + decisions; tick all plan checkboxes in this file.
- [x] Final gate, in order: `./build_lib.sh --clean` + full `ctest`; `./build_ros2.sh --clean`; scratch tailoring both ways (with and without `--remove-ros2`) + standalone build; docker `ros:jazzy` rehearsal of both CI jobs.

## Risks & mitigations

- **spdlog fetch under colcon** (root fetch runs regardless of main-project status): CI container has network; keep default parity; note `ENABLE_FETCH_SPDLOG OFF` as hermeticity knob in docs.
- **`--symlink-install` vs `install(FILES VERSION DESTINATION ".")`**: verify in Stage 1; drop `--symlink-install` default if it conflicts.
- **Rename collisions** in `add_ros2_support.sh` (names containing `template_project` as substring): word-boundary-aware replacement + fixture test.
- **rosdep key `eigen`**: correct for Jazzy (donor CI resolves it); `-r` keeps CI resilient.

## Next fix to evaluate

- [x] Extend derived-repo ROS2 rollout tailoring for projects whose CMake package name is not a valid ROS package name, e.g. `space-nav-frontend` -> ROS package prefix `space_nav_frontend`, while keeping core CMake `find_package()`/target links pointed at the original CMake package. Document the manual tailoring path for derived repos that intentionally remove optional template features such as OptiX/spdlog so copied `build_ros2.sh`, shim CMake, docs, and CI do not advertise unsupported options.

## 2026-07-16 post-review hardening

### Stage A - Operational lifecycle launch paths

- [x] Replace the standalone `Node` action with `LifecycleNode(..., autostart=True)` and keep the original complete `Node(...)` form commented beside it as the externally managed template alternative.
- [x] Replace the composition `ComposableNode` description with `ComposableLifecycleNode(..., autostart=True)` and keep the original complete `ComposableNode(...)` form commented beside it as the externally managed template alternative.
- [x] Preserve native composed autostart semantics on Jazzy with a fully qualified lifecycle-event identity until the upstream `launch_ros` name-resolution fix is available.
- [x] Add one parameterized `launch_testing` integration test that proves both supplied launch files reach `PRIMARY_STATE_ACTIVE` and return `output=14.0`, `status="ok"` for `run_algorithm(3.0)`.
- [x] Declare the spinup package's direct runtime and test dependencies and guard the launch contract in `VerifyTemplateProjectRos2Overlay.cmake`.

### Stage B - Additive rollout atomicity

- [x] Extend the static rollout fixture so existing `doc/ros2_overlay.md` and CI workflow destinations fail before any overlay path is copied.
- [x] Preflight every deterministic required or optional destination in `target_is_clean()` while preserving existing `COLCON_IGNORE` markers.

### Stage C - Version and CI metadata integrity

- [x] Extend the copied-manifest sync test to verify mode bits are unchanged by `generate_version.sh --sync-ros2`, then preserve the source mode on replacement.
- [x] Trigger overlay CI for every owned agent/doc/marker input and execute `VerifyTemplateProjectRos2Overlay.cmake` directly with a manifest-derived expected version.

### Stage D - Mirroring and closure

- [x] Document supplied-launch autostart versus externally managed lifecycle startup in `doc/ros2_overlay.md`.
- [x] Mirror and red-green test the operational changes in `cpp_cuda_template_testfield` without changing its adapted `conversions.cpp` seam.
- [x] Run the full standalone, ROS, validation, lint, parse, invariant, and Docker gates; append evidence to `doc/developments/ROS2_OVERLAY_STAGE_OUTPUTS.md`.
- [x] Perform an extensive correctness and design review, then create only the two approved testfield commits without pushing.

## 2026-07-16 project metadata flowdown

### Stage M1 - Root project metadata contract

- [x] Define the project description and homepage through standard CMake `project(DESCRIPTION ... HOMEPAGE_URL ...)` fields, plus explicit cache-backed maintainer name, maintainer email, and SPDX-style license fields.
- [x] Add an opt-in metadata-only configure mode that resolves the project/version with `LANGUAGES NONE` and returns before dependencies, targets, wrappers, tests, docs, or packaging are configured.
- [x] Reuse the same fields for CPack description, homepage, vendor, and contact metadata.

### Stage M2 - Structured ROS manifest synchronization

- [x] Extend the static pytest and CMake verifier first so missing root metadata, missing helper integration, stale manifest metadata, lost XML processing instructions, or changed manifest modes fail red.
- [x] Add a typed Python helper under `ros2/tools/` that reads the metadata-only CMake cache and updates immediate `ros2/*/package.xml` files with `xml.etree.ElementTree` while preserving package names, non-website URLs, unrelated dependencies, and file modes.
- [x] Expand `generate_version.sh --sync-ros2` to synchronize version, description, maintainer, license, and website metadata through the helper; preserve the existing no-overlay and version-fallback behavior.
- [x] Synchronize and commit all four template manifests from the root metadata contract.

### Stage M3 - Rollout, CI, and documentation contract

- [x] Keep package identity mapping at rollout time: `add_ros2_support.sh` derives the ROS-valid prefix from root `set(project_name ...)` or `--ros-prefix`, while recurring metadata synchronization preserves those established ROS package names.
- [x] Run guarded metadata synchronization in the ROS workflow after dependencies are installed and before `rosdep install`; keep copied workflows compatible with missing or older `generate_version.sh` helpers.
- [x] Update `build_ros2.sh`, rollout checklists, `doc/ros2_overlay.md`, and `doc/template_usage.md` to describe the one-time name mapping versus recurring metadata flowdown.

### Stage M4 - Validation and review

- [x] Run the metadata-only configure probe, direct helper/pytest/CMake verifiers, full standalone tests, clean ROS tests, and scratch remove/re-add validation with non-default metadata and a non-ROS CMake name.
- [x] Run shell lint, YAML/XML/Python parsing, manifest mode checks, conflict scan, and confirm `src/` and `python/` remain untouched; the only root CMake change must be the approved metadata contract.
- [x] Perform a final correctness/design review, tick this section, and append validation evidence to `doc/developments/ROS2_OVERLAY_STAGE_OUTPUTS.md` and `CONTEXT.md` without staging those bookkeeping files.

## 2026-07-17 implementation-review remediation

The source of truth for this follow-up is
`doc/developments/ros2_overlay_review_remediation_plan.md`. Work proceeds one
approved stage at a time, test-first where behavior changes, with command and
result evidence appended to `doc/developments/ROS2_OVERLAY_STAGE_OUTPUTS.md` before staging each
stage for review.

### Stage R0 - Baseline and narrow invariant exception

- [x] Capture a clean baseline on `feature/ros2-overlay` at `fc5478a`, including
  standalone, ROS 2, focused rollout/tailoring, pytest, and shell-lint gates.
- [x] Record the compiler, CMake, ROS, CUDA, GPU, and OptiX availability used by
  the remediation pass.
- [x] Preserve the overlay architecture and the ROS-free standalone build
  boundary.
- [x] Authorize the following exception to the former frozen-`src/` rollout
  invariant. It is triggered by empirical installed-consumer and CUDA
  compile-graph evidence, not by an architecture change:
  - Stage 1 may replace `CMAKE_SOURCE_DIR` with `PROJECT_SOURCE_DIR` only in
    `src/wrapped_impl/CMakeLists.txt`, `src/utils/CMakeLists.txt`,
    `src/utils/logging/CMakeLists.txt`,
    `src/utils/wrap_adapters/CMakeLists.txt`,
    `src/template_src/CMakeLists.txt`, and
    `src/template_src_kernels/CMakeLists.txt`.
  - Stage 2 may correct ordinary CUDA source discovery only in the same six
    module files plus `src/CMakeLists.txt`, including explicit exclusion of
    dedicated `*.ptx.cu` inputs from the normal library source list.
  - No C++, CUDA, or Python implementation/API change is authorized by this
    exception. Root `CMakeLists.txt` remains outside the expected change set.
- [x] Require each later stage to record the expected red guard before its fix,
  the matching green result afterward, and the final regression gates in
  `doc/developments/ROS2_OVERLAY_STAGE_OUTPUTS.md`.

### Stage R1 - Nested header installation and installed consumption

- [x] Add a nested `add_subdirectory()` install regression that rejects
  install destinations escaping `include/template_project`, checks all public
  header families including the optional logging module, and builds an
  installed-only consumer through `template_project::template_project`.
- [x] Record expected red failures for the generated nested install path and
  the six-file static `CMAKE_SOURCE_DIR` guard before applying the fix.
- [x] Derive module install paths from `PROJECT_SOURCE_DIR` in the six approved
  `src/**/CMakeLists.txt` files, with no C++/CUDA API or implementation change.
- [x] Remove the obsolete repository-source include from the lifecycle
  component while retaining and accurately documenting the conversions-only
  additive-rollout compatibility path.
- [x] Make the ROS workflow assert the real colcon-installed header layout and
  reject the former package-prefix-root leak.
- [x] Pass the fresh standalone `25/25` CTest suite, clean ROS `10/10` suite,
  installed target inspection, and an installed-only colcon consumer probe.

### Stage R1A - Template-validation and tailored-project CI ownership

- [x] Keep the four runnable workflows template-owned and add four dormant,
  generic `.yml.tpl` project workflows with explicit ownership markers.
- [x] Materialize generic workflows atomically during cleanup, preserve modes,
  support idempotent reapplication, and remove both ROS workflow forms with
  `--remove-ros2`.
- [x] Make additive ROS rollout validate and materialize only the dormant
  generic workflow, never the active template-validation workflow.
- [x] Add auto-discovered YAML/ownership tests plus byte-equivalent tailoring
  and additive-rollout fixtures so dormant templates cannot rot silently.
- [x] Validate materialized CPU/docs, CUDA, and ROS project paths from active
  template CI, using full-history scratch clones where version provenance is
  required.
- [x] Pass standalone `26/26`, clean ROS `10/10`, tailored CUDA `9/9`, normal
  tailoring/docs, remove/re-add rollout, YAML, shell-lint, conflict, and frozen
  surface gates; record red-green evidence in
  `doc/developments/ROS2_OVERLAY_STAGE_OUTPUTS.md`.

### Stage R2 - Truthful CUDA and relocatable OptiX package evidence

- [x] Replace the malformed ordinary-source glob in the seven authorized
  `src/**/CMakeLists.txt` sites, explicitly excluding `*.ptx.cu` while
  preserving the dedicated PTX handoff.
- [x] Add a CUDA-only compile-database regression that requires the real
  `placeholder.cu` project source and rejects the OptiX PTX input as an
  ordinary translation unit.
- [x] Record the expected compile-graph red result, then pass standalone CUDA
  `29/29` and ROS CUDA `10/10` with real project objects for `sm_120`.
- [x] During the required OptiX probe, reproduce and fix the installed-target
  export that pointed at a non-existent package-local SDK include directory.
- [x] Resolve OptiX headers from consumer-provided CMake/environment inputs,
  keep machine-local CUDA/OptiX paths out of installed exports, and prove the
  contract with an OptiX install-consumer regression.
- [x] Pass standalone CUDA+OptiX `30/30` and ROS CUDA+OptiX `10/10`, including
  the real CUDA object plus generated PTX and embedded-object artifacts.
- [x] Remove the CUDA and OptiX template-only verifiers during tailoring and
  cover both their advertised and applied cleanup behavior.
- [x] Keep the real-source compile-graph assertion in active template CUDA CI
  after it materializes the dormant generic workflow; leave that dormant
  workflow free of template-specific source names.

## 2026-07-18 release/version-sync extension

This final extension closes the cross-repository release and metadata gaps
without changing the C++/CUDA/ROS API surface or broadening CI topology.

### Stage R3 - Atomic project tailoring

- [x] Preflight every removable ROS documentation fence before confirmation or
  mutation.
- [x] Replace root/test CMake and documentation targets atomically while
  preserving their original modes.
- [x] Prove all malformed-fence failures preserve the complete input tree and
  do not materialize workflows or partially remove development content.

### Stage R4 - Testfield-owned metadata

- [x] Centralize testfield description, homepage, maintainer/contact, license,
  prerelease-aware version, and CPack fields in its root CMake contract.
- [x] Add metadata-only configuration and a typed, mode-preserving full ROS
  manifest synchronizer with explicit capability detection.
- [x] Preserve manifest identities/dependencies and the adapted testfield core
  seam while validating idempotence, invalid-input rejection, native CTest, and
  clean ROS behavior.

### Stage R5 - Canonical source releases

- [x] Define CPack source TGZ, not GitHub's automatic source links, as the
  canonical release artifact in both repositories.
- [x] Extend synthetic-tag tests through archive generation and no-Git
  extraction/consumption, including strict core/full version equivalence and a
  missing-`VERSION` negative case.
- [x] Keep artifact upload automation deferred to a later CI upgrade.

### Stage R6 - Narrow CI drift gates

- [x] Run native and ROS workflows for `v*.*.*` pushes while retaining branch
  path filters.
- [x] Reject tracked manifest changes after supported metadata synchronization;
  retain the compatibility warning only in the generic derived-project
  workflow.
- [x] Derive active-template expected version from generated `VERSION` and run
  repository-owned documentation static verifiers directly.
- [x] Guard active/generic separation with semantic workflow tests and pass
  active-template, tailored-generic, and testfield rehearsals in `ros:jazzy`.

### Stage R7 - Full closeout and handoff

- [x] Pass main native, CPU ROS, CUDA+OptiX, ROS CUDA+OptiX, documentation, and
  both real tailoring outcomes.
- [x] Pass testfield native, ROS, metadata-idempotence, canonical-archive, and
  documentation gates without changing `conversions.cpp` or `lib/wrap`.
- [x] Reconstruct and verify the proposed five-main/three-testfield commit split
  in disposable clones; leave both source repositories uncommitted.
- [x] Inventory every build and temporary artifact created by the extension so
  the final post-verification cleanup removes only those paths.
