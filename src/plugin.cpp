#include "plugin.h"

#include "guitar_pro.h"
#include "reaper.h"

#include <array>
#include <memory>
#include <stdexcept>

namespace tnt {

// REAPER runs this periodically 30 times/second so a desync window of 9 is approximately 300ms
static constexpr int DESYNC_WINDOW_SIZE = 9;

static constexpr double DESYNC_THRESHOLD = 0.3;                 // Seconds
static constexpr double MINIMUM_TIME_STEP = 0.001;              // Seconds
static constexpr double MINIMUM_PLAY_RATE_STEP = 0.001;         // Seconds
static constexpr double GUITAR_PRO_CURSOR_JUMP_THRESHOLD = 0.1; // Seconds
static constexpr double LATENCY_COMPENSATION = 0.05;           // Seconds

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

            return;
        }

        if (!m_last_error.empty())
        {
            m_reaper.ShowConsoleMessage("Successfully connected to Guitar Pro process.\n");
            m_last_error = "";
        }
        
        // Ensure REAPER stays in sync while Guitar Pro is playing
        if (m_guitar_pro_state.play_state)
        {
            this->SyncLoopState();
            this->SyncTimeSelection();
            this->SyncPlayPosition();
            this->SyncPlayRate();
        }

        // Allow some control while Guitar Pro and REAPER are both paused
        else if (this->ReaperStoppedOrPaused())
        {
            // Sync loop state
            if (this->GuitarProLoopStateChanged())
            {
                this->SyncLoopState();
            }

            // Sync time selection and cursor
            if (this->GuitarProTimeSelectionChanged() && m_guitar_pro_state.time_selection_end_position > MINIMUM_PLAY_RATE_STEP)
            {
                this->SyncTimeSelection();
                this->SetPlayPosition(m_guitar_pro_state.time_selection_start_position);
            }
            else if (this->GuitarProCursorMoved())
            {
                this->SyncTimeSelection();
                this->SetPlayPosition(m_guitar_pro_state.play_position);
            }

            // Sync play rate
            if (this->GuitarProPlayRateChanged())
            {
                // TODO this doesn't work while paused because the value read from memory only updates at runtime.
                // We need to find a new memory address to get this to work more effectively
                this->SyncPlayRate();
            }
        }

        // Ensure REAPER is playing if Guitar Pro is playing
        this->SyncPlayState();

        // Save previous Guitar Pro state
        m_prev_guitar_pro_state = m_guitar_pro_state;
    }

private:
    void SyncLoopState()
    {
        // Sync the loop state (unless we are playing and there is a count in timer)
        if (m_guitar_pro_state.loop_state && !(m_guitar_pro_state.play_state && m_guitar_pro_state.count_in_state))
        {
            m_reaper.SetRepeat(true);
        }
        else
        {
            m_reaper.SetRepeat(false);
        }
    }

    void SyncTimeSelection()
    {
        // Sync the time selection
        m_reaper.SetTimeSelection(m_guitar_pro_state.time_selection_start_position, m_guitar_pro_state.time_selection_end_position);
    }

    void SyncPlayPosition()
    {
        if (this->GuitarProCursorMoved() && !CompareDoubles(m_reaper.GetPlayPosition(), m_guitar_pro_state.play_position, DESYNC_THRESHOLD))
        {
            // DO NOT SYNC if REAPER is right at the start or end of the loop
            if (this->CompareDoubles(m_reaper.GetPlayPosition(), m_guitar_pro_state.time_selection_start_position, DESYNC_THRESHOLD)
             || this->CompareDoubles(m_reaper.GetPlayPosition(), m_guitar_pro_state.time_selection_end_position, DESYNC_THRESHOLD))
            {
                return;
            }

            // If the guitar pro cursor has jumped, follow the jump
            if (!this->CompareDoubles(m_prev_guitar_pro_state.play_position, m_guitar_pro_state.play_position, GUITAR_PRO_CURSOR_JUMP_THRESHOLD))
            {
                this->SetPlayPosition(m_guitar_pro_state.play_position + LATENCY_COMPENSATION);
            }

            // If a desync occurs for any other reason, get it back in sync
            // Guitar Pro can be a bit inconsistent so this needs to be checked over the course of a few loops though to ensure accuracy
            else if (this->Desync(DESYNC_THRESHOLD))
            {
                this->SetPlayPosition(m_guitar_pro_state.play_position + LATENCY_COMPENSATION);
            }
        }
    }

    void SyncPlayRate()
    {
        // TODO: The running playback rate memory location seems to take a bit to update when playing the song
        // Because of this, the playback rate may register as 0 for a fraction of a second.
        // Look for a better address in Cheat Engine so this can be done faster
        if (m_guitar_pro_state.play_rate > MINIMUM_PLAY_RATE_STEP)
        {
            // If playback rates don't match, sync them
            if (!this->CompareDoubles(m_reaper.GetPlayRate(), m_guitar_pro_state.play_rate, MINIMUM_PLAY_RATE_STEP))
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
            // Stop REAPER if Guitar Pro is currently counting in and the cursor is not moving
            if (m_guitar_pro_state.count_in_state
             && (!this->GuitarProCursorMoved() || (m_guitar_pro_state.time_selection_start_position > MINIMUM_TIME_STEP && m_prev_guitar_pro_state.play_position < MINIMUM_TIME_STEP)))
            {
                // DO NOT cut a loop short
                if (!this->CompareDoubles(m_reaper.GetPlayPosition(), m_guitar_pro_state.time_selection_start_position, MINIMUM_TIME_STEP)
                 && m_reaper.GetPlayPosition() < m_guitar_pro_state.time_selection_end_position)
                {
                    return;
                }

                m_reaper.SetPlayState(ReaperPlayState::STOPPED);
            }

            else if (this->ReaperStoppedOrPaused())
            {
                // If a loop is specified start there
                if (m_guitar_pro_state.time_selection_start_position > MINIMUM_TIME_STEP)
                {
                    this->SetPlayPosition(m_guitar_pro_state.time_selection_start_position + LATENCY_COMPENSATION);
                }

                else
                {
                    this->SetPlayPosition(m_guitar_pro_state.play_position + LATENCY_COMPENSATION);
                }

                m_reaper.SetPlayState(ReaperPlayState::PLAYING);
            }
        }

        // Stop REAPER if Guitar Pro is not playing
        else if (!this->ReaperStoppedOrPaused() && m_prev_guitar_pro_state.play_state)
        {
            // DO NOT cut a time selection short
            if (m_reaper.GetPlayPosition() < m_guitar_pro_state.time_selection_end_position
             && this->CompareDoubles(m_reaper.GetPlayPosition(), m_guitar_pro_state.time_selection_end_position, DESYNC_THRESHOLD)
             && !this->CompareDoubles(m_reaper.GetPlayPosition(), m_guitar_pro_state.time_selection_start_position, DESYNC_THRESHOLD))
            {
                m_guitar_pro_state.play_state = true;
                return;
            }

            m_reaper.SetPlayState(ReaperPlayState::STOPPED);
        }
    }

    bool Desync(const double threshold)
    {
        std::rotate(m_desync_window.rbegin(), m_desync_window.rbegin() + 1, m_desync_window.rend());
        m_desync_window[0] = fabs(m_reaper.GetPlayPosition() - m_guitar_pro_state.play_position);

        // Return false if ANY value in the window is not greater than the threshold
        for (const double value : m_desync_window)
        {
            if (value < threshold)
            {
                return false;
            }
        }

        // Desync has definitively occurred
        return true;
    }
    
    void SetPlayPosition(const double time)
    {
        m_reaper.SetEditCursorPosition(time, false, true);
        m_desync_window.fill(0.0);
    }

    // Returns true if the two values are within epsilon of each other
    bool CompareDoubles(const double val1, const double val2, const double epsilon) const
    {
        return (fabs(val1 - val2) < epsilon);
    }

    bool GuitarProLoopStateChanged() const
    {
        return m_guitar_pro_state.loop_state != m_prev_guitar_pro_state.loop_state;
    }

    bool GuitarProTimeSelectionChanged() const
    {
        return !this->CompareDoubles(m_guitar_pro_state.time_selection_start_position, m_prev_guitar_pro_state.time_selection_start_position, MINIMUM_TIME_STEP)
            || !this->CompareDoubles(m_guitar_pro_state.time_selection_end_position, m_prev_guitar_pro_state.time_selection_end_position, MINIMUM_TIME_STEP);
    }

    bool GuitarProCursorMoved() const
    {
        return !this->CompareDoubles(m_guitar_pro_state.play_position, m_prev_guitar_pro_state.play_position, MINIMUM_TIME_STEP);
    }

    bool GuitarProPlayRateChanged() const
    {
        return !this->CompareDoubles(m_guitar_pro_state.play_rate, m_prev_guitar_pro_state.play_rate, MINIMUM_PLAY_RATE_STEP);
    }

    bool ReaperStoppedOrPaused() const
    {
        switch (m_reaper.GetPlayState())
        {
        case ReaperPlayState::STOPPED:
        case ReaperPlayState::PAUSED:
            return true;
        case ReaperPlayState::PLAYING:
            return false;
        default:
            // This should never happen
            throw std::runtime_error("REAPER is in an invalid play state!\n");
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

    std::array<double, DESYNC_WINDOW_SIZE> m_desync_window = { 0.0 };

    // Keeps track of the last error (prevents spamming the log with errors)
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