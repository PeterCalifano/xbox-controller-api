# Documentation Workflow

The documentation workflow is Doxygen-first and CMake-driven. It is intentionally scoped to the main project so nested template-derived dependencies do not publish duplicate `doc` targets or appear in the generated pages.

## Local Build

```bash
cmake -S . -B build_docs \
  -D ENABLE_TESTS=OFF \
  -D ENABLE_CUDA=OFF \
  -D ENABLE_OPTIX=OFF \
  -D BUILD_DOC_HTML=ON \
  -D BUILD_DOC_XML=ON
cmake --build build_docs --target doc
```

HTML output is written to:

```text
build_docs/doc/html/index.html
```

XML output is written to:

```text
build_docs/doc/xml/
```

Use the preset when your CMake version supports presets:

```bash
cmake --preset docs
cmake --build --preset docs
```

## CMake Targets

`HandleDoxygenDocs.cmake` creates namespaced targets first:

```text
<LIB_NAMESPACE>_doc
<LIB_NAMESPACE>_doc_clean
```

When the project is the top-level build, it also creates the conventional aliases:

```text
doc
doc_clean
```

Nested builds do not create these generic aliases.

## Input Scope

Doxygen input is explicit:

```text
README.md
src/
doc/
```

The generated Doxyfile excludes `lib/`, `doc/developments/`, build directories, and `_deps/`. Do not point `INPUT` at the repository root; that would include nested dependencies, internal notes, vendored code, and generated artifacts.

## GitHub Pages

`.github/workflows/docs_pages.yml` builds the Doxygen HTML site and uploads
`build_docs/doc/html` as a Pages artifact. The same workflow is inherited by a
tailored project; generic workflow structure and template documentation
conformance are tested externally by `cpp_cuda_template_testfield`.

Pull requests build and upload the artifact for inspection but do not deploy. Manual `workflow_dispatch` runs are build-only by default; set `deploy_pages=true` to publish intentionally. Default-branch pushes deploy to the `github-pages` environment.

`actions/configure-pages` runs only in the deploy job. This keeps pre-merge and manual build-only checks useful even when the repository has not yet enabled Pages.

After deployment, the workflow fetches the published Pages URL and checks that the served index contains the expected documentation links. This guards against successful artifact upload with a broken or stale deployed site.

Required repository setting:

```text
Settings > Pages > Build and deployment > Source: GitHub Actions
```

## Verification

Build the documentation from a fresh preset before publishing:

```bash
cmake --preset docs
cmake --build --preset docs
test -f build_docs/doc/html/index.html
test -d build_docs/doc/xml
```

After a Pages deployment, check:

1. The workflow run passed.
2. The uploaded artifact contains `index.html`.
3. The deployment URL serves the project title and expected docs headings.

If the Pages setting, authentication, network, or deployment state blocks verification, report the failing command, error excerpt, likely owner, and next action.
