#pragma once

#include <memory>

namespace tnt {

// Basic API to extract data from Guitar Pro
class GuitarPro final
{
public:
    GuitarPro();
    ~GuitarPro();    

    // Refreshes all data from program memory
    // Throws std::runtime_error on failure
    void ReadProcessMemory();

    // Gets the play cursor position in seconds
    double GetPlayPosition() const;

    // Gets the current playback speed
    double GetPlayRate() const;

    // Gets the current pause/play state
    bool GetPlayState() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}