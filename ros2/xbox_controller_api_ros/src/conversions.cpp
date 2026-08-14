#include "xbox_controller_api_ros/conversions.h"

// --- core call site (EDIT ME once the real controller API exists): include ---
// Replace this placeholder include with the real xbox_controller_api header.
#include "wrapped_impl/CWrapperPlaceholder.h"
// --- core call site include end ---

#include <utility>

namespace xbox_controller_api_ros {

CAlgorithmRunResult::CAlgorithmRunResult(
    double dInput_,
    double dOutput_,
    std::uint64_t uiEvaluationCount_,
    std::string charState_)
    : dInput_(dInput_),
      dOutput_(dOutput_),
      uiEvaluationCount_(uiEvaluationCount_),
      charState_(std::move(charState_)) {}

double CAlgorithmRunResult::input() const noexcept {
  return dInput_;
}

double CAlgorithmRunResult::output() const noexcept {
  return dOutput_;
}

std::uint64_t CAlgorithmRunResult::evaluationCount() const noexcept {
  return uiEvaluationCount_;
}

const std::string& CAlgorithmRunResult::state() const noexcept {
  return charState_;
}

double EvaluateTemplateCore(double dInput_, double dGain_, double dBias_) {
  // --- core call site (EDIT ME once the real controller API exists): body ---
  // This still calls the placeholder so the overlay keeps building. Swap it for
  // the real xbox_controller_api entry point when the library gains one.
  const double dAdaptedInput_ = (dInput_ * dGain_) + dBias_;
  const double dOutput_ = xbox_controller_api::CWrapperPlaceholder::multiplyBy2(dAdaptedInput_);
  // --- core call site body end ---
  return dOutput_;
}

xbox_controller_api_interfaces::srv::RunAlgorithm::Response MakeRunAlgorithmResponse(
    double dOutput_,
    const std::string& charStatus_) {
  xbox_controller_api_interfaces::srv::RunAlgorithm::Response objResponse_;
  objResponse_.output = dOutput_;
  objResponse_.status = charStatus_;
  return objResponse_;
}

xbox_controller_api_interfaces::msg::AlgorithmStatus MakeAlgorithmStatus(
    const CAlgorithmRunResult& objRunResult_,
    const xbox_controller_api_interfaces::msg::AlgorithmStatus::_stamp_type& objStamp_) {
  xbox_controller_api_interfaces::msg::AlgorithmStatus objStatus_;
  objStatus_.stamp = objStamp_;
  objStatus_.last_input = objRunResult_.input();
  objStatus_.last_output = objRunResult_.output();
  objStatus_.evaluation_count = objRunResult_.evaluationCount();
  objStatus_.state = objRunResult_.state();
  return objStatus_;
}

}  // namespace xbox_controller_api_ros
