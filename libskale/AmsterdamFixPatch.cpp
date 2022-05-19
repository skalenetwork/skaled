#include "AmsterdamFixPatch.h"

using namespace dev;
using namespace dev::eth;
using namespace std;

bool AmsterdamFixPatch::isInitOnChainNeeded( batched_io::db_operations_face& _blocksDB, batched_io::db_operations_face& _extrasDB) {
    h256 best_hash = h256( RLP( _extrasDB.lookup( db::Slice( "best" ) ) ) );
    std::string best_binary = _blocksDB.lookup( toSlice( best_hash ) );
    BlockHeader best_header( best_binary );
    uint64_t best_number = best_header.number();

    uint64_t totalStorageUsed;
    try{
        std::string totalStorageUsedStr = _blocksDB.lookup( db::Slice( "\x0totalStorageUsed", 17 ) );
        totalStorageUsed = std::stoull( totalStorageUsedStr );
    } catch(...) {
        return false;
    }

    return totalStorageUsed != best_number * 32;
}
bool AmsterdamFixPatch::isEnabled( Client& _client ) {
    //_client.call();
    (void) _client;
    return !boost::filesystem::exists("/homa/dimalit/magic_file.txt");
}
void AmsterdamFixPatch::initOnChain( boost::filesystem::path const& _path ) {
    repair_blocks_and_extras_db( _path );
}
bool AmsterdamFixPatch::stateRootCheckingEnabled( Client& _client ){
    uint64_t chainID = _client.chainParams().chainID;
    if( !isEnabled( _client ) )
        return true;
    if ( true || chainID == 0xd2ba743e9fef4 ||
         chainID == 0x292a2c91ca6a3 ||
         chainID == 0x1c6fa7f59eeac ||
         chainID == 0x4b127e9c2f7de)
        return false;
    else
        return true;
}

