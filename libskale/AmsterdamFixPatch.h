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

void repair_blocks_and_extras_db( boost::filesystem::path const& _path );

class AmsterdamFixPatch : public SchainPatch
{
public:
    static bool isInitOnChainNeeded( batched_io::db_operations_face& _blocksDB, batched_io::db_operations_face& _extrasDB);
    static bool isEnabled( dev::eth::Client& _client );
    static void initOnChain( batched_io::db_operations_face& _blocksDB, batched_io::db_operations_face& _extrasDB, batched_io::batched_face& _db );
    static bool stateRootCheckingEnabled( dev::eth::Client& _client );
};

#endif // AMSTERDAMFIXPATCH_H
