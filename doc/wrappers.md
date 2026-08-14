# Python and MATLAB Wrapper Guide

Wrappers are generated from gtwrap interface files. The default top-level interface is `src/wrap_interface.i`; implementation classes intended for binding live under `src/wrapped_impl/`.

## Wrapper Options

Wrapper options are namespaced by `LIB_NAMESPACE`, which prevents nested template-derived projects from colliding.

| Option | Purpose |
|---|---|
| `<namespace>_BUILD_PYTHON_WRAPPER` | Build the pybind11 wrapper |
| `<namespace>_BUILD_MATLAB_WRAPPER` | Build the MATLAB MEX wrapper |
| `<namespace>_WRAPPER_INTERFACE_FILES` | Ordered list of `.i` files; first is the top module |
| `<namespace>_GTWRAP_TOP_NAMESPACE` | C++ namespace exposed at the Python/MATLAB module root |
| `<namespace>_GTWRAP_DEPENDENCY_TARGETS` | Additional build-order dependencies required before wrapper generation |
| `<namespace>_GTWRAP_RUNTIME_DEPENDENCY_TARGETS` | Direct project-owned shared runtime build targets packaged beside the Python wrapper |
| `<namespace>_GTWRAP_ROOT_DIR` | Local `wrap` checkout override |

`build_lib.sh -p` and `build_lib.sh -m` set the Python and MATLAB wrapper options for the main project.

## gtwrap Resolution

The wrapper resolver checks, in order:

1. An explicit `--gtwrap-root <dir>` or `<namespace>_GTWRAP_ROOT_DIR`.
2. Local `./wrap`, `./lib/wrap`, or adjacent checkout candidates.
3. An installed `gtwrap` CMake package.
4. A declared `wrap` or `lib/wrap` submodule when submodule initialization is enabled.

Wrapper checkout maintenance is disabled by default. Explicitly update a
resolved local checkout with:

```bash
./build_lib.sh -p --wrap-update
```

Explicitly initialize a declared submodule fallback with:

```bash
./build_lib.sh -p --wrap-submodule-init
```

Direct CMake callers must set both `GTWRAP_MAINTENANCE_UPDATE=ON` and
`GTWRAP_SYNC_TO_MASTER=ON` to update a checkout. Initialization is limited to a
`wrap` or `lib/wrap` entry already declared in `.gitmodules`; use Git directly
when intentionally adding a new submodule.

## Python Package

The source package under `python/<project>/` is immutable wrapper input. CMake
copies it into `<build>/python/<project>` and configures `pyproject.toml` plus
`setup.py` beside that staged package. Building the wrapper target validates and
stages its native runtime set, then writes build-only `_wrapper_build.py`
metadata for build-tree imports and wheel construction.

```bash
./build_lib.sh -p
cd build/python
python -m pip install .
python -c "import template_project; assert template_project.HAS_WRAPPER"
```

The package requires Python 3.12 or newer by default. Adjust `PROJECT_PYTHON_VERSION` in the root `CMakeLists.txt` and `requires-python` in `python/pyproject.toml.in` together.

The main project shared library is packaged automatically. List additional
direct project-owned `SHARED_LIBRARY` or `MODULE_LIBRARY` build targets in
`<namespace>_GTWRAP_RUNTIME_DEPENDENCY_TARGETS`; CMake rejects imported,
static, interface, or missing targets rather than scanning arbitrary build
directories. CMake resolves configuration- and platform-specific target and
SONAME filenames, including generator-expression output names, before one
serialized staging operation validates the complete flat package namespace.
The wrapper build fails before copying anything when different owners resolve
to the same destination. CMake alias target names are not accepted. System
libraries remain the responsibility of the target platform.

Each build tree owns its staged package and `_wrapper_build.py`. Separate build
directories can therefore package different configurations without competing
for generated files in the checkout.

Wrapper install destinations remain relative to `CMAKE_INSTALL_PREFIX`.
In particular, keep `CMAKE_INSTALL_LIBDIR` relative when Python wrapping is
enabled; an absolute value is rejected rather than allowing CMake installation
to escape a user-selected prefix.

Production responsibilities are separated across `HandleWrapper.cmake`
(gtwrap discovery and orchestration), `HandlePythonWrapper.cmake`,
`HandleMatlabWrapper.cmake`, and `StagePythonRuntimeArtifacts.cmake`. All four
modules are retained by project tailoring.

## MATLAB Wrapper

The MATLAB wrapper needs a MATLAB installation visible to CMake. Use the same local `wrap` checkout as Python when validating both wrapper types.

```bash
./build_lib.sh -m --gtwrap-root /path/to/wrap
```

MATLAB wrapper tests should include construction, method dispatch, caught error recovery, and teardown through `clear classes` and `clear mex`.

## Docstrings

Set `GTWRAP_ADD_DOCSTRINGS=ON` together with `BUILD_DOC_XML=ON` to generate Python docstrings from the top project Doxygen XML.

```bash
cmake -S . -B build_wrap_docs \
  -D template_project_BUILD_PYTHON_WRAPPER=ON \
  -D GTWRAP_ADD_DOCSTRINGS=ON \
  -D BUILD_DOC_XML=ON
cmake --build build_wrap_docs --target template_project_py
```

The XML source is the build-tree `doc/xml` directory for the project being built. It does not use `${CMAKE_SOURCE_DIR}/xml`, so nested template-derived libraries cannot leak their docs into the top project wrapper generation.
