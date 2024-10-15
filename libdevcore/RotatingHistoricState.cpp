#include "RotatingHistoricState.h"

#include <secp256k1_sha256.h>

namespace dev {
namespace db {

using namespace batched_io;

RotatingHistoricState::RotatingHistoricState(
    std::shared_ptr< BatchedRotatingHistoricDbIO > ioBackend_ )
    : ioBackend( ioBackend_ ) {}

void RotatingHistoricState::rotate( uint64_t blockNumber ) {
    std::unique_lock< std::shared_mutex > lock( m_mutex );

    assert( this->batch_cache.empty() );

    ioBackend->rotate( blockNumber );
}

std::string RotatingHistoricState::lookup( Slice _key, uint64_t _rootBlockNumber ) const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );

    ioBackend->checkOpenedDbsAndCloseIfNeeded();

    if ( _key.toString() == std::string( "storageUsed" ) )
        return currentPiece()->lookup( _key );

    auto range = ioBackend->getRangeForBlockNumber( _rootBlockNumber );

    for ( auto it = range.first; it != range.second; ++it ) {
        auto db = ioBackend->getPieceByBlockNumber( *it );
        auto v = db->lookup( _key );
        if ( !v.empty() )
            return v;
    }

    return std::string();
}

bool RotatingHistoricState::exists( Slice _key ) const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );

    ioBackend->checkOpenedDbsAndCloseIfNeeded();

    auto range = ioBackend->getRangeForBlockNumber( UINT64_MAX );  // TODO check if it needs real
                                                                   // _timestamp

    for ( auto it = range.first; it != range.second; ++it ) {
        auto db = ioBackend->getPieceByBlockNumber( *it );
        if ( db->exists( _key ) )
            return true;
    }

    return false;
}

void RotatingHistoricState::insert( Slice _key, Slice _value ) {
    std::shared_lock< std::shared_mutex > lock( m_mutex );

    ioBackend->checkOpenedDbsAndCloseIfNeeded();

    currentPiece()->insert( _key, _value );
}

void RotatingHistoricState::kill( Slice _key ) {
    std::shared_lock< std::shared_mutex > lock( m_mutex );

    ioBackend->checkOpenedDbsAndCloseIfNeeded();

    auto range = ioBackend->getRangeForBlockNumber( UINT64_MAX );  // TODO check if it needs real
                                                                   // _timestamp

    for ( auto it = range.first; it != range.second; ++it ) {
        auto db = ioBackend->getPieceByBlockNumber( *it );
        db->kill( _key );
    }
}

std::unique_ptr< WriteBatchFace > RotatingHistoricState::createWriteBatch() const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );

    std::unique_ptr< WriteBatchFace > wbf = currentPiece()->createWriteBatch();
    batch_cache.insert( wbf.get() );

    return wbf;
}

void RotatingHistoricState::commit( std::unique_ptr< WriteBatchFace > _batch ) {
    std::shared_lock< std::shared_mutex > lock( m_mutex );

    batch_cache.erase( _batch.get() );
    currentPiece()->commit( std::move( _batch ) );
}

void RotatingHistoricState::forEach( std::function< bool( Slice, Slice ) > f ) const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );

    for ( auto blockNumber : ioBackend->getBlockNumbers() ) {
        auto db = ioBackend->getPieceByBlockNumber( blockNumber );
        db->forEach( f );
    }
}

void RotatingHistoricState::forEachWithPrefix(
    std::string& _prefix, std::function< bool( Slice, Slice ) > f ) const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );

    for ( auto blockNumber : ioBackend->getBlockNumbers() ) {
        auto db = ioBackend->getPieceByBlockNumber( blockNumber );
        db->forEachWithPrefix( _prefix, f );
    }
}

h256 RotatingHistoricState::hashBase() const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    secp256k1_sha256_t ctx;
    secp256k1_sha256_initialize( &ctx );

    for ( auto blockNumber : ioBackend->getBlockNumbers() ) {
        auto db = ioBackend->getPieceByBlockNumber( blockNumber );
        h256 h = db->hashBase();
        secp256k1_sha256_write( &ctx, h.data(), h.size );
    }

    h256 hash;
    secp256k1_sha256_finalize( &ctx, hash.data() );

    return hash;
}

}  // namespace db
}  // namespace dev
