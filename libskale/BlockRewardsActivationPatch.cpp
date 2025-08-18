#include <libethereum/Client.h>
#include <libskale/BlockRewardsActivationPatch.h>

dev::eth::Client* BlockRewardsActivationPatch::client;

const dev::Address BlockRewardsActivationPatch::blockRewardsActivationPatchAddress =
    dev::eth::toAddress( "0xE8E4Ea98530Bfe86f841E258fd6F3FD5c210c68f" );
const dev::Address BlockRewardsActivationPatch::testBlockRewardsActivationPatchAddress =
    dev::eth::toAddress( "0x5339Ef05428d1b87f4e2F2db64E782c68E9cDA56" );

dev::Address BlockRewardsActivationPatch::getMagicAddress() {
    return std::getenv( "TEST_BLOCK_REWARDS_ACTIVATION" ) ? testBlockRewardsActivationPatchAddress :
                                                            blockRewardsActivationPatchAddress;
}

bool BlockRewardsActivationPatch::isEnabled( uint64_t _chainId ) {
    if ( _chainId != fairChainId ) {
        // means that we are in unit test or testnet environment
        // always enable for unit tests by default
        // check epochId for testnet
        if ( client ) {
            if ( !client->chainParams().isTestSignaturesEnabled() )
                // we are on testnet. enable only after first committee rotation
                return client->getCurrentEpochId() > 0;
        }
        return true;
    }
    // we are on mainnet
    return client->countAt( getMagicAddress() ) != 0;
}

dev::eth::EVMSchedule BlockRewardsActivationPatch::makeSchedule(
    const dev::eth::EVMSchedule& _base ) {
    dev::eth::EVMSchedule ret = _base;
    ret.blockRewardOverwrite = { 5 * dev::eth::ether };
    ret.shareOfTransactionFeeToRewardPromille = 500;    // 50%
    ret.shareOfBlockRewardToBlockAuthorPromille = 500;  // 50%
    return ret;
}
