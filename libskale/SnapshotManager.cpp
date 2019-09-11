#include "SnapshotManager.h"

#include <skutils/btrfs.h>

#include <string>

using namespace std;
namespace fs = boost::filesystem;

// exceptions:
// - bad data dir
// - not btrfs
// - volumes don't exist
SnapshotManager::SnapshotManager(
    const fs::path& _dataDir, const std::vector< std::string >& _volumes ) {
    assert( _volumes.size() > 0 );

    if ( !fs::exists( _dataDir ) )
        throw InvalidPath( _dataDir );

    if ( btrfs.subvolume.list( _dataDir.c_str() ) )
        throw CannotPerformBtrfsOperation( btrfs.strerror() );

    for ( const auto& vol : _volumes ) {
        if ( !fs::exists( _dataDir / vol ) )
            throw InvalidPath( _dataDir / vol );
    }  // for

    data_dir = _dataDir;
    volumes = _volumes;
    snapshots_dir = data_dir / "snapshots";

    try {
        fs::create_directory( snapshots_dir );
    } catch ( ... ) {
        std::throw_with_nested( CannotCreate( snapshots_dir ) );
    }  // catch
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
    } catch ( fs::filesystem_error ) {
        std::throw_with_nested( CannotRead( snapshot_dir ) );
    }  // catch

    try {
        fs::create_directory( snapshot_dir );
    } catch ( fs::filesystem_error ) {
        std::throw_with_nested( CannotCreate( snapshot_dir ) );
    }  // catch

    for ( const string& vol : volumes ) {
        int res = btrfs.subvolume.snapshot_r( ( data_dir / vol ).c_str(), snapshot_dir.c_str() );
        if ( res )
            throw CannotPerformBtrfsOperation( btrfs.strerror() );
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
            throw CannotDelete( data_dir / vol );

        if ( btrfs.subvolume.snapshot(
                 ( snapshots_dir / to_string( _blockNumber ) / vol ).c_str(), data_dir.c_str() ) )
            throw CannotPerformBtrfsOperation( btrfs.strerror() );
    }
}

// exceptions:
// - no such snapshots
// - cannot read
// - cannot create tmp file
boost::filesystem::path SnapshotManager::makeDiff( unsigned _fromBlock, unsigned _toBlock ) {
    string filename_string = "/tmp/skaled_snapshot_" + to_string( _toBlock ) + "_XXXXXX";
    char buf[filename_string.length() + 1];
    strcpy( buf, filename_string.c_str() );
    int fd = mkstemp( buf );
    if ( fd < 0 ) {
        throw CannotCreateTmpFile( strerror( errno ) );
    }
    close( fd );

    fs::path path( buf );

    try {
        if ( !fs::exists( snapshots_dir / to_string( _fromBlock ) ) )
            throw SnapshotAbsent( _fromBlock );
        if ( !fs::exists( snapshots_dir / to_string( _toBlock ) ) )
            throw SnapshotAbsent( _toBlock );
    } catch ( const fs::filesystem_error& ex ) {
        std::throw_with_nested( CannotRead( ex.path1() ) );
    }

    for ( const string& vol : volumes ) {
        if ( btrfs.send( ( snapshots_dir / to_string( _fromBlock ) / vol ).c_str(), path.c_str(),
                 ( snapshots_dir / to_string( _toBlock ) / vol ).c_str() ) )
            throw CannotPerformBtrfsOperation( btrfs.strerror() );
    }

    return path;
}

// exceptions:
// - no such file/cannot read
// - cannot input as diff (no base state?)
void SnapshotManager::importDiff(
    unsigned _blockNumber, const boost::filesystem::path& _diffPath ) {
    if ( !fs::is_regular_file( _diffPath ) )
        throw InvalidPath( _diffPath );

    fs::path snapshot_dir = snapshots_dir / to_string( _blockNumber );

    if ( fs::exists( snapshot_dir ) )
        throw SnapshotPresent( _blockNumber );

    try {
        fs::create_directory( snapshot_dir );
    } catch ( ... ) {
        std::throw_with_nested( CannotCreate( snapshot_dir ) );
    }  // catch

    btrfs.receive( _diffPath.c_str(), ( snapshots_dir / to_string( _blockNumber ) ).c_str() );
}
