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


#include "LevelDBSnapManager.h"
#include "Assertions.h"
#include "LevelDB.h"
#include "LevelDBSnap.h"


using std::string, std::runtime_error;

namespace dev::db {


void LevelDBSnapManager::closeAllOpenSnaps(
    std::unique_ptr< leveldb::DB >& _db, uint64_t _dbInstanceId ) {
    auto startTimeMs = LevelDB::getCurrentTimeMs();

    if ( m_lastBlockSnap ) {
        // move current last block snap to old snaps so all snaps can be cleaned in a single
        // function
        std::unique_lock< std::shared_mutex > snapLock( m_snapMutex );
        oldSnaps.emplace( m_lastBlockSnap->getInstanceId(), m_lastBlockSnap );
        m_lastBlockSnap = nullptr;
    }

    while ( LevelDB::getCurrentTimeMs() <= startTimeMs + FORCE_SNAP_CLOSE_TIME_MS ) {
        // when LevelDB is reopened we need to close all
        // all snap handles created for  this database
        // a snaps is used if  used its shared pointer reference count is more than 1
        // here we wait for FORCE_TIME_MS, hoping that all eth_calls complete nicely
        // and all snaps becomes unused
        auto aliveSnaps = cleanUnusedOldSnaps( _db, _dbInstanceId, OLD_SNAP_LIFETIME_MS );
        if ( aliveSnaps == 0 ) {
            // no more snaps
            break;
        }
        usleep( 1000 );
    }

    // now if there are still unclosed steps we will close them forcefully
    // this means that the corresponding eth_calls will return and error

    auto aliveSnaps = cleanUnusedOldSnaps( _db, _dbInstanceId, 0 );


    LDB_CHECK( aliveSnaps );
}


// this function will close snaps that are not used in on-going eth_calls,
// meaning that no one has a reference
// to the shared pointer except the oldSnaps map itself
// It will also close snaps that are used but that are older than _maxSnapLifetimeMs
// this closure will cause the corresponding eth_calls return with an error.
// this should only happen to eth_calls that hang for a long time
// this function returns the size of oldSnaps after cleanup
uint64_t LevelDBSnapManager::cleanUnusedOldSnaps(
    std::unique_ptr< leveldb::DB >& _db, uint64_t _dbInstanceId, uint64_t _maxSnapLifetimeMs ) {
    std::unique_lock< std::shared_mutex > snapLock( m_snapMutex );
    // now we iterate over snaps closing the ones that are not more in use
    auto currentTimeMs = LevelDB::getCurrentTimeMs();
    for ( auto it = oldSnaps.begin(); it != oldSnaps.end(); ) {
        if ( it->second.use_count() == 1 ||  // no one using this snap anymore except this map
             it->second->getCreationTimeMs() + _maxSnapLifetimeMs <= currentTimeMs ) {
            // close the snap
            it->second->close( _db, _dbInstanceId );
            it = oldSnaps.erase( it );  // Erase returns the iterator to the next element
        } else {
            ++it;  // Only increment if not erasing
        }
    }
    return oldSnaps.size();
}
void LevelDBSnapManager::addSnapForBlock(
    uint64_t _blockId, std::unique_ptr< leveldb::DB >& _db, uint64_t _dbInstanceId ) {
    LDB_CHECK( _db );
    auto newSnapHandle = _db->GetSnapshot();
    LDB_CHECK( newSnapHandle );

    auto newSnap = std::make_shared< LevelDBSnap >( _blockId, newSnapHandle, _dbInstanceId );
    LDB_CHECK( newSnap );

    {
        std::unique_lock< std::shared_mutex > lock( m_snapMutex );
        auto oldSnap = m_lastBlockSnap;
        m_lastBlockSnap = newSnap;
        oldSnaps.emplace( oldSnap->getInstanceId(), oldSnap );
    }


    // we clean unneeded old snaps that no-one used or that exist for more that max
    // lifetime we give for eth_calls to complete
    cleanUnusedOldSnaps( _db, _dbInstanceId, OLD_SNAP_LIFETIME_MS );
}
const std::shared_ptr< LevelDBSnap >& LevelDBSnapManager::getLastBlockSnap() const {
    std::shared_lock< std::shared_mutex > lock( m_snapMutex );
    return m_lastBlockSnap;
}


}  // namespace dev::db
