#include "BatchedRotatingHistoricDbIO.h"

#include <libdevcore/LevelDB.h>

namespace batched_io {

using namespace dev::db;

const uint64_t BatchedRotatingHistoricDbIO::MAX_OPENED_DB_COUNT = 10;
const std::chrono::system_clock::duration BatchedRotatingHistoricDbIO::OPENED_DB_CHECK_INTERVAL =
    std::chrono::seconds( 1000 );

BatchedRotatingHistoricDbIO::BatchedRotatingHistoricDbIO( const boost::filesystem::path& _path )
    : basePath( _path ) {
    // initialize timestamps
    if ( boost::filesystem::exists( basePath ) )
        for ( const auto& f : boost::filesystem::directory_iterator( basePath ) )
            timestamps.push_back( std::stoull( boost::filesystem::basename( f ) ) );

    if ( timestamps.empty() )
        timestamps.push_back( 0 );

    // initialize current with the latest existing db
    std::sort( timestamps.begin(), timestamps.end() );
    current.reset( new LevelDB( basePath / std::to_string( timestamps.back() ) ) );

    lastCleanup = std::chrono::system_clock::now();

    test_crash_before_commit( "after_recover" );
}

void BatchedRotatingHistoricDbIO::rotate( uint64_t timestamp ) {
    std::lock_guard< std::mutex > lock( mutex );
    auto storageUsed = currentPiece()->lookup( dev::db::Slice( "storageUsed" ) );
    currentPiece()->kill( dev::db::Slice( "storageUsed" ) );

    // move current to used
    dbsInUse[basePath / std::to_string( timestamps.back() )] = current;

    timestamps.push_back( timestamp );
    current.reset( new LevelDB( basePath / std::to_string( timestamp ) ) );

    test_crash_before_commit( "after_open_leveldb" );
}

void BatchedRotatingHistoricDbIO::checkOpenedDbsAndCloseIfNeeded() {
    std::lock_guard< std::mutex > lock( mutex );

    if ( ( lastCleanup + OPENED_DB_CHECK_INTERVAL > std::chrono::system_clock::now() ) &&
         dbsInUse.size() < MAX_OPENED_DB_COUNT )
        return;

    for ( auto it = dbsInUse.begin(); it != dbsInUse.end(); ) {
        if ( it->second.use_count() == 0 )
            it = dbsInUse.erase( it );
        else
            ++it;
    }

    lastCleanup = std::chrono::system_clock::now();
}

void BatchedRotatingHistoricDbIO::recover() {}

size_t BatchedRotatingHistoricDbIO::elementByTimestamp_WITH_LOCK( uint64_t timestamp ) const {
    if ( timestamp >= timestamps.back() )
        return timestamps.size() - 1;

    if ( timestamp < timestamps.front() )
        throw std::invalid_argument( "Invalid timestamp passed to BatchedRotatingHistoricDbIO." );

    auto it = std::upper_bound( timestamps.begin(), timestamps.end(), timestamp );
    it = ( it == timestamps.begin() ? it : it - 1 );
    return std::distance( timestamps.begin(), it );
}

std::shared_ptr< dev::db::DatabaseFace > BatchedRotatingHistoricDbIO::getPieceByTimestamp(
    uint64_t timestamp ) {
    std::lock_guard< std::mutex > lock( mutex );

    auto pos = elementByTimestamp_WITH_LOCK( timestamp );

    // if it's current
    if ( pos == timestamps.size() - 1 )
        return current;

    auto timestampToLook = timestamps[pos];

    auto path = basePath / std::to_string( timestampToLook );
    if ( dbsInUse.count( path ) )
        return dbsInUse.at( path );
    if ( dbsInUse.size() > 0 )
        std::cout << "Didn't find " << path << " had only " << dbsInUse.begin()->first << std::endl;
    dbsInUse[path].reset( new LevelDB( path ) );
    return dbsInUse[path];
}

std::pair< std::vector< uint64_t >::const_reverse_iterator,
    std::vector< uint64_t >::const_reverse_iterator >
BatchedRotatingHistoricDbIO::getRangeForBlockTimestamp( uint64_t _timestamp ) const {
    std::lock_guard< std::mutex > lock( mutex );
    auto pos = elementByTimestamp_WITH_LOCK( _timestamp );
    return { timestamps.rend() - 1 - pos, timestamps.rend() };
}

BatchedRotatingHistoricDbIO::~BatchedRotatingHistoricDbIO() {}

}  // namespace batched_io
