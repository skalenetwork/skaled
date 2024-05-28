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


#include "LevelDBSnap.h"
#include "Assertions.h"
#include "LevelDB.h"

const leveldb::Snapshot* dev::db::LevelDBSnap::getSnapHandle() const {
    return m_snap;
}

using std::string, std::runtime_error;

namespace dev::db {


LevelDBSnap::LevelDBSnap(
    uint64_t _blockId, const leveldb::Snapshot* _snap, uint64_t _parentLevelDBIdentifier )
    : m_blockId( _blockId ), m_snap( _snap ), m_parentLevelDBId( _parentLevelDBIdentifier ) {
    LDB_CHECK( m_snap )
    m_creationTimeMs = LevelDB::getCurrentTimeMs();
    m_objectId = objectCounter.fetch_add(1);
}

uint64_t LevelDBSnap::getParentLevelDBId() const {
    return m_parentLevelDBId;
}

// close LevelDB snap. This will happen when all eth_calls using this snap
// complete, so it is not needed anymore
// reopen the DB
void LevelDBSnap::close( std::unique_ptr< leveldb::DB >& _parentDB, uint64_t _parentDBIdentifier ) {
    auto isClosed = m_isClosed.exchange( true );
    if ( isClosed ) {
        // this should never happen
        cwarn << "Close called twice on a snap";
        return;
    }

    LDB_CHECK( _parentDB );
    LDB_CHECK( m_snap );
    // sanity check. We should use the same DB referencr that was used to open this snap
    if ( _parentDBIdentifier != m_parentLevelDBId ) {
        // this should never happen, since it means that we are attempting to close snapshot
        // after its parent database is closed due to reopen
        // normally we close all snapshots before reopenining the databasew
        cwarn << "Closing the snapshot after the database is closed";
        return;
    }

    _parentDB->ReleaseSnapshot( this->m_snap );
    m_snap = nullptr;
}
LevelDBSnap::~LevelDBSnap() {
    // LevelDB should be closed before releasing it, otherwise
    // we use cerr here since destructor may be called during late stages of
    // skaled exit where logging is not available
    if ( !m_isClosed ) {
        std::cerr << "LevelDB warning: destroying active snap" << std::endl;
    }
}


uint64_t LevelDBSnap::getObjectId() const {
    return m_objectId;
}

std::atomic<uint64_t> LevelDBSnap::objectCounter = 0;
uint64_t LevelDBSnap::getCreationTimeMs() const {
    return m_creationTimeMs;
}
std::shared_mutex& LevelDBSnap::getCloseMutex()  {
    return m_closeMutex;
}


}  // namespace dev::db
