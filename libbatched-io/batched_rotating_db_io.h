#ifndef BATCHED_ROTATING_DB_IO_H
#define BATCHED_ROTATING_DB_IO_H

#include "batched_io.h"

#include <libdevcore/db.h>

#include <boost/filesystem.hpp>

#include <deque>

namespace batched_io {

class batched_rotating_db_io : public batched_face {
private:
    const boost::filesystem::path base_path;
    std::deque< std::unique_ptr< dev::db::DatabaseFace > > pieces;
    size_t current_piece_file_no;

public:
    using const_iterator = std::deque< std::unique_ptr< dev::db::DatabaseFace > >::const_iterator;
    batched_rotating_db_io( const boost::filesystem::path& _path, size_t _nPieces );
    const_iterator begin() const { return pieces.begin(); }
    const_iterator end() const { return pieces.end(); }
    dev::db::DatabaseFace* current_piece() const { return begin()->get(); }
    size_t pieces_count() const { return pieces.size(); }
    void rotate();
    virtual void revert() { /* no need - as all write is in rotate() */
    }
    virtual void commit() { /*already implemented in rotate()*/
    }
    virtual ~batched_rotating_db_io();

protected:
    virtual void recover();
};

}  // namespace batched_io

#endif  // BATCHED_ROTATING_DB_IO_H
