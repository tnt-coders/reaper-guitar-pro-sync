#include "reaper.h"

#include <WDL/wdltypes.h> // Must be included before reaper_plugin_functions
#include <reaper_plugin_functions.h>

#include <memory>
#include <string>
#include <stdexcept>

namespace tnt {

// Constants
static constexpr int PRESERVE_PITCH_COMMAND = 40671;

struct Reaper::Impl final
{
    // double GetPlayPosition()
    double GetPlayPosition() const
    {
        return ::GetPlayPosition();
    }
    
    // double Master_GetPlayRate(ReaProject* project)
    double GetPlayRate() const
    {
        return ::Master_GetPlayRate(nullptr);
    }

    // int GetPlayState()
    ReaperPlayState GetPlayState() const
    {
        const int play_state = ::GetPlayState();
        switch (play_state)
        {
        case 0:
            return ReaperPlayState::STOPPED;
        case 1:
            return ReaperPlayState::PLAYING;
        case 2:
            return ReaperPlayState::PAUSED;
        case 4:
            return ReaperPlayState::RECORDING;
        default:
            throw std::runtime_error("GetPlayState: REAPER is in an invalid play state!\n");
        }
    }

    // int GetToggleCommandState(int command_id)
    bool GetToggleCommandState(const ReaperToggleCommand& command) const
    {
        switch (command)
        {
        case ReaperToggleCommand::PRESERVE_PITCH:
            return ::GetToggleCommandState(PRESERVE_PITCH_COMMAND) != 0;
        default:
            // This should never happen
            throw std::runtime_error("GetToggleCommandState: Command not found!\n");
        }
    }

    // void SetEditCurPos(double time, bool moveview, bool seekplay)
    void SetEditCursorPosition(const double time, const bool move_view, const bool seek_play) const
    {
        ::SetEditCurPos(time, move_view, seek_play);
    }

    // void CSurf_OnPlayRateChange(double playrate)
    void SetPlayRate(const double play_rate) const
    {
        ::CSurf_OnPlayRateChange(play_rate);
    }

    // void CSurf_OnStop()
    // void CSurf_OnPlay()
    // void CSurf_OnPause()
    // void CSurf_OnRecord()
    void SetPlayState(const ReaperPlayState& play_state) const
    {
        switch (play_state)
        {
        case ReaperPlayState::STOPPED:
            ::CSurf_OnStop();
            break;
        case ReaperPlayState::PLAYING:
            ::CSurf_OnPlay();
            break;
        case ReaperPlayState::PAUSED:
            ::CSurf_OnPause();
            break;
        case ReaperPlayState::RECORDING:
            ::CSurf_OnRecord();
            break;
        default:
            // This should never happen
            throw std::runtime_error("SetPlayState: Invalid play state!\n");
        }
    }

    // int GetSetRepeat(int val)
    void SetRepeat(const bool repeat) const
    {
        const int val = repeat ? 1 : 0;
        ::GetSetRepeat(val);
    }

    // void GetSet_LoopTimeRange(bool isSet, bool isLoop, double* startOut, double* endOut, bool allowautoseek)
    void SetTimeSelection(const double start_time, const double end_time) const
    {
        // Const cast is used here because we are explicitly setting the value, not getting it.
        ::GetSet_LoopTimeRange(true, false, const_cast<double*>(&start_time), const_cast<double*>(&end_time), false);
    }

    // void ShowConsoleMsg(const char* msg)
    void ShowConsoleMessage(const std::string& message) const
    {
        ::ShowConsoleMsg(message.c_str());
    }

    // void Main_OnCommand(int command, int flag)
    void ToggleCommand(const ReaperToggleCommand& command) const
    {
        switch (command)
        {
        case ReaperToggleCommand::PRESERVE_PITCH:
            ::Main_OnCommand(PRESERVE_PITCH_COMMAND, 0);
            break;
        default:
            // This should never happen
            throw std::runtime_error("ToggleCommand: Command not found!\n");
        }
    }
};

Reaper::Reaper()
    : m_impl(std::make_unique<Impl>())
{}

Reaper::~Reaper() = default;

double Reaper::GetPlayPosition() const
{
    return m_impl->GetPlayPosition();
}

double Reaper::GetPlayRate() const
{
    return m_impl->GetPlayRate();
}

ReaperPlayState Reaper::GetPlayState() const
{
    return m_impl->GetPlayState();
}

bool Reaper::GetToggleCommandState(const ReaperToggleCommand& command) const
{
    return m_impl->GetToggleCommandState(command);
}

void Reaper::SetEditCursorPosition(const double time, const bool move_view, const bool seek_play) const
{
    m_impl->SetEditCursorPosition(time, move_view, seek_play);
}

void Reaper::SetPlayRate(const double play_rate) const
{
    m_impl->SetPlayRate(play_rate);
}

void Reaper::SetPlayState(const ReaperPlayState& play_state) const
{
    m_impl->SetPlayState(play_state);
}

void Reaper::SetRepeat(const bool repeat) const
{
    m_impl->SetRepeat(repeat);
}

void Reaper::SetTimeSelection(const double start_time, const double end_time) const
{
    m_impl->SetTimeSelection(start_time, end_time);
}

void Reaper::ShowConsoleMessage(const std::string& message) const
{
    m_impl->ShowConsoleMessage(message);    
}

void Reaper::ToggleCommand(const ReaperToggleCommand& command) const
{
    m_impl->ToggleCommand(command);
}

}