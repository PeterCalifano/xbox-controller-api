# CUDA test gate + a placeholder CUDA test demonstrating the fail-fast fixture
# pattern. Built only when CUDA is enabled.
#
# cudaInitGate is a CTest FIXTURES_SETUP test: if the CUDA runtime fails to
# initialize on a GPU host, every FIXTURES_REQUIRED;Cuda test is reported
# "Not Run" instead of producing N identical CUDA-init failures. With no GPU the
# gate SKIPs and the dependent tests fall back to their own HasGpu() guards.
if(ENABLE_CUDA)
    # Exclude EXCLUDED_LIST from the auto-discovered list of tests
    set(EXCLUDED_LIST "test_to_exclude")  # Specify files to exclude WE

    # Extend the project-wide Catch2 properties with the CUDA fixture requirement
    # so every auto-discovered test*.cpp in this directory depends on the gate
    # below. The 5th add_tests() argument is a VARIABLE NAME the helper
    # dereferences (see cmake/cmake_utils.cmake).
    set(CUDA_TEST_PROPERTIES "${CATCH2_TEST_PROPERTIES};FIXTURES_REQUIRED;Cuda")

    add_tests(${project_name} EXCLUDED_LIST TESTS_LIST ${CUDA_COMPILE_TARGET} CUDA_TEST_PROPERTIES Catch2::Catch2WithMain)

    # CUDA-init gate = the fixture provider. Its file name does not match the
    # test*.cpp glob, so add_tests() ignores it; register it by hand as the
    # FIXTURES_SETUP so it is itself excluded from FIXTURES_REQUIRED (a setup must
    # not depend on its own fixture).
    if(Catch2_FOUND)
        add_executable(cudaInitGate cudaInitGate.cpp)
        target_link_libraries(cudaInitGate PRIVATE ${CUDA_COMPILE_TARGET} Catch2::Catch2WithMain)
        catch_discover_tests(cudaInitGate PROPERTIES FIXTURES_SETUP Cuda LABELS "cuda;setup")
    else()
        message(STATUS "Catch2 not found. Skipping CUDA test gate (tests/template_cuda/cudaInitGate.cpp).")
    endif()

    set(TESTS_LIST ${TESTS_LIST} PARENT_SCOPE) # Export list of tests to parent scope (applied ONLY here)
else()
    message(STATUS "ENABLE_CUDA=OFF: skipping CUDA test gate (tests/template_cuda).")
endif()
