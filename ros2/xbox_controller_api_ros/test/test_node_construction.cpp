/**
 * @file test_node_construction.cpp
 * @brief Tests lifecycle transitions that do not require a controller.
 */

#include "xbox_controller_api_ros/CXboxControllerLifecycleNode.h"

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <cstdint>
#include <limits>
#include <memory>

namespace {

void EnsureRosInitialized() {
  if (!rclcpp::ok()) {
    rclcpp::init(0, nullptr);
  }
}

rclcpp::NodeOptions MakeOptions(
    double dPublishRateHz,
    std::int64_t i64JoystickIndex = -1,
    double dStickDeadzone = 0.15) {
  rclcpp::NodeOptions objOptions_;
  objOptions_.append_parameter_override("joystick_index", i64JoystickIndex);
  objOptions_.append_parameter_override("publish_rate_hz", dPublishRateHz);
  objOptions_.append_parameter_override("stick_deadzone", dStickDeadzone);
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

TEST(XboxControllerLifecycleNode, ANonFiniteDeadzoneFailsConfiguration) {
  EnsureRosInitialized();

  auto objNode_ = std::make_shared<xbox_controller_api_ros::CXboxControllerLifecycleNode>(
      MakeOptions(50.0, -1, std::numeric_limits<double>::quiet_NaN()));

  // Invalid conditioning must fail before a publisher can emit NaN axes.
  EXPECT_EQ(objNode_->configure().label(), "unconfigured");
}

TEST(XboxControllerLifecycleNode, AnOutOfRangeJoystickIndexFailsConfiguration) {
  EnsureRosInitialized();

  constexpr std::int64_t i64OutOfRangeIndex_ =
      static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) + 1;
  auto objNode_ = std::make_shared<xbox_controller_api_ros::CXboxControllerLifecycleNode>(
      MakeOptions(50.0, i64OutOfRangeIndex_));

  // Reject the ROS int64 value instead of narrowing it into the default policy.
  EXPECT_EQ(objNode_->configure().label(), "unconfigured");
}

TEST(XboxControllerLifecycleNode, ActivationWithImpossibleIndexStaysInactive) {
  EnsureRosInitialized();

  // A valid SDL backend cannot expose a controller at INT32_MAX, so this
  // exercises the real open failure path independently of host hardware.
  auto objNode_ = std::make_shared<xbox_controller_api_ros::CXboxControllerLifecycleNode>(
      MakeOptions(50.0, std::numeric_limits<std::int32_t>::max()));
  ASSERT_EQ(objNode_->configure().label(), "inactive");

  // Failed activation remains configured and never depends on a physical pad.
  const std::string charStateAfterActivate_ = objNode_->activate().label();
  EXPECT_EQ(charStateAfterActivate_, "inactive");

  EXPECT_EQ(objNode_->cleanup().label(), "unconfigured");
}
