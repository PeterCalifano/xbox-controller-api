/**
 * @file testGamepadFilters.cpp
 * @brief Verifies the pure conditioning helpers against their documented ranges.
 * @details Floating-point expectations use Catch2 matchers rather than operator==
 *          because the default RelWithDebInfo build enables -Wfloat-equal. A zero
 *          tolerance still expresses an exactness requirement.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <xbox_controller_api/GamepadFilters.h>

#include <cstdint>
#include <limits>

using Catch::Matchers::WithinAbs;
using xbox_controller_api::ApplyRescaledDeadzone;
using xbox_controller_api::ClassifyButtonEdge;
using xbox_controller_api::EButtonEdge;
using xbox_controller_api::InvertAxis;
using xbox_controller_api::NormalizeStickAxis;
using xbox_controller_api::NormalizeTriggerAxis;

namespace
{
    /// Tolerance for values that must be reproduced bit-exactly.
    constexpr double dExactTolerance = 0.0;

    /// Tolerance for values reached through a division and a rescale.
    constexpr double dRescaleTolerance = 1.0e-12;
} // namespace

TEST_CASE("rescaled deadzone suppresses rest and preserves full deflection", "[filters]")
{
    constexpr double dDeadzone_ = 0.25;

    SECTION("a disabled deadzone passes the value through unchanged")
    {
        REQUIRE_THAT(ApplyRescaledDeadzone(0.0, 0.0), WithinAbs(0.0, dExactTolerance));
        REQUIRE_THAT(ApplyRescaledDeadzone(0.37, 0.0), WithinAbs(0.37, dExactTolerance));
        REQUIRE_THAT(ApplyRescaledDeadzone(-0.37, 0.0), WithinAbs(-0.37, dExactTolerance));

        // A negative deadzone is not an error: it degrades to the same
        // clamp-only behavior as zero.
        REQUIRE_THAT(ApplyRescaledDeadzone(0.37, -0.5), WithinAbs(0.37, dExactTolerance));
    }

    SECTION("values within the deadzone collapse to rest")
    {
        REQUIRE_THAT(ApplyRescaledDeadzone(0.0, dDeadzone_), WithinAbs(0.0, dExactTolerance));
        REQUIRE_THAT(ApplyRescaledDeadzone(0.10, dDeadzone_), WithinAbs(0.0, dExactTolerance));
        REQUIRE_THAT(ApplyRescaledDeadzone(-0.10, dDeadzone_), WithinAbs(0.0, dExactTolerance));
    }

    SECTION("the deadzone boundary itself is still rest")
    {
        REQUIRE_THAT(ApplyRescaledDeadzone(dDeadzone_, dDeadzone_),
                     WithinAbs(0.0, dExactTolerance));
        REQUIRE_THAT(ApplyRescaledDeadzone(-dDeadzone_, dDeadzone_),
                     WithinAbs(0.0, dExactTolerance));
    }

    SECTION("full deflection survives the rescale")
    {
        REQUIRE_THAT(ApplyRescaledDeadzone(1.0, dDeadzone_), WithinAbs(1.0, dRescaleTolerance));
        REQUIRE_THAT(ApplyRescaledDeadzone(-1.0, dDeadzone_), WithinAbs(-1.0, dRescaleTolerance));
    }

    SECTION("the midpoint of the surviving band maps to half deflection")
    {
        // With d = 0.25 the band [0.25, 1] rescales onto [0, 1], so 0.625 is
        // exactly half way along it.
        REQUIRE_THAT(ApplyRescaledDeadzone(0.625, dDeadzone_), WithinAbs(0.5, dRescaleTolerance));
    }

    SECTION("the response is symmetric about rest")
    {
        constexpr double dSamples_[] = {0.05, 0.25, 0.30, 0.6, 0.99, 1.0};
        for (const double dSample_ : dSamples_)
        {
            const double dPositiveResult_ = ApplyRescaledDeadzone(dSample_, dDeadzone_);
            const double dNegativeResult_ = ApplyRescaledDeadzone(-dSample_, dDeadzone_);

            REQUIRE_THAT(dNegativeResult_, WithinAbs(-dPositiveResult_, dExactTolerance));
        }
    }

    SECTION("the response never decreases as the input grows")
    {
        double dPreviousResult_ = ApplyRescaledDeadzone(0.0, dDeadzone_);
        for (int i32Step_ = 1; i32Step_ <= 100; ++i32Step_)
        {
            const double dSample_ = static_cast<double>(i32Step_) / 100.0;
            const double dResult_ = ApplyRescaledDeadzone(dSample_, dDeadzone_);

            REQUIRE(dResult_ >= dPreviousResult_);
            dPreviousResult_ = dResult_;
        }

        REQUIRE_THAT(dPreviousResult_, WithinAbs(1.0, dRescaleTolerance));
    }

    SECTION("a deadzone covering the whole range suppresses every input")
    {
        REQUIRE_THAT(ApplyRescaledDeadzone(1.0, 1.0), WithinAbs(0.0, dExactTolerance));
        REQUIRE_THAT(ApplyRescaledDeadzone(-1.0, 1.0), WithinAbs(0.0, dExactTolerance));
        REQUIRE_THAT(ApplyRescaledDeadzone(0.8, 2.5), WithinAbs(0.0, dExactTolerance));
    }

    SECTION("out-of-range inputs are clamped before any shaping")
    {
        REQUIRE_THAT(ApplyRescaledDeadzone(5.0, 0.0), WithinAbs(1.0, dExactTolerance));
        REQUIRE_THAT(ApplyRescaledDeadzone(-5.0, 0.0), WithinAbs(-1.0, dExactTolerance));
        REQUIRE_THAT(ApplyRescaledDeadzone(5.0, dDeadzone_), WithinAbs(1.0, dRescaleTolerance));
        REQUIRE_THAT(ApplyRescaledDeadzone(-5.0, dDeadzone_), WithinAbs(-1.0, dRescaleTolerance));
    }
}

TEST_CASE("raw stick axes normalize onto a symmetric unit range", "[filters]")
{
    constexpr std::int16_t i16RawMinimum_ = std::numeric_limits<std::int16_t>::min();
    constexpr std::int16_t i16RawMaximum_ = std::numeric_limits<std::int16_t>::max();

    SECTION("the extremes map exactly onto the unit bounds")
    {
        // The negative extreme is one count larger in magnitude than the
        // positive one, so this asserts the clamp rather than the division.
        REQUIRE_THAT(NormalizeStickAxis(i16RawMinimum_), WithinAbs(-1.0, dExactTolerance));
        REQUIRE_THAT(NormalizeStickAxis(i16RawMaximum_), WithinAbs(1.0, dExactTolerance));
    }

    SECTION("rest maps exactly onto zero")
    {
        REQUIRE_THAT(NormalizeStickAxis(0), WithinAbs(0.0, dExactTolerance));
    }

    SECTION("intermediate readings scale linearly")
    {
        REQUIRE_THAT(NormalizeStickAxis(static_cast<std::int16_t>(16384)),
                     WithinAbs(0.5, 1.0e-4));
        REQUIRE_THAT(NormalizeStickAxis(static_cast<std::int16_t>(-16384)),
                     WithinAbs(-0.5, 1.0e-4));
    }
}

TEST_CASE("raw trigger axes normalize onto a unidirectional range", "[filters]")
{
    constexpr std::int16_t i16RawMinimum_ = std::numeric_limits<std::int16_t>::min();
    constexpr std::int16_t i16RawMaximum_ = std::numeric_limits<std::int16_t>::max();

    SECTION("released and fully pulled map onto the range bounds")
    {
        REQUIRE_THAT(NormalizeTriggerAxis(0), WithinAbs(0.0, dExactTolerance));
        REQUIRE_THAT(NormalizeTriggerAxis(i16RawMaximum_), WithinAbs(1.0, dExactTolerance));
    }

    SECTION("negative readings clamp to released instead of mirroring")
    {
        REQUIRE_THAT(NormalizeTriggerAxis(static_cast<std::int16_t>(-1)),
                     WithinAbs(0.0, dExactTolerance));
        REQUIRE_THAT(NormalizeTriggerAxis(i16RawMinimum_), WithinAbs(0.0, dExactTolerance));
    }

    SECTION("intermediate readings scale linearly")
    {
        REQUIRE_THAT(NormalizeTriggerAxis(static_cast<std::int16_t>(16384)),
                     WithinAbs(0.5, 1.0e-4));
    }
}

TEST_CASE("axis inversion flips the sign without reshaping", "[filters]")
{
    REQUIRE_THAT(InvertAxis(1.0), WithinAbs(-1.0, dExactTolerance));
    REQUIRE_THAT(InvertAxis(-1.0), WithinAbs(1.0, dExactTolerance));
    REQUIRE_THAT(InvertAxis(0.0), WithinAbs(0.0, dExactTolerance));
    REQUIRE_THAT(InvertAxis(0.42), WithinAbs(-0.42, dExactTolerance));

    // Applying the inversion twice must return the original value, which is the
    // property the Y-axis convention relies on.
    REQUIRE_THAT(InvertAxis(InvertAxis(0.42)), WithinAbs(0.42, dExactTolerance));
}

TEST_CASE("button edges cover every transition between two samples", "[filters]")
{
    REQUIRE(ClassifyButtonEdge(false, false) == EButtonEdge::None);
    REQUIRE(ClassifyButtonEdge(false, true) == EButtonEdge::Pressed);
    REQUIRE(ClassifyButtonEdge(true, true) == EButtonEdge::Held);
    REQUIRE(ClassifyButtonEdge(true, false) == EButtonEdge::Released);
}
