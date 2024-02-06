#include <libethereum/SchainPatch.h>
#include <time.h>

namespace dev {
namespace eth {
class Client;
}
}  // namespace dev

/*
 * Context: enable effective storage destruction
 *
class PushZeroPatch : public SchainPatch {
public:
    static bool isEnabled();

    static void setTimestamp( time_t _timeStamp ) {
        printInfo( __FILE__, _timeStamp );
        pushZeroPatchTimestamp = _timeStamp;
    }


private:
    friend class dev::eth::Client;
    static time_t pushZeroPatchTimestamp;
    static time_t lastBlockTimestamp;
};
*/

DEFINE_EVM_PATCH( PushZeroPatch )