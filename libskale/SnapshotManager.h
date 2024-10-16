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
 * @file SnapshotManager.h
 * @author Dima Litvinov
 * @date 2019
 */

#ifndef SNAPSHOTMANAGER_H
#define SNAPSHOTMANAGER_H

#include <libdevcore/FixedHash.h>
#include <libethereum/BlockChain.h>
#include <secp256k1_sha256.h>

#include <boost/filesystem.hpp>

#include <mutex>
#include <string>
#include <vector>

class SnapshotManager {
    //////////////////////// EXCEPTIONS /////////////////////

private:
    class FsException : public std::exception {
    protected:
        std::string what_str;

    public:
        boost::filesystem::path path;

        FsException( const boost::filesystem::path& _path ) : path( _path ) {
            what_str = "Cannot process path " + path.string();
        }
        virtual const char* what() const noexcept override { return what_str.c_str(); }
    };

public:
    class InvalidPath : public FsException {
    public:
        InvalidPath( const boost::filesystem::path& _path ) : FsException( _path ) {
            what_str = "Invalid path " + path.string();
        }
    };
    class CannotRead : public FsException {
    public:
        CannotRead( const boost::filesystem::path& _path ) : FsException( _path ) {
            what_str = "Cannot read " + path.string();
        }
    };
    class CannotWrite : public FsException {
    public:
        CannotWrite( const boost::filesystem::path& _path ) : FsException( _path ) {
            what_str = "Cannot write " + path.string();
        }
    };
    class CannotCreate : public CannotWrite {
    public:
        CannotCreate( const boost::filesystem::path& _path ) : CannotWrite( _path ) {
            what_str = "Cannot create " + path.string();
        }
    };
    class CannotDelete : public CannotWrite {
    public:
        CannotDelete( const boost::filesystem::path& _path ) : CannotWrite( _path ) {
            what_str = "Cannote delete " + path.string();
        }
    };
    class CannotCreateTmpFile : public FsException {
    public:
        CannotCreateTmpFile( const boost::filesystem::path& _path ) : FsException( _path ) {
            what_str = "Cannot create tmp file " + path.string();
        }
    };


    class CannotPerformBtrfsOperation : public std::exception {
    protected:
        std::string what_str;

    public:
        std::string cmd;
        std::string strerror;

    public:
        CannotPerformBtrfsOperation( const char* _cmd, const char* _msg ) {
            this->cmd = _cmd;
            this->strerror = _msg;
            what_str = "BTRFS operation: " + this->cmd + " - failed: " + this->strerror;
        }
        virtual const char* what() const noexcept override { return what_str.c_str(); }
    };

    class SnapshotAbsent : public std::exception {
    protected:
        std::string what_str;

    public:
        unsigned block_number;

        SnapshotAbsent( unsigned _blockNumber ) {
            block_number = _blockNumber;
            what_str = "Snapshot for block " + std::to_string( block_number ) + " is absent";
        }
        virtual const char* what() const noexcept override { return what_str.c_str(); }
    };

    class SnapshotPresent : public std::exception {
    protected:
        std::string what_str;

    public:
        unsigned block_number;

        SnapshotPresent( unsigned _blockNumber ) {
            block_number = _blockNumber;
            what_str =
                "Snapshot for block " + std::to_string( block_number ) + " is already present";
        }
        virtual const char* what() const noexcept override { return what_str.c_str(); }
    };


    class CouldNotFindBlocksDB : public std::exception {
    protected:
        std::string m_whatStr;

    public:
        CouldNotFindBlocksDB( const std::string& _path, const std::string& _message ) {
            m_whatStr = "Could not find BlocksDB at " + _path + "." + _message;
        }
        virtual const char* what() const noexcept override { return m_whatStr.c_str(); }
    };

    /////////////// MORE INTERESTING STUFF ////////////////

public:
    SnapshotManager( const dev::eth::ChainParams& _chainParams,
        const boost::filesystem::path& _dataDir, const std::string& diffs_dir = std::string() );
    void doSnapshot( unsigned _blockNumber );
    void restoreSnapshot( unsigned _blockNumber );
    boost::filesystem::path makeOrGetDiff( unsigned _toBlock );
    void importDiff( unsigned _toBlock );
    boost::filesystem::path getDiffPath( unsigned _toBlock );
    void removeSnapshot( unsigned _blockNumber );
    void cleanup();
    void cleanupButKeepSnapshot( unsigned _keepSnapshot );

    void leaveNLastSnapshots( unsigned n );
    void leaveNLastDiffs( unsigned n );

    dev::h256 getSnapshotHash( unsigned _blockNumber ) const;
    std::pair< int, int > getLatestSnapshots() const;
    bool isSnapshotHashPresent( unsigned _blockNumber ) const;
    void computeSnapshotHash( unsigned _blockNumber, bool is_checking = false );

    uint64_t getBlockTimestamp( unsigned _blockNumber ) const;

    static boost::filesystem::path findMostRecentBlocksDBPath(
        const boost::filesystem::path& _dirPath );

private:
    boost::filesystem::path dataDir;
    std::vector< std::string > coreVolumes;
    std::vector< std::string > archiveVolumes;
    std::vector< std::string > allVolumes;
    boost::filesystem::path snapshotsDir;
    boost::filesystem::path diffsDir;

    static const std::string snapshotHashFileName;
    mutable std::mutex hashFileMutex;

    dev::eth::ChainParams chainParams;

    void cleanupDirectory(
        const boost::filesystem::path& p, const boost::filesystem::path& _keepDirectory = "" );

    void computeFileStorageHash( const boost::filesystem::path& _fileSystemDir,
        secp256k1_sha256_t* ctx, bool is_checking ) const;
    void proceedFileStorageDirectory( const boost::filesystem::path& _fileSystemDir,
        secp256k1_sha256_t* ctx, bool is_checking ) const;
    void proceedRegularFile(
        const boost::filesystem::path& path, secp256k1_sha256_t* ctx, bool is_checking ) const;
    void proceedDirectory( const boost::filesystem::path& path, secp256k1_sha256_t* ctx ) const;
    void computeAllVolumesHash(
        unsigned _blockNumber, secp256k1_sha256_t* ctx, bool is_checking ) const;
    void computeDatabaseHash(
        const boost::filesystem::path& _dbDir, secp256k1_sha256_t* ctx ) const;
    void addLastPriceToHash( unsigned _blockNumber, secp256k1_sha256_t* ctx ) const;
};

#endif  // SNAPSHOTMANAGER_H
