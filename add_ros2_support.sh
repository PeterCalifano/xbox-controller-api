#!/usr/bin/env bash
# Add the optional ROS 2 overlay to a derived project without editing existing files.

set -Eeuo pipefail
IFS=$'\n\t'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="${SCRIPT_DIR}"
ROOT_DIR="${PWD}"

APPLY=0
ASSUME_YES=0
LIST_ONLY=1
VERIFY=0
NO_CI=0
ROS_PREFIX_OVERRIDE=""
cmake_project_name=""
ros_package_prefix=""

copied_roots=()

info() { printf '\033[34m[INFO]\033[0m %s\n' "$*"; }
warn() { printf '\033[33m[WARN]\033[0m %s\n' "$*" >&2; }
die() { printf '\033[31m[ERROR]\033[0m %s\n' "$*" >&2; exit 1; }

usage() {
  cat <<'EOF'
Usage:
  ./add_ros2_support.sh [--list] [--root <dir>] [--ros-prefix <name>] [--no-ci]
  ./add_ros2_support.sh --apply [--yes] [--root <dir>] [--ros-prefix <name>] [--verify] [--no-ci]

Purpose:
  Copy the optional ROS 2 overlay from this template checkout into a derived
  project. The operation is additive: existing target files are never edited.

Options:
  --list        Print the rollout plan and exit. This is the default.
  --apply       Copy and rename overlay files into the target.
  --yes         Do not prompt before applying.
  --root <dir>  Target project root. Defaults to the current directory.
  --ros-prefix <name>
                ROS package prefix to use for copied packages. Defaults to a
                ROS-valid form derived from set(project_name "...").
  --verify      After applying, run a configure-only standalone CMake check.
  --no-ci       Do not copy the optional ROS 2 overlay CI workflow.
  -h, --help    Show this help.
EOF
}

parse_args() {
  while (($# > 0)); do
    case "$1" in
      --list)
        LIST_ONLY=1
        APPLY=0
        shift
        ;;
      --apply)
        APPLY=1
        LIST_ONLY=0
        shift
        ;;
      --yes|-y)
        ASSUME_YES=1
        shift
        ;;
      --root)
        [[ $# -ge 2 ]] || die "--root requires a directory"
        ROOT_DIR="$2"
        shift 2
        ;;
      --ros-prefix)
        [[ $# -ge 2 ]] || die "--ros-prefix requires a package prefix"
        ROS_PREFIX_OVERRIDE="$2"
        shift 2
        ;;
      --verify)
        VERIFY=1
        shift
        ;;
      --no-ci)
        NO_CI=1
        shift
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      *)
        die "Unknown argument: $1"
        ;;
    esac
  done
}

detect_project_name() {
  local cmakelists_="$1"
  sed -nE 's/^[[:space:]]*set[[:space:]]*[(][[:space:]]*project_name[[:space:]]+"?([^" )]+)"?.*/\1/p' "${cmakelists_}" | head -n1
}

derive_default_ros_prefix() {
  local source_name_="$1"
  printf '%s' "${source_name_}" \
    | tr '[:upper:]' '[:lower:]' \
    | sed -E 's/[^a-z0-9_]+/_/g; s/_+/_/g; s/^_+//; s/_+$//'
}

validate_ros_prefix() {
  local prefix_="$1"
  [[ "${prefix_}" =~ ^[a-z][a-z0-9_]*$ ]] || die "Invalid ROS package prefix '${prefix_}'. Expected [a-z][a-z0-9_]*"
}

validate_source() {
  [[ -d "${SOURCE_DIR}/ros2" ]] || die "Source checkout is missing ros2/: ${SOURCE_DIR}"
  [[ -f "${SOURCE_DIR}/build_ros2.sh" ]] || die "Source checkout is missing build_ros2.sh: ${SOURCE_DIR}"
  if ((NO_CI == 0)); then
    [[ -f "${SOURCE_DIR}/.github/workflows/build_ros2_overlay.yml" ]] \
      || die "Source checkout is missing the ROS 2 project workflow"
  fi
}

validate_target() {
  [[ -d "${ROOT_DIR}" ]] || die "Target root does not exist: ${ROOT_DIR}"
  ROOT_DIR="$(cd "${ROOT_DIR}" && pwd)"
  [[ -f "${ROOT_DIR}/CMakeLists.txt" ]] || die "Missing CMakeLists.txt in target root: ${ROOT_DIR}"
  [[ -f "${ROOT_DIR}/build_lib.sh" ]] || die "Missing build_lib.sh in target root: ${ROOT_DIR}"

  cmake_project_name="$(detect_project_name "${ROOT_DIR}/CMakeLists.txt")"
  [[ -n "${cmake_project_name}" ]] || die "Could not detect set(project_name \"<name>\") in ${ROOT_DIR}/CMakeLists.txt"
  [[ "${cmake_project_name}" =~ ^[A-Za-z][A-Za-z0-9_.+-]*$ ]] \
    || die "Invalid project_name '${cmake_project_name}'. Expected [A-Za-z][A-Za-z0-9_.+-]*"

  if [[ -n "${ROS_PREFIX_OVERRIDE}" ]]; then
    ros_package_prefix="${ROS_PREFIX_OVERRIDE}"
  else
    ros_package_prefix="$(derive_default_ros_prefix "${cmake_project_name}")"
  fi
  validate_ros_prefix "${ros_package_prefix}"
}

target_is_clean() {
  local conflict_=0
  local optional_target_

  if [[ -e "${ROOT_DIR}/ros2" || -L "${ROOT_DIR}/ros2" ]]; then
    warn "Target already has ros2/: ${ROOT_DIR}/ros2"
    conflict_=1
  fi
  if [[ -e "${ROOT_DIR}/build_ros2.sh" || -L "${ROOT_DIR}/build_ros2.sh" ]]; then
    warn "Target already has build_ros2.sh: ${ROOT_DIR}/build_ros2.sh"
    conflict_=1
  fi

  optional_target_="${ROOT_DIR}/doc/ros2_overlay.md"
  if [[ -f "${SOURCE_DIR}/doc/ros2_overlay.md" && -d "${ROOT_DIR}/doc" \
      && ( -e "${optional_target_}" || -L "${optional_target_}" ) ]]; then
    warn "Target already has doc/ros2_overlay.md: ${optional_target_}"
    conflict_=1
  fi

  optional_target_="${ROOT_DIR}/.github/workflows/build_ros2_overlay.yml"
  if ((NO_CI == 0)) \
      && [[ -f "${SOURCE_DIR}/.github/workflows/build_ros2_overlay.yml" \
      && -d "${ROOT_DIR}/.github/workflows" \
      && ( -e "${optional_target_}" || -L "${optional_target_}" ) ]]; then
    warn "Target already has .github/workflows/build_ros2_overlay.yml: ${optional_target_}"
    conflict_=1
  fi

  return "${conflict_}"
}

require_no_conflicts() {
  if target_is_clean; then
    return
  fi
  die "Target has an overlay destination conflict; refusing to copy any files"
}

print_rollout_plan() {
  cat <<EOF
ROS 2 overlay rollout plan
  Source template        : ${SOURCE_DIR}
  Target project root    : ${ROOT_DIR}
  Detected target project: ${cmake_project_name}
  ROS package prefix: ${ros_package_prefix}
  Mode                   : $([[ "${APPLY}" == 1 ]] && printf 'apply' || printf 'list')
  CI workflow            : $([[ "${NO_CI}" == 1 ]] && printf 'skip' || printf 'copy if present')

Required copies:
  - ros2/ -> ros2/ with template_project renamed to ${ros_package_prefix}
  - build_ros2.sh -> build_ros2.sh
  - core CMake package references stay pointed at ${cmake_project_name}

Optional copies when source and target directories exist:
  - doc/ros2_overlay.md
  - .github/workflows/build_ros2_overlay.yml
  - python/COLCON_IGNORE, lib/COLCON_IGNORE, examples/COLCON_IGNORE, tests/COLCON_IGNORE

Never copied:
  - add_ros2_support.sh
  - template-development verifier scripts
EOF

  if ! target_is_clean; then
    cat <<'EOF'

Conflicts:
  - target already contains ROS 2 overlay files; --apply will refuse to continue.
EOF
  fi
}

confirm_apply() {
  ((APPLY)) || return
  ((ASSUME_YES)) && return

  printf 'Add ROS 2 overlay to %s? Type "yes" to continue: ' "${ROOT_DIR}"
  read -r answer_
  [[ "${answer_}" == "yes" ]] || die "Aborted"
}

remember_copied_root() {
  copied_roots+=("$1")
}

copy_required_file() {
  local source_relative_="$1"
  local target_relative_="$2"
  local source_path_="${SOURCE_DIR}/${source_relative_}"
  local target_path_="${ROOT_DIR}/${target_relative_}"

  [[ -f "${source_path_}" ]] || die "Required source file missing: ${source_relative_}"
  [[ ! -e "${target_path_}" && ! -L "${target_path_}" ]] || die "Target path already exists: ${target_relative_}"

  cp -a "${source_path_}" "${target_path_}"
  remember_copied_root "${target_path_}"
  info "copied ${target_relative_}"
}

copy_optional_file_if_possible() {
  local source_relative_="$1"
  local target_relative_="$2"
  local source_path_="${SOURCE_DIR}/${source_relative_}"
  local target_path_="${ROOT_DIR}/${target_relative_}"
  local target_dir_

  [[ -f "${source_path_}" ]] || {
    info "skip missing optional source ${source_relative_}"
    return
  }

  target_dir_="$(dirname "${target_path_}")"
  [[ -d "${target_dir_}" ]] || {
    info "skip optional ${target_relative_}; target directory is absent"
    return
  }
  [[ ! -e "${target_path_}" && ! -L "${target_path_}" ]] || die "Target path already exists: ${target_relative_}"

  cp -a "${source_path_}" "${target_path_}"
  remember_copied_root "${target_path_}"
  info "copied ${target_relative_}"
}

copy_colcon_marker_if_possible() {
  local target_relative_="$1"
  local target_path_="${ROOT_DIR}/${target_relative_}"
  local target_dir_

  target_dir_="$(dirname "${target_path_}")"
  [[ -d "${target_dir_}" ]] || {
    info "skip ${target_relative_}; target directory is absent"
    return
  }
  if [[ -e "${target_path_}" || -L "${target_path_}" ]]; then
    info "keeping existing ${target_relative_}"
    return
  fi

  : > "${target_path_}"
  info "created ${target_relative_}"
}

copy_ros2_tree() {
  local source_item_
  local source_base_
  local target_ros2_="${ROOT_DIR}/ros2"

  [[ ! -e "${target_ros2_}" && ! -L "${target_ros2_}" ]] || die "Target path already exists: ros2"
  mkdir -p "${target_ros2_}"

  shopt -s nullglob
  for source_item_ in "${SOURCE_DIR}"/ros2/*; do
    source_base_="$(basename "${source_item_}")"
    case "${source_base_}" in
      build|install|log)
        info "skip generated ros2/${source_base_}"
        continue
        ;;
    esac
    cp -a "${source_item_}" "${target_ros2_}/"
  done
  shopt -u nullglob

  find "${target_ros2_}" -type f \( -name '*.pyc' -o -name '*.pyo' \) -delete
  find "${target_ros2_}" -depth -name '__pycache__' -exec rm -rf -- {} +

  remember_copied_root "${target_ros2_}"
  info "copied ros2/"
}

replace_placeholder_in_file() {
  local file_path_="$1"

  grep -Iq . "${file_path_}" || return 0
  REPLACEMENT_NAME="${ros_package_prefix}" perl -0pi -e \
    's/(?<![A-Za-z0-9_])template_project(?=(_[A-Za-z0-9_]+)?\b)/$ENV{REPLACEMENT_NAME}/g' \
    "${file_path_}"
}

restore_core_cmake_references_in_file() {
  local file_path_="$1"

  [[ "${cmake_project_name}" != "${ros_package_prefix}" ]] || return 0
  grep -Iq . "${file_path_}" || return 0
  ROS_PACKAGE_PREFIX="${ros_package_prefix}" CMAKE_PROJECT_NAME="${cmake_project_name}" perl -0pi -e '
    my $ros = quotemeta $ENV{ROS_PACKAGE_PREFIX};
    my $cmake = $ENV{CMAKE_PROJECT_NAME};
    s/find_package\(${ros} REQUIRED\)/"find_package(" . $cmake . " REQUIRED)"/ge;
    s/${ros}::${ros}/$cmake . "::" . $cmake/ge;
    s/(ament_export_dependencies\([^)]*?\n[ \t]*)${ros}([ \t\r\n][^)]*\))/$1 . $cmake . $2/gse;
  ' "${file_path_}"
}

replace_placeholder_in_path_component() {
  local path_component_="$1"

  REPLACEMENT_NAME="${ros_package_prefix}" perl -e '
    my $value = shift;
    $value =~ s/(?<![A-Za-z0-9_])template_project(?=(_[A-Za-z0-9_]+)?\b)/$ENV{REPLACEMENT_NAME}/g;
    print $value;
  ' -- "${path_component_}"
}

rename_copied_paths() {
  local root_path_
  local path_
  local path_dir_
  local path_base_
  local new_base_
  local new_path_

  for root_path_ in "${copied_roots[@]}"; do
    [[ -e "${root_path_}" || -L "${root_path_}" ]] || continue

    if [[ -f "${root_path_}" ]]; then
      replace_placeholder_in_file "${root_path_}"
      restore_core_cmake_references_in_file "${root_path_}"
      continue
    fi

    while IFS= read -r -d '' path_; do
      replace_placeholder_in_file "${path_}"
      restore_core_cmake_references_in_file "${path_}"
    done < <(find "${root_path_}" -type f -print0)
  done

  for root_path_ in "${copied_roots[@]}"; do
    [[ -e "${root_path_}" || -L "${root_path_}" ]] || continue

    while IFS= read -r -d '' path_; do
      path_dir_="$(dirname "${path_}")"
      path_base_="$(basename "${path_}")"
      new_base_="$(replace_placeholder_in_path_component "${path_base_}")"
      [[ "${new_base_}" != "${path_base_}" ]] || continue
      new_path_="${path_dir_}/${new_base_}"
      [[ ! -e "${new_path_}" && ! -L "${new_path_}" ]] || die "Rename collision: ${new_path_}"
      mv "${path_}" "${new_path_}"
    done < <(find "${root_path_}" -depth -name '*template_project*' -print0)
  done
}

copy_overlay() {
  require_no_conflicts
  confirm_apply

  copy_ros2_tree
  copy_required_file "build_ros2.sh" "build_ros2.sh"
  copy_optional_file_if_possible "doc/ros2_overlay.md" "doc/ros2_overlay.md"
  if ((NO_CI)); then
    info "skipping CI workflow because --no-ci is set"
  else
    copy_optional_file_if_possible ".github/workflows/build_ros2_overlay.yml" ".github/workflows/build_ros2_overlay.yml"
  fi

  copy_colcon_marker_if_possible "python/COLCON_IGNORE"
  copy_colcon_marker_if_possible "lib/COLCON_IGNORE"
  copy_colcon_marker_if_possible "examples/COLCON_IGNORE"
  copy_colcon_marker_if_possible "tests/COLCON_IGNORE"

  rename_copied_paths
}

run_verify() {
  local scratch_dir_

  scratch_dir_="$(mktemp -d /tmp/add_ros2_support_verify_XXXXXX)"
  VERIFY_SCRATCH_DIR="${scratch_dir_}"
  trap 'rm -rf -- "${VERIFY_SCRATCH_DIR:-}"' EXIT
  info "verifying standalone configure in ${scratch_dir_}"
  cmake -S "${ROOT_DIR}" -B "${scratch_dir_}/build" -DENABLE_TESTS=OFF
}

print_post_apply_checklist() {
  cat <<EOF

Post-apply checklist:
  1. Adapt the fenced core-call seam in ros2/${ros_package_prefix}_ros/src/conversions.cpp to a real ${cmake_project_name} API.
     Review ros2/${ros_package_prefix}_ros/src/CTemplateLifecycleNode.cpp only for ROS node wiring changes.
  2. Run ./build_ros2.sh --clean in the target project.
  3. Adopt the root CMake metadata contract, including PROJECT_METADATA_ONLY, and upgrade
     generate_version.sh if it predates project metadata sync. Then run
     ./generate_version.sh --sync-ros2 to synchronize project metadata.
  4. If the target removed optional CUDA or OptiX support, manually tailor the copied
     build_ros2.sh facade, shim CMake options, docs, and CI so unsupported options are not advertised.
EOF
}

main() {
  parse_args "$@"
  if ((VERIFY && !APPLY)); then
    die "--verify requires --apply"
  fi
  validate_source
  validate_target

  if ((LIST_ONLY)); then
    print_rollout_plan
    exit 0
  fi

  if ((APPLY)); then
    copy_overlay
    if ((VERIFY)); then
      run_verify
    fi
    print_post_apply_checklist
    info "ROS 2 overlay added to ${ROOT_DIR}"
    exit 0
  fi

  usage
  exit 1
}

main "$@"
