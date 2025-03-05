#include "reaper.h"

#include <WDL/wdltypes.h> // Must be included before reaper_plugin_functions
#include <reaper_plugin_functions.h>

#include <memory>
#include <string>

namespace tnt {

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
            return ReaperPlayState::UNKNOWN;
        }
    }

    // void SetEditCurPos(double time, bool moveview, bool seekplay)
    void SetEditCursorPosition(const double time, const bool move_view, const bool seek_play) const
    {
        ::SetEditCurPos(time, move_view, seek_play);
    }

    // void ShowConsoleMsg(const char* msg)
    void ShowConsoleMessage(const std::string& message) const
    {
        ::ShowConsoleMsg(message.c_str());
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

void Reaper::SetEditCursorPosition(const double time, const bool move_view, const bool seek_play) const
{
    m_impl->SetEditCursorPosition(time, move_view, seek_play);
}

void Reaper::ShowConsoleMessage(const std::string& message) const
{
    m_impl->ShowConsoleMessage(message);    
}

}