#include <libethereum/Client.h>
#include <libskale/BlockRewardsActivationPatch.h>

const dev::Address BlockRewardsActivationPatch::blockRewardsActivationPatchAddress =
    dev::eth::toAddress( "0xE8E4Ea98530Bfe86f841E258fd6F3FD5c210c68f" );
dev::eth::Client* BlockRewardsActivationPatch::client;

bool BlockRewardsActivationPatch::isEnabled() {
    return client->countAt( blockRewardsActivationPatchAddress ) != 0;
}

dev::eth::EVMSchedule BlockRewardsActivationPatch::makeSchedule(
    const dev::eth::EVMSchedule& _base ) {
    dev::eth::EVMSchedule ret = _base;
    ret.blockRewardOverwrite = { 5 * dev::eth::ether };
    ret.shareOfFeesToReward = 0.5;
    return ret;
}
