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

using std::string, std::runtime_error;

namespace dev::db {

bool LevelDBSnap::isClosed() const {
    return m_isClosed;
}

// construct a snap object that can be used to read snapshot of a state
LevelDBSnap::LevelDBSnap(
    uint64_t _blockId, const leveldb::Snapshot* _snap, uint64_t _parentLevelDBReopenId )
    : m_blockId( _blockId ), m_snap( _snap ), m_parentDBReopenId( _parentLevelDBReopenId ) {
    LDB_CHECK( m_snap )
    m_creationTimeMs = LevelDB::getCurrentTimeMs();
    m_instanceId = objectCounter.fetch_add( 1 );
}


// close LevelDB snap. This will happen when all eth_calls using this snap
// complete, so it is not needed anymore
// reopen the DB
void LevelDBSnap::close( std::unique_ptr< leveldb::DB >& _parentDB, uint64_t _parentDBReopenId ) {
    // do an write lock to make sure all on-going read calls from this snap complete before
    // this to make the snap is not being used in a levelb call
    std::unique_lock< std::shared_mutex > lock( m_usageMutex );
    if ( m_isClosed ) {
        cerror << "Close called twice on a snap" << std::endl;
        return;
    }

    m_isClosed = true;

    LDB_CHECK( _parentDB );
    LDB_CHECK( m_snap );
    // sanity check. We should use the same DB handle that was used to open this snap
    LDB_CHECK( _parentDBReopenId == m_parentDBReopenId );
    // do an exclusive lock on the usage mutex
    // this to make the snap is not being used in a levelb call
    _parentDB->ReleaseSnapshot( m_snap );
    m_snap = nullptr;
}
LevelDBSnap::~LevelDBSnap() {
    if ( !m_isClosed ) {
        cerror << "LevelDB error: destroying active snap. This will leak a handle" << std::endl;
    }
}


uint64_t LevelDBSnap::getInstanceId() const {
    return m_instanceId;
}

std::atomic< uint64_t > LevelDBSnap::objectCounter = 0;

uint64_t LevelDBSnap::getCreationTimeMs() const {
    return m_creationTimeMs;
}

// this is used primary in eth_calls
leveldb::Status LevelDBSnap::getValue( const std::unique_ptr< leveldb::DB >& _db,
    leveldb::ReadOptions _readOptions, const leveldb::Slice& _key, std::string& _value ) {
    LDB_CHECK( _db );
    // lock to make sure snap is not concurrently closed while reading from it
    std::shared_lock< std::shared_mutex > lock( m_usageMutex );
    LDB_CHECK( !isClosed() )
    _readOptions.snapshot = m_snap;
    return _db->Get( _readOptions, _key, &_value );
}

std::unique_ptr< leveldb::Iterator > LevelDBSnap::getIterator(
    const std::unique_ptr< leveldb::DB >& _db, leveldb::ReadOptions _readOptions ) {
    LDB_CHECK( _db );
    // lock to make sure snap is not concurrently closed while reading from it
    std::shared_lock< std::shared_mutex > lock( m_usageMutex );
    LDB_CHECK( !isClosed() )
    _readOptions.snapshot = m_snap;
    auto iterator = _db->NewIterator( _readOptions );
    return std::unique_ptr< leveldb::Iterator >( iterator );
}

uint64_t LevelDBSnap::getParentDbReopenId() const {
    return m_parentDBReopenId;
}

}  // namespace dev::db
