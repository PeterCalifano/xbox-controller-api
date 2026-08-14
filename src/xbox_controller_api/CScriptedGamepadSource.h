/**
 * @file CScriptedGamepadSource.h
 * @brief Deterministic gamepad source that replays a queued script of samples.
 * @details Exists so consumer logic, edge detection, and deadzone policy can be
 *          exercised without a controller, a driver, or SDL. It is also the
 *          replay backend for recorded sessions, which is why it lives in the
 *          shipped library rather than in the test tree.
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
     * Each update() consumes one queued frame in FIFO order, so a test drives
     * time explicitly instead of waiting on real hardware. The class is final
     * because it adds no extension point of its own.
     */
    class CScriptedGamepadSource final : public CGamepadSource
    {
      public:
        /**
         * @brief Queue one connected sample for a later update().
         *
         * The queued copy is forced to a connected state: pushFrame() means "a
         * device answered with these values", while a detach is expressed only
         * through pushDisconnect(). The sequence id is assigned on consumption
         * and any value set here is overwritten; the timestamp is preserved
         * verbatim so replays stay bit-for-bit reproducible.
         *
         * @param strFrame Sample values to replay.
         */
        void pushFrame(const SGamepadState &strFrame);

        /**
         * @brief Queue an unplug event for a later update().
         *
         * The queued frame is the canonical zeroed snapshot, so a replayed
         * disconnect is indistinguishable from one a real backend publishes.
         */
        void pushDisconnect();

        /** @brief Number of queued frames not yet consumed by update(). */
        [[nodiscard]] std::size_t pendingFrameCount() const noexcept;

        /**
         * @brief Consume the next queued frame and publish it.
         *
         * An exhausted queue is deliberately not treated as a disconnect: the
         * previously published snapshot is left untouched so a caller can tell
         * "the script ended" from "the device went away".
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
