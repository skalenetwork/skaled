#include "AmsterdamFixPatch.h"

#include <libdevcore/Log.h>
#include <libethcore/Common.h>
#include <boost/algorithm/string.hpp>

using namespace dev;
using namespace dev::eth;
using namespace std;

dev::h256 AmsterdamFixPatch::newStateRootForAll;
const dev::Address magicAddress(
    dev::eth::toAddress( "0xE8E4Ea98530Bfe86f841E258fd6F3FD5c210c68f" ) );

/* Test net:
 * 0xd2ba743e9fef4 // rhythmic-tegmen -- 15713
 * 0x292a2c91ca6a3 // squeaking-nash --15596
 * 0x1c6fa7f59eeac // chubby-sadr --15479
 * 0x4b127e9c2f7de // tinkling-kaffaljidhma -- 15564
 */

// Main net:
//    case 0xd2ba743e9fef4:
//        return 1981742; // checked on http://18.130.254.6:10003 and http://88.99.209.96:10003
//    case 0x292a2c91ca6a3:
//        return 1861279; // checked on http://185.144.83.169:10003 and http://95.217.227.165:10067
//    case 0x1c6fa7f59eeac:
//        return 1854549; // checked on http://52.229.104.190:10003 and http://185.56.139.86:10003
//    case 0x4b127e9c2f7de:
//        return 1989445; // cheked on http://54.39.184.192:10003 and http://52.147.206.214:10003

size_t AmsterdamFixPatch::lastGoodBlock( const ChainParams& _chainParams ) {
    const char* lgb_str = getenv( "SKALED_TEST_LAST_GOOD_BLOCK_FOR_AMSTERDAM_FIX" );
    if ( lgb_str )
        return strtoul( lgb_str, nullptr, 10 );

    switch ( _chainParams.chainID ) {
    case 0xd2ba743e9fef4:
        return 1981742;  // checked on http://18.130.254.6:10003 and http://88.99.209.96:10003
    case 0x292a2c91ca6a3:
        return 1861279;  // checked on http://185.144.83.169:10003 and http://95.217.227.165:10067
    case 0x1c6fa7f59eeac:
        return 1854549;  // checked on http://52.229.104.190:10003 and http://185.56.139.86:10003
    case 0x4b127e9c2f7de:
        return 1989445;  // cheked on http://54.39.184.192:10003 and http://52.147.206.214:10003
    default:
        assert( false && "lastGoodBlock requested in a non-affected schain!" );
    }  // switch

    return 0;
}

bool AmsterdamFixPatch::isInitOnChainNeeded(
    batched_io::db_operations_face& _blocksDB, batched_io::db_operations_face& _extrasDB ) try {
    h256 best_hash = h256( _extrasDB.lookup( db::Slice( "best" ) ), h256::FromBinary );
    std::string best_binary = _blocksDB.lookup( toSlice( best_hash ) );
    BlockHeader best_header( best_binary );
    uint64_t best_number = best_header.number();

    uint64_t totalStorageUsed;
    std::string totalStorageUsedStr = _blocksDB.lookup( db::Slice( "totalStorageUsed" ) );
    totalStorageUsed = std::stoull( totalStorageUsedStr );

    if ( totalStorageUsed != best_number * 32 )
        clog( VerbosityInfo, "AmsterdamFixPatch" )
            << "Will fix old stateRoots because totalStorageUsed = " << totalStorageUsed;

    return totalStorageUsed != best_number * 32;
} catch ( ... ) {
    // return false if clean DB or no totalStorageUsed
    return false;
}


bool AmsterdamFixPatch::isEnabled( const Client& _client ) {
    //_client.call();
    // return _client.number() < lastBlockToModify;
    auto chainID = _client.chainParams().chainID;
    bool res = ( chainID == 0xd2ba743e9fef4 || chainID == 0x292a2c91ca6a3 ||
                   chainID == 0x1c6fa7f59eeac || chainID == 0x4b127e9c2f7de ) &&
               _client.countAt( magicAddress ) == 0;

    if ( res )
        setenv( "CONSENSUS_USE_STATEROOT_PATCH", "1", 1 );
    else
        unsetenv( "CONSENSUS_USE_STATEROOT_PATCH" );

    return res;
}

static dev::h256 numberHash( batched_io::db_operations_face& _db, unsigned _i ) {
    string const s = _db.lookup( toSlice( _i, ExtraBlockHash ) );
    if ( s.empty() )
        return h256();

    return h256( RLP( s ) );
}

static RLPStream assemble_new_block( const RLP& old_block_rlp, const BlockHeader& header ) {
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

void AmsterdamFixPatch::initOnChain( batched_io::db_operations_face& _blocksDB,
    batched_io::db_operations_face& _extrasDB, batched_io::db_face& _db,
    ChainParams const& _chainParams ) {
    // TODO catch

    h256 best_hash = h256( _extrasDB.lookup( db::Slice( "best" ) ), h256::FromBinary );
    // string best_binary = blocksDB->lookup( toSlice( best_hash ) );
    // BlockHeader best_header( best_binary );
    // uint64_t best_number = best_header.number();

    size_t start_block = lastGoodBlock( _chainParams );

    h256 prev_hash;
    BlockDetails prev_details;

    clog( VerbosityInfo, "AmsterdamFixPatch" )
        << "Repairing stateRoots using base block " << start_block;

    for ( size_t bn = start_block;; ++bn ) {
        // read block

        h256 old_hash = numberHash( _extrasDB, bn );

        string block_binary = _blocksDB.lookup( toSlice( old_hash ) );

        RLP old_block_rlp( block_binary );

        string details_binary = _extrasDB.lookup( toSlice( old_hash, ExtraDetails ) );
        BlockDetails block_details = BlockDetails( RLP( details_binary ) );

        if ( bn == start_block ) {
            newStateRootForAll = old_block_rlp[0][3].toHash< h256 >();
            prev_hash = old_hash;
            prev_details = block_details;
            continue;
        }

        BlockHeader header( block_binary );

        // 1 update parent
        header.setParentHash( prev_hash );

        // 2 update stateRoot
        header.setRoots( header.transactionsRoot(), header.receiptsRoot(), header.sha3Uncles(),
            newStateRootForAll );

        RLPStream new_block_rlp;
        new_block_rlp = assemble_new_block( old_block_rlp, header );

        bytes new_binary = new_block_rlp.out();
        // assert( new_binary != old_block_rlp.data().toVector() );

        // 3 recompute hash
        h256 new_hash = header.hash();

        // write block

        _blocksDB.kill( toSlice( old_hash ) );
        _blocksDB.insert( toSlice( new_hash ), db::Slice( ref( new_binary ) ) );

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

        if ( bn == start_block + 1 || old_hash == best_hash || transactions.size() ||
             bn % 1000 == 0 )
            cout << "Repairing block " << bn << " " << old_hash << " -> " << new_hash << endl;

        TransactionAddress ta;
        ta.blockHash = new_hash;
        ta.index = 0;

        for ( size_t i = 0; i < transactions.size(); ++i ) {
            h256 hash = sha3( transactions[i].data() );
            ta.index = i;
            cout << "Updating transaction " << hash << " location " << old_hash << " -> "
                 << new_hash << " " << ta.index << endl;
            _extrasDB.insert(
                toSlice( hash, ExtraTransactionAddress ), ( db::Slice ) dev::ref( ta.rlp() ) );
        }  // for

        _db.commit( "repair_block" );

        if ( old_hash == best_hash ) {
            // fix "" key
            _db.kill( db::Slice( "\x0" ) );
            // update latest
            _extrasDB.kill( db::Slice( "best" ) );
            _extrasDB.insert(
                db::Slice( "best" ), db::Slice( ( const char* ) new_hash.data(), 32 ) );
            _db.commit( "repair_best" );
            clog( VerbosityInfo, "AmsterdamFixPatch" ) << "Repaired till block " << bn;
            break;
        }

        prev_hash = new_hash;
        prev_details = block_details;
    }  // for
}
bool AmsterdamFixPatch::stateRootCheckingEnabled( const Client& _client ) {
    uint64_t chainID = _client.chainParams().chainID;
    if ( !isEnabled( _client ) )
        return true;

    // NEXT same change should be in consensus!
    if ( chainID == 0xd2ba743e9fef4 || chainID == 0x292a2c91ca6a3 || chainID == 0x1c6fa7f59eeac ||
         chainID == 0x4b127e9c2f7de )
        return false;
    else
        return true;
}

h256 AmsterdamFixPatch::overrideStateRoot( const Client& _client ) {
    if ( !isEnabled( _client ) )
        return h256();  // do not override
    if ( newStateRootForAll == h256() &&
         _client.blockChain().number() >= lastGoodBlock( _client.chainParams() ) )
        newStateRootForAll =
            _client.blockChain()
                .info( _client.blockChain().numberHash( lastGoodBlock( _client.chainParams() ) ) )
                .stateRoot();
    return newStateRootForAll;
}

bool AmsterdamFixPatch::snapshotHashCheckingEnabled( const dev::eth::ChainParams& _cp ) {
    if ( _cp.chainID != 0xd2ba743e9fef4 && _cp.chainID != 0x292a2c91ca6a3 &&
         _cp.chainID != 0x1c6fa7f59eeac && _cp.chainID != 0x4b127e9c2f7de )
        return true;

    std::vector< size_t > majority = majorityNodesIds();
    bool found = majority.end() != std::find( majority.begin(), majority.end(), _cp.nodeInfo.id );

    // disable checking on minority
    return found;
}

std::vector< size_t > AmsterdamFixPatch::majorityNodesIds() {
    const char* str = getenv( "SKALED_TEST_GOOD_NODES_IDS_FOR_AMSTERDAM_FIX" );
    if ( !str )
        return { 90, 134, 162, 169, 177, 179, 183, 189, 192, 208 };
    // else
    std::vector< string > ret_str;
    boost::split( ret_str, str, boost::is_any_of( "," ) );
    std::vector< size_t > ret;
    for_each( ret_str.begin(), ret_str.end(),
        [&ret]( const string& arg ) { ret.push_back( stoul( arg ) ); } );
    return ret;
}
