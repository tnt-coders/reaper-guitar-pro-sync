#include "guitar_pro_sync.h"
#include "reaper_vararg.hpp"
#include <gsl/gsl>
#include <string>
#include <tlhelp32.h>
#include <vector>
#include <windows.h>

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

// some global non-const variables
// the necessary 'evil'
int command_id{0};
bool toggle_action_state{false};
constexpr auto command_name = "TNT_" STRINGIZE(PROJECT_NAME) "_COMMAND";
constexpr auto action_name = "tnt: " STRINGIZE(PROJECT_NAME) " (on/off)";
custom_action_register_t action = {0, command_name, action_name, nullptr};

// hInstance is declared in header file guitar_pro_sync.hpp
// defined here
REAPER_PLUGIN_HINSTANCE hInstance{nullptr}; // used for dialogs, if any

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

// the main function of guitar pro sync
// gets called via callback or timer
void MainFunctionOfGuitarProSync()
{
    static double prevEditCurPos = 0.0;

    DWORD processID = GetProcessID(L"GuitarPro.exe");
    HANDLE hProcess = OpenProcess(PROCESS_VM_READ, FALSE, processID);
    if (!hProcess) {
        ShowConsoleMsg("Failed to open GuitarPro.exe\n");
        return;
    }

    DWORD64 moduleBase = GetModuleBaseAddress(processID, L"GuitarPro.exe");

    // Base Address from Cheat Engine (GuitarPro.exe + 0x02FB0468)
    DWORD64 baseAddress = moduleBase + 0x02FB0468;

    // Offset Chain (from Cheat Engine)
    std::vector<DWORD64> offsets = { 0x10, 0x28, 0x30, 0x50, 0x1D8, 0x0 };

    // Resolve the pointer
    DWORD64 finalAddress = ReadPointer(hProcess, baseAddress, offsets);

    // Read the actual value at the final address
    int value;
    ReadProcessMemory(hProcess, (LPCVOID)finalAddress, &value, sizeof(value), nullptr);

    double editCurPos = static_cast<double>(value)/44100;

    std::string message = std::to_string(editCurPos) + "\n";
    ShowConsoleMsg(message.c_str());

    CloseHandle(hProcess);

    //SetEditCurPos(editCurPos, true, true);
    //ShowConsoleMsg("hello, world\n");
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