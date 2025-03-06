#pragma once

#include <memory>

namespace tnt {

struct GuitarProState final
{
    // Play position in seconds
    double play_position = 0.0;

    // Loop start position in seconds
    double loop_start_position = 0.0;

    // Loop end position in seconds
    double loop_end_position = 0.0;

    // Play rate
    double play_rate = 1.0;

    // Pause/play state
    bool play_state = false;

    // Count in state
    bool count_in_state = false;
};    

// Basic API to extract data from Guitar Pro
class GuitarPro final
{
public:
    GuitarPro();
    ~GuitarPro();    

    // Reads program state from memory
    // Throws std::runtime_error on failure
    GuitarProState ReadProcessMemory();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}