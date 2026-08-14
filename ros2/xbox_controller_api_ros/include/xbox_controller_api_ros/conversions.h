#pragma once

#include "xbox_controller_api_interfaces/msg/algorithm_status.hpp"
#include "xbox_controller_api_interfaces/srv/run_algorithm.hpp"

#include <cstdint>
#include <string>

namespace xbox_controller_api_ros {

class CAlgorithmRunResult final {
 public:
  CAlgorithmRunResult(
      double dInput_,
      double dOutput_,
      std::uint64_t uiEvaluationCount_,
      std::string charState_);

  double input() const noexcept;
  double output() const noexcept;
  std::uint64_t evaluationCount() const noexcept;
  const std::string& state() const noexcept;

 private:
  double dInput_;
  double dOutput_;
  std::uint64_t uiEvaluationCount_;
  std::string charState_;
};

double EvaluateTemplateCore(double dInput_, double dGain_, double dBias_);

xbox_controller_api_interfaces::srv::RunAlgorithm::Response MakeRunAlgorithmResponse(
    double dOutput_,
    const std::string& charStatus_);

xbox_controller_api_interfaces::msg::AlgorithmStatus MakeAlgorithmStatus(
    const CAlgorithmRunResult& objRunResult_,
    const xbox_controller_api_interfaces::msg::AlgorithmStatus::_stamp_type& objStamp_);

}  // namespace xbox_controller_api_ros
