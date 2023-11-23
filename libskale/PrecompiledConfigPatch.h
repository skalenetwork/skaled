#ifndef PRECOMPILEDCONFIGPATCH_H
#define PRECOMPILEDCONFIGPATCH_H

#include <libethereum/SchainPatch.h>
#include <time.h>

namespace dev {
namespace eth {
class Client;
}
}  // namespace dev

/*
 * Context: enable precompiled contracts to read historical config data
 */
class PrecompiledConfigPatch : public SchainPatch {
public:
    static bool isEnabled();

    static void setTimestamp( time_t _timeStamp ) {
        printInfo( __FILE__, _timeStamp );
        precompiledConfigPatchTimestamp = _timeStamp;
    }

private:
    friend class dev::eth::Client;
    static time_t precompiledConfigPatchTimestamp;
    static time_t lastBlockTimestamp;
};


#endif  // PRECOMPILEDCONFIGPATCH_H
