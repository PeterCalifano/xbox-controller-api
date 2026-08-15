/**
 * @file test_node_construction.cpp
 * @brief Tests lifecycle transitions that do not require a controller.
 */

#include "xbox_controller_api_ros/CXboxControllerLifecycleNode.h"

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <xbox_controller_api/CSdlGamepadSource.h>

#include <memory>

namespace {

void EnsureRosInitialized() {
  if (!rclcpp::ok()) {
    rclcpp::init(0, nullptr);
  }
}

rclcpp::NodeOptions MakeOptions(double dPublishRateHz) {
  rclcpp::NodeOptions objOptions_;
  objOptions_.append_parameter_override("joystick_index", -1);
  objOptions_.append_parameter_override("publish_rate_hz", dPublishRateHz);
  objOptions_.append_parameter_override("stick_deadzone", 0.15);
  objOptions_.append_parameter_override("frame_id", std::string("test_pad"));

  return objOptions_;
}

}  // namespace

TEST(XboxControllerLifecycleNode, ConfigureTouchesNoDevice) {
  EnsureRosInitialized();

  // Configuration does not access controller hardware.
  auto objNode_ = std::make_shared<xbox_controller_api_ros::CXboxControllerLifecycleNode>(
      MakeOptions(50.0));

  EXPECT_EQ(objNode_->configure().label(), "inactive");
  EXPECT_EQ(objNode_->cleanup().label(), "unconfigured");
}

TEST(XboxControllerLifecycleNode, AnUnusablePublishRateFailsConfiguration) {
  EnsureRosInitialized();

  auto objNode_ = std::make_shared<xbox_controller_api_ros::CXboxControllerLifecycleNode>(
      MakeOptions(0.0));

  // Invalid rates fail configuration instead of being clamped.
  EXPECT_EQ(objNode_->configure().label(), "unconfigured");
}

TEST(XboxControllerLifecycleNode, ActivationOutcomeMatchesBackendAvailability) {
  EnsureRosInitialized();

  auto objNode_ = std::make_shared<xbox_controller_api_ros::CXboxControllerLifecycleNode>(
      MakeOptions(50.0));
  ASSERT_EQ(objNode_->configure().label(), "inactive");

  // The test accepts either hardware outcome.
  const std::string charStateAfterActivate_ = objNode_->activate().label();

  if (!xbox_controller_api::CSdlGamepadSource::isBackendAvailable()) {
    EXPECT_EQ(charStateAfterActivate_, "inactive");
  }

  EXPECT_TRUE(charStateAfterActivate_ == "active" || charStateAfterActivate_ == "inactive");

  if (charStateAfterActivate_ == "active") {
    EXPECT_EQ(objNode_->deactivate().label(), "inactive");
  }

  EXPECT_EQ(objNode_->cleanup().label(), "unconfigured");
}
