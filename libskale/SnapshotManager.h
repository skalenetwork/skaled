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

#ifndef SNAPSHOTAGENT_H
#define SNAPSHOTAGENT_H

#include <libdevcore/FixedHash.h>
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

    /////////////// MORE INTERESTING STUFF ////////////////

public:
    SnapshotManager(
        const boost::filesystem::path& _dataDir, const std::vector< std::string >& _volumes );
    void doSnapshot( unsigned _blockNumber );
    void restoreSnapshot( unsigned _blockNumber );
    boost::filesystem::path makeOrGetDiff( unsigned _fromBlock, unsigned _toBlock );
    void importDiff( unsigned _fromBlock, unsigned _toBlock );
    boost::filesystem::path getDiffPath( unsigned _fromBlock, unsigned _toBlock );
    void removeSnapshot( unsigned _blockNumber );

    void leaveNLastSnapshots( unsigned n );
    void leaveNLastDiffs( unsigned n );

    dev::h256 getSnapshotHash( unsigned _blockNumber ) const;
    bool isSnapshotHashPresent( unsigned _blockNumber ) const;
    void computeSnapshotHash( unsigned _blockNumber );

private:
    boost::filesystem::path data_dir;
    std::vector< std::string > volumes;
    boost::filesystem::path snapshots_dir;
    boost::filesystem::path diffs_dir;

    static const std::string snapshot_hash_file_name;
    mutable std::mutex hash_file_mutex;

    void computeAllVolumesHash( unsigned _blockNumber, secp256k1_sha256_t* ctx ) const;
    void computeVolumeHash(
        const boost::filesystem::path& _volumeDir, secp256k1_sha256_t* ctx ) const;
};

#endif  // SNAPSHOTAGENT_H
