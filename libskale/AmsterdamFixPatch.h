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
    static bool isInitOnChainNeeded( batched_io::db_operations_face& _blocksDB, batched_io::db_operations_face& _extrasDB) {
        dev::h256 best_hash = dev::h256( dev::RLP( _extrasDB.lookup( dev::db::Slice( "best" ) ) ) );
        std::string best_binary = _blocksDB.lookup( dev::eth::toSlice( best_hash ) );
        dev::eth::BlockHeader best_header( best_binary );
        uint64_t best_number = best_header.number();

        uint64_t totalStorageUsed;
        try{
            std::string totalStorageUsedStr = _blocksDB.lookup( dev::db::Slice( "\x0totalStorageUsed", 17 ) );
            totalStorageUsed = std::stoull( totalStorageUsedStr );
        } catch(...) {
            return false;
        }

        return totalStorageUsed != best_number * 32;
    }
    static bool isEnabled( dev::eth::Client& _client ) {
        //_client.call();
        (void) _client;
        return true;
    }
    static void initOnChain( boost::filesystem::path const& _path ) {
        repair_blocks_and_extras_db( _path );
    }
    static bool stateRootCheckingEnabled( dev::eth::Client& _client ){
        uint64_t chainID = _client.chainParams().chainID;
        if( !isEnabled( _client ) )
            return true;
        if ( chainID == 0xd2ba743e9fef4 ||
             chainID == 0x292a2c91ca6a3 ||
             chainID == 0x1c6fa7f59eeac ||
             chainID == 0x4b127e9c2f7de)
            return false;
        else
            return true;
    }
};

#endif // AMSTERDAMFIXPATCH_H
