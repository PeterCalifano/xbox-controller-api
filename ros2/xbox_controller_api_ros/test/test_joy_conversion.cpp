/**
 * @file test_joy_conversion.cpp
 * @brief Tests Joy message conversion without a ROS executor.
 */

#include "xbox_controller_api_ros/joy_conversion.h"

#include <gtest/gtest.h>
#include <xbox_controller_api/GamepadControls.h>

#include <cstddef>

namespace {

std::size_t AxisIndex(xbox_controller_api_ros::EJoyAxis enumAxis) {
  return static_cast<std::size_t>(enumAxis);
}

}  // namespace

TEST(JoyConversion, AxesFollowTheDocumentedOrderAndConventions) {
  xbox_controller_api::SGamepadState strState_;
  strState_.dLeftStickX_ = -1.0;
  strState_.dLeftStickY_ = 1.0;
  strState_.dRightStickX_ = 0.5;
  strState_.dRightStickY_ = -0.5;
  strState_.dLeftTrigger_ = 0.25;
  strState_.dRightTrigger_ = 1.0;

  const auto objJoy_ = xbox_controller_api_ros::MakeJoyMessage(strState_, "pad");

  ASSERT_EQ(objJoy_.axes.size(), xbox_controller_api_ros::kJoyAxisCount);
  EXPECT_FLOAT_EQ(objJoy_.axes[AxisIndex(xbox_controller_api_ros::EJoyAxis::LeftStickX)], -1.0F);
  EXPECT_FLOAT_EQ(objJoy_.axes[AxisIndex(xbox_controller_api_ros::EJoyAxis::LeftStickY)], 1.0F);
  EXPECT_FLOAT_EQ(objJoy_.axes[AxisIndex(xbox_controller_api_ros::EJoyAxis::RightStickX)], 0.5F);
  EXPECT_FLOAT_EQ(objJoy_.axes[AxisIndex(xbox_controller_api_ros::EJoyAxis::RightStickY)], -0.5F);
  EXPECT_FLOAT_EQ(objJoy_.axes[AxisIndex(xbox_controller_api_ros::EJoyAxis::LeftTrigger)], 0.25F);
  EXPECT_FLOAT_EQ(objJoy_.axes[AxisIndex(xbox_controller_api_ros::EJoyAxis::RightTrigger)], 1.0F);

  EXPECT_EQ(objJoy_.header.frame_id, "pad");
}

TEST(JoyConversion, ButtonsFollowTheLibraryControlOrder) {
  // Verify that each library button maps to the same Joy index.
  const auto spanButtons_ = xbox_controller_api::AllGamepadButtons();

  for (std::size_t szPressedIndex_ = 0; szPressedIndex_ < spanButtons_.size(); ++szPressedIndex_) {
    xbox_controller_api::SGamepadState strState_;
    switch (spanButtons_[szPressedIndex_]) {
      case xbox_controller_api::EGamepadButton::A: strState_.bButtonA_ = true; break;
      case xbox_controller_api::EGamepadButton::B: strState_.bButtonB_ = true; break;
      case xbox_controller_api::EGamepadButton::X: strState_.bButtonX_ = true; break;
      case xbox_controller_api::EGamepadButton::Y: strState_.bButtonY_ = true; break;
      case xbox_controller_api::EGamepadButton::LeftShoulder: strState_.bLeftShoulder_ = true; break;
      case xbox_controller_api::EGamepadButton::RightShoulder: strState_.bRightShoulder_ = true; break;
      case xbox_controller_api::EGamepadButton::LeftStickClick: strState_.bLeftStickClick_ = true; break;
      case xbox_controller_api::EGamepadButton::RightStickClick: strState_.bRightStickClick_ = true; break;
      case xbox_controller_api::EGamepadButton::Back: strState_.bBack_ = true; break;
      case xbox_controller_api::EGamepadButton::Start: strState_.bStart_ = true; break;
      case xbox_controller_api::EGamepadButton::Guide: strState_.bGuide_ = true; break;
      case xbox_controller_api::EGamepadButton::DpadUp: strState_.bDpadUp_ = true; break;
      case xbox_controller_api::EGamepadButton::DpadDown: strState_.bDpadDown_ = true; break;
      case xbox_controller_api::EGamepadButton::DpadLeft: strState_.bDpadLeft_ = true; break;
      case xbox_controller_api::EGamepadButton::DpadRight: strState_.bDpadRight_ = true; break;
    }

    const auto objJoy_ = xbox_controller_api_ros::MakeJoyMessage(strState_, "pad");
    ASSERT_EQ(objJoy_.buttons.size(), spanButtons_.size());

    for (std::size_t szProbedIndex_ = 0; szProbedIndex_ < objJoy_.buttons.size(); ++szProbedIndex_) {
      const std::int32_t i32Expected_ = (szProbedIndex_ == szPressedIndex_) ? 1 : 0;
      EXPECT_EQ(objJoy_.buttons[szProbedIndex_], i32Expected_)
          << "pressed index " << szPressedIndex_ << ", probed index " << szProbedIndex_;
    }
  }
}

TEST(JoyConversion, ADefaultSnapshotPublishesEverythingAtRest) {
  const xbox_controller_api::SGamepadState strState_;

  const auto objJoy_ = xbox_controller_api_ros::MakeJoyMessage(strState_, "pad");

  for (const float fAxis_ : objJoy_.axes) {
    EXPECT_FLOAT_EQ(fAxis_, 0.0F);
  }
  for (const std::int32_t i32Button_ : objJoy_.buttons) {
    EXPECT_EQ(i32Button_, 0);
  }

  // The lifecycle node, not the converter, supplies the ROS timestamp.
  EXPECT_EQ(objJoy_.header.stamp.sec, 0);
  EXPECT_EQ(objJoy_.header.stamp.nanosec, 0U);
}
