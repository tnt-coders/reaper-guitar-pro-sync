#pragma once

#include <memory>
#include <string>

namespace tnt {

enum class ReaperPlayState
{
    STOPPED,
    PLAYING,
    PAUSED,
    RECORDING,
    UNKNOWN,
};

// C++ wrapper around C-style REAPER API functions
// Since it is in a class it is capable of holding state
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

    // void SetEditCurPos(double time, bool moveview, bool seekplay)
    void SetEditCursorPosition(const double time, const bool move_view, const bool seek_play) const;

    // void ShowConsoleMsg(const char* msg)
    void ShowConsoleMessage(const std::string& message) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}