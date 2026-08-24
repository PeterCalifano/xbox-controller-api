/**
 * @file testGamepadState.cpp
 * @brief Pins the snapshot aggregate defaults and the disconnect contract.
 * @details The disconnect contract is safety-relevant: a consumer that ignores
 *          the connection flag must still receive a neutral command, so these
 *          checks cover every field rather than a representative sample.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <xbox_controller_api/SGamepadState.h>

#include <cstdint>

using Catch::Matchers::WithinAbs;
using xbox_controller_api::MakeDisconnectedState;
using xbox_controller_api::SGamepadState;

namespace
{
    /// Tolerance for values that must be reproduced bit-exactly.
    constexpr double dExactTolerance = 0.0;

    /// @brief Require that every input field of a snapshot sits at rest.
    void RequireNeutralInputs(const SGamepadState &strGamepadState)
    {
        REQUIRE_THAT(strGamepadState.dLeftStickX_, WithinAbs(0.0, dExactTolerance));
        REQUIRE_THAT(strGamepadState.dLeftStickY_, WithinAbs(0.0, dExactTolerance));
        REQUIRE_THAT(strGamepadState.dRightStickX_, WithinAbs(0.0, dExactTolerance));
        REQUIRE_THAT(strGamepadState.dRightStickY_, WithinAbs(0.0, dExactTolerance));
        REQUIRE_THAT(strGamepadState.dLeftTrigger_, WithinAbs(0.0, dExactTolerance));
        REQUIRE_THAT(strGamepadState.dRightTrigger_, WithinAbs(0.0, dExactTolerance));

        REQUIRE_FALSE(strGamepadState.bButtonA_);
        REQUIRE_FALSE(strGamepadState.bButtonB_);
        REQUIRE_FALSE(strGamepadState.bButtonX_);
        REQUIRE_FALSE(strGamepadState.bButtonY_);
        REQUIRE_FALSE(strGamepadState.bLeftShoulder_);
        REQUIRE_FALSE(strGamepadState.bRightShoulder_);
        REQUIRE_FALSE(strGamepadState.bLeftStickClick_);
        REQUIRE_FALSE(strGamepadState.bRightStickClick_);
        REQUIRE_FALSE(strGamepadState.bBack_);
        REQUIRE_FALSE(strGamepadState.bStart_);
        REQUIRE_FALSE(strGamepadState.bGuide_);
        REQUIRE_FALSE(strGamepadState.bDpadUp_);
        REQUIRE_FALSE(strGamepadState.bDpadDown_);
        REQUIRE_FALSE(strGamepadState.bDpadLeft_);
        REQUIRE_FALSE(strGamepadState.bDpadRight_);
    }
} // namespace

TEST_CASE("a default snapshot reports nothing read yet", "[state]")
{
    const SGamepadState strDefaultState_;

    RequireNeutralInputs(strDefaultState_);

    REQUIRE_FALSE(strDefaultState_.bConnected_);
    REQUIRE(strDefaultState_.ui64SequenceId_ == 0U);
    REQUIRE(strDefaultState_.ui64TimestampNs_ == 0U);
}

TEST_CASE("the disconnect snapshot zeroes every input", "[state]")
{
    constexpr std::uint64_t ui64SequenceId_ = 42U;
    constexpr std::uint64_t ui64TimestampNs_ = 123456789U;

    const SGamepadState strDisconnectedState_ = MakeDisconnectedState(ui64SequenceId_,
                                                                      ui64TimestampNs_);

    RequireNeutralInputs(strDisconnectedState_);
    REQUIRE_FALSE(strDisconnectedState_.bConnected_);

    // The counters are the only fields that survive the unplug, so a consumer
    // can still order and timestamp the disconnect event itself.
    REQUIRE(strDisconnectedState_.ui64SequenceId_ == ui64SequenceId_);
    REQUIRE(strDisconnectedState_.ui64TimestampNs_ == ui64TimestampNs_);
}

TEST_CASE("the disconnect snapshot ignores any prior deflection", "[state]")
{
    // Build a fully deflected sample first to prove the contract does not merely
    // pass an already-neutral value through.
    SGamepadState strActiveState_;
    strActiveState_.dLeftStickX_ = -1.0;
    strActiveState_.dLeftStickY_ = 1.0;
    strActiveState_.dRightStickX_ = 0.5;
    strActiveState_.dRightStickY_ = -0.5;
    strActiveState_.dLeftTrigger_ = 1.0;
    strActiveState_.dRightTrigger_ = 0.75;
    strActiveState_.bButtonA_ = true;
    strActiveState_.bDpadUp_ = true;
    strActiveState_.bConnected_ = true;

    strActiveState_ = MakeDisconnectedState(strActiveState_.ui64SequenceId_ + 1U, 7U);

    RequireNeutralInputs(strActiveState_);
    REQUIRE_FALSE(strActiveState_.bConnected_);
    REQUIRE(strActiveState_.ui64SequenceId_ == 1U);
    REQUIRE(strActiveState_.ui64TimestampNs_ == 7U);
}
