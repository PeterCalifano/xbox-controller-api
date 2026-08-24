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

#ifdef __SDL2_ENABLED__
#include <SDL.h>
#endif

#include <cstdint>

using Catch::Matchers::WithinAbs;
using xbox_controller_api::CSdlGamepadSource;

#ifdef __SDL2_ENABLED__
#if SDL_VERSION_ATLEAST(2, 0, 14)
namespace
{
    /// @brief Own SDL test state so failing assertions still release it.
    struct SSdlVirtualControllerGuard
    {
        int i32DeviceIndex_ = -1;
        bool bHoldsJoystickSubsystemRef_ = false;

        ~SSdlVirtualControllerGuard()
        {
            if (i32DeviceIndex_ >= 0)
            {
                (void)SDL_JoystickDetachVirtual(i32DeviceIndex_);
            }
            if (bHoldsJoystickSubsystemRef_)
            {
                SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
            }
        }
    };

    /// @brief Attach a GameController-shaped virtual joystick for queue tests.
    [[nodiscard]] int AttachVirtualGameController()
    {
#if SDL_VERSION_ATLEAST(2, 24, 0)
        SDL_VirtualJoystickDesc strDescriptor_{};
        strDescriptor_.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
        strDescriptor_.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
        strDescriptor_.naxes = static_cast<Uint16>(SDL_CONTROLLER_AXIS_MAX);
        strDescriptor_.nbuttons = static_cast<Uint16>(SDL_CONTROLLER_BUTTON_MAX);
        strDescriptor_.axis_mask = (1U << SDL_CONTROLLER_AXIS_MAX) - 1U;
        strDescriptor_.button_mask = (1U << SDL_CONTROLLER_BUTTON_MAX) - 1U;
        strDescriptor_.name = "xbox_controller_api event queue test";

        return SDL_JoystickAttachVirtualEx(&strDescriptor_);
#else
        return SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                         SDL_CONTROLLER_AXIS_MAX,
                                         SDL_CONTROLLER_BUTTON_MAX, 0);
#endif
    }
} // namespace
#endif
#endif

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

#ifdef __SDL2_ENABLED__
#if SDL_VERSION_ATLEAST(2, 0, 14)
TEST_CASE("polling preserves the host SDL event queue", "[source][sdl][events]")
{
    // Initialize only the joystick dependency so the source remains the first
    // owner of the game-controller subsystem, which is the regression path.
    REQUIRE(SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0U);

    SSdlVirtualControllerGuard strGuard_;
    REQUIRE(SDL_InitSubSystem(SDL_INIT_JOYSTICK) == 0);
    strGuard_.bHoldsJoystickSubsystemRef_ = true;

    strGuard_.i32DeviceIndex_ = AttachVirtualGameController();
    REQUIRE(strGuard_.i32DeviceIndex_ >= 0);

    // Queue one identifiable host event before the source initializes and
    // polls the controller. Neither operation may remove it.
    SDL_FlushEvent(SDL_JOYAXISMOTION);
    SDL_Event objPendingEvent_{};
    objPendingEvent_.type = SDL_JOYAXISMOTION;
    objPendingEvent_.jaxis.which = 0x1234;
    objPendingEvent_.jaxis.axis = 2;
    objPendingEvent_.jaxis.value = 12345;
    REQUIRE(SDL_PushEvent(&objPendingEvent_) == 1);

    CSdlGamepadSource objSource_;
    REQUIRE(objSource_.open(strGuard_.i32DeviceIndex_));
    REQUIRE(objSource_.update());

    SDL_Event objObservedEvent_{};
    REQUIRE(SDL_PeepEvents(&objObservedEvent_, 1, SDL_GETEVENT,
                           SDL_JOYAXISMOTION, SDL_JOYAXISMOTION) == 1);
    REQUIRE(objObservedEvent_.jaxis.which == objPendingEvent_.jaxis.which);
    REQUIRE(objObservedEvent_.jaxis.axis == objPendingEvent_.jaxis.axis);
    REQUIRE(objObservedEvent_.jaxis.value == objPendingEvent_.jaxis.value);
}
#endif
#endif
