#pragma once

#include <memory>
#include <reaper_plugin.h>

namespace tnt {

struct PluginState final
{
    REAPER_PLUGIN_HINSTANCE hinstance = nullptr;
    int command_id = 0;
    bool action_state = false;
    custom_action_register_t action = {0, "TNT_GUITAR_PRO_SYNC_COMMAND", "TNT: Toggle Guitar Pro sync", nullptr};
};
    
// Class for the plugin
class Plugin final
{
public:
    Plugin(PluginState& plugin_state);
    ~Plugin();

    void MainLoop();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}