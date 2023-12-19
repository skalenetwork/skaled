#ifndef CORRECTFORKINPOWPATCH_H
#define CORRECTFORKINPOWPATCH_H

#include <libethereum/SchainPatch.h>

#include <time.h>

namespace dev {
namespace eth {
class Client;
}
}  // namespace dev

/*
 * Context: use current, and not Constantinople,  fork in Transaction::checkOutExternalGas()
 */
class CorrectForkInPowPatch : public SchainPatch {
public:
    static bool isEnabled();

    static void setTimestamp( time_t _timeStamp ) {
        printInfo( __FILE__, _timeStamp );
        activationTimestamp = _timeStamp;
    }

    static unsigned getLastBlockNumber() { return lastBlockNumber; }

private:
    friend class dev::eth::Client;
    static time_t activationTimestamp;
    static time_t lastBlockTimestamp;
    static unsigned lastBlockNumber;
};

#endif  // CORRECTFORKINPOWPATCH_H
