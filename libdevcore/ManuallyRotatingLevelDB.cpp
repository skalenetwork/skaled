#include "ManuallyRotatingLevelDB.h"

namespace dev {
namespace db {

namespace {
uint64_t dir_size( const boost::filesystem::path& _path ) {
    std::vector< boost::filesystem::path > files;
    boost::filesystem::create_directories( _path );

    uint64_t size = 0;

    for ( boost::filesystem::directory_iterator it = boost::filesystem::directory_iterator( _path );
          it != boost::filesystem::directory_iterator(); ++it ) {
        if ( is_regular_file( *it ) ) {
            size = size + boost::filesystem::file_size( *it );
        }
    }
    return size;
}
}  // namespace

ManuallyRotatingLevelDB::ManuallyRotatingLevelDB(
    const boost::filesystem::path& _path, size_t _nPieces )
    : base_path( _path ) {
    size_t min_i;
    uint64_t min_size;

    // open and find min size
    for ( size_t i = 0; i < _nPieces; ++i ) {
        boost::filesystem::path path = base_path / ( std::to_string( i ) + ".db" );
        DatabaseFace* db = new LevelDB( path );

        pieces.emplace_back( db );

        uint64_t size = dir_size( path );
        if ( i == 0 || size < min_size ) {
            min_size = size;
            min_i = i;
        }
    }  // for

    // rotate so min_i will be first
    for ( size_t i = 0; i < min_i; ++i ) {
        DatabaseFace* el = pieces.front().get();
        pieces.pop_front();
        pieces.emplace_back( el );
    }  // for

    this->current_piece = pieces.front().get();
    this->current_piece_file_no = min_i;
}

void ManuallyRotatingLevelDB::rotate() {
    assert( this->batch_cache.empty() );
    // we delete one below and make it current

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
}

std::string ManuallyRotatingLevelDB::lookup( Slice _key ) const {
    return current_piece->lookup( _key );
}

bool ManuallyRotatingLevelDB::exists( Slice _key ) const {
    return current_piece->exists( _key );
}

void ManuallyRotatingLevelDB::insert( Slice _key, Slice _value ) {
    current_piece->insert( _key, _value );
}

void ManuallyRotatingLevelDB::kill( Slice _key ) {
    current_piece->kill( _key );
}

std::unique_ptr< WriteBatchFace > ManuallyRotatingLevelDB::createWriteBatch() const {
    std::unique_ptr< WriteBatchFace > wbf = current_piece->createWriteBatch();
    batch_cache.insert( wbf.get() );
    return wbf;
}
void ManuallyRotatingLevelDB::commit( std::unique_ptr< WriteBatchFace > _batch ) {
    batch_cache.erase( _batch.get() );
    current_piece->commit( std::move( _batch ) );
}

void ManuallyRotatingLevelDB::forEach( std::function< bool( Slice, Slice ) > f ) const {
    current_piece->forEach( f );
}

h256 ManuallyRotatingLevelDB::hashBase() const {
    return h256();
}

}  // namespace db
}  // namespace dev
