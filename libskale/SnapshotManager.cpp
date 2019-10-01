#include "SnapshotManager.h"

#include <skutils/btrfs.h>

#include <iostream>
#include <map>
#include <sstream>
#include <string>

using namespace std;
namespace fs = boost::filesystem;

// Can manage snapshots as non-prvivileged user
// For send/receive neeeds root!

// exceptions:
// - bad data dir
// - not btrfs
// - volumes don't exist
SnapshotManager::SnapshotManager(
    const fs::path& _dataDir, const std::vector< std::string >& _volumes ) {
    assert( _volumes.size() > 0 );

    data_dir = _dataDir;
    volumes = _volumes;
    snapshots_dir = data_dir / "snapshots";
    diffs_dir = data_dir / "diffs";

    if ( !fs::exists( _dataDir ) )
        try {
            throw InvalidPath( _dataDir );
        } catch ( const fs::filesystem_error& ex ) {
            throw_with_nested( CannotRead( ex.path1() ) );
        }

    int res = btrfs.present( _dataDir.c_str() );
    if ( 0 != res ) {
        throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
    }

    try {
        fs::create_directory( snapshots_dir );
        fs::remove_all( diffs_dir );
        fs::create_directory( diffs_dir );
    } catch ( const fs::filesystem_error& ex ) {
        std::throw_with_nested( CannotWrite( ex.path1() ) );
    }  // catch

    for ( const auto& vol : _volumes )
        try {
            // throw if it is present but is NOT btrfs
            if ( fs::exists( _dataDir / vol ) && 0 != btrfs.present( ( _dataDir / vol ).c_str() ) )
                throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );

            btrfs.subvolume.create( ( _dataDir / vol ).c_str() );

        } catch ( const fs::filesystem_error& ex ) {
            throw_with_nested( CannotRead( ex.path1() ) );
        }
}

// exceptions:
// - exists
// - cannot read
// - cannot write
void SnapshotManager::doSnapshot( unsigned _blockNumber ) {
    fs::path snapshot_dir = snapshots_dir / to_string( _blockNumber );

    try {
        if ( fs::exists( snapshot_dir ) )
            throw SnapshotPresent( _blockNumber );
    } catch ( const fs::filesystem_error& ) {
        std::throw_with_nested( CannotRead( snapshot_dir ) );
    }  // catch

    try {
        fs::create_directory( snapshot_dir );
    } catch ( const fs::filesystem_error& ) {
        std::throw_with_nested( CannotCreate( snapshot_dir ) );
    }  // catch

    for ( const string& vol : volumes ) {
        int res = btrfs.subvolume.snapshot_r( ( data_dir / vol ).c_str(), snapshot_dir.c_str() );
        if ( res )
            throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
    }
}

// exceptions:
// - not found/cannot read
void SnapshotManager::restoreSnapshot( unsigned _blockNumber ) {
    try {
        if ( !fs::exists( snapshots_dir / to_string( _blockNumber ) ) )
            throw SnapshotAbsent( _blockNumber );
    } catch ( const fs::filesystem_error& ) {
        std::throw_with_nested( CannotRead( snapshots_dir / to_string( _blockNumber ) ) );
    }

    for ( const string& vol : volumes ) {
        if ( btrfs.subvolume._delete( ( data_dir / vol ).c_str() ) )
            throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );

        if ( btrfs.subvolume.snapshot(
                 ( snapshots_dir / to_string( _blockNumber ) / vol ).c_str(), data_dir.c_str() ) )
            throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
    }
}

// exceptions:
// - no such snapshots
// - cannot read
// - cannot create tmp file
boost::filesystem::path SnapshotManager::makeOrGetDiff( unsigned _fromBlock, unsigned _toBlock ) {
    fs::path path = getDiffPath( _fromBlock, _toBlock );

    try {
        if ( fs::is_regular( path ) )
            return path;

        if ( !fs::exists( snapshots_dir / to_string( _fromBlock ) ) ) {
            // TODO wrong error message if this fails
            fs::remove( path );
            throw SnapshotAbsent( _fromBlock );
        }
        if ( !fs::exists( snapshots_dir / to_string( _toBlock ) ) ) {
            // TODO wrong error message if this fails
            fs::remove( path );
            throw SnapshotAbsent( _toBlock );
        }
    } catch ( const fs::filesystem_error& ex ) {
        std::throw_with_nested( CannotRead( ex.path1() ) );
    }

    stringstream cat_cmd;
    cat_cmd << "cat ";
    vector< string > created;

    for ( const string& vol : volumes ) {
        string part_path = path.string() + "_" + vol;
        cat_cmd << part_path << " ";

        created.push_back( part_path );  // file is created even in case of error

        if ( btrfs.send( ( snapshots_dir / to_string( _fromBlock ) / vol ).c_str(),
                 part_path.c_str(), ( snapshots_dir / to_string( _toBlock ) / vol ).c_str() ) ) {
            try {
                fs::remove( path );
                for ( const string& vol : created )
                    fs::remove( vol );
            } catch ( const fs::filesystem_error& ex ) {
                throw_with_nested( CannotDelete( ex.path1() ) );
            }  // catch

            throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
        }  // if error

    }  // for

    cat_cmd << ">" << path;
    int cat_res = system( cat_cmd.str().c_str() );

    for ( const string& vol : created )
        try {
            fs::remove( vol );
        } catch ( const fs::filesystem_error& ex ) {
            throw_with_nested( CannotDelete( ex.path1() ) );
        }

    if ( cat_res != 0 )
        throw CannotWrite( path );

    return path;
}

// exceptions:
// - no such file/cannot read
// - cannot input as diff (no base state?)
void SnapshotManager::importDiff( unsigned _fromBlock, unsigned _toBlock ) {
    fs::path diffPath = getDiffPath( _fromBlock, _toBlock );
    fs::path snapshot_dir = snapshots_dir / to_string( _toBlock );

    try {
        if ( !fs::is_regular_file( diffPath ) )
            throw InvalidPath( diffPath );

        if ( fs::exists( snapshot_dir ) )
            throw SnapshotPresent( _toBlock );

    } catch ( const fs::filesystem_error& ex ) {
        throw_with_nested( CannotRead( ex.path1() ) );
    }

    try {
        fs::create_directory( snapshot_dir );
    } catch ( ... ) {
        std::throw_with_nested( CannotCreate( snapshot_dir ) );
    }  // catch

    if ( btrfs.receive( diffPath.c_str(), ( snapshots_dir / to_string( _toBlock ) ).c_str() ) ) {
        fs::remove_all( snapshot_dir );
        throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
    }  // if
}

boost::filesystem::path SnapshotManager::getDiffPath( unsigned _fromBlock, unsigned _toBlock ) {
    return diffs_dir / ( to_string( _fromBlock ) + "_" + to_string( _toBlock ) );
}

// exeptions: filesystem
void SnapshotManager::leaveNLastSnapshots( unsigned n ) {
    multimap< time_t, fs::path, std::greater< time_t > > time_map;
    for ( auto& f : fs::directory_iterator( snapshots_dir ) ) {
        time_map.insert( make_pair( fs::last_write_time( f ), f ) );
    }  // for

    // delete all efter n first
    unsigned i = 1;
    for ( const auto& p : time_map ) {
        if ( i > n ) {
            const fs::path& path = p.second;
            btrfs.subvolume._delete( path.c_str() );
        }  // if
    }      // for
}

// exeptions: filesystem
void SnapshotManager::leaveNLastDiffs( unsigned n ) {
    multimap< time_t, fs::path, std::greater< time_t > > time_map;
    for ( auto& f : fs::directory_iterator( diffs_dir ) ) {
        time_map.insert( make_pair( fs::last_write_time( f ), f ) );
    }  // for

    // delete all efter n first
    unsigned i = 1;
    for ( const auto& p : time_map ) {
        if ( i > n ) {
            const fs::path& path = p.second;
            fs::remove( path );
        }  // if
    }      // for
}
