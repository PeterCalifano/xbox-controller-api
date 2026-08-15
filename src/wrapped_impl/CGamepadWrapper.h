/**
 * @file CGamepadWrapper.h
 * @brief Flat, binding-friendly facade over the SDL2 gamepad source.
 * @details The generator that produces the Python bindings handles a narrow
 *          subset of C++: no overloads, no references to aggregates, no
 *          templates. This class therefore trades the snapshot-oriented C++ API
 *          for one scalar accessor per control, which is also the shape a Python
 *          caller expects. It owns the deadzone policy the native API leaves to
 *          the consumer, so a script gets conditioned values without importing
 *          the filter layer.
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
     * Every accessor reads the snapshot published by the most recent update(),
     * so a caller polls once per loop iteration and then reads as many controls
     * as it likes without re-sampling the device.
     *
     * In a build without the SDL2 backend the class still exists and still
     * compiles: open() returns false and every accessor reports its rest value.
     * A script therefore branches on a returned bool rather than on an
     * exception or a missing attribute.
     *
     * The type is neither copyable nor movable, because it owns a device handle.
     */
    class CGamepadWrapper
    {
      public:
        CGamepadWrapper();
        ~CGamepadWrapper();

        CGamepadWrapper(const CGamepadWrapper &) = delete;
        CGamepadWrapper &operator=(const CGamepadWrapper &) = delete;
        CGamepadWrapper(CGamepadWrapper &&) = delete;
        CGamepadWrapper &operator=(CGamepadWrapper &&) = delete;

        /** @brief Report whether this build contains the SDL2 backend. */
        [[nodiscard]] static bool isBackendAvailable();

        /**
         * @brief Attach to the lowest-numbered available game controller.
         * @return True when a device was attached; see lastError() otherwise.
         */
        [[nodiscard]] bool open();

        /**
         * @brief Attach to an explicit joystick index.
         *
         * Kept as a separate name rather than an overload of open(), because the
         * binding generator cannot disambiguate overloaded methods.
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

        /** @brief True while a device is attached and answering polls. */
        [[nodiscard]] bool connected() const;

        /** @brief Device name reported by SDL, empty when nothing is open. */
        [[nodiscard]] std::string deviceName() const;

        /** @brief Description of the most recent failure, empty when none. */
        [[nodiscard]] std::string lastError() const;

        /**
         * @brief Set the deadzone applied to the four stick axes.
         *
         * Defaults to 0.0, which passes normalized values through untouched, so
         * the wrapper never silently reshapes input a caller did not ask to
         * reshape. Triggers are unidirectional and are never deadzoned here.
         *
         * @param dStickDeadzone Deadzone half-width; values <= 0 disable it and
         *        values >= 1 suppress the sticks entirely.
         */
        void setStickDeadzone(double dStickDeadzone);

        /** @brief Currently configured stick deadzone. */
        [[nodiscard]] double stickDeadzone() const;

        /** @brief Left stick X in [-1, 1], positive to the right. */
        [[nodiscard]] double leftStickX() const;

        /** @brief Left stick Y in [-1, 1], positive upward. */
        [[nodiscard]] double leftStickY() const;

        /** @brief Right stick X in [-1, 1], positive to the right. */
        [[nodiscard]] double rightStickX() const;

        /** @brief Right stick Y in [-1, 1], positive upward. */
        [[nodiscard]] double rightStickY() const;

        /** @brief Left trigger in [0, 1]; never deadzoned. */
        [[nodiscard]] double leftTrigger() const;

        /** @brief Right trigger in [0, 1]; never deadzoned. */
        [[nodiscard]] double rightTrigger() const;

        [[nodiscard]] bool buttonA() const;
        [[nodiscard]] bool buttonB() const;
        [[nodiscard]] bool buttonX() const;
        [[nodiscard]] bool buttonY() const;
        [[nodiscard]] bool leftShoulder() const;
        [[nodiscard]] bool rightShoulder() const;
        [[nodiscard]] bool leftStickClick() const;
        [[nodiscard]] bool rightStickClick() const;
        [[nodiscard]] bool back() const;
        [[nodiscard]] bool start() const;
        [[nodiscard]] bool guide() const;
        [[nodiscard]] bool dpadUp() const;
        [[nodiscard]] bool dpadDown() const;
        [[nodiscard]] bool dpadLeft() const;
        [[nodiscard]] bool dpadRight() const;

        /**
         * @brief Sequence number of the published sample, 0 before the first.
         *
         * Lets a script detect a missed or repeated poll without comparing every
         * control value.
         */
        [[nodiscard]] std::uint64_t sequenceId() const;

        /**
         * @brief Number of buttons reachable through the indexed accessors.
         *
         * The indexed trio below exists so a script can iterate controls
         * generically instead of hard-coding its own list of accessor names.
         * The named accessors above remain the ergonomic choice when a script
         * wants one specific control.
         */
        [[nodiscard]] std::int32_t buttonCount() const;

        /**
         * @brief Stable control name for an index in [0, buttonCount()).
         * @return The control name, or "Unknown" when the index is out of range.
         */
        [[nodiscard]] std::string buttonName(std::int32_t i32ButtonIndex) const;

        /**
         * @brief Pressed state for an index in [0, buttonCount()).
         * @return The pressed state, or false when the index is out of range,
         *         so a bad index from a script cannot read out of bounds.
         */
        [[nodiscard]] bool buttonPressed(std::int32_t i32ButtonIndex) const;

      private:
        /**
         * @brief Recompute the conditioned snapshot every accessor reads.
         *
         * Called whenever the sample or the deadzone changes, so the shaping
         * cost is paid once per poll rather than once per accessor call.
         */
        void refreshConditionedState_();

      private:
        CSdlGamepadSource objSource_;
        double dStickDeadzone_{0.0};
        SGamepadState strConditionedState_;
    };
} // namespace xbox_controller_api
