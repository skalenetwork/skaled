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


#include "RocksDBSnapManager.h"
#include "Assertions.h"
#include "RocksDB.h"
#include "RocksDBSnap.h"


using std::string, std::runtime_error;

namespace dev::db {

// this function will close snaps that are not used in on-going eth_calls,
// meaning that no one has a reference
// to the shared pointer except the oldSnaps map itself
// It will also close snaps that are used but that are older than _maxSnapLifetimeMs
// this closure will cause the corresponding eth_calls return with an error.
// this should only happen to eth_calls that hang for a long time
// this function returns the size of oldSnaps after cleanup
uint64_t RocksDBSnapManager::garbageCollectUnusedOldSnaps(
    std::unique_ptr< rocksdb::DB >& _db, uint64_t _dbReopenId, uint64_t _maxSnapLifetimeMs ) {


    std::vector< std::shared_ptr< RocksDBSnap > > unusedOldSnaps;

    uint64_t mapSizeAfterCleanup;

    auto currentTimeMs = RocksDB::getCurrentTimeMs();

    {
        // find all old snaps and remove them from the map
        // this needs to be done write lock so the map is not concurrently modified
        std::unique_lock< std::shared_mutex > snapLock( m_snapMutex );
        for ( auto it = oldSnaps.begin(); it != oldSnaps.end(); ) {
            // a snap is unused if no one using this snap anymore except the map itself
            if ( it->second.use_count() == 1 ||
                 it->second->getCreationTimeMs() + _maxSnapLifetimeMs <= currentTimeMs ) {
                // erase this snap from the map
                // note that push back needs to happen before erase since erase will
                // destroy the snap object if there ar no more references
                unusedOldSnaps.push_back( it->second );
                it = oldSnaps.erase( it );
            } else {
                ++it;  // Only increment if not erasing
            }
        }
        mapSizeAfterCleanup = oldSnaps.size();
    }

    // now we removed unused snaps from the map. Close them

    for (auto&& snap: unusedOldSnaps) {
        RDB_CHECK(snap);
        snap->close(_db, _dbReopenId);
    }
    return mapSizeAfterCleanup;
}


// this will be called from EVM processing thread just after the block is processed
void RocksDBSnapManager::addSnapForBlock(
    uint64_t _blockNumber, std::unique_ptr< rocksdb::DB >& _db, uint64_t _dbInstanceId ) {

    createNewSnap( _blockNumber, _db, _dbInstanceId );

    // we garbage-collect  unused old snaps that no-one used or that exist for more that max
    // lifetime we give for eth_calls to complete
    garbageCollectUnusedOldSnaps( _db, _dbInstanceId,
        OLD_SNAP_LIFETIME_MS );
}


// this will create new last block snap and move previous last block snap into old snaps map
void RocksDBSnapManager::createNewSnap(
    uint64_t _blockId, std::unique_ptr< rocksdb::DB >& _db, uint64_t _dbInstanceId ) {
        // hold the write lock during the update so snap manager does not return inconsistent value
        // snap creation in LevelDB should happen really fast
        std::unique_lock< std::shared_mutex > lock( m_snapMutex );

        RDB_CHECK( _db );
        auto newSnapHandle = _db->GetSnapshot();
        RDB_CHECK( newSnapHandle );

        auto newSnap = std::make_shared< RocksDBSnap >( _blockId, newSnapHandle, _dbInstanceId );
        RDB_CHECK( newSnap );

        auto oldSnap = m_lastBlockSnap;
        m_lastBlockSnap = newSnap;
        if (oldSnap) {
            oldSnaps.emplace( oldSnap->getInstanceId(), oldSnap );
        }
}
const std::shared_ptr< RocksDBSnap >& RocksDBSnapManager::getLastBlockSnap() const {
    // read lock briefly to make no snap is concurrently created.
    // Note: shared pointer is not thread safe
    std::shared_lock< std::shared_mutex > lock( m_snapMutex );
    return m_lastBlockSnap;
}


}  // namespace dev::db
