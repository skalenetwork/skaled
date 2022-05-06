#include "BlockChain.h"

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

void repair_blocks_and_extras_db(boost::filesystem::path const& _path){
    // TODO do we have anough classes for batched and non-batched access?
    auto rotator = std::make_shared< batched_io::rotating_db_io >( _path, 5 );
    auto rotating_db = std::make_shared< dev::db::ManuallyRotatingLevelDB >( rotator );
    auto db = std::make_shared< batched_io::batched_db >();
    db->open( rotating_db );
    auto db_splitter = std::make_unique< batched_io::db_splitter >( db );
    auto blocksDB = db_splitter->new_interface();
    auto extrasDB = db_splitter->new_interface();

    // TODO catch

    size_t start_block = 1000;
    h256 new_state_root_for_all; // = get it from block

    h256 prev_hash;

    for( size_t bn = start_block;; ++bn ){

        // read block

        h256 block_hash = numberHash( extrasDB, bn );

        string block_binary = blocksDB->lookup( toSlice( block_hash ) );

        RLP block_rlp( block_binary );

        // 1 update parent

        block_rlp[0][0] = RLP( RLPStream().append( prev_hash ).out() );

        // 2 update stateRoot

        block_rlp[0][3] = RLP( RLPStream().append( prev_hash ).out() );

        // 3 recompute hash

        BlockHeader new_header( block_rlp.data() );

        // write block

        blocksDB->kill( toSlice( block_hash ) );
        blocksDB->insert( toSlice( new_header.hash() ), db::Slice( block_rlp.data() ) );

        // update extras
        // parent! extrasWriteBatch.insert( toSlice( _block.info.parentHash(), ExtraDetails ),
        //     ( db::Slice ) dev::ref( m_details[_block.info.parentHash()].rlp() ) );

        string details_binary = extrasDB->lookup( toSlice( block_hash, ExtraDetails ) );
        BlockDetails block_details( RLP( details_binary ) );


        extrasWriteBatch.insert(
            toSlice( new_header.hash(), ExtraDetails ), ( db::Slice ) dev::ref( details_rlp ) );

        BlockLogBlooms blb;
        for ( auto i : RLP( _receipts ) )
            blb.blooms.push_back( TransactionReceipt( i.data() ).bloom() );
        extrasWriteBatch.insert(
            toSlice( _block.info.hash(), ExtraLogBlooms ), ( db::Slice ) dev::ref( blb.rlp() ) );

        extrasWriteBatch.insert(
            toSlice( _block.info.hash(), ExtraReceipts ), ( db::Slice ) _receipts );


        // update block hashes for transaction locations

        db->commit("repair_block");

        prev_hash = new_header.hash();
    }// for
}
