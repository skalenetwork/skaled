#include "TotalStorageUsedPatch.h"

#include <libethcore/Common.h>

#include <libethereum/Client.h>

using namespace dev;
using namespace dev::eth;

const Address magicAddress( toAddress("0xf15f970E370486d5137461c5936dC6019898e6C8") );
dev::eth::Client* TotalStorageUsedPatch::g_client;

void TotalStorageUsedPatch::onProgress( batched_io::db_operations_face& _db, size_t _blockNumber ) {
    if ( !_db.exists( dev::db::Slice( "\x0totalStorageUsed", 17 ) ) )
        return;
    if ( g_client->countAt( magicAddress ) == 0 )
        _db.insert( dev::db::Slice( "\x0totalStorageUsed", 17 ),
            dev::db::Slice( std::to_string( _blockNumber * 32 ) ) );
    else
        _db.kill( dev::db::Slice( "\x0totalStorageUsed", 17 ) );
}
