#!/usr/bin/env bash
# Replace selected MATLAB runtime-library symlinks with host-default libraries.
# Inspection is the default; --apply and --restore are explicit root-only modes.

set -Eeuo pipefail

readonly BACKUP_SUFFIX='.matlab-backup'
readonly DEFAULT_MATLAB_PREFIX='/usr/local/MATLAB'
readonly ORIGINAL_ARGUMENTS=("$@")

declare MATLAB_ROOT=''
declare MATLAB_ROOT_ARGUMENT=''
declare MATLAB_VERSION=''
declare MATLAB_PREFIX="${DEFAULT_MATLAB_PREFIX}"
declare MODE='dry-run'
declare CAPTURED_OUTPUT=''
declare -i SELECT_LIBSTDCXX=0
declare -i SELECT_OPENCV=0

declare -a LINK_PATHS=()
declare -a CURRENT_TARGETS=()
declare -a PLANNED_TARGETS=()
declare -a LIBRARY_FAMILIES=()
declare -A HOST_LIBRARY_TARGETS=()
declare -A HOST_LIBRARY_SONAMES=()
declare LDCONFIG_CACHE=''

usage() {
  cat <<'EOF'
Use host-default C++ and OpenCV libraries in a MATLAB installation.

Usage:
  use_system_matlab_libraries.sh [location] [selection] [mode]

Location:
  --matlab-root PATH          Use an exact MATLAB installation root.
  --matlab-version RELEASE    Use RELEASE below --matlab-prefix.
  --matlab-prefix PATH        MATLAB prefix (default: /usr/local/MATLAB).

Selection (at least one is required):
  --libstdcxx                 Manage active libstdc++.so.6 links.
  --opencv                    Manage MATLAB libopencv SONAME links.
  --all                       Select both library families.

Mode:
  [no mode]                   Inspect and print the plan without changes.
  --apply                     Back up and replace links; requires sudo/root.
  --restore                   Restore links from backups; requires sudo/root.

Options:
  -h, --help                  Show this help.

Examples:
  ./scripts/use_system_matlab_libraries.sh --matlab-version R2024b --all
  sudo ./scripts/use_system_matlab_libraries.sh --matlab-version R2024b --all --apply
  sudo ./scripts/use_system_matlab_libraries.sh --matlab-version R2024b --all --restore
EOF
}

info() {
  printf '[INFO] %s\n' "$*"
}

warn() {
  printf '[WARN] %s\n' "$*" >&2
}

die() {
  printf '[ERROR] %s\n' "$*" >&2
  exit 1
}

print_command_() {
  printf '[CMD]'
  printf ' %q' "$@"
  printf '\n'
}

run_capture_() {
  local status_

  print_command_ "$@"
  set +e
  CAPTURED_OUTPUT="$("$@" 2>&1)"
  status_=$?
  set -e

  if [[ -n "${CAPTURED_OUTPUT}" ]]; then
    printf '[OUTPUT]\n%s\n' "${CAPTURED_OUTPUT}"
  else
    printf '[OUTPUT] <none>\n'
  fi
  printf '[EXIT] %d\n' "${status_}"

  ((status_ == 0)) || die "Command failed with exit status ${status_}: $*"
}

parse_arguments_() {
  while (($# > 0)); do
    case "$1" in
      --matlab-root)
        (($# >= 2)) || die '--matlab-root requires a path.'
        [[ -z "${MATLAB_ROOT_ARGUMENT}" ]] || die '--matlab-root may be specified only once.'
        MATLAB_ROOT_ARGUMENT="$2"
        shift 2
        ;;
      --matlab-version)
        (($# >= 2)) || die '--matlab-version requires a release such as R2024b.'
        [[ -z "${MATLAB_VERSION}" ]] || die '--matlab-version may be specified only once.'
        MATLAB_VERSION="$2"
        shift 2
        ;;
      --matlab-prefix)
        (($# >= 2)) || die '--matlab-prefix requires a path.'
        MATLAB_PREFIX="$2"
        shift 2
        ;;
      --libstdcxx)
        SELECT_LIBSTDCXX=1
        shift
        ;;
      --opencv)
        SELECT_OPENCV=1
        shift
        ;;
      --all)
        SELECT_LIBSTDCXX=1
        SELECT_OPENCV=1
        shift
        ;;
      --apply)
        [[ "${MODE}" == 'dry-run' ]] || die '--apply conflicts with --restore.'
        MODE='apply'
        shift
        ;;
      --restore)
        [[ "${MODE}" == 'dry-run' ]] || die '--restore conflicts with --apply.'
        MODE='restore'
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

  [[ -z "${MATLAB_ROOT_ARGUMENT}" || -z "${MATLAB_VERSION}" ]] ||
    die '--matlab-root and --matlab-version are mutually exclusive.'
  ((SELECT_LIBSTDCXX || SELECT_OPENCV)) ||
    die 'Select at least one library family: --libstdcxx, --opencv, or --all.'
}

canonicalize_path_() {
  local input_path_="$1"

  [[ -e "${input_path_}" || -L "${input_path_}" ]] || die "Path does not exist: ${input_path_}"
  run_capture_ readlink -f -- "${input_path_}"
  [[ -n "${CAPTURED_OUTPUT}" ]] || die "Unable to canonicalize path: ${input_path_}"
}

validate_matlab_root_() {
  local candidate_root_="$1"

  [[ -x "${candidate_root_}/bin/matlab" ]] ||
    die "MATLAB launcher is missing or not executable: ${candidate_root_}/bin/matlab"
  [[ -d "${candidate_root_}/sys/os/glnxa64" ]] ||
    die "MATLAB Linux runtime directory is missing: ${candidate_root_}/sys/os/glnxa64"
}

resolve_matlab_root_() {
  local candidate_path_=''
  local canonical_launcher_=''
  local -a discovered_launchers_=()

  if [[ -n "${MATLAB_ROOT_ARGUMENT}" ]]; then
    candidate_path_="${MATLAB_ROOT_ARGUMENT}"
  elif [[ -n "${MATLAB_VERSION}" ]]; then
    candidate_path_="${MATLAB_PREFIX%/}/${MATLAB_VERSION}"
  elif command -v matlab >/dev/null 2>&1; then
    candidate_path_="$(command -v matlab)"
    info "MATLAB launcher found on PATH: ${candidate_path_}"
    canonicalize_path_ "${candidate_path_}"
    canonical_launcher_="${CAPTURED_OUTPUT}"
    [[ "${canonical_launcher_}" == */bin/matlab ]] ||
      die "Cannot infer MATLAB root from launcher: ${canonical_launcher_}"
    candidate_path_="${canonical_launcher_%/bin/matlab}"
  else
    shopt -s nullglob
    discovered_launchers_=("${MATLAB_PREFIX%/}"/R*/bin/matlab)
    shopt -u nullglob

    ((${#discovered_launchers_[@]} > 0)) ||
      die "No MATLAB installation found below ${MATLAB_PREFIX}."
    if ((${#discovered_launchers_[@]} > 1)); then
      printf '[ERROR] Multiple MATLAB installations found:\n' >&2
      printf '  %s\n' "${discovered_launchers_[@]}" >&2
      die 'Use --matlab-root or --matlab-version to select one installation.'
    fi
    candidate_path_="${discovered_launchers_[0]%/bin/matlab}"
  fi

  canonicalize_path_ "${candidate_path_}"
  MATLAB_ROOT="${CAPTURED_OUTPUT}"
  validate_matlab_root_ "${MATLAB_ROOT}"
  info "MATLAB root: ${MATLAB_ROOT}"
}

load_ldconfig_cache_() {
  [[ -n "${LDCONFIG_CACHE}" ]] && return
  command -v ldconfig >/dev/null 2>&1 || die 'ldconfig is required but was not found on PATH.'
  run_capture_ ldconfig -p
  LDCONFIG_CACHE="${CAPTURED_OUTPUT}"
}

resolve_host_library_() {
  local lookup_name_="$1"
  local family_="$2"
  local line_=''
  local target_path_=''
  local canonical_target_=''
  local file_description_=''
  local soname_output_=''
  local soname_regex_=''
  local soname_=''

  if [[ -n "${HOST_LIBRARY_TARGETS[${lookup_name_}]:-}" ]]; then
    return
  fi

  load_ldconfig_cache_
  while IFS= read -r line_; do
    if [[ "${line_}" == *"${lookup_name_} ("* &&
          "${line_}" == *'x86-64'* && "${line_}" == *'=> '* ]]; then
      target_path_="${line_##*=> }"
      break
    fi
  done <<<"${LDCONFIG_CACHE}"
  [[ -n "${target_path_}" ]] || die "Host library not found in ldconfig cache: ${lookup_name_}"

  canonicalize_path_ "${target_path_}"
  canonical_target_="${CAPTURED_OUTPUT}"
  [[ -f "${canonical_target_}" ]] || die "Host library is not a regular file: ${canonical_target_}"
  [[ "${canonical_target_}" != "${MATLAB_ROOT}" &&
     "${canonical_target_}" != "${MATLAB_ROOT}/"* ]] ||
    die "Resolved host library is inside MATLAB: ${canonical_target_}"

  run_capture_ file -L -- "${canonical_target_}"
  file_description_="${CAPTURED_OUTPUT}"
  [[ "${file_description_}" == *'ELF 64-bit'* && "${file_description_}" == *'x86-64'* ]] ||
    die "Host library is not an x86-64 ELF shared object: ${canonical_target_}"

  run_capture_ readelf -d -- "${canonical_target_}"
  soname_output_="${CAPTURED_OUTPUT}"
  [[ "${soname_output_}" == *"${lookup_name_}"* ]] ||
    die "Host library SONAME does not match ${family_}: ${canonical_target_}"

  soname_regex_="${lookup_name_//./\\.}\\.([0-9]+)"
  if [[ "${soname_output_}" =~ ${soname_regex_} ]]; then
    soname_="${BASH_REMATCH[0]}"
  else
    soname_="${lookup_name_}"
  fi

  HOST_LIBRARY_TARGETS["${lookup_name_}"]="${canonical_target_}"
  HOST_LIBRARY_SONAMES["${lookup_name_}"]="${soname_}"
  info "Host ${family_}: ${lookup_name_} -> ${canonical_target_}"
}

append_plan_entry_() {
  local link_path_="$1"
  local planned_target_="$2"
  local family_="$3"
  local current_target_

  [[ -L "${link_path_}" ]] || die "MATLAB library candidate is not a symlink: ${link_path_}"
  run_capture_ readlink -- "${link_path_}"
  current_target_="${CAPTURED_OUTPUT}"

  LINK_PATHS+=("${link_path_}")
  CURRENT_TARGETS+=("${current_target_}")
  PLANNED_TARGETS+=("${planned_target_}")
  LIBRARY_FAMILIES+=("${family_}")
}

planned_target_for_() {
  local link_path_="$1"
  local lookup_name_="$2"
  local family_="$3"
  local backup_path_="${link_path_}${BACKUP_SUFFIX}"

  if [[ "${MODE}" == 'restore' ]]; then
    [[ -L "${backup_path_}" ]] || die "Restore backup is missing or invalid: ${backup_path_}"
    run_capture_ readlink -- "${backup_path_}"
  else
    resolve_host_library_ "${lookup_name_}" "${family_}"
    CAPTURED_OUTPUT="${HOST_LIBRARY_TARGETS[${lookup_name_}]}"
  fi
}

discover_libstdcxx_() {
  local discovery_output_=''
  local link_path_=''
  local planned_target_=''
  local -i count_=0

  run_capture_ find "${MATLAB_ROOT}" -name 'libstdc++.so.6' -not -path '*/orig/*' -print
  discovery_output_="${CAPTURED_OUTPUT}"
  while IFS= read -r link_path_; do
    [[ -n "${link_path_}" ]] || continue
    planned_target_for_ "${link_path_}" 'libstdc++.so.6' 'libstdc++'
    planned_target_="${CAPTURED_OUTPUT}"
    append_plan_entry_ "${link_path_}" "${planned_target_}" 'libstdc++'
    count_+=1
  done <<<"${discovery_output_}"

  ((count_ > 0)) || die "No active libstdc++.so.6 links found below ${MATLAB_ROOT}."
}

discover_opencv_() {
  local opencv_dir_="${MATLAB_ROOT}/bin/glnxa64"
  local discovery_output_=''
  local link_path_=''
  local link_name_=''
  local lookup_name_=''
  local matlab_soname_version_=''
  local system_soname_=''
  local system_soname_version_=''
  local planned_target_=''
  local -i count_=0

  [[ -d "${opencv_dir_}" ]] || die "MATLAB OpenCV directory not found: ${opencv_dir_}"
  run_capture_ find "${opencv_dir_}" -maxdepth 1 -mindepth 1 -name 'libopencv_*.so.*' -print
  discovery_output_="${CAPTURED_OUTPUT}"

  while IFS= read -r link_path_; do
    [[ -n "${link_path_}" ]] || continue
    link_name_="${link_path_##*/}"
    [[ "${link_name_}" =~ ^(libopencv_.*\.so)\.([0-9]+)$ ]] || continue
    lookup_name_="${BASH_REMATCH[1]}"
    matlab_soname_version_="${BASH_REMATCH[2]}"

    planned_target_for_ "${link_path_}" "${lookup_name_}" 'OpenCV'
    planned_target_="${CAPTURED_OUTPUT}"
    if [[ "${MODE}" != 'restore' ]]; then
      system_soname_="${HOST_LIBRARY_SONAMES[${lookup_name_}]}"
      if [[ "${system_soname_}" =~ \.so\.([0-9]+)$ ]]; then
        system_soname_version_="${BASH_REMATCH[1]}"
        if [[ "${matlab_soname_version_}" != "${system_soname_version_}" ]]; then
          warn "OpenCV SONAME change: ${matlab_soname_version_} -> ${system_soname_version_} (${link_name_})"
        fi
      fi
    fi

    append_plan_entry_ "${link_path_}" "${planned_target_}" 'OpenCV'
    count_+=1
  done <<<"${discovery_output_}"

  ((count_ > 0)) || die "No MATLAB OpenCV SONAME links found in ${opencv_dir_}."
}

validate_backups_() {
  local index_
  local backup_path_=''

  [[ "${MODE}" == 'apply' ]] || return 0
  for index_ in "${!LINK_PATHS[@]}"; do
    [[ "${CURRENT_TARGETS[index_]}" != "${PLANNED_TARGETS[index_]}" ]] || continue
    backup_path_="${LINK_PATHS[index_]}${BACKUP_SUFFIX}"
    if [[ -e "${backup_path_}" || -L "${backup_path_}" ]]; then
      [[ -L "${backup_path_}" ]] || die "Backup exists but is not a symlink: ${backup_path_}"
    fi
  done
}

print_plan_() {
  local index_

  for index_ in "${!LINK_PATHS[@]}"; do
    if [[ "${CURRENT_TARGETS[index_]}" == "${PLANNED_TARGETS[index_]}" ]]; then
      printf '[UNCHANGED] %s: %s -> %s\n' \
        "${LIBRARY_FAMILIES[index_]}" "${LINK_PATHS[index_]}" "${CURRENT_TARGETS[index_]}"
    else
      printf '[%s] %s: %s\n' "${MODE^^}" "${LIBRARY_FAMILIES[index_]}" "${LINK_PATHS[index_]}"
      printf '  current: %s\n' "${CURRENT_TARGETS[index_]}"
      printf '  target : %s\n' "${PLANNED_TARGETS[index_]}"
    fi
  done
}

require_root_() {
  local effective_uid_

  [[ "${MODE}" != 'dry-run' ]] || return 0
  run_capture_ id -u
  effective_uid_="${CAPTURED_OUTPUT}"
  if [[ "${effective_uid_}" != '0' ]]; then
    printf '[ERROR] %s requires root privileges. Re-run with:\n  sudo' "${MODE}" >&2
    printf ' %q' "$0" "${ORIGINAL_ARGUMENTS[@]}" >&2
    printf '\n' >&2
    exit 1
  fi
}

apply_plan_() {
  local index_
  local link_path_=''
  local current_target_=''
  local planned_target_=''
  local backup_path_=''

  [[ "${MODE}" != 'dry-run' ]] || return 0
  for index_ in "${!LINK_PATHS[@]}"; do
    link_path_="${LINK_PATHS[index_]}"
    current_target_="${CURRENT_TARGETS[index_]}"
    planned_target_="${PLANNED_TARGETS[index_]}"
    [[ "${current_target_}" != "${planned_target_}" ]] || continue

    if [[ "${MODE}" == 'apply' ]]; then
      backup_path_="${link_path_}${BACKUP_SUFFIX}"
      if [[ ! -L "${backup_path_}" ]]; then
        run_capture_ ln -s -- "${current_target_}" "${backup_path_}"
        info "Backup created: ${backup_path_} -> ${current_target_}"
      else
        info "Backup retained: ${backup_path_}"
      fi
    fi

    run_capture_ ln -sfn -- "${planned_target_}" "${link_path_}"
    info "Link updated: ${link_path_} -> ${planned_target_}"
  done
}

main() {
  parse_arguments_ "$@"
  resolve_matlab_root_

  if ((SELECT_LIBSTDCXX)); then
    discover_libstdcxx_
  fi
  if ((SELECT_OPENCV)); then
    discover_opencv_
  fi

  validate_backups_
  print_plan_
  require_root_
  apply_plan_

  if [[ "${MODE}" == 'dry-run' ]]; then
    info 'Dry-run complete; no links were changed.'
  else
    info "${MODE^} complete. Backup links were retained."
  fi
}

main "$@"
