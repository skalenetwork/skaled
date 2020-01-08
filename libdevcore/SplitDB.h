#ifndef SPLITDB_H
#define SPLITDB_H

#include "db.h"

#include <shared_mutex>

namespace dev {
namespace db {

class SplitDB {
private:
    class PrefixedWriteBatchFace : public WriteBatchFace {
    public:
        PrefixedWriteBatchFace( WriteBatchFace& _backend, char _prefix );
        virtual void insert( Slice _key, Slice _value );
        virtual void kill( Slice _key );

    private:
        WriteBatchFace& backend;
        char prefix;
        std::list< std::vector< char > > store;
    };

    class PrefixedDB : public DatabaseFace {
    public:
        PrefixedDB( char _prefix, DatabaseFace* _backend, std::shared_mutex& _mutex );

        virtual std::string lookup( Slice _key ) const;
        virtual bool exists( Slice _key ) const;
        virtual void insert( Slice _key, Slice _value );
        virtual void kill( Slice _key );

        virtual std::unique_ptr< WriteBatchFace > createWriteBatch() const;
        virtual void commit( std::unique_ptr< WriteBatchFace > _batch );

        virtual void forEach( std::function< bool( Slice, Slice ) > f ) const;
        virtual h256 hashBase() const;

    private:
        char prefix;
        DatabaseFace* backend;
        std::shared_mutex& backend_mutex;
    };


public:
    SplitDB( std::shared_ptr< DatabaseFace > _backend );
    DatabaseFace* newInterface();

private:
    std::shared_ptr< DatabaseFace > backend;
    std::vector< std::shared_ptr< DatabaseFace > > interfaces;
    std::vector< std::unique_ptr< std::shared_mutex > > mutexes;
};

}  // namespace db
}  // namespace dev

#endif  // SPLITDB_H
