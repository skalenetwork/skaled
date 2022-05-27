#include "AmsterdamFixPatch.h"

#include <libdevcore/Log.h>

using namespace dev;
using namespace dev::eth;
using namespace std;

bool AmsterdamFixPatch::isInitOnChainNeeded( batched_io::db_operations_face& _blocksDB, batched_io::db_operations_face& _extrasDB ) try {
    h256 best_hash =  h256( _extrasDB.lookup( db::Slice( "best" ) ), h256::FromBinary );
    std::string best_binary = _blocksDB.lookup( toSlice( best_hash ) );
    BlockHeader best_header( best_binary );
    uint64_t best_number = best_header.number();

    uint64_t totalStorageUsed;
    std::string totalStorageUsedStr = _blocksDB.lookup( db::Slice( "totalStorageUsed" ) );
    totalStorageUsed = std::stoull( totalStorageUsedStr );

    if( totalStorageUsed != best_number * 32 )
        clog(VerbosityInfo, "AmsterdamFixPatch") << "Will fix old stateRoots because totalStorageUsed = " << totalStorageUsed;

    return totalStorageUsed != best_number * 32;
} catch(...) {
    // return false if clean DB or no totalStorageUsed
    return false;
}


bool AmsterdamFixPatch::isEnabled( Client& _client ) {
    //_client.call();
    (void) _client;
    return !boost::filesystem::exists("/homa/dimalit/magic_file.txt");
}

dev::h256 numberHash( batched_io::db_operations_face& _db, unsigned _i ) {
    string const s = _db.lookup( toSlice( _i, ExtraBlockHash ) );
    if ( s.empty() )
        return h256();

    return h256( RLP( s ) );
}

RLPStream assemble_new_block(const RLP& old_block_rlp, const BlockHeader& header ){

    // see Client::sealUnconditionally
    RLPStream header_rlp;
    header.streamRLP( header_rlp );

    // see Block::sealBlock
    RLPStream ret;
    ret.appendList( 3 );
    ret.appendRaw( header_rlp.out() );
    ret.appendRaw( old_block_rlp[1].data() );
    ret.appendRaw( old_block_rlp[2].data() );

    return ret;
}

void AmsterdamFixPatch::initOnChain( batched_io::db_operations_face& _blocksDB, batched_io::db_operations_face& _extrasDB, batched_io::batched_face& _db ) {

    // TODO catch

    h256 best_hash =  h256( _extrasDB.lookup( db::Slice( "best" ) ), h256::FromBinary );
    // string best_binary = blocksDB->lookup( toSlice( best_hash ) );
    // BlockHeader best_header( best_binary );
    // uint64_t best_number = best_header.number();

    size_t last_good_block = 110;
    size_t start_block = last_good_block;
    h256 new_state_root_for_all;  // = get it from block

    h256 prev_hash;
    BlockDetails prev_details;

    clog(VerbosityInfo, "AmsterdamFixPatch") << "Repairing stateRoots using base block " << start_block;

    for ( size_t bn = start_block;; ++bn ) {
        // read block

        h256 old_hash = numberHash( _extrasDB, bn );

        string block_binary = _blocksDB.lookup( toSlice( old_hash ) );

        RLP old_block_rlp( block_binary );

        string details_binary = _extrasDB.lookup( toSlice( old_hash, ExtraDetails ) );
        BlockDetails block_details = BlockDetails( RLP( details_binary ) );

        if ( bn == last_good_block ){
            new_state_root_for_all = old_block_rlp[0][3].toHash< h256 >();
            prev_hash = old_hash;
            prev_details = block_details;
            continue;
        }

        BlockHeader header( block_binary );

        // 1 update parent
        header.setParentHash( prev_hash );

        // 2 update stateRoot
        header.setRoots( header.transactionsRoot(), header.receiptsRoot(), new_state_root_for_all, header.sha3Uncles() );

        RLPStream new_block_rlp;
        new_block_rlp = assemble_new_block(old_block_rlp, header);

        // 3 recompute hash
        h256 new_hash = header.hash();
        cout << "Repairing block " << bn << " " << old_hash << " -> " << new_hash << endl;

        // write block

        _blocksDB.kill( toSlice( old_hash ) );
        _blocksDB.insert( toSlice( new_hash ), db::Slice( ref( new_block_rlp.out() ) ) );

        // update extras

        block_details.parent = prev_hash;

        _extrasDB.kill( toSlice( old_hash, ExtraDetails ) );
        _extrasDB.insert(
            toSlice( new_hash, ExtraDetails ), ( db::Slice ) dev::ref( block_details.rlp() ) );

        // same for parent details
        prev_details.children = h256s( { new_hash } );
        _extrasDB.insert(
            toSlice( prev_hash, ExtraDetails ), ( db::Slice ) dev::ref( prev_details.rlp() ) );

        string log_blooms = _extrasDB.lookup( toSlice( old_hash, ExtraLogBlooms ) );
        _extrasDB.kill( toSlice( old_hash, ExtraLogBlooms ) );
        _extrasDB.insert( toSlice( new_hash, ExtraLogBlooms ), db::Slice( log_blooms ) );

        string receipts = _extrasDB.lookup( toSlice( old_hash, ExtraReceipts ) );
        _extrasDB.kill( toSlice( old_hash, ExtraReceipts ) );
        _extrasDB.insert( toSlice( new_hash, ExtraReceipts ), db::Slice( receipts ) );

        _extrasDB.insert( toSlice( h256( bn ), ExtraBlockHash ),
            ( db::Slice ) dev::ref( BlockHash( new_hash ).rlp() ) );

        // update block hashes for transaction locations
        RLPs transactions = old_block_rlp[1].toList();

        TransactionAddress ta;
        ta.blockHash = new_hash;
        ta.index = 0;

        for ( size_t i = 0; i < transactions.size(); ++i ) {
            h256 hash = sha3( transactions[i].payload() );
            _extrasDB.insert(
                toSlice( hash, ExtraTransactionAddress ), ( db::Slice ) dev::ref( ta.rlp() ) );
        }  // for

        _db.commit( "repair_block" );

        if ( old_hash == best_hash ) {
            // update latest
            _extrasDB.kill( db::Slice( "best" ) );
            _extrasDB.insert( db::Slice( "best" ), toSlice( new_hash ) );
            _db.commit( "repair_best" );
            clog(VerbosityInfo, "AmsterdamFixPatch") << "Repaired till block " << bn;
            break;
        }

        prev_hash = new_hash;
        prev_details = block_details;
    }  // for
}
bool AmsterdamFixPatch::stateRootCheckingEnabled( Client& _client ){
    uint64_t chainID = _client.chainParams().chainID;
    if( !isEnabled( _client ) )
        return true;
    // NEXT same change should be in consensus!
    if ( true || chainID == 0xd2ba743e9fef4 ||
         chainID == 0x292a2c91ca6a3 ||
         chainID == 0x1c6fa7f59eeac ||
         chainID == 0x4b127e9c2f7de)
        return false;
    else
        return true;
}

