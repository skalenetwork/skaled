#include "BlockChain.h"
#include <libweb3jsonrpc/JsonHelper.h>

#include <libdevcore/ManuallyRotatingLevelDB.h>

using namespace std;
using namespace dev;
using namespace eth;

dev::h256 numberHash( batched_io::db_operations_face* _db, unsigned _i ) {
    string const s = _db->lookup( toSlice( _i, ExtraBlockHash ) );
    if ( s.empty() )
        return h256();

    return h256( RLP( s ) );
}

void repair_blocks_and_extras_db( boost::filesystem::path const& _path ) {
    // TODO do we have enough classes for batched and non-batched access?
    auto rotator = std::make_shared< batched_io::rotating_db_io >( _path, 5 );
    auto rotating_db = std::make_shared< dev::db::ManuallyRotatingLevelDB >( rotator );
    auto db = std::make_shared< batched_io::batched_db >();
    db->open( rotating_db );
    auto db_splitter = std::make_unique< batched_io::db_splitter >( db );
    auto blocksDB = db_splitter->new_interface();
    auto extrasDB = db_splitter->new_interface();

    // TODO catch

    h256 best_hash = h256( RLP( extrasDB->lookup( db::Slice( "best" ) ) ) );
    // string best_binary = blocksDB->lookup( toSlice( best_hash ) );
    // BlockHeader best_header( best_binary );
    // uint64_t best_number = best_header.number();

    size_t last_good_block = 1800000;
    size_t start_block = last_good_block;
    h256 new_state_root_for_all;  // = get it from block

    h256 prev_hash;
    BlockDetails prev_details;

    for ( size_t bn = start_block;; ++bn ) {
        // read block

        h256 old_hash = numberHash( extrasDB, bn );

        string block_binary = blocksDB->lookup( toSlice( old_hash ) );

        RLP block_rlp( block_binary );

        // 1 update parent

        if ( bn != start_block )
            block_rlp[0][0] = RLP( RLPStream().append( prev_hash ).out() );

        // 2 update stateRoot

        if ( bn == last_good_block )
            new_state_root_for_all = block_rlp[0][3].toHash< h256 >();
        else
            block_rlp[0][3] = RLP( RLPStream().append( new_state_root_for_all ).out() );

        // 3 recompute hash

        BlockHeader new_header( block_rlp.data() );
        h256 new_hash = new_header.hash();
        if ( bn == start_block ) {
            assert( new_hash == old_hash );
        }
        cout << "Repairing block " << bn << " " << old_hash << " -> " << new_hash << endl;

        // write block

        blocksDB->kill( toSlice( old_hash ) );
        blocksDB->insert( toSlice( new_hash ), db::Slice( block_rlp.data() ) );

        // update extras

        string details_binary = extrasDB->lookup( toSlice( old_hash, ExtraDetails ) );
        BlockDetails block_details = BlockDetails( RLP( details_binary ) );
        if ( bn != start_block )
            block_details.parent = prev_hash;

        extrasDB->kill( toSlice( old_hash, ExtraDetails ) );
        extrasDB->insert(
            toSlice( new_hash, ExtraDetails ), ( db::Slice ) dev::ref( block_details.rlp() ) );

        // same for parent details
        if ( bn != start_block )
            prev_details.children = h256s( { new_hash } );
        extrasDB->insert(
            toSlice( prev_hash, ExtraDetails ), ( db::Slice ) dev::ref( prev_details.rlp() ) );

        string log_blooms = extrasDB->lookup( toSlice( old_hash, ExtraLogBlooms ) );
        extrasDB->kill( toSlice( old_hash, ExtraLogBlooms ) );
        extrasDB->insert( toSlice( new_hash, ExtraLogBlooms ), db::Slice( log_blooms ) );

        string receipts = extrasDB->lookup( toSlice( old_hash, ExtraReceipts ) );
        extrasDB->kill( toSlice( old_hash, ExtraReceipts ) );
        extrasDB->insert( toSlice( new_hash, ExtraReceipts ), db::Slice( receipts ) );

        extrasDB->insert( toSlice( h256( bn ), ExtraBlockHash ),
            ( db::Slice ) dev::ref( BlockHash( new_hash ).rlp() ) );

        // update block hashes for transaction locations
        RLPs transactions = block_rlp[1].toList();

        TransactionAddress ta;
        ta.blockHash = new_hash;
        ta.index = 0;

        for ( size_t i = 0; i < transactions.size(); ++i ) {
            h256 hash = sha3( transactions[i].payload() );
            extrasDB->insert(
                toSlice( hash, ExtraTransactionAddress ), ( db::Slice ) dev::ref( ta.rlp() ) );
        }  // for

        db->commit( "repair_block" );

        if ( old_hash == best_hash ) {
            // update latest
            extrasDB->kill( db::Slice( "best" ) );
            extrasDB->insert( db::Slice( "best" ), toSlice( new_hash ) );
            break;
        }

        prev_hash = new_hash;
        prev_details = block_details;
    }  // for
}

void dump_blocks_and_extras_db(
    boost::filesystem::path const& _path, ChainParams const& _chainParams, size_t _startBlock ) {
    BlockChain bc( _chainParams, _path, WithExisting::Trust );

    for ( size_t bn = _startBlock; bn != bc.number(); ++bn ) {
        h256 hash = bc.numberHash( bn );
        bytes block_binary = bc.block( hash );
        BlockHeader header( block_binary );
        BlockDetails details = bc.details( hash );
        TransactionHashes transaction_hashes = bc.transactionHashes( hash );

        Json::Value block_json = toJson( header, details, UncleHashes(), transaction_hashes );

        LogBloom bloom = bc.blockBloom( bn );

        block_json["hash"] = "suppressed";
        block_json["parentHash"] = "suppressed";
        block_json["stateRoot"] = "suppressed";

        cout << "Block " << bn << "\n";
        if ( transaction_hashes.size() || header.stateRoot() == u256() ) {
            cout << block_json << "\n";
            cout << "Transactions: "
                 << "\n";
            for ( size_t i = 0; i < transaction_hashes.size(); ++i ) {
                h256 tx_hash = transaction_hashes[i];
                pair< h256, int > loc = bc.transactionLocation( tx_hash );
                cout << tx_hash << " -> "
                     << ( loc.first == header.hash() ? "block hash ok" : "block hash error!" )
                     << " " << loc.second << "\n";
            }  // for t
            cout << "Bloom:"
                 << "\n";
            cout << bloom.hex() << endl;
        }  // if

    }  // for
}
