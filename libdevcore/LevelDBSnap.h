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

#include <leveldb/db.h>
#include <leveldb/filter_policy.h>
#include <leveldb/write_batch.h>
#include <boost/filesystem.hpp>

#include <secp256k1_sha256.h>
#include <shared_mutex>

namespace dev::db {

class LevelDB;

// internal class of LevelDB that represents the
// this class represents a LevelDB snap corresponding to the point immediately
// after processing of a particular block id.
class LevelDBSnap {
public:
    LevelDBSnap( uint64_t _blockId, const leveldb::Snapshot* _snap,
        uint64_t _parentLevelDBId );
    uint64_t getParentLevelDBId() const;

    // close this snapshot. Use after close will cause an exception
    void close(std::unique_ptr< leveldb::DB >& _parentDB, uint64_t  _parentDBIdentifier);


    std::unique_ptr<std::shared_lock<std::shared_mutex>> lockToPreventConcurrentClose();

    uint64_t getCreationTimeMs() const;

    virtual ~LevelDBSnap();

    uint64_t getObjectId() const;

    const leveldb::Snapshot* getSnapHandle() const;
    bool isClosed() const;

private:

    std::atomic<bool> m_isClosed = false;
    // this mutex is shared=locked everytime a LevedLB API call is done on the
    // snapshot, and unique-locked when the snapshot is to be closed
    // This is to prevent closing snapshot handle while concurrently executing a LevelDB call
    std::shared_mutex m_usageMutex;

    const uint64_t m_blockId;
    const leveldb::Snapshot* m_snap = nullptr;
    uint64_t m_creationTimeMs;
    // LevelDB identifier for which this snapShot has been // created
    // this is used to match snaps to leveldb handles
    uint64_t m_parentLevelDBId;
    uint64_t m_objectId;
    static std::atomic<uint64_t> objectCounter;


};

}  // namespace dev::db
