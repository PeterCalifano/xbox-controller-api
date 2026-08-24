/**
 * @file CSdlGamepadSource.h
 * @brief SDL2-backed gamepad source for xpad-class controllers on Linux.
 * @details SDL types remain in the implementation, so this header is available
 *          in builds with or without the SDL2 backend.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <xbox_controller_api/CGamepadSource.h>

namespace xbox_controller_api
{
    /**
     * @brief Gamepad source reading a physical controller through SDL2.
     *
     * Construction does not initialize SDL. Call open() to attach a controller
     * and close() to release it. Instances are single-threaded and do not
     * consume the SDL event queue. After a disconnect, call open() again to
     * attach a controller.
     */
    class CSdlGamepadSource final : public CGamepadSource
    {
      public:
        /** @brief Create a source without opening a controller. */
        CSdlGamepadSource();

        /** @brief Release any open controller and SDL subsystem reference. */
        ~CSdlGamepadSource() override;

        CSdlGamepadSource(const CSdlGamepadSource &) = delete;
        CSdlGamepadSource &operator=(const CSdlGamepadSource &) = delete;
        CSdlGamepadSource(CSdlGamepadSource &&) = delete;
        CSdlGamepadSource &operator=(CSdlGamepadSource &&) = delete;

        /**
         * @brief Report whether this build contains the SDL2 backend at all.
         *
         * A true result does not imply that a controller is attached.
         * @return True when this build includes SDL2 support.
         */
        [[nodiscard]] static bool isBackendAvailable() noexcept;

        /**
         * @brief Attach to a controller, initializing SDL2 on first use.
         *
         * Any open controller is released first.
         *
         * @param i32JoystickIndex Joystick index to attach to. The default -1
         *        selects the lowest-numbered SDL game controller.
         * @return True when a device was attached; otherwise lastError()
         *         describes the failure.
         */
        [[nodiscard]] bool open(std::int32_t i32JoystickIndex = -1);

        /**
         * @brief Release the device and any SDL subsystem reference held.
         *
         * Safe to call repeatedly. The published snapshot is reset to rest.
         */
        void close() noexcept;

        /**
         * @brief Poll the attached device and publish a fresh snapshot.
         *
         * @return True when a connected sample was published. False when no
         *         device is open, when the backend is absent, or when the device
         *         was detached.
         */
        [[nodiscard]] bool update() override;

        /**
         * @brief Return the name reported by SDL for the attached device.
         * @return Empty string when no controller is open.
         */
        [[nodiscard]] const std::string &deviceName() const noexcept;

        /**
         * @brief Describe the most recent failure, empty when none occurred.
         *
         * A successful open() clears this value.
         * @return Empty string when no failure has been recorded.
         */
        [[nodiscard]] const std::string &lastError() const noexcept;

      private:
        struct SImpl;
        std::unique_ptr<SImpl> pImpl_;
    };
} // namespace xbox_controller_api
