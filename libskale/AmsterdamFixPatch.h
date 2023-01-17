#ifndef AMSTERDAMFIXPATCH_H
#define AMSTERDAMFIXPATCH_H

#include <libethereum/Client.h>
#include <libethereum/SchainPatch.h>

#include <boost/filesystem.hpp>

/*
 * Context: version 3.14.9-stable.1 broke totalStorageUsed field updating
 *     (see TotalStorageUsedPatch::onProgress)
 * Purpose: This patch disables stateRoot checking for 4 affected schains.
 *     After specially-crafted transaction, it enables stateRoot checking back.
 * Version introduced: 3.14.9-stable.3 (stateRoot ignorance)
 *                     TBD (stateRoot check reintroducing)
 */

class AmsterdamFixPatch : public SchainPatch {
public:
    static bool isInitOnChainNeeded(
        batched_io::db_operations_face& _blocksDB, batched_io::db_operations_face& _extrasDB );
    static bool isEnabled( const dev::eth::Client& _client );
    static void initOnChain( batched_io::db_operations_face& _blocksDB,
        batched_io::db_operations_face& _extrasDB, batched_io::db_face& _db,
        const dev::eth::ChainParams& _chainParams );
    static bool stateRootCheckingEnabled( const dev::eth::Client& _client );
    static dev::h256 overrideStateRoot( const dev::eth::Client& _client );

    static bool snapshotHashCheckingEnabled( const dev::eth::ChainParams& _cp );

    static size_t lastGoodBlock( const dev::eth::ChainParams& _chainParams );
    static dev::h256 newStateRootForAll;
    static size_t lastBlockToModify;
    static std::vector< size_t > majorityNodesIds();
};

#endif  // AMSTERDAMFIXPATCH_H
