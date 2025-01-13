#ifndef ROTATINGLEVELDB_H
#define ROTATINGLEVELDB_H

#include "LevelDB.h"

#include <libbatched-io/batched_rotating_db_io.h>

#include <deque>
#include <mutex>
#include <set>
#include <shared_mutex>

namespace dev {
namespace db {

class ManuallyRotatingLevelDB : public DatabaseFace {
private:
    std::shared_ptr< batched_io::rotating_db_io > io_backend;

    mutable std::set< WriteBatchFace* > batch_cache;
    mutable std::shared_mutex m_mutex;

public:
    ManuallyRotatingLevelDB( std::shared_ptr< batched_io::rotating_db_io > _io_backend );
    void rotate();
    size_t piecesCount() const { return io_backend->pieces_count(); }
    DatabaseFace* currentPiece() const { return io_backend->begin()->get(); }

    virtual std::string lookup( Slice _key ) const;
    virtual bool exists( Slice _key ) const;
    virtual void insert( Slice _key, Slice _value );
    virtual void kill( Slice _key );

    virtual std::unique_ptr< WriteBatchFace > createWriteBatch() const;
    virtual void commit( std::unique_ptr< WriteBatchFace > _batch );
    // batches don't survive rotation!
    bool discardCreatedBatches() {
        std::unique_lock< std::shared_mutex > lock( m_mutex );
        size_t size = batch_cache.size();
        batch_cache.clear();
        return size > 0;
    }

    virtual void forEach( std::function< bool( Slice, Slice ) > f ) const;
    virtual void forEachWithPrefix(
        std::string& _prefix, std::function< bool( Slice, Slice ) > f ) const;

    virtual h256 hashBase() const;
};

}  // namespace db
}  // namespace dev

#endif  // ROTATINGLEVELDB_H
