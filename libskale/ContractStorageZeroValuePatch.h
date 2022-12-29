#ifndef CONTRACTSTORAGEZEROVALUEPATCH_H
#define CONTRACTSTORAGEZEROVALUEPATCH_H

#include <libethereum/SchainPatch.h>

#include <time.h>

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
class ContractStorageZeroValuePatch : public SchainPatch {
public:
    static bool isEnabled();

private:
    friend class dev::eth::Client;
    static time_t contractStorageZeroValuePatchTimestamp;
    static time_t lastBlockTimestamp;
};

#endif  // CONTRACTSTORAGEZEROVALUYEPATCH_H
