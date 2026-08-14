if(NOT DEFINED PROJECT_LIBRARY_FILE OR "${PROJECT_LIBRARY_FILE}" STREQUAL "")
  message(FATAL_ERROR "PROJECT_LIBRARY_FILE is required")
endif()
if(NOT DEFINED MATLAB_MEX_DIR OR "${MATLAB_MEX_DIR}" STREQUAL "")
  message(FATAL_ERROR "MATLAB_MEX_DIR is required")
endif()
if(NOT DEFINED TCMALLOC_EXPECTATION OR "${TCMALLOC_EXPECTATION}" STREQUAL "")
  set(TCMALLOC_EXPECTATION "absent")
endif()

file(GLOB _mex_files "${MATLAB_MEX_DIR}/*_wrapper.mex*")
if(NOT _mex_files)
  message(FATAL_ERROR "No MATLAB wrapper MEX file found in '${MATLAB_MEX_DIR}'")
endif()
list(GET _mex_files 0 _mex_file)

find_program(LDD_EXECUTABLE ldd)
find_program(READELF_EXECUTABLE readelf)
if(NOT LDD_EXECUTABLE OR NOT READELF_EXECUTABLE)
  message(STATUS "ldd/readelf not found; skipping tcmalloc dependency check")
  return()
endif()

execute_process(
  COMMAND "${LDD_EXECUTABLE}" "${_mex_file}"
  RESULT_VARIABLE _ldd_result
  OUTPUT_VARIABLE _ldd_output
  ERROR_VARIABLE _ldd_error)
if(NOT _ldd_result EQUAL 0)
  message(FATAL_ERROR "ldd failed for '${_mex_file}': ${_ldd_error}")
endif()

execute_process(
  COMMAND "${READELF_EXECUTABLE}" -d "${PROJECT_LIBRARY_FILE}"
  RESULT_VARIABLE _readelf_result
  OUTPUT_VARIABLE _readelf_output
  ERROR_VARIABLE _readelf_error)
if(NOT _readelf_result EQUAL 0)
  message(FATAL_ERROR "readelf failed for '${PROJECT_LIBRARY_FILE}': ${_readelf_error}")
endif()

set(_combined_output "${_ldd_output}\n${_readelf_output}")
string(FIND "${_combined_output}" "libtcmalloc" _tcmalloc_pos)

if(TCMALLOC_EXPECTATION STREQUAL "present")
  if(_tcmalloc_pos EQUAL -1)
    message(FATAL_ERROR "Expected libtcmalloc dependency, but none was found")
  endif()
else()
  if(NOT _tcmalloc_pos EQUAL -1)
    message(FATAL_ERROR "Unexpected libtcmalloc dependency found")
  endif()
endif()

message(STATUS "tcmalloc dependency expectation '${TCMALLOC_EXPECTATION}' satisfied")
