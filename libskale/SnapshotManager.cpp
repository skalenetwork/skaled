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

#include "SnapshotManager.h"

#include <libdevcore/LevelDB.h>
#include <libdevcore/Log.h>
#include <libdevcrypto/Hash.h>
#include <skutils/btrfs.h>

#include <boost/interprocess/sync/named_mutex.hpp>

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

using namespace std;
namespace fs = boost::filesystem;

// Can manage snapshots as non-prvivileged user
// For send/receive needs root!

const std::string SnapshotManager::snapshot_hash_file_name = "snapshot_hash.txt";

// exceptions:
// - bad data dir
// - not btrfs
// - volumes don't exist
SnapshotManager::SnapshotManager( const fs::path& _dataDir,
    const std::vector< std::string >& _volumes, const std::string& _diffsDir ) {
    assert( _volumes.size() > 0 );

    data_dir = _dataDir;
    volumes = _volumes;
    snapshots_dir = data_dir / "snapshots";
    if ( _diffsDir.empty() )
        diffs_dir = data_dir / "diffs";
    else
        diffs_dir = _diffsDir;

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
        if ( fs::exists( data_dir / vol ) ) {
            if ( btrfs.subvolume._delete( ( data_dir / vol ).c_str() ) )
                throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
        }
        if ( btrfs.subvolume.snapshot(
                 ( snapshots_dir / to_string( _blockNumber ) / vol ).c_str(), data_dir.c_str() ) )
            throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
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

        if ( !fs::exists( snapshots_dir / to_string( _toBlock ) ) ) {
            // TODO wrong error message if this fails
            fs::remove( path );
            throw SnapshotAbsent( _toBlock );
        }
    } catch ( const fs::filesystem_error& ex ) {
        std::throw_with_nested( CannotRead( ex.path1() ) );
    }

    stringstream volumes_cat;

    for ( auto it = volumes.begin(); it != volumes.end(); ++it ) {
        const string& vol = *it;
        if ( it + 1 != volumes.end() )
            volumes_cat << ( snapshots_dir / to_string( _toBlock ) / vol ).string() << " ";
        else
            volumes_cat << ( snapshots_dir / to_string( _toBlock ) / vol ).string();
    }  // for cat

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
        auto ex = CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
        cleanupDirectory( snapshot_dir );
        fs::remove_all( snapshot_dir );
        throw ex;
    }  // if
}

boost::filesystem::path SnapshotManager::getDiffPath( unsigned _toBlock ) {
    // check existance
    assert( boost::filesystem::exists( diffs_dir ) );
    return diffs_dir / ( std::to_string( _toBlock ) );
}

void SnapshotManager::removeSnapshot( unsigned _blockNumber ) {
    if ( !fs::exists( snapshots_dir / to_string( _blockNumber ) ) ) {
        throw SnapshotAbsent( _blockNumber );
    }

    for ( const auto& volume : this->volumes ) {
        int res = btrfs.subvolume._delete(
            ( this->snapshots_dir / std::to_string( _blockNumber ) / volume ).string().c_str() );

        if ( res != 0 ) {
            throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
        }
    }

    fs::remove_all( snapshots_dir / to_string( _blockNumber ) );
}

void SnapshotManager::cleanupButKeepSnapshot( unsigned _keepSnapshot ) {
    this->cleanupDirectory( snapshots_dir, snapshots_dir / std::to_string( _keepSnapshot ) );
    this->cleanupDirectory( data_dir, snapshots_dir );
}

void SnapshotManager::cleanup() {
    this->cleanupDirectory( snapshots_dir );
    this->cleanupDirectory( data_dir );

    try {
        boost::filesystem::create_directory( snapshots_dir );
        boost::filesystem::create_directory( diffs_dir );
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
    for ( auto& f : fs::directory_iterator( snapshots_dir ) ) {
        // HACK We exclude 0 snapshot forcefully
        if ( fs::basename( f ) != "0" )
            numbers.insert( make_pair( std::stoi( fs::basename( f ) ), f ) );
    }  // for

    // delete all after n first
    unsigned i = 1;
    for ( const auto& p : numbers ) {
        if ( i++ > n ) {
            const fs::path& path = p.second;
            for ( const string& v : this->volumes ) {
                if ( btrfs.subvolume._delete( ( path / v ).c_str() ) ) {
                    throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
                }
            }
            fs::remove_all( path );
        }  // if
    }      // for
}

std::pair< int, int > SnapshotManager::getLatestSnasphots() const {
    map< int, fs::path, std::greater< int > > numbers;
    for ( auto& f : fs::directory_iterator( snapshots_dir ) ) {
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
    for ( auto& f : fs::directory_iterator( diffs_dir ) ) {
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
    fs::path snapshot_dir = snapshots_dir / to_string( block_number );

    try {
        if ( !fs::exists( snapshot_dir ) )
            throw SnapshotAbsent( block_number );
    } catch ( const fs::filesystem_error& ) {
        std::throw_with_nested( CannotRead( snapshot_dir ) );
    }  // catch

    std::string hash_file =
        ( this->snapshots_dir / std::to_string( block_number ) / this->snapshot_hash_file_name )
            .string();

    if ( !isSnapshotHashPresent( block_number ) ) {
        BOOST_THROW_EXCEPTION( SnapshotManager::CannotRead( hash_file ) );
    }

    dev::h256 hash;

    try {
        std::lock_guard< std::mutex > lock( hash_file_mutex );
        std::ifstream in( hash_file );
        in >> hash;
    } catch ( const std::exception& ex ) {
        std::throw_with_nested( SnapshotManager::CannotRead( hash_file ) );
    }
    return hash;
}

bool SnapshotManager::isSnapshotHashPresent( unsigned _blockNumber ) const {
    fs::path snapshot_dir = snapshots_dir / to_string( _blockNumber );

    try {
        if ( !fs::exists( snapshot_dir ) )
            throw SnapshotAbsent( _blockNumber );
    } catch ( const fs::filesystem_error& ) {
        std::throw_with_nested( CannotRead( snapshot_dir ) );
    }  // catch

    boost::filesystem::path hash_file =
        this->snapshots_dir / std::to_string( _blockNumber ) / this->snapshot_hash_file_name;
    try {
        std::lock_guard< std::mutex > lock( hash_file_mutex );
        return boost::filesystem::exists( hash_file );
    } catch ( const fs::filesystem_error& ) {
        std::throw_with_nested( CannotRead( hash_file ) );
    }
}

void SnapshotManager::computeDatabaseHash(
    const boost::filesystem::path& _dbDir, secp256k1_sha256_t* ctx ) const try {
    if ( !boost::filesystem::exists( _dbDir ) ) {
        BOOST_THROW_EXCEPTION( InvalidPath( _dbDir ) );
    }

    std::unique_ptr< dev::db::LevelDB > m_db( new dev::db::LevelDB( _dbDir.string() ) );
    dev::h256 hash_volume = m_db->hashBase();
    cnote << _dbDir << " hash is: " << hash_volume << std::endl;

    secp256k1_sha256_write( ctx, hash_volume.data(), hash_volume.size );
} catch ( const fs::filesystem_error& ex ) {
    std::throw_with_nested( CannotRead( ex.path1() ) );
}

void SnapshotManager::addLastPriceToHash( unsigned _blockNumber, secp256k1_sha256_t* ctx ) const {
    dev::u256 last_price = 0;
    // manually open DB
    boost::filesystem::path prices_path =
        this->snapshots_dir / std::to_string( _blockNumber ) / this->volumes[2];
    if ( boost::filesystem::exists( prices_path ) ) {
        boost::filesystem::directory_iterator it( prices_path ), end;
        std::string last_price_str;
        std::string last_price_key = "1.0:" + std::to_string( _blockNumber );
        while ( it != end ) {
            std::unique_ptr< dev::db::LevelDB > m_db( new dev::db::LevelDB( it->path().string() ) );
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

void SnapshotManager::proceedFileSystemDirectory( const boost::filesystem::path& _fileSystemDir,
    secp256k1_sha256_t* ctx, bool is_checking ) const {
    boost::filesystem::recursive_directory_iterator it( _fileSystemDir ), end;

    while ( it != end ) {
        std::string fileHashPathStr = it->path().string() + "._hash";
        if ( boost::filesystem::is_regular_file( *it ) ) {
            if ( boost::filesystem::extension( it->path() ) != "._hash" ) {
                if ( !is_checking ) {
                    if ( !boost::filesystem::exists( fileHashPathStr ) ) {
                        // file has not been downloaded fully
                        secp256k1_sha256_t fileData;
                        secp256k1_sha256_initialize( &fileData );

                        dev::h256 filePathHash = dev::sha256( it->path().string() );
                        secp256k1_sha256_write( &fileData, filePathHash.data(), filePathHash.size );

                        std::ifstream originFile( it->path().string() );
                        originFile.seekg( 0, std::ios::end );
                        size_t fileContentSize = originFile.tellg();
                        std::string fileContent( fileContentSize, ' ' );
                        originFile.seekg( 0 );
                        originFile.read( &fileContent[0], fileContentSize );

                        dev::h256 fileContentHash = dev::sha256( fileContent );

                        secp256k1_sha256_write(
                            &fileData, fileContentHash.data(), fileContentHash.size );

                        dev::h256 fileHash;
                        secp256k1_sha256_finalize( &fileData, fileHash.data() );

                        std::ofstream hash( fileHashPathStr );
                        hash << fileHash;
                    }

                    std::ifstream hash_file( fileHashPathStr );
                    dev::h256 hash;
                    hash_file >> hash;
                    secp256k1_sha256_write( ctx, hash.data(), hash.size );
                } else {
                    secp256k1_sha256_t fileData;
                    secp256k1_sha256_initialize( &fileData );

                    dev::h256 filePathHash = dev::sha256( it->path().string() );
                    secp256k1_sha256_write( &fileData, filePathHash.data(), filePathHash.size );

                    std::ifstream originFile( it->path().string() );
                    originFile.seekg( 0, std::ios::end );
                    size_t fileContentSize = originFile.tellg();
                    std::string fileContent( fileContentSize, ' ' );
                    originFile.seekg( 0 );
                    originFile.read( &fileContent[0], fileContentSize );

                    dev::h256 fileContentHash = dev::sha256( fileContent );
                    secp256k1_sha256_write(
                        &fileData, fileContentHash.data(), fileContentHash.size );

                    dev::h256 fileHash;
                    secp256k1_sha256_finalize( &fileData, fileHash.data() );

                    std::ofstream hash( fileHashPathStr );
                    hash.clear();
                    hash << fileHash;

                    secp256k1_sha256_write( ctx, fileHash.data(), fileHash.size );
                }
            }
        } else {
            if ( !is_checking ) {
                if ( !boost::filesystem::exists( fileHashPathStr ) ) {
                    // hash file hasn't been computed
                    std::ofstream hash_file( fileHashPathStr );
                    dev::h256 hash = dev::sha256( it->path().string() );
                    hash_file << hash;
                }
                std::ifstream hash_file( fileHashPathStr );
                dev::h256 hash;
                hash_file >> hash;
                secp256k1_sha256_write( ctx, hash.data(), hash.size );
            } else {
                dev::h256 directoryHash = dev::sha256( it->path().string() );
                secp256k1_sha256_write( ctx, directoryHash.data(), directoryHash.size );

                std::ofstream hash( fileHashPathStr );
                hash.clear();
                hash << directoryHash;
            }
        }

        ++it;
    }
}

void SnapshotManager::computeFileSystemHash( const boost::filesystem::path& _fileSystemDir,
    secp256k1_sha256_t* ctx, bool is_checking ) const {
    if ( !boost::filesystem::exists( _fileSystemDir ) ) {
        throw std::logic_error( "filestorage btrfs subvolume was corrupted - " +
                                _fileSystemDir.string() + " doesn't exist" );
    }

    this->proceedFileSystemDirectory( _fileSystemDir, ctx, is_checking );
}

void SnapshotManager::computeAllVolumesHash(
    unsigned _blockNumber, secp256k1_sha256_t* ctx, bool is_checking ) const {
    assert( this->volumes.size() != 0 );

    // TODO XXX Remove volumes structure knowledge from here!!

    this->computeDatabaseHash(
        this->snapshots_dir / std::to_string( _blockNumber ) / this->volumes[0] / "12041" / "state",
        ctx );

    boost::filesystem::path blocks_extras_path = this->snapshots_dir /
                                                 std::to_string( _blockNumber ) / this->volumes[0] /
                                                 "blocks_and_extras";

    // few dbs
    boost::filesystem::directory_iterator it( blocks_extras_path ), end;

    while ( it != end ) {
        this->computeDatabaseHash( it->path(), ctx );
        ++it;
    }

    // filestorage
    this->computeFileSystemHash(
        this->snapshots_dir / std::to_string( _blockNumber ) / "filestorage", ctx, is_checking );

    // if have prices and blocks
    if ( this->volumes.size() > 3 ) {
        this->addLastPriceToHash( _blockNumber, ctx );
    }
}

void SnapshotManager::computeSnapshotHash( unsigned _blockNumber, bool is_checking ) {
    if ( this->isSnapshotHashPresent( _blockNumber ) ) {
        return;
    }

    secp256k1_sha256_t ctx;
    secp256k1_sha256_initialize( &ctx );

    for ( const auto& volume : this->volumes ) {
        int res = btrfs.btrfs_subvolume_property_set(
            ( this->snapshots_dir / std::to_string( _blockNumber ) / volume ).string().c_str(),
            "ro", "false" );

        if ( res != 0 ) {
            throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
        }
    }

    this->computeAllVolumesHash( _blockNumber, &ctx, is_checking );

    for ( const auto& volume : this->volumes ) {
        int res = btrfs.btrfs_subvolume_property_set(
            ( this->snapshots_dir / std::to_string( _blockNumber ) / volume ).string().c_str(),
            "ro", "true" );

        if ( res != 0 ) {
            throw CannotPerformBtrfsOperation( btrfs.last_cmd(), btrfs.strerror() );
        }
    }

    dev::h256 hash;
    secp256k1_sha256_finalize( &ctx, hash.data() );

    string hash_file = ( this->snapshots_dir / std::to_string( _blockNumber ) ).string() + '/' +
                       this->snapshot_hash_file_name;

    try {
        std::lock_guard< std::mutex > lock( hash_file_mutex );
        std::ofstream out( hash_file );
        out.clear();
        out << hash;
    } catch ( const std::exception& ex ) {
        std::throw_with_nested( SnapshotManager::CannotCreate( hash_file ) );
    }
}
