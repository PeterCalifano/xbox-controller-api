#include "xbox_controller_api_ros/conversions.h"

#include <gtest/gtest.h>

TEST(TemplateProjectConversions, EvaluateCoreAndBuildResponse) {
  // Inputs stay inside the normalized axis range the controller API guarantees,
  // so this exercises the conditioning rather than its clamp.
  const double dOutput_ = xbox_controller_api_ros::EvaluateTemplateCore(0.2, 2.0, 0.1);
  EXPECT_DOUBLE_EQ(dOutput_, 0.5);

  const auto objResponse_ = xbox_controller_api_ros::MakeRunAlgorithmResponse(dOutput_, "ok");
  EXPECT_DOUBLE_EQ(objResponse_.output, 0.5);
  EXPECT_EQ(objResponse_.status, "ok");
}

TEST(TemplateProjectConversions, EvaluateCoreClampsOutOfRangeInput) {
  // Anything beyond full deflection is clamped instead of propagating, which is
  // the contract the conditioning layer enforces for every consumer.
  EXPECT_DOUBLE_EQ(xbox_controller_api_ros::EvaluateTemplateCore(3.0, 2.0, 1.0), 1.0);
  EXPECT_DOUBLE_EQ(xbox_controller_api_ros::EvaluateTemplateCore(-3.0, 2.0, -1.0), -1.0);
}

TEST(TemplateProjectConversions, BuildStatusWithoutRclcppInit) {
  builtin_interfaces::msg::Time objStamp_;
  objStamp_.sec = 12;
  objStamp_.nanosec = 34;

  const xbox_controller_api_ros::CAlgorithmRunResult objRunResult_(1.5, 4.5, 7U, "ok");
  const auto objStatus_ = xbox_controller_api_ros::MakeAlgorithmStatus(objRunResult_, objStamp_);

  EXPECT_EQ(objStatus_.stamp.sec, 12);
  EXPECT_EQ(objStatus_.stamp.nanosec, 34U);
  EXPECT_DOUBLE_EQ(objStatus_.last_input, 1.5);
  EXPECT_DOUBLE_EQ(objStatus_.last_output, 4.5);
  EXPECT_EQ(objStatus_.evaluation_count, 7U);
  EXPECT_EQ(objStatus_.state, "ok");
}
