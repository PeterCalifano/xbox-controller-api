function(add_template_matlab_wrapper_regression_tests)
  if(NOT ENABLE_TESTS OR NOT BUILD_TESTING)
    return()
  endif()

  set(_gtwrap_matlab_option_name "${LIB_NAMESPACE}_BUILD_MATLAB_WRAPPER")
  if(NOT DEFINED ${_gtwrap_matlab_option_name} OR NOT ${${_gtwrap_matlab_option_name}})
    return()
  endif()

  set(_matlab_program "")
  if(DEFINED Matlab_MAIN_PROGRAM AND EXISTS "${Matlab_MAIN_PROGRAM}")
    set(_matlab_program "${Matlab_MAIN_PROGRAM}")
  elseif(DEFINED Matlab_ROOT_DIR AND EXISTS "${Matlab_ROOT_DIR}/bin/matlab")
    set(_matlab_program "${Matlab_ROOT_DIR}/bin/matlab")
  else()
    find_program(_matlab_program matlab)
  endif()

  if(NOT _matlab_program)
    add_test(
      NAME ${LIB_NAMESPACE}_matlab_wrapper_unavailable
      COMMAND ${CMAKE_COMMAND} -E echo "MATLAB executable not found; MATLAB wrapper regression tests skipped")
    set_tests_properties(
      ${LIB_NAMESPACE}_matlab_wrapper_unavailable
      PROPERTIES SKIP_REGULAR_EXPRESSION "MATLAB executable not found")
    return()
  endif()

  set(_matlab_test_dir "${PROJECT_SOURCE_DIR}/tests/matlab")
  set(_matlab_toolbox_dir "${PROJECT_BINARY_DIR}/wrap/${PROJECT_NAME}")
  set(_matlab_mex_dir "${PROJECT_BINARY_DIR}/wrap/${PROJECT_NAME}_mex")

  set(_matlab_cases
      load_construct_clear
      live_exit
      string_value
      string_const_ref
      uint32_roundtrip
      bad_input_recovery
      stdout_error_recovery
      clear_all)

  foreach(_case IN LISTS _matlab_cases)
    set(_test_name "${LIB_NAMESPACE}_matlab_${_case}")
    add_test(
      NAME ${_test_name}
      COMMAND "${_matlab_program}" -batch
        "addpath('${_matlab_test_dir}'); RunTemplateWrapperRegression('${_case}', '${_matlab_toolbox_dir}', '${_matlab_mex_dir}');")
    set_tests_properties(
      ${_test_name}
      PROPERTIES
        LABELS "matlab;wrapper"
        ENVIRONMENT "LD_LIBRARY_PATH=${PROJECT_BINARY_DIR}/src:$ENV{LD_LIBRARY_PATH}")
  endforeach()

  if(ENABLE_TCMALLOC)
    set(_tcmalloc_expectation "present")
  else()
    set(_tcmalloc_expectation "absent")
  endif()

  add_test(
    NAME ${LIB_NAMESPACE}_matlab_wrapper_tcmalloc_${_tcmalloc_expectation}
    COMMAND
      ${CMAKE_COMMAND}
        -DPROJECT_LIBRARY_FILE=$<TARGET_FILE:${PROJECT_NAME}>
        -DMATLAB_MEX_DIR=${_matlab_mex_dir}
        -DTCMALLOC_EXPECTATION=${_tcmalloc_expectation}
        -P ${PROJECT_SOURCE_DIR}/tests/cmake/CheckTcmallocDependency.cmake)
  set_tests_properties(
    ${LIB_NAMESPACE}_matlab_wrapper_tcmalloc_${_tcmalloc_expectation}
    PROPERTIES LABELS "matlab;wrapper;elf")
endfunction()
