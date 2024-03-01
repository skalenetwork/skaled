#include <libethereum/SchainPatch.h>

namespace dev {
namespace eth {
class Client;
}
}  // namespace dev

/*
 * Context: enable effective storage destruction
 */
DEFINE_AMNESIC_PATCH( StorageDestructionPatch );
