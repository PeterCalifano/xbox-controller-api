/// @file example_scripted_replay.cpp
/// @brief Demonstrates the hardware-free core of xbox_controller_api.
/// @details Uses only the pure layer, so its behavior and output are identical
///          whether or not the SDL2 backend was compiled in. This is the example
///          to run when validating a build on a machine with no controller.

#include <utils/logging/CLogger.h>
#include <xbox_controller_api/CScriptedGamepadSource.h>
#include <xbox_controller_api/GamepadFilters.h>
#include <xbox_controller_api/SGamepadState.h>

#include <iomanip>
#include <iostream>

namespace
{
    /// @brief Render a button edge as a short display label.
    [[nodiscard]] const char *DescribeEdge(xbox_controller_api::EButtonEdge enumEdge) noexcept
    {
        switch (enumEdge)
        {
        case xbox_controller_api::EButtonEdge::Pressed:
            return "Pressed";
        case xbox_controller_api::EButtonEdge::Released:
            return "Released";
        case xbox_controller_api::EButtonEdge::Held:
            return "Held";
        case xbox_controller_api::EButtonEdge::None:
            break;
        }

        return "None";
    }
} // namespace

int main()
{
    using namespace xbox_controller_api;
    using namespace xbox_controller_api::logging;

    CLogger objLogger_("example_scripted_replay", ELogLevel::Info);
    objLogger_.setLevelFromEnvironment();
    objLogger_.info("Replaying a scripted controller session through the pure filters.");

    // Script a short session: a resting sample, a deflection with A held, and an
    // unplug. No hardware and no SDL are involved, so the run is deterministic.
    CScriptedGamepadSource objSource_;

    const SGamepadState strRestingFrame_;
    objSource_.pushFrame(strRestingFrame_);

    SGamepadState strActiveFrame_;
    strActiveFrame_.dLeftStickY_ = 0.80;
    strActiveFrame_.dRightStickX_ = 0.05; // Deliberately inside the deadzone.
    strActiveFrame_.bButtonA_ = true;
    objSource_.pushFrame(strActiveFrame_);

    objSource_.pushDisconnect();

    constexpr double dDeadzone_ = 0.15;
    SGamepadState strPreviousState_;

    // Drive the queue rather than the return value, so the disconnect frame is
    // observed too instead of ending the loop before it is printed.
    while (objSource_.pendingFrameCount() > 0U)
    {
        const bool bConnected_ = objSource_.update();
        const SGamepadState &strState_ = objSource_.state();

        const double dLeftStickY_ = ApplyRescaledDeadzone(strState_.dLeftStickY_, dDeadzone_);
        const double dRightStickX_ = ApplyRescaledDeadzone(strState_.dRightStickX_, dDeadzone_);
        const EButtonEdge enumButtonAEdge_ =
            ClassifyButtonEdge(strPreviousState_.bButtonA_, strState_.bButtonA_);

        std::cout << std::fixed << std::setprecision(2) << "sample " << strState_.ui64SequenceId_
                  << "  connected=" << (bConnected_ ? "yes" : "no ") << "  leftStickY=" << dLeftStickY_
                  << "  rightStickX=" << dRightStickX_ << "  buttonA=" << DescribeEdge(enumButtonAEdge_)
                  << "\n";

        strPreviousState_ = strState_;
    }

    objLogger_.info("Scripted session complete.");

    // Example output:
    // [example_scripted_replay][INFO] Replaying a scripted controller session through the pure filters.
    // sample 1  connected=yes  leftStickY=0.00  rightStickX=0.00  buttonA=None
    // sample 2  connected=yes  leftStickY=0.76  rightStickX=0.00  buttonA=Pressed
    // sample 3  connected=no   leftStickY=0.00  rightStickX=0.00  buttonA=Released
    // [example_scripted_replay][INFO] Scripted session complete.

    return 0;
}
