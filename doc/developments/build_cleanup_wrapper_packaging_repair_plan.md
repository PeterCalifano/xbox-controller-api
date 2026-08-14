# Build Cleanup and Wrapper Packaging Repair

> Historical record. Active v1.12.1 hardening and v2 test-ownership work is
> tracked only in
> `doc/developments/template_v2_test_ownership_plan.md`.

## Objective

Retain the safe portions of the working-tree changes relative to current
`main` at `b4fb532` (`v1.11.3` ancestor `dbffe4d`), repair the clean-build and
Python packaging regressions, and validate every stage before integrating the
next one. Do not stage or commit while implementing this plan.

## Stage 1: Remove isolated ROS editor leakage

- [x] Remove the ROS-specific keys added to `.vscode/settings.json`.
- [x] Preserve the existing C/C++ and CMake editor settings.
- [x] Preserve the optional ROS overlay boundary and run its static verifier.

### Stage 1 output

- `python3 -m json.tool .vscode/settings.json`: passed.
- `VerifyTemplateProjectRos2Overlay.cmake`: passed against version `1.11.3`.
- No ROS overlay source, helper, workflow, marker, or documentation file was
  changed.

## Stage 2: Integrate source-archive hygiene

- [x] Keep `CPACK_VERBATIM_VARIABLES` and recursive generated-directory ignore
  expressions.
- [x] Add a nested `examples/build_release_sentinel` fixture.
- [x] Verify root and nested build output are absent from the release archive.
- [x] Run build-tree package and release-tag checks.

### Stage 2 output

- `VerifyTemplateProjectReleaseTagSync.cmake`: passed.
- `VerifyTemplateProjectBuildTreePackage.cmake`: passed.
- `git diff --check HEAD`: passed.

## Stage 3: Integrate clean safety without changing established behavior

- [x] Skip clean validation when `--rebuild-only` is active.
- [x] Preserve canonical path, conventional name, cache, and cache-owner checks.
- [x] Move all verifier fixtures into a disposable miniature source checkout.
- [x] Cover rejection and success paths, including rebuild-only preservation.
- [x] Update build-script documentation.
- [x] Run in-repository and external-build validation.

### Stage 3 output

- `shellcheck build_lib.sh`: passed.
- `bash -n build_lib.sh`: passed.
- Clean-safety verifier with an in-repository binary root: passed.
- Clean-safety verifier with a `/tmp` binary root: passed.
- The verifier confirmed rejection of outside, unconventional, cacheless,
  markerless, and foreign-owned targets.
- The verifier confirmed successful owned cleanup and rebuild-only sentinel
  preservation.
- `git diff --check HEAD`: passed.

## Stage 4: Repair explicit main-runtime wrapper packaging

- [x] Replace native-library directory globs with exact target-derived paths.
- [x] Stage the shared main target and its SONAME beside the wrapper.
- [x] Keep loader-relative RPATH and prefix-relative CMake installation.
- [x] Exclude checkout metadata, caches, and bytecode from distributions.
- [x] Validate isolated wheel and CMake-install imports.

### Stage 4 output

- Fresh wrapper configure and `template_project_py` build: passed.
- Build-linked import with an empty `LD_LIBRARY_PATH`: passed.
- Wheel contents contained only `template_project.so`,
  `libtemplate_project.so`, package source, and distribution metadata.
- A synthetic `build/src/libunrelated_plugin.so` was excluded from the wheel.
- `_wrapper_build.py` was excluded from the wheel.
- Isolated wheel import with an empty `LD_LIBRARY_PATH`: passed.
- Prefix-relative CMake install placed the wrapper and main runtime library
  under `lib/python3.12/site-packages/template_project`.
- Isolated CMake-install import with an empty `LD_LIBRARY_PATH`: passed.
- Installed wrapper RUNPATH was loader-relative:
  `$ORIGIN/../lib:$ORIGIN`.
- Generated `python/setup.py` passed `py_compile`.

## Stage 5: Add optional declared runtime dependencies

- [x] Add `<namespace>_GTWRAP_RUNTIME_DEPENDENCY_TARGETS` without changing the
  existing build-order dependency option.
- [x] Validate target existence, ownership, and supported library type.
- [x] Stage and install declared targets through the same explicit path.
- [x] Document the additive interface and its limitations.

### Stage 5 output

- A wrapper configure using
  `template_project_GTWRAP_RUNTIME_DEPENDENCY_TARGETS=template_project`
  passed; duplicate automatic/explicit targets were deduplicated.
- The configured wrapper target built successfully.
- A missing declared target failed configure with the expected diagnostic.
- An interface-library declaration failed configure with the expected
  supported-type diagnostic.
- The established `<namespace>_GTWRAP_DEPENDENCY_TARGETS` build-order
  contract remains unchanged.

## Stage 6: Add self-contained packaging conformance coverage

- [x] Build a network-free Python C-API fixture with versioned and unrelated
  shared libraries.
- [x] Verify exact wheel and CMake-install contents.
- [x] Import from isolated locations without build-tree loader paths.
- [x] Ensure template tailoring removes the conformance test.

### Stage 6 output

- The fixture built a Python C-API module, a two-library versioned runtime
  chain, and an unrelated shared-library sentinel without downloading
  dependencies.
- The generated wheel contained exactly the wrapper and declared runtime
  artifact names; the unrelated library and `_wrapper_build.py` were absent.
- Wheel and CMake-prefix imports returned the expected value with an empty
  `LD_LIBRARY_PATH`.
- The installed wrapper used a loader-relative runtime path and retained no
  disposable build-root path.
- An unlinked declared runtime was modified and rebuilt through the wrapper
  target; its staged copy refreshed byte-for-byte without requiring the
  extension to relink.
- `VerifyTemplateProjectPythonPackaging.cmake`: passed.
- `VerifyTemplateProjectTailoringScript.cmake`: passed after adding explicit
  removal coverage for both new template-conformance verifiers.
- `git diff --check HEAD`: passed.

## Stage 7: Integrated acceptance and review

- [x] Run the complete external CPU build and CTest suite.
- [x] Run actual gtwrap wheel and CMake-install smoke checks.
- [x] Run source release, install/export, and external C++ consumer checks.
- [x] Run static and syntax checks plus `git diff --check`.
- [x] Review complete documentation and readability without staging.
- [x] Record proposed branch name and dependency-ordered commit split.

### Stage 7 output

- Fresh external Ninja configure and full CPU/gtwrap build under
  `/tmp/cpp_cuda_template_acceptance`: passed. The compiler reported only
  existing warnings from bundled pybind11 headers.
- Complete CTest suite: 29/29 passed. This includes clean safety, exact Python
  packaging, canonical source release, tailoring, cross-compile, C++ runtime,
  Python, ROS-overlay static, and workflow checks.
- CMake prefix install: passed.
- A fresh external C++ consumer found the installed package, linked
  `template_project::template_project`, built, and ran successfully.
- CMake-installed Python import with an empty `LD_LIBRARY_PATH`: passed.
- The real gtwrap wheel contained exactly `template_project.so` and
  `libtemplate_project.so` as native package artifacts; `_wrapper_build.py`
  was absent.
- Isolated real-wheel import with an empty `LD_LIBRARY_PATH`: passed.
- Wheel native artifacts used loader-relative RUNPATH entries and contained no
  acceptance-build path.
- `bash -n`, `shellcheck`, configured Python `py_compile`, JSON parsing, and
  `git diff --check HEAD`: passed.
- Final review added destructive-boundary clean revalidation, incremental
  runtime-copy refresh, package-lifetime Windows DLL directory handles, exact
  CMake-install native-content assertions, and concise verifier/module
  documentation.
- `.vscode/settings.json` now matches `HEAD`; no editor-only ROS change remains.
- The Git index is empty. No files were staged, committed, reset, or pushed.

## Proposed branch and commit split

Suggested branch: `fix/clean-wrapper-packaging-safety`

1. `fix(build): constrain cleanup to owned build trees`
   - `build_lib.sh`, clean documentation, the clean-safety verifier, its CTest
     registration, and its tailoring-removal coverage.
2. `fix(package): exclude nested generated trees from source archives`
   - The CPack ignore rules and canonical release/archive regression fixtures.
3. `fix(wrapper): package declared runtimes relocatably`
   - Runtime-target options, `HandleWrapper.cmake`, Python package templates
     and entrypoint, plus wrapper documentation.
4. `test(wrapper): add isolated runtime packaging conformance`
   - The self-contained packaging verifier, its CTest registration, and its
     tailoring-removal coverage.
5. `docs(dev): record cleanup and packaging repair`
   - This development plan and validation log.

Files containing more than one concern (`CMakeLists.txt`, `README.md`,
`tests/CMakeLists.txt`, `tailor_template_cleanup.sh`, and the tailoring
verifier) should be split by hunk when forming these commits.

## Commit staging log

- [x] Stage and review `fix(build): constrain cleanup to owned build trees`.
- [x] Stage and review `fix(package): exclude nested generated trees from
  source archives`.
- [x] Stage and review `fix(wrapper): package declared runtimes relocatably`.
- [x] Stage and review
  `test(wrapper): add isolated runtime packaging conformance`.
- [ ] Stage and review `docs(dev): record cleanup and packaging repair`.

### First staged batch output

- Staged seven clean-safety files/hunks; wrapper and archive changes remain
  unstaged.
- Reviewed the complete `git diff --cached`, including the new verifier as a
  reader will receive it.
- Updated the staged `build_lib.sh` module header and documented the new
  clean-path validation contract during the staged-code quality pass.
- Materialized the exact Git index at
  `/tmp/cpp_cuda_template_index_stage1.xUDNvI`.
- Exact-index `bash -n`, ShellCheck, direct clean-safety verification, direct
  tailoring verification, and CMake configure: passed.
- Exact-index focused CTest: 2/2 passed.
- `git diff --cached --check`: passed.

### Second staged batch output

- Confirmed the first batch was committed as `7b0429c` on
  `bugfix/clean-wrapper-packaging-safety`.
- Staged only the CPack source-ignore rules and the two canonical
  release/archive verifier updates; runtime-wrapper CMake options remain
  unstaged.
- Reviewed the complete three-file `git diff --cached`.
- Materialized and tagged the final exact Git index at
  `/tmp/cpp_cuda_template_index_stage2_final.Iercsg`.
- Exact-index canonical release/source-archive verification: passed.
- Exact-index build-tree package, install, and external-consumer verification:
  passed.
- `git diff --cached --check`: passed.

### Third staged batch output

- Confirmed the source-archive batch was committed as `ee09cd8` on
  `bugfix/clean-wrapper-packaging-safety`.
- Staged only the seven wrapper implementation and documentation files;
  packaging-conformance tests, tailoring coverage, and this development plan
  remain unstaged.
- Reviewed the complete `git diff --cached` as the commit will be received.
  Strengthened the `HandleWrapper.cmake` file header and documented the
  multi-config output-path invariant during the staged-code quality pass.
- Materialized and tagged the final exact Git index at
  `/tmp/cpp_cuda_template_index_stage3.Eln2e6`.
- Exact-index shared-library configure, complete build, and wrapper import:
  passed.
- Exact-index wheel and prefix-relative CMake install contained only
  `template_project.so` and `libtemplate_project.so` as native package
  artifacts. `_wrapper_build.py` and a synthetic unrelated shared library were
  excluded.
- Isolated wheel and CMake-install imports with an empty `LD_LIBRARY_PATH`:
  passed. All packaged native artifacts used loader-relative `$ORIGIN` paths
  without retaining the disposable checkout path.
- Explicit duplicate runtime declarations were deduplicated. Missing and
  interface-library runtime declarations failed configure with the intended
  diagnostics.
- Exact-index static-library configure, build, import, and wheel-content
  matrix: passed; the wheel contained only the extension module as a native
  artifact.
- `git diff --cached --check`: passed.

### Third staged batch minimality audit

- Re-reviewed all 388 additions and 41 removals relative to `ee09cd8`. The
  staged scope remains limited to seven wrapper implementation, package
  metadata, entrypoint, and public documentation files.
- Every functional area maps to a required contract: a separate runtime-target
  option, exact target/SONAME staging, relocatable loader paths,
  prefix-relative CMake installation, deterministic wheel contents, metadata
  exclusion, and Windows DLL-directory lifetime.
- The larger helper is required to support project-owned shared/module targets,
  versioned libraries, unlinked declared runtimes, incremental refresh, CMake
  install, and Linux/macOS/Windows output without scanning build directories.
- Documentation, comments, type hints, examples, and docstrings add no runtime
  behavior but satisfy the repository's staged-code documentation/readability
  gate and expose the new public option.
- One caller-side `list(REMOVE_DUPLICATES)` is redundant because
  `configure_python_runtime_artifacts()` already deduplicates its input.
- The two `getattr(..., "WRAPPER_RUNTIME_LIBRARY_PATHS", [])` fallbacks are not
  required for newly generated metadata. In the wheel builder, the default can
  hide stale pre-change metadata and should fail early instead of producing an
  extension-only wheel. In the package entrypoint, graceful fallback is part of
  the existing pure-Python import behavior, so strict access would need to be
  converted into a caught `ImportError` rather than allowed to escape.
- CMake alias targets are not valid operands for `set_target_properties()` or
  `install(TARGETS)`. The public option should either be documented as accepting
  direct project-owned build targets only or resolve aliases before validation.
- The fallback `pyproject.toml` and `setup.py` generators predate this change
  and do not implement binary-wheel assembly. Expanding those legacy fallbacks
  would broaden this commit; the checked-in templates remain the supported
  relocatable-wheel path.
- No staged source was changed during the initial audit. The user subsequently
  approved the three minimal tightening recommendations.

### Third staged batch minimality tightening output

- Removed the redundant caller-side runtime-target deduplication; the packaging
  helper remains the single owner of that invariant.
- Changed wheel assembly to require
  `WRAPPER_RUNTIME_LIBRARY_PATHS`. Stale pre-change metadata now fails the wheel
  build instead of silently producing an extension-only wheel.
- Documented the minimal direct-build-target contract rather than adding CMake
  alias resolution. Alias target names are explicitly outside the option
  contract.
- Reduced the staged delta from 388 to 384 additions while preserving the
  seven-file wrapper-only scope.
- Materialized the tightened exact Git index at
  `/tmp/cpp_cuda_template_index_stage3_minimal.DWF3j9`; its tree is
  `737f0b60a2adf0a8c959b0e568de73d7c04a716e`.
- Exact-index shared configure, full build, focused wrapper CTest, wheel build,
  CMake install, and isolated wheel/install imports with an empty
  `LD_LIBRARY_PATH`: passed.
- The shared wheel contained exactly `template_project.so` and
  `libtemplate_project.so` as native package artifacts; checkout metadata was
  absent and packaged artifacts retained loader-relative RUNPATH entries.
- Deliberately removing `WRAPPER_RUNTIME_LIBRARY_PATHS` from generated metadata
  caused the wheel build to fail with the expected missing-attribute
  diagnostic.
- Exact-index static configure, wrapper build, focused CTest, wheel build,
  native-content assertion, and isolated import: passed. Valid static metadata
  retained an explicit empty runtime list.
- The compiler reported only the established warnings from bundled pybind11
  headers.

### Fourth staged batch output

- Confirmed the wrapper implementation was committed as `ff6af61`
  (`[MAJOR] Add relocatable runtime packaging for wrapper`) on
  `bugfix/clean-wrapper-packaging-safety`.
- Staged exactly four template-conformance files: the packaging verifier, its
  CTest registration, the cleanup manifest entry, and tailoring-removal
  assertions. This development plan remains unstaged.
- Reviewed the complete 512-line staged delta as the commit will be received.
  The new verifier owns a disposable, network-free fixture and separates
  incremental refresh, exact wheel contents, exact CMake-install contents,
  isolated imports, and loader-path assertions into documented blocks.
- Added fixture-local Windows symbol export handling during the staged-code
  quality pass so the generated runtime chain is linkable on DLL platforms.
- Materialized the exact Git index at
  `/tmp/cpp_cuda_template_index_stage4.bHX3Wf`; its tree is
  `733ea9b6f9fc96b27e3800ad3a0fa4c564010e3a`.
- Exact-index configure registered the packaging and tailoring tests.
- Exact-index focused CTest: 2/2 passed
  (`template_project_python_packaging` and
  `template_project_tailoring_cleanup_script`).
- The packaging test built versioned linked runtimes, an unlinked declared
  runtime, an unrelated sentinel, and a Python C-API module without gtwrap or
  network access. Wheel/install contents, incremental refresh, isolated
  imports, and loader-relative paths passed.
- Exact-index ShellCheck, `bash -n`, `git diff --cached --check`, index-tree
  equality, and four-file scope assertions: passed.

## PR review remediation

PR #28 received two current, unresolved packaging findings after the initial
`v1.12.0` tag was pushed. The release has not been published, so the tag may be
replaced after the accepted fixes are committed and final validation passes.
Each functional fix remains an independently staged, user-committed batch.

### Stage 8: Install for the resolved Python ABI

- [x] Derive the prefix-relative Python install root from the resolved
  interpreter major and minor versions, never the requested version string.
- [x] Fail configuration clearly when the resolved ABI version is unavailable.
- [x] Add deterministic coverage where the requested version contains a patch
  component but the install directory remains `python<major>.<minor>`.
- [x] Validate, stage, and review only the implementation and regression test.
- [ ] Wait for the user to commit before starting Stage 9.

### Stage 9: Reject flat runtime destination collisions

- [ ] Detect different runtime targets that would stage to the same target-file
  or SONAME destination.
- [ ] Report both conflicting targets and the destination name during
  configuration.
- [ ] Add a negative fixture with two declared shared targets using the same
  output name.
- [ ] Re-run the positive packaging fixture to preserve exact runtime packaging.
- [ ] Validate, stage, and review only the implementation and regression test.
- [ ] Wait for the user to commit before starting final acceptance.

### Stage 10: Final acceptance and unpublished tag replacement

- [ ] Run the focused packaging tests and complete repository validation
  appropriate to the two fixes.
- [ ] Review the complete branch diff and record final evidence here.
- [ ] Confirm both fix commits and the development log are present at `HEAD`.
- [ ] Replace the unpublished remote `v1.12.0` tag with the validated `HEAD`.

### Stage 8 output

- Added `_resolve_python_install_root()` in `cmake/HandleWrapper.cmake`. It
  accepts the modern `Python_VERSION_*` result variables and gtwrap's cached
  `PYTHON_VERSION_*` compatibility variables, uses only major and minor, and
  fails when neither resolved pair is available.
- Removed the requested-version and generic `python` install fallbacks from the
  real gtwrap installation path.
- Extended the existing self-contained packaging verifier with a full
  major.minor.patch request and an exact major.minor install-root assertion.
- Worktree packaging verification: passed.
- A real local-gtwrap configure with `WRAP_PYTHON_VERSION=3.12.3` generated
  `lib/python3.12/site-packages` install rules: passed.
- A configure without resolved major/minor values failed with the intended
  diagnostic: passed.
- Staged exactly `cmake/HandleWrapper.cmake` and
  `tests/cmake/VerifyTemplateProjectPythonPackaging.cmake`; this development
  log remains unstaged.
- Reviewed the complete staged diff. The existing module headers remain
  accurate, and the new internal helper and regression block document the ABI
  and requested-versus-resolved version invariant.
- Materialized exact index tree
  `4ded0e4fac7a3ea9644533dd3f33d164d831dffb` at
  `/tmp/cpp_cuda_template_index_stage8.gWmG7y`.
- Exact-index self-contained packaging verification and real local-gtwrap
  configure with a full patch request: passed.
- `git diff --cached --check`: passed.
