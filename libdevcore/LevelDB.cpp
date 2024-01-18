/*
    Modifications Copyright (C) 2018 SKALE Labs

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


#include "LevelDB.h"
#include "Assertions.h"
#include "Log.h"
#include <libdevcore/microprofile.h>

namespace dev {
namespace db {

unsigned c_maxOpenLeveldbFiles = 25;

const size_t LevelDB::BATCH_CHUNK_SIZE = 10000;

namespace {
inline leveldb::Slice toLDBSlice( Slice _slice ) {
    return leveldb::Slice( _slice.data(), _slice.size() );
}

DatabaseStatus toDatabaseStatus( leveldb::Status const& _status ) {
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

void checkStatus( leveldb::Status const& _status, boost::filesystem::path const& _path = {} ) {
    if ( _status.ok() )
        return;

    DatabaseError ex;
    ex << errinfo_dbStatusCode( toDatabaseStatus( _status ) )
       << errinfo_dbStatusString( _status.ToString() );
    if ( !_path.empty() )
        ex << errinfo_path( _path.string() );

    BOOST_THROW_EXCEPTION( ex );
}

class LevelDBWriteBatch : public WriteBatchFace {
public:
    void insert( Slice _key, Slice _value ) override;
    void kill( Slice _key ) override;

    leveldb::WriteBatch const& writeBatch() const { return m_writeBatch; }
    leveldb::WriteBatch& writeBatch() { return m_writeBatch; }

private:
    leveldb::WriteBatch m_writeBatch;
    std::atomic< uint64_t > keysToBeDeletedCount;
};

void LevelDBWriteBatch::insert( Slice _key, Slice _value ) {
    MICROPROFILE_SCOPEI( "LevelDBWriteBatch", "insert", MP_LAVENDERBLUSH );
    m_writeBatch.Put( toLDBSlice( _key ), toLDBSlice( _value ) );
}

void LevelDBWriteBatch::kill( Slice _key ) {
    LevelDB::g_keysToBeDeletedStats++;
    m_writeBatch.Delete( toLDBSlice( _key ) );
}

}  // namespace

leveldb::ReadOptions LevelDB::defaultReadOptions() {
    return leveldb::ReadOptions();
}

leveldb::WriteOptions LevelDB::defaultWriteOptions() {
    leveldb::WriteOptions writeOptions = leveldb::WriteOptions();
    //    writeOptions.sync = true;
    return writeOptions;
}

leveldb::Options LevelDB::defaultDBOptions() {
    leveldb::Options options;
    options.create_if_missing = true;
    options.max_open_files = c_maxOpenLeveldbFiles;
    options.filter_policy = leveldb::NewBloomFilterPolicy( 10 );
    return options;
}

leveldb::ReadOptions LevelDB::defaultSnapshotReadOptions() {
    leveldb::ReadOptions options;
    options.fill_cache = false;
    return options;
}

leveldb::Options LevelDB::defaultSnapshotDBOptions() {
    leveldb::Options options;
    options.create_if_missing = true;
    options.max_open_files = c_maxOpenLeveldbFiles;
    return options;
}

LevelDB::LevelDB( boost::filesystem::path const& _path, leveldb::ReadOptions _readOptions,
    leveldb::WriteOptions _writeOptions, leveldb::Options _dbOptions )
    : m_db( nullptr ),
      m_readOptions( std::move( _readOptions ) ),
      m_writeOptions( std::move( _writeOptions ) ),
      m_options( std::move( _dbOptions ) ),
      m_path( _path ) {
    auto db = static_cast< leveldb::DB* >( nullptr );
    auto const status = leveldb::DB::Open( m_options, _path.string(), &db );
    checkStatus( status, _path );

    assert( db );
    m_db.reset( db );
}

LevelDB::~LevelDB() {
    if ( m_db )
        m_db.reset();
    if ( m_options.filter_policy )
        delete m_options.filter_policy;
}

std::string LevelDB::lookup( Slice _key ) const {
    leveldb::Slice const key( _key.data(), _key.size() );
    std::string value;
    auto const status = m_db->Get( m_readOptions, key, &value );
    if ( status.IsNotFound() )
        return std::string();

    checkStatus( status );
    return value;
}

bool LevelDB::exists( Slice _key ) const {
    std::string value;
    leveldb::Slice const key( _key.data(), _key.size() );
    auto const status = m_db->Get( m_readOptions, key, &value );
    if ( status.IsNotFound() )
        return false;

    checkStatus( status );
    return true;
}

void LevelDB::insert( Slice _key, Slice _value ) {
    leveldb::Slice const key( _key.data(), _key.size() );
    leveldb::Slice const value( _value.data(), _value.size() );
    auto const status = m_db->Put( m_writeOptions, key, value );
    checkStatus( status );
}

void LevelDB::kill( Slice _key ) {
    leveldb::Slice const key( _key.data(), _key.size() );
    auto const status = m_db->Delete( m_writeOptions, key );
    // At this point the key is not actually deleted. It will be deleted when the batch
    // is committed
    g_keysToBeDeletedStats++;
    checkStatus( status );
}

std::unique_ptr< WriteBatchFace > LevelDB::createWriteBatch() const {
    return std::unique_ptr< WriteBatchFace >( new LevelDBWriteBatch() );
}

void LevelDB::commit( std::unique_ptr< WriteBatchFace > _batch ) {
    if ( !_batch ) {
        BOOST_THROW_EXCEPTION( DatabaseError() << errinfo_comment( "Cannot commit null batch" ) );
    }
    auto* batchPtr = dynamic_cast< LevelDBWriteBatch* >( _batch.get() );
    if ( !batchPtr ) {
        BOOST_THROW_EXCEPTION(
            DatabaseError() << errinfo_comment( "Invalid batch type passed to LevelDB::commit" ) );
    }
    auto const status = m_db->Write( m_writeOptions, &batchPtr->writeBatch() );
    // Commit happened. This means the keys actually got deleted in LevelDB. Increment key deletes
    // stats and set g_keysToBeDeletedStats to zero
    g_keyDeletesStats += g_keysToBeDeletedStats;
    g_keysToBeDeletedStats = 0;
    checkStatus( status );
}

void LevelDB::forEach( std::function< bool( Slice, Slice ) > f ) const {
    cwarn << "Iterating over the entire LevelDB database: " << this->m_path;
    std::unique_ptr< leveldb::Iterator > itr( m_db->NewIterator( m_readOptions ) );
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

void LevelDB::forEachWithPrefix(
    std::string& _prefix, std::function< bool( Slice, Slice ) > f ) const {
    cnote << "Iterating over the LevelDB prefix: " << _prefix;
    std::unique_ptr< leveldb::Iterator > itr( m_db->NewIterator( m_readOptions ) );
    if ( itr == nullptr ) {
        BOOST_THROW_EXCEPTION( DatabaseError() << errinfo_comment( "null iterator" ) );
    }
    auto keepIterating = true;
    auto prefixSlice = leveldb::Slice( _prefix );
    for ( itr->Seek( prefixSlice );
          keepIterating && itr->Valid() && itr->key().starts_with( prefixSlice ); itr->Next() ) {
        auto const dbKey = itr->key();
        auto const dbValue = itr->value();
        Slice const key( dbKey.data(), dbKey.size() );
        Slice const value( dbValue.data(), dbValue.size() );
        keepIterating = f( key, value );
    }
}

h256 LevelDB::hashBase() const {
    std::unique_ptr< leveldb::Iterator > it( m_db->NewIterator( m_readOptions ) );
    if ( it == nullptr ) {
        BOOST_THROW_EXCEPTION( DatabaseError() << errinfo_comment( "null iterator" ) );
    }

    secp256k1_sha256_t ctx;
    secp256k1_sha256_initialize( &ctx );
    for ( it->SeekToFirst(); it->Valid(); it->Next() ) {
        std::string keyTmp = it->key().ToString();
        std::string valueTmp = it->value().ToString();
        // HACK! For backward compatibility! When snapshot could happen between update of two nodes
        // - it would lead to stateRoot mismatch
        // TODO Move this logic to separate "compatiliblity layer"!
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

h256 LevelDB::hashBaseWithPrefix( char _prefix ) const {
    std::unique_ptr< leveldb::Iterator > it( m_db->NewIterator( m_readOptions ) );
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

void LevelDB::hashBasePartially( secp256k1_sha256_t* ctx, std::string& lastHashedKey ) const {
    std::unique_ptr< leveldb::Iterator > it( m_db->NewIterator( m_readOptions ) );
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
        // HACK! For backward compatibility! When snapshot could happen between update of two nodes
        // - it would lead to stateRoot mismatch
        // TODO Move this logic to separate "compatiliblity layer"!
        if ( keyTmp == "pieceUsageBytes" )
            continue;
        std::string keyValue = keyTmp + valueTmp;
        const std::vector< uint8_t > usc( keyValue.begin(), keyValue.end() );
        bytesConstRef strKeyValue( usc.data(), usc.size() );
        secp256k1_sha256_write( ctx, strKeyValue.data(), strKeyValue.size() );
        ++counter;
    }

    if ( it->Valid() )
        lastHashedKey = it->key().ToString();
    else
        lastHashedKey = "stop";
}

void LevelDB::doCompaction() const {
    m_db->CompactRange( nullptr, nullptr );
}

std::atomic< uint64_t > LevelDB::g_keysToBeDeletedStats = 0;
std::atomic< uint64_t > LevelDB::g_keyDeletesStats = 0;

uint64_t LevelDB::getKeyDeletesStats() {
    return g_keyDeletesStats;
}

}  // namespace db
}  // namespace dev
