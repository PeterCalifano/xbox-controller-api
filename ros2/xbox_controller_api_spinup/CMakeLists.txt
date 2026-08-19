cmake_minimum_required(VERSION 3.16)

project(xbox_controller_api_spinup)

find_package(ament_cmake REQUIRED)

install(
  DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(launch_testing_ament_cmake REQUIRED)

  add_launch_test(
    test/test_spinup_launch.py
    TIMEOUT 30
  )
endif()

ament_package()
