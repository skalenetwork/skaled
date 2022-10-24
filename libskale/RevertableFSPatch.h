#include <libethereum/SchainPatch.h>
#include <time.h>

namespace dev {
namespace eth {
class Client;
}
}  // namespace dev

/*
 * Context: enable revertable filestorage precompileds
 */
class RevertableFSPatch : public SchainPatch {
public:
    static bool isEnabled();

private:
    friend class dev::eth::Client;
    static time_t revertableFSPatchTimestamp;
    static time_t lastBlockTimestamp;
};