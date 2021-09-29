#include "batched_rotating_db_io.h"

#include <libdevcore/LevelDB.h>

namespace batched_io {

using namespace dev::db;

namespace {
const std::string current_piece_mark_key =
    "ead48ec575aaa7127384dee432fc1c02d9f6a22950234e5ecf59f35ed9f6e78d";
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
    int oldest_db_no = current_piece_file_no - 1;
    if ( oldest_db_no < 0 )
        oldest_db_no += pieces.size();
    boost::filesystem::path oldest_path = base_path / ( std::to_string( oldest_db_no ) + ".db" );
    pieces.pop_back();                             // will delete here
    boost::filesystem::remove_all( oldest_path );  // delete oldest

    // 2 recreate it as new current
    DatabaseFace* new_db = new LevelDB( oldest_path );
    pieces.emplace_front( new_db );

    current_piece_file_no = oldest_db_no;

    pieces[0]->insert( current_piece_mark_key, std::string( "" ) );
    // NB crash in this place (between insert() and kill() is handled in recover()!
    pieces[1]->kill( current_piece_mark_key );
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

batched_rotating_db_io::~batched_rotating_db_io() {}

}  // namespace batched_io
