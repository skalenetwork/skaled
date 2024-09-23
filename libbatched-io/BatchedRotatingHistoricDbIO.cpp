#include "BatchedRotatingHistoricDbIO.h"

#include <libdevcore/LevelDB.h>

namespace batched_io {

using namespace dev::db;

BatchedRotatingHistoricDbIO::BatchedRotatingHistoricDbIO( const boost::filesystem::path& _path )
    : basePath( _path ) {
    // initialize timestamps
    if ( boost::filesystem::exists( basePath ) )
        for ( const auto& f : boost::filesystem::directory_iterator( basePath ) ) {
            piecesByTimestamp.push_back( std::stoull( boost::filesystem::basename( f ) ) );
        }
    if ( piecesByTimestamp.empty() )
        piecesByTimestamp.push_back( 0 );
    std::sort( piecesByTimestamp.begin(), piecesByTimestamp.end() );

    // open current
    current.reset( new LevelDB( basePath / std::to_string( piecesByTimestamp.back() ) ) );

    test_crash_before_commit( "after_recover" );
}

void BatchedRotatingHistoricDbIO::rotate( uint64_t timestamp ) {
    piecesByTimestamp.push_back( timestamp );

    current.reset( new LevelDB( basePath / std::to_string( timestamp ) ) );

    test_crash_before_commit( "after_open_leveldb" );
}

void BatchedRotatingHistoricDbIO::recover() {}

dev::db::DatabaseFace* BatchedRotatingHistoricDbIO::getPieceByTimestamp( uint64_t timestamp ) {
    if ( timestamp < piecesByTimestamp.front() )
        return nullptr;

    if ( timestamp >= piecesByTimestamp.back() )
        return currentPiece();

    auto it = std::upper_bound( piecesByTimestamp.begin(), piecesByTimestamp.end(), timestamp );

    dev::db::DatabaseFace* ret = new LevelDB( basePath / std::to_string( *( --it ) ) );

    return ret;
}

BatchedRotatingHistoricDbIO::~BatchedRotatingHistoricDbIO() {}

}  // namespace batched_io
