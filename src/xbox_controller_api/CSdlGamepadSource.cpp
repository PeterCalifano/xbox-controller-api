/**
 * @file CSdlGamepadSource.cpp
 * @brief Implements the SDL2-backed gamepad source and its no-backend stub.
 * @details The file compiles in two shapes selected by __SDL2_ENABLED__, which
 *          the SDL2 interface target defines when the dependency was resolved.
 *          Both shapes define every method, so the class contract is identical
 *          in either configuration and only the outcomes differ.
 */

#include <xbox_controller_api/CSdlGamepadSource.h>

#include <utils/logging/CLogger.h>
#include <xbox_controller_api/GamepadFilters.h>
#include <xbox_controller_api/SGamepadState.h>

#include <chrono>
#include <memory>
#include <string>

#ifdef __SDL2_ENABLED__
#include <SDL.h>
#endif

namespace xbox_controller_api
{
    namespace
    {
        /// Fixed diagnostic reported by open() when the backend was compiled out.
        /// Consumers branch on isBackendAvailable() rather than parsing this.
        constexpr const char *charBackendUnavailableMessage =
            "SDL2 backend is not compiled into this build (ENABLE_SDL2 was OFF at configure time)";

        /// @brief Read the steady clock in nanoseconds for snapshot timestamps.
        [[nodiscard]] std::uint64_t ReadSteadyClockNs() noexcept
        {
            const std::chrono::steady_clock::duration objSinceEpoch_ =
                std::chrono::steady_clock::now().time_since_epoch();
            const auto i64Nanoseconds_ =
                std::chrono::duration_cast<std::chrono::nanoseconds>(objSinceEpoch_).count();

            return static_cast<std::uint64_t>(i64Nanoseconds_);
        }

#ifdef __SDL2_ENABLED__
        /// @brief Read one stick axis and normalize it onto [-1, 1].
        [[nodiscard]] double ReadStickAxis(SDL_GameController &objController,
                                           SDL_GameControllerAxis enumAxis) noexcept
        {
            return NormalizeStickAxis(SDL_GameControllerGetAxis(&objController, enumAxis));
        }

        /// @brief Read one trigger axis and normalize it onto [0, 1].
        [[nodiscard]] double ReadTriggerAxis(SDL_GameController &objController,
                                             SDL_GameControllerAxis enumAxis) noexcept
        {
            return NormalizeTriggerAxis(SDL_GameControllerGetAxis(&objController, enumAxis));
        }

        /// @brief Read one button as a boolean press state.
        [[nodiscard]] bool ReadButton(SDL_GameController &objController,
                                      SDL_GameControllerButton enumButton) noexcept
        {
            return SDL_GameControllerGetButton(&objController, enumButton) != 0;
        }

        /**
         * @brief Select which joystick index to attach to.
         *
         * @param i32RequestedIndex Caller request, negative to apply the default
         *        policy.
         * @return A usable game controller index, or -1 when none qualifies.
         */
        [[nodiscard]] std::int32_t ResolveGameControllerIndex(std::int32_t i32RequestedIndex) noexcept
        {
            const std::int32_t i32DeviceCount_ = SDL_NumJoysticks();

            // Version 1 policy: an unspecified index attaches to the
            // lowest-numbered device SDL recognizes as a game controller, since
            // that is the only kind whose button mapping is reliable. An
            // explicit index is the extension seam for multi-pad setups.
            if (i32RequestedIndex < 0)
            {
                for (std::int32_t i32Index_ = 0; i32Index_ < i32DeviceCount_; ++i32Index_)
                {
                    if (SDL_IsGameController(i32Index_) == SDL_TRUE)
                    {
                        return i32Index_;
                    }
                }

                return -1;
            }

            if (i32RequestedIndex >= i32DeviceCount_ ||
                SDL_IsGameController(i32RequestedIndex) != SDL_TRUE)
            {
                return -1;
            }

            return i32RequestedIndex;
        }

        /**
         * @brief Build a normalized snapshot from the current device state.
         *
         * No deadzone or edge detection is applied: the backend reports what the
         * hardware said, and shaping stays an explicit consumer decision.
         */
        [[nodiscard]] SGamepadState ReadControllerState(SDL_GameController &objController,
                                                        std::uint64_t ui64SequenceId,
                                                        std::uint64_t ui64TimestampNs) noexcept
        {
            SGamepadState strState_;

            // SDL reports stick Y as positive downward. The library contract is
            // up = +1, so the two Y axes, and only those, are inverted here.
            strState_.dLeftStickX_ = ReadStickAxis(objController, SDL_CONTROLLER_AXIS_LEFTX);
            strState_.dLeftStickY_ = InvertAxis(ReadStickAxis(objController, SDL_CONTROLLER_AXIS_LEFTY));
            strState_.dRightStickX_ = ReadStickAxis(objController, SDL_CONTROLLER_AXIS_RIGHTX);
            strState_.dRightStickY_ = InvertAxis(ReadStickAxis(objController, SDL_CONTROLLER_AXIS_RIGHTY));

            strState_.dLeftTrigger_ = ReadTriggerAxis(objController, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
            strState_.dRightTrigger_ = ReadTriggerAxis(objController, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

            strState_.bButtonA_ = ReadButton(objController, SDL_CONTROLLER_BUTTON_A);
            strState_.bButtonB_ = ReadButton(objController, SDL_CONTROLLER_BUTTON_B);
            strState_.bButtonX_ = ReadButton(objController, SDL_CONTROLLER_BUTTON_X);
            strState_.bButtonY_ = ReadButton(objController, SDL_CONTROLLER_BUTTON_Y);

            strState_.bLeftShoulder_ = ReadButton(objController, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            strState_.bRightShoulder_ = ReadButton(objController, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);

            strState_.bLeftStickClick_ = ReadButton(objController, SDL_CONTROLLER_BUTTON_LEFTSTICK);
            strState_.bRightStickClick_ = ReadButton(objController, SDL_CONTROLLER_BUTTON_RIGHTSTICK);

            strState_.bBack_ = ReadButton(objController, SDL_CONTROLLER_BUTTON_BACK);
            strState_.bStart_ = ReadButton(objController, SDL_CONTROLLER_BUTTON_START);
            strState_.bGuide_ = ReadButton(objController, SDL_CONTROLLER_BUTTON_GUIDE);

            strState_.bDpadUp_ = ReadButton(objController, SDL_CONTROLLER_BUTTON_DPAD_UP);
            strState_.bDpadDown_ = ReadButton(objController, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            strState_.bDpadLeft_ = ReadButton(objController, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
            strState_.bDpadRight_ = ReadButton(objController, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);

            strState_.bConnected_ = true;
            strState_.ui64SequenceId_ = ui64SequenceId;
            strState_.ui64TimestampNs_ = ui64TimestampNs;

            return strState_;
        }
#endif // __SDL2_ENABLED__
    } // namespace

    /**
     * @brief Private state, kept out of the header so SDL types never leak.
     *
     * The SDL members exist only in a build that has the backend, which is what
     * allows the stub configuration to compile without SDL headers at all.
     */
    struct CSdlGamepadSource::SImpl
    {
        std::string charDeviceName_;
        std::string charLastError_;
        logging::CLogger objLogger_{"xbox_controller_api.sdl"};

#ifdef __SDL2_ENABLED__
        SDL_GameController *pGameController_ = nullptr;

        /// True while this instance holds one SDL_InitSubSystem reference.
        bool bHoldsSubsystemRef_ = false;

        /// True when this instance was the first to start the subsystem, which
        /// is what makes it safe to set hints and flush device events.
        bool bStartedSubsystem_ = false;
#endif
    };

    CSdlGamepadSource::CSdlGamepadSource() : pImpl_(std::make_unique<SImpl>())
    {
        // Deliberately free of SDL calls: initialization is deferred to open()
        // so constructing a source in a headless process is always safe.
        pImpl_->objLogger_.setLevelFromEnvironment();
    }

    CSdlGamepadSource::~CSdlGamepadSource()
    {
        close();
    }

    bool CSdlGamepadSource::isBackendAvailable() noexcept
    {
#ifdef __SDL2_ENABLED__
        return true;
#else
        return false;
#endif
    }

    const std::string &CSdlGamepadSource::deviceName() const noexcept
    {
        return pImpl_->charDeviceName_;
    }

    const std::string &CSdlGamepadSource::lastError() const noexcept
    {
        return pImpl_->charLastError_;
    }

    bool CSdlGamepadSource::open(std::int32_t i32JoystickIndex)
    {
        // open() is also the documented reconnection path, so release any
        // previous device first instead of leaking it.
        close();
        pImpl_->charLastError_.clear();

#ifndef __SDL2_ENABLED__
        (void)i32JoystickIndex;

        pImpl_->charLastError_ = charBackendUnavailableMessage;
        pImpl_->objLogger_.error(charBackendUnavailableMessage);

        return false;
#else
        // SDL_InitSubSystem is reference counted. Record whether this call is
        // the first initializer, because hints only take effect before the
        // subsystem starts and must not override a host application's setup.
        const bool bIsFirstInitializer_ = (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0U);

        if (bIsFirstInitializer_)
        {
            SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        }

        if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0)
        {
            pImpl_->charLastError_ = std::string("SDL_InitSubSystem failed: ") + SDL_GetError();
            pImpl_->objLogger_.error(pImpl_->charLastError_);

            return false;
        }

        pImpl_->bHoldsSubsystemRef_ = true;
        pImpl_->bStartedSubsystem_ = bIsFirstInitializer_;

        // Refresh the device list without draining the event queue.
        SDL_GameControllerUpdate();

        const std::int32_t i32ResolvedIndex_ = ResolveGameControllerIndex(i32JoystickIndex);

        if (i32ResolvedIndex_ < 0)
        {
            pImpl_->charLastError_ =
                "No SDL game controller is available at the requested joystick index";
            pImpl_->objLogger_.error(pImpl_->charLastError_);
            close();

            return false;
        }

        SDL_GameController *pGameController_ = SDL_GameControllerOpen(i32ResolvedIndex_);

        if (pGameController_ == nullptr)
        {
            pImpl_->charLastError_ = std::string("SDL_GameControllerOpen failed: ") + SDL_GetError();
            pImpl_->objLogger_.error(pImpl_->charLastError_);
            close();

            return false;
        }

        pImpl_->pGameController_ = pGameController_;

        const char *charDeviceName_ = SDL_GameControllerName(pGameController_);
        pImpl_->charDeviceName_ = (charDeviceName_ != nullptr) ? charDeviceName_ : "Unknown controller";

        // This instance never consumes the event queue, so discard the
        // device-added events our own initialization generated. Doing so is only
        // safe when nobody else owns the queue.
        if (pImpl_->bStartedSubsystem_)
        {
            SDL_FlushEvent(SDL_JOYDEVICEADDED);
            SDL_FlushEvent(SDL_CONTROLLERDEVICEADDED);
        }

        // Publish a first snapshot so state() is meaningful before the caller
        // reaches its first update().
        setState(ReadControllerState(*pGameController_, state().ui64SequenceId_ + 1U,
                                     ReadSteadyClockNs()));

        pImpl_->objLogger_.info("Opened controller '", pImpl_->charDeviceName_, "' at joystick index ",
                                i32ResolvedIndex_);

        return true;
#endif
    }

    void CSdlGamepadSource::close() noexcept
    {
#ifdef __SDL2_ENABLED__
        if (pImpl_->pGameController_ != nullptr)
        {
            SDL_GameControllerClose(pImpl_->pGameController_);
            pImpl_->pGameController_ = nullptr;
        }

        // Balance every successful SDL_InitSubSystem exactly once, so a host
        // application's own reference count survives this teardown.
        if (pImpl_->bHoldsSubsystemRef_)
        {
            SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
            pImpl_->bHoldsSubsystemRef_ = false;
        }

        pImpl_->bStartedSubsystem_ = false;
#endif

        pImpl_->charDeviceName_.clear();

        // Publish the neutral snapshot only when something was attached, which
        // keeps repeated close() calls observably identical.
        if (connected())
        {
            setState(MakeDisconnectedState(state().ui64SequenceId_ + 1U, ReadSteadyClockNs()));
        }
    }

    bool CSdlGamepadSource::update()
    {
#ifndef __SDL2_ENABLED__
        return false;
#else
        if (pImpl_->pGameController_ == nullptr)
        {
            return false;
        }

        // Poll rather than pump: SDL_GameControllerUpdate refreshes device state
        // without consuming events that a host application may own.
        SDL_GameControllerUpdate();

        // A detach is reported once and then left to the caller, since recovery
        // requires an explicit open(). Publishing and logging only on the
        // transition also keeps a disconnected poll loop free of per-frame spam.
        if (SDL_GameControllerGetAttached(pImpl_->pGameController_) != SDL_TRUE)
        {
            if (connected())
            {
                setState(MakeDisconnectedState(state().ui64SequenceId_ + 1U, ReadSteadyClockNs()));
                pImpl_->objLogger_.warning("Controller '", pImpl_->charDeviceName_,
                                           "' was detached; call open() to reattach");
            }

            return false;
        }

        setState(ReadControllerState(*pImpl_->pGameController_, state().ui64SequenceId_ + 1U,
                                     ReadSteadyClockNs()));

        return true;
#endif
    }
} // namespace xbox_controller_api
