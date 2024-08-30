#include "batched_db.h"

namespace batched_io {

using namespace dev::db;

batched_db::~batched_db() {
    // all batches should be either commit()'ted or revert()'ed!
    assert( !m_batch );
}

db_operations_face* db_splitter::new_interface() {
    assert( this->m_interfaces.size() < 256 );

    unsigned char prefix = this->m_interfaces.size();

    db_face* pdb = new prefixed_db( prefix, m_backend );
    m_interfaces.emplace_back( pdb );

    return pdb;
}

db_splitter::prefixed_db::prefixed_db( char _prefix, std::shared_ptr< db_face > _backend )
    : prefix( _prefix ), backend( _backend ) {}

void db_splitter::prefixed_db::insert( dev::db::Slice _key, dev::db::Slice _value ) {
    std::vector< char > key2 = _key.toVector();
    key2.insert( key2.begin(), prefix );

    backend->insert( dev::ref( key2 ), _value );
}
void db_splitter::prefixed_db::kill( dev::db::Slice _key ) {
    std::vector< char > key2 = _key.toVector();
    key2.insert( key2.begin(), prefix );

    backend->kill( dev::ref( key2 ) );
}

std::string db_splitter::prefixed_db::lookup( dev::db::Slice _key ) const {
    assert( _key.size() >= 1 );

    std::vector< char > key2 = _key.toVector();
    key2.insert( key2.begin(), prefix );
    return backend->lookup( dev::ref( key2 ) );
}
bool db_splitter::prefixed_db::exists( dev::db::Slice _key ) const {
    std::vector< char > key2 = _key.toVector();
    key2.insert( key2.begin(), prefix );

    return backend->exists( dev::ref( key2 ) );
}
void db_splitter::prefixed_db::forEach(
    std::function< bool( dev::db::Slice, dev::db::Slice ) > f ) const {
    backend->forEach( [&]( dev::db::Slice _key, dev::db::Slice _val ) -> bool {
        if ( _key[0] != this->prefix )
            return true;
        dev::db::Slice key_short = dev::db::Slice( _key.data() + 1, _key.size() - 1 );
        return f( key_short, _val );
    } );
}

void db_splitter::prefixed_db::forEachWithPrefix(
    std::string& _prefix, std::function< bool( dev::db::Slice, dev::db::Slice ) > f ) const {
    backend->forEachWithPrefix( _prefix, [&]( dev::db::Slice _key, dev::db::Slice _val ) -> bool {
        if ( _key[0] != this->prefix )
            return true;
        dev::db::Slice key_short = dev::db::Slice( _key.data() + 1, _key.size() - 1 );
        return f( key_short, _val );
    } );
}

}  // namespace batched_io
