/*
    Copyright (C) 2019-present, SKALE Labs

    This file is part of skaled.

    skaled is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skaled is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skaled.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * @file SnapshotManager.cpp
 * @author Dima Litvinov
 * @date 2019
 */


#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "UnsafeRegion.h"
#include "boost/filesystem.hpp"
#include <libbatched-io/batched_io.h>
#include <libdevcore/LevelDB.h>
#include <libdevcore/Log.h>
#include <libdevcrypto/Hash.h>
#include <skutils/btrfs.h>
#include <boost/filesystem/operations.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>


#include "SnapshotManager.h"

using namespace std;
namespace fs = boost::filesystem;

// Can manage snapshots as non-prvivileged user
// For send/receive needs root!

const std::string SnapshotManager::snapshotHashFileName = "snapshot_hash.txt";

// exceptions:
// - bad data dir
// - not btrfs
// - volumes don't exist
SnapshotManager::SnapshotManager( const dev::eth::ChainParams& _chainParams,
    const fs::path& _dataDir, const std::string& _diffsDir )
    : chainParams( _chainParams ) {
    dataDir = _dataDir;
    coreVolumes = { dev::eth::BlockChain::getChainDirName( chainParams ), "filestorage",
        "prices_" + chainParams.nodeInfo.id.str() + ".db",
        "blocks_" + chainParams.nodeInfo.id.str() + ".db" };

#ifdef HISTORIC_STATE
    archiveVolumes = { "historic_roots", "historic_state" };
#else
    archiveVolumes = {};
#endif

    allVolumes.reserve( coreVolumes.size() + archiveVolumes.size() );
    allVolumes.insert( allVolumes.end(), coreVolumes.begin(), coreVolumes.end() );
#ifdef HISTORIC_STATE
    allVolumes.insert( allVolumes.end(), archiveVolumes.begin(), archiveVolumes.end() );
#endif

    snapshotsDir = dataDir / "snapshots";
    if ( _diffsDir.empty() )
        diffsDir = dataDir / "diffs";
    else
        diffsDir = _diffsDir;

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
        fs::create_directory( snapshotsDir );
        if ( _diffsDir.empty() ) {
            fs::remove_all( diffsDir );
            fs::create_directory( diffsDir );
        }
    } catch ( const fs::filesystem_error& ex ) {
        std::throw_with_nested( CannotWrite( ex.path1() ) );
    }  // catch

    for ( const auto& vol : allVolumes )
        try {
            // throw if it is present but is NOT btrfs
            if ( fs::exists( _dataDir / vol ) && 0 != btrfs.present( ( _dataDir / vol ).c_str() ) )
                throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );

            // Ignoring exception
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
    fs::path snapshotDir = snapshotsDir / to_string( _blockNumber );

    UnsafeRegion::lock ur_lock;

    try {
        if ( fs::exists( snapshotDir ) )
            throw SnapshotPresent( _blockNumber );
    } catch ( const fs::filesystem_error& ) {
        std::throw_with_nested( CannotRead( snapshotDir ) );
    }  // catch

    try {
        fs::create_directory( snapshotDir );
    } catch ( const fs::filesystem_error& ) {
        std::throw_with_nested( CannotCreate( snapshotDir ) );
    }  // catch

    int dummy_counter = 0;
    for ( const string& vol : allVolumes ) {
        int res = btrfs.subvolume.snapshot_r( ( dataDir / vol ).c_str(), snapshotDir.c_str() );
        if ( res )
            throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
        if ( dummy_counter++ == 1 )
            batched_io::test_crash_before_commit( "SnapshotManager::doSnapshot" );
    }  // for
}

// exceptions:
// - not found/cannot read
void SnapshotManager::restoreSnapshot( unsigned _blockNumber ) {
    try {
        if ( !fs::exists( snapshotsDir / to_string( _blockNumber ) ) )
            throw SnapshotAbsent( _blockNumber );
    } catch ( const fs::filesystem_error& ) {
        std::throw_with_nested( CannotRead( snapshotsDir / to_string( _blockNumber ) ) );
    }

    UnsafeRegion::lock ur_lock;

    std::vector< std::string > volumes = coreVolumes;
#ifdef HISTORIC_STATE
    if ( _blockNumber > 0 )
        volumes = allVolumes;
#endif

    int dummy_counter = 0;
    for ( const string& vol : volumes ) {
        if ( fs::exists( dataDir / vol ) ) {
            if ( btrfs.subvolume._delete( ( dataDir / vol ).c_str() ) )
                throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
        }
        if ( btrfs.subvolume.snapshot(
                 ( snapshotsDir / to_string( _blockNumber ) / vol ).c_str(), dataDir.c_str() ) )
            throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );

        if ( dummy_counter++ == 1 )
            batched_io::test_crash_before_commit( "SnapshotManager::doSnapshot" );

    }  // for

    if ( _blockNumber == 0 ) {
#ifdef HISTORIC_STATE
        for ( const string& vol : allVolumes ) {
            // continue if already present
            if ( fs::exists( dataDir / vol ) && 0 == btrfs.present( ( dataDir / vol ).c_str() ) )
                continue;

            // create if not created yet ( only makes sense for historic nodes and 0 block number )
            btrfs.subvolume.create( ( dataDir / vol ).c_str() );
        }  // for
#endif
    }
}

// exceptions:
// - no such snapshots
// - cannot read
// - cannot create tmp file
boost::filesystem::path SnapshotManager::makeOrGetDiff( unsigned _toBlock ) {
    fs::path path = getDiffPath( _toBlock );

    try {
        if ( fs::is_regular( path ) )
            return path;

        if ( !fs::exists( snapshotsDir / to_string( _toBlock ) ) ) {
            // TODO wrong error message if this fails
            fs::remove( path );
            throw SnapshotAbsent( _toBlock );
        }
    } catch ( const fs::filesystem_error& ex ) {
        std::throw_with_nested( CannotRead( ex.path1() ) );
    }

    stringstream volumes_cat;

    std::vector< std::string > volumes = _toBlock > 0 ? allVolumes : coreVolumes;
    for ( auto it = volumes.begin(); it != volumes.end(); ++it ) {
        const string& vol = *it;
        if ( it + 1 != volumes.end() )
            volumes_cat << ( snapshotsDir / to_string( _toBlock ) / vol ).string() << " ";
        else
            volumes_cat << ( snapshotsDir / to_string( _toBlock ) / vol ).string();
    }  // for cat

    UnsafeRegion::lock ur_lock;

    if ( btrfs.send( NULL, path.c_str(), volumes_cat.str().c_str() ) ) {
        try {
            fs::remove( path );
        } catch ( const fs::filesystem_error& ex ) {
            throw_with_nested( CannotDelete( ex.path1() ) );
        }  // catch

        throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
    }  // if error

    return path;
}

// exceptions:
// - no such file/cannot read
// - cannot input as diff (no base state?)
void SnapshotManager::importDiff( unsigned _toBlock ) {
    fs::path diffPath = getDiffPath( _toBlock );
    fs::path snapshot_dir = snapshotsDir / to_string( _toBlock );

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

    if ( btrfs.receive( diffPath.c_str(), ( snapshotsDir / to_string( _toBlock ) ).c_str() ) ) {
        auto ex = CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
        cleanupDirectory( snapshot_dir );
        fs::remove_all( snapshot_dir );
        throw ex;
    }  // if
}

boost::filesystem::path SnapshotManager::getDiffPath( unsigned _toBlock ) {
    // check existance
    assert( boost::filesystem::exists( diffsDir ) );
    return diffsDir / ( std::to_string( _toBlock ) );
}

void SnapshotManager::removeSnapshot( unsigned _blockNumber ) {
    if ( !fs::exists( snapshotsDir / to_string( _blockNumber ) ) ) {
        throw SnapshotAbsent( _blockNumber );
    }

    UnsafeRegion::lock ur_lock;

    int dummy_counter = 0;

    for ( const auto& volume : allVolumes ) {
        int res = btrfs.subvolume._delete(
            ( this->snapshotsDir / std::to_string( _blockNumber ) / volume ).string().c_str() );

        if ( res != 0 ) {
            throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
        }

        if ( dummy_counter++ == 1 )
            batched_io::test_crash_before_commit( "SnapshotManager::doSnapshot" );
    }

    fs::remove_all( snapshotsDir / to_string( _blockNumber ) );
}

void SnapshotManager::cleanupButKeepSnapshot( unsigned _keepSnapshot ) {
    this->cleanupDirectory( snapshotsDir, snapshotsDir / std::to_string( _keepSnapshot ) );
    this->cleanupDirectory( dataDir, snapshotsDir );
    if ( !fs::exists( diffsDir ) )
        try {
            boost::filesystem::create_directory( diffsDir );
        } catch ( const fs::filesystem_error& ex ) {
            std::throw_with_nested( CannotWrite( ex.path1() ) );
        }
}

void SnapshotManager::cleanup() {
    this->cleanupDirectory( snapshotsDir );
    this->cleanupDirectory( dataDir );

    try {
        boost::filesystem::create_directory( snapshotsDir );
        if ( !fs::exists( diffsDir ) )
            boost::filesystem::create_directory( diffsDir );
    } catch ( const fs::filesystem_error& ex ) {
        std::throw_with_nested( CannotWrite( ex.path1() ) );
    }  // catch
}

void SnapshotManager::cleanupDirectory(
    const boost::filesystem::path& p, const boost::filesystem::path& _keepDirectory ) {
    // remove all
    boost::filesystem::directory_iterator it( p ), end;

    while ( it != end ) {
        if ( boost::filesystem::is_directory( it->path() ) && it->path() != _keepDirectory ) {
            int res1 = 0, res2 = 0;
            try {
                res1 = btrfs.subvolume._delete( ( it->path() / "*" ).c_str() );
                res2 = btrfs.subvolume._delete( ( it->path() ).c_str() );

                boost::filesystem::remove_all( it->path() );
            } catch ( boost::filesystem::filesystem_error& ) {
                if ( res1 != 0 || res2 != 0 ) {
                    std::throw_with_nested(
                        CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() ) );
                }
                std::throw_with_nested( CannotDelete( it->path() ) );
            }
        }
        ++it;
    }
}

// exeptions: filesystem
void SnapshotManager::leaveNLastSnapshots( unsigned n ) {
    map< int, fs::path, std::greater< int > > numbers;
    for ( auto& f : fs::directory_iterator( snapshotsDir ) ) {
        // HACK We exclude 0 snapshot forcefully
        if ( fs::basename( f ) != "0" )
            numbers.insert( make_pair( std::stoi( fs::basename( f ) ), f ) );
    }  // for

    // delete all after n first
    unsigned i = 1;
    for ( const auto& p : numbers ) {
        if ( i++ > n ) {
            const fs::path& path = p.second;
            for ( const string& v : coreVolumes ) {
                if ( btrfs.subvolume._delete( ( path / v ).c_str() ) ) {
                    throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
                }
            }

#ifdef HISTORIC_STATE
            for ( const string& v : archiveVolumes ) {
                // ignore as it might indicate that archive volumes weren't snapshotted
                if ( !fs::exists( path / v ) )
                    continue;
                if ( btrfs.subvolume._delete( ( path / v ).c_str() ) ) {
                    throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
                }
            }
#endif

            fs::remove_all( path );
        }  // if
    }      // for
}

std::pair< int, int > SnapshotManager::getLatestSnapshots() const {
    map< int, fs::path, std::greater< int > > numbers;
    for ( auto& f : fs::directory_iterator( snapshotsDir ) ) {
        // HACK We exclude 0 snapshot forcefully
        if ( fs::basename( f ) != "0" )
            numbers.insert( make_pair( std::stoi( fs::basename( f ) ), f ) );
    }  // for

    if ( numbers.empty() ) {
        return std::make_pair( 0, 0 );
    }

    auto it = numbers.begin();
    int snd = std::stoi( fs::basename( ( *it++ ).second ) );

    int fst;
    if ( numbers.size() == 1 ) {
        fst = 0;
    } else {
        fst = std::stoi( fs::basename( ( *it ).second ) );
    }

    return std::make_pair( fst, snd );
}

// exeptions: filesystem
void SnapshotManager::leaveNLastDiffs( unsigned n ) {
    map< int, fs::path, std::greater< int > > numbers;
    for ( auto& f : fs::directory_iterator( diffsDir ) ) {
        try {
            numbers.insert( make_pair( std::stoi( fs::basename( f ) ), f ) );
        } catch ( ... ) { /*ignore non-numbers*/
        }
    }  // for

    // delete all after n first
    unsigned i = 1;
    for ( const auto& p : numbers ) {
        if ( i++ > n ) {
            const fs::path& path = p.second;
            fs::remove( path );
        }  // if
    }      // for
}

dev::h256 SnapshotManager::getSnapshotHash( unsigned block_number ) const {
    fs::path snapshot_dir = snapshotsDir / to_string( block_number );

    try {
        if ( !fs::exists( snapshot_dir ) )
            throw SnapshotAbsent( block_number );
    } catch ( const fs::filesystem_error& ) {
        std::throw_with_nested( CannotRead( snapshot_dir ) );
    }  // catch

    std::string hashFile =
        ( this->snapshotsDir / std::to_string( block_number ) / this->snapshotHashFileName )
            .string();

    if ( !isSnapshotHashPresent( block_number ) ) {
        BOOST_THROW_EXCEPTION( SnapshotManager::CannotRead( hashFile ) );
    }

    dev::h256 hash;

    try {
        std::lock_guard< std::mutex > lock( hashFileMutex );
        std::ifstream in( hashFile );
        in >> hash;
    } catch ( const std::exception& ex ) {
        std::throw_with_nested( SnapshotManager::CannotRead( hashFile ) );
    }
    return hash;
}

bool SnapshotManager::isSnapshotHashPresent( unsigned _blockNumber ) const {
    fs::path snapshot_dir = snapshotsDir / to_string( _blockNumber );

    try {
        if ( !fs::exists( snapshot_dir ) )
            throw SnapshotAbsent( _blockNumber );
    } catch ( const fs::filesystem_error& ) {
        std::throw_with_nested( CannotRead( snapshot_dir ) );
    }  // catch

    boost::filesystem::path hashFile = snapshot_dir / this->snapshotHashFileName;
    try {
        std::lock_guard< std::mutex > lock( hashFileMutex );

        return boost::filesystem::exists( hashFile );
    } catch ( const fs::filesystem_error& ) {
        std::throw_with_nested( CannotRead( hashFile ) );
    }
}

void SnapshotManager::computeDatabaseHash(
    const boost::filesystem::path& _dbDir, secp256k1_sha256_t* ctx ) const try {
    if ( !boost::filesystem::exists( _dbDir ) ) {
        BOOST_THROW_EXCEPTION( InvalidPath( _dbDir ) );
    }

    secp256k1_sha256_t dbCtx;
    secp256k1_sha256_initialize( &dbCtx );

    std::string lastHashedKey = "start";
    bool isContinue = true;

    while ( isContinue ) {
        std::unique_ptr< dev::db::LevelDB > m_db( new dev::db::LevelDB( _dbDir.string(),
            dev::db::LevelDB::defaultSnapshotReadOptions(), dev::db::LevelDB::defaultWriteOptions(),
            dev::db::LevelDB::defaultSnapshotDBOptions() ) );

        isContinue = m_db->hashBasePartially( &dbCtx, lastHashedKey );
    }

    dev::h256 dbHash;
    secp256k1_sha256_finalize( &dbCtx, dbHash.data() );
    cnote << _dbDir << " hash is: " << dbHash << std::endl;

    secp256k1_sha256_write( ctx, dbHash.data(), dbHash.size );
} catch ( const fs::filesystem_error& ex ) {
    std::throw_with_nested( CannotRead( ex.path1() ) );
}

void SnapshotManager::addLastPriceToHash( unsigned _blockNumber, secp256k1_sha256_t* ctx ) const {
    dev::u256 last_price = 0;
    // manually open DB
    boost::filesystem::path prices_path =
        this->snapshotsDir / std::to_string( _blockNumber ) / coreVolumes[2];
    if ( boost::filesystem::exists( prices_path ) ) {
        boost::filesystem::directory_iterator it( prices_path ), end;
        std::string last_price_str;
        std::string last_price_key = "1.0:" + std::to_string( _blockNumber );
        while ( it != end ) {
            std::unique_ptr< dev::db::LevelDB > m_db( new dev::db::LevelDB( it->path().string(),
                dev::db::LevelDB::defaultReadOptions(), dev::db::LevelDB::defaultWriteOptions(),
                dev::db::LevelDB::defaultSnapshotDBOptions() ) );
            if ( m_db->exists( last_price_key ) ) {
                last_price_str = m_db->lookup( last_price_key );
                break;
            }
            ++it;
        }

        if ( last_price_str.empty() ) {
            throw std::invalid_argument(
                "No such key in database: " + last_price_key + " : " + prices_path.string() );
        }

        last_price = dev::u256( last_price_str );
    } else {
        throw std::invalid_argument( "No such file or directory: " + prices_path.string() );
    }

    dev::h256 last_price_hash = dev::sha256( last_price.str() );
    cnote << "Latest price hash is: " << last_price_hash << std::endl;
    secp256k1_sha256_write( ctx, last_price_hash.data(), last_price_hash.size );
}

void SnapshotManager::proceedRegularFile(
    const boost::filesystem::path& path, secp256k1_sha256_t* ctx, bool is_checking ) const {
    if ( boost::filesystem::extension( path ) == "._hash" ) {
        return;
    }

    std::string relativePath = path.string().substr( path.string().find( "filestorage" ) );

    std::string fileHashPathStr = path.string() + "._hash";
    if ( !is_checking ) {
        dev::h256 fileHash;
        if ( !boost::filesystem::exists( fileHashPathStr ) ) {
            // file has not been downloaded fully
            // calculate hash, add to global hash, do not create ._hash file
            secp256k1_sha256_t fileData;
            secp256k1_sha256_initialize( &fileData );

            dev::h256 filePathHash = dev::sha256( relativePath );
            secp256k1_sha256_write( &fileData, filePathHash.data(), filePathHash.size );

            std::ifstream originFile( path.string() );
            originFile.seekg( 0, std::ios::end );
            size_t fileContentSize = originFile.tellg();
            std::string fileContent( fileContentSize, ' ' );
            originFile.seekg( 0 );
            originFile.read( &fileContent[0], fileContentSize );

            dev::h256 fileContentHash = dev::sha256( fileContent );

            secp256k1_sha256_write( &fileData, fileContentHash.data(), fileContentHash.size );

            secp256k1_sha256_finalize( &fileData, fileHash.data() );
        } else {
            std::ifstream hash_file( fileHashPathStr );
            hash_file >> fileHash;
        }

        secp256k1_sha256_write( ctx, fileHash.data(), fileHash.size );
    } else {
        secp256k1_sha256_t fileData;
        secp256k1_sha256_initialize( &fileData );

        dev::h256 filePathHash = dev::sha256( relativePath );
        secp256k1_sha256_write( &fileData, filePathHash.data(), filePathHash.size );

        std::ifstream originFile( path.string() );
        originFile.seekg( 0, std::ios::end );
        size_t fileContentSize = originFile.tellg();
        std::string fileContent( fileContentSize, ' ' );
        originFile.seekg( 0 );
        originFile.read( &fileContent[0], fileContentSize );

        dev::h256 fileContentHash = dev::sha256( fileContent );
        secp256k1_sha256_write( &fileData, fileContentHash.data(), fileContentHash.size );

        dev::h256 fileHash;
        secp256k1_sha256_finalize( &fileData, fileHash.data() );

        if ( boost::filesystem::exists( fileHashPathStr ) ) {
            // write to ._hash if exists
            // if no ._hash - file has not been fully downloaded
            std::ofstream hash( fileHashPathStr );
            hash.clear();
            hash << fileHash;
        }

        secp256k1_sha256_write( ctx, fileHash.data(), fileHash.size );
    }
}

void SnapshotManager::proceedDirectory(
    const boost::filesystem::path& path, secp256k1_sha256_t* ctx ) const {
    std::string relativePath = path.string().substr( path.string().find( "filestorage" ) );
    dev::h256 directoryHash = dev::sha256( relativePath );
    secp256k1_sha256_write( ctx, directoryHash.data(), directoryHash.size );
}

void SnapshotManager::proceedFileStorageDirectory( const boost::filesystem::path& _fileSystemDir,
    secp256k1_sha256_t* ctx, bool is_checking ) const {
    boost::filesystem::recursive_directory_iterator directory_it( _fileSystemDir ), end;

    std::vector< boost::filesystem::path > contents;
    while ( directory_it != end ) {
        contents.push_back( *directory_it );
        ++directory_it;
    }
    std::sort( contents.begin(), contents.end(),
        []( const boost::filesystem::path& lhs, const boost::filesystem::path& rhs ) {
            return lhs.string() < rhs.string();
        } );

    for ( auto& content : contents ) {
        if ( boost::filesystem::is_regular_file( content ) ) {
            proceedRegularFile( content, ctx, is_checking );
        } else {
            proceedDirectory( content, ctx );
        }
    }
}

void SnapshotManager::computeFileStorageHash( const boost::filesystem::path& _fileSystemDir,
    secp256k1_sha256_t* ctx, bool is_checking ) const {
    if ( !boost::filesystem::exists( _fileSystemDir ) ) {
        throw std::logic_error( "filestorage btrfs subvolume was corrupted - " +
                                _fileSystemDir.string() + " doesn't exist" );
    }

    this->proceedFileStorageDirectory( _fileSystemDir, ctx, is_checking );
}

void SnapshotManager::computeAllVolumesHash(
    unsigned _blockNumber, secp256k1_sha256_t* ctx, bool is_checking ) const {
    assert( allVolumes.size() != 0 );

    // TODO XXX Remove volumes structure knowledge from here!!

    this->computeDatabaseHash(
        this->snapshotsDir / std::to_string( _blockNumber ) / coreVolumes[0] / "12041" / "state",
        ctx );

    boost::filesystem::path blocks_extras_path =
        this->snapshotsDir / std::to_string( _blockNumber ) / coreVolumes[0] / "blocks_and_extras";

    // few dbs
    boost::filesystem::directory_iterator directory_it( blocks_extras_path ), end;

    std::vector< boost::filesystem::path > contents;

    while ( directory_it != end ) {
        contents.push_back( *directory_it );
        ++directory_it;
    }
    std::sort( contents.begin(), contents.end(),
        []( const boost::filesystem::path& lhs, const boost::filesystem::path& rhs ) {
            return lhs.string() < rhs.string();
        } );

    // 5 is DB pieces count
    size_t cnt = 0;
    for ( auto& content : contents ) {
        if ( cnt++ >= 5 )
            break;
        this->computeDatabaseHash( content, ctx );
    }

    // filestorage
    this->computeFileStorageHash(
        this->snapshotsDir / std::to_string( _blockNumber ) / "filestorage", ctx, is_checking );

    // if have prices and blocks
    if ( _blockNumber && allVolumes.size() > 3 ) {
        this->addLastPriceToHash( _blockNumber, ctx );
    }

    // disable this code until further notice
    // we haven't implemented hash computation for archive submodules yet
    //    if ( chainParams.nodeInfo.archiveMode ) {
    //        // save partial snapshot hash
    //        secp256k1_sha256_t partialCtx = *ctx;

    //        dev::h256 partialHash;
    //        secp256k1_sha256_finalize( &partialCtx, partialHash.data() );

    //        string hashFile = ( this->snapshotsDir / std::to_string( _blockNumber ) ).string() +
    //        '/' +
    //                          this->partialSnapshotHashFileName;

    //        try {
    //            std::lock_guard< std::mutex > lock( hashFileMutex );
    //            std::ofstream out( hashFile );
    //            out.clear();
    //            out << partialHash;
    //        } catch ( const std::exception& ex ) {
    //            std::throw_with_nested( SnapshotManager::CannotCreate( hashFile ) );
    //        }


    //        if ( _blockNumber > 0 ) {
    //            // archive blocks
    //            for ( auto& content : contents ) {
    //                if ( content.leaf().string().find( "archive" ) == std::string::npos )
    //                    continue;
    //                this->computeDatabaseHash( content, ctx );
    //            }

    //#ifdef HISTORIC_STATE
    //            // historic dbs
    //            this->computeDatabaseHash(
    //                this->snapshotsDir / std::to_string( _blockNumber ) / archiveVolumes[0] /
    //                    dev::eth::BlockChain::getChainDirName( chainParams ) / "state",
    //                ctx );
    //            this->computeDatabaseHash(
    //                this->snapshotsDir / std::to_string( _blockNumber ) / archiveVolumes[1] /
    //                    dev::eth::BlockChain::getChainDirName( chainParams ) / "state",
    //                ctx );
    //#endif
    //        }
    //    }
}

void SnapshotManager::computeSnapshotHash( unsigned _blockNumber, bool is_checking ) {
    if ( this->isSnapshotHashPresent( _blockNumber ) ) {
        return;
    }

    secp256k1_sha256_t ctx;
    secp256k1_sha256_initialize( &ctx );

    // TODO Think if we really need it
    // UnsafeRegion::lock ur_lock;

    int dummy_counter = 0;

    std::vector< std::string > volumes = coreVolumes;
#ifdef HISTORIC_STATE
    if ( _blockNumber > 0 )
        volumes = allVolumes;
#endif

    for ( const auto& volume : volumes ) {
        int res = btrfs.subvolume.property_set(
            ( this->snapshotsDir / std::to_string( _blockNumber ) / volume ).string().c_str(), "ro",
            "false" );

        if ( res != 0 ) {
            throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
        }

        if ( dummy_counter++ == 1 )
            batched_io::test_crash_before_commit( "SnapshotManager::doSnapshot" );
    }

    this->computeAllVolumesHash( _blockNumber, &ctx, is_checking );

    for ( const auto& volume : volumes ) {
        int res = btrfs.subvolume.property_set(
            ( this->snapshotsDir / std::to_string( _blockNumber ) / volume ).string().c_str(), "ro",
            "true" );

        if ( res != 0 ) {
            throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
        }
    }

    dev::h256 hash;
    secp256k1_sha256_finalize( &ctx, hash.data() );

    string hash_file = ( this->snapshotsDir / std::to_string( _blockNumber ) ).string() + '/' +
                       this->snapshotHashFileName;

    try {
        std::lock_guard< std::mutex > lock( hashFileMutex );
        std::ofstream out( hash_file );
        out.clear();
        out << hash;
    } catch ( const std::exception& ex ) {
        std::throw_with_nested( SnapshotManager::CannotCreate( hash_file ) );
    }
}

uint64_t SnapshotManager::getBlockTimestamp( unsigned _blockNumber ) const {
    fs::path snapshot_dir = snapshotsDir / to_string( _blockNumber );

    try {
        if ( !fs::exists( snapshot_dir ) )
            throw SnapshotAbsent( _blockNumber );
    } catch ( const fs::filesystem_error& ) {
        std::throw_with_nested( CannotRead( snapshot_dir ) );
    }

    fs::path db_dir = this->snapshotsDir / std::to_string( _blockNumber );

    int res = btrfs.subvolume.property_set(
        ( db_dir / coreVolumes.at( 0 ) ).string().c_str(), "ro", "false" );

    if ( res != 0 ) {
        throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
    }

    dev::eth::BlockChain bc( chainParams, db_dir, false );
    dev::h256 hash = bc.numberHash( _blockNumber );
    uint64_t timestamp = dev::eth::BlockHeader( bc.block( hash ) ).timestamp();

    res = btrfs.subvolume.property_set(
        ( db_dir / coreVolumes.at( 0 ) ).string().c_str(), "ro", "true" );

    if ( res != 0 ) {
        throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
    }

    return timestamp;
}


/*
      Find the most recent database out of the four rotated block atabases in consensus
      This will find the directory in the form "${_dirname}/.db.X" with the largest X
*/
boost::filesystem::path SnapshotManager::findMostRecentBlocksDBPath(
    const boost::filesystem::path& _dirPath ) {
    vector< boost::filesystem::path > dirs;
    vector< uint64_t > indices;

    // First check that _dirname exists and is a directory

    if ( !exists( _dirPath ) ) {
        throw CouldNotFindBlocksDB( _dirPath.string(), "The provided path does not exist." );
    }

    if ( !is_directory( _dirPath ) ) {
        throw CouldNotFindBlocksDB( _dirPath.string(), "The provided path is not a directory." );
    }

    // Find and sort all directories and files in _dirName
    copy( boost::filesystem::directory_iterator( _dirPath ),
        boost::filesystem::directory_iterator(), back_inserter( dirs ) );
    sort( dirs.begin(), dirs.end() );

    size_t offset = string( "db." ).size();

    // First, find all databases in the correct format and collect indices
    for ( auto& path : dirs ) {
        if ( is_directory( path ) ) {
            auto fileName = path.filename().string();
            if ( fileName.find( "db." ) == 0 ) {
                auto index = fileName.substr( offset );
                auto value = strtoull( index.c_str(), nullptr, 10 );
                if ( value != 0 ) {
                    indices.push_back( value );
                }
            }
        }
    }

    // Could not find any database in the correct format. Throw exception
    if ( indices.size() == 0 ) {
        throw CouldNotFindBlocksDB(
            _dirPath.string(), "No rotated databases in correct format found" );
    }

    // Now find the maximum index X. This is the most recent database
    auto maxIndex = *max_element( begin( indices ), end( indices ) );

    return _dirPath / ( "db." + to_string( maxIndex ) );
}
