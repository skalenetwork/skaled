#ifndef CONTRACTSTORAGELIMITPATCH_H
#define CONTRACTSTORAGELIMITPATCH_H

#include <libethereum/SchainPatch.h>

namespace dev {
namespace eth {
class Client;
}
}  // namespace dev

/*
 * Context: contractStorageUsed counter didn't work well in one case
 * Solution: we fixed the bug and added new config field introudceChangesTimestamp
 * Purpose: avoid incorrect txn behaviour
 * Version introduced:
 */
class ContractStorageLimitPatch : public SchainPatch {
public:
    static bool isEnabled();

private:
    friend class dev::eth::Client;
    static dev::eth::Client* g_client;
};

#endif // CONTRACTSTORAGELIMITPATCH_H
