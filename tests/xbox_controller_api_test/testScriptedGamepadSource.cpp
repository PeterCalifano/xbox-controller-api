/**
 * @file testScriptedGamepadSource.cpp
 * @brief Exercises the deterministic replay source and the poll-model contract.
 * @details These checks also stand in for the shared CGamepadSource behavior,
 *          since the scripted source is the only implementation that can be
 *          driven without hardware.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <xbox_controller_api/CScriptedGamepadSource.h>
#include <xbox_controller_api/GamepadFilters.h>
#include <xbox_controller_api/SGamepadState.h>

#include <cstdint>
#include <vector>

using Catch::Matchers::WithinAbs;
using xbox_controller_api::ClassifyButtonEdge;
using xbox_controller_api::CScriptedGamepadSource;
using xbox_controller_api::EButtonEdge;
using xbox_controller_api::SGamepadState;

namespace
{
    /// Tolerance for values that must be reproduced bit-exactly.
    constexpr double dExactTolerance = 0.0;

    /// @brief Build a recognizable sample with one button pressed.
    SGamepadState MakeTestFrame(double dLeftStickY, bool bButtonAPressed)
    {
        SGamepadState strFrame_;
        strFrame_.dLeftStickY_ = dLeftStickY;
        strFrame_.bButtonA_ = bButtonAPressed;

        return strFrame_;
    }
} // namespace

TEST_CASE("a fresh scripted source has published nothing", "[source][scripted]")
{
    const CScriptedGamepadSource objSource_;

    REQUIRE(objSource_.pendingFrameCount() == 0U);
    REQUIRE_FALSE(objSource_.connected());
    REQUIRE(objSource_.state().ui64SequenceId_ == 0U);
}

TEST_CASE("an empty script reports no new data without faking a disconnect",
          "[source][scripted]")
{
    CScriptedGamepadSource objSource_;

    REQUIRE_FALSE(objSource_.update());
    REQUIRE(objSource_.state().ui64SequenceId_ == 0U);

    // Play one frame, exhaust the script, and confirm the last snapshot stands.
    objSource_.pushFrame(MakeTestFrame(0.5, true));
    REQUIRE(objSource_.update());
    REQUIRE_FALSE(objSource_.update());

    REQUIRE(objSource_.connected());
    REQUIRE(objSource_.state().bButtonA_);
    REQUIRE_THAT(objSource_.state().dLeftStickY_, WithinAbs(0.5, dExactTolerance));

    // An exhausted script must not advance script time.
    REQUIRE(objSource_.state().ui64SequenceId_ == 1U);
}

TEST_CASE("queued frames replay verbatim in order", "[source][scripted]")
{
    CScriptedGamepadSource objSource_;

    objSource_.pushFrame(MakeTestFrame(-0.25, false));
    objSource_.pushFrame(MakeTestFrame(0.75, true));
    REQUIRE(objSource_.pendingFrameCount() == 2U);

    REQUIRE(objSource_.update());
    REQUIRE(objSource_.pendingFrameCount() == 1U);
    REQUIRE_THAT(objSource_.state().dLeftStickY_, WithinAbs(-0.25, dExactTolerance));
    REQUIRE_FALSE(objSource_.state().bButtonA_);
    REQUIRE(objSource_.state().ui64SequenceId_ == 1U);

    REQUIRE(objSource_.update());
    REQUIRE(objSource_.pendingFrameCount() == 0U);
    REQUIRE_THAT(objSource_.state().dLeftStickY_, WithinAbs(0.75, dExactTolerance));
    REQUIRE(objSource_.state().bButtonA_);

    // The sequence id must advance by exactly one per consumed frame so
    // consumers can detect a dropped sample.
    REQUIRE(objSource_.state().ui64SequenceId_ == 2U);
}

TEST_CASE("a queued frame always represents an attached device", "[source][scripted]")
{
    CScriptedGamepadSource objSource_;

    // A caller cannot express a detach through pushFrame(): the disconnect
    // contract has exactly one entry point.
    SGamepadState strFrame_ = MakeTestFrame(0.1, false);
    strFrame_.bConnected_ = false;
    objSource_.pushFrame(strFrame_);

    REQUIRE(objSource_.update());
    REQUIRE(objSource_.connected());
}

TEST_CASE("a replayed disconnect zeroes the published snapshot", "[source][scripted]")
{
    CScriptedGamepadSource objSource_;

    objSource_.pushFrame(MakeTestFrame(1.0, true));
    objSource_.pushDisconnect();

    REQUIRE(objSource_.update());
    REQUIRE(objSource_.connected());

    // The unplug reports false yet still publishes a snapshot, so a consumer
    // reading state() blindly sees a neutral command instead of the last value.
    REQUIRE_FALSE(objSource_.update());
    REQUIRE_FALSE(objSource_.connected());
    REQUIRE_THAT(objSource_.state().dLeftStickY_, WithinAbs(0.0, dExactTolerance));
    REQUIRE_FALSE(objSource_.state().bButtonA_);

    // Consuming the disconnect is itself an observable event.
    REQUIRE(objSource_.state().ui64SequenceId_ == 2U);
}

TEST_CASE("button edges are recoverable across replayed frames", "[source][scripted]")
{
    CScriptedGamepadSource objSource_;

    constexpr bool bScriptedPresses_[] = {false, true, true, false};
    for (const bool bPressed_ : bScriptedPresses_)
    {
        objSource_.pushFrame(MakeTestFrame(0.0, bPressed_));
    }

    // Classify against the previous sample the way a consumer loop would, so
    // this covers the intended pairing of source and filter rather than the
    // filter alone.
    std::vector<EButtonEdge> vecObservedEdges_;
    bool bPreviousPressed_ = objSource_.state().bButtonA_;

    while (objSource_.update())
    {
        const bool bCurrentPressed_ = objSource_.state().bButtonA_;
        vecObservedEdges_.push_back(ClassifyButtonEdge(bPreviousPressed_, bCurrentPressed_));
        bPreviousPressed_ = bCurrentPressed_;
    }

    const std::vector<EButtonEdge> vecExpectedEdges_ = {EButtonEdge::None,
                                                        EButtonEdge::Pressed,
                                                        EButtonEdge::Held,
                                                        EButtonEdge::Released};

    REQUIRE(vecObservedEdges_ == vecExpectedEdges_);
}
