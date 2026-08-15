/**
 * @file joy_conversion.h
 * @brief Converts controller snapshots to sensor_msgs/msg/Joy messages.
 * @details Defines the stable axis and button order used by the ROS 2 bridge.
 */

#pragma once

#include <sensor_msgs/msg/joy.hpp>
#include <xbox_controller_api/SGamepadState.h>

#include <cstddef>
#include <string>

namespace xbox_controller_api_ros {

/// @brief Number of axes published in each Joy message.
inline constexpr std::size_t kJoyAxisCount = 6U;

/// @brief Index of an axis within a published Joy message.
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
/// Axes use the library's normalized values: sticks are in [-1, 1] with Y
/// positive upward, and triggers are in [0, 1]. Buttons follow
/// xbox_controller_api::AllGamepadButtons() order.
///
/// The stamp is left unset because snapshot timestamps use a steady clock. The
/// lifecycle node supplies ROS time before publication.
///
/// @param strGamepadState Snapshot to convert.
/// @param charFrameId Frame id to place in the message header.
/// @return A fully populated Joy message with an unset stamp.
sensor_msgs::msg::Joy MakeJoyMessage(
    const xbox_controller_api::SGamepadState& strGamepadState,
    const std::string& charFrameId);

}  // namespace xbox_controller_api_ros
