# Derived Project Upgrade Agent Guidelines

## Purpose

Use this guide when carrying improvements from `cpp_cuda_template_project`
into one or more existing derived repositories.

This is a template-maintainer procedure. It is not content to copy into derived
projects. The entire `doc/developments/` tree is removed by normal template
tailoring.

The objective is not to make a derived repository look like the current
template. The objective is to apply relevant donor improvements without
regressing the derived project's behavior or undoing any intentional tailoring.

## Core Upgrade Model

Treat every update as a three-way semantic port:

1. Determine the old donor state from which the derived project was created or
   last synchronized.
2. Determine the improvement delta between that old donor state and the
   reviewed new donor revision.
3. Apply the intent of that donor delta to the current derived project while
   preserving the derived project's own tailoring delta.

In shorthand:

```text
updated derived project = current derived project + applicable donor intent
```

It is never:

```text
updated derived project = new donor tree + copied project sources
```

File equality is not the goal. Preserved project semantics, explicit feature
choices, and verified behavior are the goal.

## Authority Order

When instructions conflict, use this order:

1. The current user request and explicit per-repository permissions.
2. The target repository's own `AGENTS.md`, `CLAUDE.md`, contributor guidance,
   and development plans.
3. The target repository's architecture, public API, tests, packaging contract,
   and established local conventions.
4. The campaign plan created for the current fleet update.
5. This guide.
6. The donor template's current implementation.

The donor is authoritative for the reusable improvement being ported. It is not
authoritative for a derived project's product architecture.

## Primary Invariant: Preserve Tailoring

Before editing a derived repository, identify and record its existing tailoring.
Preserve both explicit changes and intentional absences.

Tailoring includes, but is not limited to:

- project, package, namespace, target, artifact, and workflow names;
- source layout, public APIs, executable structure, and module boundaries;
- removed template skeletons, helpers, tests, examples, or optional features;
- enabled or disabled CUDA, OptiX, TensorRT, TBB, OpenGL, profiling, wrapper,
  docs, and ROS support;
- dependency providers, minimum versions, fetch policy, and offline policy;
- CMake option names, defaults, install layout, exports, and package metadata;
- Python or MATLAB package names, wrapper interfaces, and environment policy;
- ROS package prefixes, interface definitions, launch behavior, parameters,
  topic/service names, and the adapted core-call seam;
- CI triggers, runner labels, branch policy, release policy, and deployment
  behavior;
- submodule revisions, vendored dependencies, local patches, and generated-file
  policy.

Rules:

- A donor path that is absent in the target is intentionally absent until
  repository history or the user proves otherwise. Do not recreate it merely
  because it exists in the donor.
- A target file that differs from the donor is not stale by default. Determine
  the local reason before editing it.
- Preserve the behavior represented by tailoring even when the donor change
  requires a different textual implementation.
- Never replace an adapted target file wholesale when a semantic port is
  possible.
- When the donor improvement conflicts with target architecture, stop and
  present the conflict and options. Do not disguise an architecture change as
  a template update.

Use a tailoring-preservation ledger with this minimum shape:

| Contract | Current tailored state | Evidence | Required post-update proof |
|---|---|---|---|
| Identity | Names and namespaces | Package metadata | Donor identity stays absent |
| Architecture/API | Modules and public API | Source and tests | Existing consumers pass |
| Feature policy | Enabled/removed features | Options/history | Feature matrix stays stable |
| Dependencies | Provider and version policy | Build files/submodules | No new provider appears |
| CI/release | Branches, runners, triggers | Active workflows | Existing policy remains effective |
| Optional overlays | ROS/wrapper/docs adaptations | Seams and tests | Adaptations remain intact |

Add rows for every repository-specific decision discovered during inventory.
Where practical, turn each required proof into an executable guard instead of a
manual statement.

## No-Regression Policy

No regression is a measured contract, not an assumption.

Before any edit:

- run the target's relevant existing build and tests;
- record exact passing totals and any pre-existing failures;
- record generated artifacts or package-consumer checks that define success;
- identify user-facing APIs, options, names, and workflows that must remain
  stable;
- add a focused red guard first when the donor delta fixes a reproducible bug,
  but do not turn a one-time feature-matrix or consumer acceptance build into a
  recursive test in the target's ordinary CTest suite.

After the update:

- rerun every applicable baseline gate;
- run the new regression tests introduced by the donor delta;
- prove that intentionally removed features and files remain absent;
- prove that renamed identities and local defaults have not reverted;
- compare failures against the baseline and reject any new failure;
- review warnings and generated output for newly introduced degradation;
- test installed or packaged consumption when the update touches exports,
  metadata, headers, or dependency flow.

Keep template conformance and target acceptance separate:

- do not copy donor `tests/cmake/VerifyTemplateProject*` files into a derived
  repository;
- do not register recursive configure/build/install tests in ordinary target
  CTest when explicit fresh acceptance commands or CI already own the contract;
- use disposable nested/build-tree/install-tree consumers outside the normal
  test build;
- add a permanent CMake-script test only when it is lightweight, target-owned,
  non-recursive, and covers behavior unavailable through Catch2, pytest, an
  existing target, or the acceptance matrix.

If the baseline is already red, record the exact failure set before proceeding.
The update may continue only when the failure is unrelated and the user agrees.
Do not report the target as green; report that no new regression was introduced
relative to the recorded baseline.

Use these acceptance states:

| Baseline state | Required closure |
|---|---|
| Green | Every applicable baseline gate remains green and new guards pass |
| Known red | Approved existing failures are unchanged; no new failure appears |
| Required gate unavailable | Mark blocked/partial; never report it as passed |

## Repository and Git Safety

- Work on one target repository at a time unless the user explicitly authorizes
  parallel modifications.
- Read the target's instructions after entering its worktree.
- Confirm the repository root, branch, `HEAD`, upstream relation, worktree
  status, staged state, submodule state, and ignored local configuration.
- Treat all pre-existing modifications as user-owned. Do not revert, overwrite,
  restage, or include them in an upgrade batch without explicit agreement.
- Never use destructive Git commands to clean a target.
- Commit and push permissions are per repository and per campaign. Permission to
  commit the testfield does not authorize commits in any other repository.
- Follow the target's recent `git log` style when proposing or creating commits.
- Stage exact paths by functional batch. Do not use a broad `git add` in a dirty
  multi-repository campaign.
- Never push unless the user explicitly requests it.

Use a committed donor revision for production fleet updates. A reviewed but
uncommitted donor patch may be applied to a scratch target or designated
testfield for validation, but record the donor base commit and patch identity.
Do not propagate that moving worktree state across the fleet.

## Portability and Hygiene

- Never add absolute paths tied to the current machine, user account, SDK
  installation, workspace layout, or scratch directory.
- Express external tools through documented CMake variables, environment
  variables, package discovery, or target-owned configuration.
- Exclude build/install trees, virtual environments, caches, bytecode, logs,
  generated wrappers, generated docs, and IDE state from donor comparisons and
  copied content.
- Preserve file modes and executable bits where scripts or manifests require
  them.
- Parse structured formats with their native parsers. Do not use broad textual
  replacement for XML, JSON, YAML, TOML, or CMake package metadata when a
  structured owner exists.
- Search for conflict markers and stale donor identifiers before staging.

## Campaign Artifacts

For a multi-repository update, maintain one campaign plan in the donor under
`doc/developments/`. Use Markdown checkboxes and update them as work lands.

The campaign plan should contain:

- the reviewed donor revision or base commit plus patch identity;
- the ordered target repository list;
- a feature and tailoring matrix for every target;
- per-target baseline results and known blockers;
- applicable, adapted, skipped, and blocked donor deltas;
- allowed and frozen file surfaces;
- validation commands and expected evidence;
- staging/commit permissions and proposed commit groups;
- status and residual risk.

Keep detailed stage outputs in a companion donor-side development log so the
evidence is readable after the campaign. Temporary full command logs may remain
outside Git, but the persistent log must summarize the commands, results,
artifacts, warnings, and blockers needed for review.

## Required Inventory

Record this before planning edits in each target:

| Field | Required evidence |
|---|---|
| Repository | Logical name and path placeholder used by the campaign |
| Git state | Branch, `HEAD`, upstream, staged/unstaged/untracked files |
| Donor baseline | Last known donor revision, tag, or evidence-backed estimate |
| Identity | CMake package, namespace, library target, Python/MATLAB package |
| Core architecture | Source modules, public API, programs, install/export shape |
| Optional features | CUDA, OptiX, TBB, OpenGL, profiling, docs, wrappers, ROS |
| Dependencies | System, package-manager, fetched, vendored, or submodule policy |
| CI/release | Active workflows, runners, branches, Pages, tags, packaging |
| Known tailoring | Renames, removals, local adaptations, frozen areas |
| Baseline gates | Commands, totals, warnings, pre-existing failures |
| Permissions | Edit, stage, commit, and push authority for this repository |

Do not infer the donor baseline from similar filenames alone. Use repository
history, prior rollout records, tags, commit messages, or a three-way content
comparison and state the confidence of the result.

## Delta Classification

Classify every donor change before applying it:

- **Adopt:** reusable improvement applies without changing target intent.
- **Adapt:** improvement applies, but target names, APIs, layout, dependencies,
  or feature choices require a semantic port.
- **Skip:** target deliberately removed or replaced the affected capability.
- **Upstream first:** review discovered a generic donor defect that should be
  fixed and validated in the template before fleet propagation.
- **Blocked:** required environment, dependency, external repository, ownership
  decision, or architecture decision is unavailable.

Record why a delta was adapted or skipped. Silence is not evidence that it was
considered.

## Staged Upgrade Procedure

### Stage 0 - Establish the donor contract

- Select the exact reviewed donor revision.
- Read its upgrade plans, stage outputs, release notes, and relevant commits.
- Identify the smallest functional donor deltas to port.
- Confirm which donor tests are template-only and which contracts must survive
  in a derived project.
- Do not treat template-validation workflows as project workflows.
- Translate applicable donor CMake conformance into target acceptance commands;
  do not copy recursive verifier scripts into target CTest.

### Stage 1 - Baseline the target and capture tailoring

- Read target instructions and recent history.
- Record the required inventory and feature matrix.
- Run applicable tests before editing.
- Create a tailoring-preservation ledger with explicit invariants such as:
  - renamed identity remains unchanged;
  - removed feature remains absent;
  - adapted public API remains intact;
  - target workflow remains project-generic;
  - local dependency provider remains selected;
  - project-specific ROS seam remains adapted.
- Mark unrelated dirty paths as excluded from the campaign.

### Stage 2 - Build the per-target plan

- List each applicable donor delta and its classification.
- Name exact target files and line/semantic anchors.
- Define allowed and frozen surfaces.
- Add red-green tests before implementation where practical. For build-system
  feature matrices and consumer behavior, use a recorded failing acceptance
  command rather than a permanent recursive CTest.
- Order changes so every commit candidate is internally coherent and testable.
- Obtain user approval when the plan changes architecture, dependencies, public
  API, release behavior, or established tailoring.

### Stage 3 - Port generic infrastructure

- Apply the smallest semantic change that satisfies the donor contract.
- Reuse target patterns and helpers instead of importing unrelated donor
  abstractions.
- Preserve target metadata, names, option defaults, dependency policy, and
  feature removals.
- Update tests with the behavior, not donor-specific placeholder text, unless
  the target is explicitly a template testfield.
- When changing CMake package exports, verify both build-tree and install-tree
  consumers as applicable.

### Stage 4 - Adapt optional features

Apply only sections enabled in the target feature matrix.

#### CUDA and OptiX

- Preserve the target's architecture selection and toolkit policy.
- Keep ordinary `.cu` translation units separate from dedicated `*.ptx.cu`
  generation and embedding.
- Verify a real project CUDA object or compile-database entry. Compiler probe
  artifacts do not count.
- Keep SDK paths out of installed exports and tracked configuration.
- Skip OptiX cleanly when the target intentionally removed it; do not restore
  the facade merely because the donor supports it.

#### ROS 2 overlay

- Use additive rollout only when the target has no overlay. Never use
  `add_ros2_support.sh` to overwrite an existing adapted overlay.
- Preserve the distinction between the CMake package identity and ROS-valid
  package prefix.
- Preserve existing messages, services, node names, namespaces, launch policy,
  parameters, and package dependencies unless the campaign explicitly changes
  them.
- Treat `ros2/<ros_prefix>_ros/src/conversions.cpp` as the primary project API
  adaptation seam. Preserve its target-specific core include and call.
- Review `CTemplateLifecycleNode.cpp` separately as ROS wiring; do not move core
  architecture into the node.
- Ensure metadata synchronization preserves established package names and
  requires the target's root metadata contract before enabling it.
- Materialize the dormant generic `.yml.tpl` workflow for a derived project.
  Never copy the donor's active template-validation workflow.

#### Wrappers

- Preserve the target's wrapped API, package namespace, generated-file policy,
  supported Python version, MATLAB policy, and gtwrap source.
- Distinguish source-generation defects from unavailable external environments.
- Do not commit generated wrappers unless the target already owns them in Git.

#### Documentation and CI

- Keep active derived-project workflows generic and runnable after tailoring.
- Do not add template-only placeholder assertions to derived CI.
- Preserve runner labels, branch names, release triggers, deployment policy,
  secrets contract, and intentionally disabled jobs.
- Validate dormant workflow templates by materializing them in a scratch copy.
- Keep internal campaign reports out of generated public documentation and
  tailored project output.

### Stage 5 - Validate no regression

Start with the target's own documented entry points. Typical gates include:

```bash
./build_lib.sh --clean
ctest --test-dir build --output-on-failure
```

Add only applicable feature gates:

- clean CPU-only configure/build/test;
- install and external consumer configure/build;
- CUDA build, runtime test, and compile-graph inspection;
- OptiX PTX generation, embedding, and installed consumer;
- ROS clean build/test plus launch/service behavior;
- wrapper generation, package install/import, and supported-version rejection;
- docs configure/build and generated-content checks;
- workflow YAML, package XML, JSON, TOML, and Python parsing;
- shell syntax and shellcheck for changed scripts;
- cross-compilation or packaging gates owned by the target.

Always run:

- `git diff --check`;
- exact conflict-marker scanning;
- stale donor/template identifier scanning;
- generated-artifact and bytecode scanning;
- machine-local path scanning over added lines;
- tailoring-preservation assertions from Stage 1;
- comparison against the recorded baseline totals and failure set.

### Stage 6 - Perform an extensive review

Review the completed implementation and its design before staging:

- inspect every changed file in target context;
- look for copied assumptions that do not hold in the target;
- verify ownership boundaries and install/package behavior;
- inspect failure paths, clean/repeated runs, and idempotency;
- check that tests would fail if the regression returned;
- verify that absent features were not accidentally reintroduced;
- check docs and workflows against actual commands;
- identify latent issues discovered during validation and fix worthwhile ones
  red-green before closure;
- record residual risks and external blockers honestly.

### Stage 7 - Stage and hand off

- Group changes by functionality and dependency, not by file type.
- Keep plans/evidence with the functional batch they explain when that matches
  the target's history; otherwise propose a separate documentation batch.
- Stage exact files only after the batch passes its gates.
- Show staged and unstaged status separately.
- Propose commit titles and bullet descriptions in the target maintainer's
  established style.
- Commit only with explicit permission for that target. Never push by default.

## Fleet Rollout Strategy

1. Validate the donor change in the template first.
2. Rehearse the semantic port in the designated testfield or the closest
   representative derived repository.
3. Feed generic defects found by the rehearsal back into the donor, add a guard,
   and revalidate before continuing.
4. Update production derived repositories one at a time, ordered from closest
   to the donor to most heavily tailored.
5. Reuse knowledge and test patterns, but rebuild the tailoring ledger and
   baseline for every repository.
6. Do not copy a patch from one derived project to another without reviewing
   their tailoring deltas.
7. Close each repository with its own evidence, staged status, commit proposal,
   and residual-risk report.

Read-only inventory may be parallelized when repositories are independent.
Edits, staging, and commits remain sequential unless the user explicitly
authorizes a different campaign strategy.

## Stop Conditions

Stop and report before modifying further when:

- the intended target repository or owning checkout is ambiguous;
- the donor revision or target donor baseline cannot be established with useful
  confidence;
- unrelated dirty changes overlap required files and cannot be preserved safely;
- an update would restore an intentionally removed feature;
- donor behavior conflicts with target architecture or public API;
- a new dependency, release change, or workflow privilege needs user approval;
- a required test environment or external repository is unavailable;
- baseline failures prevent a meaningful no-regression comparison;
- the same external blocker prevents progress after reasonable local diagnosis;
- commit or push permission is unclear.

Report the blocker, evidence, files already changed, unaffected work that can
continue, and the smallest decision or external action needed to resume.

## Reusable Campaign Checklist

Copy this checklist into the donor-side campaign plan and expand it per target.

### Campaign setup

- [ ] Select and record the exact donor revision.
- [ ] Inventory all target repositories and per-repository permissions.
- [ ] Order targets by similarity, risk, and dependency.
- [ ] Create the campaign feature/tailoring matrix.
- [ ] Create the persistent stage-output log.

### Reference rehearsal

- [ ] Capture the reference repo baseline and tailoring ledger.
- [ ] Classify every donor delta as adopt, adapt, skip, upstream first, or
  blocked.
- [ ] Add red guards for reproduced defects.
- [ ] Port and adapt the approved deltas.
- [ ] Run the complete applicable validation matrix.
- [ ] Perform the extensive review and feed generic findings upstream.
- [ ] Re-run the reference gates after upstream corrections.

### Each derived repository

- [ ] Read local instructions and confirm Git state and permissions.
- [ ] Establish the old donor baseline with stated confidence.
- [ ] Record baseline tests, warnings, failures, and artifacts.
- [ ] Record all relevant tailoring and intentional absences.
- [ ] Define allowed/frozen surfaces and excluded dirty paths.
- [ ] Classify and plan each applicable donor delta.
- [ ] Port generic behavior without copying donor identity or architecture.
- [ ] Adapt enabled optional features only.
- [ ] Assert that removed/renamed/disabled tailoring remains preserved.
- [ ] Run baseline, new regression, package-consumer, and feature gates.
- [ ] Run portability, hygiene, conflict, and stale-identifier checks.
- [ ] Complete an extensive implementation and design review.
- [ ] Update persistent campaign evidence.
- [ ] Stage exact functional batches.
- [ ] Propose user-style commit titles and bullet descriptions.
- [ ] Commit only when explicitly authorized for this repository.
- [ ] Record residual risk and blockers before moving to the next target.

### Campaign closure

- [ ] Confirm every target has an explicit completed, skipped, or blocked state.
- [ ] Confirm no target contains machine-local paths or generated campaign
  artifacts.
- [ ] Confirm generic fixes and regression guards landed in the donor first.
- [ ] Summarize fleet-wide validation and per-target exceptions.
- [ ] Propose follow-up work separately from the completed upgrade scope.
