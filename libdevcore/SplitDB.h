#ifndef SPLITDB_H
#define SPLITDB_H

#include "LevelDB.h"
#include "RocksDB.h"

#include <boost/filesystem.hpp>

#include <shared_mutex>

namespace dev {
namespace db {

class SplitDB {
private:
    struct PrefixedWriteBatchFace : public WriteBatchFace {
        PrefixedWriteBatchFace( std::unique_ptr< WriteBatchFace > _backend, char _prefix );
        virtual void insert( Slice _key, Slice _value );
        virtual void kill( Slice _key );

        std::unique_ptr< WriteBatchFace > backend;
        std::list< std::vector< char > > store;
        char prefix;
    };

    class PrefixedDB : public DatabaseFace {
    public:
        PrefixedDB(
            char _prefix, std::shared_ptr< DatabaseFace > _backend, std::shared_mutex& _mutex );

        virtual std::string lookup( Slice _key ) const;
        virtual bool exists( Slice _key ) const;
        virtual void insert( Slice _key, Slice _value );
        virtual void kill( Slice _key );

        virtual std::unique_ptr< WriteBatchFace > createWriteBatch() const;
        virtual void commit( std::unique_ptr< WriteBatchFace > _batch );

        virtual void forEach( std::function< bool( Slice, Slice ) > f ) const;
        virtual void forEachWithPrefix(
            std::string& _prefix, std::function< bool( Slice, Slice ) > f ) const;

        virtual h256 hashBase() const;

    private:
        char prefix;
        std::shared_ptr< DatabaseFace > backend;
        std::shared_mutex& backend_mutex;
    };


public:
    SplitDB( std::shared_ptr< DatabaseFace > _backend );
    DatabaseFace* newInterface();
    DatabaseFace* getBackendInterface() const { return backend.get(); }

private:
    std::shared_ptr< DatabaseFace > backend;
    std::vector< std::shared_ptr< DatabaseFace > > interfaces;
    std::vector< std::unique_ptr< std::shared_mutex > > mutexes;
};

class LevelDBThroughSplit : public DatabaseFace {
public:
    static leveldb::ReadOptions defaultReadOptions() { return LevelDB::defaultReadOptions(); }
    static leveldb::WriteOptions defaultWriteOptions() { return LevelDB::defaultWriteOptions(); }
    static leveldb::Options defaultDBOptions() { return LevelDB::defaultDBOptions(); }

    explicit LevelDBThroughSplit( boost::filesystem::path const& _path,
        leveldb::ReadOptions _readOptions = defaultReadOptions(),
        leveldb::WriteOptions _writeOptions = defaultWriteOptions(),
        leveldb::Options _dbOptions = defaultDBOptions() ) {
        auto leveldb =
            std::make_shared< LevelDB >( _path, _readOptions, _writeOptions, _dbOptions );
        split_db = std::make_unique< SplitDB >( leveldb );
        backend = split_db->newInterface();
    }

    std::string lookup( Slice _key ) const override { return backend->lookup( _key ); }

    bool exists( Slice _key ) const override { return backend->exists( _key ); }

    void insert( Slice _key, Slice _value ) override { backend->insert( _key, _value ); }

    void kill( Slice _key ) override { backend->kill( _key ); }

    std::unique_ptr< WriteBatchFace > createWriteBatch() const override {
        return backend->createWriteBatch();
    }

    void commit( std::unique_ptr< WriteBatchFace > _batch ) override {
        backend->commit( std::move( _batch ) );
    }

    void forEach( std::function< bool( Slice, Slice ) > f ) const override {
        backend->forEach( f );
    }

    h256 hashBase() const override { return backend->hashBase(); }

private:
    std::unique_ptr< SplitDB > split_db;
    DatabaseFace* backend;
};

}  // namespace db
}  // namespace dev

#endif  // SPLITDB_H
