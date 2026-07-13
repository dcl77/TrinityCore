#include "ScriptMgr.h"
#include "Log.h"

class mod_hello_world_world_script : public WorldScript
{
public:
    mod_hello_world_world_script() : WorldScript("mod_hello_world_world_script") { }

    void OnStartup() override
    {
        TC_LOG_INFO("server.loading", "Hello World from Module!");
    }
};

void Addmod_hello_worldScripts()
{
    new mod_hello_world_world_script();
}
