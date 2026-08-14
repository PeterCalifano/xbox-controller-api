# MATLAB wrap crash investigation and remediation plan

## Scope

This note records the current MATLAB wrapper crash investigation for the template
and for the `wrap` generator used by downstream repositories.

Observed failure class:

- MATLAB segfaults during wrapped method calls.
- MATLAB reports allocator corruption or invalid frees during `clear all`, `clear mex`,
  class clearing, or process exit.
- Failures are worse in stateful wrappers that own CUDA, OptiX, OpenCV, or other
  process-sensitive resources.

This document separates:

- `wrap` generator/header bugs.
- Template CMake policy bugs.
- Downstream wrapped-library lifecycle bugs.

## Current workspace state

Probe-only changes are intentionally kept in this template checkout:

- `src/wrapped_impl/CWrapperPlaceholder.h`
- `src/wrapped_impl/CWrapperPlaceholder.cpp`
- `src/wrap_interface.i`

The probe API adds:

- `getTextData()`
- `setTextDataByConstRef(const std::string &)`
- `setTextDataByValue(std::string)`
- `echoFrameId(std::uint32_t)`
- `printToStdout()`

Purpose:

- Reproduce string reference marshalling failures.
- Prove by-value string path works.
- Reproduce fixed-width integer marshalling failures.
- Probe `std::cout` redirection behavior.

The template CMake tcmalloc policy fix is now applied in the template and synced
into the testfield harness. Normal wrapper builds do not link `libtcmalloc`;
`-DENABLE_TCMALLOC=ON` keeps explicit opt-in behavior.

## Summary findings

| ID | Symptom | Primary owner | Cause | Current status |
| --- | --- | --- | --- | --- |
| W1 | MATLAB segfault on string setters | `wrap` | `const string&` generated as `unwrap_shared_ptr<string>` | Confirmed with probe |
| W2 | MATLAB proxy rejects `uint32`, direct MEX crashes | `wrap` | `uint32_t` parsed as custom object type | Confirmed with probe |
| W3 | Null pointer dereference in MEX | `wrap` | `unwrap_shared_ptr` does not validate `mxGetProperty` result | Confirmed by crash stack |
| W4 | Potential memory corruption after MEX error path | `wrap` | `std::cout.rdbuf()` restore can be bypassed by `mexErrMsg*` | Code-level hazard confirmed |
| T1 | `clear all` / exit allocator failure | Template CMake | tcmalloc linked into normal shared library/MEX dependency tree | Confirmed with original build and isolated no-tcmalloc build |
| D1 | Resource teardown leaks or crashes | Downstream repos | Destructors do not always call no-throw ordered cleanup | Confirmed in spectral raytracer case, not template-specific |
| D2 | MEX load failure without `LD_PRELOAD` | Downstream dependency stack | OpenCV/OpenEXR linkage mismatch | Confirmed in spectral raytracer case, not template-specific |

## Implementation checklist and current status

### Stage 0 - baseline and guardrails

- [x] Confirmed template dirty state was probe-only before implementation:
  `src/wrap_interface.i`, `src/wrapped_impl/CWrapperPlaceholder.cpp`,
  `src/wrapped_impl/CWrapperPlaceholder.h`, and existing `doc/developments/`.
- [x] Confirmed `/home/peterc/devDir/dev-tools/wrap` was clean before edits.
- [x] Reused the recorded MATLAB baseline failures in this document:
  by-value string passed, `const string&` crashed, direct `uint32_t` MEX call
  crashed, and default `clear all` could fail with tcmalloc.
- [x] Kept downstream renderer repos out of this pass.

### Stage 1 - `wrap` type classification and MATLAB codegen

- [x] Treated `string` and `std::string` as scalar MATLAB string types.
- [x] Generated local `string value = unwrap<string>(...)` for
  `const string&` and `const std::string&`.
- [x] Rejected non-const `string&` at generator time.
- [x] Added fixed-width scalar integer support:
  `int8_t`, `uint8_t`, `int16_t`, `uint16_t`, `int32_t`, `uint32_t`,
  `int64_t`, `uint64_t`, and `std::int*_t` / `std::uint*_t` spellings.
- [x] Mapped MATLAB proxy checks to MATLAB classes such as `uint32`, not
  C++ spellings such as `uint32_t`.
- [x] Added generator fixtures:
  `/home/peterc/devDir/dev-tools/wrap/tests/fixtures/matlab_scalar_contracts.i`
  and
  `/home/peterc/devDir/dev-tools/wrap/tests/fixtures/matlab_invalid_string_ref.i`.
- [x] Updated expected MATLAB wrapper outputs under
  `/home/peterc/devDir/dev-tools/wrap/tests/expected/matlab/`.
- [x] Verified static output has no `unwrap_shared_ptr< string >` or
  `unwrap_shared_ptr< uint32_t >` for scalar/string APIs and proxy checks use
  `isa(...,'uint32')`.

### Stage 2 - `wrap` MATLAB runtime header

- [x] Added fixed-width integer `wrap<>` / `unwrap<>` scalar support in
  `/home/peterc/devDir/dev-tools/wrap/matlab.h`.
- [x] Added null checks in `unwrap_shared_ptr`.
- [x] Added null checks in `unwrap_ptr`.
- [x] Routed invalid MATLAB input toward catchable MEX errors instead of raw
  segmentation faults.

### Stage 3 - `wrap` stdout and error-path safety

- [x] Added stdout restoration helpers in `/home/peterc/devDir/dev-tools/wrap/matlab.h`.
- [x] Changed generated `mexErrMsg*` paths to restore `std::cout` before raising
  MATLAB errors.
- [x] Changed generated `_deleteAllObjects` warning path away from stack-local
  `std::cout.rdbuf(&mout)` lifetime hazards.
- [x] Verified MATLAB error recovery:
  valid stdout call, forced bad direct MEX call, catch MATLAB error, valid stdout
  call again, clear object/classes/MEX.

### Stage 4 - template allocator policy

- [x] Added `ENABLE_GPERFTOOLS`, defaulting to `ENABLE_PROFILING`.
- [x] Added `ENABLE_TCMALLOC`, defaulting to `OFF`.
- [x] Gated profiler detection/linking behind `ENABLE_GPERFTOOLS`.
- [x] Gated tcmalloc detection/linking behind `ENABLE_TCMALLOC`.
- [x] Kept profiling flags controlled by `ENABLE_PROFILING`.
- [x] Linked profiling interface privately by default so allocator/profiling
  choices do not leak into consumers unless explicitly requested.
- [x] Updated template README/build documentation.

### Stage 5 - template MATLAB regression tests

- [x] Added CTest helper:
  `tests/cmake/AddMatlabWrapperRegressionTests.cmake`.
- [x] Added ELF dependency gate:
  `tests/cmake/CheckTcmallocDependency.cmake`.
- [x] Added MATLAB regression runner:
  `tests/matlab/RunTemplateWrapperRegression.m`.
- [x] Added separate MATLAB process tests for load/construct/clear.
- [x] Added live-exit test without explicit cleanup.
- [x] Added string by-value setter round-trip.
- [x] Added string const-ref setter round-trip.
- [x] Added `uint32_t` round-trip.
- [x] Added bad-input catch and valid-call-after-catch recovery.
- [x] Added stdout/error recovery.
- [x] Added `clear all` lifecycle.
- [x] Added default no-tcmalloc ELF gate.
- [x] Added opt-in tcmalloc-present ELF gate.

### Stage 6 - verification matrix completed

- [x] `wrap`: `pytest -q tests` passed with `97 passed`.
- [x] `wrap`: `pytest -q tests/test_matlab_wrapper.py` passed with `12 passed`.
- [x] `wrap`: plain `pytest -q` caveat recorded; vendored `pybind11/tests`
  collect `pybind11_tests`, which is not built by this project test command.
- [x] Template default MATLAB wrapper build in
  `/tmp/cpp_cuda_template_wrap_default` passed full CTest with `14/14`.
- [x] Template default MEX and project shared library had no `libtcmalloc` or
  profiler dependency by `ldd` / `readelf`.
- [x] Template opt-in tcmalloc build in
  `/tmp/cpp_cuda_template_wrap_tcmalloc` linked `libtcmalloc.so.4`.
- [x] Template opt-in tcmalloc ELF CTest passed.
- [x] Template Python wrapper build in
  `/tmp/cpp_cuda_template_wrap_python` passed CTest with `6/6`, including
  `template_project_python_import`.
- [x] Template direct Python smoke passed for construction, double setter/getter,
  string by-value, string const-ref, `echoFrameId`, and `multiplyBy2`.

### Stage 7 - testfield regression harness

- [x] Synced testfield wrapper probe surface to the same string and `uint32_t`
  contract.
- [x] Synced testfield profiling/tcmalloc policy.
- [x] Added automatic CMake test:
  `template_project_matlab_wrapper_script_runs`, which builds and runs the
  sibling `cpp_cuda_template_project` MATLAB wrapper.
- [x] Added automatic CMake test:
  `testfield_matlab_wrapper_script_runs`, which builds and runs the testfield
  wrapper against the same probe contract.
- [x] Added reusable CMake driver:
  `/home/peterc/devDir/dev-tools/cpp_cuda_template_testfield/tests/cmake/VerifyMatlabWrapperSmoke.cmake`.
- [x] Added reusable MATLAB smoke:
  `/home/peterc/devDir/dev-tools/cpp_cuda_template_testfield/tests/matlab/RunMatlabWrapperSmoke.m`.
- [x] Testfield new MATLAB CTests passed with `2/2`.
- [x] Testfield full CTest suite passed with `21/21`.
- [x] Testfield default template-built and testfield-built MATLAB wrappers had no
  `libtcmalloc` or profiler ELF dependency.
- [x] Testfield opt-in `-DENABLE_TCMALLOC=ON` build linked `libtcmalloc.so.4`.
- [x] Testfield Python wrapper import CTest passed.
- [x] Testfield direct Python smoke passed for construction, double setter/getter,
  string by-value, string const-ref, `echoFrameId`, and `multiplyBy2`.
- [x] Testfield Python package CTests passed:
  Python 3.12 install/import succeeds and Python 3.11 install is rejected.

### Stage 8 - critical follow-up fixes

- [x] Fixed `wrap` MATLAB test collection from the repo root by appending the
  repo root to `sys.path` before importing `gtwrap`.
- [x] Regenerated committed MATLAB expected fixtures from the actual generator
  output so `tests/test_matlab_wrapper.py` is self-contained.
- [x] Kept runtime and generated MEX error-path changes together:
  generated code now depends on `gtwrap::CoutRedirect` and `gtwrap::MexErrMsg*`
  helpers from `/home/peterc/devDir/dev-tools/wrap/matlab.h`.
- [x] Made `CoutRedirect` nested/reentrant safe with a previous-pointer stack
  model instead of a single active redirect pointer.
- [x] Restored every active stdout redirect before `mexErrMsg*` longjmp paths.
- [x] Genericized user-facing generated diagnostics:
  `Exception from wrapped C++ code` replaces `Exception from gtsam`.
- [x] Genericized user-facing RTTI errors from `gtsam wrap:` to `wrap:`.
- [x] Genericized generated MATLAB comments away from hard-coded
  `https://gtsam.org/doxygen/`.
- [x] Replaced old `FindPythonInterp` / `FindPythonLibs` usage in
  `wrap/cmake/MatlabWrap.cmake` with modern `find_package(Python ... COMPONENTS
  Interpreter)`.
- [x] Re-verified Python wrapper generation after MATLAB changes:
  `pytest -q tests/test_pybind_wrapper.py -p no:cacheprovider` passed.

### Stage 9 - downstream experiments with fixed local `wrap`

- [x] Built `/home/peterc/devDir/rendering-sw/spectral_raytracer_dev` against
  `/home/peterc/devDir/dev-tools/wrap` with MATLAB wrapper enabled and wrap
  auto-update disabled.
- [x] Confirmed generated spectral MEX uses scalar paths:
  `unwrap< std::string >` for by-value strings,
  `unwrap< uint32_t >` for `uint32_t`, and MATLAB proxy `isa(...,'uint32')`.
- [x] Confirmed spectral generated MEX no longer emits
  `Exception from gtsam`.
- [x] Ran spectral MATLAB smoke with `LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libOpenEXR-3_1.so.30`:
  const-ref string setters passed, by-value string setter passed,
  `uint32_t` overload gate passed, bad C++ call was catchable, and valid calls
  still worked after catch.
- [x] Recorded spectral environmental blocker without preload:
  MATLAB loads OpenEXR from its own runtime before `/usr/local/lib` OpenCV,
  causing an unresolved OpenEXR symbol in `libopencv_imgcodecs.so.410`.
- [x] Added minimal torchAutoForge wrapper probe in
  `/home/peterc/devDir/ML-repos/torchAutoForge-deploy/src/wrap_interface.i`
  for existing `placeholder::placeholder_fcn()`.
- [x] Built torchAutoForge Python wrapper against fixed local `wrap` with
  `autoforge_deploy_GTWRAP_TOP_NAMESPACE=placeholder`.
- [x] Ran torchAutoForge Python smoke with Python 3.11 and called the wrapped
  C++ placeholder function.
- [x] Built torchAutoForge MATLAB wrapper against fixed local `wrap` with
  `autoforge_deploy_GTWRAP_TOP_NAMESPACE=placeholder`.
- [x] Ran torchAutoForge MATLAB smoke and called
  `placeholder.placeholder_fcn()` through MEX.

### Remaining to do / suggested follow-ups

- [ ] Decide whether probe methods stay in `CWrapperPlaceholder` or move to a
  dedicated wrapper contract fixture class.
- [ ] Add a burst MATLAB lifecycle test that repeats construct/use/clear 10 to
  50 times in one MATLAB process.
- [ ] Add a CI-friendly skip policy for MATLAB tests when MATLAB is unavailable,
  while still keeping non-MATLAB tests active.
- [ ] Add an installed-package Python smoke test in the template itself, matching
  the current testfield package-install regression.
- [ ] Parameterize generated wrapper diagnostics so user-facing MEX/Python errors
  can optionally name the generated module or project. The current fix uses
  generic wording, not module-specific wording.
- [ ] Split user-facing diagnostic text from runtime compatibility symbols:
  changing messages should be cheap, while changing registry/global names must
  preserve existing wrapper compatibility.
- [ ] Add a configurable MATLAB RTTI registry prefix for generic wrapped modules,
  with backward-compatible defaults for existing wrappers.
- [ ] Split generic MATLAB runtime support from project-specific adapters for
  vector, matrix, point, serialization, redirect, and RTTI helpers.
- [ ] Split generic Python/pybind runtime support from project-specific adapters
  such as redirected `print` / `repr` helpers and serialization utilities.
- [ ] Keep backward aliases or migration gates for existing wrappers that depend
  on legacy runtime names.
- [x] Add regression checks that generated MATLAB wrappers for generic modules
  do not emit unrelated project names in user-facing errors.
- [ ] Add matching Python generated-output checks for unrelated project names
  where Python generator support is expected to be generic.
- [ ] Upstream or vendor-sync the fixed local `/home/peterc/devDir/dev-tools/wrap`
  changes after review.
- [x] Run affected downstream repositories against the fixed local `wrap`.
- [ ] For stateful downstream objects, add no-throw idempotent cleanup on delete
  and MATLAB unload paths.
- [ ] Run a narrow MATLAB C++ MEX API spike only after the hardened C API backend
  is stable.

## Historical baseline probe details

The probe declarations in `src/wrap_interface.i`:

```cpp
string getTextData() const;
void setTextDataByConstRef(const string& charValue);
void setTextDataByValue(string charValue);
uint32_t echoFrameId(uint32_t ui32FrameId) const;
void printToStdout() const;
```

Generated MEX code for the bad string reference path:

```cpp
string& charValue = *unwrap_shared_ptr< string >(in[1], "ptr_string");
obj->setTextDataByConstRef(charValue);
```

Generated MEX code for the good by-value string path:

```cpp
string charValue = unwrap< string >(in[1]);
obj->setTextDataByValue(charValue);
```

Generated MEX code for `uint32_t`:

```cpp
std::shared_ptr<uint32_t> ui32FrameId =
    unwrap_shared_ptr< uint32_t >(in[1], "ptr_uint32_t");
out[0] = wrap_shared_ptr(
    std::make_shared<uint32_t>(obj->echoFrameId(*ui32FrameId)),
    "uint32_t",
    false);
```

Generated MATLAB proxy code for `uint32_t`:

```matlab
if length(varargin) == 1 && isa(varargin{1},'uint32_t')
```

That MATLAB class name is not valid for normal MATLAB numeric values. The normal
class is `uint32`, not `uint32_t`.

## How it was tested

### Build

Regenerated and rebuilt MATLAB wrapper:

```bash
cmake --build build --target template_project_matlab_wrapper
```

Native tests:

```bash
ctest --test-dir build --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 6
```

### MATLAB by-value string path

Command:

```bash
matlab -batch "addpath('/home/peterc/devDir/dev-tools/cpp_cuda_template_project/build/wrap/template_project'); addpath('/home/peterc/devDir/dev-tools/cpp_cuda_template_project/build/wrap/template_project_mex'); obj = cpp_playground.CWrapperPlaceholder(); obj.setTextDataByValue('value_ok'); assert(strcmp(obj.getTextData(), 'value_ok')); obj.printToStdout(); clear obj; clear classes; clear mex;"
```

Result:

```text
CWrapperPlaceholder text=value_ok
```

Exit code: 0.

### MATLAB const string reference path

Command:

```bash
matlab -batch "addpath('/home/peterc/devDir/dev-tools/cpp_cuda_template_project/build/wrap/template_project'); addpath('/home/peterc/devDir/dev-tools/cpp_cuda_template_project/build/wrap/template_project_mex'); obj = cpp_playground.CWrapperPlaceholder(); obj.setTextDataByConstRef('boom');"
```

Result:

```text
Segmentation violation
...
libmx.so ... mxGetClassID
template_project_wrapper.mexa64 ... mexFunction
```

Root cause:

- MATLAB passes a normal `char`.
- `unwrap_shared_ptr<string>` calls `mxGetProperty` for `ptr_string`.
- `mxGetProperty` returns null.
- `mxGetClassID(NULL)` segfaults.

### MATLAB uint32_t path through proxy

Command:

```bash
matlab -batch "addpath('/home/peterc/devDir/dev-tools/cpp_cuda_template_project/build/wrap/template_project'); addpath('/home/peterc/devDir/dev-tools/cpp_cuda_template_project/build/wrap/template_project_mex'); obj = cpp_playground.CWrapperPlaceholder(); try, obj.echoFrameId(uint32(7)); catch ME, disp(ME.identifier); disp(ME.message); end; clear obj; clear classes; clear mex;"
```

Result:

```text
Arguments do not match any overload of function cpp_playground.CWrapperPlaceholder.echoFrameId
```

Cause:

- Proxy checks `isa(x,'uint32_t')`.
- MATLAB value class is `uint32`.

### MATLAB uint32_t direct MEX path

Command:

```bash
matlab -batch "addpath('/home/peterc/devDir/dev-tools/cpp_cuda_template_project/build/wrap/template_project'); addpath('/home/peterc/devDir/dev-tools/cpp_cuda_template_project/build/wrap/template_project_mex'); obj = cpp_playground.CWrapperPlaceholder(); template_project_wrapper(3, obj, uint32(7));"
```

Result:

```text
Segmentation violation
...
libmx.so ... mxGetClassID
template_project_wrapper.mexa64 ... mexFunction
```

Cause:

- Generated C++ expects `ptr_uint32_t` object property.
- MATLAB `uint32` scalar has no such property.
- `unwrap_shared_ptr<uint32_t>` dereferences null.

### MATLAB clear-all allocator path

Original build dependency check:

```bash
ldd build/wrap/template_project_mex/template_project_wrapper.mexa64
readelf -d build/src/libtemplate_project.so
```

Observed dependency:

```text
libtcmalloc.so.4
```

Crash command:

```bash
matlab -batch "addpath('/home/peterc/devDir/dev-tools/cpp_cuda_template_project/build/wrap/template_project'); addpath('/home/peterc/devDir/dev-tools/cpp_cuda_template_project/build/wrap/template_project_mex'); obj = cpp_playground.CWrapperPlaceholder(); clear all;"
```

Result:

```text
src/tcmalloc.cc:304] Attempt to free invalid pointer ...
MATLAB is exiting because of fatal error
```

Isolation build:

```bash
cmake -S . -B /tmp/cpp_cuda_template_no_tcmalloc \
  -DGTWRAP_BUILD_MATLAB_DEFAULT=ON \
  -Dtemplate_project_BUILD_MATLAB_WRAPPER=ON \
  -DGTWRAP_BUILD_PYTHON_DEFAULT=OFF \
  -Dtemplate_project_BUILD_PYTHON_WRAPPER=OFF \
  -Dtemplate_project_GTWRAP_ROOT_DIR=/home/peterc/devDir/dev-tools/wrap \
  -DGTWRAP_SYNC_TO_MASTER=OFF \
  -DGPERFTOOLS_PROFILER_LIBRARY=GPERFTOOLS_PROFILER_LIBRARY-NOTFOUND \
  -DGPERFTOOLS_TCMALLOC_LIBRARY=GPERFTOOLS_TCMALLOC_LIBRARY-NOTFOUND \
  -DENABLE_TESTS=OFF
cmake --build /tmp/cpp_cuda_template_no_tcmalloc --target template_project_matlab_wrapper
```

No-tcmalloc dependency check:

```bash
ldd /tmp/cpp_cuda_template_no_tcmalloc/wrap/template_project_mex/template_project_wrapper.mexa64
readelf -d /tmp/cpp_cuda_template_no_tcmalloc/src/libtemplate_project.so
```

Result:

- No `libtcmalloc.so.4`.
- Same `clear all` MATLAB command exited 0.
- Same const-ref string probe still crashed, proving allocator and marshalling bugs are independent.

## Upstream `wrap` check

Checked local fork:

```text
/home/peterc/devDir/dev-tools/wrap
origin/master = fc811c127d7e66442be2a3bc71c3012a88ac9202
```

Checked borglab upstream without editing local external checkout:

```text
https://github.com/borglab/wrap.git
master = 17404733ea315bd8c6cb06341eb153064e859794
```

Upstream clone used for read-only inspection:

```text
/tmp/tmp.YxlXtHRZVN/wrap_upstream
```

### Upstream still has string reference bug

Upstream generator treats any non-primitive reference as shared pointer:

```python
elif self.is_ref(arg.ctype):  # and not constructor:
    arg_type = "{ctype}&".format(ctype=ctype_sep)
    unwrap = '*unwrap_shared_ptr< {ctype} >(in[{id}], "ptr_{ctype_camel}");'
```

Upstream primitive list does not include `string`, so `const string&` takes that
reference branch:

```python
not_ptr_type = (
    "int",
    "double",
    "bool",
    "char",
    "unsigned char",
    "size_t",
    "Key",
)
```

Upstream expected MATLAB wrapper output already contains the problematic pattern:

```cpp
string& s = *unwrap_shared_ptr< string >(in[0], "ptr_string");
string& name = *unwrap_shared_ptr< string >(in[1], "ptr_string");
```

Conclusion:

- This is not only local fork behavior.
- Upstream expected-output tests encode the broken generated output.
- Runtime MATLAB crash tests are missing upstream.

### Upstream still has fixed-width integer gap

Upstream parser basic types:

```python
"void",
"bool",
"unsigned char",
"char",
"int",
"size_t",
"double",
"float",
```

Missing:

- `int8_t`
- `uint8_t`
- `int16_t`
- `uint16_t`
- `int32_t`
- `uint32_t`
- `int64_t`
- `uint64_t`
- `std::int*_t`
- `std::uint*_t`

Upstream `matlab.h` adds `uint64_t` `wrap`/`unwrap` helpers for `gtsam::Key`, but
that does not solve direct `uint64_t` or `uint32_t` interface signatures unless
the generator emits `unwrap<uint64_t>` / `wrap<uint64_t>`.

Conclusion:

- `uint32_t` remains broken upstream.
- `uint64_t` helper exists upstream, but generator classification is still incomplete.
- Alias `Key` is supported separately; fixed-width C++ scalar types are not.

### Upstream still lacks null guard

Upstream `matlab.h`:

```cpp
mxArray* mxh = mxGetProperty(obj,0, propertyName.c_str());
if (mxGetClassID(mxh) != mxUINT32OR64_CLASS || mxIsComplex(mxh)
```

No null check exists between `mxGetProperty` and `mxGetClassID`.

Conclusion:

- Bad generator output becomes MATLAB process crash instead of catchable MATLAB error.
- Null guard must be added even after generator fixes, because it hardens all object unwrap paths.

### Upstream still has error-unsafe cout redirection

Upstream generated `_deleteAllObjects` redirects `std::cout` to stack `mstream`:

```cpp
mstream mout;
std::streambuf *outbuf = std::cout.rdbuf(&mout);
...
std::cout.rdbuf(outbuf);
```

Upstream generated `mexFunction` also redirects `std::cout` before code paths that
can call `mexErrMsgTxt`:

```cpp
mstream mout;
std::streambuf *outbuf = std::cout.rdbuf(&mout);
...
catch(const std::exception& e) {
  mexErrMsgTxt(("Exception from gtsam:\n" + std::string(e.what()) + "\n").c_str());
}
std::cout.rdbuf(outbuf);
```

Conclusion:

- Any MEX error path can bypass restoration.
- This can leave `std::cout` pointing to dead stack storage.
- This is especially dangerous during later teardown or next MEX call.

## Fix plan by owner

### `wrap`: scalar and string marshalling

Files:

- `gtwrap/interface_parser/tokens.py`
- `gtwrap/matlab_wrapper/mixins.py`
- `gtwrap/matlab_wrapper/wrapper.py`
- `matlab.h`
- `tests/fixtures/*.i`
- `tests/expected/matlab/*.cpp`
- MATLAB runtime tests, if test harness supports MATLAB.

Required behavior:

- `std::string` / `string` by value: keep `unwrap<string>`.
- `const std::string&` / `const string&`: generate local `string` from MATLAB char and pass it by lvalue.
- Non-const `std::string&`: either generate local `string` and document that mutations do not propagate, or reject with generator error. Safer first fix: reject non-const string references unless a real mutable MATLAB string-object contract is designed.
- Fixed-width integers: classify as scalar numeric types, not custom objects.
- Generated MATLAB proxy uses MATLAB classes:
  - `uint8`, `int8`, `uint16`, `int16`, `uint32`, `int32`, `uint64`, `int64`
  - not `uint32_t`.
- Generated C++ uses:
  - `unwrap<std::uint32_t>` or `unwrap<uint32_t>`
  - `wrap<std::uint32_t>` or `wrap<uint32_t>`
  - not `unwrap_shared_ptr<uint32_t>`.

Test additions:

- Interface fixture with `string`, `const string&`, and rejected/non-rejected `string&`.
- Interface fixture with all fixed-width integer inputs and returns.
- Expected MATLAB proxy checks for valid MATLAB class names.
- Expected generated C++ checks no `unwrap_shared_ptr<string>` or `unwrap_shared_ptr<uint32_t>` appears for scalar/string types.
- Runtime `matlab -batch` tests for string setter and fixed-width integer round-trip.

### `wrap`: null-safe unwrap

File:

- `matlab.h`

Required behavior:

- `unwrap_shared_ptr` checks:
  - `obj != nullptr`
  - `mxGetProperty(...) != nullptr`
  - class id, complexity, dimensions
  - pointer payload non-null
- `unwrap_ptr` gets same guard.
- Error should be `mexErrMsgIdAndTxt`, not segfault.

Test additions:

- Direct MEX bad-type call returns catchable MATLAB error.
- After caught error, next valid wrapped method still works.
- `clear mex` after caught error exits cleanly.

### `wrap`: MEX output redirection safety

Files:

- `gtwrap/matlab_wrapper/templates.py`
- Possibly `matlab.h`

Required behavior:

- Do not hold stack-local `mstream` as global `std::cout` buffer across any code
  that can call `mexErrMsg*`.
- Restore `std::cout` before every generated `mexErrMsg*` path.
- Prefer a small generated helper for `throwMatlabErrorAfterRestore(...)`.
- Do not rely only on C++ RAII if `mexErrMsg*` long-jump behavior can skip C++
  destructors in a given MATLAB release/compiler mode.

Test additions:

- Force generated `checkArguments` error.
- Catch MATLAB error.
- Call `printToStdout()` again.
- Clear object/classes/MEX.
- Repeat in fresh MATLAB process.

### Template: tcmalloc gating

Files:

- `cmake/HandleProfiling.cmake`
- `CMakeLists.txt` if option propagation needs cleanup.
- `tests/cmake` or CTest scripts for ELF checks.
- Documentation under `README.md` / build docs.

Required behavior:

- `ENABLE_PROFILING=OFF`: no profiler, no tcmalloc link.
- `ENABLE_PROFILING=ON`: profiling flags enabled, profiler optional.
- `ENABLE_TCMALLOC=ON`: tcmalloc explicitly linked.
- MATLAB wrapper builds default to no tcmalloc.
- ELF gate fails if MEX dependency tree contains `libtcmalloc`.

Candidate implementation already tested then reverted:

- `ENABLE_GPERFTOOLS` defaulted to `ENABLE_PROFILING`.
- `ENABLE_TCMALLOC` defaulted to `OFF`.
- Detection/linking ran only when requested.

Why this belongs in template:

- `wrap` does not decide what allocator the project shared library links.
- Template currently builds libraries consumed by MEX.
- MEX loading makes process allocator policy much stricter than normal CLI runs.

### Downstream repos: stateful resource teardown

Files depend on repo. For spectral raytracer evidence:

- Renderer constructors create CUDA/OptiX resources.
- Destructor did not call full ordered cleanup.
- MATLAB-facing `cleanup()` had more complete teardown.

Required behavior:

- Public `cleanup()` may report errors.
- Destructor and MATLAB delete path call no-throw idempotent cleanup.
- Ordered teardown releases GPU streams, OptiX contexts, texture handles, buffers,
  file-save threads, and host resources exactly once.
- `clear all`, `clear classes`, `clear mex`, and MATLAB exit work even without explicit user cleanup.

## C API hardening vs new MATLAB C++ MEX API

Current `wrap` MATLAB backend uses C MEX API:

- `mxArray*`
- `mexFunction`
- `mexErrMsgTxt` / `mexErrMsgIdAndTxt`
- manual pointer handles in MATLAB proxy classes
- generated switch on numeric wrapper id

Modern MATLAB C++ MEX API uses:

- `matlab::mex::Function`
- `matlab::data::Array`
- typed array factories
- C++ exceptions and `matlabPtr->feval(...)`
- less direct `mxArray*` manipulation

### Option A: harden current C API backend

Benefits:

- Smallest change set.
- Directly fixes confirmed crashes.
- Preserves current generated MATLAB proxy model.
- Lower risk for existing downstream repositories.
- Compatible with older MATLAB releases than C++ MEX API.
- Easier to upstream as focused bug fixes.

Costs:

- Still manual `mxArray*` and pointer-handle code.
- Still must be careful with `mexErrMsg*` and global stream state.
- Type safety remains mostly generator-enforced, not API-enforced.

Feasibility:

- High.
- Best first step.

Expected fixes covered:

- W1 string reference marshalling.
- W2 fixed-width integers.
- W3 null guards.
- W4 safer error path.

### Option B: hybrid backend with current proxy contract and C++ MEX internals

Idea:

- Keep MATLAB `.m` proxy classes and numeric dispatch ids.
- Generate a C++ MEX API implementation internally.
- Convert inputs from `matlab::data::Array` instead of raw `mxArray*`.

Benefits:

- Better typed API inside generated MEX.
- Better C++ exception model.
- Less raw pointer API use for scalar/string arrays.
- Can preserve much of current MATLAB surface.

Costs:

- Still needs object-handle design.
- Still needs RTTI/proxy-object creation story.
- Existing `wrap_shared_ptr` / `create_object` MATLAB callback logic must be redesigned.
- More invasive than fixing current C API backend.

Feasibility:

- Medium.
- Good second-stage modernization after C API crash fixes and regression tests.

### Option C: full MATLAB C++ MEX API backend rewrite

Idea:

- New MATLAB backend for `wrap`.
- Generate `matlab::mex::Function` modules and modern array conversion utilities.
- Potentially redesign proxy creation, lifetime tracking, and object registry.

Benefits:

- Strongest long-term type and error semantics.
- Cleaner implementation for strings, numeric arrays, and exceptions.
- Better fit for C++17/C++20 template repos.
- Opportunity to remove legacy GTSAM-specific assumptions.

Costs:

- Large rewrite.
- High regression risk across all current wrapper users.
- Requires MATLAB version policy decision.
- Requires new generated expected outputs and runtime test suite.
- Still must solve C++ object lifetime and shared_ptr ownership explicitly.

Feasibility:

- Medium-low as immediate fix.
- Useful as dedicated modernization project, not as first crash fix.

### Recommendation

Do not start with full C++ MEX API rewrite.

Recommended order:

1. Fix current C API backend in `wrap` for strings, fixed-width ints, null guards, and error-safe output restoration.
2. Add isolated MATLAB process regression tests in this template.
3. Add template tcmalloc gating and ELF dependency gate.
4. Apply fixes to downstream wrapper builds and verify crash matrix.
5. Then run a focused C++ MEX API spike with one narrow class and compare generated complexity, lifetime behavior, and MATLAB version requirements.

Reason:

- Current crashes have specific, localized causes.
- C API hardening gives fast risk reduction.
- Regression suite created for C API fixes becomes migration safety net for any later C++ API backend.

## Proposed regression matrix

Each MATLAB test should run in a separate MATLAB process where fatal MEX crashes do
not kill the main test runner.

Required tests:

- Load-only:
  - add wrapper paths
  - construct object
  - clear object
  - clear classes
  - clear mex
  - exit 0
- Live exit:
  - construct object
  - let MATLAB exit without explicit cleanup
  - exit 0
- String by-value:
  - call `setTextDataByValue`
  - read back `getTextData`
  - clear MEX
- String const-ref:
  - call `setTextDataByConstRef`
  - must not crash after `wrap` fix
- Fixed-width integer:
  - call `echoFrameId(uint32(7))`
  - must return scalar `uint32` or documented numeric equivalent
- Error path:
  - deliberately call bad overload or direct MEX bad type
  - catch MATLAB error
  - call valid method
  - clear MEX
- Clear-all lifecycle:
  - construct object
  - call `clear all`
  - exit 0
- Burst lifecycle:
  - repeat construct/use/clear 10 to 50 times in one process
  - repeat fresh MATLAB processes
- ELF gate:
  - fail if MEX or linked project shared library needs `libtcmalloc`
- Downstream GPU lifecycle:
  - construct renderer
  - build minimal pipeline
  - render or allocate/free resources
  - clear without explicit cleanup
  - explicit cleanup path also works

## Decision points

- Keep probe methods in template as permanent regression fixture, or replace them
  with a dedicated wrapper-test class.
- Patch local `PeterCalifano/wrap` fork first, then upstream PR to `borglab/wrap`.
- Add template tcmalloc gating now, or keep it as downstream build flag workaround
  until `wrap` fixes land.
- Choose C++ MEX API spike scope after C API fixes:
  - one class,
  - strings,
  - fixed-width ints,
  - one shared_ptr object,
  - one error path,
  - one `clear mex` lifecycle.

## Historical immediate implementation plan

This section is preserved as the original execution outline. Current checkbox
status is tracked in the implementation checklist above.

Phase 1 - `wrap` minimal correctness:

- [x] Fix string reference generation.
- [x] Add fixed-width scalar type support.
- [x] Add `unwrap_shared_ptr` and `unwrap_ptr` null guards.
- [x] Add generated tests that fail on `unwrap_shared_ptr<string>` and
  `unwrap_shared_ptr<uint32_t>` for scalar/string APIs.

Phase 2 - template regression harness:

- [x] Convert current probe methods into stable wrapper regression fixture.
- [x] Add CTest entries that run separate `matlab -batch` processes.
- [x] Add clear-all and error-recovery tests.

Phase 3 - template allocator policy:

- [x] Add opt-in tcmalloc option.
- [x] Default MATLAB wrapper builds to no tcmalloc.
- [x] Add ELF gate.

Phase 4 - downstream validation:

- [ ] Rebuild affected downstream repos against fixed `wrap`.
- [ ] Run string setters, integer APIs, clear-all, live-exit, and burst lifecycle tests.
- [ ] Add no-throw cleanup where downstream objects own GPU/OptiX/OpenCV resources.

Phase 5 - C++ MEX API spike:

- [ ] Implement one generated prototype outside production path.
- [ ] Compare code size, lifetime semantics, MATLAB version constraints, and failure behavior.
- [ ] Decide whether to migrate backend or keep hardened C API backend.
