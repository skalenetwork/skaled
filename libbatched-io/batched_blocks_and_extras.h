#ifndef BATCHED_BLOCKS_AND_EXTRAS_H
#define BATCHED_BLOCKS_AND_EXTRAS_H

#include "batched_io.h"

#include <libdevcore/db.h>

#include <shared_mutex>

namespace batched_io {

class batched_db_face : public batched_face {
public:
    virtual void insert( dev::db::Slice _key, dev::db::Slice _value ) = 0;
    virtual void kill( dev::db::Slice _key ) = 0;

    // readonly
    virtual std::string lookup( dev::db::Slice _key ) const = 0;
    virtual bool exists( dev::db::Slice _key ) const = 0;
    virtual void forEach( std::function< bool( dev::db::Slice, dev::db::Slice ) > f ) const = 0;
    ;
};

class batched_db : public batched_db_face {
private:
    std::shared_ptr< dev::db::DatabaseFace > m_db;
    std::unique_ptr< dev::db::WriteBatchFace > m_batch;
    mutable std::mutex m_batch_mutex;

    void ensure_batch() {
        if ( !m_batch )
            m_batch = m_db->createWriteBatch();
    }

public:
    void open( std::shared_ptr< dev::db::DatabaseFace > _db ) { m_db = _db; }
    bool is_open() const { return !!m_db; }
    void insert( dev::db::Slice _key, dev::db::Slice _value ) {
        std::lock_guard< std::mutex > batch_lock( m_batch_mutex );
        ensure_batch();
        m_batch->insert( _key, _value );
    }
    void kill( dev::db::Slice _key ) {
        std::lock_guard< std::mutex > batch_lock( m_batch_mutex );
        ensure_batch();
        m_batch->kill( _key );
    }
    virtual void revert() {
        std::lock_guard< std::mutex > batch_lock( m_batch_mutex );
        if ( m_batch )
            m_batch.reset();
    }
    virtual void commit() {
        std::lock_guard< std::mutex > batch_lock( m_batch_mutex );
        ensure_batch();
        m_db->commit( std::move( m_batch ) );
    }

    // readonly
    virtual std::string lookup( dev::db::Slice _key ) const { return m_db->lookup( _key ); }
    virtual bool exists( dev::db::Slice _key ) const { return m_db->exists( _key ); }
    virtual void forEach( std::function< bool( dev::db::Slice, dev::db::Slice ) > f ) const {
        std::lock_guard< std::mutex > foreach_lock( m_batch_mutex );
        m_db->forEach( f );
    }

    virtual ~batched_db();

protected:
    void recover() { /*nothing*/
    }
};

class batched_db_splitter {
private:
    std::shared_ptr< batched_db_face > m_backend;
    std::vector< std::shared_ptr< batched_db_face > > m_interfaces;

public:
    batched_db_splitter( std::shared_ptr< batched_db_face > _backend ) : m_backend( _backend ) {}
    batched_db_face* new_interface();
    batched_db_face* backend() const { return m_backend.get(); }

private:
    class prefixed_batched_db : public batched_db_face {
    private:
        char prefix;
        std::shared_ptr< batched_db_face > backend;

    public:
        prefixed_batched_db( char _prefix, std::shared_ptr< batched_db_face > _backend );
        virtual void insert( dev::db::Slice _key, dev::db::Slice _value );
        virtual void kill( dev::db::Slice _key );
        virtual void revert() { backend->revert(); }
        virtual void commit() { backend->commit(); }

        // readonly
        virtual std::string lookup( dev::db::Slice _key ) const;
        virtual bool exists( dev::db::Slice _key ) const;
        virtual void forEach( std::function< bool( dev::db::Slice, dev::db::Slice ) > f ) const;

    protected:
        virtual void recover() { /* nothing */
        }
    };
};

}  // namespace batched_io

#endif  // BATCHED_BLOCKS_AND_EXTRAS_H
