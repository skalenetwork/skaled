#include "TotalStorageUsedPatch.h"

#include <libethcore/Common.h>

#include <libethereum/Client.h>

using namespace dev;
using namespace dev::eth;

const Address magicAddress( toAddress("0xf15f970E370486d5137461c5936dC6019898e6C8") );
dev::eth::Client* TotalStorageUsedPatch::g_client;

bool TotalStorageUsedPatch::isEnabled( batched_io::db_operations_face& _db ) {
    return _db.exists( dev::db::Slice( "\x0totalStorageUsed", 17 ) ) &&
           g_client->countAt( magicAddress ) == 0;
}
