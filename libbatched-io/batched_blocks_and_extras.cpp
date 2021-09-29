#include "batched_blocks_and_extras.h"

namespace batched_io {

using namespace dev::db;

batched_db::~batched_db() {
    // all batches should be either commit()'ted or revert()'ed!
    assert( !m_batch );
}

batched_db_face* batched_db_splitter::new_interface() {
    assert( this->m_interfaces.size() < 256 );

    unsigned char prefix = this->m_interfaces.size();

    batched_db_face* pdb = new prefixed_batched_db( prefix, m_backend );
    m_interfaces.emplace_back( pdb );

    return pdb;
}

batched_db_splitter::prefixed_batched_db::prefixed_batched_db(
    char _prefix, std::shared_ptr< batched_db_face > _backend )
    : prefix( _prefix ), backend( _backend ) {}

void batched_db_splitter::prefixed_batched_db::insert(
    dev::db::Slice _key, dev::db::Slice _value ) {
    std::vector< char > key2 = _key.toVector();
    key2.insert( key2.begin(), prefix );

    backend->insert( dev::ref( key2 ), _value );
}
void batched_db_splitter::prefixed_batched_db::kill( dev::db::Slice _key ) {
    std::vector< char > key2 = _key.toVector();
    key2.insert( key2.begin(), prefix );

    backend->kill( dev::ref( key2 ) );
}

std::string batched_db_splitter::prefixed_batched_db::lookup( dev::db::Slice _key ) const {
    assert( _key.size() >= 1 );

    std::vector< char > key2 = _key.toVector();
    key2.insert( key2.begin(), prefix );
    return backend->lookup( dev::ref( key2 ) );
}
bool batched_db_splitter::prefixed_batched_db::exists( dev::db::Slice _key ) const {
    std::vector< char > key2 = _key.toVector();
    key2.insert( key2.begin(), prefix );

    return backend->exists( dev::ref( key2 ) );
}
void batched_db_splitter::prefixed_batched_db::forEach(
    std::function< bool( dev::db::Slice, dev::db::Slice ) > f ) const {
    backend->forEach( [&]( dev::db::Slice _key, dev::db::Slice _val ) -> bool {
        if ( _key[0] != this->prefix )
            return true;
        dev::db::Slice key_short = dev::db::Slice( _key.data() + 1, _key.size() - 1 );
        return f( key_short, _val );
    } );
}

}  // namespace batched_io
