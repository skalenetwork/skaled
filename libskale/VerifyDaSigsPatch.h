#ifndef VERIFYDASIGSPATCH_H
#define VERIFYDASIDSPATCH_H

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
class VerifyDaSigsPatch : public SchainPatch {
public:
    static bool isEnabled();

private:
    friend class dev::eth::Client;
    static time_t verifyDaSigsPatchTimestamp;
    static time_t lastBlockTimestamp;

    static void setTimestamp( time_t _timeStamp ) {
        printInfo( __FILE__, _timeStamp );
        verifyDaSigsPatchTimestamp = _timeStamp;
    }

public:
    static time_t getVerifyDaSigsPatchTimestamp();
};

#endif