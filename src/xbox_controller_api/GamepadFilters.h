/**
 * @file GamepadFilters.h
 * @brief Pure conditioning helpers applied to normalized controller samples.
 * @details Every function here is a free, stateless, side-effect-free
 *          transformation. Keeping deadzone shaping, axis normalization, and
 *          edge detection out of the backends means each policy is opt-in,
 *          independently testable without hardware, and reusable by consumers
 *          that never construct a source at all.
 */
#pragma once

#include <cstdint>

namespace xbox_controller_api
{
    /**
     * @brief Transition of one button between two consecutive samples.
     *
     * Edges are derived rather than reported by the backend, because only the
     * consumer knows which pair of samples defines "consecutive" for its loop.
     */
    enum class EButtonEdge : std::uint8_t
    {
        None = 0,     ///< Released in both samples.
        Pressed = 1,  ///< Released, then pressed.
        Released = 2, ///< Pressed, then released.
        Held = 3      ///< Pressed in both samples.
    };

    /**
     * @brief Suppress a deadzone around rest and rescale the surviving range.
     *
     * Computes `sign(x) * (|x| - d) / (1 - d)`, so the response is continuous at
     * the deadzone boundary and still reaches full deflection at the extremes.
     * A plain cut-off would instead leave the axis unable to reach 1.0 and would
     * jump discontinuously as the stick crosses the threshold.
     *
     * @param dAxisValue Normalized axis value; values outside [-1, 1] are clamped.
     * @param dDeadzone Deadzone half-width. Values <= 0 clamp only, leaving the
     *        input otherwise untouched; values >= 1 suppress the axis entirely.
     * @return Conditioned value in [-1, 1].
     */
    [[nodiscard]] double ApplyRescaledDeadzone(double dAxisValue, double dDeadzone) noexcept;

    /**
     * @brief Normalize a raw signed stick axis to [-1, 1].
     *
     * Scaling uses the positive full-scale value so 32767 maps to exactly 1.0.
     * The asymmetric negative extreme -32768 would otherwise land just below
     * -1.0 and is clamped to exactly -1.0.
     *
     * @param i16RawAxis Raw hardware axis reading.
     * @return Normalized value in [-1, 1].
     */
    [[nodiscard]] double NormalizeStickAxis(std::int16_t i16RawAxis) noexcept;

    /**
     * @brief Normalize a raw signed trigger axis to [0, 1].
     *
     * Triggers are unidirectional, so negative readings carry no meaning and are
     * clamped to the released position rather than mirrored.
     *
     * @param i16RawAxis Raw hardware axis reading.
     * @return Normalized value in [0, 1].
     */
    [[nodiscard]] double NormalizeTriggerAxis(std::int16_t i16RawAxis) noexcept;

    /**
     * @brief Flip the sign of an axis value.
     *
     * Provided so a sign convention change is a named, greppable step rather
     * than a bare negation scattered across backends and consumers.
     *
     * @param dAxisValue Normalized axis value.
     * @return The negated value.
     */
    [[nodiscard]] double InvertAxis(double dAxisValue) noexcept;

    /**
     * @brief Classify how one button changed between two consecutive samples.
     *
     * @param bPreviousPressed Button state in the earlier sample.
     * @param bCurrentPressed Button state in the later sample.
     * @return The matching edge classification.
     */
    [[nodiscard]] EButtonEdge ClassifyButtonEdge(bool bPreviousPressed,
                                                 bool bCurrentPressed) noexcept;
} // namespace xbox_controller_api
