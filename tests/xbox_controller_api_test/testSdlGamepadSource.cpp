/**
 * @file testSdlGamepadSource.cpp
 * @brief Invariant checks for the SDL2 backend that hold in every build.
 * @details These cases must pass with the backend compiled in or out, and with
 *          or without a controller attached, so none of them asserts a specific
 *          open() outcome. What they pin down is the relationship between
 *          isBackendAvailable(), open(), lastError(), and the published
 *          snapshot, which is the part a consumer actually reasons about.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <xbox_controller_api/CSdlGamepadSource.h>
#include <xbox_controller_api/SGamepadState.h>

#include <cstdint>

using Catch::Matchers::WithinAbs;
using xbox_controller_api::CSdlGamepadSource;

TEST_CASE("a fresh SDL source touches no device", "[source][sdl]")
{
    // Construction must stay free of SDL calls, so this case is meaningful even
    // in a headless environment with no input subsystem at all.
    CSdlGamepadSource objSource_;

    REQUIRE_FALSE(objSource_.connected());
    REQUIRE(objSource_.deviceName().empty());
    REQUIRE(objSource_.lastError().empty());
    REQUIRE(objSource_.state().ui64SequenceId_ == 0U);
    REQUIRE(objSource_.state().ui64TimestampNs_ == 0U);
    REQUIRE_THAT(objSource_.state().dLeftStickX_, WithinAbs(0.0, 0.0));

    // Polling without an open device reports no data rather than failing.
    REQUIRE_FALSE(objSource_.update());
    REQUIRE(objSource_.state().ui64SequenceId_ == 0U);
}

TEST_CASE("open reports its outcome consistently with backend availability", "[source][sdl]")
{
    const bool bBackendAvailable_ = CSdlGamepadSource::isBackendAvailable();

    CSdlGamepadSource objSource_;
    const bool bOpened_ = objSource_.open();

    // A build without the backend can never attach, and must say why.
    if (!bBackendAvailable_)
    {
        REQUIRE_FALSE(bOpened_);
    }

    if (bOpened_)
    {
        // Success implies a usable device and a cleared error channel.
        REQUIRE(objSource_.connected());
        REQUIRE_FALSE(objSource_.deviceName().empty());
        REQUIRE(objSource_.lastError().empty());

        // open() publishes a first snapshot so state() is meaningful before the
        // caller reaches its own update() loop.
        REQUIRE(objSource_.state().ui64SequenceId_ > 0U);
        REQUIRE(objSource_.state().bConnected_);
    }
    else
    {
        // Failure must be explicit rather than silent, whatever the cause.
        REQUIRE_FALSE(objSource_.connected());
        REQUIRE_FALSE(objSource_.lastError().empty());
        WARN("No controller attached or backend unavailable; hardware path not exercised");
    }
}

TEST_CASE("close is idempotent and safe before any open", "[source][sdl]")
{
    CSdlGamepadSource objSource_;

    objSource_.close();
    objSource_.close();

    REQUIRE_FALSE(objSource_.connected());
    REQUIRE(objSource_.deviceName().empty());

    // Closing without a prior attachment must not fabricate a state transition.
    REQUIRE(objSource_.state().ui64SequenceId_ == 0U);
}

TEST_CASE("an out-of-range joystick index never attaches", "[source][sdl]")
{
    constexpr std::int32_t i32ImplausibleIndex_ = 9999;

    CSdlGamepadSource objSource_;

    REQUIRE_FALSE(objSource_.open(i32ImplausibleIndex_));
    REQUIRE_FALSE(objSource_.connected());
    REQUIRE_FALSE(objSource_.lastError().empty());
}

TEST_CASE("closing an attached device publishes the neutral snapshot", "[source][sdl]")
{
    CSdlGamepadSource objSource_;

    if (!objSource_.open())
    {
        WARN("No controller attached; disconnect-on-close path not exercised");
        return;
    }

    const std::uint64_t ui64SequenceIdWhileOpen_ = objSource_.state().ui64SequenceId_;

    objSource_.close();

    // The neutral snapshot is an observable event, so it advances the sequence
    // while leaving a consumer that ignores bConnected_ with a safe command.
    REQUIRE_FALSE(objSource_.connected());
    REQUIRE(objSource_.deviceName().empty());
    REQUIRE(objSource_.state().ui64SequenceId_ == ui64SequenceIdWhileOpen_ + 1U);
    REQUIRE_THAT(objSource_.state().dLeftStickX_, WithinAbs(0.0, 0.0));
    REQUIRE_THAT(objSource_.state().dLeftTrigger_, WithinAbs(0.0, 0.0));
    REQUIRE_FALSE(objSource_.state().bButtonA_);

    // A closed source reports no data instead of replaying the last sample.
    REQUIRE_FALSE(objSource_.update());
}

TEST_CASE("repeated polling advances the sequence and stays in range", "[source][sdl]")
{
    CSdlGamepadSource objSource_;

    if (!objSource_.open())
    {
        WARN("No controller attached; polling path not exercised");
        return;
    }

    constexpr int i32PollCount_ = 5;
    std::uint64_t ui64PreviousSequenceId_ = objSource_.state().ui64SequenceId_;

    for (int i32Poll_ = 0; i32Poll_ < i32PollCount_; ++i32Poll_)
    {
        REQUIRE(objSource_.update());

        const xbox_controller_api::SGamepadState &strState_ = objSource_.state();

        // Every successful poll is a distinct sample.
        REQUIRE(strState_.ui64SequenceId_ == ui64PreviousSequenceId_ + 1U);
        REQUIRE(strState_.ui64TimestampNs_ > 0U);
        ui64PreviousSequenceId_ = strState_.ui64SequenceId_;

        // Normalization must hold for whatever the sticks happen to be doing.
        REQUIRE(strState_.dLeftStickX_ >= -1.0);
        REQUIRE(strState_.dLeftStickX_ <= 1.0);
        REQUIRE(strState_.dLeftStickY_ >= -1.0);
        REQUIRE(strState_.dLeftStickY_ <= 1.0);
        REQUIRE(strState_.dRightStickX_ >= -1.0);
        REQUIRE(strState_.dRightStickX_ <= 1.0);
        REQUIRE(strState_.dRightStickY_ >= -1.0);
        REQUIRE(strState_.dRightStickY_ <= 1.0);
        REQUIRE(strState_.dLeftTrigger_ >= 0.0);
        REQUIRE(strState_.dLeftTrigger_ <= 1.0);
        REQUIRE(strState_.dRightTrigger_ >= 0.0);
        REQUIRE(strState_.dRightTrigger_ <= 1.0);
    }
}
