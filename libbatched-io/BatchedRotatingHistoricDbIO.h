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

    std::vector< uint64_t > piecesByTimestamp;

    std::map< boost::filesystem::path, std::shared_ptr< dev::db::DatabaseFace > > dbsInUse;
    std::shared_ptr< dev::db::DatabaseFace > current;
    std::chrono::system_clock::time_point lastCleanup;

    std::mutex mutex;

public:
    BatchedRotatingHistoricDbIO( const boost::filesystem::path& _path );
    std::shared_ptr< dev::db::DatabaseFace > currentPiece() const { return current; }
    std::shared_ptr< dev::db::DatabaseFace > getPieceByTimestamp( uint64_t timestamp );
    std::vector< uint64_t > getPieces() const { return piecesByTimestamp; }
    void rotate( uint64_t timestamp );
    void closeAllOpenedDbs();
    virtual void revert() { /* no need - as all write is in rotate() */
    }
    virtual void commit(
        const std::string& test_crash_string = std::string() ) { /*already implemented in rotate()*/
        ( void ) test_crash_string;
    }
    virtual ~BatchedRotatingHistoricDbIO();

protected:
    virtual void recover();
};

}  // namespace batched_io

#endif  // BATCHEDROTATINGHISTORICDBIO_H
