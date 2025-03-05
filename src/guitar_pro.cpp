#include "guitar_pro.h"

#include "process_reader.h"

namespace tnt {

struct GuitarPro::Impl final
{
    void ReadProcessMemory()
    {
        ProcessReader process_reader(L"GuitarPro.exe", L"GPCore.dll");

        // Addresses and offsets acquired from CheatEngine with Guitar Pro version 8.1.3 - Build 121
        int cursor_location = process_reader.ReadMemoryAddress<int>(0x00A24F80, { 0x18, 0xA0, 0x38, 0x1A8, 0x20, 0x1D8, 0x0 });
        float play_rate = process_reader.ReadMemoryAddress<float>(0x00A24F80, { 0x18, 0xA0, 0x38, 0x80, 0x18, 0x68, 0x28, 0x74 });
        DWORD flags = process_reader.ReadMemoryAddress<DWORD>(0x00A24F80, { 0x18, 0xA0, 0x38, 0x70, 0x30, 0x4E0, 0x0, 0x20, 0x20, 0x0 });

        m_play_position = static_cast<double>(cursor_location) / SAMPLE_RATE;
        m_play_rate = static_cast<double>(play_rate);
        m_play_state = flags & (1U << PLAY_STATE_FLAG_POSITION);
    }

    double GetPlayPosition()
    {
        return m_play_position;
    }

    double GetPlayRate()
    {
        return m_play_rate; 
    }

    bool GetPlayState()
    {
        return m_play_state;
    }

private:
    // Constants
    static constexpr int SAMPLE_RATE = 44100;
    static constexpr int PLAY_STATE_FLAG_POSITION = 8;

    double m_play_position = 0.0;
    double m_play_rate = 1.0;
    bool m_play_state = false;
};

GuitarPro::GuitarPro()
    : m_impl(std::make_unique<Impl>())
{}

GuitarPro::~GuitarPro() = default;

void GuitarPro::ReadProcessMemory()
{
    m_impl->ReadProcessMemory();
}

double GuitarPro::GetPlayPosition() const
{
    return m_impl->GetPlayPosition();
}

double GuitarPro::GetPlayRate() const
{
    return m_impl->GetPlayRate();
}

bool GuitarPro::GetPlayState() const
{
    return m_impl->GetPlayState();
}

}