/**
 * @file CXboxControllerLifecycleNode.h
 * @brief Declares the lifecycle node that publishes controller input as Joy.
 */

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
/// Configuration reads parameters and creates the publisher without accessing
/// hardware. Activation opens the controller and starts polling; deactivation
/// stops polling and closes the device. A disconnect stops publication until a
/// lifecycle manager deactivates and activates the node again.
class CXboxControllerLifecycleNode final : public rclcpp_lifecycle::LifecycleNode {
 public:
  /// @brief Construct a node with optional ROS node options.
  /// @param objOptions ROS options used to construct the lifecycle node.
  explicit CXboxControllerLifecycleNode(const rclcpp::NodeOptions& objOptions_ = rclcpp::NodeOptions());

  /// @brief Read parameters and create the inactive Joy publisher.
  /// @param objPreviousState Lifecycle state being left.
  /// @return Success when all parameters are valid and the publisher is created.
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn on_configure(
      const rclcpp_lifecycle::State& objPreviousState_) override;
  /// @brief Open the controller and start the polling timer.
  /// @param objPreviousState Lifecycle state being left.
  /// @return Success when the controller opens; failure otherwise.
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn on_activate(
      const rclcpp_lifecycle::State& objPreviousState_) override;
  /// @brief Stop polling and release the controller.
  /// @param objPreviousState Lifecycle state being left.
  /// @return Success after stopping the timer and closing the controller.
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn on_deactivate(
      const rclcpp_lifecycle::State& objPreviousState_) override;
  /// @brief Release lifecycle resources after deactivation.
  /// @param objPreviousState Lifecycle state being left.
  /// @return Success after releasing the publisher and timer.
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn on_cleanup(
      const rclcpp_lifecycle::State& objPreviousState_) override;

 private:
  /// @brief Poll the device once and publish, or report a detach.
  void pollAndPublish();

  /// @brief Build a Joy message and stamp it with the current ROS time.
  ///
  /// Snapshot timestamps use a steady clock, so the node supplies ROS time.
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
