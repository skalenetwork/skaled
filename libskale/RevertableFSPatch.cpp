#include "RevertableFSPatch.h"

#include <libethereum/Client.h>

dev::eth::Client* RevertableFSPatch::g_client;

bool RevertableFSPatch::isEnabled() {
    return true;
}