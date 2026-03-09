#include "switch_network.h"

#ifdef __SWITCH__

extern "C" {
#include <switch/result.h>
#include <switch/services/nifm.h>
#include <switch/services/ssl.h>
#include <switch/runtime/devices/socket.h>
}

static bool s_switch_network_initialized = false;
static bool s_switch_network_ready = false;

bool switch_network_init()
{
    if (s_switch_network_initialized)
        return s_switch_network_ready;

    s_switch_network_initialized = true;
    s_switch_network_ready = !R_FAILED(socketInitializeDefault());
    return s_switch_network_ready;
}

bool switch_network_ready()
{
    return s_switch_network_ready;
}

void switch_network_shutdown()
{
    if (!s_switch_network_initialized)
        return;

    if (s_switch_network_ready)
    {
        socketExit();
    }

    s_switch_network_ready = false;
    s_switch_network_initialized = false;
}

#endif
