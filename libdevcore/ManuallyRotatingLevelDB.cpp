#include "ManuallyRotatingLevelDB.h"

#include <secp256k1_sha256.h>

namespace dev {
namespace db {

namespace {

const std::string current_piece_mark_key =
    "ead48ec575aaa7127384dee432fc1c02d9f6a22950234e5ecf59f35ed9f6e78d";

}  // namespace

ManuallyRotatingLevelDB::ManuallyRotatingLevelDB(
    std::shared_ptr< batched_rotating_db_io > _io_backend )
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
    io_backend->current_piece()->insert( _key, _value );
}

void ManuallyRotatingLevelDB::kill( Slice _key ) {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    for ( const auto& p : *io_backend )
        p->kill( _key );
}

std::unique_ptr< WriteBatchFace > ManuallyRotatingLevelDB::createWriteBatch() const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    std::unique_ptr< WriteBatchFace > wbf = io_backend->current_piece()->createWriteBatch();
    batch_cache.insert( wbf.get() );
    return wbf;
}
void ManuallyRotatingLevelDB::commit( std::unique_ptr< WriteBatchFace > _batch ) {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    batch_cache.erase( _batch.get() );
    io_backend->current_piece()->commit( std::move( _batch ) );
}

void ManuallyRotatingLevelDB::forEach( std::function< bool( Slice, Slice ) > f ) const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    for ( const auto& p : *io_backend ) {
        p->forEach( f );
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

batched_rotating_db_io::batched_rotating_db_io(
    const boost::filesystem::path& _path, size_t _nPieces )
    : base_path( _path ) {
    // open all
    for ( size_t i = 0; i < _nPieces; ++i ) {
        boost::filesystem::path path = base_path / ( std::to_string( i ) + ".db" );
        DatabaseFace* db = new LevelDB( path );
        pieces.emplace_back( db );
    }  // for

    // fix possible errors (i.e. duplicated mark key)
    recover();

    // find current
    size_t current_i = _nPieces;
    for ( size_t i = 0; i < _nPieces; ++i ) {
        if ( pieces[i]->exists( current_piece_mark_key ) ) {
            current_i = i;
            break;
        }  // if
    }      // for

    // if newly created DB
    // TODO Correctly, we should write here into current_piece_mark_key
    // but as it is - it works ok too
    if ( current_i == _nPieces )
        current_i = 0;

    // TODO Generally, we shoud rotate in different direction, but in reality this doesn't matter
    // (but reverse it!) rotate so min_i will be first
    for ( size_t i = 0; i < current_i; ++i ) {
        std::unique_ptr< DatabaseFace > el = std::move( pieces.front() );
        pieces.pop_front();
        pieces.push_back( std::move( el ) );
    }  // for

    this->current_piece_file_no = current_i;
}

void batched_rotating_db_io::rotate() {
    // 1 remove oldest
    int old_db_no = current_piece_file_no - 1;
    if ( old_db_no < 0 )
        old_db_no += pieces.size();
    boost::filesystem::path old_path = base_path / ( std::to_string( old_db_no ) + ".db" );
    boost::filesystem::remove_all( old_path );

    // 2 recreate it as new current
    DatabaseFace* new_db = new LevelDB( old_path );
    pieces.emplace_front( new_db );

    current_piece_file_no = old_db_no;

    pieces[0]->insert( current_piece_mark_key, std::string( "" ) );

    // NB crash in this place (between insert() and kill() is handled in recover()!

    // 3 clear previous current
    pieces.back()->kill( current_piece_mark_key );
    pieces.pop_back();  // will delete here
}

void batched_rotating_db_io::recover() {
    // delete 2nd mark
    // NB there can be 2 marked items in case of unfinished rotation
    // in this case do the following:
    // 0 and 1 -> choose 0
    // 1 and 2 -> choose 1
    // 2 and 3 -> choose 2
    // 3 and 4 -> choose 3
    // 0 and 4 -> choose 4

    for ( size_t i = 0; i < pieces.size(); ++i ) {
        if ( pieces[i]->exists( current_piece_mark_key ) ) {
            size_t prev_i = ( i + pieces.size() - 1 ) % pieces.size();
            if ( pieces[prev_i]->exists( current_piece_mark_key ) )
                pieces[i]->kill( current_piece_mark_key );
        }  // if
    }      // for
}

}  // namespace db
}  // namespace dev
