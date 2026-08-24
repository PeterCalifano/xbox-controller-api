/**
 * @file CScriptedGamepadSource.cpp
 * @brief Implements the deterministic replay gamepad source.
 */

#include <xbox_controller_api/CScriptedGamepadSource.h>

namespace xbox_controller_api
{
    void CScriptedGamepadSource::pushFrame(const SGamepadState &strFrame)
    {
        // pushFrame() always represents a connected sample.
        SGamepadState strQueuedFrame_ = strFrame;
        strQueuedFrame_.bConnected_ = true;

        dequePendingFrames_.push_back(strQueuedFrame_);
    }

    void CScriptedGamepadSource::pushDisconnect()
    {
        // Counters are stamped on consumption, so the queued sentinel only has
        // to carry the zeroed values.
        dequePendingFrames_.push_back(MakeDisconnectedState(0U, 0U));
    }

    std::size_t CScriptedGamepadSource::pendingFrameCount() const noexcept
    {
        return dequePendingFrames_.size();
    }

    bool CScriptedGamepadSource::update()
    {
        // No queued frame means the script ended, which leaves the last
        // published snapshot as the still-valid current view.
        if (dequePendingFrames_.empty())
        {
            return false;
        }

        SGamepadState strFrame_ = dequePendingFrames_.front();
        dequePendingFrames_.pop_front();

        // Consuming a frame is what advances script time, so the sequence id
        // Disconnect frames also advance the observable sequence number.
        ++ui64LastSequenceId_;
        strFrame_.ui64SequenceId_ = ui64LastSequenceId_;

        setState(strFrame_);

        return strFrame_.bConnected_;
    }
} // namespace xbox_controller_api
