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

    std::vector< uint64_t > timestamps;

    std::map< boost::filesystem::path, std::shared_ptr< dev::db::DatabaseFace > > dbsInUse;
    std::shared_ptr< dev::db::DatabaseFace > current;
    std::chrono::system_clock::time_point lastCleanup;

    std::mutex mutex;

    static const uint64_t MAX_OPENED_DB_COUNT;
    static const std::chrono::system_clock::duration OPENED_DB_CHECK_INTERVAL;

public:
    BatchedRotatingHistoricDbIO( const boost::filesystem::path& _path );
    std::shared_ptr< dev::db::DatabaseFace > currentPiece() const { return current; }
    std::shared_ptr< dev::db::DatabaseFace > getPieceByTimestamp( uint64_t timestamp );
    std::vector< uint64_t > getTimestamps() const { return timestamps; }
    std::pair< std::vector< uint64_t >::const_iterator, std::vector< uint64_t >::const_iterator >
    getRangeForKey( dev::db::Slice& _key );
    void rotate( uint64_t timestamp );
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

private:
    static uint64_t getTimestampFromKey( dev::db::Slice& _key );
};

}  // namespace batched_io

#endif  // BATCHEDROTATINGHISTORICDBIO_H
