#ifndef CONTRACTSTORAGELIMITPATCH_H
#define CONTRACTSTORAGELIMITPATCH_H

#include <libethereum/SchainPatch.h>

namespace dev {
namespace eth {
class Client;
}
}  // namespace dev

/*
 * Context: totalStorageUsed field in DB was actually broken
 *     and just equal to block_number*32
 * Solution: we introduced new field pieceUsageBytes for this
 * Purpose: keep totalStorageUsed field in DB compatible
 * Version introduced: 3.7.5-stable.0
 */
class ContractStorageLimitPatch : public SchainPatch {
public:
    static bool isEnabled();

private:
    friend class dev::eth::Client;
    static dev::eth::Client* g_client;
};

#endif // CONTRACTSTORAGELIMITPATCH_H
