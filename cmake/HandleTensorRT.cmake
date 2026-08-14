# Configure optional TensorRT usage requirements on one interface target.

include_guard(GLOBAL)
include(CMakeParseArguments)

# handle_tensorrt(TARGET <target>)
#
# Create the named interface target in every configuration. When
# ENABLE_TENSORRT is true, require the project CUDA path and TensorRT SDK, then
# propagate the vendor runtime targets and feature definition to consumers.
function(handle_tensorrt)
  set(one_value_args TARGET)
  cmake_parse_arguments(HTRT "" "${one_value_args}" "" ${ARGN})

  if(NOT HTRT_TARGET)
    set(HTRT_TARGET tensorrt_compile_interface)
  endif()
  if(NOT TARGET ${HTRT_TARGET})
    add_library(${HTRT_TARGET} INTERFACE)
  endif()

  if(NOT ENABLE_TENSORRT)
    return()
  endif()

  if(NOT ENABLE_CUDA)
    message(FATAL_ERROR "ENABLE_TENSORRT requires ENABLE_CUDA=ON.")
  endif()
  if(NOT TARGET CUDA::cudart)
    message(FATAL_ERROR
        "ENABLE_TENSORRT requires CUDA::cudart from the configured CUDA toolkit.")
  endif()

  # Keep SDK discovery in the reusable find module while this handler owns
  # project integration policy and validates its required imported targets.
  find_package(TensorRT REQUIRED MODULE)

  foreach(_tensorrt_target IN ITEMS TensorRT::nvinfer TensorRT::nvinfer_plugin)
    if(NOT TARGET ${_tensorrt_target})
      message(FATAL_ERROR
          "TensorRT discovery did not define required target ${_tensorrt_target}.")
    endif()
  endforeach()

  target_compile_definitions(${HTRT_TARGET} INTERFACE __TENSORRT_ENABLED__=1)
  target_link_libraries(
      ${HTRT_TARGET} INTERFACE
      TensorRT::nvinfer TensorRT::nvinfer_plugin CUDA::cudart)

  message(STATUS "TensorRT enabled: ${TensorRT_VERSION}")
endfunction()
