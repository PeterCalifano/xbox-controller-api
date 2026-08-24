/**
 * @file SGamepadState.h
 * @brief Backend-independent snapshot of one xpad-class controller poll.
 * @details Hardware-specific mapping stays in each backend. This type exposes
 *          the normalized values shared by every source.
 */
#pragma once

#include <cstdint>

namespace xbox_controller_api
{
    /**
     * @brief One complete, normalized controller sample.
     *
     * This plain aggregate represents one sample. A default-constructed value
     * has every control at rest and @ref bConnected_ set to false.
     *
     * Axis conventions:
     * - Stick axes lie in [-1, 1]. X grows to the right and **Y grows up**, so a
     *   forward push on either stick yields a positive value. Backends negate
     *   the down-positive raw hardware Y to honor this.
     * - Trigger axes lie in [0, 1], with 0 fully released.
     * - No deadzone is applied here; use GamepadFilters.h when needed.
     */
    struct SGamepadState
    {
        // Analog sticks in [-1, 1], with X to the right and Y up.
        double dLeftStickX_ = 0.0;
        double dLeftStickY_ = 0.0;
        double dRightStickX_ = 0.0;
        double dRightStickY_ = 0.0;

        // Analog triggers in [0, 1], 0 when fully released.
        double dLeftTrigger_ = 0.0;
        double dRightTrigger_ = 0.0;

        // Face buttons.
        bool bButtonA_ = false;
        bool bButtonB_ = false;
        bool bButtonX_ = false;
        bool bButtonY_ = false;

        // Shoulder buttons.
        bool bLeftShoulder_ = false;
        bool bRightShoulder_ = false;

        // Pressable stick axes.
        bool bLeftStickClick_ = false;
        bool bRightStickClick_ = false;

        // Center cluster.
        bool bBack_ = false;
        bool bStart_ = false;
        bool bGuide_ = false;

        // Directional pad, reported as four independent booleans so diagonals
        // stay representable without a separate hat encoding.
        bool bDpadUp_ = false;
        bool bDpadDown_ = false;
        bool bDpadLeft_ = false;
        bool bDpadRight_ = false;

        /// @brief True only while a device is open and answering polls.
        bool bConnected_ = false;

        /// @brief Monotonic counter incremented once per consumed sample. Zero
        ///        means no sample has ever been published.
        std::uint64_t ui64SequenceId_ = 0U;

        /// @brief Steady-clock capture time in nanoseconds. Zero means never
        ///        sampled; the epoch is arbitrary, so only differences are
        ///        meaningful.
        std::uint64_t ui64TimestampNs_ = 0U;
    };

    /**
     * @brief Build the canonical snapshot published when no device is attached.
     *
     * Every axis and button is reset to rest and
     * bConnected_ is set to false.
     *
     * @param ui64SequenceId Sequence number to stamp on the disconnect sample.
     * @param ui64TimestampNs Steady-clock capture time in nanoseconds.
     * @return A fully zeroed snapshot carrying only the two supplied counters.
     */
    [[nodiscard]] SGamepadState MakeDisconnectedState(std::uint64_t ui64SequenceId,
                                                      std::uint64_t ui64TimestampNs) noexcept;
} // namespace xbox_controller_api
