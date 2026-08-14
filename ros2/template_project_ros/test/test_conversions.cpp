#include "template_project_ros/conversions.h"

#include <gtest/gtest.h>

TEST(TemplateProjectConversions, EvaluateCoreAndBuildResponse) {
  const double dOutput_ = template_project_ros::EvaluateTemplateCore(3.0, 2.0, 1.0);
  EXPECT_DOUBLE_EQ(dOutput_, 14.0);

  const auto objResponse_ = template_project_ros::MakeRunAlgorithmResponse(dOutput_, "ok");
  EXPECT_DOUBLE_EQ(objResponse_.output, 14.0);
  EXPECT_EQ(objResponse_.status, "ok");
}

TEST(TemplateProjectConversions, BuildStatusWithoutRclcppInit) {
  builtin_interfaces::msg::Time objStamp_;
  objStamp_.sec = 12;
  objStamp_.nanosec = 34;

  const template_project_ros::CAlgorithmRunResult objRunResult_(1.5, 4.5, 7U, "ok");
  const auto objStatus_ = template_project_ros::MakeAlgorithmStatus(objRunResult_, objStamp_);

  EXPECT_EQ(objStatus_.stamp.sec, 12);
  EXPECT_EQ(objStatus_.stamp.nanosec, 34U);
  EXPECT_DOUBLE_EQ(objStatus_.last_input, 1.5);
  EXPECT_DOUBLE_EQ(objStatus_.last_output, 4.5);
  EXPECT_EQ(objStatus_.evaluation_count, 7U);
  EXPECT_EQ(objStatus_.state, "ok");
}
