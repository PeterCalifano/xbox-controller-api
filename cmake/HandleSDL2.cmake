# CMake configuration to handle SDL2 discovery and linking
include_guard(GLOBAL)
include(CMakeParseArguments)

if (NOT DEFINED ENABLE_SDL2)
    option(ENABLE_SDL2 "Enable the SDL2-backed gamepad input backend" ON)
endif()

if (NOT DEFINED ENABLE_SDL2_STRICT)
    option(ENABLE_SDL2_STRICT
           "Fail configuration when SDL2 support is requested but SDL2 is unavailable"
           OFF)
endif()

# Resolve SDL2 and record the outcome on an INTERFACE target.
#
# ENABLE_SDL2 expresses what the caller asked for, while SDL2_ENABLED reports
# what was actually resolved. Keeping the two separate is what allows the
# default non-strict mode to warn and continue without leaving the request flag
# contradicting the build that was produced.
function(handle_sdl2)
    set(oneValueArgs TARGET)
    cmake_parse_arguments(HSDL "" "${oneValueArgs}" "" ${ARGN})

    if(NOT HSDL_TARGET)
        set(HSDL_TARGET sdl2_compile_interface)
    endif()

    # The interface target always exists, even when SDL2 is unavailable, so
    # dependents can reference it unconditionally instead of guarding every
    # link site. When disabled it simply carries no usage requirements.
    if(NOT TARGET ${HSDL_TARGET})
        add_library(${HSDL_TARGET} INTERFACE)
    endif()

    set(SDL2_ENABLED OFF PARENT_SCOPE)
    set(SDL2_FOUND_VIA_CONFIG OFF PARENT_SCOPE)
    set(SDL2_RESOLVED_VERSION "" PARENT_SCOPE)

    if(NOT ENABLE_SDL2)
        message(STATUS "SDL2 gamepad backend disabled by configuration (ENABLE_SDL2=OFF).")
        return()
    endif()

    set(_bFoundViaConfig OFF)
    set(_charResolvedVersion "")
    set(_charUnavailableReason "")

    # Prefer the upstream CMake package. It is the only discovery mode whose
    # imported target a consumer of the installed export can recreate, through
    # find_dependency(SDL2).
    find_package(SDL2 CONFIG QUIET)

    if(SDL2_FOUND AND TARGET SDL2::SDL2)
        target_link_libraries(${HSDL_TARGET} INTERFACE SDL2::SDL2)

        set(_bFoundViaConfig ON)
        set(_charResolvedVersion "${SDL2_VERSION}")
    else()
        find_package(PkgConfig QUIET)

        if(NOT PkgConfig_FOUND)
            set(_charUnavailableReason
                "ENABLE_SDL2 is ON but neither an SDL2 CMake package nor PkgConfig was found.")
        else()
            pkg_check_modules(SDL2_PC QUIET IMPORTED_TARGET sdl2)

            if(NOT SDL2_PC_FOUND)
                set(_charUnavailableReason
                    "ENABLE_SDL2 is ON but SDL2 was found through neither its CMake package nor pkg-config.")
            else()
                # Link the resolved flags rather than the PkgConfig:: imported
                # target. This interface target is installed and exported, and an
                # imported target created by pkg_check_modules does not exist in
                # a consumer's project, which would break its configure step.
                target_link_libraries(${HSDL_TARGET} INTERFACE ${SDL2_PC_LINK_LIBRARIES})

                if(SDL2_PC_INCLUDE_DIRS)
                    target_include_directories(${HSDL_TARGET} INTERFACE ${SDL2_PC_INCLUDE_DIRS})
                endif()

                set(_charResolvedVersion "${SDL2_PC_VERSION}")
            endif()
        endif()
    endif()

    # A missing dependency is fatal only when the caller asked for that, so an
    # ordinary developer build degrades to the stub backend while CI can demand
    # the real one.
    if(_charUnavailableReason)
        if(ENABLE_SDL2_STRICT)
            message(FATAL_ERROR "${_charUnavailableReason}")
        endif()

        message(WARNING "${_charUnavailableReason} The SDL2 gamepad backend will be disabled.")
        return()
    endif()

    target_compile_definitions(${HSDL_TARGET} INTERFACE __SDL2_ENABLED__=1)

    set(SDL2_ENABLED ON PARENT_SCOPE)
    set(SDL2_FOUND_VIA_CONFIG ${_bFoundViaConfig} PARENT_SCOPE)
    set(SDL2_RESOLVED_VERSION "${_charResolvedVersion}" PARENT_SCOPE)

    if(_bFoundViaConfig)
        message(STATUS "SDL2 gamepad backend enabled via CMake package (version ${_charResolvedVersion})")
    else()
        message(STATUS "SDL2 gamepad backend enabled via pkg-config (version ${_charResolvedVersion})")
    endif()
endfunction()
