#include "BlockChain.h"
#include <libweb3jsonrpc/JsonHelper.h>

#include <libdevcore/Log.h>
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

void dump_blocks_and_extras_db( const BlockChain& _bc, size_t _startBlock ) {
    int64_t prev_ts = -1;

    for ( size_t bn = _startBlock; bn <= _bc.number(); ++bn ) {
        h256 hash = _bc.numberHash( bn );
        assert( hash != h256() );
        bytes block_binary = _bc.block( hash );
        BlockHeader header( block_binary );
        BlockDetails details = _bc.details( hash );
        TransactionHashes transaction_hashes = _bc.transactionHashes( hash );

        Json::Value block_json = toJson( header, details, UncleHashes(), transaction_hashes );

        LogBloom bloom = _bc.blockBloom( bn );

        //        block_json["hash"] = "suppressed";
        //        block_json["parentHash"] = "suppressed";
        //        block_json["stateRoot"] = "suppressed";

        cout << "Block " << bn << "\n";
        if ( transaction_hashes.size() || header.timestamp() == prev_ts ) {
            cout << block_json << "\n";
            cout << "Transactions: "
                 << "\n";
            for ( size_t i = 0; i < transaction_hashes.size(); ++i ) {
                h256 tx_hash = transaction_hashes[i];
                pair< h256, int > loc = _bc.transactionLocation( tx_hash );
                cout << tx_hash << " -> "
                     << ( loc.first == header.hash() ? "block hash ok" : "block hash error!" )
                     << " " << loc.second << "\n";
            }  // for t
            cout << "Bloom:"
                 << "\n";
            cout << bloom.hex() << endl;
        }  // if

        prev_ts = header.timestamp();

    }  // for
}

void dump_blocks_and_extras_db( boost::filesystem::path const& _path, size_t _startBlock ) {
    ChainParams dummy_cp;
    dummy_cp.sealEngineName = NoProof::name();
    BlockChain bc( dummy_cp, _path, false, WithExisting::Trust );
    dump_blocks_and_extras_db( bc, _startBlock );
}
