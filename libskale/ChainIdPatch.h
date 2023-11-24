#ifndef CHAINIDPATCH_H
#define CHAINIDPATCH_H

#include <libethereum/SchainPatch.h>

#include <time.h>

namespace dev {
namespace eth {
class Client;
}
}  // namespace dev

/*
 * Context: txns should be replay protected
 * Solution: we fixed the bug and added new config field introduceChangesTimestamp
 * Purpose: improve security
 * Version introduced:
 */
class ChainIdPatch : public SchainPatch {
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

#endif  // CHAINIDPATCH_H
