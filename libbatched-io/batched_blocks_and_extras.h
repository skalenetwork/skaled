#ifndef BATCHED_BLOCKS_AND_EXTRAS_H
#define BATCHED_BLOCKS_AND_EXTRAS_H

#include "batched_io.h"

#include <libdevcore/db.h>

namespace batched_io {

class batched_blocks_and_extras : public batched_io::batched_face {
private:
    dev::db::DatabaseFace* m_db;
    std::unique_ptr< dev::db::WriteBatchFace > m_batch;
    void ensure_batch() {
        if ( !m_batch )
            m_batch = m_db->createWriteBatch();
    }

public:
    void open( dev::db::DatabaseFace* _db ) { m_db = _db; }
    bool is_open() const { return !!m_db; }
    void insert( dev::db::Slice _key, dev::db::Slice _value ) {
        ensure_batch();
        m_batch->insert( _key, _value );
    }
    void kill( dev::db::Slice _key ) {
        ensure_batch();
        m_batch->kill( _key );
    }
    virtual void commit() {
        ensure_batch();
        m_db->commit( std::move( m_batch ) );
    }

protected:
    void recover() { /*nothing*/
    }
};

}  // namespace batched_io

#endif  // BATCHED_BLOCKS_AND_EXTRAS_H
