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

private:
    friend class dev::eth::Client;
    static time_t storageDestructionPatchTimestamp;
    static time_t lastBlockTimestamp;
};