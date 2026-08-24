/**
 * @file CGamepadSource.h
 * @brief Abstract poll-model interface shared by every gamepad backend.
 * @details update() refreshes the current sample when one is available. The
 *          most recent sample remains available until the next update.
 */
#pragma once

#include <xbox_controller_api/SGamepadState.h>

namespace xbox_controller_api
{
    /**
     * @brief Base class for a pollable source of normalized controller samples.
     *
     * The caller controls the polling rate. Implementations publish normalized
     * @ref SGamepadState values and do not apply deadzones or edge detection.
     */
    class CGamepadSource
    {
      public:
        CGamepadSource() = default;
        virtual ~CGamepadSource() = default;

        /**
         * @brief Refresh the cached snapshot from the underlying device.
         *
         * A disconnected device must publish @ref MakeDisconnectedState rather
         * than leave the previous control values in place.
         *
         * @return True when a connected sample was published, false when no new
         *         connected sample is available.
         */
        [[nodiscard]] virtual bool update() = 0;

        /**
         * @brief Read the most recently published snapshot.
         *
         * The reference remains valid for the lifetime of the source.
         * @return Current source snapshot.
         */
        [[nodiscard]] const SGamepadState &state() const noexcept
        {
            return strGamepadState_;
        }

        /**
         * @brief Report the connection state of the current snapshot.
         * @return True when the source reports an attached controller.
         */
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
         * Backends prepare a complete sample before publishing it.
         */
        void setState(const SGamepadState &strGamepadState) noexcept
        {
            strGamepadState_ = strGamepadState;
        }

      private:
        SGamepadState strGamepadState_{};
    };
} // namespace xbox_controller_api
