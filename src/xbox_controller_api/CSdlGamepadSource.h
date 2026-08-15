/**
 * @file CSdlGamepadSource.h
 * @brief SDL2-backed gamepad source for xpad-class controllers on Linux.
 * @details Declares the only hardware-facing class in the library. The SDL types
 *          stay behind a pimpl so this header compiles without SDL2 headers on
 *          the include path, which is what lets consumers and the ROS overlay
 *          depend on the full API surface regardless of how the library was
 *          configured.
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
     * The class keeps the full API surface in every build. When the library was
     * configured with SDL2 disabled, isBackendAvailable() returns false and
     * open() fails with a fixed diagnostic instead of the header disappearing,
     * so a consumer branches on a value rather than on a preprocessor symbol.
     *
     * Lifecycle and threading contract:
     * - Construction performs no SDL call, so creating a source in a headless
     *   process is always safe. SDL is initialized lazily by open() and released
     *   by close() or the destructor.
     * - The instance must be used from a single thread. SDL's game controller
     *   subsystem does not support concurrent access to one device.
     * - The source never pumps or consumes the SDL event queue; it refreshes
     *   state by polling. A host application that owns its own SDL event loop
     *   therefore keeps every event it would otherwise have lost.
     * - A hot unplug is reported, never healed. update() publishes the
     *   disconnect snapshot and keeps returning false until the caller decides
     *   to open() again, so reconnection timing stays a caller policy.
     *
     * The type is neither copyable nor movable: it owns a device handle and an
     * SDL subsystem reference, and callers hold references to its snapshot.
     * Wrap it in a smart pointer when ownership has to move.
     */
    class CSdlGamepadSource final : public CGamepadSource
    {
      public:
        CSdlGamepadSource();
        ~CSdlGamepadSource() override;

        CSdlGamepadSource(const CSdlGamepadSource &) = delete;
        CSdlGamepadSource &operator=(const CSdlGamepadSource &) = delete;
        CSdlGamepadSource(CSdlGamepadSource &&) = delete;
        CSdlGamepadSource &operator=(CSdlGamepadSource &&) = delete;

        /**
         * @brief Report whether this build contains the SDL2 backend at all.
         *
         * This answers a build-configuration question, not a runtime one: a true
         * result does not imply that a controller is attached.
         */
        [[nodiscard]] static bool isBackendAvailable() noexcept;

        /**
         * @brief Attach to a controller, initializing SDL2 on first use.
         *
         * Any previously opened device is closed first, so open() doubles as the
         * explicit reconnection entry point after an unplug.
         *
         * @param i32JoystickIndex Joystick index to attach to. The default -1
         *        selects the lowest-numbered device SDL recognizes as a game
         *        controller, which is the only one whose mapping is reliable.
         * @return True when a device was attached. On failure the snapshot stays
         *         disconnected and lastError() describes the cause.
         */
        [[nodiscard]] bool open(std::int32_t i32JoystickIndex = -1);

        /**
         * @brief Release the device and any SDL subsystem reference held.
         *
         * Safe to call when nothing is open, and safe to call repeatedly. The
         * published snapshot is reset to the disconnect state so a consumer that
         * keeps reading state() after a close sees a neutral command.
         */
        void close() noexcept;

        /**
         * @brief Poll the attached device and publish a fresh snapshot.
         *
         * @return True when a connected sample was published. False when no
         *         device is open, when the backend is absent, or when the device
         *         was detached, in which case the disconnect snapshot is
         *         published exactly once per detach.
         */
        [[nodiscard]] bool update() override;

        /** @brief Name reported by SDL for the attached device, empty if none. */
        [[nodiscard]] const std::string &deviceName() const noexcept;

        /**
         * @brief Describe the most recent failure, empty when none occurred.
         *
         * Cleared by a successful open(), so it always refers to the current
         * attachment attempt rather than to an older one.
         */
        [[nodiscard]] const std::string &lastError() const noexcept;

      private:
        struct SImpl;
        std::unique_ptr<SImpl> pImpl_;
    };
} // namespace xbox_controller_api
