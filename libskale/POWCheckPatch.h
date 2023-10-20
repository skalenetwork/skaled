#include <libethereum/SchainPatch.h>
#include <time.h>

#ifndef POWCHECKPATCH_H
#define POWCHECKPATCH_H

namespace dev {
namespace eth {
class Client;
}
}  // namespace dev

/*
 * Context: enable fix for POW txns gas limit check
 */
class POWCheckPatch : public SchainPatch {
public:
    static bool isEnabled();

    static void setTimestamp( time_t _timeStamp ) {
        printInfo( __FILE__, _timeStamp );
        powCheckPatchTimestamp = _timeStamp;
    }

private:
    friend class dev::eth::Client;
    static time_t powCheckPatchTimestamp;
    static time_t lastBlockTimestamp;
};

#endif  // POWCHECKPATCH_H
