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

    std::map< uint64_t, std::unique_ptr< dev::db::DatabaseFace > > piecesByTimestamp;

public:
    using constIterator =
        std::map< uint64_t, std::unique_ptr< dev::db::DatabaseFace > >::const_iterator;

    BatchedRotatingHistoricDbIO( const boost::filesystem::path& _path );
    dev::db::DatabaseFace* currentPiece() const { return piecesByTimestamp.rbegin()->second.get(); }
    //    dev::db::DatabaseFace* getPieceByTimestamp( uint64_t timestamp );
    constIterator begin() const { return piecesByTimestamp.begin(); }
    constIterator end() const { return piecesByTimestamp.end(); }
    void rotate( uint64_t timestamp );
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
