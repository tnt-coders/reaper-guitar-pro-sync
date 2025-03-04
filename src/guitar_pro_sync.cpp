#include "guitar_pro_sync.h"
#include "reaper_vararg.hpp"
#include <gsl/gsl>
#include <stdexcept>
#include <string>
#include <tlhelp32.h>
#include <vector>
#include <windows.h>

//TODO remove
#include <iostream>
#include <chrono>
#include <thread>

#define STRINGIZE_DEF(x) #x
#define STRINGIZE(x) STRINGIZE_DEF(x)
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
// register main function on timer
// true or false
#define RUN_ON_TIMER true

// confine guitar pro sync to namespace
namespace PROJECT_NAME
{

constexpr int PRESERVE_PITCH_COMMAND = 40671;

// Helper functions
DWORD GetProcessID(const wchar_t* exeName)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnap, &pe))
    {
        do
        {
            if (_wcsicmp(pe.szExeFile, exeName) == 0)
            {
                CloseHandle(hSnap);
                return pe.th32ProcessID;
            }
        } while (Process32Next(hSnap, &pe));
    }

    CloseHandle(hSnap);

    return 0;
}

DWORD64 GetModuleBaseAddress(DWORD processID, const wchar_t* moduleName)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processID);
    if (hSnap == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    MODULEENTRY32W modEntry;
    modEntry.dwSize = sizeof(modEntry);

    if (Module32FirstW(hSnap, &modEntry))
    {
        do
        {
            if (_wcsicmp(modEntry.szModule, moduleName) == 0)
            {
                CloseHandle(hSnap);
                return (DWORD64)modEntry.modBaseAddr;
            }
        } while (Module32NextW(hSnap, &modEntry));
    }

    CloseHandle(hSnap);

    return 0;
}

uintptr_t ReadPointer(HANDLE hProcess, uintptr_t baseAddress, std::vector<DWORD64> offsets)
{
    uintptr_t address = baseAddress;
    uintptr_t tempAddress;

    for (size_t i = 0; i < offsets.size(); i++)
    {
        if (!ReadProcessMemory(hProcess, (LPCVOID)address, &tempAddress, sizeof(tempAddress), nullptr))
        {
            return 0;
        }

        address = tempAddress + offsets[i];
    }

    return address; // Final address pointing to the actual value
}

// Returns true if doubles are within epsilon
bool CompareDouble(const double val1, const double val2, const double epsilon)
{
    return fabs(val1 - val2) < epsilon;
}

bool PlayStatePausedOrStopped(const int reaperPlayState)
{
    // 0 is stopped, 1 is playing, 2 is paused, 4 is recording
    if (reaperPlayState == 0 || reaperPlayState == 2)
    {
        return true;
    }

    return false;
}

// Class for the plugin
class GuitarProSync final
{
public:
    GuitarProSync() = default;
    ~GuitarProSync() = default;

    void Run()
    {
        // Read Guitar Pro data
        if (!this->ReadGuitarProData())
        {
            // Check readError flag to prevent spamming the error message repeatedly
            if (!m_readError)
            {
                ShowConsoleMsg("GuitarProSync: Failed to attach to Guitar Pro process. Check that Guitar Pro is running and that you are using a supported version.\n");
                m_readError = true;
            }
        }
        else
        {
            if (m_readError)
            {
                ShowConsoleMsg("GuitarProSync: Guitar Pro process found!\n");
            }

            // Read was successful, clear the error flag
            m_readError = false;
        }

        // Play state (1=playing, 2=paused, 4=recording)
        const int reaperPlayState = GetPlayState();

        // Ensure REAPER stays in sync while Guitar Pro is playing
        if (m_guitarProPlayState)
        {
            this->SyncPlayPosition();
            this->SyncPlaybackRate();
        }

        // Allow Guitar Pro to move the REAPER cursor even when paused.
        else if (!CompareDouble(m_prevGuitarProPlayPosition, m_guitarProPlayPosition, 0.001))
        {
            // But ONLY if REAPER is ALSO not playing
            if (PlayStatePausedOrStopped(reaperPlayState))
            {
                this->SyncPlayPosition();
            }
        }

        // Ensure REAPER is playing if Guitar Pro is playing
        this->SyncPlayState();

        // Save previous REAPER state
        m_prevReaperPlayState = reaperPlayState;

        // Save previous Guitar Pro state
        m_prevGuitarProPlayPosition = m_guitarProPlayPosition;
        m_prevGuitarProPlayState = m_guitarProPlayState;
    }

private:
    bool ReadGuitarProData()
    {
        DWORD processID = GetProcessID(L"GuitarPro.exe");
        HANDLE hProcess = OpenProcess(PROCESS_VM_READ, FALSE, processID);
        if (!hProcess) {
            return false;
        }

        DWORD64 moduleBase = GetModuleBaseAddress(processID, L"GPCore.dll");

        // Read playback rate
        m_guitarProPlaybackRate = [&] {
        
            // Base Address from Cheat Engine (GPCore.dll + 0x00A24F80)
            DWORD64 baseAddress = moduleBase + 0x00A24F80;

            // Offset Chain (from Cheat Engine)
            std::vector<DWORD64> offsets = { 0x18, 0xA0, 0x38, 0x80, 0x18, 0x68, 0x28, 0x74 };

            // Resolve the pointer
            DWORD64 finalAddress = ReadPointer(hProcess, baseAddress, offsets);

            // Read the actual value at the final address
            float value;
            ReadProcessMemory(hProcess, (LPCVOID)finalAddress, &value, sizeof(value), nullptr);

            return static_cast<double>(value);
        }();

        // Read play position
        m_guitarProPlayPosition = [&] {

            // Base Address from Cheat Engine (GPCore.dll + 0x00A24F80)
            DWORD64 baseAddress = moduleBase + 0x00A24F80;

            // Offset Chain (from Cheat Engine)
            std::vector<DWORD64> offsets = { 0x18, 0xA0, 0x38, 0x1A8, 0x20, 0x1D8, 0x0 };

            // Resolve the pointer
            DWORD64 finalAddress = ReadPointer(hProcess, baseAddress, offsets);

            // Read the actual value at the final address
            int value;
            ReadProcessMemory(hProcess, (LPCVOID)finalAddress, &value, sizeof(value), nullptr);

            return static_cast<double>(value)/m_sampleRate;
        }();

        // Read play state
        m_guitarProPlayState = [&] {
            // Base Address from Cheat Engine (GPCore.dll + 0x00A24F80)
            DWORD64 baseAddress = moduleBase + 0x00A24F80;

            // Offset Chain (from Cheat Engine)
            std::vector<DWORD64> offsets = { 0x18, 0xA0, 0x38, 0x70, 0x30, 0x4E0, 0x0, 0x20, 0x20, 0x0 };

            // Resolve the pointer
            DWORD64 finalAddress = ReadPointer(hProcess, baseAddress, offsets);

            // Read the actual value at the final address
            DWORD value;
            ReadProcessMemory(hProcess, (LPCVOID)finalAddress, &value, sizeof(value), nullptr);

            constexpr int FLAG_POSITION = 8;

            return value & (1U << FLAG_POSITION);
        }();
        
        CloseHandle(hProcess);

        return true;
    }

    void SyncPlayPosition()
    {
        const double reaperPlayPosition = GetPlayPosition();

        // If cursor locations don't match, sync them
        if (m_guitarProPlayPosition < m_prevGuitarProPlayPosition || !CompareDouble(reaperPlayPosition, m_guitarProPlayPosition, 1))
        {
            SetEditCurPos(m_guitarProPlayPosition, false, true);
        }
    }

    void SyncPlaybackRate()
    {
        const double reaperPlaybackRate = Master_GetPlayRate(nullptr);

        //TODO: The running playback rate memory location seems to take a bit to update when playing the song
        // Because of this, the playback rate may register as 0 for a fraction of a second.
        // Look for a better address in Cheat Engine so this can be done faster
        if (m_guitarProPlaybackRate > 0.001)
        {
            // If playback rates don't match, sync them
            if (!CompareDouble(reaperPlaybackRate, m_guitarProPlaybackRate, 0.001))
            {
                // If Preserve Pitch is OFF, enable it
                if (GetToggleCommandState(PRESERVE_PITCH_COMMAND) == 0) {
                    Main_OnCommand(PRESERVE_PITCH_COMMAND, 0);
                }
    
                // REAPER handles stretching much more efficiently if the song is paused
                CSurf_OnPause();
                CSurf_OnPlayRateChange(m_guitarProPlaybackRate);
                SetEditCurPos(m_guitarProPlayPosition, false, true);
                CSurf_OnPlay();
            }
        }
    }

    void SyncPlayState()
    {
        // Play state (1=playing, 2=paused, 4=recording)
        const int reaperPlayState = GetPlayState();

        if (m_guitarProPlayState)
        {
            if (!m_prevGuitarProPlayState)
            {
                // Set REAPER state to match Guitar Pro (use previous cursor position to reduce latency)
                SetEditCurPos(m_prevGuitarProPlayPosition, false, true);
            }

            if (reaperPlayState != 1)
            {
                CSurf_OnPlay();
            }
        }
        else if (m_prevGuitarProPlayState && !PlayStatePausedOrStopped(reaperPlayState))
        {
            CSurf_OnStop();
        }
    }

private:
    // Seems to always be 44100 for guitar pro
    const int m_sampleRate = 44100;

    // Previous REAPER state
    int m_prevReaperPlayState = 2;

    // Previous Guitar Pro state
    double m_prevGuitarProPlayPosition = 0.0;
    bool m_prevGuitarProPlayState = false;

    // Current Guitar Pro state
    double m_guitarProPlayPosition = 0.0;
    double m_guitarProPlaybackRate = 0.0;
    bool m_guitarProPlayState = false;

    // Delay sync if REAPER is playing but Guitar Pro is paused
    bool m_delaySync = false;

    // Tracks if a read error is currently occurring
    bool m_readError = false;
};

// some global non-const variables
// the necessary 'evil'
int command_id{0};
bool toggle_action_state{false};
constexpr auto command_name = "TNT_GUITAR_PRO_SYNC_COMMAND";
constexpr auto action_name = "TNT: Toggle Guitar Pro sync";
custom_action_register_t action = {0, command_name, action_name, nullptr};

GuitarProSync guitarProSync;

// hInstance is declared in header file guitar_pro_sync.hpp
// defined here
REAPER_PLUGIN_HINSTANCE hInstance{nullptr}; // used for dialogs, if any

// the main function of guitar pro sync
// gets called via callback or timer
void MainFunctionOfGuitarProSync()
{
    guitarProSync.Run();
}

// c++11 trailing return type syntax
// REAPER calls this to check guitar pro sync toggle state
auto ToggleActionCallback(int command) -> int
{
    if (command != command_id)
    {
        // not quite our command_id
        return -1;
    }
    if (toggle_action_state) // if toggle_action_state == true
        return 1;
    return 0;
}

// this gets called when guitar pro sync action is run (e.g. from action list)
bool OnAction(KbdSectionInfo* sec, int command, int val, int valhw, int relmode, HWND hwnd)
{
    // treat unused variables 'pedantically'
    (void)sec;
    (void)val;
    (void)valhw;
    (void)relmode;
    (void)hwnd;

    // check command
    if (command != command_id)
        return false;

    // depending on RUN_ON_TIMER #definition,
    // register guitar pro syncs main function to timer
    if (RUN_ON_TIMER) // RUN_ON_TIMER is true or false
    {
        // flip state on/off
        toggle_action_state = !toggle_action_state;

        if (toggle_action_state) // if toggle_action_state == true
        {
            // "reaper.defer(main)"
            plugin_register("timer", (void*)MainFunctionOfGuitarProSync);
        }
        else
        {
            // "reaper.atexit(shutdown)"
            plugin_register("-timer", (void*)MainFunctionOfGuitarProSync);
            // shutdown stuff
        }
    }
    else
    {
        // else call main function once
        MainFunctionOfGuitarProSync();
    }

    return true;
}

// when guitar pro sync gets loaded
// function to register guitar pro syncs 'stuff' with REAPER
void Register()
{
    // register action name and get command_id
    command_id = plugin_register("custom_action", &action);

    // register action on/off state and callback function
    if (RUN_ON_TIMER)
        plugin_register("toggleaction", (void*)ToggleActionCallback);

    // register run action/command
    plugin_register("hookcommand2", (void*)OnAction);
}

// shutdown, time to exit
// modern C++11 syntax
auto Unregister() -> void
{
    plugin_register("-custom_action", &action);
    plugin_register("-toggleaction", (void*)ToggleActionCallback);
    plugin_register("-hookcommand2", (void*)OnAction);
}

} // namespace PROJECT_NAME