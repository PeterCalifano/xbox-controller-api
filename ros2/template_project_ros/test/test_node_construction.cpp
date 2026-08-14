#include "template_project_ros/CTemplateLifecycleNode.h"

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

TEST(TemplateProjectLifecycleNode, ConstructConfigureAndCleanup) {
  if (!rclcpp::ok()) {
    rclcpp::init(0, nullptr);
  }

  rclcpp::NodeOptions objOptions_;
  objOptions_.append_parameter_override("gain", 2.0);
  objOptions_.append_parameter_override("bias", 1.0);

  auto objNode_ = std::make_shared<template_project_ros::CTemplateLifecycleNode>(objOptions_);
  const auto objState_ = objNode_->configure();
  EXPECT_EQ(objState_.label(), "inactive");

  objNode_->cleanup();
  rclcpp::shutdown();
}
