#include <libethereum/SchainPatch.h>
#include <time.h>

namespace dev {
namespace eth {
class Client;
}
}  // namespace dev

/*
 * Context: enable effective storage destruction
 */
class StorageDestructionPatch : public SchainPatch {
public:
    static bool isEnabled();

    static void setTimestamp( time_t _timeStamp ) {
        printInfo( __FILE__, _timeStamp );
        storageDestructionPatchTimestamp = _timeStamp;
    }


private:
    friend class dev::eth::Client;
    static time_t storageDestructionPatchTimestamp;
    static time_t lastBlockTimestamp;
};