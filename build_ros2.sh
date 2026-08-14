#!/usr/bin/env bash
# Build the optional ROS 2 overlay without involving the standalone C++ build.

set -Eeuo pipefail
IFS=$'\n\t'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}"
WORKSPACE_DIR="${ROOT_DIR}/ros2"
ROS_DISTRO_NAME="${ROS_DISTRO:-jazzy}"

build_type="RelWithDebInfo"
clean=false
skip_tests=false
enable_cuda=false
enable_optix=false
metadata_sync=true
packages_select=()
cmake_args=()
colcon_args=()

info() { printf '\033[34m[INFO]\033[0m %s\n' "$*"; }
warn() { printf '\033[33m[WARN]\033[0m %s\n' "$*" >&2; }
die() { printf '\033[31m[ERROR]\033[0m %s\n' "$*" >&2; exit 1; }

usage() {
  cat <<'EOF'
Usage:
  ./build_ros2.sh [options]

Purpose:
  Build the optional ROS 2 overlay in ros2/ with colcon. The standalone C++
  library still builds with ./build_lib.sh and never requires ROS.

Options:
  --clean                    Remove ros2/build, ros2/install, and ros2/log first.
  --skip-tests               Skip colcon test and colcon test-result.
  --debug                    Use CMAKE_BUILD_TYPE=Debug.
  --release                  Use CMAKE_BUILD_TYPE=Release.
  --relwithdebinfo           Use CMAKE_BUILD_TYPE=RelWithDebInfo (default).
  --build-type <type>        Use an explicit CMake build type.
  --packages-select <pkg...> Build/test selected packages.
  --cuda                     Enable core CUDA support.
  --optix                    Enable core OptiX support; implies --cuda.
  --cmake-arg <arg>          Append one CMake argument. Repeatable.
  --colcon-arg <arg>         Append one colcon build argument. Repeatable.
  --no-version-sync          Do not synchronize ROS 2 package metadata (legacy flag name).
  -h, --help                 Show this help.

Examples:
  ./build_ros2.sh --clean
  ./build_ros2.sh --packages-select template_project template_project_interfaces
  ./build_ros2.sh --cuda --cmake-arg -DCMAKE_CUDA_ARCHITECTURES=87
EOF
}

parse_args() {
  while (($# > 0)); do
    case "$1" in
      --clean)
        clean=true
        shift
        ;;
      --skip-tests)
        skip_tests=true
        shift
        ;;
      --debug)
        build_type="Debug"
        shift
        ;;
      --release)
        build_type="Release"
        shift
        ;;
      --relwithdebinfo)
        build_type="RelWithDebInfo"
        shift
        ;;
      --build-type)
        [[ $# -ge 2 ]] || die "--build-type requires a value"
        build_type="$2"
        shift 2
        ;;
      --packages-select)
        shift
        [[ $# -gt 0 && "$1" != --* ]] || die "--packages-select requires at least one package"
        while [[ $# -gt 0 && "$1" != --* ]]; do
          packages_select+=("$1")
          shift
        done
        ;;
      --cuda)
        enable_cuda=true
        shift
        ;;
      --optix)
        enable_optix=true
        enable_cuda=true
        shift
        ;;
      --cmake-arg)
        [[ $# -ge 2 ]] || die "--cmake-arg requires a value"
        cmake_args+=("$2")
        shift 2
        ;;
      --colcon-arg)
        [[ $# -ge 2 ]] || die "--colcon-arg requires a value"
        colcon_args+=("$2")
        shift 2
        ;;
      --no-version-sync)
        metadata_sync=false
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

source_ros_environment() {
  local setup_file_="/opt/ros/${ROS_DISTRO_NAME}/setup.bash"

  if [[ ! -f "${setup_file_}" ]]; then
    die "ROS setup file not found: ${setup_file_}. Install ROS 2 ${ROS_DISTRO_NAME} or set ROS_DISTRO to an installed distro. The C++ library itself builds with ./build_lib.sh and never needs ROS."
  fi

  # ROS setup scripts may read unset environment variables.
  set +u
  # shellcheck disable=SC1090
  source "${setup_file_}"
  set -u

  command -v colcon >/dev/null 2>&1 || die "colcon was not found after sourcing ${setup_file_}."
}

touch_root_colcon_ignore_markers() {
  local path_

  shopt -s nullglob
  for path_ in "${ROOT_DIR}"/build* "${ROOT_DIR}"/install "${ROOT_DIR}"/template_subbuild; do
    [[ -d "${path_}" ]] || continue
    if touch "${path_}/COLCON_IGNORE" 2>/dev/null; then
      info "ensured COLCON_IGNORE in ${path_#"${ROOT_DIR}"/}"
    else
      warn "could not write COLCON_IGNORE in ${path_}"
    fi
  done
  shopt -u nullglob
}

sync_ros2_package_metadata() {
  if [[ "${metadata_sync}" != true ]]; then
    return
  fi

  if [[ ! -x "${ROOT_DIR}/generate_version.sh" ]]; then
    warn "generate_version.sh is missing or not executable; skipping ROS 2 package metadata sync"
    return
  fi

  if ! grep -q -- "ROS2_PROJECT_METADATA_SYNC=1" "${ROOT_DIR}/generate_version.sh"; then
    warn "generate_version.sh predates project metadata sync; skipping ROS 2 package metadata sync"
    return
  fi

  if "${ROOT_DIR}/generate_version.sh" --sync-ros2; then
    info "synchronized ROS 2 package metadata"
  else
    warn "ROS 2 package metadata sync failed; continuing with existing package.xml metadata"
  fi
}

run_colcon_build() {
  local cuda_flag_
  local optix_flag_
  local build_cmd_
  local package_
  local test_cmd_

  cuda_flag_="OFF"
  optix_flag_="OFF"
  [[ "${enable_cuda}" == true ]] && cuda_flag_="ON"
  [[ "${enable_optix}" == true ]] && optix_flag_="ON"

  build_cmd_=(
    colcon build
    --symlink-install
  )
  if ((${#packages_select[@]} > 0)); then
    build_cmd_+=(--packages-select "${packages_select[@]}")
  fi
  if ((${#colcon_args[@]} > 0)); then
    build_cmd_+=("${colcon_args[@]}")
  fi
  build_cmd_+=(
    --cmake-args
    "-DCMAKE_BUILD_TYPE=${build_type}"
    "-DTEMPLATE_PROJECT_ENABLE_CUDA=${cuda_flag_}"
    "-DTEMPLATE_PROJECT_ENABLE_OPTIX=${optix_flag_}"
    "${cmake_args[@]}"
  )

  info "Workspace : ${WORKSPACE_DIR}"
  info "ROS distro: ${ROS_DISTRO_NAME}"
  info "Build type: ${build_type}"
  info "CUDA      : ${cuda_flag_}"
  info "OptiX     : ${optix_flag_}"

  (
    cd "${WORKSPACE_DIR}"
    "${build_cmd_[@]}"
  )

  if [[ "${skip_tests}" == true ]]; then
    return
  fi

  test_cmd_=(colcon test --event-handlers console_direct+)
  if ((${#packages_select[@]} > 0)); then
    test_cmd_+=(--packages-select "${packages_select[@]}")
  fi

  (
    cd "${WORKSPACE_DIR}"
    "${test_cmd_[@]}"
    if ((${#packages_select[@]} > 0)); then
      for package_ in "${packages_select[@]}"; do
        colcon test-result --test-result-base "build/${package_}" --verbose
      done
    else
      colcon test-result --verbose
    fi
  )
}

main() {
  parse_args "$@"

  [[ -d "${WORKSPACE_DIR}" ]] || die "ROS 2 workspace directory not found: ${WORKSPACE_DIR}"
  source_ros_environment
  touch_root_colcon_ignore_markers

  if [[ "${clean}" == true ]]; then
    rm -rf "${WORKSPACE_DIR}/build" "${WORKSPACE_DIR}/install" "${WORKSPACE_DIR}/log"
  fi

  sync_ros2_package_metadata
  run_colcon_build
}

main "$@"
