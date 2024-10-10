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

std::shared_ptr< dev::db::DatabaseFace > BatchedRotatingHistoricDbIO::getPieceByTimestamp(
    uint64_t timestamp ) {
    std::lock_guard< std::mutex > lock( mutex );

    if ( timestamp >= timestamps.back() )
        return currentPiece();

    if ( timestamp < timestamps.front() )
        throw std::invalid_argument( "Invalid timestamp passed to BatchedRotatingHistoricDbIO." );

    auto it = std::upper_bound( timestamps.begin(), timestamps.end(), timestamp );
    auto timestampToLook = it == timestamps.begin() ? *it : *( --it );

    auto path = basePath / std::to_string( timestampToLook );
    if ( dbsInUse.count( path ) )
        return dbsInUse.at( path );
    dbsInUse[path].reset( new LevelDB( path ) );
    return dbsInUse[path];
}

uint64_t BatchedRotatingHistoricDbIO::getTimestampFromKey( Slice& _key ) {
    auto keyStr = _key.toString();
    size_t pos = keyStr.find( ':' );
    if ( pos == std::string::npos )
        return -1;
    uint64_t timestamp = std::stoull( keyStr.substr( pos ) );
    _key = Slice( _key.begin(), pos + 1 );
    return timestamp;
}

std::pair< std::vector< uint64_t >::const_reverse_iterator, std::vector< uint64_t >::const_reverse_iterator >
BatchedRotatingHistoricDbIO::getRangeForBlockTimestamp( uint64_t _timestamp ) {
    return {timestamps.rbegin(), timestamps.rend()};
    // auto timestamps = getTimestamps();
    // auto timestampForCall = getTimestampFromKey( _key );
    // auto it = timestampForCall == uint64_t( -1 ) ?
    //               timestamps.begin() :
    //               std::lower_bound( timestamps.begin(), timestamps.end(), timestampForCall );
    // if ( it != timestamps.begin() )
    //     --it;
    // return { it, timestamps.end() };
}

BatchedRotatingHistoricDbIO::~BatchedRotatingHistoricDbIO() {}

}  // namespace batched_io
