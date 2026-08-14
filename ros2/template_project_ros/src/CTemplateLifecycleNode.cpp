#include "template_project_ros/CTemplateLifecycleNode.h"

#include <rclcpp_components/register_node_macro.hpp>

#include <utility>

namespace template_project_ros {

namespace {
using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;
}  // namespace

CTemplateLifecycleNode::CTemplateLifecycleNode(const rclcpp::NodeOptions& objOptions_)
    : rclcpp_lifecycle::LifecycleNode("template_algorithm", objOptions_),
      dGain_(1.0),
      dBias_(0.0),
      uiEvaluationCount_(0U) {
  declare_parameter<double>("gain", dGain_);
  declare_parameter<double>("bias", dBias_);
}

CallbackReturn CTemplateLifecycleNode::on_configure(const rclcpp_lifecycle::State&) {
  dGain_ = get_parameter("gain").as_double();
  dBias_ = get_parameter("bias").as_double();
  uiEvaluationCount_ = 0U;

  objStatusPublisher_ = create_publisher<AlgorithmStatus>("~/status", rclcpp::QoS(10));
  objService_ = create_service<RunAlgorithm>(
      "~/run_algorithm",
      [this](const std::shared_ptr<RunAlgorithm::Request> objRequest_,
             std::shared_ptr<RunAlgorithm::Response> objResponse_) {
        handleRunAlgorithm(objRequest_, std::move(objResponse_));
      });

  RCLCPP_INFO(get_logger(), "Configured template algorithm with gain=%f bias=%f", dGain_, dBias_);
  return CallbackReturn::SUCCESS;
}

CallbackReturn CTemplateLifecycleNode::on_activate(const rclcpp_lifecycle::State&) {
  if (objStatusPublisher_) {
    objStatusPublisher_->on_activate();
  }
  RCLCPP_INFO(get_logger(), "Activated template algorithm node");
  return CallbackReturn::SUCCESS;
}

CallbackReturn CTemplateLifecycleNode::on_deactivate(const rclcpp_lifecycle::State&) {
  if (objStatusPublisher_) {
    objStatusPublisher_->on_deactivate();
  }
  RCLCPP_INFO(get_logger(), "Deactivated template algorithm node");
  return CallbackReturn::SUCCESS;
}

CallbackReturn CTemplateLifecycleNode::on_cleanup(const rclcpp_lifecycle::State&) {
  objService_.reset();
  objStatusPublisher_.reset();
  uiEvaluationCount_ = 0U;
  RCLCPP_INFO(get_logger(), "Cleaned up template algorithm node");
  return CallbackReturn::SUCCESS;
}

void CTemplateLifecycleNode::handleRunAlgorithm(
    const std::shared_ptr<RunAlgorithm::Request> objRequest_,
    std::shared_ptr<RunAlgorithm::Response> objResponse_) {
  // The editable core-library seam is in conversions.cpp; keep this node ROS-only.
  const double dOutput_ = EvaluateTemplateCore(objRequest_->input, dGain_, dBias_);

  ++uiEvaluationCount_;
  const CAlgorithmRunResult objRunResult_(
      objRequest_->input,
      dOutput_,
      uiEvaluationCount_,
      "ok");

  *objResponse_ = MakeRunAlgorithmResponse(objRunResult_.output(), objRunResult_.state());
  publishStatus(objRunResult_);
}

void CTemplateLifecycleNode::publishStatus(const CAlgorithmRunResult& objRunResult_) {
  if (!objStatusPublisher_ || !objStatusPublisher_->is_activated()) {
    return;
  }

  const auto objStatus_ = MakeAlgorithmStatus(objRunResult_, get_clock()->now());
  objStatusPublisher_->publish(objStatus_);
}

}  // namespace template_project_ros

RCLCPP_COMPONENTS_REGISTER_NODE(template_project_ros::CTemplateLifecycleNode)
