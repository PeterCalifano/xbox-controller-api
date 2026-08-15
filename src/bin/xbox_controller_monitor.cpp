/// @file xbox_controller_monitor.cpp
/// @brief Installed diagnostic that monitors an attached controller live.
/// @details Answers the questions worth asking on a fresh machine: was the SDL2
///          backend compiled in, can it attach to a device, and what is that
///          device actually reporting. Unlike the examples this tool applies no
///          deadzone, so stick drift and stuck triggers stay visible, which is
///          the whole point of a diagnostic.
///
///          Polls at 50 Hz until interrupted, reporting only what changed so a
///          long session stays readable. Exits cleanly on SIGINT or SIGTERM.
///
///          POSIX only: the shutdown path uses sigaction.

#include <utils/logging/CLogger.h>
#include <xbox_controller_api/CSdlGamepadSource.h>
#include <xbox_controller_api/GamepadControls.h>
#include <xbox_controller_api/GamepadFilters.h>
#include <xbox_controller_api/SGamepadState.h>

#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace
{
    using xbox_controller_api::AllGamepadButtons;
    using xbox_controller_api::ClassifyButtonEdge;
    using xbox_controller_api::CSdlGamepadSource;
    using xbox_controller_api::EButtonEdge;
    using xbox_controller_api::EGamepadButton;
    using xbox_controller_api::GetGamepadButtonName;
    using xbox_controller_api::SGamepadState;
    using xbox_controller_api::logging::CLogger;

    /// Poll period giving the 50 Hz sampling rate of the monitor.
    constexpr std::chrono::milliseconds objPollPeriod{20};

    /// Smallest axis movement worth reporting, chosen above sensor noise.
    constexpr double dAxisChangeEpsilon = 0.02;

    /// Polls between heartbeat lines, so a quiet pad still proves liveness.
    constexpr int i32HeartbeatPolls = 250;

    /// Shutdown request recorded by the signal handler. volatile sig_atomic_t is
    /// the only object type the standard permits a handler to touch.
    volatile std::sig_atomic_t i32TerminateRequest = 0;

    /// @brief Record a shutdown request and return immediately.
    void SignalHandler(int i32Signal) noexcept
    {
        // Almost nothing is async-signal-safe, so the handler only stores the
        // request and lets the polling loop decide what to do about it.
        i32TerminateRequest = i32Signal;
    }

    /// @brief Install the shutdown handler for SIGINT and SIGTERM.
    void InstallSignalHandlers()
    {
        // sigaction is used rather than std::signal because the latter leaves it
        // implementation defined whether the disposition resets to SIG_DFL on
        // delivery, which would let a second interrupt kill the process partway
        // through releasing the SDL device.
        struct sigaction strAction{};
        strAction.sa_handler = SignalHandler;
        sigemptyset(&strAction.sa_mask);
        strAction.sa_flags = 0;

        if (sigaction(SIGINT, &strAction, nullptr) == -1)
        {
            std::perror("sigaction(SIGINT)");
            std::exit(EXIT_FAILURE);
        }

        if (sigaction(SIGTERM, &strAction, nullptr) == -1)
        {
            std::perror("sigaction(SIGTERM)");
            std::exit(EXIT_FAILURE);
        }
    }

    /// @brief Log every button that changed between two consecutive samples.
    /// @return True when at least one transition was reported.
    bool ReportButtonEdges(CLogger &objLogger,
                           const SGamepadState &strPreviousState,
                           const SGamepadState &strCurrentState)
    {
        bool bReportedAny_ = false;

        // Driven by the library's control list, so a control added there is
        // reported here without touching this loop.
        for (const EGamepadButton enumButton_ : AllGamepadButtons())
        {
            const EButtonEdge enumEdge_ =
                ClassifyButtonEdge(strPreviousState, strCurrentState, enumButton_);

            if (enumEdge_ != EButtonEdge::Pressed && enumEdge_ != EButtonEdge::Released)
            {
                continue;
            }

            objLogger.info("Sample ", strCurrentState.ui64SequenceId_, ": button ",
                           GetGamepadButtonName(enumButton_), " ",
                           (enumEdge_ == EButtonEdge::Pressed) ? "pressed" : "released");
            bReportedAny_ = true;
        }

        return bReportedAny_;
    }

    /// @brief Report whether any axis moved enough to be worth logging.
    [[nodiscard]] bool HasAxisChange(const SGamepadState &strPreviousState,
                                     const SGamepadState &strCurrentState) noexcept
    {
        const double arrAxisDeltas_[] = {
            strCurrentState.dLeftStickX_ - strPreviousState.dLeftStickX_,
            strCurrentState.dLeftStickY_ - strPreviousState.dLeftStickY_,
            strCurrentState.dRightStickX_ - strPreviousState.dRightStickX_,
            strCurrentState.dRightStickY_ - strPreviousState.dRightStickY_,
            strCurrentState.dLeftTrigger_ - strPreviousState.dLeftTrigger_,
            strCurrentState.dRightTrigger_ - strPreviousState.dRightTrigger_};

        for (const double dDelta_ : arrAxisDeltas_)
        {
            if (std::abs(dDelta_) > dAxisChangeEpsilon)
            {
                return true;
            }
        }

        return false;
    }

    /// @brief Log the raw normalized axis values of the current sample.
    void ReportAxes(CLogger &objLogger, const SGamepadState &strState)
    {
        // Deliberately unconditioned: a diagnostic must show the drift and the
        // stuck axes that a deadzone would hide.
        objLogger.info("Sample ", strState.ui64SequenceId_, ": LS(", strState.dLeftStickX_, ", ",
                       strState.dLeftStickY_, ") RS(", strState.dRightStickX_, ", ",
                       strState.dRightStickY_, ") LT=", strState.dLeftTrigger_,
                       " RT=", strState.dRightTrigger_);
    }
} // namespace

int main()
{
    using namespace xbox_controller_api;
    using namespace xbox_controller_api::logging;

    InstallSignalHandlers();

    CLogger objLogger_("xbox_controller_monitor", ELogLevel::Info);
    objLogger_.setLevelFromEnvironment();

    if (!CSdlGamepadSource::isBackendAvailable())
    {
        objLogger_.info("SDL2 backend is not compiled into this build; "
                        "reconfigure with -DENABLE_SDL2=ON to enable it.");

        return 0;
    }

    objLogger_.info("SDL2 backend is available; attempting to attach.");

    CSdlGamepadSource objSource_;

    if (!objSource_.open())
    {
        objLogger_.info("No controller attached: ", objSource_.lastError());

        return 0;
    }

    objLogger_.info("Attached to '", objSource_.deviceName(),
                    "'. Monitoring at 50 Hz; press Ctrl-C to stop.");

    // Compare against the snapshot open() published, so the monitor reports
    // changes since attachment rather than re-reporting the resting position.
    SGamepadState strPreviousState_ = objSource_.state();
    int i32PollCount_ = 0;

    while (i32TerminateRequest == 0)
    {
        const std::chrono::steady_clock::time_point objFrameStart_ =
            std::chrono::steady_clock::now();

        if (!objSource_.update())
        {
            objLogger_.warning("Controller detached; stopping.");
            break;
        }

        const SGamepadState &strState_ = objSource_.state();

        // Report only what changed. Logging every sample would bury the events
        // worth seeing and serialize the process-wide output lock at poll rate.
        const bool bButtonsChanged_ = ReportButtonEdges(objLogger_, strPreviousState_, strState_);

        if (HasAxisChange(strPreviousState_, strState_))
        {
            ReportAxes(objLogger_, strState_);
        }
        else if (!bButtonsChanged_ && (i32PollCount_ % i32HeartbeatPolls == 0))
        {
            objLogger_.info("Monitoring; ", i32PollCount_, " polls so far, sample ",
                            strState_.ui64SequenceId_);
        }

        strPreviousState_ = strState_;
        ++i32PollCount_;

        // Sleep against the frame start so the cadence does not drift with the
        // time spent polling and logging.
        std::this_thread::sleep_until(objFrameStart_ + objPollPeriod);
    }

    objSource_.close();
    objLogger_.info("Shutdown complete after ", i32PollCount_, " polls; final sample ",
                    objSource_.state().ui64SequenceId_, ".");

    return 0;
}
