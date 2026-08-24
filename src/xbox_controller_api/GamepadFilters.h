/**
 * @file GamepadFilters.h
 * @brief Pure conditioning helpers applied to normalized controller samples.
 * @details These stateless functions normalize axes, apply deadzones, and
 *          classify button transitions without requiring a hardware source.
 */
#pragma once

#include <cstdint>

namespace xbox_controller_api
{
    /**
     * @brief Transition of one button between two consecutive samples.
     *
     * The caller chooses the two samples being compared.
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
     * Computes `sign(x) * (|x| - d) / (1 - d)` outside the deadzone. The result
     * is continuous at the boundary and still reaches full deflection.
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
     * 32767 maps to 1.0. The asymmetric -32768 value is clamped to -1.0.
     *
     * @param i16RawAxis Raw hardware axis reading.
     * @return Normalized value in [-1, 1].
     */
    [[nodiscard]] double NormalizeStickAxis(std::int16_t i16RawAxis) noexcept;

    /**
     * @brief Normalize a raw signed trigger axis to [0, 1].
     *
     * Negative readings are clamped to the released position.
     *
     * @param i16RawAxis Raw hardware axis reading.
     * @return Normalized value in [0, 1].
     */
    [[nodiscard]] double NormalizeTriggerAxis(std::int16_t i16RawAxis) noexcept;

    /**
     * @brief Flip the sign of an axis value.
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
