/**
 * @file joy_conversion.cpp
 * @brief Implements conversion from controller snapshots to Joy messages.
 */

#include "xbox_controller_api_ros/joy_conversion.h"

#include <xbox_controller_api/GamepadControls.h>

namespace xbox_controller_api_ros {

sensor_msgs::msg::Joy MakeJoyMessage(
    const xbox_controller_api::SGamepadState& strGamepadState,
    const std::string& charFrameId) {
  sensor_msgs::msg::Joy objJoy_;
  objJoy_.header.frame_id = charFrameId;

  // Keep the documented EJoyAxis order in the published message.
  objJoy_.axes.resize(kJoyAxisCount);
  objJoy_.axes[static_cast<std::size_t>(EJoyAxis::LeftStickX)] =
      static_cast<float>(strGamepadState.dLeftStickX_);
  objJoy_.axes[static_cast<std::size_t>(EJoyAxis::LeftStickY)] =
      static_cast<float>(strGamepadState.dLeftStickY_);
  objJoy_.axes[static_cast<std::size_t>(EJoyAxis::RightStickX)] =
      static_cast<float>(strGamepadState.dRightStickX_);
  objJoy_.axes[static_cast<std::size_t>(EJoyAxis::RightStickY)] =
      static_cast<float>(strGamepadState.dRightStickY_);
  objJoy_.axes[static_cast<std::size_t>(EJoyAxis::LeftTrigger)] =
      static_cast<float>(strGamepadState.dLeftTrigger_);
  objJoy_.axes[static_cast<std::size_t>(EJoyAxis::RightTrigger)] =
      static_cast<float>(strGamepadState.dRightTrigger_);

  // Use the library's stable control order for Joy buttons.
  const auto spanButtons_ = xbox_controller_api::AllGamepadButtons();
  objJoy_.buttons.reserve(spanButtons_.size());
  for (const xbox_controller_api::EGamepadButton enumButton_ : spanButtons_) {
    objJoy_.buttons.push_back(
        xbox_controller_api::GetButton(strGamepadState, enumButton_) ? 1 : 0);
  }

  return objJoy_;
}

}  // namespace xbox_controller_api_ros
