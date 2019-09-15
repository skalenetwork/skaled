#include "SnapshotManager.h"

#include <skutils/btrfs.h>

#include <sstream>
#include <iostream>
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

    if ( !fs::exists( _dataDir ) ) try{
        throw InvalidPath( _dataDir );
    } catch(const fs::filesystem_error& ex){
        throw_with_nested(CannotRead(ex.path1()));
    }

    int res = btrfs.present( _dataDir.c_str() );
    if ( 0 != res ){
        throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
    }

    try {
        fprintf(stderr, "create: %d %d\n", geteuid(), getegid());
        fs::create_directory( snapshots_dir );
    } catch ( ... ) {
        std::throw_with_nested( CannotCreate( snapshots_dir ) );
    }  // catch

    for ( const auto& vol : _volumes ) try {
        if ( !fs::exists( _dataDir / vol ) )
            throw InvalidPath( _dataDir / vol );
        if(0 != btrfs.present((_dataDir / vol).c_str()))
            throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
    } catch(const fs::filesystem_error& ex){
        throw_with_nested(CannotRead(ex.path1()));
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
            throw CannotDelete( data_dir / vol );

        if ( btrfs.subvolume.snapshot(
                 ( snapshots_dir / to_string( _blockNumber ) / vol ).c_str(), data_dir.c_str() ) )
            throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
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

    stringstream cat_cmd;
        cat_cmd << "cat ";
    for ( const string& vol : volumes ) {

        string part_path = path.string()+"_"+vol;
        cat_cmd << part_path << " ";

        if ( btrfs.send( ( snapshots_dir / to_string( _fromBlock ) / vol ).c_str(), part_path.c_str(),
                 ( snapshots_dir / to_string( _toBlock ) / vol ).c_str() ) )
            throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
    }

    cat_cmd << ">" << path;
    int res = system(cat_cmd.str().c_str());            // TODO check exit code
    if(res != 0)
        throw CannotWrite(path);

    for(const string& vol: volumes) try {
        string part_path = path.string()+"_"+vol;
        fs::remove(part_path);
    } catch(const fs::filesystem_error& ex){
        throw_with_nested( CannotDelete(ex.path1()) );
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
