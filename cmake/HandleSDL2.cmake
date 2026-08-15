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

# Resolve SDL2 and record the result on an INTERFACE target.
# ENABLE_SDL2 is the request; SDL2_ENABLED is the resolved outcome.
function(handle_sdl2)
    set(oneValueArgs TARGET)
    cmake_parse_arguments(HSDL "" "${oneValueArgs}" "" ${ARGN})

    if(NOT HSDL_TARGET)
        set(HSDL_TARGET sdl2_compile_interface)
    endif()

    # The target always exists. Without SDL2 it has no usage requirements.
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

    # Prefer the CMake package because exported consumers can recreate its target.
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
                # Export resolved flags: PkgConfig imported targets are local to
                # this configure and cannot be used by installed consumers.
                target_link_libraries(${HSDL_TARGET} INTERFACE ${SDL2_PC_LINK_LIBRARIES})

                if(SDL2_PC_INCLUDE_DIRS)
                    target_include_directories(${HSDL_TARGET} INTERFACE ${SDL2_PC_INCLUDE_DIRS})
                endif()

                set(_charResolvedVersion "${SDL2_PC_VERSION}")
            endif()
        endif()
    endif()

    # Strict mode fails; non-strict mode builds the stub backend.
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
