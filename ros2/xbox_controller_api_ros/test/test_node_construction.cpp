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

  // Configuration must succeed on a machine with no controller attached, which
  // is what keeps this test meaningful in CI.
  auto objNode_ = std::make_shared<xbox_controller_api_ros::CXboxControllerLifecycleNode>(
      MakeOptions(50.0));

  EXPECT_EQ(objNode_->configure().label(), "inactive");
  EXPECT_EQ(objNode_->cleanup().label(), "unconfigured");
}

TEST(XboxControllerLifecycleNode, AnUnusablePublishRateFailsConfiguration) {
  EnsureRosInitialized();

  auto objNode_ = std::make_shared<xbox_controller_api_ros::CXboxControllerLifecycleNode>(
      MakeOptions(0.0));

  // An out-of-range rate is rejected rather than silently clamped, so a bad
  // parameter surfaces as a failed transition instead of a strange cadence.
  EXPECT_EQ(objNode_->configure().label(), "unconfigured");
}

TEST(XboxControllerLifecycleNode, ActivationOutcomeMatchesBackendAvailability) {
  EnsureRosInitialized();

  auto objNode_ = std::make_shared<xbox_controller_api_ros::CXboxControllerLifecycleNode>(
      MakeOptions(50.0));
  ASSERT_EQ(objNode_->configure().label(), "inactive");

  // Never assert that hardware exists: only that a failed activation leaves the
  // node inactive and a successful one leaves it active and publishing.
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
