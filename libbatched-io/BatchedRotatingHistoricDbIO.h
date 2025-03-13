#ifndef BATCHEDROTATINGHISTORICDBIO_H
#define BATCHEDROTATINGHISTORICDBIO_H

#include "batched_io.h"

#include <libdevcore/LevelDB.h>

#include <boost/filesystem.hpp>

#include <vector>

namespace batched_io {

class BatchedRotatingHistoricDbIO : public batched_face {
private:
    const boost::filesystem::path basePath;

    // increasing list of all db pieces
    // numbers indicate 1st block present in a piece
    std::vector< uint64_t > blockNumbers;

    // containts all open DBs but NOT current!
    std::map< boost::filesystem::path, std::shared_ptr< dev::db::DatabaseFace > > dbsInUse;

    // it's always last in timestamps, but NOT in dbsInUse, to prevent it's closing
    std::shared_ptr< dev::db::DatabaseFace > current;
    std::chrono::system_clock::time_point lastCleanup;

    mutable std::mutex mutex;

    static const uint64_t MAX_OPENED_DB_COUNT;
    static const std::chrono::system_clock::duration OPENED_DB_CHECK_INTERVAL;

public:
    BatchedRotatingHistoricDbIO( const boost::filesystem::path& _path );
    std::shared_ptr< dev::db::DatabaseFace > currentPiece() const { return current; }
    // get last piece with 1st block <= supplied value
    std::shared_ptr< dev::db::DatabaseFace > getPieceByBlockNumber( uint64_t blockNumber );
    std::vector< uint64_t > getBlockNumbers() const { return blockNumbers; }
    std::pair< std::vector< uint64_t >::const_reverse_iterator,
        std::vector< uint64_t >::const_reverse_iterator >
    getRangeForBlockNumber( uint64_t _blockNumber ) const;
    void rotate( uint64_t blockNumber );
    void checkOpenedDbsAndCloseIfNeeded();
    virtual void revert() { /* no need - as all write is in rotate() */
    }
    virtual void commit(
        const std::string& test_crash_string = std::string() ) { /*already implemented in rotate()*/
        ( void ) test_crash_string;
    }
    virtual ~BatchedRotatingHistoricDbIO();

protected:
    virtual void recover();
    size_t elementByBlockNumber_WITH_LOCK( uint64_t timestamp ) const;
};

}  // namespace batched_io

#endif  // BATCHEDROTATINGHISTORICDBIO_H
