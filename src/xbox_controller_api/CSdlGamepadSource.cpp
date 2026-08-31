/**
 * @file CSdlGamepadSource.cpp
 * @brief Implements the SDL2-backed gamepad source and its no-backend stub.
 * @details __SDL2_ENABLED__ selects either the SDL2 implementation or the
 *          no-backend stub. Both provide the same public methods.
 */

#include <xbox_controller_api/CSdlGamepadSource.h>

#include <utils/logging/CLogger.h>
#include <xbox_controller_api/GamepadFilters.h>
#include <xbox_controller_api/SGamepadState.h>

#include <charconv>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#if defined(__linux__)
#include <unistd.h>
#endif

#ifdef __SDL2_ENABLED__
#include <SDL.h>
#endif

namespace xbox_controller_api
{
    namespace
    {
#ifndef __SDL2_ENABLED__
        /// @brief Diagnostic reported when this build has no SDL2 backend.
        constexpr const char *charBackendUnavailableMessage =
            XBOX_CONTROLLER_API_SDL2_UNAVAILABLE_REASON;
#endif

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

        /// @brief Selected joystick index and the reason selection failed.
        struct SIndexResolution
        {
            std::int32_t i32Index_ = -1;
            std::string charDiagnostic_;
            bool bNoDevicesVisible_ = false;
        };

#if defined(__linux__)
        /// @brief Whether automatic legacy-device fallback is enabled.
        [[nodiscard]] bool IsLegacyFallbackEnabled()
        {
            const char *charDisabled_ = std::getenv(
                "XBOX_CONTROLLER_API_DISABLE_LEGACY_FALLBACK");
            if (charDisabled_ == nullptr)
            {
                return true;
            }

            const std::string_view charDisabledValue_(charDisabled_);
            return charDisabledValue_ != "1" && charDisabledValue_ != "true";
        }

        /// @brief Parse the numeric suffix from a Linux js* device name.
        [[nodiscard]] std::optional<std::uint32_t> ParseLegacyJoystickIndex(
            const std::string_view charName)
        {
            constexpr std::string_view charPrefix_ = "js";
            if (!charName.starts_with(charPrefix_) || charName.size() == charPrefix_.size())
            {
                return std::nullopt;
            }

            std::uint32_t ui32Index_ = 0;
            const char *charBegin_ = charName.data() + charPrefix_.size();
            const char *charEnd_ = charName.data() + charName.size();
            const auto [charParsedEnd_, objError_] =
                std::from_chars(charBegin_, charEnd_, ui32Index_);

            if (objError_ != std::errc{} || charParsedEnd_ != charEnd_)
            {
                return std::nullopt;
            }

            return ui32Index_;
        }

        /// @brief Find the lowest-numbered readable legacy joystick device node.
        [[nodiscard]] std::optional<std::string> FindReadableLegacyJoystick()
        {
            const std::filesystem::path objInputRoot_("/dev/input");
            std::error_code objError_;

            if (!std::filesystem::is_directory(objInputRoot_, objError_))
            {
                return std::nullopt;
            }

            // Select in one pass so discovery uses constant extra memory while
            // retaining the natural js0, js1, ... device ordering.
            std::optional<std::uint32_t> ui32SelectedIndex_;
            std::filesystem::path objSelectedPath_;
            std::filesystem::directory_iterator objIterator_(objInputRoot_, objError_);
            const std::filesystem::directory_iterator objEnd_;

            while (!objError_ && objIterator_ != objEnd_)
            {
                const std::filesystem::path objCandidatePath_ = objIterator_->path();
                const std::optional<std::uint32_t> ui32CandidateIndex_ =
                    ParseLegacyJoystickIndex(objCandidatePath_.filename().string());

                if (ui32CandidateIndex_ &&
                    (!ui32SelectedIndex_ || *ui32CandidateIndex_ < *ui32SelectedIndex_) &&
                    ::access(objCandidatePath_.c_str(), R_OK) == 0)
                {
                    ui32SelectedIndex_ = ui32CandidateIndex_;
                    objSelectedPath_ = objCandidatePath_;
                }

                objIterator_.increment(objError_);
            }

            if (objError_ || !ui32SelectedIndex_)
            {
                return std::nullopt;
            }

            return objSelectedPath_.string();
        }
#endif

        /// @brief Name one joystick index as SDL reports it, never returning null.
        [[nodiscard]] std::string DescribeJoystick(std::int32_t i32Index)
        {
            const char *charName_ = SDL_JoystickNameForIndex(i32Index);

            return std::string("'") + ((charName_ != nullptr) ? charName_ : "unnamed device") + "'";
        }

        /// @brief List every joystick SDL can see, for inclusion in a diagnostic.
        [[nodiscard]] std::string DescribeVisibleJoysticks(std::int32_t i32DeviceCount)
        {
            std::string charDescription_;

            for (std::int32_t i32Index_ = 0; i32Index_ < i32DeviceCount; ++i32Index_)
            {
                if (!charDescription_.empty())
                {
                    charDescription_ += ", ";
                }

                charDescription_ += DescribeJoystick(i32Index_);
            }

            return charDescription_;
        }

        /**
         * @brief Select which joystick index to attach to.
         *
         * @param i32RequestedIndex Caller request, negative to apply the default
         *        policy.
         * @return The chosen index, or a negative index plus the reason no
         *         device qualified.
         */
        [[nodiscard]] SIndexResolution ResolveGameControllerIndex(std::int32_t i32RequestedIndex)
        {
            const std::int32_t i32DeviceCount_ = SDL_NumJoysticks();

            if (i32DeviceCount_ < 0)
            {
                return {-1,
                        std::string("SDL_NumJoysticks failed: ") + SDL_GetError(),
                        false};
            }

            // An empty evdev list can be a permissions issue even when the
            // narrower legacy device node remains readable.
            if (i32DeviceCount_ == 0)
            {
                return {-1,
                        "SDL reports no input devices (SDL_NumJoysticks() == 0). SDL enumerates "
                        "through evdev: check /dev/input/event* is readable by this user",
                        true};
            }

            // No index requested: take the first device carrying a mapping.
            if (i32RequestedIndex < 0)
            {
                for (std::int32_t i32Index_ = 0; i32Index_ < i32DeviceCount_; ++i32Index_)
                {
                    if (SDL_IsGameController(i32Index_) == SDL_TRUE)
                    {
                        return {i32Index_, {}, false};
                    }
                }

                return {-1,
                        "SDL reports " + std::to_string(i32DeviceCount_) +
                            " device(s), none with a game controller mapping: " +
                            DescribeVisibleJoysticks(i32DeviceCount_) +
                            ". Set SDL_GAMECONTROLLERCONFIG to add one",
                        false};
            }

            if (i32RequestedIndex >= i32DeviceCount_)
            {
                return {-1,
                        "Joystick index " + std::to_string(i32RequestedIndex) +
                            " is out of range: SDL reports " + std::to_string(i32DeviceCount_) +
                            " device(s): " + DescribeVisibleJoysticks(i32DeviceCount_),
                        false};
            }

            if (SDL_IsGameController(i32RequestedIndex) != SDL_TRUE)
            {
                return {-1,
                        "Joystick index " + std::to_string(i32RequestedIndex) + " (" +
                            DescribeJoystick(i32RequestedIndex) +
                            ") has no game controller mapping. Set SDL_GAMECONTROLLERCONFIG, or "
                            "request no index to take the first mapped device",
                        false};
            }

            return {i32RequestedIndex, {}, false};
        }

        /**
         * @brief Build a normalized snapshot from the current device state.
         *
         * The backend reports normalized hardware values without conditioning.
         */
        [[nodiscard]] SGamepadState ReadControllerState(SDL_GameController &objController,
                                                        std::uint64_t ui64SequenceId,
                                                        std::uint64_t ui64TimestampNs) noexcept
        {
            SGamepadState strState_;

            // Convert SDL's down-positive Y axes to the library's up-positive convention.
            strState_.dLeftStickX_ = ReadStickAxis(objController, SDL_CONTROLLER_AXIS_LEFTX);
            strState_.dLeftStickY_ = InvertAxis(ReadStickAxis(objController,
                                                             SDL_CONTROLLER_AXIS_LEFTY));
            strState_.dRightStickX_ = ReadStickAxis(objController, SDL_CONTROLLER_AXIS_RIGHTX);
            strState_.dRightStickY_ = InvertAxis(ReadStickAxis(objController,
                                                              SDL_CONTROLLER_AXIS_RIGHTY));

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
     * SDL members exist only when the backend is enabled.
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
#endif
    };

    CSdlGamepadSource::CSdlGamepadSource() : pImpl_(std::make_unique<SImpl>())
    {
        // Defer SDL initialization until a controller is opened.
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
        // Reopening replaces any previously attached controller.
        close();
        pImpl_->charLastError_.clear();

#ifndef __SDL2_ENABLED__
        (void)i32JoystickIndex;

        pImpl_->charLastError_ = charBackendUnavailableMessage;
        pImpl_->objLogger_.error(charBackendUnavailableMessage);

        return false;
#else
        // Set hints only when this instance initializes the subsystem first.
        const bool bIsFirstJoystickInitializer_ = (SDL_WasInit(SDL_INIT_JOYSTICK) == 0U);
        const bool bIsFirstControllerInitializer_ =
            (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0U);

        if (bIsFirstControllerInitializer_)
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

        // Refresh the device list without draining the event queue.
        SDL_GameControllerUpdate();

        SIndexResolution strResolution_ = ResolveGameControllerIndex(i32JoystickIndex);

#if defined(__linux__)
        // SDL normally enumerates through evdev. In containers and non-seat
        // sessions that node is commonly blocked while the device-specific
        // legacy js* node remains readable. Retry through that narrower node
        // without changing permissions or installing host udev rules.
        const bool bShouldTryLegacyFallback_ =
            strResolution_.bNoDevicesVisible_ && IsLegacyFallbackEnabled() &&
            SDL_GetHint(SDL_HINT_JOYSTICK_DEVICE) == nullptr;

        if (bShouldTryLegacyFallback_ && !bIsFirstJoystickInitializer_)
        {
            strResolution_.charDiagnostic_ +=
                ". SDL's joystick subsystem was already initialized; set "
                "SDL_JOYSTICK_DEVICE before its first initialization";
        }
        else if (bShouldTryLegacyFallback_)
        {
            const std::optional<std::string> charFallbackDevice_ =
                FindReadableLegacyJoystick();

            if (charFallbackDevice_)
            {
                close();

                // The Linux SDL driver reads this hint during initialization.
                // Reset it immediately afterwards to avoid changing process
                // policy for unrelated SDL consumers.
                if (SDL_SetHint(SDL_HINT_JOYSTICK_DEVICE,
                                charFallbackDevice_->c_str()) != SDL_TRUE)
                {
                    pImpl_->charLastError_ =
                        "SDL refused the temporary legacy joystick device hint";
                    pImpl_->objLogger_.error(pImpl_->charLastError_);

                    return false;
                }

                const std::int32_t i32FallbackInitResult_ =
                    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
                const std::string charFallbackInitError_ = SDL_GetError();
                SDL_ResetHint(SDL_HINT_JOYSTICK_DEVICE);

                if (i32FallbackInitResult_ != 0)
                {
                    pImpl_->charLastError_ =
                        "SDL fallback initialization failed: " + charFallbackInitError_;
                    pImpl_->objLogger_.error(pImpl_->charLastError_);

                    return false;
                }

                pImpl_->bHoldsSubsystemRef_ = true;
                SDL_GameControllerUpdate();
                strResolution_ = ResolveGameControllerIndex(i32JoystickIndex);

                if (strResolution_.i32Index_ >= 0)
                {
                    pImpl_->objLogger_.warning(
                        "SDL evdev enumeration found no controllers; using readable legacy device ",
                        *charFallbackDevice_);
                }
                else
                {
                    strResolution_.charDiagnostic_ =
                        "SDL legacy retry through '" + *charFallbackDevice_ +
                        "' failed: " + strResolution_.charDiagnostic_;
                }
            }
            else
            {
                strResolution_.charDiagnostic_ +=
                    "; no readable /dev/input/jsN fallback device was found";
            }
        }
#endif

        if (strResolution_.i32Index_ < 0)
        {
            // Report the specific reason selection failed, not a generic one.
            pImpl_->charLastError_ = strResolution_.charDiagnostic_;
            pImpl_->objLogger_.error(pImpl_->charLastError_);
            close();

            return false;
        }

        const std::int32_t i32ResolvedIndex_ = strResolution_.i32Index_;
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

        // Publish the initial controller state.
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

        // Balance this instance's SDL_InitSubSystem call.
        if (pImpl_->bHoldsSubsystemRef_)
        {
            SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
            pImpl_->bHoldsSubsystemRef_ = false;
        }
#endif

        pImpl_->charDeviceName_.clear();

        // Publish a neutral snapshot after disconnecting an attached controller.
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

        // Refresh state without consuming the host application's event queue.
        SDL_GameControllerUpdate();

        // Publish and log the disconnect once; reopening is caller-controlled.
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
