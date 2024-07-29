#ifndef BATCHED_BLOCKS_AND_EXTRAS_H
#define BATCHED_BLOCKS_AND_EXTRAS_H

#include "batched_io.h"

#include <libdevcore/DBImpl.h>
#include <libdevcore/LevelDB.h>

#include <shared_mutex>

namespace batched_io {

class db_operations_face {
public:
    virtual void insert( dev::db::Slice _key, dev::db::Slice _value ) = 0;
    virtual void kill( dev::db::Slice _key ) = 0;

    // readonly
    virtual std::string lookup( dev::db::Slice _key ) const = 0;
    virtual bool exists( dev::db::Slice _key ) const = 0;
    virtual void forEach( std::function< bool( dev::db::Slice, dev::db::Slice ) > f ) const = 0;
    virtual void forEachWithPrefix(
        std::string& _prefix, std::function< bool( dev::db::Slice, dev::db::Slice ) > f ) const = 0;
    virtual ~db_operations_face() = default;
};

class db_face : public db_operations_face, public batched_face {};

class batched_db : public db_face {
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
    void insert( dev::db::Slice _key, dev::db::Slice _value ) override {
        std::lock_guard< std::mutex > batch_lock( m_batch_mutex );
        ensure_batch();
        m_batch->insert( _key, _value );
    }
    void kill( dev::db::Slice _key ) override {
        std::lock_guard< std::mutex > batch_lock( m_batch_mutex );
        ensure_batch();
        m_batch->kill( _key );
    }
    void revert() override {
        std::lock_guard< std::mutex > batch_lock( m_batch_mutex );
        if ( m_batch )
            m_batch.reset();
        m_db->discardCreatedBatches();
    }
    void commit( const std::string& test_crash_string = std::string() ) override {
        std::lock_guard< std::mutex > batch_lock( m_batch_mutex );
        ensure_batch();
        test_crash_before_commit( test_crash_string );
        m_db->commit( std::move( m_batch ) );
    }

    // readonly
    std::string lookup( dev::db::Slice _key ) const override { return m_db->lookup( _key ); }

    bool exists( dev::db::Slice _key ) const override { return m_db->exists( _key ); }

    void forEach( std::function< bool( dev::db::Slice, dev::db::Slice ) > f ) const override{
        std::lock_guard< std::mutex > foreach_lock( m_batch_mutex );
        m_db->forEach( f );
    }

    void forEachWithPrefix(
        std::string& _prefix, std::function< bool( dev::db::Slice, dev::db::Slice ) > f ) const override {
        std::lock_guard< std::mutex > foreach_lock( m_batch_mutex );
        m_db->forEachWithPrefix( _prefix, f );
    }

    ~batched_db() override{
        // all batches should be either commit()'ted or revert()'ed!
        assert( !m_batch );
    }

protected:
    void recover() override { /*nothing*/ }
};


class read_only_snap_based_batched_db : public db_face {
private:
    std::shared_ptr< dev::db::DBImpl > m_db;
    std::shared_ptr< dev::db::LevelDBSnap > m_snap;

public:

    read_only_snap_based_batched_db( std::shared_ptr< dev::db::DBImpl > _db,
        std::shared_ptr< dev::db::LevelDBSnap > _snap ) {
        LDB_CHECK(_db);
        LDB_CHECK(_snap);
        m_db = _db;
        m_snap = _snap;
    }

    bool is_open() const  { return !!m_db; };

    void insert( dev::db::Slice, dev::db::Slice ) override {
        throw std::runtime_error("Function not implemented:" + std::string(__FUNCTION__));
    }

    void kill( dev::db::Slice )  override{
        throw std::runtime_error("Function not implemented:" + std::string(__FUNCTION__));
    }

    void revert() override {
        throw std::runtime_error("Function not implemented:" + std::string(__FUNCTION__));
    }

    void commit( const std::string& ) override{
        throw std::runtime_error("Function not implemented:" + std::string(__FUNCTION__));
    }

    // readonly
    std::string lookup( dev::db::Slice _key ) const  override{ return m_db->lookup( _key, m_snap ); }

    bool exists( dev::db::Slice _key ) const override { return m_db->exists( _key, m_snap ); }

    void forEach( std::function< bool( dev::db::Slice, dev::db::Slice ) >  _f) const override{
        static std::string emptyString;
        return forEachWithPrefix( emptyString, _f );
    }

    void forEachWithPrefix(
        std::string& _prefix, std::function< bool( dev::db::Slice, dev::db::Slice ) >  _f) const override {
        m_db->forEachWithPrefix( _prefix, _f, m_snap );
    }

    virtual ~read_only_snap_based_batched_db() = default;

protected:
    void recover() override { /*nothing*/ }
};


class db_splitter {
private:
    std::shared_ptr< db_face > m_backend;
    std::vector< std::shared_ptr< db_face > > m_interfaces;

public:
    db_splitter( std::shared_ptr< db_face > _backend ) : m_backend( _backend ) {}
    db_operations_face* new_interface();
    db_face* backend() const { return m_backend.get(); }

private:
    class prefixed_db : public db_face {
    private:
        char prefix;
        std::shared_ptr< db_face > backend;

    public:
        prefixed_db( char _prefix, std::shared_ptr< db_face > _backend );
        virtual void insert( dev::db::Slice _key, dev::db::Slice _value );
        virtual void kill( dev::db::Slice _key );
        virtual void revert() { backend->revert(); }
        virtual void commit( const std::string& test_crash_string = std::string() ) {
            backend->commit( test_crash_string );
        }

        // readonly
        virtual std::string lookup( dev::db::Slice _key ) const;
        virtual bool exists( dev::db::Slice _key ) const;
        virtual void forEach( std::function< bool( dev::db::Slice, dev::db::Slice ) > f ) const;
        virtual void forEachWithPrefix(
            std::string& _prefix, std::function< bool( dev::db::Slice, dev::db::Slice ) > f ) const;

    protected:
        virtual void recover() { /* nothing */ }
    };
};

}  // namespace batched_io

#endif  // BATCHED_BLOCKS_AND_EXTRAS_H
