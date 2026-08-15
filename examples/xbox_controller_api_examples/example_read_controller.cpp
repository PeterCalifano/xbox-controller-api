/// @file example_read_controller.cpp
/// @brief Live read-out of an attached controller at a fixed 50 Hz poll rate.
/// @details Demonstrates the intended consumer shape: open once, poll on a
///          cadence the caller owns, derive button edges through the pure
///          filters, and treat a detach as an explicit stop rather than
///          something the library silently recovers from. Runs for a bounded
///          time so it is safe to launch from a script, and exits cleanly when
///          no controller or no backend is present.
///
/// Usage:
///     ./example_read_controller [runtime_seconds]

#include <xbox_controller_api/CSdlGamepadSource.h>
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
    using xbox_controller_api::ClassifyButtonEdge;
    using xbox_controller_api::CSdlGamepadSource;
    using xbox_controller_api::EButtonEdge;
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

    /// @brief One button of the snapshot, paired with its display name.
    struct SNamedButton
    {
        const char *charName_;
        bool SGamepadState::*pPressedMember_;
    };

    // A member-pointer table keeps the edge loop independent of the button
    // count: adding a control later means adding one row, not another branch.
    constexpr SNamedButton arrTrackedButtons[] = {
        {"A", &SGamepadState::bButtonA_},
        {"B", &SGamepadState::bButtonB_},
        {"X", &SGamepadState::bButtonX_},
        {"Y", &SGamepadState::bButtonY_},
        {"LB", &SGamepadState::bLeftShoulder_},
        {"RB", &SGamepadState::bRightShoulder_},
        {"LS", &SGamepadState::bLeftStickClick_},
        {"RS", &SGamepadState::bRightStickClick_},
        {"Back", &SGamepadState::bBack_},
        {"Start", &SGamepadState::bStart_},
        {"Guide", &SGamepadState::bGuide_},
        {"DpadUp", &SGamepadState::bDpadUp_},
        {"DpadDown", &SGamepadState::bDpadDown_},
        {"DpadLeft", &SGamepadState::bDpadLeft_},
        {"DpadRight", &SGamepadState::bDpadRight_}};

    /// @brief Read an optional positive runtime in seconds from the arguments.
    [[nodiscard]] int ParseRuntimeSeconds(int argc, char **argv)
    {
        if (argc < 2)
        {
            return i32DefaultRuntimeSeconds;
        }

        char *charParseEnd_ = nullptr;
        const long i64Parsed_ = std::strtol(argv[1], &charParseEnd_, 10);

        // Reject trailing garbage as well as out-of-range values, so a typo
        // cannot silently turn into an unexpected run length.
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
        for (const SNamedButton &strButton_ : arrTrackedButtons)
        {
            const EButtonEdge enumEdge_ =
                ClassifyButtonEdge(strPreviousState.*strButton_.pPressedMember_,
                                   strCurrentState.*strButton_.pPressedMember_);

            // Only transitions are interesting; Held and None would repeat at
            // the poll rate and drown out everything else.
            if (enumEdge_ == EButtonEdge::Pressed)
            {
                std::cout << "  [" << strCurrentState.ui64SequenceId_ << "] "
                          << strButton_.charName_ << " pressed\n";
            }
            else if (enumEdge_ == EButtonEdge::Released)
            {
                std::cout << "  [" << strCurrentState.ui64SequenceId_ << "] "
                          << strButton_.charName_ << " released\n";
            }
        }
    }

    /// @brief Report whether any conditioned axis has left its rest position.
    [[nodiscard]] bool HasAxisActivity(const SGamepadState &strState) noexcept
    {
        using xbox_controller_api::ApplyRescaledDeadzone;

        const bool bStickActive_ = std::abs(ApplyRescaledDeadzone(strState.dLeftStickX_,
                                                                  dDisplayDeadzone)) > 0.0 ||
                                   std::abs(ApplyRescaledDeadzone(strState.dLeftStickY_,
                                                                  dDisplayDeadzone)) > 0.0 ||
                                   std::abs(ApplyRescaledDeadzone(strState.dRightStickX_,
                                                                  dDisplayDeadzone)) > 0.0 ||
                                   std::abs(ApplyRescaledDeadzone(strState.dRightStickY_,
                                                                  dDisplayDeadzone)) > 0.0;

        return bStickActive_ || strState.dLeftTrigger_ > 0.0 || strState.dRightTrigger_ > 0.0;
    }

    /// @brief Print the conditioned stick and trigger values on one line.
    void PrintAxes(const SGamepadState &strState)
    {
        using xbox_controller_api::ApplyRescaledDeadzone;

        std::cout << std::fixed << std::setprecision(2) << "  LS("
                  << ApplyRescaledDeadzone(strState.dLeftStickX_, dDisplayDeadzone) << ", "
                  << ApplyRescaledDeadzone(strState.dLeftStickY_, dDisplayDeadzone) << ")  RS("
                  << ApplyRescaledDeadzone(strState.dRightStickX_, dDisplayDeadzone) << ", "
                  << ApplyRescaledDeadzone(strState.dRightStickY_, dDisplayDeadzone)
                  << ")  LT=" << strState.dLeftTrigger_ << "  RT=" << strState.dRightTrigger_ << "\n";
    }
} // namespace

int main(int argc, char **argv)
{
    const int i32RuntimeSeconds_ = ParseRuntimeSeconds(argc, argv);

    // Both unavailability cases exit successfully: this is a demo, and a machine
    // without SDL2 or without a pad has not done anything wrong.
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

    // The previous sample starts fully released, so a button already held at
    // startup registers as a press on the first poll.
    SGamepadState strPreviousState_;

    const std::chrono::steady_clock::time_point objDeadline_ =
        std::chrono::steady_clock::now() + std::chrono::seconds(i32RuntimeSeconds_);
    int i32PollCount_ = 0;

    while (std::chrono::steady_clock::now() < objDeadline_)
    {
        const std::chrono::steady_clock::time_point objFrameStart_ =
            std::chrono::steady_clock::now();

        // A failed poll on an opened device means the pad went away, which this
        // demo treats as a reason to stop rather than to retry.
        if (!objSource_.update())
        {
            std::cout << "Controller detached after " << i32PollCount_ << " polls; stopping.\n";
            break;
        }

        const SGamepadState &strState_ = objSource_.state();

        PrintButtonEdges(strPreviousState_, strState_);

        if (ClassifyButtonEdge(strPreviousState_.bStart_, strState_.bStart_) ==
            EButtonEdge::Pressed)
        {
            std::cout << "Start pressed; quitting.\n";
            break;
        }

        // Decimate the axis read-out so a resting pad stays silent and an active
        // one does not scroll the button edges off screen.
        if ((i32PollCount_ % i32AxisPrintDecimation == 0) && HasAxisActivity(strState_))
        {
            PrintAxes(strState_);
        }

        strPreviousState_ = strState_;
        ++i32PollCount_;

        // Sleep against the frame start so the cadence does not drift with the
        // time spent polling and printing.
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
