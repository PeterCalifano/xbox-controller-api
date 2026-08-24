/// @file example_read_controller.cpp
/// @brief Live read-out of an attached controller at a fixed 50 Hz poll rate.
/// @details Opens a controller, polls at 50 Hz, prints button transitions, and
///          stops after the requested duration or a disconnect.
///
/// Usage:
///     ./example_read_controller [runtime_seconds]

#include <xbox_controller_api/CSdlGamepadSource.h>
#include <xbox_controller_api/GamepadControls.h>
#include <xbox_controller_api/GamepadFilters.h>
#include <xbox_controller_api/SGamepadState.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <thread>

namespace
{
    using xbox_controller_api::AllGamepadButtons;
    using xbox_controller_api::ApplyStickDeadzone;
    using xbox_controller_api::ClassifyButtonEdge;
    using xbox_controller_api::CSdlGamepadSource;
    using xbox_controller_api::EButtonEdge;
    using xbox_controller_api::EGamepadButton;
    using xbox_controller_api::GetGamepadButtonName;
    using xbox_controller_api::SGamepadState;

    /// Poll period corresponding to the 50 Hz sampling rate of the demo.
    constexpr std::chrono::milliseconds objPollPeriod{20};

    /// Runtime applied when the caller passes no argument.
    constexpr int i32DefaultRuntimeSeconds = 20;

    /// Upper bound accepted for the runtime argument, in seconds.
    constexpr long i64MaximumRuntimeSeconds = 3600;

    /// Deadzone applied to sticks before display, inside the suggested range.
    constexpr double dDisplayDeadzone = 0.15;

    /// Print the axis summary at 5 Hz rather than at the full poll rate.
    constexpr int i32AxisPrintDecimation = 10;

    /// @brief Read an optional positive runtime in seconds from the arguments.
    [[nodiscard]] int ParseRuntimeSeconds(int argc, char **argv)
    {
        if (argc < 2)
        {
            return i32DefaultRuntimeSeconds;
        }

        char *charParseEnd_ = nullptr;
        const long i64Parsed_ = std::strtol(argv[1], &charParseEnd_, 10);

        // Accept only a complete, positive runtime in the supported range.
        const bool bIsValid_ = (charParseEnd_ != argv[1]) && (*charParseEnd_ == '\0') &&
                               (i64Parsed_ > 0) && (i64Parsed_ <= i64MaximumRuntimeSeconds);

        if (!bIsValid_)
        {
            std::cerr << "Ignoring invalid runtime argument '" << argv[1] << "'; using "
                      << i32DefaultRuntimeSeconds << " s\n";

            return i32DefaultRuntimeSeconds;
        }

        return static_cast<int>(i64Parsed_);
    }

    /// @brief Print every button that changed between two consecutive samples.
    void PrintButtonEdges(const SGamepadState &strPreviousState, const SGamepadState &strCurrentState)
    {
        // Iterate the library's stable control list.
        for (const EGamepadButton enumButton_ : AllGamepadButtons())
        {
            const EButtonEdge enumEdge_ =
                ClassifyButtonEdge(strPreviousState, strCurrentState, enumButton_);

            // Print transitions, not repeated held states.
            if (enumEdge_ == EButtonEdge::Pressed)
            {
                std::cout << "  [" << strCurrentState.ui64SequenceId_ << "] "
                          << GetGamepadButtonName(enumButton_) << " pressed\n";
            }
            else if (enumEdge_ == EButtonEdge::Released)
            {
                std::cout << "  [" << strCurrentState.ui64SequenceId_ << "] "
                          << GetGamepadButtonName(enumButton_) << " released\n";
            }
        }
    }

    /// @brief Report whether any axis of a conditioned sample is deflected.
    [[nodiscard]] bool HasAxisActivity(const SGamepadState &strConditionedState) noexcept
    {
        return std::abs(strConditionedState.dLeftStickX_) > 0.0 ||
               std::abs(strConditionedState.dLeftStickY_) > 0.0 ||
               std::abs(strConditionedState.dRightStickX_) > 0.0 ||
               std::abs(strConditionedState.dRightStickY_) > 0.0 ||
               strConditionedState.dLeftTrigger_ > 0.0 || strConditionedState.dRightTrigger_ > 0.0;
    }

    /// @brief Print the stick and trigger values of a conditioned sample.
    void PrintAxes(const SGamepadState &strConditionedState)
    {
        std::cout << std::fixed << std::setprecision(2) << "  LS("
                  << strConditionedState.dLeftStickX_ << ", " << strConditionedState.dLeftStickY_
                  << ")  RS(" << strConditionedState.dRightStickX_ << ", "
                  << strConditionedState.dRightStickY_
                  << ")  LT=" << strConditionedState.dLeftTrigger_
                  << "  RT=" << strConditionedState.dRightTrigger_ << "\n";
    }
} // namespace

int main(int argc, char **argv)
{
    const int i32RuntimeSeconds_ = ParseRuntimeSeconds(argc, argv);

    // This example treats an unavailable backend or controller as a normal exit.
    if (!CSdlGamepadSource::isBackendAvailable())
    {
        std::cout << "SDL2 backend is not compiled into this build; nothing to read.\n"
                     "Reconfigure with -DENABLE_SDL2=ON to enable it.\n";

        return 0;
    }

    CSdlGamepadSource objSource_;

    if (!objSource_.open())
    {
        std::cout << "No controller available: " << objSource_.lastError() << "\n";

        return 0;
    }

    std::cout << "Reading '" << objSource_.deviceName() << "' at 50 Hz for " << i32RuntimeSeconds_
              << " s.\nPress Start to quit early. Sticks use a " << dDisplayDeadzone
              << " deadzone.\n";

    // A held button is reported as pressed on the first poll.
    SGamepadState strPreviousState_;

    const std::chrono::steady_clock::time_point objDeadline_ =
        std::chrono::steady_clock::now() + std::chrono::seconds(i32RuntimeSeconds_);
    int i32PollCount_ = 0;

    while (std::chrono::steady_clock::now() < objDeadline_)
    {
        const std::chrono::steady_clock::time_point objFrameStart_ =
            std::chrono::steady_clock::now();

        // A failed poll after open() means the controller was detached.
        if (!objSource_.update())
        {
            std::cout << "Controller detached after " << i32PollCount_ << " polls; stopping.\n";
            break;
        }

        // Apply the display deadzone once per sample.
        const SGamepadState strState_ = ApplyStickDeadzone(objSource_.state(), dDisplayDeadzone);

        PrintButtonEdges(strPreviousState_, strState_);

        if (ClassifyButtonEdge(strPreviousState_.bStart_, strState_.bStart_) ==
            EButtonEdge::Pressed)
        {
            std::cout << "Start pressed; quitting.\n";
            break;
        }

        // Limit axis output to keep button transitions readable.
        if ((i32PollCount_ % i32AxisPrintDecimation == 0) && HasAxisActivity(strState_))
        {
            PrintAxes(strState_);
        }

        strPreviousState_ = strState_;
        ++i32PollCount_;

        // Keep the poll cadence relative to the frame start.
        std::this_thread::sleep_until(objFrameStart_ + objPollPeriod);
    }

    std::cout << "Completed " << i32PollCount_ << " polls; last sequence id was "
              << objSource_.state().ui64SequenceId_ << ".\n";

    objSource_.close();

    // Example output:
    // Reading 'X360 Controller' at 50 Hz for 20 s.
    // Press Start to quit early. Sticks use a 0.15 deadzone.
    //   [37] A pressed
    //   [42] A released
    //   LS(0.00, 0.62)  RS(0.00, 0.00)  LT=0.00  RT=0.41
    // Start pressed; quitting.
    // Completed 96 polls; last sequence id was 97.

    return 0;
}
