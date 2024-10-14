#ifndef ROTATINGHISTORICSTATE_H
#define ROTATINGHISTORICSTATE_H

#include "LevelDB.h"

#include <libbatched-io/BatchedRotatingHistoricDbIO.h>

#include <set>
#include <shared_mutex>

namespace dev {
namespace db {

class RotatingHistoricState : public DatabaseFace {
private:
    std::shared_ptr< batched_io::BatchedRotatingHistoricDbIO > ioBackend;

    mutable std::set< WriteBatchFace* > batch_cache;
    mutable std::shared_mutex m_mutex;

public:
    RotatingHistoricState( std::shared_ptr< batched_io::BatchedRotatingHistoricDbIO > ioBackend );
    void rotate( uint64_t timestamp );
    std::shared_ptr< dev::db::DatabaseFace > currentPiece() const {
        return ioBackend->currentPiece();
    }

    using DatabaseFace::lookup;  // 1-argument version
    virtual std::string lookup( Slice _key, uint64_t _rootBlockTimestamp ) const;
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

#endif  // ROTATINGHISTORICSTATE_H
