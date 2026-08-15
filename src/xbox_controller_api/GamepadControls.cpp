/**
 * @file GamepadControls.cpp
 * @brief Implements the button identity table and snapshot-level operations.
 * @details The binding table below is the single authority pairing a control
 *          identity with its snapshot member and its name. Everything else in
 *          this file, and every consumer that iterates controls, derives from
 *          it, so adding a control means adding one row.
 */

#include <xbox_controller_api/GamepadControls.h>

#include <array>
#include <cstddef>

namespace xbox_controller_api
{
    namespace
    {
        /// @brief One control identity paired with its storage and its name.
        struct SButtonBinding
        {
            EGamepadButton enumButton_;
            bool SGamepadState::*pPressedMember_;
            std::string_view charName_;
        };

        constexpr SButtonBinding arrButtonBindings[] = {
            {EGamepadButton::A, &SGamepadState::bButtonA_, "A"},
            {EGamepadButton::B, &SGamepadState::bButtonB_, "B"},
            {EGamepadButton::X, &SGamepadState::bButtonX_, "X"},
            {EGamepadButton::Y, &SGamepadState::bButtonY_, "Y"},
            {EGamepadButton::LeftShoulder, &SGamepadState::bLeftShoulder_, "LB"},
            {EGamepadButton::RightShoulder, &SGamepadState::bRightShoulder_, "RB"},
            {EGamepadButton::LeftStickClick, &SGamepadState::bLeftStickClick_, "LS"},
            {EGamepadButton::RightStickClick, &SGamepadState::bRightStickClick_, "RS"},
            {EGamepadButton::Back, &SGamepadState::bBack_, "Back"},
            {EGamepadButton::Start, &SGamepadState::bStart_, "Start"},
            {EGamepadButton::Guide, &SGamepadState::bGuide_, "Guide"},
            {EGamepadButton::DpadUp, &SGamepadState::bDpadUp_, "DpadUp"},
            {EGamepadButton::DpadDown, &SGamepadState::bDpadDown_, "DpadDown"},
            {EGamepadButton::DpadLeft, &SGamepadState::bDpadLeft_, "DpadLeft"},
            {EGamepadButton::DpadRight, &SGamepadState::bDpadRight_, "DpadRight"}};

        constexpr std::size_t szButtonCount = std::size(arrButtonBindings);

        /// @brief Verify at compile time that the table is indexable by identity.
        constexpr bool AreBindingsInEnumerationOrder() noexcept
        {
            for (std::size_t szIndex_ = 0; szIndex_ < szButtonCount; ++szIndex_)
            {
                if (arrButtonBindings[szIndex_].enumButton_ !=
                    static_cast<EGamepadButton>(szIndex_))
                {
                    return false;
                }
            }

            return true;
        }

        // Lookup below indexes the table directly by identity, so a row typed in
        // the wrong position must be a build failure rather than a silent
        // mislabelled control.
        static_assert(AreBindingsInEnumerationOrder(),
                      "arrButtonBindings must be ordered exactly as EGamepadButton");

        static_assert(szButtonCount == 15U,
                      "The binding table must cover every EGamepadButton value");

        /// @brief Build the identity list from the single authoritative table.
        constexpr std::array<EGamepadButton, szButtonCount> MakeAllButtons() noexcept
        {
            std::array<EGamepadButton, szButtonCount> arrButtons_{};
            for (std::size_t szIndex_ = 0; szIndex_ < szButtonCount; ++szIndex_)
            {
                arrButtons_[szIndex_] = arrButtonBindings[szIndex_].enumButton_;
            }

            return arrButtons_;
        }

        constexpr std::array<EGamepadButton, szButtonCount> arrAllButtons = MakeAllButtons();

        /// @brief Report whether an identity can index the binding table.
        [[nodiscard]] constexpr bool IsKnownButton(EGamepadButton enumButton) noexcept
        {
            return static_cast<std::size_t>(enumButton) < szButtonCount;
        }
    } // namespace

    std::span<const EGamepadButton> AllGamepadButtons() noexcept
    {
        return std::span<const EGamepadButton>(arrAllButtons);
    }

    bool GetButton(const SGamepadState &strGamepadState, EGamepadButton enumButton) noexcept
    {
        // An identity arriving from a binding or a file can be out of range, so
        // this reports "not pressed" rather than reading past the table.
        if (!IsKnownButton(enumButton))
        {
            return false;
        }

        const SButtonBinding &strBinding_ =
            arrButtonBindings[static_cast<std::size_t>(enumButton)];

        return strGamepadState.*(strBinding_.pPressedMember_);
    }

    std::string_view GetGamepadButtonName(EGamepadButton enumButton) noexcept
    {
        if (!IsKnownButton(enumButton))
        {
            return "Unknown";
        }

        return arrButtonBindings[static_cast<std::size_t>(enumButton)].charName_;
    }

    EButtonEdge ClassifyButtonEdge(const SGamepadState &strPreviousState,
                                   const SGamepadState &strCurrentState,
                                   EGamepadButton enumButton) noexcept
    {
        return ClassifyButtonEdge(GetButton(strPreviousState, enumButton),
                                  GetButton(strCurrentState, enumButton));
    }

    SGamepadState ApplyStickDeadzone(const SGamepadState &strGamepadState,
                                     double dStickDeadzone) noexcept
    {
        // Copy first so counters, buttons, and triggers pass through untouched
        // and any field added to the aggregate later is preserved by default.
        SGamepadState strConditionedState_ = strGamepadState;

        strConditionedState_.dLeftStickX_ =
            ApplyRescaledDeadzone(strGamepadState.dLeftStickX_, dStickDeadzone);
        strConditionedState_.dLeftStickY_ =
            ApplyRescaledDeadzone(strGamepadState.dLeftStickY_, dStickDeadzone);
        strConditionedState_.dRightStickX_ =
            ApplyRescaledDeadzone(strGamepadState.dRightStickX_, dStickDeadzone);
        strConditionedState_.dRightStickY_ =
            ApplyRescaledDeadzone(strGamepadState.dRightStickY_, dStickDeadzone);

        return strConditionedState_;
    }
} // namespace xbox_controller_api
