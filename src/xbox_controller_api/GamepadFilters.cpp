/**
 * @file GamepadFilters.cpp
 * @brief Implements the conditioning helpers and disconnect snapshot factory.
 */

#include <xbox_controller_api/GamepadFilters.h>
#include <xbox_controller_api/SGamepadState.h>

#include <algorithm>
#include <cmath>

namespace xbox_controller_api
{
    namespace
    {
        /// Positive full scale of a signed 16-bit hardware axis. The negative
        /// extreme is one count larger in magnitude, which is why every
        /// normalization below clamps instead of trusting the division.
        constexpr double dAxisPositiveFullScale = 32767.0;
    } // namespace

    SGamepadState MakeDisconnectedState(std::uint64_t ui64SequenceId,
                                        std::uint64_t ui64TimestampNs) noexcept
    {
        // Value initialization resets any future fields to their rest values.
        SGamepadState strDisconnectedState_{};
        strDisconnectedState_.ui64SequenceId_ = ui64SequenceId;
        strDisconnectedState_.ui64TimestampNs_ = ui64TimestampNs;

        return strDisconnectedState_;
    }

    double ApplyRescaledDeadzone(double dAxisValue, double dDeadzone) noexcept
    {
        // Clamp up front so an out-of-range input can never escape the output
        // range, whichever branch below handles it.
        const double dClampedValue_ = std::clamp(dAxisValue, -1.0, 1.0);

        // A non-positive deadzone is the documented pass-through default.
        if (dDeadzone <= 0.0)
        {
            return dClampedValue_;
        }

        // A deadzone spanning the full range leaves nothing to rescale.
        if (dDeadzone >= 1.0)
        {
            return 0.0;
        }

        const double dMagnitude_ = std::abs(dClampedValue_);
        if (dMagnitude_ <= dDeadzone)
        {
            return 0.0;
        }

        // Map the surviving band [d, 1] back onto [0, 1]. This keeps the
        // response continuous at the boundary and preserves full deflection,
        // which a bare cut-off would lose.
        const double dRescaledMagnitude_ = (dMagnitude_ - dDeadzone) / (1.0 - dDeadzone);

        return std::copysign(dRescaledMagnitude_, dClampedValue_);
    }

    double NormalizeStickAxis(std::int16_t i16RawAxis) noexcept
    {
        // Scale by the positive full scale so full right/up maps to exactly 1.0,
        // then clamp to absorb the asymmetric negative extreme.
        const double dRawValue_ = static_cast<double>(i16RawAxis);

        return std::clamp(dRawValue_ / dAxisPositiveFullScale, -1.0, 1.0);
    }

    double NormalizeTriggerAxis(std::int16_t i16RawAxis) noexcept
    {
        // Negative trigger readings represent the released position.
        const double dRawValue_ = static_cast<double>(i16RawAxis);

        return std::clamp(dRawValue_ / dAxisPositiveFullScale, 0.0, 1.0);
    }

    double InvertAxis(double dAxisValue) noexcept
    {
        return -dAxisValue;
    }

    EButtonEdge ClassifyButtonEdge(bool bPreviousPressed, bool bCurrentPressed) noexcept
    {
        if (bCurrentPressed)
        {
            return bPreviousPressed ? EButtonEdge::Held : EButtonEdge::Pressed;
        }

        return bPreviousPressed ? EButtonEdge::Released : EButtonEdge::None;
    }
} // namespace xbox_controller_api
