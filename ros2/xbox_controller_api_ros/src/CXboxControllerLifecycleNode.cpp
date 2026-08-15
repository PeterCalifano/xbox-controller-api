#include "xbox_controller_api_ros/CXboxControllerLifecycleNode.h"

#include <rclcpp_components/register_node_macro.hpp>
#include <xbox_controller_api/GamepadControls.h>

#include <chrono>
#include <cmath>

namespace xbox_controller_api_ros {

namespace {
using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

/// Slowest publish rate accepted, below which the node is not useful.
constexpr double kMinimumPublishRateHz = 1.0;

/// Fastest publish rate accepted, above which polling dominates the CPU.
constexpr double kMaximumPublishRateHz = 1000.0;
}  // namespace

CXboxControllerLifecycleNode::CXboxControllerLifecycleNode(const rclcpp::NodeOptions& objOptions_)
    : rclcpp_lifecycle::LifecycleNode("xbox_controller", objOptions_),
      i32JoystickIndex_(-1),
      dPublishRateHz_(50.0),
      dStickDeadzone_(0.0),
      charFrameId_("xbox_controller") {
  // A negative index selects the lowest-numbered game controller, matching the
  // library default.
  declare_parameter<int>("joystick_index", i32JoystickIndex_);
  declare_parameter<double>("publish_rate_hz", dPublishRateHz_);
  declare_parameter<double>("stick_deadzone", dStickDeadzone_);
  declare_parameter<std::string>("frame_id", charFrameId_);
}

CallbackReturn CXboxControllerLifecycleNode::on_configure(const rclcpp_lifecycle::State&) {
  i32JoystickIndex_ = static_cast<std::int32_t>(get_parameter("joystick_index").as_int());
  dPublishRateHz_ = get_parameter("publish_rate_hz").as_double();
  dStickDeadzone_ = get_parameter("stick_deadzone").as_double();
  charFrameId_ = get_parameter("frame_id").as_string();

  if (!(dPublishRateHz_ >= kMinimumPublishRateHz) || !(dPublishRateHz_ <= kMaximumPublishRateHz)) {
    RCLCPP_ERROR(
        get_logger(),
        "publish_rate_hz must lie in [%.1f, %.1f]; got %f",
        kMinimumPublishRateHz, kMaximumPublishRateHz, dPublishRateHz_);
    return CallbackReturn::FAILURE;
  }

  // Default reliable QoS, matching the standard joy node. Sensor-data QoS would
  // be defensible for a stream of samples, but it is best effort and therefore
  // incompatible with the reliable subscriptions used by teleop_twist_joy and
  // most existing joystick consumers, which is the interoperability this
  // publisher exists for.
  objJoyPublisher_ = create_publisher<sensor_msgs::msg::Joy>("~/joy", rclcpp::QoS(10));

  // No device access happens here, so configuring is safe with nothing plugged
  // in and the failure to find hardware is reported at activation instead.
  RCLCPP_INFO(
      get_logger(),
      "Configured: index=%d rate=%.1f Hz deadzone=%.3f frame_id=%s",
      i32JoystickIndex_, dPublishRateHz_, dStickDeadzone_, charFrameId_.c_str());

  return CallbackReturn::SUCCESS;
}

CallbackReturn CXboxControllerLifecycleNode::on_activate(const rclcpp_lifecycle::State&) {
  if (!xbox_controller_api::CSdlGamepadSource::isBackendAvailable()) {
    RCLCPP_ERROR(
        get_logger(),
        "SDL2 backend is not compiled into this build; rebuild the overlay with ENABLE_SDL2=ON");
    return CallbackReturn::FAILURE;
  }

  if (!objSource_.open(i32JoystickIndex_)) {
    RCLCPP_ERROR(get_logger(), "Could not open a controller: %s", objSource_.lastError().c_str());
    return CallbackReturn::FAILURE;
  }

  objJoyPublisher_->on_activate();

  const auto objPollPeriod_ = std::chrono::duration<double>(1.0 / dPublishRateHz_);
  objPollTimer_ = create_wall_timer(
      std::chrono::duration_cast<std::chrono::nanoseconds>(objPollPeriod_),
      [this]() { pollAndPublish(); });

  RCLCPP_INFO(get_logger(), "Publishing '%s' on ~/joy", objSource_.deviceName().c_str());

  return CallbackReturn::SUCCESS;
}

CallbackReturn CXboxControllerLifecycleNode::on_deactivate(const rclcpp_lifecycle::State&) {
  objPollTimer_.reset();
  objJoyPublisher_->on_deactivate();

  // Release the device here rather than at cleanup, so a deactivate/activate
  // cycle is the documented way to reattach after a detach.
  objSource_.close();

  return CallbackReturn::SUCCESS;
}

CallbackReturn CXboxControllerLifecycleNode::on_cleanup(const rclcpp_lifecycle::State&) {
  objPollTimer_.reset();
  objJoyPublisher_.reset();

  return CallbackReturn::SUCCESS;
}

sensor_msgs::msg::Joy CXboxControllerLifecycleNode::makeStampedJoyMessage(
    const xbox_controller_api::SGamepadState& strGamepadState) {
  sensor_msgs::msg::Joy objJoy_ = MakeJoyMessage(strGamepadState, charFrameId_);
  objJoy_.header.stamp = now();

  return objJoy_;
}

void CXboxControllerLifecycleNode::pollAndPublish() {
  if (!objSource_.update()) {
    // The library reports a detach and never heals it, so stop publishing and
    // leave reattachment to an explicit deactivate/activate cycle. Publish the
    // neutral snapshot once first, so a subscriber holding the last command
    // sees a safe value rather than the deflection read before the unplug.
    objJoyPublisher_->publish(makeStampedJoyMessage(objSource_.state()));

    objPollTimer_->cancel();
    RCLCPP_ERROR(
        get_logger(),
        "Controller detached; publication stopped. Deactivate and activate to reattach.");

    return;
  }

  objJoyPublisher_->publish(makeStampedJoyMessage(
      xbox_controller_api::ApplyStickDeadzone(objSource_.state(), dStickDeadzone_)));
}

}  // namespace xbox_controller_api_ros

RCLCPP_COMPONENTS_REGISTER_NODE(xbox_controller_api_ros::CXboxControllerLifecycleNode)
