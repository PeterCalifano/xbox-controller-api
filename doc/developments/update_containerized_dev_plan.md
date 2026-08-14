# Plan: update containerized dev setup (template first, then spectral_raytracer)

## Context

The devcontainer setup is shared between `cpp_cuda_template_project` (upstream template) and `spectral_raytracer`. Audit findings driving this work:

1. **Major:** `update_devcontainer_json.py` regenerates `devcontainer.json` emitting only `name`/`build`/`features` — running `configure_devcontainer.sh` silently wipes `runArgs: --gpus all`, the whole `remoteEnv` block (`OPTIX_HOME`, `CUDA_HOME`, PATH/`LD_LIBRARY_PATH`) and `customizations`; it also pins CUDA 12.5 while the raytracer uses 12.9.
2. Template `Dockerfile` has a broken no-op GPG line (`RUN export GPG_TTY=$(tty)`).
3. `ros-setup.sh` installs `ros-dev-tools` even in ROS 1 mode (package only exists in ROS 2 repos → image build failure) and appends ROS sourcing to root's `~/.bashrc` (invisible to the `vscode` user).
4. Raytracer `custom-setup.sh` carries dead OS-detection code (leftover of the template's Qt block).
5. Raytracer `Dockerfile` clones `optix-dev` at unpinned `main` (non-reproducible + stale via layer cache).
6. Useful template-only bits missing in the raytracer: VS Code extensions list, `DISPLAY` containerEnv.
7. Host requirement (NVIDIA Container Toolkit / CDI for GPU passthrough) undocumented.
8. The Dockerfile is only usable through the devcontainer flow: CUDA comes from a devcontainer *feature*, env vars from `remoteEnv` — a plain `docker build .devcontainer` yields an image without CUDA and without `CUDA_HOME`/`OPTIX_HOME`.

**Decisions:** generator becomes merge-preserve (only rewrites keys it manages; script stays byte-identical across repos); OptiX clone hard-pinned to the current HEAD commit of `PeterCalifano/optix-dev`; Dockerfile made standalone-capable via an opt-in CUDA build arg.

**Host blocker (separate from config):** VS Code uses rootless **Podman 4.9.3**; image pull fails with `insufficient UIDs or GIDs available in user namespace` because `/etc/subuid`/`/etc/subgid` have **no entry for `pcalifano`**.

## Stage 0 — Unblock rootless Podman on the host (needs admin/root)

- [ ] Allocate subordinate ID ranges (next free range after mmutti: 427680):
      `sudo usermod --add-subuids 427680-493215 --add-subgids 427680-493215 pcalifano`
- [ ] As `pcalifano` (no sudo): `podman system migrate`
- [ ] Smoke test: `podman run --rm ubuntu:24.04 echo ok`
- [ ] GPU passthrough with Podman: generate CDI spec `sudo nvidia-ctk cdi generate --output=/etc/cdi/nvidia.yaml`; use `./configure_devcontainer.sh --cuda --gpu-runtime podman` to emit CDI runArgs

## Stage A — cpp_cuda_template_project (branch `feature/update-containerized-dev`)

- [x] Create branch `feature/update-containerized-dev`
- [x] A1. Rewrite `.devcontainer/update_devcontainer_json.py` as merge-preserve: loads existing devcontainer.json (JSONC comments stripped), rewrites only managed keys — `build`, `features` (CUDA feature w/ `CUDA_VERSION`, default 12.9), CUDA GPU runArgs selected for Docker or Podman, CUDA entries of `remoteEnv`, ROS entries of `containerEnv` — preserves everything else; idempotent
- [x] A2. `configure_devcontainer.sh`: add `--cuda-version <v>` flag (default 12.9, exported as `CUDA_VERSION`) and `--gpu-runtime auto|docker|podman`; render JSON to a temp file then `mv` (avoid shell `>` truncating the file before Python reads it)
- [x] A3. `ros-setup.sh`: install `ros-dev-tools` only for ROS 2; append ROS sourcing to `/etc/bash.bashrc` instead of root's `~/.bashrc`; tolerate `rosdep init` re-run
- [x] A4. Template `Dockerfile`: fix GPG line → `RUN echo "export GPG_TTY=\$(tty)" >> /etc/bash.bashrc`
- [x] A5. Standalone-capable Dockerfile: new `.devcontainer/cuda-setup.sh` installing the CUDA toolkit from NVIDIA's apt repo (cuda-keyring, version from build arg), gated by `ARG INSTALL_CUDA="off"` + `ARG CUDA_VERSION="12.9"`; add `ENV CUDA_HOME=/usr/local/cuda` + PATH/`LD_LIBRARY_PATH` so a plain `docker build`/`podman build` produces a working image (devcontainer flow unchanged: feature still installs CUDA, args default off)
- [x] A6. Regenerate template `devcontainer.json` via `./configure_devcontainer.sh --cuda --base ubuntu-24.04 --non-interactive`; verify `customizations`/`DISPLAY` preserved, CUDA 12.9, selected GPU runArgs present
- [x] A7. Template README: document NVIDIA Container Toolkit (Docker) / CDI spec (Podman) host requirement + standalone build usage (`docker build --build-arg INSTALL_CUDA=on .devcontainer`)
- [x] A8. New root-level `run_in_container.sh`: builds the `.devcontainer` image standalone if missing (or with `--build`), autodetects docker/podman, mounts the repo at `/workspace`, applies GPU flags (`--gpus all` for Docker, `--device nvidia.com/gpu=all` for Podman/CDI) and runs a given binary/command inside the container (e.g. `./run_in_container.sh ./build/my_app --args`)
- [x] Commit on the template branch (2fd4025)

Out of scope in template: Qt block in `custom-setup.sh` (legit user of OS-detection), `reinstall-cmake.sh`, mingw package.

## Stage B — spectral_raytracer (branch `feature/update-containerized-dev` from main @ ee18af1)

- [x] Create branch `feature/update-containerized-dev`
- [x] Copy byte-identical from template: `configure_devcontainer.sh`, `run_in_container.sh`, `.devcontainer/update_devcontainer_json.py`, `.devcontainer/ros-setup.sh`, `.devcontainer/cuda-setup.sh`
- [x] `.devcontainer/custom-setup.sh`: drop dead `os_id`/`os_version` block (keep no-Qt variant)
- [x] `.devcontainer/Dockerfile`: adopt standalone CUDA args/env from template; pin OptiX headers clone — resolve `git ls-remote https://github.com/PeterCalifano/optix-dev.git main`, replace `OPTIX_SDK_BRANCH` with `ARG OPTIX_SDK_REF="<sha>"`, clone via `git init` + `git fetch --depth 1 <repo> $OPTIX_SDK_REF` + `checkout FETCH_HEAD`; add `ENV OPTIX_HOME=/opt/optix-sdk`
- [x] `.devcontainer/devcontainer.json`: add template's VS Code extensions + `containerEnv.DISPLAY` (no ROS vars), then regenerate via configure script; confirm round-trip keeps selected GPU runArgs, full `remoteEnv` incl. `OPTIX_HOME`, CUDA 12.9
- [x] README.md: same host-requirement + standalone build note as template
- [x] Remove `.bak.*` files produced during regeneration (gitignored; don't commit)
- [x] Commit on `feature/update-containerized-dev` (64b05de)

`spectral_raytracer_dev` untouched (synced later via its own branch flow).

## Stage V — Verification

- [x] `bash -n` on all touched shell scripts; `python3 -m py_compile` on the generator; `shellcheck` if available (not installed on host)
- [x] Generator round-trip (both repos): run configure twice → identical output; `--no-cuda` removes CUDA feature/managed GPU runArgs/CUDA env but preserves `OPTIX_HOME`/`customizations`/`DISPLAY`; `--cuda` restores selected GPU runArgs
- [x] Regenerated `devcontainer.json` parses with `python3 -m json.tool`
- [x] Pinned-SHA clone test outside Docker (temp dir, resolved SHA) to confirm GitHub serves it
- [ ] Optional (slow, on request): full standalone `podman build --build-arg INSTALL_CUDA=on .devcontainer`
