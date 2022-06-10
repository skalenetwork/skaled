#ifndef TOTALSTORAGEUSEDPATCH_H
#define TOTALSTORAGEUSEDPATCH_H

#include <libethereum/SchainPatch.h>

#include <libethereum/BlockChain.h>

#include <libbatched-io/batched_db.h>

namespace dev {
namespace eth {
class Client;
}
}

/*
 * Context: totalStorageUsed field in DB was actually broken
 *     and just equal to block_number*32
 * Solution: we introduced new field pieceUsageBytes for this
 * Purpose: keep totalStorageUsed field in DB compatible
 * Version introduced: 3.7.5-stable.0
 */
class TotalStorageUsedPatch : public SchainPatch {
public:
    static bool isInitOnChainNeeded( batched_io::db_operations_face& _db ) {
        return !_db.exists( ( dev::db::Slice ) "pieceUsageBytes" );
    }
    static bool isEnabled( batched_io::db_operations_face& _db );
    static void initOnChain( dev::eth::BlockChain& _bc ) {
        // TODO move it here, as bc can be unitialized yet!
        _bc.recomputeExistingOccupiedSpaceForBlockRotation();
    }
    static void onProgress( batched_io::db_operations_face& _db, size_t _blockNumber ) {
        _db.insert( dev::db::Slice( "\x0totalStorageUsed", 17 ),
            dev::db::Slice( std::to_string( _blockNumber * 32 ) ) );
    }
private:
    friend class dev::eth::Client;
    static dev::eth::Client* g_client;
};

#endif  // TOTALSTORAGEUSEDPATCH_H
