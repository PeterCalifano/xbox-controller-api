cmake_minimum_required(VERSION 3.16)

project(template_project_interfaces)

option(TEMPLATE_PROJECT_ENABLE_CUDA "Accepted for workspace-wide build helper compatibility." OFF)
option(TEMPLATE_PROJECT_ENABLE_OPTIX "Accepted for workspace-wide build helper compatibility." OFF)

find_package(ament_cmake REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/AlgorithmStatus.msg"
  "srv/RunAlgorithm.srv"
  DEPENDENCIES
    builtin_interfaces
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
