#include "plugin.h"

#include "guitar_pro.h"
#include "reaper.h"

#include <memory>
#include <stdexcept>

namespace tnt {

struct Plugin::Impl final {
    Impl(PluginState& plugin_state)
        : m_plugin_state(plugin_state)
    {}

    void MainLoop()
    {
        try
        {
            // Read current Guitar Pro and REAPER states
            m_guitar_pro_state = m_guitar_pro.ReadProcessMemory();
        }
        catch (const std::runtime_error& error)
        {
            if (m_last_error != error.what())
            {
                m_reaper.ShowConsoleMessage(error.what());
                m_last_error = error.what();
            }
        }
        
        // Ensure REAPER stays in sync while Guitar Pro is playing
        if (m_guitar_pro_state.play_state)
        {
            this->SyncTimeSelection();
            this->SyncPlayPosition();
            this->SyncPlayRate();
        }

        // Allow some control while REAPER is paused
        else if (this->ReaperStoppedOrPaused())
        {
            this->SyncTimeSelection();
            this->SyncPlayPosition();
        }

        // Ensure REAPER is playing if Guitar Pro is playing
        this->SyncPlayState();

        // Save previous Guitar Pro state
        m_prev_guitar_pro_state = m_guitar_pro_state;
    }

private:
    void SyncTimeSelection()
    {
        if (this->Desync(m_guitar_pro_state.loop_start_position, m_prev_guitar_pro_state.loop_start_position, 0.001) ||
            this->Desync(m_guitar_pro_state.loop_end_position, m_prev_guitar_pro_state.loop_end_position, 0.001) ||
            m_guitar_pro_state.loop_state != m_prev_guitar_pro_state.loop_state)
        {
            m_reaper.SetTimeSelection(m_guitar_pro_state.loop_start_position, m_guitar_pro_state.loop_end_position, m_guitar_pro_state.loop_state);
        }
    }

    void SyncPlayPosition()
    {
        if (this->GuitarProCursorMovedBack() ||
            this->Desync(m_reaper.GetPlayPosition(), m_guitar_pro_state.play_position, 1))
        {
            m_reaper.SetEditCursorPosition(m_guitar_pro_state.play_position, false, true);
        }
    }

    void SyncPlayRate()
    {
        // TODO: The running playback rate memory location seems to take a bit to update when playing the song
        // Because of this, the playback rate may register as 0 for a fraction of a second.
        // Look for a better address in Cheat Engine so this can be done faster
        if (m_guitar_pro_state.play_rate > 0.001)
        {
            // If playback rates don't match, sync them
            if (this->Desync(m_reaper.GetPlayRate(), m_guitar_pro_state.play_rate, 0.001))
            {
                // Always ensure preserve pitch is set before stretching
                this->EnablePreservePitch();

                // REAPER handles stretching much more efficiently if the song is paused
                m_reaper.SetPlayState(ReaperPlayState::PAUSED);
                m_reaper.SetPlayRate(m_guitar_pro_state.play_rate);
            }
        }
    }

    void SyncPlayState()
    {
        if (m_guitar_pro_state.play_state)
        {
            // Do nothing if Guitar Pro is currently counting in and the cursor is not moving
            if (m_guitar_pro_state.count_in_state && !this->GuitarProCursorMoved())
            {
                return;
            }

            if (this->ReaperStoppedOrPaused())
            {
                // Use previous Guitar Pro play state to reduce latency on startup
                m_reaper.SetEditCursorPosition(m_prev_guitar_pro_state.play_position, false, true);
                m_reaper.SetPlayState(ReaperPlayState::PLAYING);
            }
        }

        // Only stop playback if Guitar Pro was previously playing
        else if (m_prev_guitar_pro_state.play_state && !this->ReaperStoppedOrPaused())
        {
            m_reaper.SetPlayState(ReaperPlayState::PAUSED);
        }
    }

    // Returns true if the two values have drifted further than epsilon out of sync
    bool Desync(const double val1, const double val2, const double epsilon) const
    {
        return !(fabs(val1 - val2) < epsilon);
    }

    bool GuitarProCursorMoved() const
    {
        return this->Desync(m_guitar_pro_state.play_position, m_prev_guitar_pro_state.play_position, 0.001);
    }

    bool GuitarProCursorMovedBack() const
    {
        return GuitarProCursorMoved() && m_guitar_pro_state.play_position < m_prev_guitar_pro_state.play_position;
    }

    bool ReaperStoppedOrPaused() const
    {
        switch (m_reaper.GetPlayState())
        {
        case ReaperPlayState::STOPPED:
        case ReaperPlayState::PAUSED:
            return true;
        case ReaperPlayState::PLAYING:
        case ReaperPlayState::RECORDING:
            return false;
        default:
            // This should never happen
            throw std::runtime_error("REAPER is in an invalid play state!");
        }
    }

    void EnablePreservePitch() const
    {
        // If Preserve Pitch is OFF, enable it
        if (!m_reaper.GetToggleCommandState(ReaperToggleCommand::PRESERVE_PITCH)) {
            m_reaper.ToggleCommand(ReaperToggleCommand::PRESERVE_PITCH);
        }
    }

    PluginState& m_plugin_state;
    GuitarPro m_guitar_pro;
    Reaper m_reaper;

    GuitarProState m_prev_guitar_pro_state;
    GuitarProState m_guitar_pro_state;

    // Keeps track of the last error (prevents spamming the console with errors)
    std::string m_last_error = "";
};

Plugin::Plugin(PluginState& plugin_state)
    : m_impl(std::make_unique<Impl>(plugin_state))
{}

Plugin::~Plugin() = default;

void Plugin::MainLoop()
{
    m_impl->MainLoop();
}

}