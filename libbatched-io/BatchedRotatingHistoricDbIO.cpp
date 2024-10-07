#include "BatchedRotatingHistoricDbIO.h"

#include <libdevcore/LevelDB.h>

namespace batched_io {

using namespace dev::db;

BatchedRotatingHistoricDbIO::BatchedRotatingHistoricDbIO( const boost::filesystem::path& _path )
    : basePath( _path ) {
    // initialize timestamps
    if ( boost::filesystem::exists( basePath ) )
        for ( const auto& f : boost::filesystem::directory_iterator( basePath ) )
            piecesByTimestamp.push_back( std::stoull( boost::filesystem::basename( f ) ) );

    if ( piecesByTimestamp.empty() )
        piecesByTimestamp.push_back( 0 );

    // initialize current with the latest existing db
    std::sort( piecesByTimestamp.begin(), piecesByTimestamp.end() );
    current.reset( new LevelDB( basePath / std::to_string( piecesByTimestamp.back() ) ) );

    lastCleanup = std::chrono::system_clock::now();

    test_crash_before_commit( "after_recover" );
}

void BatchedRotatingHistoricDbIO::rotate( uint64_t timestamp ) {
    std::lock_guard< std::mutex > lock( mutex );
    auto storageUsed = currentPiece()->lookup( dev::db::Slice( "storageUsed" ) );
    currentPiece()->kill( dev::db::Slice( "storageUsed" ) );

    piecesByTimestamp.push_back( timestamp );
    current.reset( new LevelDB( basePath / std::to_string( timestamp ) ) );

    test_crash_before_commit( "after_open_leveldb" );
}

void BatchedRotatingHistoricDbIO::closeAllOpenedDbs() {
    std::lock_guard< std::mutex > lock( mutex );
    for ( auto it = dbsInUse.begin(); it != dbsInUse.end(); ++it ) {
        if ( it->second.use_count() == 0 )
            dbsInUse.erase( it );
    }
}

void BatchedRotatingHistoricDbIO::recover() {}

std::shared_ptr< dev::db::DatabaseFace > BatchedRotatingHistoricDbIO::getPieceByTimestamp(
    uint64_t timestamp ) {
    if ( lastCleanup + std::chrono::seconds( 1000 ) < std::chrono::system_clock::now() ) {
        closeAllOpenedDbs();
        lastCleanup = std::chrono::system_clock::now();
    }

    std::lock_guard< std::mutex > lock( mutex );

    if ( timestamp >= piecesByTimestamp.back() )
        return currentPiece();

    if ( timestamp < piecesByTimestamp.front() )
        throw std::invalid_argument( "Invalid timestamp passed to BatchedRotatingHistoricDbIO." );

    auto it = std::upper_bound( piecesByTimestamp.begin(), piecesByTimestamp.end(), timestamp );
    auto timestampToLook = it == piecesByTimestamp.begin() ? *it : *( --it );

    auto path = basePath / std::to_string( timestampToLook );
    if ( dbsInUse.count( path ) )
        return dbsInUse.at( path );
    dbsInUse[path].reset( new LevelDB( path ) );
    return dbsInUse[path];
}

BatchedRotatingHistoricDbIO::~BatchedRotatingHistoricDbIO() {}

}  // namespace batched_io
