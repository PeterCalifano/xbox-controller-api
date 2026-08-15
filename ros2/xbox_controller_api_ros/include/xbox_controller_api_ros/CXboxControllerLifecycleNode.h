#pragma once

#include "xbox_controller_api_ros/joy_conversion.h"

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>
#include <sensor_msgs/msg/joy.hpp>
#include <xbox_controller_api/CSdlGamepadSource.h>

#include <cstdint>
#include <memory>
#include <string>

namespace xbox_controller_api_ros {

/// @brief Publishes an attached controller as sensor_msgs/Joy.
///
/// The lifecycle states map onto the library's explicit device contract:
///
/// - on_configure allocates the publisher and reads parameters. It performs no
///   device access, so configuring is safe on a machine with no controller.
/// - on_activate opens the device and starts the poll timer. Activation fails
///   when no controller is available, which is the honest signal for "cannot
///   start publishing".
/// - on_deactivate stops the timer and closes the device.
///
/// The library never reconnects on its own, so a detach stops publication and
/// recovery is a deactivate/activate cycle driven by an operator or a lifecycle
/// manager. That keeps retry policy outside this node.
class CXboxControllerLifecycleNode final : public rclcpp_lifecycle::LifecycleNode {
 public:
  explicit CXboxControllerLifecycleNode(const rclcpp::NodeOptions& objOptions_ = rclcpp::NodeOptions());

  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn on_configure(
      const rclcpp_lifecycle::State& objPreviousState_) override;
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn on_activate(
      const rclcpp_lifecycle::State& objPreviousState_) override;
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn on_deactivate(
      const rclcpp_lifecycle::State& objPreviousState_) override;
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn on_cleanup(
      const rclcpp_lifecycle::State& objPreviousState_) override;

 private:
  /// @brief Poll the device once and publish, or report a detach.
  void pollAndPublish();

  /// @brief Build a Joy message and stamp it with the current ROS time.
  ///
  /// Snapshot timestamps come from a steady clock with an arbitrary epoch, so
  /// the ROS stamp is taken here rather than converted from the sample.
  sensor_msgs::msg::Joy makeStampedJoyMessage(
      const xbox_controller_api::SGamepadState& strGamepadState);

  rclcpp_lifecycle::LifecyclePublisher<sensor_msgs::msg::Joy>::SharedPtr objJoyPublisher_;
  rclcpp::TimerBase::SharedPtr objPollTimer_;
  xbox_controller_api::CSdlGamepadSource objSource_;

  std::int32_t i32JoystickIndex_;
  double dPublishRateHz_;
  double dStickDeadzone_;
  std::string charFrameId_;
};

}  // namespace xbox_controller_api_ros
