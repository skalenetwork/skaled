#include "BatchedRotatingHistoricDbIO.h"

#include <libdevcore/LevelDB.h>

namespace batched_io {

using namespace dev::db;

const uint64_t BatchedRotatingHistoricDbIO::MAX_OPENED_DB_COUNT = 10;
const std::chrono::system_clock::duration BatchedRotatingHistoricDbIO::OPENED_DB_CHECK_INTERVAL =
    std::chrono::seconds( 1000 );

BatchedRotatingHistoricDbIO::BatchedRotatingHistoricDbIO( const boost::filesystem::path& _path )
    : basePath( _path ) {
    // initialize blockNumbers
    if ( boost::filesystem::exists( basePath ) )
        for ( const auto& f : boost::filesystem::directory_iterator( basePath ) )
            blockNumbers.push_back( std::stoull( boost::filesystem::basename( f ) ) );

    if ( blockNumbers.empty() )
        blockNumbers.push_back( 0 );

    // initialize current with the latest existing db
    std::sort( blockNumbers.begin(), blockNumbers.end() );
    current.reset( new LevelDB( basePath / std::to_string( blockNumbers.back() ) ) );

    lastCleanup = std::chrono::system_clock::now();

    test_crash_before_commit( "after_recover" );
}

void BatchedRotatingHistoricDbIO::rotate( uint64_t blockNumber ) {
    std::lock_guard< std::mutex > lock( mutex );
    assert( blockNumber > blockNumbers.back() );
    auto storageUsed = currentPiece()->lookup( dev::db::Slice( "storageUsed" ) );
    currentPiece()->kill( dev::db::Slice( "storageUsed" ) );

    // move current to used
    dbsInUse[basePath / std::to_string( blockNumbers.back() )] = current;

    blockNumbers.push_back( blockNumber );
    current.reset( new LevelDB( basePath / std::to_string( blockNumber ) ) );

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

size_t BatchedRotatingHistoricDbIO::elementByBlockNumber_WITH_LOCK( uint64_t blockNumber ) const {
    if ( blockNumber >= blockNumbers.back() )
        return blockNumbers.size() - 1;

    if ( blockNumber < blockNumbers.front() )
        throw std::invalid_argument( "Invalid timestamp passed to BatchedRotatingHistoricDbIO." );

    auto it = std::upper_bound( blockNumbers.begin(), blockNumbers.end(), blockNumber );
    it = ( it == blockNumbers.begin() ? it : it - 1 );
    return std::distance( blockNumbers.begin(), it );
}

std::shared_ptr< dev::db::DatabaseFace > BatchedRotatingHistoricDbIO::getPieceByBlockNumber(
    uint64_t blockNumber ) {
    std::lock_guard< std::mutex > lock( mutex );

    auto pos = elementByBlockNumber_WITH_LOCK( blockNumber );

    // if it's current
    if ( pos == blockNumbers.size() - 1 )
        return current;

    auto blockNumberToLook = blockNumbers[pos];

    auto path = basePath / std::to_string( blockNumberToLook );
    if ( dbsInUse.count( path ) )
        return dbsInUse.at( path );
    if ( dbsInUse.size() > 0 )
        std::cout << "Didn't find " << path << " had only " << dbsInUse.begin()->first << std::endl;
    dbsInUse[path].reset( new LevelDB( path ) );
    return dbsInUse[path];
}

std::pair< std::vector< uint64_t >::const_reverse_iterator,
    std::vector< uint64_t >::const_reverse_iterator >
BatchedRotatingHistoricDbIO::getRangeForBlockNumber( uint64_t _blockNumber ) const {
    std::lock_guard< std::mutex > lock( mutex );
    auto pos = elementByBlockNumber_WITH_LOCK( _blockNumber );
    return { blockNumbers.rend() - 1 - pos, blockNumbers.rend() };
}

BatchedRotatingHistoricDbIO::~BatchedRotatingHistoricDbIO() {}

}  // namespace batched_io
