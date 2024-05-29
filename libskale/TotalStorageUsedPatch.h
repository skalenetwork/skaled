#ifndef TOTALSTORAGEUSEDPATCH_H
#define TOTALSTORAGEUSEDPATCH_H

#include <libethereum/SchainPatch.h>

#include <libethereum/BlockChain.h>

#include <libbatched-io/batched_db.h>

namespace dev {
namespace eth {
class Client;
}
}  // namespace dev

/*
 * Context: totalStorageUsed field in DB was actually broken
 *     and just equal to block_number*32
 * Solution: we introduced new field pieceUsageBytes for this
 * Purpose: keep totalStorageUsed field in DB compatible
 * Version introduced: 3.7.5-stable.0
 */
class TotalStorageUsedPatch : public SchainPatch {
public:
    static void init( dev::eth::Client* _client ) {
        assert( _client );
        client = _client;
    }
    static bool isInitOnChainNeeded( batched_io::db_operations_face& _db ) {
        return !_db.exists( ( dev::db::Slice ) "pieceUsageBytes" );
    }
    static void initOnChain( dev::eth::BlockChain& _bc ) {
        // TODO move it here, as bc can be unitialized yet!
        _bc.recomputeExistingOccupiedSpaceForBlockRotation();
    }
    static void onProgress( batched_io::db_operations_face& _db, size_t _blockNumber );

private:
    static dev::eth::Client* client;
};

#endif  // TOTALSTORAGEUSEDPATCH_H
