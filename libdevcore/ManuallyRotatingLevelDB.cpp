#include "ManuallyRotatingLevelDB.h"

#include <secp256k1_sha256.h>

namespace dev {
namespace db {

namespace {

const std::string current_piece_mark_key =
    "ead48ec575aaa7127384dee432fc1c02d9f6a22950234e5ecf59f35ed9f6e78d";

}  // namespace

ManuallyRotatingLevelDB::ManuallyRotatingLevelDB(
    const boost::filesystem::path& _path, size_t _nPieces )
    : base_path( _path ) {
    std::unique_lock< std::shared_mutex > lock( m_mutex );

    size_t current_i = _nPieces;

    // open and find min size
    for ( size_t i = 0; i < _nPieces; ++i ) {
        boost::filesystem::path path = base_path / ( std::to_string( i ) + ".db" );
        DatabaseFace* db = new LevelDB( path );

        pieces.emplace_back( db );

        if ( db->exists( current_piece_mark_key ) ) {
            if ( current_i != _nPieces ) {
                DatabaseError ex;
                ex << errinfo_dbStatusCode( DatabaseStatus::Corruption )
                   << errinfo_dbStatusString( "Rotating DB has more then one 'current' piece" )
                   << errinfo_path( path.string() );
                BOOST_THROW_EXCEPTION( ex );
            }  // if error

            current_i = i;
        }  // if

    }  // for

    // if newly created DB
    if ( current_i == _nPieces )
        current_piece = 0;

    // rotate so min_i will be first
    for ( size_t i = 0; i < current_i; ++i ) {
        std::unique_ptr< DatabaseFace > el = std::move( pieces.front() );
        pieces.pop_front();
        pieces.push_back( std::move( el ) );
    }  // for

    this->current_piece = pieces.front().get();
    this->current_piece_file_no = current_i;
}

void ManuallyRotatingLevelDB::rotate() {
    std::unique_lock< std::shared_mutex > lock( m_mutex );

    assert( this->batch_cache.empty() );
    // we delete one below and make it current

    current_piece->kill( current_piece_mark_key );

    int old_db_no = current_piece_file_no - 1;
    if ( old_db_no < 0 )
        old_db_no += pieces.size();
    boost::filesystem::path old_path = base_path / ( std::to_string( old_db_no ) + ".db" );

    pieces.pop_back();  // will delete here
    boost::filesystem::remove_all( old_path );

    DatabaseFace* new_db = new LevelDB( old_path );
    pieces.emplace_front( new_db );

    current_piece_file_no = old_db_no;
    current_piece = new_db;

    current_piece->insert( current_piece_mark_key, std::string( "" ) );
}

std::string ManuallyRotatingLevelDB::lookup( Slice _key ) const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );

    for ( const auto& p : pieces ) {
        const std::string& v = p->lookup( _key );
        if ( !v.empty() )
            return v;
    }
    return std::string();
}

bool ManuallyRotatingLevelDB::exists( Slice _key ) const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );

    for ( const auto& p : pieces ) {
        if ( p->exists( _key ) )
            return true;
    }
    return false;
}

void ManuallyRotatingLevelDB::insert( Slice _key, Slice _value ) {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    current_piece->insert( _key, _value );
}

void ManuallyRotatingLevelDB::kill( Slice _key ) {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    for ( const auto& p : pieces )
        p->kill( _key );
}

std::unique_ptr< WriteBatchFace > ManuallyRotatingLevelDB::createWriteBatch() const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    std::unique_ptr< WriteBatchFace > wbf = current_piece->createWriteBatch();
    batch_cache.insert( wbf.get() );
    return wbf;
}
void ManuallyRotatingLevelDB::commit( std::unique_ptr< WriteBatchFace > _batch ) {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    batch_cache.erase( _batch.get() );
    current_piece->commit( std::move( _batch ) );
}

void ManuallyRotatingLevelDB::forEach( std::function< bool( Slice, Slice ) > f ) const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    for ( const auto& p : pieces ) {
        p->forEach( f );
    }
}

h256 ManuallyRotatingLevelDB::hashBase() const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    secp256k1_sha256_t ctx;
    secp256k1_sha256_initialize( &ctx );

    for ( const auto& p : pieces ) {
        h256 h = p->hashBase();
        secp256k1_sha256_write( &ctx, h.data(), h.size );
    }  // for

    h256 hash;
    secp256k1_sha256_finalize( &ctx, hash.data() );
    return hash;
}

}  // namespace db
}  // namespace dev
