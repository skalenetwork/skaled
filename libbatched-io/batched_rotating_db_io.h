#ifndef BATCHED_ROTATING_DB_IO_H
#define BATCHED_ROTATING_DB_IO_H

#include "batched_io.h"

#include <libdevcore/LevelDB.h>

#include <boost/filesystem.hpp>

#include <deque>

namespace batched_io {

class rotating_db_io : public batched_face {
private:
    const boost::filesystem::path base_path;
    std::deque< std::unique_ptr< dev::db::DatabaseFace > > pieces;
    size_t current_piece_file_no;
    size_t n_pieces;

    bool archive_mode;
    std::deque< std::unique_ptr< dev::db::DatabaseFace > > archive_pieces;

public:
    using const_iterator = std::deque< std::unique_ptr< dev::db::DatabaseFace > >::const_iterator;

    rotating_db_io( const boost::filesystem::path& _path, size_t _nPieces, bool _archiveMode );
    const_iterator begin() const { return pieces.begin(); }
    const_iterator end() const { return pieces.end(); }
    size_t pieces_count() const { return n_pieces; }
    void rotate();
    virtual void revert() { /* no need - as all write is in rotate() */
    }
    virtual void commit(
        const std::string& test_crash_string = std::string() ) { /*already implemented in rotate()*/
        ( void ) test_crash_string;
    }
    virtual ~rotating_db_io();

protected:
    virtual void recover();
};

}  // namespace batched_io

#endif  // BATCHED_ROTATING_DB_IO_H
