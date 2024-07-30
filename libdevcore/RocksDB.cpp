/*
    Modifications Copyright (C) 2024 SKALE Labs

    This file is part of skaled.

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

#include "RocksDB.h"
#include "Assertions.h"
#include "Log.h"
#include <libdevcore/microprofile.h>

#include <rocksdb/table.h>

using std::string, std::runtime_error;

namespace dev::db {

unsigned c_maxOpenRocksdbFiles = 25;

const size_t RocksDB::BATCH_CHUNK_SIZE = 10000;

namespace {
inline rocksdb::Slice toLDBSlice( Slice _slice ) {
    return rocksdb::Slice( _slice.data(), _slice.size() );
}

DatabaseStatus toDatabaseStatus( rocksdb::Status const& _status ) {
    if ( _status.ok() )
        return DatabaseStatus::Ok;
    else if ( _status.IsIOError() )
        return DatabaseStatus::IOError;
    else if ( _status.IsCorruption() )
        return DatabaseStatus::Corruption;
    else if ( _status.IsNotFound() )
        return DatabaseStatus::NotFound;
    else
        return DatabaseStatus::Unknown;
}

void checkStatus( rocksdb::Status const& _status, boost::filesystem::path const& _path = {} ) {
    if ( _status.ok() )
        return;

    DatabaseError ex;
    ex << errinfo_dbStatusCode( toDatabaseStatus( _status ) )
       << errinfo_dbStatusString( _status.ToString() );
    if ( !_path.empty() )
        ex << errinfo_path( _path.string() );

    BOOST_THROW_EXCEPTION( ex );
}

class RocksDBWriteBatch : public WriteBatchFace {
public:
    void insert( Slice _key, Slice _value ) override;
    void kill( Slice _key ) override;

    rocksdb::WriteBatch const& writeBatch() const { return m_writeBatch; }
    rocksdb::WriteBatch& writeBatch() { return m_writeBatch; }

private:
    rocksdb::WriteBatch m_writeBatch;
};

void RocksDBWriteBatch::insert( Slice _key, Slice _value ) {
    MICROPROFILE_SCOPEI( "LevelDBWriteBatch", "insert", MP_LAVENDERBLUSH );
    m_writeBatch.Put( toLDBSlice( _key ), toLDBSlice( _value ) );
}

void RocksDBWriteBatch::kill( Slice _key ) {
    RocksDB::g_keysToBeDeletedStats++;
    m_writeBatch.Delete( toLDBSlice( _key ) );
}

}  // namespace

rocksdb::ReadOptions RocksDB::defaultReadOptions() {
    rocksdb::ReadOptions readOptions = rocksdb::ReadOptions();
    readOptions.ignore_range_deletions = true;
    return readOptions;
}

rocksdb::WriteOptions RocksDB::defaultWriteOptions() {
    rocksdb::WriteOptions writeOptions = rocksdb::WriteOptions();
    return writeOptions;
}

rocksdb::Options RocksDB::defaultDBOptions() {
    rocksdb::Options options;
    options.create_if_missing = true;
    options.max_open_files = c_maxOpenRocksdbFiles;
    options.max_background_jobs = 8;
    options.max_subcompactions = 4;

    rocksdb::BlockBasedTableOptions table_options;
    table_options.filter_policy.reset( rocksdb::NewRibbonFilterPolicy( 10 ) );
    options.table_factory.reset( rocksdb::NewBlockBasedTableFactory( table_options ) );
    return options;
}

rocksdb::ReadOptions RocksDB::defaultSnapshotReadOptions() {
    rocksdb::ReadOptions options;
    options.ignore_range_deletions = true;
    options.fill_cache = false;
    options.async_io = true;
    return options;
}

rocksdb::Options RocksDB::defaultSnapshotDBOptions() {
    rocksdb::Options options;
    options.create_if_missing = true;
    options.max_open_files = c_maxOpenRocksdbFiles;
    return options;
}

RocksDB::RocksDB( boost::filesystem::path const& _path, rocksdb::ReadOptions _readOptions,
    rocksdb::WriteOptions _writeOptions, rocksdb::Options _dbOptions, int64_t _reopenPeriodMs )
    : m_db( nullptr ),
      m_readOptions( std::move( _readOptions ) ),
      m_writeOptions( std::move( _writeOptions ) ),
      m_options( std::move( _dbOptions ) ),
      m_path( _path ),
      m_reopenPeriodMs( _reopenPeriodMs ) {
    openDBInstanceUnsafe();
}

// this does not hold any locks so it needs to be called
// either from a constructor or from a function that holds a lock on m_db
void RocksDB::openDBInstanceUnsafe() {
    cnote << "Time to (re)open LevelDB at " + m_path.string();
    auto startTimeMs = getCurrentTimeMs();
    auto db = static_cast< rocksdb::DB* >( nullptr );
    auto const status = rocksdb::DB::Open( m_options, m_path.string(), &db );
    checkStatus( status, m_path );

    if ( !db ) {
        BOOST_THROW_EXCEPTION( runtime_error( string( "Null db in " ) + __FUNCTION__ ) );
    }

    m_db.reset( db );
    m_lastDBOpenTimeMs = getCurrentTimeMs();
    cnote << "LEVELDB_OPENED:TIME_MS:" << m_lastDBOpenTimeMs - startTimeMs;
}
uint64_t RocksDB::getCurrentTimeMs() {
    auto currentTime = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast< std::chrono::milliseconds >( currentTime ).count();
}

RocksDB::~RocksDB() {
    if ( m_db )
        m_db.reset();
}

std::string RocksDB::lookup( Slice _key ) const {
    rocksdb::Slice const key( _key.data(), _key.size() );
    std::string value;

    rocksdb::Status status;
    {
        SharedDBGuard readLock( *this );
        status = m_db->Get( m_readOptions, key, &value );
    }
    if ( status.IsNotFound() )
        return std::string();

    checkStatus( status );
    return value;
}

bool RocksDB::exists( Slice _key ) const {
    std::string value;
    rocksdb::Slice const key( _key.data(), _key.size() );
    rocksdb::Status status;
    {
        SharedDBGuard lock( *this );
        status = m_db->Get( m_readOptions, key, &value );
    }
    if ( status.IsNotFound() )
        return false;

    checkStatus( status );
    return true;
}

void RocksDB::insert( Slice _key, Slice _value ) {
    rocksdb::Slice const key( _key.data(), _key.size() );
    rocksdb::Slice const value( _value.data(), _value.size() );
    rocksdb::Status status;
    {
        SharedDBGuard lock( *this );
        status = m_db->Put( m_writeOptions, key, value );
    }
    checkStatus( status );
}

void RocksDB::kill( Slice _key ) {
    rocksdb::Slice const key( _key.data(), _key.size() );
    auto const status = m_db->Delete( m_writeOptions, key );
    // At this point the key is not actually deleted. It will be deleted when the batch
    // is committed
    g_keysToBeDeletedStats++;
    checkStatus( status );
}

std::unique_ptr< WriteBatchFace > RocksDB::createWriteBatch() const {
    return std::unique_ptr< WriteBatchFace >( new RocksDBWriteBatch() );
}

void RocksDB::commit( std::unique_ptr< WriteBatchFace > _batch ) {
    if ( !_batch ) {
        BOOST_THROW_EXCEPTION( DatabaseError() << errinfo_comment( "Cannot commit null batch" ) );
    }
    auto* batchPtr = dynamic_cast< RocksDBWriteBatch* >( _batch.get() );
    if ( !batchPtr ) {
        BOOST_THROW_EXCEPTION(
            DatabaseError() << errinfo_comment( "Invalid batch type passed to LevelDB::commit" ) );
    }
    rocksdb::Status status;
    {
        SharedDBGuard lock( *this );
        status = m_db->Write( m_writeOptions, &batchPtr->writeBatch() );
    }
    // Commit happened. This means the keys actually got deleted in LevelDB. Increment key deletes
    // stats and set g_keysToBeDeletedStats to zero
    g_keyDeletesStats += g_keysToBeDeletedStats;
    g_keysToBeDeletedStats = 0;

    checkStatus( status );

    // now lets check if it is time to reopen the database

    reopenDataBaseIfNeeded();
}
void RocksDB::reopenDataBaseIfNeeded() {
    if ( m_reopenPeriodMs < 0 ) {
        // restarts not enabled
        return;
    }

    auto currentTimeMs = getCurrentTimeMs();

    if ( currentTimeMs - m_lastDBOpenTimeMs >= ( uint64_t ) m_reopenPeriodMs ) {
        ExclusiveDBGuard lock( *this );
        // releasing unique pointer will cause database destructor to be called that will close db
        m_db.reset();
        // now open db while holding the exclusive lock
        openDBInstanceUnsafe();
    }
}

void RocksDB::forEach( std::function< bool( Slice, Slice ) > f ) const {
    cwarn << "Iterating over the entire LevelDB database: " << this->m_path;
    SharedDBGuard lock( *this );
    std::unique_ptr< rocksdb::Iterator > itr( m_db->NewIterator( m_readOptions ) );
    if ( itr == nullptr ) {
        BOOST_THROW_EXCEPTION( DatabaseError() << errinfo_comment( "null iterator" ) );
    }
    auto keepIterating = true;
    for ( itr->SeekToFirst(); keepIterating && itr->Valid(); itr->Next() ) {
        auto const dbKey = itr->key();
        auto const dbValue = itr->value();
        Slice const key( dbKey.data(), dbKey.size() );
        Slice const value( dbValue.data(), dbValue.size() );
        keepIterating = f( key, value );
    }
}

void RocksDB::forEachWithPrefix(
    std::string& _prefix, std::function< bool( Slice, Slice ) > f ) const {
    cnote << "Iterating over the LevelDB prefix: " << _prefix;
    SharedDBGuard lock( *this );
    std::unique_ptr< rocksdb::Iterator > itr( m_db->NewIterator( m_readOptions ) );
    if ( itr == nullptr ) {
        BOOST_THROW_EXCEPTION( DatabaseError() << errinfo_comment( "null iterator" ) );
    }
    auto keepIterating = true;
    auto prefixSlice = rocksdb::Slice( _prefix );
    for ( itr->Seek( prefixSlice );
          keepIterating && itr->Valid() && itr->key().starts_with( prefixSlice ); itr->Next() ) {
        auto const dbKey = itr->key();
        auto const dbValue = itr->value();
        Slice const key( dbKey.data(), dbKey.size() );
        Slice const value( dbValue.data(), dbValue.size() );
        keepIterating = f( key, value );
    }
}

h256 RocksDB::hashBase() const {
    SharedDBGuard lock( *this );
    std::unique_ptr< rocksdb::Iterator > it( m_db->NewIterator( m_readOptions ) );
    if ( it == nullptr ) {
        BOOST_THROW_EXCEPTION( DatabaseError() << errinfo_comment( "null iterator" ) );
    }

    secp256k1_sha256_t ctx;
    secp256k1_sha256_initialize( &ctx );
    for ( it->SeekToFirst(); it->Valid(); it->Next() ) {
        std::string keyTmp = it->key().ToString();
        std::string valueTmp = it->value().ToString();
        // For backward compatibility. When snapshot could happen between update of two nodes
        // it would lead to stateRoot mismatch
        // TODO Move this logic to separate compatiliblity layer
        if ( keyTmp == "pieceUsageBytes" )
            continue;
        std::string keyValue = keyTmp + valueTmp;
        const std::vector< uint8_t > usc( keyValue.begin(), keyValue.end() );
        bytesConstRef strKeyValue( usc.data(), usc.size() );
        secp256k1_sha256_write( &ctx, strKeyValue.data(), strKeyValue.size() );
    }

    h256 hash;
    secp256k1_sha256_finalize( &ctx, hash.data() );
    return hash;
}

h256 RocksDB::hashBaseWithPrefix( char _prefix ) const {
    SharedDBGuard lock( *this );
    std::unique_ptr< rocksdb::Iterator > it( m_db->NewIterator( m_readOptions ) );
    if ( it == nullptr ) {
        BOOST_THROW_EXCEPTION( DatabaseError() << errinfo_comment( "null iterator" ) );
    }

    secp256k1_sha256_t ctx;
    secp256k1_sha256_initialize( &ctx );
    for ( it->SeekToFirst(); it->Valid(); it->Next() ) {
        if ( it->key()[0] == _prefix ) {
            std::string keyTmp = it->key().ToString();
            std::string valueTmp = it->value().ToString();
            std::string keyValue = keyTmp + valueTmp;
            const std::vector< uint8_t > usc( keyValue.begin(), keyValue.end() );
            bytesConstRef strKeyValue( usc.data(), usc.size() );
            secp256k1_sha256_write( &ctx, strKeyValue.data(), strKeyValue.size() );
        }
    }
    h256 hash;
    secp256k1_sha256_finalize( &ctx, hash.data() );
    return hash;
}

bool RocksDB::hashBasePartially( secp256k1_sha256_t* ctx, std::string& lastHashedKey ) const {
    SharedDBGuard lock( *this );
    std::unique_ptr< rocksdb::Iterator > it( m_db->NewIterator( m_readOptions ) );
    if ( it == nullptr ) {
        BOOST_THROW_EXCEPTION( DatabaseError() << errinfo_comment( "null iterator" ) );
    }

    if ( lastHashedKey != "start" )
        it->Seek( lastHashedKey );
    else
        it->SeekToFirst();

    for ( size_t counter = 0; it->Valid() && counter < BATCH_CHUNK_SIZE; it->Next() ) {
        std::string keyTmp = it->key().ToString();
        std::string valueTmp = it->value().ToString();
        // For backward compatibility. When snapshot could happen between update of two nodes
        // it would lead to stateRoot mismatch
        // TODO Move this logic to separate compatiliblity layer
        if ( keyTmp == "pieceUsageBytes" )
            continue;
        std::string keyValue = keyTmp + valueTmp;
        const std::vector< uint8_t > usc( keyValue.begin(), keyValue.end() );
        bytesConstRef strKeyValue( usc.data(), usc.size() );
        secp256k1_sha256_write( ctx, strKeyValue.data(), strKeyValue.size() );
        ++counter;
    }

    if ( it->Valid() ) {
        lastHashedKey = it->key().ToString();
        return true;
    } else
        return false;
}

void RocksDB::doCompaction() const {
    SharedDBGuard lock( *this );
}

std::atomic< uint64_t > RocksDB::g_keysToBeDeletedStats = 0;
std::atomic< uint64_t > RocksDB::g_keyDeletesStats = 0;

uint64_t RocksDB::getKeyDeletesStats() {
    return g_keyDeletesStats;
}

}  // namespace dev::db
