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
class LevelDBSnap;

// internal class of LevelDB that represents the
// this class represents a LevelDB snap corresponding to the point immediately
// after processing of a particular block id.
class LevelDBSnapManager {

public:
    const std::shared_ptr< LevelDBSnap >& getLastBlockSnap() const;


    LevelDBSnapManager() {};

    void addSnapForBlock(
        uint64_t _blockNumber, std::unique_ptr< leveldb::DB >& _db, uint64_t _dbInstanceId );

    void closeAllOpenSnaps(std::unique_ptr< leveldb::DB >& _db, uint64_t  _dbInstanceId);

    // this function should be called while holding database reopen lock
    uint64_t garbageCollectUnusedOldSnaps(
        std::unique_ptr< leveldb::DB >& _db, uint64_t _dbReopenId, uint64_t _maxSnapLifetimeMs );

private:

    // old snaps contains snap objects for older blocks
    // these objects are alive untils the
    // corresponding eth_calls complete
    std::map<std::uint64_t , std::shared_ptr<LevelDBSnap>> oldSnaps;
    std::shared_ptr<LevelDBSnap> m_lastBlockSnap;
    // mutex to protect snaps and m_lastBlockSnap;
    mutable std::shared_mutex m_snapMutex;

    // time after an existing old snap will be closed if no-one is using it
    static const size_t OLD_SNAP_LIFETIME_MS = 30000;
    // time after an existing old snap will be closed it is used in eth_call
    // this will cause the eth_call to return an error
    static const size_t FORCE_SNAP_CLOSE_TIME_MS = 3000;

    void createNewSnap( uint64_t _blockId, std::unique_ptr< leveldb::DB >& _db, uint64_t _dbInstanceId );
};

}  // namespace dev::db
