#include "ManuallyRotatingLevelDB.h"

#include <secp256k1_sha256.h>

namespace dev {
namespace db {

using namespace batched_io;

ManuallyRotatingLevelDB::ManuallyRotatingLevelDB( std::shared_ptr< rotating_db_io > _io_backend )
    : io_backend( _io_backend ) {}

void ManuallyRotatingLevelDB::rotate() {
    std::unique_lock< std::shared_mutex > lock( m_mutex );
    assert( this->batch_cache.empty() );
    io_backend->rotate();
}

std::string ManuallyRotatingLevelDB::lookup( Slice _key ) const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );

    for ( const auto& p : *io_backend ) {
        const std::string& v = p->lookup( _key );
        if ( !v.empty() )
            return v;
    }
    return std::string();
}

bool ManuallyRotatingLevelDB::exists( Slice _key ) const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );

    for ( const auto& p : *io_backend ) {
        if ( p->exists( _key ) )
            return true;
    }
    return false;
}

void ManuallyRotatingLevelDB::insert( Slice _key, Slice _value ) {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    currentPiece()->insert( _key, _value );
}

void ManuallyRotatingLevelDB::kill( Slice _key ) {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    for ( const auto& p : *io_backend )
        p->kill( _key );
}

std::unique_ptr< WriteBatchFace > ManuallyRotatingLevelDB::createWriteBatch() const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    std::unique_ptr< WriteBatchFace > wbf = currentPiece()->createWriteBatch();
    batch_cache.insert( wbf.get() );
    return wbf;
}
void ManuallyRotatingLevelDB::commit( std::unique_ptr< WriteBatchFace > _batch ) {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    batch_cache.erase( _batch.get() );
    currentPiece()->commit( std::move( _batch ) );
}

void ManuallyRotatingLevelDB::forEach( std::function< bool( Slice, Slice ) > f ) const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    for ( const auto& p : *io_backend ) {
        p->forEach( f );
    }
}

void ManuallyRotatingLevelDB::forEachWithPrefix(
    std::string& _prefix, std::function< bool( Slice, Slice ) > f ) const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    for ( const auto& p : *io_backend ) {
        p->forEachWithPrefix( _prefix, f );
    }
}


h256 ManuallyRotatingLevelDB::hashBase() const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    secp256k1_sha256_t ctx;
    secp256k1_sha256_initialize( &ctx );

    for ( const auto& p : *io_backend ) {
        h256 h = p->hashBase();
        secp256k1_sha256_write( &ctx, h.data(), h.size );
    }  // for

    h256 hash;
    secp256k1_sha256_finalize( &ctx, hash.data() );
    return hash;
}

}  // namespace db
}  // namespace dev
