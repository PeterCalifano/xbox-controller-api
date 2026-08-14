#include "template_project_ros/conversions.h"

// --- template-core call site (EDIT ME after renaming the template): include ---
#include "wrapped_impl/CWrapperPlaceholder.h"
// --- template-core call site include end ---

#include <utility>

namespace template_project_ros {

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
  // --- template-core call site (EDIT ME after renaming the template): body ---
  const double dAdaptedInput_ = (dInput_ * dGain_) + dBias_;
  const double dOutput_ = cpp_playground::CWrapperPlaceholder::multiplyBy2(dAdaptedInput_);
  // --- template-core call site body end ---
  return dOutput_;
}

template_project_interfaces::srv::RunAlgorithm::Response MakeRunAlgorithmResponse(
    double dOutput_,
    const std::string& charStatus_) {
  template_project_interfaces::srv::RunAlgorithm::Response objResponse_;
  objResponse_.output = dOutput_;
  objResponse_.status = charStatus_;
  return objResponse_;
}

template_project_interfaces::msg::AlgorithmStatus MakeAlgorithmStatus(
    const CAlgorithmRunResult& objRunResult_,
    const template_project_interfaces::msg::AlgorithmStatus::_stamp_type& objStamp_) {
  template_project_interfaces::msg::AlgorithmStatus objStatus_;
  objStatus_.stamp = objStamp_;
  objStatus_.last_input = objRunResult_.input();
  objStatus_.last_output = objRunResult_.output();
  objStatus_.evaluation_count = objRunResult_.evaluationCount();
  objStatus_.state = objRunResult_.state();
  return objStatus_;
}

}  // namespace template_project_ros
