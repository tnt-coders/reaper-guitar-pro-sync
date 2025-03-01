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
#define API_ID MYAPI
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

// Class for the plugin
class GuitarProSync final
{
public:
    GuitarProSync() = default;
    ~GuitarProSync() = default;

    void Run()
    {
        try
        {
            // Reads REAPER and Guitar Pro data
            this->ReadApplicationData();
 
            // If guitar pro is playing then enable Guitar Pro control
            m_guitarProControl = m_guitarProPlaybackRate > 0.001;

            // If Guitar Pro control is enabled sync with Guitar Pro
            if (m_guitarProControl)
            {
                if (!m_prevGuitarProControl)
                {
                    // If guitar pro control was not previously on, save original settings
                    m_origReaperPreservePitch = GetToggleCommandState(PRESERVE_PITCH_COMMAND) == 1;
                    m_origReaperCursorPosition = GetCursorPosition();
                    m_origReaperPlaybackRate = Master_GetPlayRate(nullptr);

                    // If Preserve Pitch is OFF, enable it
                    if (!m_origReaperPreservePitch)
                    {
                        Main_OnCommand(PRESERVE_PITCH_COMMAND, 0);
                    }

                    // Set REAPER state to match Guitar Pro (use previous cursor position to reduce latency)
                    SetEditCurPos(m_prevGuitarProPlayPosition, false, true);
                    CSurf_OnPlayRateChange(m_guitarProPlaybackRate);
                    CSurf_OnPlay();
                }

                // Ensure REAPER stays in sync
                this->SyncCursor();
                this->SyncPlaybackRate();
                this->SyncPlayState();
            }

            // If Guitar Pro control is NOT enabled, but it was previously, restore saved settings
            else if (m_prevGuitarProControl)
            {
                // If Preserve Pitch is OFF, enable it
                if (GetToggleCommandState(PRESERVE_PITCH_COMMAND) == 0) {
                    Main_OnCommand(PRESERVE_PITCH_COMMAND, 0);
                }

                CSurf_OnStop(); // Stop playback
                CSurf_OnPlayRateChange(m_origReaperPlaybackRate); // Restore playback rate
                SetEditCurPos(m_origReaperCursorPosition, false, true); // Restore edit cursor position

                // Reset preserve pitch
                if (!m_origReaperPreservePitch)
                {
                    Main_OnCommand(PRESERVE_PITCH_COMMAND, 0);
                }
            }

            // Save last guitar pro control state
            m_prevGuitarProControl = m_guitarProControl;
        }
        catch(const std::exception& e)
        {
            ShowConsoleMsg(e.what());
        }
    }

private:
    void ReadApplicationData()
    {
        DWORD processID = GetProcessID(L"GuitarPro.exe");
        HANDLE hProcess = OpenProcess(PROCESS_VM_READ, FALSE, processID);
        if (!hProcess) {
            throw std::exception("Failed to open GuitarPro.exe\n");
        }

        DWORD64 moduleBase = GetModuleBaseAddress(processID, L"GPCore.dll");

        // Read playback rate
        m_reaperPlaybackRate = Master_GetPlayRate(nullptr);
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

        // Read cursor position
        m_reaperPlayPosition = GetPlayPosition();
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

        m_reaperPlayState = GetPlayState();
        
        CloseHandle(hProcess);
    }

    void SyncCursor()
    {
        // If cursor locations don't match, sync them
        if (!CompareDouble(m_reaperPlayPosition, m_guitarProPlayPosition, 1)
            || m_guitarProPlayPosition < m_prevGuitarProPlayPosition)
        {
            SetEditCurPos(m_guitarProPlayPosition, false, true);
        }

        m_prevGuitarProPlayPosition = m_guitarProPlayPosition;
    }

    void SyncPlaybackRate()
    {
        // If playback rates don't match, sync them
        if (!CompareDouble(m_reaperPlaybackRate, m_guitarProPlaybackRate, 0.001))
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

    void SyncPlayState()
    {
        // If guitar pro control is enabled, playstate is always 1 (playing)
        if (m_reaperPlayState != 1)
        {
            CSurf_OnPlay();
        }
    }

private:
    // Seems to always be 44100 for guitar pro
    const int m_sampleRate = 44100;

    // Guitar Pro control state
    bool m_prevGuitarProControl = false;
    bool m_guitarProControl = false;

    // Preserve pitch seting
    bool m_origReaperPreservePitch = false;
    
    // Cursor position
    double m_origReaperCursorPosition = 0.0;

    // Play position
    double m_reaperPlayPosition = 0.0;
    double m_prevGuitarProPlayPosition = 0.0;
    double m_guitarProPlayPosition = 0.0;

    // Playback rate
    double m_origReaperPlaybackRate = 1.0;
    double m_reaperPlaybackRate = 1.0;
    double m_prevGuitarProPlaybackRate = 0.0;
    double m_guitarProPlaybackRate = 0.0;

    // Play state (1=playing, 2=paused, 4=recording)
    int m_reaperPlayState = 2;
};

// some global non-const variables
// the necessary 'evil'
int command_id{0};
bool toggle_action_state{false};
constexpr auto command_name = "TNT_" STRINGIZE(PROJECT_NAME) "_COMMAND";
constexpr auto action_name = "tnt: " STRINGIZE(PROJECT_NAME) " (on/off)";
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

// definition string for example API function
auto reascript_api_function_example_defstring =
    "int" // return type
    "\0"  // delimiter ('separator')
    // input parameter types
    "int,bool,double,const char*,int,const int*,double*,char*,int"
    "\0"
    // input parameter names
    "whole_number,boolean_value,decimal_number,string_of_text,"
    "string_of_text_sz,input_parameterInOptional,"
    "return_valueOutOptional,"
    "return_stringOutOptional,return_stringOutsz"
    "\0"
    "help text for myfunction\n"
    "If optional input parameter is provided, produces optional return "
    "value.\n"
    "If boolean is true, copies input string to optional output string.\n";

// example api function
int ReaScriptAPIFunctionExample(
    int whole_number,
    bool boolean_value,
    double decimal_number,
    const char* string_of_text,
    int string_of_text_sz,
    const int* input_parameterInOptional,
    double* return_valueOutOptional,
    char* return_stringOutOptional,
    int return_string_sz
)
{
    // if optional integer is provided
    if (input_parameterInOptional != nullptr)
    {
        // assign value to deferenced output pointer
        *return_valueOutOptional =
            // by making this awesome calculation
            (*input_parameterInOptional + whole_number + decimal_number);
    }

    // lets conditionally produce optional output string
    if (boolean_value)
    {
        // copy string_of_text to return_stringOutOptional
        // *_sz is length/size of zero terminated string (C-style char array)
        memcpy(return_stringOutOptional, string_of_text, min(return_string_sz, string_of_text_sz) * sizeof(char));
    }
    return whole_number * whole_number;
}

auto defstring_GetVersion =
    "void" // return type
    "\0"   // delimiter ('separator')
    // input parameter types
    "int*,int*,int*,int*,char*,int"
    "\0"
    // input parameter names
    "majorOut,minorOut,patchOut,tweakOut,commitOut,commitOut_sz"
    "\0"
    "returns version numbers of guitar pro sync\n";

void GetVersion(int* majorOut, int* minorOut, int* patchOut, int* tweakOut, char* commitOut, int commitOut_sz)
{
    *majorOut = PROJECT_VERSION_MAJOR;
    *minorOut = PROJECT_VERSION_MINOR;
    *patchOut = PROJECT_VERSION_PATCH;
    *tweakOut = PROJECT_VERSION_TWEAK;
    const char* commit = STRINGIZE(PROJECT_VERSION_COMMIT);
    std::copy(commit, commit + min(commitOut_sz - 1, (int)strlen(commit)), commitOut);
    commitOut[min(commitOut_sz - 1, (int)strlen(commit))] = '\0'; // Ensure null termination
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

    // register the API function example
    // function, definition string and function 'signature'
    plugin_register("API_" STRINGIZE(API_ID)"_ReaScriptAPIFunctionExample", (void*)ReaScriptAPIFunctionExample);
    plugin_register(
        "APIdef_" STRINGIZE(API_ID)"_ReaScriptAPIFunctionExample", (void*)reascript_api_function_example_defstring
    );
    plugin_register("APIvararg_" STRINGIZE(API_ID)"_ReaScriptAPIFunctionExample", (void*)&InvokeReaScriptAPI<&ReaScriptAPIFunctionExample>);

    plugin_register("API_" STRINGIZE(API_ID)"_GetVersion", (void*)GetVersion);
    plugin_register("APIdef_" STRINGIZE(API_ID)"_GetVersion", (void*)defstring_GetVersion);
    plugin_register("APIvararg_" STRINGIZE(API_ID)"_GetVersion", (void*)&InvokeReaScriptAPI<&GetVersion>);
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