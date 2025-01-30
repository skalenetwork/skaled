#include "SplitDB.h"

#include <memory>
#include <mutex>

namespace dev {
namespace db {


SplitDB::SplitDB( std::shared_ptr< DatabaseFace > _backend ) : backend( _backend ) {}

DatabaseFace* SplitDB::newInterface() {
    assert( this->interfaces.size() < 256 );

    unsigned char prefix = this->interfaces.size();

    mutexes.push_back( std::make_unique< std::shared_mutex >() );
    PrefixedDB* pdb = new PrefixedDB( prefix, backend, *mutexes.back() );
    interfaces.emplace_back( pdb );

    return pdb;
}

SplitDB::PrefixedWriteBatchFace::PrefixedWriteBatchFace(
    std::unique_ptr< WriteBatchFace > _backend, char _prefix )
    : backend( std::move( _backend ) ), prefix( _prefix ) {}

void SplitDB::PrefixedWriteBatchFace::insert( Slice _key, Slice _value ) {
    std::vector< char > key2 = _key.toVector();
    key2.insert( key2.begin(), prefix );

    store.push_back( std::move( key2 ) );
    backend->insert( ref( store.back() ), _value );
}

// TODO Unit test this!
void SplitDB::PrefixedWriteBatchFace::kill( Slice _key ) {
    std::vector< char > key2 = _key.toVector();
    key2.insert( key2.begin(), prefix );

    store.push_back( std::move( key2 ) );
    backend->kill( ref( store.back() ) );
}

SplitDB::PrefixedDB::PrefixedDB(
    char _prefix, std::shared_ptr< DatabaseFace > _backend, std::shared_mutex& _mutex )
    : prefix( _prefix ), backend( _backend ), backend_mutex( _mutex ) {}

std::string SplitDB::PrefixedDB::lookup( Slice _key ) const {
    std::shared_lock< std::shared_mutex > lock( this->backend_mutex );
    assert( _key.size() >= 1 );

    std::vector< char > key2 = _key.toVector();
    key2.insert( key2.begin(), prefix );
    return backend->lookup( ref( key2 ) );
}

bool SplitDB::PrefixedDB::exists( Slice _key ) const {
    std::shared_lock< std::shared_mutex > lock( this->backend_mutex );
    std::vector< char > key2 = _key.toVector();
    key2.insert( key2.begin(), prefix );

    return backend->exists( ref( key2 ) );
}

void SplitDB::PrefixedDB::insert( Slice _key, Slice _value ) {
    std::unique_lock< std::shared_mutex > lock( this->backend_mutex );
    std::vector< char > key2 = _key.toVector();
    key2.insert( key2.begin(), prefix );

    backend->insert( ref( key2 ), _value );
}

void SplitDB::PrefixedDB::kill( Slice _key ) {
    std::unique_lock< std::shared_mutex > lock( this->backend_mutex );
    std::vector< char > key2 = _key.toVector();
    key2.insert( key2.begin(), prefix );

    backend->kill( ref( key2 ) );
}

std::unique_ptr< WriteBatchFace > SplitDB::PrefixedDB::createWriteBatch() const {
    std::unique_lock< std::shared_mutex > lock( this->backend_mutex );
    auto back = backend->createWriteBatch();
    return std::make_unique< PrefixedWriteBatchFace >( std::move( back ), this->prefix );
}
void SplitDB::PrefixedDB::commit( std::unique_ptr< WriteBatchFace > _batch ) {
    std::unique_lock< std::shared_mutex > lock( this->backend_mutex );
    PrefixedWriteBatchFace* pwb = dynamic_cast< PrefixedWriteBatchFace* >( _batch.get() );
    std::unique_ptr< WriteBatchFace > tmp = std::move( pwb->backend );
    backend->commit( std::move( tmp ) );
}

void SplitDB::PrefixedDB::forEach( std::function< bool( Slice, Slice ) > f ) const {
    std::unique_lock< std::shared_mutex > lock( this->backend_mutex );
    backend->forEach( [&]( Slice _key, Slice _val ) -> bool {
        if ( _key[0] != this->prefix )
            return true;
        Slice key_short = Slice( _key.data() + 1, _key.size() - 1 );
        return f( key_short, _val );
    } );
}


void SplitDB::PrefixedDB::forEachWithPrefix(
    std::string& _prefix, std::function< bool( Slice, Slice ) > f ) const {
    std::unique_lock< std::shared_mutex > lock( this->backend_mutex );
    auto prefixedString = std::to_string( this->prefix ) + _prefix;
    backend->forEachWithPrefix( prefixedString, [&]( Slice _key, Slice _val ) -> bool {
        if ( _key[0] != this->prefix )
            return true;
        Slice key_short = Slice( _key.data() + 1, _key.size() - 1 );
        return f( key_short, _val );
    } );
}


h256 SplitDB::PrefixedDB::hashBase() const {
    // HACK TODO implement that it would work with any DatabaseFace*
    const LevelDB* ldb = dynamic_cast< const LevelDB* >( backend.get() );
    if ( ldb )
        return ldb->hashBaseWithPrefix( prefix );
    else
        return h256();
}

}  // namespace db
}  // namespace dev
