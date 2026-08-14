# Starter runtime tests inherited unchanged by tailored projects.
include(CTest)

# Exclude project-specific files by filename or stem.
set(EXCLUDED_LIST "test_to_exclude")
set(TESTS_LIST "")

# Make reusable fixture headers available to starter tests.
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Register C++, Python, and fixture runtime tests.
add_subdirectory(template_test)
add_subdirectory(template_fixtures)

# Register any test files placed directly under tests/.
add_tests(${project_name} EXCLUDED_LIST TESTS_LIST "" CATCH2_TEST_PROPERTIES Catch2::Catch2WithMain)

message(STATUS "List of test targets: ${TESTS_LIST}")
