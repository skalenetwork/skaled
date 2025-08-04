#ifndef BLOCKREWARDSACTIVATIONPATCH_H
#define BLOCKREWARDSACTIVATIONPATCH_H

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

    static bool isEnabled();

    static dev::eth::EVMSchedule makeSchedule( const dev::eth::EVMSchedule& _base );

private:
    static dev::eth::Client* client;
    static const dev::Address blockRewardsActivationPatchAddress;
};

#endif  // BLOCKREWARDSACTIVATIONPATCH_H
