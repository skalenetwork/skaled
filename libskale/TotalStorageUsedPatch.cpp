#include "TotalStorageUsedPatch.h"

#include <libethcore/Common.h>

#include <libethereum/Client.h>

using namespace dev;
using namespace dev::eth;

const Address magicAddress( toAddress( "0xE8E4Ea98530Bfe86f841E258fd6F3FD5c210c68f" ) );
dev::eth::Client* TotalStorageUsedPatch::client;

void TotalStorageUsedPatch::onProgress( batched_io::db_operations_face& _db, size_t _blockNumber ) {
    if ( !_db.exists( dev::db::Slice( "\x0totalStorageUsed", 17 ) ) )
        return;
    if ( client->countAt( magicAddress ) == 0 )
        _db.insert( dev::db::Slice( "\x0totalStorageUsed", 17 ),
            dev::db::Slice( std::to_string( _blockNumber * 32 ) ) );
    else
        _db.kill( dev::db::Slice( "\x0totalStorageUsed", 17 ) );
}
