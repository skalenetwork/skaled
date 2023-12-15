#ifndef CORRECTFORKINPOWPATCH_H
#define CORRECTFORKINPOWPATCH_H

#include <libethereum/SchainPatch.h>

#include <time.h>

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

private:
    static time_t activationTimestamp;
    static time_t lastBlockTimestamp;
};

#endif  // CORRECTFORKINPOWPATCH_H
