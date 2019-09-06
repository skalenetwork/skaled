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

    data_dir = _dataDir;
    volumes = _volumes;
    snapshots_dir = data_dir / "snapshots";
}

// exceptions:
// - exists
// - cannot read
// - cannot write
void SnapshotManager::doSnapshot( unsigned _blockNumber ) {
    fs::path snapshot_dir = snapshots_dir / to_string( _blockNumber );
    fs::create_directory( snapshot_dir );  // TODO check exit code

    for ( const string& vol : volumes ) {
        btrfs.subvolume.snapshot_r( ( data_dir / vol ).c_str(), snapshot_dir.c_str() );
    }
}

// exceptions:
// - not found/cannot read
void SnapshotManager::restoreSnapshot( unsigned _blockNumber ) {
    for ( const string& vol : volumes ) {
        btrfs.subvolume._delete( ( data_dir / vol ).c_str() );
        btrfs.subvolume.snapshot(
            ( snapshots_dir / to_string( _blockNumber ) / vol ).c_str(), data_dir.c_str() );
    }
}

// exceptions:
// - no such snapshots
// - cannot read
// - cannot create tmp file
boost::filesystem::path SnapshotManager::makeDiff( unsigned _fromBlock, unsigned _toBlock ) {
    string filename_string = "skaled_snapshot_" + to_string( _toBlock ) + "_XXXXXX";
    char buf[filename_string.length() + 1];
    strcpy( buf, filename_string.c_str() );
    int fd = mkstemp( buf );
    close( fd );

    fs::path path( buf );

    for ( const string& vol : volumes ) {
        btrfs.send( ( snapshots_dir / to_string( _fromBlock ) / vol ).c_str(), path.c_str(),
            ( snapshots_dir / to_string( _toBlock ) / vol ).c_str() );
    }

    return path;
}

// exceptions:
// - no such file/cannot read
// - cannot input as diff (no base state?)
void SnapshotManager::importDiff(
    unsigned _blockNumber, const boost::filesystem::path& _diffPath ) {
    fs::path snapshot_dir = snapshots_dir / to_string( _blockNumber );
    fs::create_directory( snapshot_dir );  // TODO check exit code

    btrfs.receive( _diffPath.c_str(), ( snapshots_dir / to_string( _blockNumber ) ).c_str() );
}
