/**
 * @file CGamepadWrapper.h
 * @brief Flat, binding-friendly facade over the SDL2 gamepad source.
 * @details Provides scalar accessors for binding generators that cannot expose
 *          the snapshot-oriented C++ API directly.
 */
#pragma once

#include <cstdint>
#include <string>

#include <xbox_controller_api/CSdlGamepadSource.h>
#include <xbox_controller_api/SGamepadState.h>

namespace xbox_controller_api
{
    /**
     * @brief Binding-facing controller facade with per-control accessors.
     *
     * Call update() once per poll and read the resulting snapshot through the
     * accessors. The wrapper applies its configured stick deadzone to that
     * snapshot. With no SDL2 backend, open() returns false and accessors report
     * rest values.
     */
    class CGamepadWrapper
    {
      public:
        /** @brief Create a wrapper without opening a controller. */
        CGamepadWrapper();

        /** @brief Release resources owned by the wrapped controller source. */
        ~CGamepadWrapper();

        CGamepadWrapper(const CGamepadWrapper &) = delete;
        CGamepadWrapper &operator=(const CGamepadWrapper &) = delete;
        CGamepadWrapper(CGamepadWrapper &&) = delete;
        CGamepadWrapper &operator=(CGamepadWrapper &&) = delete;

        /**
         * @brief Report whether this build contains the SDL2 backend.
         * @return True when SDL2 support was compiled in.
         */
        [[nodiscard]] static bool isBackendAvailable();

        /**
         * @brief Attach to the lowest-numbered available game controller.
         * @return True when a device was attached; see lastError() otherwise.
         */
        [[nodiscard]] bool open();

        /**
         * @brief Attach to an explicit joystick index.
         *
         * A distinct name avoids an overload in the generated bindings.
         *
         * @param i32JoystickIndex Joystick index to attach to.
         * @return True when a device was attached; see lastError() otherwise.
         */
        [[nodiscard]] bool openIndex(std::int32_t i32JoystickIndex);

        /** @brief Release the device; safe to call when nothing is open. */
        void close();

        /**
         * @brief Poll the device and refresh every accessor below.
         * @return True when a connected sample was published.
         */
        [[nodiscard]] bool update();

        /**
         * @brief Report whether a device is attached and answering polls.
         * @return True while the current snapshot is connected.
         */
        [[nodiscard]] bool connected() const;

        /**
         * @brief Return the device name reported by SDL.
         * @return Empty string when no controller is open.
         */
        [[nodiscard]] std::string deviceName() const;

        /**
         * @brief Return the most recent failure description.
         * @return Empty string when no failure has been recorded.
         */
        [[nodiscard]] std::string lastError() const;

        /**
         * @brief Set the deadzone applied to the four stick axes.
         *
         * The default 0.0 passes values through. Triggers are unchanged.
         *
         * @param dStickDeadzone Deadzone half-width; values <= 0 disable it and
         *        values >= 1 suppress the sticks entirely.
         */
        void setStickDeadzone(double dStickDeadzone);

        /**
         * @brief Return the configured stick deadzone.
         * @return Deadzone half-width supplied to setStickDeadzone().
         */
        [[nodiscard]] double stickDeadzone() const;

        /**
         * @brief Return left stick X, positive to the right.
         * @return Conditioned value in [-1, 1].
         */
        [[nodiscard]] double leftStickX() const;

        /**
         * @brief Return left stick Y, positive upward.
         * @return Conditioned value in [-1, 1].
         */
        [[nodiscard]] double leftStickY() const;

        /**
         * @brief Return right stick X, positive to the right.
         * @return Conditioned value in [-1, 1].
         */
        [[nodiscard]] double rightStickX() const;

        /**
         * @brief Return right stick Y, positive upward.
         * @return Conditioned value in [-1, 1].
         */
        [[nodiscard]] double rightStickY() const;

        /**
         * @brief Return the left trigger value.
         * @return Value in [0, 1], unchanged by the stick deadzone.
         */
        [[nodiscard]] double leftTrigger() const;

        /**
         * @brief Return the right trigger value.
         * @return Value in [0, 1], unchanged by the stick deadzone.
         */
        [[nodiscard]] double rightTrigger() const;

        /** @name Named button accessors */
        /// @{
        /** @brief Return the current A button state. @return True when pressed. */
        [[nodiscard]] bool buttonA() const;
        /** @brief Return the current B button state. @return True when pressed. */
        [[nodiscard]] bool buttonB() const;
        /** @brief Return the current X button state. @return True when pressed. */
        [[nodiscard]] bool buttonX() const;
        /** @brief Return the current Y button state. @return True when pressed. */
        [[nodiscard]] bool buttonY() const;
        /** @brief Return the current left shoulder state. @return True when pressed. */
        [[nodiscard]] bool leftShoulder() const;
        /** @brief Return the current right shoulder state. @return True when pressed. */
        [[nodiscard]] bool rightShoulder() const;
        /** @brief Return the current left stick-click state. @return True when pressed. */
        [[nodiscard]] bool leftStickClick() const;
        /** @brief Return the current right stick-click state. @return True when pressed. */
        [[nodiscard]] bool rightStickClick() const;
        /** @brief Return the current Back button state. @return True when pressed. */
        [[nodiscard]] bool back() const;
        /** @brief Return the current Start button state. @return True when pressed. */
        [[nodiscard]] bool start() const;
        /** @brief Return the current Guide button state. @return True when pressed. */
        [[nodiscard]] bool guide() const;
        /** @brief Return the current D-pad up state. @return True when pressed. */
        [[nodiscard]] bool dpadUp() const;
        /** @brief Return the current D-pad down state. @return True when pressed. */
        [[nodiscard]] bool dpadDown() const;
        /** @brief Return the current D-pad left state. @return True when pressed. */
        [[nodiscard]] bool dpadLeft() const;
        /** @brief Return the current D-pad right state. @return True when pressed. */
        [[nodiscard]] bool dpadRight() const;
        /// @}

        /**
         * @brief Sequence number of the published sample, 0 before the first.
         *
         * Zero means no sample has been published.
         */
        [[nodiscard]] std::uint64_t sequenceId() const;

        /**
         * @brief Number of buttons reachable through the indexed accessors.
         *
         * Use buttonName() and buttonPressed() to iterate controls.
         */
        [[nodiscard]] std::int32_t buttonCount() const;

        /**
         * @brief Stable control name for an index in [0, buttonCount()).
         * @return The control name, or "Unknown" when the index is out of range.
         */
        [[nodiscard]] std::string buttonName(std::int32_t i32ButtonIndex) const;

        /**
         * @brief Pressed state for an index in [0, buttonCount()).
         * @return The pressed state, or false when the index is out of range.
         */
        [[nodiscard]] bool buttonPressed(std::int32_t i32ButtonIndex) const;

      private:
        /**
         * @brief Recompute the conditioned snapshot every accessor reads.
         *
         * Called after each source update and deadzone change.
         */
        void refreshConditionedState_();

        CSdlGamepadSource objSource_;
        double dStickDeadzone_{0.0};
        SGamepadState strConditionedState_;
    };
} // namespace xbox_controller_api
