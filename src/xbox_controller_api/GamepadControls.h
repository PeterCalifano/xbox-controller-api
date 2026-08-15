/**
 * @file GamepadControls.h
 * @brief Named control identities and snapshot-level operations.
 * @details Where GamepadFilters.h conditions individual scalar values, this
 *          header names the controls themselves and operates on a whole
 *          SGamepadState. Naming the buttons once here is what lets a consumer
 *          iterate them generically, rather than every logger, publisher,
 *          recorder, and binding keeping its own copy of the same mapping.
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
     * The values are contiguous from zero, so the enumeration doubles as a
     * stable index for bindings and serialization formats that cannot carry a
     * C++ enum. Order is part of the contract: append new controls at the end.
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
     * Returned as a view over static storage, so it is free to call and safe to
     * hold. Use it to drive generic loops instead of hard-coding the control
     * list at the call site.
     */
    [[nodiscard]] std::span<const EGamepadButton> AllGamepadButtons() noexcept;

    /**
     * @brief Read one named button out of a snapshot.
     *
     * @param strGamepadState Snapshot to read.
     * @param enumButton Button identity.
     * @return Pressed state, or false when the identity is out of range, which
     *         keeps a value crossing a language boundary from indexing wildly.
     */
    [[nodiscard]] bool GetButton(const SGamepadState &strGamepadState,
                                 EGamepadButton enumButton) noexcept;

    /**
     * @brief Return the short, stable name of a button identity.
     *
     * These are control names rather than presentation strings: "A", "LB",
     * "DpadUp". A consumer that wants different labels should map from these
     * rather than redefine them.
     *
     * @param enumButton Button identity.
     * @return Name view over static storage, or "Unknown" when out of range.
     */
    [[nodiscard]] std::string_view GetGamepadButtonName(EGamepadButton enumButton) noexcept;

    /**
     * @brief Classify how one named button changed between two snapshots.
     *
     * Builds on the scalar ClassifyButtonEdge so a consumer no longer needs to
     * know which member of the aggregate a control lives in.
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
     * Applying the deadzone once per sample, rather than once per accessor call,
     * keeps the conditioning cost proportional to the poll rate instead of to
     * how many controls the consumer happens to read.
     *
     * Triggers are deliberately left untouched: they are unidirectional, so
     * suppressing a light pull is a separate policy decision. Counters and
     * button states are copied through unchanged.
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
