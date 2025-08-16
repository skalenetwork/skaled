#pragma once

#include <libethereum/SchainPatch.h>

namespace dev {
namespace eth {
class Client;
}  // namespace eth
}  // namespace dev

class BlockRewardsActivationPatch : SchainPatch {
public:
    static void init( dev::eth::Client* _client ) {
        CHECK_EXPRESSION( _client );
        client = _client;
    }

    static bool isEnabled( uint64_t _chainId );

    static dev::eth::EVMSchedule makeSchedule( const dev::eth::EVMSchedule& _base );

    static dev::Address getMagicAddress();

private:
    static dev::eth::Client* client;
    static const dev::Address blockRewardsActivationPatchAddress;
    static const dev::Address testBlockRewardsActivationPatchAddress;
    static constexpr uint64_t fairChainId = 934;  // 0x3a6
};
