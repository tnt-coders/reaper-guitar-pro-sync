#define REAPERAPI_IMPLEMENT

#include "plugin.h"

#include <WDL/wdltypes.h> // Must be included before reaper_plugin_functions
#include <reaper_plugin_functions.h>

using namespace tnt;

// Global plugin state required for registration
static PluginState g_plugin_state;
static Plugin g_plugin(g_plugin_state);

// Runs repeatedly on a timer
void MainLoop()
{
    // Static plugin instance
    g_plugin.MainLoop();
}

// REAPER calls this to check guitar pro sync toggle state
int ToggleActionCallback(int command)
{
    if (command != g_plugin_state.command_id)
    {
        // not quite our command_id
        return -1;
    }

    if (g_plugin_state.action_state)
    {
        return 1;
    }

    return 0;
}

// this gets called when guitar pro sync action is run (e.g. from action list)
bool OnAction(KbdSectionInfo* sec, int command, int val, int valhw, int relmode, HWND hwnd)
{
    // check command
    if (command != g_plugin_state.command_id)
    {
        return false;
    }

    // flip state on/off
    g_plugin_state.action_state = !g_plugin_state.action_state;

    if (g_plugin_state.action_state)
    {
        plugin_register("timer", (void*)MainLoop);
    }
    else
    {
        plugin_register("-timer", (void*)MainLoop);
    }

    return true;
}

// when guitar pro sync gets loaded
// function to register guitar pro syncs 'stuff' with REAPER
void Register()
{
    // register action name and get command_id
    g_plugin_state.command_id = plugin_register("custom_action", &g_plugin_state.action);

    // register action on/off state and callback function
    plugin_register("toggleaction", (void*)ToggleActionCallback);

    // register run action/command
    plugin_register("hookcommand2", (void*)OnAction);
}

// shutdown, time to exit
void Unregister()
{
    plugin_register("-custom_action", &g_plugin_state.action);
    plugin_register("-toggleaction", (void*)ToggleActionCallback);
    plugin_register("-hookcommand2", (void*)OnAction);
}

extern "C"
{
    REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hinstance, reaper_plugin_info_t* rec)
    {
        g_plugin_state.hinstance = hinstance;

        if (rec != nullptr && REAPERAPI_LoadAPI(rec->GetFunc) == 0)
        {
            Register();
            return 1;
        }

        Unregister();
        return 0;
    }
}