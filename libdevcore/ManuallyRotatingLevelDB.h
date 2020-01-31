#ifndef ROTATINGLEVELDB_H
#define ROTATINGLEVELDB_H

#include "LevelDB.h"

#include <deque>
#include <set>
#include <shared_mutex>

namespace dev {
namespace db {

class ManuallyRotatingLevelDB : public DatabaseFace {
private:
    const boost::filesystem::path base_path;
    DatabaseFace* current_piece;
    int current_piece_file_no;
    std::deque< std::unique_ptr< DatabaseFace > > pieces;

    mutable std::set< WriteBatchFace* > batch_cache;

    mutable std::shared_mutex m_mutex;

public:
    ManuallyRotatingLevelDB( const boost::filesystem::path& _path, size_t _nPieces );
    void rotate();

    virtual std::string lookup( Slice _key ) const;
    virtual bool exists( Slice _key ) const;
    virtual void insert( Slice _key, Slice _value );
    virtual void kill( Slice _key );

    virtual std::unique_ptr< WriteBatchFace > createWriteBatch() const;
    virtual void commit( std::unique_ptr< WriteBatchFace > _batch );

    virtual void forEach( std::function< bool( Slice, Slice ) > f ) const;
    virtual h256 hashBase() const;
};

}  // namespace db
}  // namespace dev

#endif  // ROTATINGLEVELDB_H
