#pragma once

#include "xbox_controller_api_interfaces/msg/algorithm_status.hpp"
#include "xbox_controller_api_interfaces/srv/run_algorithm.hpp"
#include "xbox_controller_api_ros/conversions.h"

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

#include <cstdint>
#include <memory>

namespace xbox_controller_api_ros {

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
  using RunAlgorithm = xbox_controller_api_interfaces::srv::RunAlgorithm;
  using AlgorithmStatus = xbox_controller_api_interfaces::msg::AlgorithmStatus;

  void handleRunAlgorithm(
      const std::shared_ptr<RunAlgorithm::Request> objRequest_,
      std::shared_ptr<RunAlgorithm::Response> objResponse_);
  void publishStatus(const CAlgorithmRunResult& objRunResult_);

  rclcpp_lifecycle::LifecyclePublisher<AlgorithmStatus>::SharedPtr objStatusPublisher_;
  rclcpp::Service<RunAlgorithm>::SharedPtr objService_;
  double dGain_;
  double dBias_;
  std::uint64_t uiEvaluationCount_;
};

}  // namespace xbox_controller_api_ros
