/*
    Modifications Copyright (C) 2024- SKALE Labs

    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "db.h"

#include <rocksdb/db.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/write_batch.h>
#include <boost/filesystem.hpp>

#include <secp256k1_sha256.h>
#include <shared_mutex>

namespace dev::db {

class RocksDB;

// internal class of LevelDB that represents the
// this class represents a LevelDB snap corresponding to the point immediately
// after processing of a particular block id.
class RocksDBSnap {
public:

    RocksDBSnap( uint64_t _blockId, const rocksdb::Snapshot* _snap, uint64_t _parentRocksDBDBReopenId);

    // close this snapshot. Use after close will cause an exception
    void close( std::unique_ptr< rocksdb::DB >& _parentDB, uint64_t _parentDBReopenId );

    virtual ~RocksDBSnap();

    rocksdb::Status getValue( const std::unique_ptr< rocksdb::DB >& _db, rocksdb::ReadOptions _readOptions,
        const rocksdb::Slice& _key, std::string& _value );

    rocksdb::Status getValue( const std::unique_ptr< rocksdb::DB >& _db, rocksdb::ReadOptions _readOptions,
        std::string& _value );

    std::unique_ptr<rocksdb::Iterator> getIterator( const std::unique_ptr< rocksdb::DB >& _db,
        rocksdb::ReadOptions _readOptions);

    uint64_t getInstanceId() const;

    uint64_t getParentDbReopenId() const;

    uint64_t getCreationTimeMs() const;

    bool isClosed() const;

private:

    const uint64_t m_blockId;

    const rocksdb::Snapshot* m_snap = nullptr;

    std::atomic< bool > m_isClosed = false;
    // this mutex is shared=locked everytime a LevedLB API read call is done on the
    // snapshot, and unique-locked when the snapshot is to be closed
    // This is to prevent closing snapshot handle while concurrently executing a LevelDB call
    std::shared_mutex m_usageMutex;

    uint64_t m_creationTimeMs;

    // LevelDB identifier for which this snapShot has been // created
    // this is used to match snaps to leveldb handles
    // the reopen id of the parent database handle. When a database is reopened
    // the database handle and all snap handles are invalidated
    // we use this field to make sure that we never use a stale snap handle for which
    // the database handle already does not exist
    uint64_t m_parentDBReopenId;

    std::atomic<uint64_t> m_instanceId; // unique id of this snap object instance

    static std::atomic< uint64_t > objectCounter;
};

}  // namespace dev::db
