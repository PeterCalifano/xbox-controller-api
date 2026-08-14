#include "template_project_ros/CTemplateLifecycleNode.h"

#include <rclcpp/executors/single_threaded_executor.hpp>
#include <rclcpp/rclcpp.hpp>

#include <memory>

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  rclcpp::executors::SingleThreadedExecutor objExecutor_;
  auto objNode_ = std::make_shared<template_project_ros::CTemplateLifecycleNode>();
  objExecutor_.add_node(objNode_->get_node_base_interface());
  objExecutor_.spin();
  rclcpp::shutdown();
  return 0;
}
