#!/usr/bin/env bash
# Remove template-development-only files after cloning this repository into a real project.

set -Eeuo pipefail
IFS=$'\n\t'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}"
APPLY=0
ASSUME_YES=0
LIST_ONLY=0
KEEP_PROFILING=0
REMOVE_ROS2=0
PROJECT_NAMESPACE=""
TEMPORARY_PATHS=()

info() { printf '\033[34m[INFO]\033[0m %s\n' "$*"; }
warn() { printf '\033[33m[WARN]\033[0m %s\n' "$*" >&2; }
die() { printf '\033[31m[ERROR]\033[0m %s\n' "$*" >&2; exit 1; }

cleanup_temporary_paths() {
    local temporary_path_

    for temporary_path_ in "${TEMPORARY_PATHS[@]}"; do
        if [[ -n "${temporary_path_}" ]]; then
            rm -f -- "${temporary_path_}" || true
        fi
    done
}

trap cleanup_temporary_paths EXIT

usage() {
    cat <<'EOF'
Usage:
  ./tailor_template_cleanup.sh --list
  ./tailor_template_cleanup.sh --apply --project-namespace <identifier> [--yes] [--root <dir>] [--keep-profiling] [--remove-ros2]

Purpose:
  Remove files that are only useful while developing cpp_cuda_template_project
  itself while preserving reusable project files.

Options:
  --list          Print the cleanup list and exit.
  --apply         Remove template-owned files and tailor retained content.
  --yes           Do not prompt before applying.
  --project-namespace <identifier>
                  Replace the template_project logger namespace.
  --root <dir>    Project root to clean. Defaults to the script directory.
  --keep-profiling
                  Keep profiling/ scripts. By default profiling/ is removed.
  --remove-ros2   Remove the optional ROS 2 overlay. By default the overlay is kept.
  -h, --help      Show this help.
EOF
}

template_development_paths=(
    "AGENTS.md"
    "CLAUDE.md"
    "CONTEXT.md"
    "TODO"
    "cpp_cuda_template_project.code-workspace"
    "doc/developments"
    "doc/reports"
)

optional_paths=(
    "profiling"
)

ros2_overlay_paths=(
    "ros2"
    "build_ros2.sh"
    "add_ros2_support.sh"
    "python/COLCON_IGNORE"
    "lib/COLCON_IGNORE"
    "examples/COLCON_IGNORE"
    "tests/COLCON_IGNORE"
    ".github/workflows/build_ros2_overlay.yml"
    "doc/ros2_overlay.md"
)

ros2_overlay_doc_paths=(
    "README.md"
    "AGENTS.md"
    "CLAUDE.md"
    "doc/bootstrap_prompts.md"
    "doc/template_usage.md"
    "doc/versioning.md"
)

logger_namespace_paths=(
    "src/utils/logging/CLogger.h"
    "src/utils/logging/CLogger.cpp"
    "src/bin/example_program.cpp"
    "src/template_src/placeholder.cpp"
    "tests/template_test/testProjectLogger.cpp"
    "doc/logging.md"
)

print_cleanup_list() {
    cat <<'EOF'
Template-development-only files/directories removed by --apply:
EOF
    for path_ in "${template_development_paths[@]}"; do
        printf '  - %s\n' "${path_}"
    done
    if ((KEEP_PROFILING)); then
        printf '  - profiling (kept because --keep-profiling is set)\n'
    else
        printf '  - profiling\n'
    fi
    cat <<'EOF'

ROS 2 overlay:
  - ROS 2 overlay KEPT by default; pass --remove-ros2 to strip it.
EOF
    if ((REMOVE_ROS2)); then
        printf '  - --remove-ros2 is set; these paths will be removed when present:\n'
        for path_ in "${ros2_overlay_paths[@]}"; do
            printf '    - %s\n' "${path_}"
        done
    fi
    cat <<'EOF'

Content edits made by --apply:
  - With --remove-ros2, strip <!-- ros2-overlay-begin/end --> fenced doc blocks.

Logger namespace edit made by --apply:
  - --project-namespace replaces template_project::logging in the reusable logger files.

Not removed:
  - cmake/ production modules, including TensorRT discovery/integration and
    Python/MATLAB wrapper staging support.
  - build_lib.sh, generate_version.sh, docs workflow files, issue forms, and docs guides.
  - src/utils/logging/ and doc/logging.md, because the logger is reusable project infrastructure.
  - Root and test CMake files; tailoring never reconstructs build-system files.
  - Starter C++/Python/CUDA tests, fixtures, and MATLAB wrapper runtime checks.
  - Downstream custom tests, including CMake-script tests.
  - Generic project workflows; --remove-ros2 removes only the ROS workflow.
  - .devcontainer, .vscode, examples/, and toolchains, because they are reusable project infrastructure.
  - profiling/ only when --keep-profiling is set.
  - ROS 2 overlay files unless --remove-ros2 is set.
EOF
}

parse_args() {
    while (($# > 0)); do
        case "$1" in
            --list)
                LIST_ONLY=1
                shift
                ;;
            --apply)
                APPLY=1
                shift
                ;;
            --yes|-y)
                ASSUME_YES=1
                shift
                ;;
            --project-namespace)
                [[ $# -ge 2 ]] || die "--project-namespace requires an identifier"
                PROJECT_NAMESPACE="$2"
                shift 2
                ;;
            --keep-profiling)
                KEEP_PROFILING=1
                shift
                ;;
            --remove-ros2)
                REMOVE_ROS2=1
                shift
                ;;
            --root)
                [[ $# -ge 2 ]] || die "--root requires a directory"
                ROOT_DIR="$2"
                shift 2
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

validate_project_namespace() {
    [[ -n "${PROJECT_NAMESPACE}" ]] \
        || die "--project-namespace is required with --apply"
    [[ "${PROJECT_NAMESPACE}" =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]] \
        || die "Invalid project namespace: ${PROJECT_NAMESPACE}"
    [[ "${PROJECT_NAMESPACE}" != "template_project" ]] \
        || die "Invalid project namespace: template_project"
}

validate_root() {
    [[ -d "${ROOT_DIR}" ]] || die "Root directory does not exist: ${ROOT_DIR}"
    ROOT_DIR="$(cd "${ROOT_DIR}" && pwd)"
    [[ -f "${ROOT_DIR}/CMakeLists.txt" ]] || die "Missing CMakeLists.txt in root: ${ROOT_DIR}"
    [[ -f "${ROOT_DIR}/build_lib.sh" ]] || die "Missing build_lib.sh in root: ${ROOT_DIR}"
}

tailor_logger_namespace() {
    local relative_path_
    local source_file_
    local tmp_

    for relative_path_ in "${logger_namespace_paths[@]}"; do
        source_file_="${ROOT_DIR}/${relative_path_}"
        [[ -f "${source_file_}" ]] || continue

        if ! grep -Fq "template_project::logging" "${source_file_}"; then
            info "logger namespace already tailored in ${relative_path_}"
            continue
        fi

        tmp_="$(mktemp "${source_file_}.tmp.XXXXXX")"
        TEMPORARY_PATHS+=("${tmp_}")
        sed "s/template_project::logging/${PROJECT_NAMESPACE}::logging/g" \
            "${source_file_}" > "${tmp_}"
        chmod --reference="${source_file_}" "${tmp_}"
        mv -f -- "${tmp_}" "${source_file_}"
        info "tailored logger namespace in ${relative_path_}"
    done
}

remove_path() {
    local relative_path_="$1"
    local absolute_path_="${ROOT_DIR}/${relative_path_}"

    if [[ ! -e "${absolute_path_}" && ! -L "${absolute_path_}" ]]; then
        info "skip missing ${relative_path_}"
        return
    fi

    rm -rf -- "${absolute_path_}"
    info "removed ${relative_path_}"
}

filter_ros2_overlay_doc() {
    local doc_file_="$1"

    awk '
        /<!--[[:space:]]*ros2-overlay-begin[[:space:]]*-->/ {
            if (in_ros2_overlay_) {
                exit 1
            }
            in_ros2_overlay_ = 1
            next
        }
        /<!--[[:space:]]*ros2-overlay-end[[:space:]]*-->/ {
            if (!in_ros2_overlay_) {
                exit 1
            }
            in_ros2_overlay_ = 0
            next
        }
        !in_ros2_overlay_ { print }
        END {
            if (in_ros2_overlay_) {
                exit 1
            }
        }
    ' "${doc_file_}"
}

validate_ros2_overlay_doc_fences() {
    local relative_path_
    local doc_file_

    ((REMOVE_ROS2)) || return 0

    for relative_path_ in "${ros2_overlay_doc_paths[@]}"; do
        doc_file_="${ROOT_DIR}/${relative_path_}"
        [[ -f "${doc_file_}" ]] || continue

        if ! filter_ros2_overlay_doc "${doc_file_}" > /dev/null; then
            die "Malformed ROS 2 overlay fence in ${relative_path_}"
        fi
    done
}

strip_ros2_overlay_doc_fences() {
    local relative_path_
    local doc_file_
    local tmp_

    ((REMOVE_ROS2)) || return

    for relative_path_ in "${ros2_overlay_doc_paths[@]}"; do
        doc_file_="${ROOT_DIR}/${relative_path_}"
        if [[ ! -f "${doc_file_}" ]]; then
            info "skip missing ${relative_path_}"
            continue
        fi

        if ! grep -q "<!-- ros2-overlay-begin -->" "${doc_file_}" && \
           ! grep -q "<!-- ros2-overlay-end -->" "${doc_file_}"; then
            info "no ROS 2 overlay fence in ${relative_path_}"
            continue
        fi

        tmp_="$(mktemp "${doc_file_}.tmp.XXXXXX")"
        TEMPORARY_PATHS+=("${tmp_}")
        if filter_ros2_overlay_doc "${doc_file_}" > "${tmp_}"; then
            chmod --reference="${doc_file_}" "${tmp_}"
            mv -f -- "${tmp_}" "${doc_file_}"
            info "stripped ROS 2 overlay fence from ${relative_path_}"
        else
            die "Malformed ROS 2 overlay fence in ${relative_path_}"
        fi
    done
}

confirm_apply() {
    ((APPLY)) || return
    ((ASSUME_YES)) && return

    printf 'Apply template cleanup to %s? Type "yes" to continue: ' "${ROOT_DIR}"
    read -r answer_
    [[ "${answer_}" == "yes" ]] || die "Aborted"
}

main() {
    parse_args "$@"

    if ((LIST_ONLY)); then
        print_cleanup_list
        exit 0
    fi

    if ((! APPLY)); then
        usage
        exit 1
    fi

    validate_project_namespace
    validate_root
    validate_ros2_overlay_doc_fences
    print_cleanup_list
    confirm_apply

    tailor_logger_namespace

    for path_ in "${template_development_paths[@]}"; do
        remove_path "${path_}"
    done
    if ((! KEEP_PROFILING)); then
        for path_ in "${optional_paths[@]}"; do
            remove_path "${path_}"
        done
    else
        info "keeping profiling/"
    fi

    if ((REMOVE_ROS2)); then
        for path_ in "${ros2_overlay_paths[@]}"; do
            remove_path "${path_}"
        done
        strip_ros2_overlay_doc_fences
    else
        info "keeping ROS 2 overlay; pass --remove-ros2 to strip it"
    fi

    info "template cleanup complete"
}

main "$@"
