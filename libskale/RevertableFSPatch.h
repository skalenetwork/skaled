#include <libethereum/SchainPatch.h>

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
    static dev::eth::Client* g_client;
};