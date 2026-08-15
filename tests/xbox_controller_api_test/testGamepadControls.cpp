/**
 * @file testGamepadControls.cpp
 * @brief Verifies the button identity mapping and snapshot-level operations.
 * @details The expected identity-to-member mapping is restated here rather than
 *          imported from the library. A test that reused the production table
 *          could only prove the table equals itself; restating it is what makes
 *          a mis-wired control a test failure.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <xbox_controller_api/GamepadControls.h>
#include <xbox_controller_api/GamepadFilters.h>
#include <xbox_controller_api/SGamepadState.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

using Catch::Matchers::WithinAbs;
using xbox_controller_api::AllGamepadButtons;
using xbox_controller_api::ApplyStickDeadzone;
using xbox_controller_api::ClassifyButtonEdge;
using xbox_controller_api::EButtonEdge;
using xbox_controller_api::EGamepadButton;
using xbox_controller_api::GetButton;
using xbox_controller_api::GetGamepadButtonName;
using xbox_controller_api::SGamepadState;

namespace
{
    /// Tolerance for values that must be reproduced bit-exactly.
    constexpr double dExactTolerance = 0.0;

    /// @brief Independently restated expectation for one control identity.
    struct SExpectedBinding
    {
        EGamepadButton enumButton_;
        bool SGamepadState::*pPressedMember_;
        std::string_view charName_;
    };

    constexpr SExpectedBinding arrExpectedBindings[] = {
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

    /// An identity no build should ever define, used to probe range handling.
    constexpr EGamepadButton enumOutOfRangeButton = static_cast<EGamepadButton>(200);
} // namespace

TEST_CASE("the identity list covers every control in enumeration order", "[controls]")
{
    const std::span<const EGamepadButton> spanButtons = AllGamepadButtons();

    REQUIRE(spanButtons.size() == std::size(arrExpectedBindings));

    for (std::size_t szIndex = 0; szIndex < spanButtons.size(); ++szIndex)
    {
        REQUIRE(spanButtons[szIndex] == arrExpectedBindings[szIndex].enumButton_);

        // Contiguity from zero is part of the contract, since consumers use the
        // identity as a stable index.
        REQUIRE(static_cast<std::size_t>(spanButtons[szIndex]) == szIndex);
    }
}

TEST_CASE("each identity reads exactly its own snapshot member", "[controls]")
{
    // Press one control at a time and require that precisely one identity
    // observes it. This is what catches a member pointer wired to the wrong row.
    for (const SExpectedBinding &strPressedBinding : arrExpectedBindings)
    {
        SGamepadState strState;
        strState.*(strPressedBinding.pPressedMember_) = true;

        for (const SExpectedBinding &strProbedBinding : arrExpectedBindings)
        {
            const bool bExpected = (strProbedBinding.enumButton_ == strPressedBinding.enumButton_);

            REQUIRE(GetButton(strState, strProbedBinding.enumButton_) == bExpected);
        }
    }
}

TEST_CASE("a default snapshot reports every control released", "[controls]")
{
    const SGamepadState strState;

    for (const EGamepadButton enumButton : AllGamepadButtons())
    {
        REQUIRE_FALSE(GetButton(strState, enumButton));
    }
}

TEST_CASE("identity names are stable control names", "[controls]")
{
    for (const SExpectedBinding &strBinding : arrExpectedBindings)
    {
        REQUIRE(GetGamepadButtonName(strBinding.enumButton_) == strBinding.charName_);
    }

    // Names must be distinct, or a consumer cannot use them as keys.
    for (const SExpectedBinding &strLeftBinding : arrExpectedBindings)
    {
        std::size_t szMatchCount = 0;
        for (const SExpectedBinding &strRightBinding : arrExpectedBindings)
        {
            if (GetGamepadButtonName(strLeftBinding.enumButton_) ==
                GetGamepadButtonName(strRightBinding.enumButton_))
            {
                ++szMatchCount;
            }
        }

        REQUIRE(szMatchCount == 1U);
    }
}

TEST_CASE("an out-of-range identity is handled rather than trusted", "[controls]")
{
    SGamepadState strState;
    strState.bButtonA_ = true;

    // A value can reach this API from a binding file or a language boundary, so
    // it must be rejected instead of indexing past the table.
    REQUIRE_FALSE(GetButton(strState, enumOutOfRangeButton));
    REQUIRE(GetGamepadButtonName(enumOutOfRangeButton) == "Unknown");
    REQUIRE(ClassifyButtonEdge(strState, strState, enumOutOfRangeButton) == EButtonEdge::None);
}

TEST_CASE("the snapshot edge overload agrees with the scalar one", "[controls]")
{
    SGamepadState strReleasedState;
    SGamepadState strPressedState;
    strPressedState.bButtonA_ = true;

    REQUIRE(ClassifyButtonEdge(strReleasedState, strReleasedState, EGamepadButton::A) ==
            EButtonEdge::None);
    REQUIRE(ClassifyButtonEdge(strReleasedState, strPressedState, EGamepadButton::A) ==
            EButtonEdge::Pressed);
    REQUIRE(ClassifyButtonEdge(strPressedState, strPressedState, EGamepadButton::A) ==
            EButtonEdge::Held);
    REQUIRE(ClassifyButtonEdge(strPressedState, strReleasedState, EGamepadButton::A) ==
            EButtonEdge::Released);

    // An untouched control must not pick up the edge of a touched one.
    REQUIRE(ClassifyButtonEdge(strReleasedState, strPressedState, EGamepadButton::B) ==
            EButtonEdge::None);
}

TEST_CASE("stick conditioning touches only the stick axes", "[controls]")
{
    constexpr double dDeadzone = 0.25;

    SGamepadState strState;
    strState.dLeftStickX_ = 0.10;  // Inside the deadzone.
    strState.dLeftStickY_ = 1.0;   // Full deflection.
    strState.dRightStickX_ = -1.0; // Full deflection, negative.
    strState.dRightStickY_ = 0.625;
    strState.dLeftTrigger_ = 0.10;
    strState.dRightTrigger_ = 1.0;
    strState.bButtonA_ = true;
    strState.bDpadLeft_ = true;
    strState.bConnected_ = true;
    strState.ui64SequenceId_ = 7U;
    strState.ui64TimestampNs_ = 99U;

    const SGamepadState strConditionedState = ApplyStickDeadzone(strState, dDeadzone);

    REQUIRE_THAT(strConditionedState.dLeftStickX_, WithinAbs(0.0, dExactTolerance));
    REQUIRE_THAT(strConditionedState.dLeftStickY_, WithinAbs(1.0, 1.0e-12));
    REQUIRE_THAT(strConditionedState.dRightStickX_, WithinAbs(-1.0, 1.0e-12));
    REQUIRE_THAT(strConditionedState.dRightStickY_, WithinAbs(0.5, 1.0e-12));

    // Triggers are unidirectional, so the stick deadzone must leave them alone.
    REQUIRE_THAT(strConditionedState.dLeftTrigger_, WithinAbs(0.10, dExactTolerance));
    REQUIRE_THAT(strConditionedState.dRightTrigger_, WithinAbs(1.0, dExactTolerance));

    // Everything that is not a stick axis passes through unchanged.
    REQUIRE(strConditionedState.bButtonA_);
    REQUIRE(strConditionedState.bDpadLeft_);
    REQUIRE(strConditionedState.bConnected_);
    REQUIRE(strConditionedState.ui64SequenceId_ == 7U);
    REQUIRE(strConditionedState.ui64TimestampNs_ == 99U);
}

TEST_CASE("a disabled stick deadzone conditions nothing", "[controls]")
{
    SGamepadState strState;
    strState.dLeftStickX_ = 0.02;
    strState.dRightStickY_ = -0.03;

    const SGamepadState strConditionedState = ApplyStickDeadzone(strState, 0.0);

    REQUIRE_THAT(strConditionedState.dLeftStickX_, WithinAbs(0.02, dExactTolerance));
    REQUIRE_THAT(strConditionedState.dRightStickY_, WithinAbs(-0.03, dExactTolerance));
}
