cmake_minimum_required(VERSION 3.16)

project(template_project_ros)

get_filename_component(TEMPLATE_PROJECT_REPOSITORY_ROOT
  "${CMAKE_CURRENT_LIST_DIR}/../.."
  ABSOLUTE
)

option(TEMPLATE_PROJECT_ENABLE_CUDA "Accepted for workspace-wide build helper compatibility." OFF)
option(TEMPLATE_PROJECT_ENABLE_OPTIX "Accepted for workspace-wide build helper compatibility." OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(lifecycle_msgs REQUIRED)
find_package(template_project REQUIRED)
find_package(template_project_interfaces REQUIRED)

add_library(template_project_ros_conversions SHARED
  src/conversions.cpp
)
target_include_directories(template_project_ros_conversions
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    # Keep this source path only for additive rollout compatibility with older
    # derived cores whose adapted seam uses a non-installed header. The exported
    # core target remains the primary template consumption interface.
    ${TEMPLATE_PROJECT_REPOSITORY_ROOT}/src
)
target_link_libraries(template_project_ros_conversions
  template_project::template_project
)
ament_target_dependencies(template_project_ros_conversions
  template_project_interfaces
)

add_library(template_project_ros_component SHARED
  src/CTemplateLifecycleNode.cpp
)
target_include_directories(template_project_ros_component
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(template_project_ros_component
  template_project_ros_conversions
  template_project::template_project
)
ament_target_dependencies(template_project_ros_component
  rclcpp
  rclcpp_components
  rclcpp_lifecycle
  lifecycle_msgs
  template_project_interfaces
)
rclcpp_components_register_nodes(template_project_ros_component
  "template_project_ros::CTemplateLifecycleNode"
)

add_executable(template_project_node src/template_project_node_main.cpp)
target_link_libraries(template_project_node
  template_project_ros_component
)
ament_target_dependencies(template_project_node
  rclcpp
)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_conversions test/test_conversions.cpp)
  if(TARGET test_conversions)
    target_link_libraries(test_conversions
      template_project_ros_conversions
    )
  endif()

  ament_add_gtest(test_node_construction test/test_node_construction.cpp)
  if(TARGET test_node_construction)
    target_link_libraries(test_node_construction
      template_project_ros_component
    )
  endif()
endif()

install(DIRECTORY include/
  DESTINATION include
)

install(
  TARGETS
    template_project_ros_conversions
    template_project_ros_component
    template_project_node
  EXPORT ${PROJECT_NAME}Targets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

ament_export_include_directories(include)
ament_export_targets(${PROJECT_NAME}Targets HAS_LIBRARY_TARGET)
ament_export_dependencies(
  rclcpp
  rclcpp_components
  rclcpp_lifecycle
  lifecycle_msgs
  template_project
  template_project_interfaces
)
ament_package()
