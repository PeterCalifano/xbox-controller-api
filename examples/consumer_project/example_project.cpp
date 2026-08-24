/// @file example_project.cpp
/// @brief Demonstrates consuming an installed xbox_controller_api package.
/// @details Exercises only the hardware-free core, so the example builds and
///          runs identically against an installation with or without the SDL2
///          backend. That keeps it a test of the packaging, not of the machine.

#include "example_project.h"

int main()
{
    using namespace xbox_controller_api;
    using namespace xbox_controller_api::logging;

    CLogger objLogger_("example_consumer_project", ELogLevel::Info);
    objLogger_.setLevelFromEnvironment();
    objLogger_.info("Consuming xbox_controller_api through its installed CMake package.");

    // Replay one scripted sample, which needs no controller and no SDL.
    CScriptedGamepadSource objSource_;

    SGamepadState strFrame_;
    strFrame_.dLeftStickX_ = 0.5;
    strFrame_.bButtonA_ = true;
    objSource_.pushFrame(strFrame_);

    if (!objSource_.update())
    {
        objLogger_.error("The scripted frame was not replayed; the installed library is broken.");

        return 1;
    }

    const SGamepadState &strState_ = objSource_.state();

    constexpr double dDeadzone_ = 0.15;
    const double dConditionedStickX_ = ApplyRescaledDeadzone(strState_.dLeftStickX_, dDeadzone_);
    const EButtonEdge enumButtonAEdge_ = ClassifyButtonEdge(false, strState_.bButtonA_);

    objLogger_.info("Replayed sample ", strState_.ui64SequenceId_, " with conditioned left stick X = ",
                    dConditionedStickX_);
    objLogger_.info("Button A registered a press: ",
                    (enumButtonAEdge_ == EButtonEdge::Pressed) ? "yes" : "no");

    // Example output:
    // [example_consumer_project][INFO] Consuming xbox_controller_api through its installed CMake package.
    // [example_consumer_project][INFO] Replayed sample 1 with conditioned left stick X = 0.411765
    // [example_consumer_project][INFO] Button A registered a press: yes

    return 0;
}
