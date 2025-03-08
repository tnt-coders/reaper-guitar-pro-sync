#include "guitar_pro.h"

#include "process_reader.h"

#include <utility>

namespace tnt {

// Constants
static constexpr int SAMPLE_RATE = 44100;
static constexpr int PLAY_STATE_FLAG_POSITION = 8;
static constexpr int COUNT_IN_STATE_FLAG_POSITION = 8;
static constexpr int LOOP_STATE_FLAG_POSITION = 8;

struct GuitarPro::Impl final
{
    GuitarProState ReadProcessMemory()
    {
        ProcessReader process_reader(L"GuitarPro.exe", L"GPCore.dll");

        // Addresses and offsets acquired from CheatEngine with Guitar Pro version 8.1.3 - Build 121
        int cursor_location = process_reader.ReadMemoryAddress<int>(0x00A24F80, { 0x18, 0xA0, 0x38, 0x1A8, 0x20, 0x1D8, 0x0 });
        int time_selection_start_location = process_reader.ReadMemoryAddress<int>(0x00A24F80, { 0x18, 0xA0, 0x38, 0x1A8, 0x20, 0x1E0, 0x0 });
        int time_selection_end_location = process_reader.ReadMemoryAddress<int>(0x00A24F80, { 0x18, 0xA0, 0x38, 0x1A8, 0x20, 0x1E0, 0x8 });
        float play_rate = process_reader.ReadMemoryAddress<float>(0x00A24F80, { 0x18, 0xA0, 0x38, 0x80, 0x18, 0x68, 0x28, 0x74 });
        DWORD play_state_flag_container = process_reader.ReadMemoryAddress<DWORD>(0x00A24F80, { 0x18, 0xA0, 0x38, 0x70, 0x30, 0x4E0, 0x0, 0x20, 0x20, 0x0 });
        DWORD count_in_state_flag_container = process_reader.ReadMemoryAddress<DWORD>(0x00A24F80, { 0x18, 0xE0, 0x0, 0x28, 0x10, 0x18, 0x60, 0x0 });
        DWORD loop_state_flag_container = process_reader.ReadMemoryAddress<DWORD>(0x00A24F80, { 0x18, 0xA0, 0x38, 0x70, 0x30, 0x4B8, 0x28, 0x88, 0x80, 0x0 });

        // Make sure the time selection start is always before the end
        // If you drag from right to left in Guitar Pro the values may be flipped
        if (time_selection_start_location > time_selection_end_location)
        {
            std::swap(time_selection_start_location, time_selection_end_location);
        }

        GuitarProState state{};
        state.play_position = static_cast<double>(cursor_location) / SAMPLE_RATE;
        state.time_selection_start_position = static_cast<double>(time_selection_start_location) / SAMPLE_RATE;
        state.time_selection_end_position = static_cast<double>(time_selection_end_location) / SAMPLE_RATE;
        state.play_rate = static_cast<double>(play_rate);
        state.play_state = play_state_flag_container & (1U << PLAY_STATE_FLAG_POSITION);
        state.count_in_state = count_in_state_flag_container & (1U << COUNT_IN_STATE_FLAG_POSITION);
        state.loop_state = loop_state_flag_container & (1U << LOOP_STATE_FLAG_POSITION);

        return state;
    }
};

GuitarPro::GuitarPro()
    : m_impl(std::make_unique<Impl>())
{}

GuitarPro::~GuitarPro() = default;

GuitarProState GuitarPro::ReadProcessMemory()
{
    return m_impl->ReadProcessMemory();
}

}