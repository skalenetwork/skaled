#include "batched_rotating_db_io.h"

#include <libdevcore/RocksDB.h>

namespace batched_io {

using namespace dev::db;

namespace {
const std::string current_piece_mark_key =
    "ead48ec575aaa7127384dee432fc1c02d9f6a22950234e5ecf59f35ed9f6e78d";
}

rotating_db_io::rotating_db_io(
    const boost::filesystem::path& _path, size_t _nPieces, bool _archiveMode )
    : base_path( _path ), n_pieces( _nPieces ), archive_mode( _archiveMode ) {
    // open all
    for ( size_t i = 0; i < n_pieces; ++i ) {
        boost::filesystem::path path = base_path / ( std::to_string( i ) + ".db" );
        DatabaseFace* db = new RocksDB( path );
        pieces.emplace_back( db );
    }  // for

    // fix possible errors (i.e. duplicated mark key)
    recover();

    test_crash_before_commit( "after_recover" );

    // find current
    size_t current_i = n_pieces;
    for ( size_t i = 0; i < n_pieces; ++i ) {
        if ( pieces[i]->exists( current_piece_mark_key ) ) {
            current_i = i;
            break;
        }  // if
    }      // for

    // if newly created DB
    // TODO Correctly, we should write here into current_piece_mark_key
    // but as it is - it works ok too
    if ( current_i == n_pieces )
        current_i = 0;

    // TODO Generally, we shoud rotate in different direction, but in reality this doesn't matter
    // (but reverse it!) rotate so current_i will be first
    for ( size_t i = 0; i < current_i; ++i ) {
        std::unique_ptr< DatabaseFace > el = std::move( pieces.front() );
        pieces.pop_front();
        pieces.push_back( std::move( el ) );
    }  // for

    this->current_piece_file_no = current_i;

    // open archive
    if ( archive_mode ) {
        for ( size_t i = 0;; ++i ) {
            boost::filesystem::path path =
                base_path / ( "archive-" + ( std::to_string( i ) + ".db" ) );

            if ( !boost::filesystem::exists( path ) )
                break;

            DatabaseFace* db = new RocksDB( path );
            pieces.emplace_back( db );
        }  // for
    }      // archive_mode
}

void rotating_db_io::rotate() {
    // 1 remove or archive oldest
    int oldest_db_no = current_piece_file_no - 1;
    if ( oldest_db_no < 0 )
        oldest_db_no += n_pieces;

    int new_archive_db_no = pieces.size() - n_pieces;

    boost::filesystem::path oldest_path = base_path / ( std::to_string( oldest_db_no ) + ".db" );
    boost::filesystem::path new_archive_path =
        base_path / ( "archive-" + ( std::to_string( new_archive_db_no ) + ".db" ) );

    pieces.erase( pieces.begin() + n_pieces - 1 );  // will delete here

    // TODO test_crash here! (and think how to recover here!)
    if ( archive_mode ) {
        boost::filesystem::rename( oldest_path, new_archive_path );
        test_crash_before_commit( "after_rename_oldest" );
        DatabaseFace* new_archive_db = new RocksDB( new_archive_path );
        pieces.emplace_back( new_archive_db );
    } else {
        boost::filesystem::remove_all( oldest_path );  // delete oldest
        test_crash_before_commit( "after_remove_oldest" );
    }

    // 2 recreate it as new current
    DatabaseFace* new_db = new RocksDB( oldest_path );
    pieces.emplace_front( new_db );

    test_crash_before_commit( "after_open_leveldb" );

    current_piece_file_no = oldest_db_no;

    pieces[0]->insert( current_piece_mark_key, std::string( "" ) );

    test_crash_before_commit( "with_two_keys" );

    pieces[1]->kill( current_piece_mark_key );
}

void rotating_db_io::recover() {
    // delete 2nd mark
    // NB there can be 2 marked items in case of unfinished rotation
    // in this case do the following:
    // 0 and 1 -> choose 0
    // 1 and 2 -> choose 1
    // 2 and 3 -> choose 2
    // 3 and 4 -> choose 3
    // 0 and 4 -> choose 4

    for ( size_t i = 0; i < n_pieces; ++i ) {
        if ( pieces[i]->exists( current_piece_mark_key ) ) {
            size_t prev_i = ( i + n_pieces - 1 ) % n_pieces;
            if ( pieces[prev_i]->exists( current_piece_mark_key ) ) {
                pieces[i]->kill( current_piece_mark_key );
                test_crash_before_commit( "after_pieces_kill" );
            }  // if
        }      // if
    }          // for
}

rotating_db_io::~rotating_db_io() {}

}  // namespace batched_io
