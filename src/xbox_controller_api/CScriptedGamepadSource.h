/**
 * @file CScriptedGamepadSource.h
 * @brief Deterministic gamepad source that replays a queued script of samples.
 * @details Replays samples without a controller, driver, or SDL dependency.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>

#include <xbox_controller_api/CGamepadSource.h>
#include <xbox_controller_api/SGamepadState.h>

namespace xbox_controller_api
{
    /**
     * @brief Gamepad source whose samples come from a caller-supplied queue.
     *
     * Each update() consumes one queued frame in FIFO order.
     */
    class CScriptedGamepadSource final : public CGamepadSource
    {
      public:
        /**
         * @brief Queue one connected sample for a later update().
         *
         * The queued copy is marked connected. update() assigns its sequence id
         * and preserves its timestamp.
         *
         * @param strFrame Sample values to replay.
         */
        void pushFrame(const SGamepadState &strFrame);

        /**
         * @brief Queue an unplug event for a later update().
         *
         * The queued frame is the canonical zeroed disconnected snapshot.
         */
        void pushDisconnect();

        /** @brief Number of queued frames not yet consumed by update(). */
        [[nodiscard]] std::size_t pendingFrameCount() const noexcept;

        /**
         * @brief Consume the next queued frame and publish it.
         *
         * An exhausted queue leaves the previous snapshot unchanged.
         *
         * @return True when a connected frame was published; false when the
         *         queue was empty or the consumed frame was a disconnect.
         */
        [[nodiscard]] bool update() override;

      private:
        std::deque<SGamepadState> dequePendingFrames_;
        std::uint64_t ui64LastSequenceId_ = 0U;
    };
} // namespace xbox_controller_api
