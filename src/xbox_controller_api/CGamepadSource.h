/**
 * @file CGamepadSource.h
 * @brief Abstract poll-model interface shared by every gamepad backend.
 * @details Fixes the polling contract used by the SDL backend, the scripted
 *          replay source, and any future source: the caller drives time by
 *          calling update(), and the most recent snapshot stays readable until
 *          the next call. Nothing here touches a device, so this header stays
 *          usable in builds without any backend compiled in.
 */
#pragma once

#include <xbox_controller_api/SGamepadState.h>

namespace xbox_controller_api
{
    /**
     * @brief Base class for a pollable source of normalized controller samples.
     *
     * The model is deliberately poll-only rather than callback-driven: control
     * loops already have a cadence, and a pull interface lets them sample at
     * that cadence without a background thread, a queue, or reentrancy rules.
     *
     * Implementations own exactly one responsibility, namely turning device
     * data into an @ref SGamepadState. They apply no deadzone and no edge
     * detection, both of which are explicit consumer-side steps.
     *
     * Copy and move operations are protected so a derived source can never be
     * sliced through a reference to this base while remaining copyable by its
     * own derived class, per C++ Core Guidelines C.67.
     */
    class CGamepadSource
    {
      public:
        CGamepadSource() = default;
        virtual ~CGamepadSource() = default;

        /**
         * @brief Refresh the cached snapshot from the underlying device.
         *
         * Implementations must always leave a well-defined snapshot behind,
         * publishing @ref MakeDisconnectedState when the device disappears
         * rather than retaining stale deflections.
         *
         * @return True when a connected sample was published, false when no new
         *         connected sample is available.
         */
        [[nodiscard]] virtual bool update() = 0;

        /**
         * @brief Read the most recently published snapshot.
         *
         * The reference stays valid for the lifetime of the source, and its
         * contents change only inside update().
         */
        [[nodiscard]] const SGamepadState &state() const noexcept
        {
            return strGamepadState_;
        }

        /** @brief Convenience accessor mirroring SGamepadState::bConnected_. */
        [[nodiscard]] bool connected() const noexcept
        {
            return strGamepadState_.bConnected_;
        }

      protected:
        CGamepadSource(const CGamepadSource &) = default;
        CGamepadSource &operator=(const CGamepadSource &) = default;
        CGamepadSource(CGamepadSource &&) = default;
        CGamepadSource &operator=(CGamepadSource &&) = default;

        /**
         * @brief Publish a complete snapshot to callers of state().
         *
         * Backends assemble a sample locally and commit it in one call, so a
         * partially updated snapshot is never observable.
         */
        void setState(const SGamepadState &strGamepadState) noexcept
        {
            strGamepadState_ = strGamepadState;
        }

      private:
        SGamepadState strGamepadState_{};
    };
} // namespace xbox_controller_api
