# ROS 2 Overlay Implementation Review

- **Date:** 2026-07-16
- **Scope:** review of the `feature/ros2-overlay` working tree against `doc/developments/ros2_overlay_upgrade_plan.md` (Stages 0-9 + review deltas + the "Next fix" follow-up).
- **Method:** every plan checkbox was cross-checked against the actual files; the static verifier (`tests/cmake/VerifyTemplateProjectRos2Overlay.cmake`), the pytest (`tests/template_test/testRos2OverlayStatic.py`), and `shellcheck` were re-executed during this review; the colcon install tree produced by the last `./build_ros2.sh` run was inspected; the rollout script was exercised via the verifier's fixture targets and its outputs examined on disk.
- **Housekeeping status:** remediation Stage 5 excludes `doc/reports/` from Doxygen and removes it during project tailoring, so this audit remains internal to template development.

---

## 1. Summary

### Verdict

The implementation is a faithful, high-discipline execution of an unusually good plan. Every stage checkbox corresponds to real, working artifacts; the review-delta subsections (Stages 2/5/6/7) were genuinely closed, not just ticked; and the static test layer is strong enough that most of the plan's contracts are machine-enforced rather than merely documented. The verifier, pytest, and shellcheck all pass in the current tree.

However, the review found **one substantive latent defect that the plan's "verified facts" analysis missed** (nested header-install layout, Major M1), **one hygiene defect in the rollout path that the test layer is structurally blind to** (binary `__pycache__` leakage, Major M2), **one predictable future CI breakage** (release-tag version coupling, Major M3), and **one advertised-but-never-executed feature path** (CUDA/OptiX facade, Major M4). None of these invalidates the design; M1 in particular is a pre-existing core-template bug that the overlay *surfaced* and then papered over with a workaround whose comment misstates its purpose.

### Confidence scores

Scores are 0-10. "Confidence" states how sure this review is of each judgement, based on what was executed vs. only read.

| Area | Score | Confidence | Basis |
|---|---|---|---|
| Plan quality (design decisions, staging, risk analysis) | 9/10 | High | Full read of plan + donor context; the misses are enumerated in §2/§3 |
| Implementation fidelity to plan (are ticked items real?) | 9.5/10 | High | Every Stage 0-9 item traced to files; verifier + pytest re-run green; only one promised item missing (spdlog doc note, m1) |
| Outcome robustness (will it hold up in derived repos / CI / releases?) | 7/10 | High for M1-M2 (empirically confirmed), Medium-high for M3-M4 (logic-verified, not executed) | See Major findings |
| Test/verification layer quality | 8.5/10 | High | Exceptional breadth (fixture-runs the rollout script); one structural blind spot (M2) and one coverage hole (m5) |
| Documentation quality | 8/10 | High | Complete and accurate except the items in m1, m7, m10 |

### What is genuinely good (and should not be "fixed")

These design points are correct and worth defending against future simplification passes:

- **The colcon-only enablement with no root `package.xml` and no `ENABLE_ROS2` option.** The shim (`ros2/template_project/CMakeLists.txt`) is small, fully commented, and the `CMAKE_MODULE_PATH` PREPEND correctly neutralizes the root's pre-`project()` `list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")` (root `CMakeLists.txt:41`) without touching the root file. Verified against the actual root CMakeLists.
- **The conversions-vs-node split.** `template_project_ros_conversions` has zero `rclcpp` surface (confirmed by reading both files), and `test_conversions.cpp` proves the executor-free property. The EDIT-ME seam relocation into `conversions.cpp` (Stage 2 delta) is complete and consistent across code, script checklist, and all three rollout docs — the verifier enforces all of it.
- **The verifier's fixture-driven testing of `add_ros2_support.sh`.** Running the rollout script against five fake targets (list, conflict, apply, word-boundary, split-name, prefix-override, invalid-prefix) inside a plain `ctest` run, with no ROS installed, is the strongest part of the test layer. This is what made the split-name feature ("Next fix" item) verifiable.
- **The CMake-name / ROS-prefix split** (`--ros-prefix`, `derive_default_ros_prefix`, `restore_core_cmake_references_in_file`). The three-regex restore pass (find_package, `::` namespace, `ament_export_dependencies` entry) was checked against the actual bridge CMakeLists content and the fixture output; it correctly leaves `<depend>` entries in `package.xml` on the ROS-valid name while restoring hyphenated CMake names in CMake files.
- **The keep-by-default tailoring policy with fenced doc blocks** and the malformed-fence `die` in the awk stripper — this fails loudly instead of silently half-stripping.

---

## 2. Major findings

Ordered by severity. Each includes evidence, rationale, and suggested action.

### M1 — Nested builds install core headers to the wrong location; the overlay's "compat" include path is actually load-bearing

**Severity: Major (latent consumer break + misleading code comment). Confidence: High — confirmed empirically in the existing install tree.**

**Evidence.** All four source subdirectory CMake files compute their header install destination from `CMAKE_SOURCE_DIR`:

```cmake
# src/wrapped_impl/CMakeLists.txt:5 (same pattern in src/utils, src/template_src, src/template_src_kernels)
file(RELATIVE_PATH relative_path_from_root ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
string(REPLACE "src/" "" relative_path_without_src ${relative_path_from_root})
install(FILES ${installable_headers} DESTINATION include/${project_name}/${relative_path_without_src})
```

In a standalone build, `CMAKE_SOURCE_DIR` is the repo root, so the destination is the intended `include/template_project/wrapped_impl`. Under the colcon shim, `CMAKE_SOURCE_DIR` is `ros2/template_project`, so the relative path is `../../src/wrapped_impl`, the `src/` strip yields `../../wrapped_impl`, and the destination becomes `include/template_project/../../wrapped_impl` — i.e. the **install prefix root**. The current install tree confirms it:

```
ros2/install/template_project/wrapped_impl/CWrapperPlaceholder.h     <-- wrong
ros2/install/template_project/template_src/placeholder.h             <-- wrong
ros2/install/template_project/utils/logging/SpdlogUtils.h            <-- wrong
ros2/install/template_project/include/template_project/config.h      <-- correct (fixed-destination rule)
```

The exported target's `INTERFACE_INCLUDE_DIRECTORIES` is `${_IMPORT_PREFIX}/include;${_IMPORT_PREFIX}/include/template_project` (verified in the installed `template_projectTarget.cmake`). Therefore `#include "wrapped_impl/CWrapperPlaceholder.h"` — the include that `conversions.cpp` itself uses — is **not resolvable through `template_project::template_project`** in the colcon install. The bridge compiles only because of this private include:

```cmake
# ros2/template_project_ros/CMakeLists.txt:32-35
  PRIVATE
    # Compatibility for older derived repo overlays whose adapted conversions.cpp
    # includes target source headers outside the exported install interface.
    ${TEMPLATE_PROJECT_REPOSITORY_ROOT}/src
```

The comment (and the Stage 2/8 delta notes that mandated it) says this is *compatibility for older derived repos*. In reality it is the only reason the template's own overlay builds. The plan's headline consumer contract — "downstream ament packages `find_package(template_project REQUIRED)` and it just works" (plan lines 20, and `doc/ros2_overlay.md` §Encapsulation contract) — holds for **linking** but is broken for **header consumption** by any third package in a larger workspace that consumes the shim's install without also adding a source-tree include hack.

**Why the plan missed it.** The "Verified facts" section correctly identified the `CMAKE_MODULE_PATH`-before-`project()` wrinkle (root `CMakeLists.txt:41`) but did not sweep for other `CMAKE_SOURCE_DIR` dependencies in the `src/` tree. The subsequent hard invariant — "`git diff --stat CMakeLists.txt src/ python/` empty" enforced at every stage gate — then made the correct fix impossible within the plan's own rules, which is exactly why Stage 8 accreted the private-include and VERSION-mirroring workarounds in the shim instead.

**Suggested actions (ordered):**

1. Fix the root cause in the core: replace `CMAKE_SOURCE_DIR` with `PROJECT_SOURCE_DIR` in the four `src/*/CMakeLists.txt` files (line 5 of each). This is behavior-preserving for standalone builds and correct for every nested consumption (the colcon shim *and* plain `add_subdirectory` consumers). The "never touch `src/`" invariant was a rollout-safety measure for the overlay branch, not a permanent law; this one-token fix is the textbook case for relaxing it, either on this branch or as an immediate follow-up PR.
2. After (1), rebuild the overlay and assert the layout: extend the overlay CI's `overlay-build` job (or a small check in `build_ros2.sh --clean` test phase) with `test -f ros2/install/<pkg>/include/<pkg>/wrapped_impl/CWrapperPlaceholder.h && test ! -e ros2/install/<pkg>/wrapped_impl`. The static verifier cannot see this (it is a build artifact), so it belongs in the workflow.
3. Then demote the private `${TEMPLATE_PROJECT_REPOSITORY_ROOT}/src` includes to what the comment claims they are (true legacy compat), or remove them and let the bridge consume the exported interface — which also makes the "exported interface remains primary" statement in the CMake comments true again.
4. Keep the shim's legacy-`VERSION` mirror (`ros2/template_project/CMakeLists.txt:28-41`): unlike the include hack, that one genuinely serves only older derived repos and is correctly commented.

### M2 — `add_ros2_support.sh` copies runtime-generated `__pycache__` bytecode into derived repos, and the placeholder-leak check is structurally blind to it

**Severity: Major (rollout hygiene + a silently unverifiable plan invariant). Confidence: High — reproduced during this review.**

**Evidence.** The source tree currently contains `ros2/template_project_spinup/launch/__pycache__/*.cpython-312.pyc` — a side effect of the local launch smoke tests: with `--symlink-install`, the installed launch files are symlinks back into the source tree, so `ros2 launch` compiles bytecode *next to the source files*. `copy_ros2_tree()` only skips the top-level `build|install|log` entries (`add_ros2_support.sh:272-279`), so `cp -a` carries the nested `__pycache__` along. Running the verifier's own fixture during this review produced:

```
fake_apply/ros2/space_nav_spinup/launch/__pycache__/space_nav_composition.launch.cpython-312.pyc
```

and `strings` on those files shows **5 surviving `template_project` references each** — the content-rename pass correctly skips binary files (`grep -Iq`), and the filename pass renames them, so a derived repo receives stale, misnamed bytecode with the old template identifiers embedded.

The verifier's final invariant — "no `template_project` remains in any copied overlay file" (`VerifyTemplateProjectRos2Overlay.cmake:294-300`) — passes anyway, because CMake's `file(READ)` truncates binary content at the first NUL byte, which in a `.pyc` occurs in the header before any string constants. So the check that was designed to guarantee exactly this invariant cannot see the one class of file that violates it. (Git hygiene is unaffected: `*.pyc` is already in `.gitignore`; this is purely a rollout-path issue.)

**Suggested actions:**

1. In `copy_ros2_tree()`, copy via a filtered walk instead of bare `cp -a` — e.g. `find "${SOURCE_DIR}/ros2" \( -name __pycache__ -o -name build -o -name install -o -name log \) -prune -o -type f -print` piped into per-file copies, or simply `rm -rf` any `__pycache__` in the target after the copy. The prune approach is better: it also future-proofs against other runtime-generated files nested inside package dirs (the exact failure class Stage 0 predicted at the workspace level but not at the package level).
2. Close the verifier's blind spot with a *name*-based assertion, which binary content cannot evade: after the fixture apply, `file(GLOB_RECURSE ...)` for `*__pycache__*` and `*.pyc` under the fake target and FATAL_ERROR on any hit.
3. Optionally, have `build_ros2.sh --clean` also remove `ros2/*/launch/__pycache__` (or run launch smoke tests from the install dir with `--symlink-install` documented as the cause). Low priority once (1) and (2) exist.

### M3 — The next release tag will predictably break core CI until someone re-syncs and commits `ros2/*/package.xml`

**Severity: Major (guaranteed future CI failure, undocumented process step). Confidence: High on the mechanism (all links verified in-tree); the failure itself is predicted, not observed.**

**Evidence chain.** (a) The four `package.xml` files are tracked with `<version>1.10.3</version>`. (b) `tests/CMakeLists.txt` registers `template_project_ros2_overlay_static_contract` with `-DEXPECTED_VERSION=${PROJECT_VERSION_CORE}`, resolved at configure time from `git describe`. (c) The verifier FATAL_ERRORs when any manifest version differs from `EXPECTED_VERSION`. (d) `build_linux.yml` runs the full ctest suite (`ctest --test-dir ... --no-tests=error`, line 166). Therefore, the moment a `v1.11.0` tag exists, every core CI run fails on this test until `./generate_version.sh --sync-ros2` is run and the manifests are committed — even for PRs that never touch `ros2/`.

This coupling is *defensible* (it is the mechanism that forces manifests to stay honest, and the error message states expected vs. got), but it is nowhere written down as a release step: `doc/ros2_overlay.md` §Version sync describes the auto-sync inside `build_ros2.sh` and the manual command, but never says "tagging a release now requires committing the synced manifests, or core CI goes red". A maintainer tagging from the GitHub UI six months from now will hit this cold.

Note also that a source tarball (no `.git`, no gitignored `VERSION` file) resolves the version to the hardcoded `0.0.0` default and the same test fails — an edge case, but worth a sentence in the docs.

**Suggested actions:**

1. Add a "Release tagging" note to `doc/ros2_overlay.md` §Version sync (and/or `doc/versioning.md`, which is the natural home): after creating a `v*.*.*` tag, run `./generate_version.sh --sync-ros2` and commit the manifest changes; core CI enforces this.
2. Optionally soften the failure mode: the contract test could `WARNING`+pass when the working tree's git version is *ahead* of the manifests but the manifests are mutually consistent, and hard-fail only on mutual inconsistency — trading strictness for not blocking unrelated PRs. This is a policy choice; documenting (1) alone is sufficient if you prefer the strict behavior.

### M4 — The CUDA/OptiX facade is advertised everywhere but has never been executed end-to-end

**Severity: Major (untested advertised feature on the riskiest path). Confidence: High that no evidence of a `--cuda` run exists (CONTEXT.md, `doc/developments/ROS2_OVERLAY_STAGE_OUTPUTS.md`, and all CMake caches checked); Medium that it would actually fail.**

**Evidence.** The only facade validation on record is the Stage 1 *negative* check: `grep "^ENABLE_CUDA:BOOL=OFF$" ros2/build/template_project/CMakeCache.txt`. There is no record — in `CONTEXT.md`, `doc/developments/ROS2_OVERLAY_STAGE_OUTPUTS.md`, or any build cache — of `./build_ros2.sh --cuda` ever completing. The ON path is exactly where nested-build risk concentrates: CUDA language enablement happens inside the *nested* `project()` call under a `LANGUAGES NONE` shim, GPU architecture auto-detection (`HandleCUDA.cmake`) runs in that nested scope, and the PTX-embedding pipeline (`cmake_cuda_ptx_tools.cmake`) has never run under colcon. The plan consciously excluded CUDA from CI (no ROS on the GPU runner — reasonable), but "no CI" quietly became "no validation at all", and `doc/ros2_overlay.md` §Build usage presents `./build_ros2.sh --cuda` as a working command.

**Suggested actions:**

1. Run `./build_ros2.sh --clean --cuda` (and `--cuda --optix` if the OptiX SDK is present) once on the local GPU machine; record the result in `doc/developments/ROS2_OVERLAY_STAGE_OUTPUTS.md`/`CONTEXT.md` the same way every other gate was recorded. If it fails, the shim is the likely fix site (e.g. deferred language enablement or architecture flags), not the root CMake.
2. Until (1) happens, add one honest sentence to `doc/ros2_overlay.md`: the CUDA/OptiX overlay path is validated manually, not in CI — and state the last validated date/hardware. The doc already says CI is CPU-only; it does not say the GPU path is otherwise unverified.
3. Cheap permanent guard: the facade *plumbing* (flag → colcon → shim cache-FORCE → core cache) can be regression-tested without a GPU by asserting `ENABLE_CUDA:BOOL=ON` appears in the shim's CMakeCache after a configure-only run that is allowed to fail at compiler detection. Worth considering, not mandatory.

---

## 3. Minor findings

### m1 — Promised spdlog hermeticity note is missing (explicit plan item not delivered)

Plan §Risks: "note `ENABLE_FETCH_SPDLOG OFF` as hermeticity knob in docs." No such note exists in `doc/ros2_overlay.md` (checked by grep; the string appears only in build caches). Consequence: the overlay build silently requires network for the spdlog FetchContent even in "offline" workflows, and the escape hatch is undocumented. **Action:** one paragraph in `doc/ros2_overlay.md` §Build usage: `./build_ros2.sh --cmake-arg -DENABLE_FETCH_SPDLOG=OFF` for hermetic builds (requires a system/preinstalled spdlog or accepts the no-spdlog path, whichever the core supports).

### m2 — The original root stage-output log was outside every cleanup set

It sits at the repo root, self-describes as "temporary", is not matched by `.gitignore`, and is not in `tailor_template_cleanup.sh`'s removal lists — so if it gets committed (easy to do with a broad `git add`), it ships into every tailored derived repo forever. **Action:** move it to `doc/developments/` (already removed by tailoring and excluded from Doxygen) before committing the branch, or delete it once its content is fully mirrored in `CONTEXT.md` (it largely is).

### m3 — Filename renaming is substring-based; the plan requires word-boundary awareness for "contents and file/dir names"

`replace_placeholder_in_file` uses a proper look-behind/look-ahead regex, but the path rename uses plain substring substitution (`add_ros2_support.sh:340`: `${path_base_//template_project/${ros_package_prefix}}`). Today all copied filenames start with `template_project`, so this is unreachable in practice, and the `my_template_project_x` fixture passes for a different reason (the *content* regex handles the boundary case). Still, it deviates from the plan's letter and would mangle a hypothetical future file like `my_template_project_notes.md` inside `ros2/`. **Action:** either apply the same boundary discipline to basenames (a small bash regex or perl one-liner), or amend the plan text to state that path renaming is prefix-anchored by convention. Cheap; pick one so plan and code agree.

### m4 — Dead dependency declarations in `template_project_interfaces`

`package.xml` declares `<depend>std_msgs</depend>` and the CMakeLists passes `std_msgs` to `rosidl_generate_interfaces`, but neither `AlgorithmStatus.msg` (only `builtin_interfaces/Time`) nor `RunAlgorithm.srv` (primitives) uses it. Likewise `<test_depend>ament_lint_auto/ament_lint_common</test_depend>` are declared but the CMakeLists never calls `ament_lint_auto_find_test_dependencies()`, so they only cost rosdep install time. **Action:** drop `std_msgs` from both files (or keep it deliberately as a template affordance with a one-line comment saying so — messages in derived repos usually need it); either wire up or drop the lint test-deps for consistency with `template_project_ros`, which declares none.

### m5 — The node's service/publish path has no automated coverage anywhere

`test_node_construction` covers construct→configure→cleanup; nothing exercises `on_activate`, `handleRunAlgorithm`, or `publishStatus` — the code that actually calls the core seam through the node — and CI has no launch smoke (the plan's launch smokes were local-only gates). A plugin-registration regression (`RCLCPP_COMPONENTS_REGISTER_NODE` / the component library name in the composition launch file) would pass all CI today. Additionally, the service is created in `on_configure` and answers in *any* lifecycle state; only publishing is activation-gated — conventional lifecycle designs gate or reject service work when inactive, and a template teaches patterns. **Action:** (a) extend the gtest: configure→activate→call `handleRunAlgorithm` via a service client (or directly) and assert response + evaluation count; (b) consider a `launch_testing` smoke of `template_project_composition.launch.py` in the `overlay-build` CI job — it is the only thing that proves component discoverability; (c) either gate the service on the active state or add a comment stating the permissive choice is deliberate.

### m6 — `build_ros2.sh` niggles

- `sync_ros2_package_versions` silently returns when the target's `generate_version.sh` predates `--sync-ros2` (`build_ros2.sh:168-170`). Every other skip path warns; this one should too, otherwise a derived repo with an old script gets stale manifests with no signal.
- `colcon test-result --verbose` scans the whole workspace: after a `--packages-select` run without `--clean`, stale failure XML from an earlier broken package fails the current, unrelated invocation. Consider `colcon test-result --test-result-base build/<pkg>` when a selection is active, or document the behavior.
- Style: `"${cmake_args[@]}"` at line 205 is expanded unguarded while `colcon_args`/`packages_select` are length-guarded. Both are correct on bash ≥ 4.4; pick one idiom (the guard is the defensive one given `set -u`).

### m7 — Template placeholder metadata in `package.xml` is not in the rename story

All four manifests carry `<maintainer email="pietro.califano@example.com">` and `<license>MIT</license>`. The rename pass (correctly) never touches them, and neither `doc/template_usage.md`'s rename map nor the `add_ros2_support.sh` post-apply checklist tells a derived repo to update maintainer/description/license. Derived repos will ship the template author's placeholder identity in their ROS metadata. **Action:** one row/bullet in the rename map and one checklist line: "update maintainer, description, and license in `ros2/*/package.xml`".

### m8 — Fence stripper accepts an orphan end-marker

In `strip_ros2_overlay_doc_fences`'s awk, a `<!-- ros2-overlay-end -->` with no preceding begin falls through and is printed verbatim (the `in_ros2_overlay_` guard only errors on an *unclosed* begin). The file passes as "stripped" while still containing a marker. Unreachable with the current five docs, but the guard exists precisely for future edits. **Action:** in the end-marker block, `exit 1` when `!in_ros2_overlay_`.

### m9 — `add_ros2_support.sh` usage text vs. actual modes

`main()` supports a standalone `--verify` (no `--apply`) run, but the usage text only documents `--verify` combined with `--apply`; conversely `--list --verify` silently ignores `--verify` because the list branch exits first. Trivial; align the usage text (document standalone `--verify`) or reject the ambiguous combination.

### m10 — Small doc imprecisions

- `doc/ros2_overlay.md` §Rollout names the seam path `ros2/<project_name>_ros/src/conversions.cpp`; with split names the directory is `<ros_prefix>_ros`, not `<project_name>_ros`. Use `<ros_prefix>` (the doc defines the term two paragraphs earlier).
- The copied `doc/ros2_overlay.md` in a derived repo says removal happens via `tailor_template_cleanup.sh --remove-ros2`, but derived repos typically deleted that script after tailoring. A half-sentence ("in already-tailored repositories, delete the listed paths manually") closes the loop.
- The uppercase facade names (`TEMPLATE_PROJECT_ENABLE_CUDA`) intentionally survive renaming into derived repos (the lowercase-only regex is deliberate, and copied script + shim stay mutually consistent — verified in the fixture output). That is a reasonable stable-flag design, but it *looks* like a rename bug to a derived-repo reader; add one sentence to `doc/ros2_overlay.md` stating the facade option names are intentionally invariant across renames.
- Worth an explicit warning in the option-flow table: the shim cache-FORCEs `ENABLE_CUDA`/`ENABLE_OPTIX` every configure, so passing `--cmake-arg -DENABLE_CUDA=ON` directly is silently overridden — always use the facade flags.

### m11 — Overlay CI has no time-based safety net for core→overlay drift

Not triggering on `src/**` is a sound, documented decision, but its residual risk (a core API change breaks `conversions.cpp` and nobody notices until the next overlay-touching PR) currently relies on someone remembering `workflow_dispatch`. A `schedule:` block (weekly) on `build_ros2_overlay.yml` costs three lines and bounds the drift window. Optional.

---

## 4. Plan-level assessment (gaps in the plan itself)

The plan deserves separate credit and critique from the implementation:

**Strengths.** Fixed design decisions stated up front with rationale; assumptions explicitly labeled with fallbacks; per-stage validation gates that were actually executed and logged; review-delta subsections that record *found* defects with root causes instead of silently fixing them; a validation stage (8) against a real derived repo that surfaced two genuine compatibility gaps; and a final audit stage with banned-naming greps. This is a model for how to run a multi-stage upgrade.

**Gaps, with hindsight:**

1. **The "verified facts" sweep was one grep short** (M1): it verified the `CMAKE_MODULE_PATH` wrinkle at root `CMakeLists.txt:41` but never searched `src/**/CMakeLists.txt` for other `CMAKE_SOURCE_DIR` couplings. A single `rg CMAKE_SOURCE_DIR src/` during Stage 0 would have found all four instances and likely changed the "no root/src change is needed" decision.
2. **The hard "never touch `src/`" invariant had no escape clause.** It was the right rollout discipline, but when Stage 8 hit the header-visibility problem, the invariant forced a workaround (private source include) instead of the one-token upstream fix — and the workaround's justifying comment then misdescribed the situation. Plans of this rigor should state under what conditions an invariant may be renegotiated.
3. **"Generated dirs" were modeled at the workspace level only** (M2): Stage 0 anticipated `build*/install/template_subbuild` at the root but not runtime-generated files *inside* package directories, which `--symlink-install` (a plan default) makes inevitable the moment the plan's own launch-smoke validation step runs.
4. **The release/tagging process was out of scope** (M3): the plan wired version sync into build time and test time but never asked "what happens on the next tag?".
5. **"Document CUDA locally" quietly substituted for "validate CUDA locally"** (M4): Stage 6 required only documentation for the GPU path; no stage gate ever required one `--cuda` run, despite the machine being a CUDA dev box.

None of these is unusual for a plan of this size; items 1-2 are the instructive ones.

---

## 5. Consolidated action checklist

Suggested order; (P) = touches the plan's frozen invariant and should be an explicit decision.

1. **(P) Fix `CMAKE_SOURCE_DIR` → `PROJECT_SOURCE_DIR`** in `src/{wrapped_impl,utils,template_src,template_src_kernels}/CMakeLists.txt:5`; rebuild overlay; add install-layout assertion to `overlay-build` CI; then demote/remove the private `src` includes in `ros2/template_project_ros/CMakeLists.txt` and fix their comments. (M1)
2. **Prune `__pycache__`/`*.pyc` in `copy_ros2_tree()`** and add a recursive name-based assertion to the verifier fixture. (M2)
3. **Run `./build_ros2.sh --clean --cuda` once** on the GPU box; record the outcome; add the "validated locally on <date>" note (or the failure follow-up) to `doc/ros2_overlay.md`. (M4)
4. **Document the release-tag step** (sync + commit manifests) in `doc/versioning.md` / `doc/ros2_overlay.md`. (M3)
5. **Relocate the stage-output log under `doc/developments/`**; add `doc/reports` to Doxygen `EXCLUDE` (and decide whether tailoring should remove it). (m2, header note)
6. Add the `ENABLE_FETCH_SPDLOG` hermeticity paragraph. (m1)
7. Batch of small script/doc fixes: warn on old-script sync skip, orphan-end-marker `exit 1`, usage-text `--verify`, `<ros_prefix>` wording, facade-name invariance note, maintainer/license rename-map row, basename-rename boundary alignment. (m3, m6-m10)
8. Optional hardening: service-call gtest + composition launch smoke in CI; weekly `schedule:` trigger. (m5, m11)

Items 1-2 are the only ones I would treat as blocking for merging the branch as a *template* (they define what every future derived repo inherits); 3-4 are blocking for the first release tag after merge; the rest are quality follow-ups.

---

## 6. GPT5.6-Sol max independent re-evaluation

This addendum preserves the original review above as a point-in-time assessment and records an independent source-and-runtime re-evaluation against commit `c70d4b6` on 2026-07-16. Status terms below distinguish defects that remain open from findings already fixed in the reviewed `HEAD`.

### Housekeeping note

**GPT5.6-Sol max:** Confirmed open at review time. `doc/CMakeLists.txt` included the whole `doc/` tree and excluded `doc/developments`, but not `doc/reports`; tailoring likewise removed `doc/developments` but not reports. Keep the report in source control for auditability, exclude `doc/reports` from generated API documentation, and remove it from tailored downstream projects. Store the stage-output log under `doc/developments/` so it follows the existing development-evidence policy. **Confidence: 100/100.**

### Summary and positive design points

- **Overall verdict. GPT5.6-Sol max:** The high implementation-fidelity assessment is justified, but the original robustness score is now optimistic for the advertised CUDA path: a clean CUDA overlay build can pass without compiling the project CUDA source. The design itself remains sound; the main corrective work is build-contract hardening rather than an overlay redesign. **Confidence: 98/100.**
- **S1, colcon-only enablement. GPT5.6-Sol max:** Confirmed and should be retained. The shim is the correct ownership boundary and keeps the standalone build ROS-free. M1 limits the stronger installed-header consumer claim, not the colcon-only architecture. **Confidence: 98/100.**
- **S2, conversions-vs-node split. GPT5.6-Sol max:** Confirmed. The core adaptation seam is node-free, the lifecycle node remains transport-focused, and the static verifier guards the seam location. **Confidence: 100/100.**
- **S3, fixture-driven rollout verification. GPT5.6-Sol max:** Confirmed. This is high-value behavioral coverage because it executes the script against copied trees rather than checking shell text alone. **Confidence: 100/100.**
- **S4, CMake-name/ROS-prefix split. GPT5.6-Sol max:** Confirmed. The separation preserves non-ROS CMake identities while making ROS package names valid, and the fixture coverage exercises default, override, and rejection paths. **Confidence: 99/100.**
- **S5, keep-by-default tailoring and fenced docs. GPT5.6-Sol max:** Confirmed in principle. The policy and unclosed-begin failure are correct; m8 is a narrow malformed-input hole and does not invalidate the design. **Confidence: 97/100.**

### Major findings

- **M1, nested header installation. GPT5.6-Sol max:** Confirmed open and broader than reported. The same bad `CMAKE_SOURCE_DIR` coupling exists in six module files, not four: `src/wrapped_impl`, `src/utils`, `src/utils/logging`, `src/utils/wrap_adapters`, `src/template_src`, and `src/template_src_kernels`. The current colcon install places public headers at the prefix root, and a direct installed-consumer compile cannot resolve `wrapped_impl/CWrapperPlaceholder.h`. Replace the coupling in all six files, prove the installed layout with a nested-build consumer test, and retain the private source include only as explicitly labeled legacy compatibility. **Confidence: 100/100.**
- **M2, copied bytecode. GPT5.6-Sol max:** The historical reproduction is credible, but the defect is resolved in current `HEAD`. `add_ros2_support.sh` removes `__pycache__`, `*.pyc`, and `*.pyo` from copied output, and the verifier creates synthetic cache/bytecode paths and asserts that they do not survive. No further implementation change is warranted; prune-before-copy would be an optimization, not a correctness requirement. **Confidence: 100/100.**
- **M3, release-tag coupling. GPT5.6-Sol max:** Confirmed open. Keep the strict manifest-version contract, but correct the suggested sequence: the final release tag must point at a commit that already contains synchronized manifests. A temporary unpublished local tag can supply the intended version to `generate_version.sh`; remove it, commit the generated metadata, recreate the final annotated tag on that commit, run gates, then push branch and tag atomically. Merely syncing after publishing the tag leaves the tagged source permanently inconsistent. **Confidence: 99/100.**
- **M4, CUDA/OptiX execution. GPT5.6-Sol max:** Partially resolved and materially expanded. `./build_ros2.sh --clean --cuda` completed on this machine with CUDA 12.9 and activated `ENABLE_CUDA=ON`; ROS reported 10 passing tests. However, no project `.cu` object was built, so this validates facade propagation and CUDA compiler activation only. OptiX remains unvalidated because no local SDK/header was available. **Confidence: 100/100.**
- **N1, CUDA source discovery false positive. GPT5.6-Sol max:** New major finding. Seven CMake files express the glob as `"*.cpp; *.cu"`; the leading space in the second list item prevents `placeholder.cu` from entering the target. Compiler-ID CUDA artifacts made the build look CUDA-active even though no project CUDA translation unit compiled. Fix all source-discovery sites, explicitly exclude dedicated `*.ptx.cu` inputs from ordinary library compilation, and make the CUDA gate assert a real project CUDA source/object rather than only cache flags. **Confidence: 100/100.**

### Minor findings

- **m1, spdlog hermeticity note. GPT5.6-Sol max:** Resolved in current `HEAD`. `doc/ros2_overlay.md` documents `ENABLE_FETCH_SPDLOG=OFF`, and the static verifier requires the note. **Confidence: 100/100.**
- **m2, stage-output evidence location. GPT5.6-Sol max:** Confirmed open in revised form. The file is now tracked rather than untracked, but it remains at the repository root and outside Doxygen/tailoring policy. Move it to `doc/developments/` and update all references. **Confidence: 100/100.**
- **m3, path rename boundaries. GPT5.6-Sol max:** Resolved. Path-component replacement now uses the same identifier-boundary discipline as content replacement, and the verifier includes a boundary fixture. **Confidence: 100/100.**
- **m4, interface package dependencies. GPT5.6-Sol max:** Resolved. The unused `std_msgs` and lint dependency declarations are absent, and the static contract rejects their return. **Confidence: 100/100.**
- **m5, service/publish coverage. GPT5.6-Sol max:** Partially resolved. Parameterized launch tests now exercise standalone and composition modes, root and namespaced launches, lifecycle activation, component discovery, and `run_algorithm(3.0) == 14.0` with status `ok`. The `~/status` lifecycle topic is still not subscribed to or asserted, so message delivery and fields remain uncovered. Add that assertion to the existing launch test; changing service availability by lifecycle state is a separate architecture policy and is not required for this improvement pass. **Confidence: 100/100.**
- **m6, `build_ros2.sh` behavior. GPT5.6-Sol max:** Resolved for the substantive items. Older version-only metadata helpers produce a warning, and selected-package test-result handling is scoped. The remaining array-expansion difference is valid Bash and not worth churn. **Confidence: 100/100.**
- **m7, manifest metadata placeholders. GPT5.6-Sol max:** Resolved by the project-metadata flowdown contract. Root CMake owns project metadata and the typed sync helper updates all manifests while preserving package-specific fields and modes. **Confidence: 100/100.**
- **m8, orphan fence end-marker. GPT5.6-Sol max:** Confirmed open. The awk stripper rejects nested/unclosed begin markers but prints an orphan end marker. Add a red fixture, then fail when an end marker is seen outside a fence. **Confidence: 99/100.**
- **m9, `--verify` mode. GPT5.6-Sol max:** Resolved. Usage documents verification as an apply-time operation, the script rejects `--verify` without `--apply`, and the verifier covers the rejection. **Confidence: 100/100.**
- **m10, documentation precision. GPT5.6-Sol max:** Partially resolved. `<ros_prefix>` terminology and metadata-renaming guidance are current. Still document manual removal for already-tailored repositories, state that facade option names intentionally survive project renames, and warn that direct `-DENABLE_CUDA`/`-DENABLE_OPTIX` arguments are overridden by the shim so users must select the facade flags. **Confidence: 98/100.**
- **m11, overlay drift detection. GPT5.6-Sol max:** Confirmed open. A weekly schedule is useful, but M1/N1 show that excluding `src/**` and `cmake/**` from event filters creates avoidable blind spots. Trigger on those owned core inputs and add a scheduled run as a backstop for toolchain/ROS drift. **Confidence: 100/100.**

### Plan-level gaps

- **P1, incomplete `CMAKE_SOURCE_DIR` sweep. GPT5.6-Sol max:** Confirmed, with six affected module files rather than four. The remediation test must enumerate or behaviorally cover every nested install path. **Confidence: 100/100.**
- **P2, invariant without an exception protocol. GPT5.6-Sol max:** Confirmed. The original invariant served the rollout well, but a plan needs an explicit process for relaxing it when empirical evidence places the root cause inside the frozen area. Limit the exception to reviewed CMake build-contract fixes in `src/`. **Confidence: 99/100.**
- **P3, package-local generated artifacts. GPT5.6-Sol max:** Confirmed as a historical planning gap; the implementation now closes it with copied-tree cleanup and regression fixtures. **Confidence: 100/100.**
- **P4, release lifecycle omitted. GPT5.6-Sol max:** Confirmed. Build-time synchronization is not a substitute for defining which commit a release tag must reference. **Confidence: 100/100.**
- **P5, documentation substituted for GPU validation. GPT5.6-Sol max:** Confirmed. The subsequent CUDA run also demonstrates that a command-level green result is insufficient unless the gate proves that a real project CUDA translation unit compiled. **Confidence: 100/100.**

### Original action-checklist disposition

- **A1. GPT5.6-Sol max:** Open; expand the fix from four to six module files and add an installed-consumer test before changing implementation. **Confidence: 100/100.**
- **A2. GPT5.6-Sol max:** Complete in current `HEAD`; retain its verifier coverage. **Confidence: 100/100.**
- **A3. GPT5.6-Sol max:** Partially complete; facade activation passed, real CUDA source compilation did not occur, and OptiX awaits an SDK-equipped host. **Confidence: 100/100.**
- **A4. GPT5.6-Sol max:** Open; replace the original sync-after-tag wording with a tag-on-synchronized-commit release procedure. **Confidence: 99/100.**
- **A5. GPT5.6-Sol max:** Open; relocate the evidence file and formalize report exclusion/removal. **Confidence: 100/100.**
- **A6. GPT5.6-Sol max:** Complete in current `HEAD`. **Confidence: 100/100.**
- **A7. GPT5.6-Sol max:** Mostly complete; only the orphan-end-marker guard and remaining m10 documentation clarifications warrant action. **Confidence: 99/100.**
- **A8. GPT5.6-Sol max:** Partially complete; component/service launch coverage exists, while status-topic coverage and drift triggers remain worthwhile. **Confidence: 100/100.**

The implementation plan for all open, worthwhile items is maintained separately in `doc/developments/ros2_overlay_review_remediation_plan.md`.

---

## 7. Stage 2 remediation update - 2026-07-17

This section updates only the current disposition of the historical M4/N1
findings; the original review and independent re-evaluation above remain as the
point-in-time record.

- **M4, CUDA/OptiX execution:** Fixed for the locally available toolchain. Clean
  ROS CUDA and CUDA+OptiX builds each completed four packages and 10 tests. The
  CUDA path compiled the real project `placeholder.cu` for `sm_120`; the OptiX
  path generated PTX and its embedded object with OptiX 8.0.0. GitHub ROS CI
  remains deliberately CPU-only. **Confidence: 100/100.**
- **N1, CUDA source discovery:** Fixed in all seven ownership sites. Ordinary
  `.cpp` and `.cu` patterns are separate list entries, every ordinary list
  explicitly excludes `*.ptx.cu`, and a CUDA-only compilation-database test
  rejects recurrence. Standalone CUDA passed 29 tests. **Confidence: 100/100.**
- **New validation finding, OptiX installed export:** The first end-to-end OptiX
  run generated the core PTX but failed when the bridge imported a target that
  claimed a non-existent package-local `include/optix`. The installed package
  now resolves the external SDK from `OPTIX_ROOT`, `OptiX_ROOT`,
  `OptiX_INSTALL_DIR`, or `OPTIX_HOME`; CUDA headers flow from recreated
  `CUDA::` targets. The install-consumer regression rejects build-machine path
  leakage and compiles `#include <optix.h>` through the installed core target.
  **Confidence: 100/100.**
- **Template/derived CI ownership follow-up:** Closed. Tailoring removes both
  template-only GPU verifiers, while active template CUDA CI checks the real
  source graph after materializing the generic project workflow. A tailored
  scratch compiled `placeholder.cu`, excluded `placeholder_to_ptx.ptx.cu`, and
  passed all nine project tests. **Confidence: 100/100.**

---

## 8. Review remediation closeout - 2026-07-17

This addendum records the final disposition after all remediation stages. It
does not replace the historical findings or the independent evaluation above.

- **M1, nested header installation:** Fixed. All six module install paths are
  rooted at `PROJECT_SOURCE_DIR`; the colcon shim no longer masks the defect
  with a private core-source include. A nested install and installed-only
  consumer regression resolves a public header exclusively through
  `template_project::template_project`. **Confidence: 100/100.**
- **M2, copied bytecode:** Remains fixed. Additive rollout removes Python cache
  directories and bytecode, and the copied-tree fixture rejects recurrence.
  **Confidence: 100/100.**
- **M3, release-tag coupling:** Fixed as a tested release contract. The
  documented and locally rehearsed sequence synchronizes all four manifests on
  the commit referenced by the final annotated tag; the strict overlay version
  verifier passes on that tagged tree. **Confidence: 100/100.**
- **M4 and N1, CUDA/OptiX execution and source discovery:** Fixed for the local
  CUDA 12.9 and OptiX 8.0 toolchain. Standalone and ROS builds compile the real
  project CUDA translation unit, keep `*.ptx.cu` under the dedicated PTX path,
  and validate installed OptiX consumption without exporting a machine-local
  SDK path. **Confidence: 100/100.**
- **m2, development evidence ownership:** Fixed. Stage evidence is under
  `doc/developments`; reports and development evidence are excluded from
  generated documentation and removed during project tailoring.
  **Confidence: 100/100.**
- **m5, service/status coverage:** Fixed. All standalone/composition and
  root/namespaced launch variants assert lifecycle activation, the service
  response, every `AlgorithmStatus` field, and a populated timestamp.
  **Confidence: 100/100.**
- **m8 and m10, fence handling and documentation precision:** Fixed. Tailoring
  rejects nested, orphaned, and unclosed fences. Overlay removal and the stable
  CUDA/OptiX facade contract are documented without implying that direct core
  cache variables survive shim configuration. **Confidence: 100/100.**
- **m11, overlay drift detection:** Fixed. Core CMake/source ownership paths
  trigger the active overlay workflow, and both the template workflow and the
  derived-project workflow template carry one weekly schedule. Static CMake
  and semantic YAML tests guard the trigger mapping. **Confidence: 100/100.**
- **P1-P5 and A1-A8:** Closed by the staged remediation plan. The explicit
  source-area exception was limited to the verified nested-install and CUDA
  ownership defects; release, GPU, publication, tailoring, and copied-workflow
  contracts now have behavioral tests. **Confidence: 99/100.**
- **Final helper-lint correction:** The closeout lint sweep found that
  `build_lib.sh` assigned `wrapper_interface_override` without ever reading it.
  Removing the two dead assignments cleared shellcheck and preserved all three
  wrapper-script behavior regressions. **Confidence: 100/100.**

The final implementation and design review found no unresolved critical or
major defect in the reviewed scope. The remaining observed output is the
pre-existing non-fatal GCC/fmt/spdlog `-Warray-bounds` warning.

---

## 9. Release and two-repository version-sync closeout - 2026-07-18

This addendum records the final release-contract review. It preserves all
historical findings above and evaluates only the Stage 8-13 extension.

- **Tailoring mutation safety:** Fixed. All removable documentation fences are
  validated before confirmation or removal. Rewritten CMake/documentation files
  are generated beside their targets, retain original modes, and are atomically
  installed. Nested, orphaned, and unclosed fences return nonzero with identical
  checksums, modes, and path inventory. **Confidence: 100/100.**
- **Testfield metadata ownership:** Fixed. The testfield owns its description,
  homepage, maintainer/contact, MIT license, and prerelease-aware version in one
  CMake contract. Its structured helper synchronizes only owned manifest fields
  while preserving identities, dependencies, processing instructions, and
  modes. Two consecutive syncs are byte-idempotent. **Confidence: 100/100.**
- **Canonical source release:** Fixed. CPack TGZ is the documented canonical
  release artifact in both repositories. Synthetic final-tag regressions create
  and extract the archive outside Git, require `VERSION`, manifests, license and
  source, exclude generated/VCS trees, compare core/full version resolution,
  and reject a missing-`VERSION` archive. GitHub-generated source links are
  explicitly non-canonical. **Confidence: 100/100.**
- **CI metadata drift:** Fixed. Active template and testfield workflows require
  the full-metadata capability marker and fail on any post-sync manifest diff.
  The generic project workflow retains a visible compatibility warning for old
  helpers and enforces the same diff gate whenever full sync is supported.
  **Confidence: 100/100.**
- **Release-tag and expected-version CI:** Fixed. Native and ROS workflows accept
  `v*.*.*` tag pushes. The active ROS verifier derives strict `X.Y.Z` from
  generated `VERSION`, independently of synchronized manifests. Semantic tests
  cover tag triggers and stale-manifest failure. **Confidence: 100/100.**
- **Documentation CI ownership:** Fixed. The template docs workflow runs the
  template verifier, the testfield build-only docs workflow runs the testfield
  verifier, and the generic docs workflow remains free of template-only checks.
  Both repositories produce fresh Doxygen HTML/XML. **Confidence: 100/100.**
- **Runtime regression:** Green. Main passed native `27/27`, standalone
  CUDA+OptiX `31/31`, CPU ROS `10/10`, and ROS CUDA+OptiX `10/10`; testfield
  passed native `43/43` and ROS `10/10`. Jazzy rehearsals passed active-template,
  tailored-generic, and testfield modes with four packages and 10 tests each.
  **Confidence: 100/100.**
- **Scope preservation:** Confirmed. No C++/CUDA/ROS API changed. Testfield's
  adapted `conversions.cpp` and `lib/wrap` state are unchanged. Canonical archive
  upload automation, cache/artifact redesign, and runner-topology changes remain
  explicitly deferred. **Confidence: 100/100.**

No unresolved critical or major defect remains in the reviewed extension. The
only recurring host output is the pre-existing non-fatal GCC/fmt/spdlog
`-Warray-bounds` warning; the testfield colcon invocation also reports expected
unused-variable warnings in packages that do not consume its external-template
path.

The proposed five-main/three-testfield history was reconstructed in disposable
clones in the required order. Every commit passed its focused gate before the
next was applied, and the source repositories remained uncommitted. This closes
the review at commit-ready, user-owned handoff rather than changing repository
history automatically.
