#pragma once

#include <memory>
#include <string>

namespace tnt {

enum class ReaperPlayState
{
    STOPPED,
    PLAYING,
    PAUSED,
};

enum class ReaperToggleCommand
{
    PRESERVE_PITCH,
};

// C++ wrapper around C-style REAPER API functions
// Since it is in a class it is also capable of holding state
class Reaper final
{
public:
    Reaper();
    ~Reaper();

    // double GetPlayPosition()
    double GetPlayPosition() const;
    
    // double Master_GetPlayRate(ReaProject* project)
    double GetPlayRate() const;

    // int GetPlayState()
    ReaperPlayState GetPlayState() const;

    // int GetToggleCommandState(int command_id)
    bool GetToggleCommandState(const ReaperToggleCommand& command) const;

    // void SetEditCurPos(double time, bool moveview, bool seekplay)
    void SetEditCursorPosition(const double time, const bool move_view, const bool seek_play) const;

    // void CSurf_OnPlayRateChange(double playrate)
    void SetPlayRate(const double play_rate) const;

    // void CSurf_OnStop()
    // void CSurf_OnPlay()
    // void CSurf_OnPause()
    // void CSurf_OnRecord()
    void SetPlayState(const ReaperPlayState& play_state) const;

    // int GetSetRepeat(int val)
    void SetRepeat(const bool repeat) const;

    // void GetSet_LoopTimeRange(bool isSet, bool isLoop, double* startOut, double* endOut, bool allowautoseek)
    void SetTimeSelection(const double start_time, const double end_time) const;

    // void ShowConsoleMsg(const char* msg)
    void ShowConsoleMessage(const std::string& message) const;

    // void Main_OnCommand(int command, int flag)
    void ToggleCommand(const ReaperToggleCommand& command) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}