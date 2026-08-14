# Docs Workflow Rollout

## Scope

- [x] Generalized top-level-only Doxygen workflow in the template.
- [x] Full usage documentation for C++, CUDA/OptiX, wrappers, versioning, docs/Pages, tests, CI, and issue workflow.
- [x] Template CTest coverage for docs build, docs content, Pages workflow, issue templates, source `VERSION` behavior, and nested-library docs isolation.
- [x] Testfield update using the same staged checks.
- [x] Local verification in both repositories.
- [ ] GitHub Pages publication/output verification.

## Stop Rule

If a critical issue prevents trustworthy validation, stop immediately and add the failing command, exit code, output excerpt, suspected owner, and next action here before reporting.

## Verification Log

- `cmake --preset docs`: passed.
- `cmake --build --preset docs`: passed and generated `build_docs/doc/html/index.html`.
- `ctest --test-dir /tmp/cpp_cuda_template_docs_gate --output-on-failure -R "docs|version|nested|tailoring"`: 6/6 passed.
- The Pages workflow now verifies the deployed URL after `actions/deploy-pages@v4` by fetching `steps.deployment.outputs.page_url` and checking the published index for expected documentation links.
- Manual `workflow_dispatch` runs are build-only by default and intentionally skip deploy unless `deploy_pages=true`.
- `actions/configure-pages@v5` runs only in the deploy job, so pre-merge and build-only manual checks do not fail just because Pages has not been enabled yet.
- `README.md` is included in both push and pull-request path filters because Doxygen uses it as an input.
- Direct script checks passed:
  - `VerifyTemplateProjectDocsStatic.cmake`
  - `VerifyTemplateProjectDocsWorkflow.cmake`
  - `VerifyTemplateProjectNestedDocsIsolation.cmake`
  - `VerifyTemplateProjectVersionSideEffects.cmake`
  - `VerifyTemplateProjectTailoringScript.cmake`
- Local Pages output check passed: generated HTML contains the template usage, documentation workflow, wrapper, and versioning pages.

## Publication Blocker

- Command: `command -v gh`
- Exit code: 1
- Output excerpt: no `gh` executable found.
- Additional check: `curl --head --location --max-time 10 https://petercalifano.github.io/cpp_cuda_template_project/` currently returns HTTP 404, so there is no existing public Pages output at the expected project URL to validate against.
- Suspected owner: local publication environment, not the template docs implementation.
- Next action: install/authenticate GitHub CLI or commit/push the workflow and let GitHub Actions run in the remote repository.
- Note: a real Pages deployment was not executed because the required changes are still uncommitted local workspace changes, and publishing would require a commit/push step that was not explicitly requested.
