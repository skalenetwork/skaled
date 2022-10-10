#include "ContractStorageLimitPatch.h"

#include <libethereum/Client.h>

dev::eth::Client* ContractStorageLimitPatch::g_client;

bool ContractStorageLimitPatch::isEnabled() {
    return g_client->chainParams().sChain.introduceChangesTimestamp <=
           g_client->blockChain().info().timestamp();
}
