/**
 * @file GamepadControls.h
 * @brief Named control identities and snapshot-level operations.
 * @details Defines the button mapping shared by snapshots, examples, bindings,
 *          and ROS publishers.
 */
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include <xbox_controller_api/GamepadFilters.h>
#include <xbox_controller_api/SGamepadState.h>

namespace xbox_controller_api
{
    /**
     * @brief Identity of one button on an xpad-class controller.
     *
     * Values are contiguous and may be used as stable indices. Append new
     * controls at the end to preserve their order.
     */
    enum class EGamepadButton : std::uint8_t
    {
        A = 0,
        B = 1,
        X = 2,
        Y = 3,
        LeftShoulder = 4,
        RightShoulder = 5,
        LeftStickClick = 6,
        RightStickClick = 7,
        Back = 8,
        Start = 9,
        Guide = 10,
        DpadUp = 11,
        DpadDown = 12,
        DpadLeft = 13,
        DpadRight = 14
    };

    /**
     * @brief Every button identity, in enumeration order.
     *
     * The returned view refers to static storage.
     */
    [[nodiscard]] std::span<const EGamepadButton> AllGamepadButtons() noexcept;

    /**
     * @brief Read one named button out of a snapshot.
     *
     * @param strGamepadState Snapshot to read.
     * @param enumButton Button identity.
     * @return Pressed state, or false when the identity is out of range.
     */
    [[nodiscard]] bool GetButton(const SGamepadState &strGamepadState,
                                 EGamepadButton enumButton) noexcept;

    /**
     * @brief Return the short, stable name of a button identity.
     *
     * Names are stable identifiers such as "A", "LB", and "DpadUp".
     *
     * @param enumButton Button identity.
     * @return Name view over static storage, or "Unknown" when out of range.
     */
    [[nodiscard]] std::string_view GetGamepadButtonName(EGamepadButton enumButton) noexcept;

    /**
     * @brief Classify how one named button changed between two snapshots.
     *
     * @param strPreviousState Earlier snapshot.
     * @param strCurrentState Later snapshot.
     * @param enumButton Button identity.
     * @return The matching edge classification.
     */
    [[nodiscard]] EButtonEdge ClassifyButtonEdge(const SGamepadState &strPreviousState,
                                                 const SGamepadState &strCurrentState,
                                                 EGamepadButton enumButton) noexcept;

    /**
     * @brief Condition all four stick axes of a snapshot in one step.
     *
     * Triggers, buttons, and metadata are copied unchanged.
     *
     * @param strGamepadState Snapshot to condition.
     * @param dStickDeadzone Deadzone half-width forwarded to
     *        ApplyRescaledDeadzone, where values <= 0 pass through and values
     *        >= 1 suppress the sticks entirely.
     * @return A copy of the snapshot with conditioned stick axes.
     */
    [[nodiscard]] SGamepadState ApplyStickDeadzone(const SGamepadState &strGamepadState,
                                                   double dStickDeadzone) noexcept;
} // namespace xbox_controller_api
