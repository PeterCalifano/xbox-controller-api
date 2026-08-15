#pragma once

#include <sensor_msgs/msg/joy.hpp>
#include <xbox_controller_api/SGamepadState.h>

#include <cstddef>
#include <string>

namespace xbox_controller_api_ros {

/// Number of axes published in sensor_msgs/Joy, in the order documented below.
inline constexpr std::size_t kJoyAxisCount = 6U;

/// Index of each axis within the published Joy message.
enum class EJoyAxis : std::size_t {
  LeftStickX = 0U,
  LeftStickY = 1U,
  RightStickX = 2U,
  RightStickY = 3U,
  LeftTrigger = 4U,
  RightTrigger = 5U,
};

/// @brief Convert a library snapshot into a sensor_msgs/Joy message.
///
/// Axes follow the library conventions rather than the raw SDL ones: sticks lie
/// in [-1, 1] with Y positive upward, and triggers lie in [0, 1]. Buttons are
/// emitted in xbox_controller_api::AllGamepadButtons() order, which is a
/// documented stable contract, so a subscriber can index them reliably.
///
/// The header stamp is deliberately left unset. Snapshot timestamps come from a
/// steady clock whose epoch is arbitrary, so only the node can supply a ROS
/// time; keeping that out of here also keeps this function free of rclcpp and
/// therefore testable without a ROS context.
///
/// @param strGamepadState Snapshot to convert.
/// @param charFrameId Frame id to place in the message header.
/// @return A fully populated Joy message with an unset stamp.
sensor_msgs::msg::Joy MakeJoyMessage(
    const xbox_controller_api::SGamepadState& strGamepadState,
    const std::string& charFrameId);

}  // namespace xbox_controller_api_ros
