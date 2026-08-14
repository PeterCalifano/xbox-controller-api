include_guard(GLOBAL)
include(CMakeParseArguments)

if (NOT DEFINED ENABLE_ZEROMQ)
    option(ENABLE_ZEROMQ "Enable ZeroMQ transport support" OFF)
endif()

if (NOT DEFINED ENABLE_FETCH_CPPZMQ)
    option(ENABLE_FETCH_CPPZMQ "Fetch cppzmq headers into lib/ when missing" OFF)
endif()

if (NOT DEFINED ENABLE_ZEROMQ_STRICT)
    option(ENABLE_ZEROMQ_STRICT
           "Fail configuration when ZeroMQ support is requested but its dependencies are unavailable"
           OFF)
endif()

if (NOT DEFINED CPPZMQ_GIT_TAG)
    set(CPPZMQ_GIT_TAG "v4.10.0" CACHE STRING "Git tag used when fetching cppzmq")
endif()

if (NOT DEFINED CPPZMQ_LOCAL_DIR)
    set(CPPZMQ_LOCAL_DIR "${PROJECT_SOURCE_DIR}/lib/cppzmq" CACHE PATH "Local cppzmq checkout directory")
endif()

function(handle_zeromq)
    set(oneValueArgs TARGET)
    cmake_parse_arguments(HZMQ "" "${oneValueArgs}" "" ${ARGN})

    if(NOT HZMQ_TARGET)
        set(HZMQ_TARGET zeromq_compile_interface)
    endif()

    if(NOT TARGET ${HZMQ_TARGET})
        add_library(${HZMQ_TARGET} INTERFACE)
    endif()

    set(_bZeroMqEnabled OFF)
    set(_bCppZmqIsLocal OFF)
    set(_charCppZmqIncludeDir "")
    set(_charZeroMqUnavailableReason "")

    if(ENABLE_ZEROMQ)
        find_package(PkgConfig QUIET)
        if(NOT PkgConfig_FOUND)
            set(_charZeroMqUnavailableReason
                "ENABLE_ZEROMQ is ON but PkgConfig was not found.")
        else()
            pkg_check_modules(LIBZMQ QUIET IMPORTED_TARGET libzmq)

            if(NOT LIBZMQ_FOUND)
                set(_charZeroMqUnavailableReason
                    "ENABLE_ZEROMQ is ON but libzmq was not found via pkg-config.")
            else()
                find_path(_charCppZmqIncludeDir
                          NAMES zmq.hpp
                          HINTS
                              "${CPPZMQ_LOCAL_DIR}"
                              "${CPPZMQ_LOCAL_DIR}/include"
                              ${LIBZMQ_INCLUDE_DIRS})

                if(NOT _charCppZmqIncludeDir)
                    find_path(_charCppZmqIncludeDir
                              NAMES zmq.hpp
                              PATHS
                                  /usr/include
                                  /usr/local/include
                                  /opt/homebrew/include
                                  /opt/local/include
                              PATH_SUFFIXES
                                  ""
                                  include)
                endif()

                if(NOT _charCppZmqIncludeDir AND EXISTS "/usr/include/zmq.hpp")
                    set(_charCppZmqIncludeDir "/usr/include")
                endif()

                if(NOT _charCppZmqIncludeDir AND EXISTS "/usr/local/include/zmq.hpp")
                    set(_charCppZmqIncludeDir "/usr/local/include")
                endif()

                if(NOT _charCppZmqIncludeDir AND ENABLE_FETCH_CPPZMQ)
                    find_package(Git QUIET)

                    if(NOT Git_FOUND)
                        set(_charZeroMqUnavailableReason
                            "ENABLE_FETCH_CPPZMQ is ON but Git was not found.")
                    elseif(EXISTS "${CPPZMQ_LOCAL_DIR}")
                        set(_charZeroMqUnavailableReason
                            "CPPZMQ_LOCAL_DIR exists but zmq.hpp was not found under it: ${CPPZMQ_LOCAL_DIR}")
                    else()
                        execute_process(
                            COMMAND "${GIT_EXECUTABLE}" ls-remote https://github.com/zeromq/cppzmq.git
                            RESULT_VARIABLE _i32GitLsRemoteResult
                            OUTPUT_QUIET
                            ERROR_QUIET
                            TIMEOUT 10)

                        if(NOT _i32GitLsRemoteResult EQUAL 0)
                            set(_charZeroMqUnavailableReason
                                "Cannot reach cppzmq upstream to fetch headers. Disable ENABLE_FETCH_CPPZMQ or provide cppzmq locally.")
                        else()
                            execute_process(
                                COMMAND "${GIT_EXECUTABLE}" clone --depth 1 --branch "${CPPZMQ_GIT_TAG}" https://github.com/zeromq/cppzmq.git "${CPPZMQ_LOCAL_DIR}"
                                RESULT_VARIABLE _i32GitCloneResult
                                OUTPUT_QUIET
                                ERROR_QUIET
                                TIMEOUT 300)

                            if(NOT _i32GitCloneResult EQUAL 0)
                                set(_charZeroMqUnavailableReason
                                    "Failed to clone cppzmq into ${CPPZMQ_LOCAL_DIR}.")
                            else()
                                find_path(_charCppZmqIncludeDir
                                          NAMES zmq.hpp
                                          HINTS
                                              "${CPPZMQ_LOCAL_DIR}"
                                              "${CPPZMQ_LOCAL_DIR}/include"
                                          NO_DEFAULT_PATH)
                            endif()
                        endif()
                    endif()
                endif()

                if(NOT _charCppZmqIncludeDir AND NOT _charZeroMqUnavailableReason)
                    set(_charZeroMqUnavailableReason
                        "ENABLE_ZEROMQ is ON but cppzmq headers were not found. "
                        "Install cppzmq system-wide, place it under ${CPPZMQ_LOCAL_DIR}, or enable ENABLE_FETCH_CPPZMQ.")
                endif()
            endif()
        endif()

        if(_charZeroMqUnavailableReason)
            if(ENABLE_ZEROMQ_STRICT)
                message(FATAL_ERROR "${_charZeroMqUnavailableReason}")
            endif()

            message(WARNING "${_charZeroMqUnavailableReason} ZeroMQ transport support will be disabled.")
        else()
            if(_charCppZmqIncludeDir MATCHES "^${PROJECT_SOURCE_DIR}(/.*)?$")
                set(_bCppZmqIsLocal ON)
            endif()

            target_link_libraries(${HZMQ_TARGET} INTERFACE PkgConfig::LIBZMQ)
            target_compile_definitions(${HZMQ_TARGET} INTERFACE __ZEROMQ_ENABLED__=1)

            if(_bCppZmqIsLocal)
                target_include_directories(${HZMQ_TARGET} INTERFACE
                                           $<BUILD_INTERFACE:${_charCppZmqIncludeDir}>
                                           $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

                set(_cellCppZmqHeaders "${_charCppZmqIncludeDir}/zmq.hpp")
                if(EXISTS "${_charCppZmqIncludeDir}/zmq_addon.hpp")
                    list(APPEND _cellCppZmqHeaders "${_charCppZmqIncludeDir}/zmq_addon.hpp")
                endif()

                install(FILES ${_cellCppZmqHeaders}
                        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
            else()
                target_include_directories(${HZMQ_TARGET} INTERFACE "${_charCppZmqIncludeDir}")
            endif()

            set(_bZeroMqEnabled ON)
            message(STATUS "ZeroMQ enabled with cppzmq headers from: ${_charCppZmqIncludeDir}")
        endif()
    else()
        message(STATUS "ZeroMQ support disabled by configuration (ENABLE_ZEROMQ=OFF).")
    endif()

    set(ZEROMQ_ENABLED ${_bZeroMqEnabled} PARENT_SCOPE)
    set(CPPZMQ_IS_LOCAL ${_bCppZmqIsLocal} PARENT_SCOPE)
    set(CPPZMQ_INCLUDE_DIR "${_charCppZmqIncludeDir}" PARENT_SCOPE)
endfunction()
