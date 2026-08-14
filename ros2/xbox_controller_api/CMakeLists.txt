cmake_minimum_required(VERSION 3.16)

project(xbox_controller_api_colcon_shim LANGUAGES NONE)

get_filename_component(XBOX_CONTROLLER_API_REPOSITORY_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE
)

set(ENABLE_TESTS OFF CACHE BOOL "Disable core template tests in the ROS 2 overlay build." FORCE)
set(ENABLE_FETCH_CATCH2 OFF CACHE BOOL "Do not fetch Catch2 while building the ROS 2 overlay." FORCE)
set(ENABLE_SDL2 OFF CACHE BOOL "Do not build the SDL2 gamepad backend in the ROS 2 overlay." FORCE)

# The root CMakeLists appends PROJECT_SOURCE_DIR/cmake before its project()
# call, which points at this shim during nested configure. Keep the real helper
# directory first so root includes resolve without patching the C++ template.
list(PREPEND CMAKE_MODULE_PATH "${XBOX_CONTROLLER_API_REPOSITORY_ROOT}/cmake")

add_subdirectory(
    "${XBOX_CONTROLLER_API_REPOSITORY_ROOT}"
    "${CMAKE_CURRENT_BINARY_DIR}/xbox_controller_api_core"
)

# Older derived repos may still write their build VERSION file to the top-level
# CMAKE_BINARY_DIR. Mirror it into the nested project binary dir expected by the
# root install rule, without touching source files or requiring a root CMake edit.
set(_ros2_overlay_legacy_version_file "${CMAKE_BINARY_DIR}/VERSION")
set(_ros2_overlay_nested_version_file "${CMAKE_CURRENT_BINARY_DIR}/xbox_controller_api_core/VERSION")
if(EXISTS "${_ros2_overlay_legacy_version_file}" AND NOT EXISTS "${_ros2_overlay_nested_version_file}")
    configure_file(
        "${_ros2_overlay_legacy_version_file}"
        "${_ros2_overlay_nested_version_file}"
        COPYONLY
    )
endif()
unset(_ros2_overlay_legacy_version_file)
unset(_ros2_overlay_nested_version_file)
