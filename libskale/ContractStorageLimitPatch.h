#ifndef CONTRACTSTORAGELIMITPATCH_H
#define CONTRACTSTORAGELIMITPATCH_H

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
class ContractStorageLimitPatch : public SchainPatch {
public:
    static bool isEnabled();

    static void setTimestamp( time_t _timeStamp ) {
        printInfo( __FILE__, _timeStamp );
        contractStoragePatchTimestamp = _timeStamp;
    }

private:
    friend class dev::eth::Client;
    static time_t contractStoragePatchTimestamp;
    static time_t lastBlockTimestamp;
};

#endif  // CONTRACTSTORAGELIMITPATCH_H
