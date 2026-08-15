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
    CSdlGamepadSource objSource;

    REQUIRE_FALSE(objSource.connected());
    REQUIRE(objSource.deviceName().empty());
    REQUIRE(objSource.lastError().empty());
    REQUIRE(objSource.state().ui64SequenceId_ == 0U);
    REQUIRE(objSource.state().ui64TimestampNs_ == 0U);
    REQUIRE_THAT(objSource.state().dLeftStickX_, WithinAbs(0.0, 0.0));

    // Polling without an open device reports no data rather than failing.
    REQUIRE_FALSE(objSource.update());
    REQUIRE(objSource.state().ui64SequenceId_ == 0U);
}

TEST_CASE("open reports its outcome consistently with backend availability", "[source][sdl]")
{
    const bool bBackendAvailable = CSdlGamepadSource::isBackendAvailable();

    CSdlGamepadSource objSource;
    const bool bOpened = objSource.open();

    // A build without the backend can never attach, and must say why.
    if (!bBackendAvailable)
    {
        REQUIRE_FALSE(bOpened);
    }

    if (bOpened)
    {
        // Success implies a usable device and a cleared error channel.
        REQUIRE(objSource.connected());
        REQUIRE_FALSE(objSource.deviceName().empty());
        REQUIRE(objSource.lastError().empty());

        // open() publishes a first snapshot so state() is meaningful before the
        // caller reaches its own update() loop.
        REQUIRE(objSource.state().ui64SequenceId_ > 0U);
        REQUIRE(objSource.state().bConnected_);
    }
    else
    {
        // Failure must be explicit rather than silent, whatever the cause.
        REQUIRE_FALSE(objSource.connected());
        REQUIRE_FALSE(objSource.lastError().empty());
        WARN("No controller attached or backend unavailable; hardware path not exercised");
    }
}

TEST_CASE("close is idempotent and safe before any open", "[source][sdl]")
{
    CSdlGamepadSource objSource;

    objSource.close();
    objSource.close();

    REQUIRE_FALSE(objSource.connected());
    REQUIRE(objSource.deviceName().empty());

    // Closing without a prior attachment must not fabricate a state transition.
    REQUIRE(objSource.state().ui64SequenceId_ == 0U);
}

TEST_CASE("an out-of-range joystick index never attaches", "[source][sdl]")
{
    constexpr std::int32_t i32ImplausibleIndex = 9999;

    CSdlGamepadSource objSource;

    REQUIRE_FALSE(objSource.open(i32ImplausibleIndex));
    REQUIRE_FALSE(objSource.connected());
    REQUIRE_FALSE(objSource.lastError().empty());
}

TEST_CASE("closing an attached device publishes the neutral snapshot", "[source][sdl]")
{
    CSdlGamepadSource objSource;

    if (!objSource.open())
    {
        WARN("No controller attached; disconnect-on-close path not exercised");
        return;
    }

    const std::uint64_t ui64SequenceIdWhileOpen = objSource.state().ui64SequenceId_;

    objSource.close();

    // The neutral snapshot is an observable event, so it advances the sequence
    // while leaving a consumer that ignores bConnected_ with a safe command.
    REQUIRE_FALSE(objSource.connected());
    REQUIRE(objSource.deviceName().empty());
    REQUIRE(objSource.state().ui64SequenceId_ == ui64SequenceIdWhileOpen + 1U);
    REQUIRE_THAT(objSource.state().dLeftStickX_, WithinAbs(0.0, 0.0));
    REQUIRE_THAT(objSource.state().dLeftTrigger_, WithinAbs(0.0, 0.0));
    REQUIRE_FALSE(objSource.state().bButtonA_);

    // A closed source reports no data instead of replaying the last sample.
    REQUIRE_FALSE(objSource.update());
}

TEST_CASE("repeated polling advances the sequence and stays in range", "[source][sdl]")
{
    CSdlGamepadSource objSource;

    if (!objSource.open())
    {
        WARN("No controller attached; polling path not exercised");
        return;
    }

    constexpr int i32PollCount = 5;
    std::uint64_t ui64PreviousSequenceId = objSource.state().ui64SequenceId_;

    for (int i32Poll = 0; i32Poll < i32PollCount; ++i32Poll)
    {
        REQUIRE(objSource.update());

        const xbox_controller_api::SGamepadState &strState = objSource.state();

        // Every successful poll is a distinct sample.
        REQUIRE(strState.ui64SequenceId_ == ui64PreviousSequenceId + 1U);
        REQUIRE(strState.ui64TimestampNs_ > 0U);
        ui64PreviousSequenceId = strState.ui64SequenceId_;

        // Normalization must hold for whatever the sticks happen to be doing.
        REQUIRE(strState.dLeftStickX_ >= -1.0);
        REQUIRE(strState.dLeftStickX_ <= 1.0);
        REQUIRE(strState.dLeftStickY_ >= -1.0);
        REQUIRE(strState.dLeftStickY_ <= 1.0);
        REQUIRE(strState.dRightStickX_ >= -1.0);
        REQUIRE(strState.dRightStickX_ <= 1.0);
        REQUIRE(strState.dRightStickY_ >= -1.0);
        REQUIRE(strState.dRightStickY_ <= 1.0);
        REQUIRE(strState.dLeftTrigger_ >= 0.0);
        REQUIRE(strState.dLeftTrigger_ <= 1.0);
        REQUIRE(strState.dRightTrigger_ >= 0.0);
        REQUIRE(strState.dRightTrigger_ <= 1.0);
    }
}
