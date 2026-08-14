# Agents instructions

Write to CONTEXT.md the context before compaction to prevent data loss.
After auto-compaction, read again AGENTS.md and CONTEXT.md before restarting.
<!-- ros2-overlay-begin -->
## Optional ROS 2 Overlay

See `doc/ros2_overlay.md` before changing the optional ROS 2 overlay. `./build_lib.sh` is the C++-first library entry point and never needs ROS. `./build_ros2.sh` is the optional ROS 2 overlay build and test entry point.

Keep ROS-related changes confined to `ros2/` plus the documented root helpers, docs, tests, markers, and the single ROS overlay workflow.
<!-- ros2-overlay-end -->

## Language and programming standards

### Language-agnostic software engineering guidelines

- Follow the owning component's established conventions and keep each change
  within the smallest coherent scope that satisfies the requested behavior.
- Prefer small, cohesive functions and classes with explicit contracts. Add an
  abstraction only when it clarifies ownership, reuse, or a stable interface.
- Use descriptive names and keep one authoritative source for each policy or
  piece of state. Avoid hidden coupling and duplicated decision logic.
- Validate external inputs at system boundaries and report actionable failures.
  Do not silently fall back to behavior that changes the advertised contract.
- Test observable behavior, invariants, and failure modes rather than internal
  implementation details or tunable defaults.
- During review and optimization, actively seek behavior-preserving ways to
  reduce complexity and improve performance, maintainability, readability, and
  implementation clarity. Simplify unnecessary nested loops, helper functions,
  conditional branches, indirection, and abstractions that do not enforce a
  useful contract.
- Keep refactoring within the reviewed scope and preserve public behavior unless
  a contract change is explicitly requested. Make performance optimization
  evidence-driven through profiling, measurement, or algorithmic analysis, and
  document any tradeoff that increases complexity.
- Use 100 columns as a soft limit. Keep assignments and function calls on one
  line when they remain readable; otherwise wrap at semantic boundaries and
  align continuation lines with the expression they continue.
- Treat newlines as logical separators. Keep statements that implement the same
  small step together, and use a blank line between distinct steps.
- Introduce each non-obvious logical block with a concise comment describing its
  purpose, rationale, or invariant. Do not translate individual statements into
  prose.

### Python

- Use Python 3.12 or newer and follow PEP 8 for naming and formatting. Use
  `snake_case` for functions, methods, and variables, `PascalCase` for classes,
  and a leading underscore for internal APIs. Use a trailing underscore only to
  avoid a keyword or name collision.
- Follow PEP 257 and use Google-style docstrings for modules, public classes,
  public methods, and public functions. Document arguments, returns, raised
  exceptions, important invariants, and examples where applicable.
- Add precise type annotations to every function and method signature, class
  attribute, and dataclass field. Keep code suitable for static checking, avoid
  untyped definitions, and isolate or justify any unavoidable `Any` boundary.
- Prefer dataclasses to unstructured dictionaries for stable records. Prefer an
  enum to string or integer literals when a choice has more than two values.
- Prefer functions for stateless transformations and classes when state,
  ownership, or a durable behavioral interface is required.
- Use Matplotlib for general plots and prefer seaborn for statistical plots.
  Use Pillow or OpenCV for image-specific work as appropriate.
- Use PyTorch for machine-learning implementations, with scikit-learn for
  supporting workflows where useful. Preserve ONNX export compatibility for model APIs
  unless the task explicitly excludes it.
- When building libraries and complex functionalities, provide examples/demos with expected output to show usage, with relevant visualization/output data to verify it.

### C++ and CUDA

- Use the repository-configured C++20 standard by default and retain C++17
  compatibility only where the owning target explicitly requires it. Target
  CUDA 12.6 or newer unless a supported platform imposes another version.
- Use Doxygen file headers and Doxygen documentation for public classes,
  functions, and methods. Cover parameters, return values, template parameters,
  exceptions, ownership, and invariants where applicable.
- Prefer concepts over SFINAE. Prefer classes when invariants, ownership, or
  behavior must be enforced; use simple aggregate types only when aggregate
  semantics are the intended contract.
- Use Catch2 for C++ and CUDA unit tests and follow the naming conventions in
  the surrounding component.
- Keep an assignment and the beginning of its right-hand expression on the same
  line when the complete statement is readable within the soft limit. Apply the
  same rule to function names and their first arguments.
- For long expressions or argument lists, wrap at semantic operators or argument
  groups and align continuation lines. Do not mechanically place every term or
  argument on a separate line.
- Keep technical explanations concise and aimed at intermediate or advanced
  readers while defining ideas, practices and syntax when they affect the decision.
- Justify design choices when proposing them including choice of language featreus to implement a certain functionality among the considered alternatives.
- Follow C++ standards best practices and guidelines and Jason Turner suggested best practices when designing implementation.

## CMake and derived-project test policy

Do not copy template-conformance CMake verifiers into a derived project merely
because the donor template has them. In particular, do not register tests that
recursively configure and rebuild the same derived project inside its ordinary
CTest suite when a fresh configure/build/install/consumer command or CI job
already proves the contract.

For a derived project:

- prefer Catch2 or pytest for project runtime behavior;
- validate CMake options, headless/full feature matrices, installation,
  packaging, and external consumers through explicit fresh out-of-tree
  acceptance commands owned by local CI;
- use disposable consumer projects outside the normal test build when nested or
  installed consumption must be proven;
- add a permanent CMake-script test only when it is lightweight, target-owned,
  isolates behavior unavailable through an existing target/test, and does not
  recursively rebuild the project;
- never import `VerifyTemplateProject*` or other donor self-validation tests as
  product tests.

Generic template conformance is owned by the standalone harness in
`cpp_cuda_template_testfield`. The template repository itself keeps the same
runtime-oriented test layout inherited by derived projects.

## Build cleanup and wrapper packaging safety

- `build_lib.sh --clean` may remove only a conventional in-repository build
  path. An existing target must contain a `CMakeCache.txt` whose
  `CMAKE_HOME_DIRECTORY` resolves to this exact checkout.
- Never weaken clean-path or cache-ownership validation to accommodate an
  unusual build layout; use a non-clean configure or remove that directory
  explicitly after independent review.
- Generated Python wheels and CMake Python installs must co-locate declared
  non-system shared runtime targets and use loader-relative runtime paths.
- `_wrapper_build.py` is checkout-only metadata and must not be installed or
  included in a wheel.
- Keep CMake Python install destinations relative to
  `CMAKE_INSTALL_PREFIX`; pip owns installation into an active environment.
- Wrapper checkout updates, submodule initialization, and submodule creation
  are explicit maintenance operations. Ordinary configure and build commands
  must not move the wrapper checkout or change the parent repository gitlinks.

For MATLAB: Use classes a lot also in MATLAB, with a python style, but do it only when it makes sense. Functions in MATLAB are often more efficient. Evaluate whether it makes sense to have stateful implementation. Use "self" instead of "obj". All variables names must specify the datatype of the variable since MATLAB does not (hungarian notation). The following list applies: d for double, f for float, b for bool, str for struct and not for strings, char for strings and chars, ui8 for uint8, i8 for int8.  All the other integers are similar to the latter. Specify "obj" as prefix if an object, cell if a cell, table if a table; "bus_" if a Simulink bus. The names are always in Pascal case including the prefix, for instance ui8MyVariable. Never nest functions definitions within other functions, always do them separate or at most in the same file (after the main function implementation). Add them as local in the same function file only when not re-used elsewhere, otherwise prefer a single implementation. Function names and static methods of classes starts with Capital letter. Local functions names ends with underscore meaning "private". Names of variables must be explicative and tell what the variable does. Short names are not allowed unless "very local in scope". Use underscore for those variables and preferably Tmp within the name. For codes that are intended to be algorithms of some kind (e.g. not plots or things to run on the host PC), make them always MATLAB codegen safe (especially if codegen directive is used). In that case names should be limited to 31 chars. Add the same template of doc to functions as below and always specify arguments-end block for input and output:
%% SIGNATURE
%
% -------------------------------------------------------------------------------------------------------------
%% DESCRIPTION
% -------------------------------------------------------------------------------------------------------------
%% INPUT
% -------------------------------------------------------------------------------------------------------------
%% OUTPUT
% -------------------------------------------------------------------------------------------------------------
%% CHANGELOG
% DD-MM-YYYY  Pietro Califano     First prototype.
% -------------------------------------------------------------------------------------------------------------
%% DEPENDENCIES
%
% -------------------------------------------------------------------------------------------------------------

%% Function code

## Commit and staged-review workflow

### Commit-message style

- Do not use Conventional Commits prefixes such as `feat:`, `fix:`, or
  `docs:`.
- Write the subject in the imperative mood and sentence case, with no trailing
  period. Aim for approximately 50-70 characters when the change can be
  described clearly within that range.
- Optionally end the subject with a short parenthetical scope when it adds
  useful context, for example `Constrain cleanup to owned build trees
  (build_lib.sh)`.
- Use an optional leading tag only when its meaning applies:
  - `[MAJOR]` for a significant capability, architectural change, or broad
    contract or workflow change;
  - `[BUGFIX]` for a correctness defect or regression;
  - `[HOTFIX]` for an urgent, narrowly targeted correction;
  - no tag for routine enhancements, tests, documentation, or maintenance.
- Use an optional body for changes that need rationale or a behavioral summary.
  Format it as imperative `-` bullets, put one blank line between bullets, omit
  terminal periods, and indent wrapped continuation lines beneath the bullet
  text.
- Describe intent, important design decisions, and behavioral consequences in
  the body instead of merely listing changed files.
- Never add `Co-Authored-By` or other AI-attribution trailers.

### Authorization and batch sequence

1. Never create or amend a commit unless the user explicitly instructs the
   agent to commit. Requests to implement, finish, stage, or continue, including
   the keyword `next`, do not grant commit permission.
2. Treat commit, tag, and push authorization independently. Permission to
   commit does not imply permission to tag or push.
3. Inspect the worktree and current index before preparing a batch. Preserve and
   report unrelated user-owned staged or unstaged work; never reset, overwrite,
   or absorb it merely to simplify the batch.
4. Partition completed work into coherent functional batches. Include directly
   dependent tests and necessary documentation with their implementation unless
   a concrete review or ownership boundary requires separation. Do not create
   micro-batches that are too small to review meaningfully.
5. A mixed batch is allowed when a few small changes do not justify independent
   review units. Label it clearly as mixed, explain why the items belong
   together, and never use it to hide a substantial independent feature or fix.
6. Before staging, review the complete candidate diff for correctness, scope,
   formatting, comments, and documentation. Run proportionate tests and static
   checks, and apply the staged-code quality gate below to every new or
   substantially modified source file.
7. Stage only the reviewed batch with an explicit path or hunk allowlist. Then
   inspect the complete index with `git diff --cached` and run
   `git diff --cached --check`. Repeat relevant validation against the staged
   result when index contents or generated inputs can affect the outcome.
8. Report the staged paths, functional purpose, verification evidence, caveats,
   exclusions, and exact proposed commit subject and body. Stop for user review
   without preparing or staging another batch.
9. Advance only after the user responds with the exact keyword `next`. Interpret
   `next` as permission to prepare the following batch, never as permission to
   commit the current batch.
10. Before advancing, confirm that the previous batch is no longer staged. If
    the index is still populated, stop and ask the user to commit or clear it,
    or to give a separate explicit instruction for the agent to commit.
11. When the user explicitly requests both actions, such as `commit and next`,
    commit the approved batch with the reviewed message, verify that the index
    is clear, and only then prepare the following batch.

## Staged-code review quality gate

Before handing staged changes to the user for commit review, inspect the complete
Git index with `git diff --cached`. Apply this gate to files staged by either the
user or the agent. This review does not authorize staging, committing, or
rewriting unrelated code.

For every staged source file that is new or substantially modified:

- Add or update both levels of applicable documentation: the file/module-level
  header and the public class/function/method documentation. Follow the
  established consolidated files for the relevant language and component.
- Organize related statements into visually separated blocks. Each block must
  implement one immediate objective or implementation step, not an entire broad
  feature.
- Introduce each non-obvious block with a concise comment explaining what it
  accomplishes and, when relevant, why that approach is required.
- Prefer purpose-, invariant-, and contract-oriented comments. Do not add
  comments that merely translate individual statements into prose.
- Preserve useful existing comments and documentation unless the staged change
  makes them incorrect.
- Review the staged result as a reader will receive it, rather than reviewing
  only the individual lines edited during implementation.

Limit cleanup to the intended scope of the staged work. Do not rewrite unrelated
legacy code merely because the same file is staged. Do not report the changes as
ready for review until this pass is complete; summarize any documentation or
readability cleanup performed during the pass.

### C++ and CUDA pattern

Use Doxygen for both the file header and public API documentation. Apply the
shared compact-line and logical-block rules consistently:

- Preserve compact grouped formatting when related call arguments or arithmetic
  terms remain readable together. Wrap at semantic expression boundaries; do
  not mechanically place every argument on a separate line.
- In a multiline function declaration, definition, or call, keep the first
  argument on the same line as the function name and align later arguments with
  it. Put the opening parenthesis at the end of a line only for a genuinely
  multiline first argument whose own structure requires separation.
- Follow the surrounding hand-formatted style and preserve intentional
  whitespace used to separate functional blocks. Do not apply broad automatic
  reformatting to staged or user-owned code.
- Prefer this compact grouped layout:

```cpp
const float gx = 0.5F * (PixelOrZero(image, width, height, x + 1, y) -
                         PixelOrZero(image, width, height, x - 1, y));

SPhotometricPatch(int id, const cv::Point2d &center, int64_t timestampUs, int patchSize);
```

  Do not expand the same calls into one line per argument unless an individual
  argument is itself a multiline expression whose structure requires it.

```cpp
/// @file observation_loader.cpp
/// @brief Loads validated observations from a delimited input file.
/// @details Owns parsing and validation; filtering policy remains with the
///          caller.

/// @brief Load and validate observations from disk.
/// @param inputPath Path to the delimited observation file.
/// @return Valid observations in input order.
/// @throws std::runtime_error When the file cannot be parsed.
std::vector<CObservation> LoadValidObservations(const std::filesystem::path& inputPath)
{
    // Parse the complete file first so malformed rows produce one consistent
    // diagnostic path.
    const std::vector<CObservation> parsedObservations = ParseObservations(inputPath);

    // Retain only observations satisfying the domain validity contract while
    // preserving their original order.
    std::vector<CObservation> validObservations;
    validObservations.reserve(parsedObservations.size());
    std::ranges::copy_if(parsedObservations, std::back_inserter(validObservations),
                         IsObservationValid);

    return validObservations;
}
```

### Python pattern

Use Google-style module, class, method, and function docstrings. Keep precise
type annotations on every callable and follow PEP 8 naming conventions:

```python
"""Load and validate observation records.

This module owns file parsing and domain validation. Selection policy remains
with the caller.

Example:
    observations = load_valid_observations(Path("observations.csv"))
    print(len(observations))

Output:
    3
"""


def load_valid_observations(input_path: Path) -> list[Observation]:
    """Load valid observations while preserving their input order.

    Args:
        input_path: Path to the delimited observation file.

    Returns:
        Valid observations in input order.

    Raises:
        ValueError: If an input row cannot be parsed.

    Example:
        observations = load_valid_observations(Path("observations.csv"))
        print(len(observations))

    Output:
        3
    """
    # Parse all rows through one path so malformed input produces consistent
    # diagnostics.
    parsed_observations = parse_observations(input_path)

    # Enforce the domain validity contract without changing source ordering.
    valid_observations = [
        observation
        for observation in parsed_observations
        if observation.is_valid()
    ]

    return valid_observations
```

### MATLAB pattern

For a primary MATLAB function file, the leading sectioned function
documentation is also the file-level entry documentation. Scripts require an
opening sectioned description, while class files require class help text plus
the same sectioned documentation on public methods. Keep the existing
`SIGNATURE`, `DESCRIPTION`, `INPUT`, `OUTPUT`, `CHANGELOG`, and `DEPENDENCIES`
template:

```matlab
function tableValidObservations = LoadValidObservations(charInputPath)
%% SIGNATURE
% tableValidObservations = LoadValidObservations(charInputPath)
% -------------------------------------------------------------------------------------------------------------
%% DESCRIPTION
% Load and validate observations while preserving their input order.
% -------------------------------------------------------------------------------------------------------------
%% INPUT
% charInputPath             Path to the delimited observation file.
% -------------------------------------------------------------------------------------------------------------
%% OUTPUT
% tableValidObservations    Valid observations in input order.
% -------------------------------------------------------------------------------------------------------------
%% CHANGELOG
% DD-MM-YYYY  Pietro Califano     First prototype.
% -------------------------------------------------------------------------------------------------------------
%% DEPENDENCIES
% ParseObservations
% -------------------------------------------------------------------------------------------------------------

arguments
    charInputPath (1, :) char
end

arguments (Output)
    tableValidObservations table
end

% Parse all rows through one path so malformed input produces consistent
% diagnostics.
tableParsedObservations = ParseObservations(charInputPath);

% Enforce the domain validity contract without changing source ordering.
bValidObservation = tableParsedObservations.bIsValid;
tableValidObservations = tableParsedObservations(bValidObservation, :);

end
```
