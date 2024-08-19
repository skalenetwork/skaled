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

#ifndef ROCKSDB_H
#define ROCKSDB_H

#include "RocksDBSnapManager.h"
#include "db.h"

#include <rocksdb/db.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/write_batch.h>
#include <boost/filesystem.hpp>

#include <secp256k1_sha256.h>
#include <shared_mutex>

#define RDB_CHECK( _EXPRESSION_ )                                                              \
    if ( !( _EXPRESSION_ ) ) {                                                                 \
        auto __msg__ = std::string( "State check failed::" ) + #_EXPRESSION_ + " " +           \
                       std::string( __FILE__ ) + ":" + std::to_string( __LINE__ );             \
        BOOST_THROW_EXCEPTION(dev::db::DatabaseError() << dev::errinfo_comment( __msg__ ));   \
    }

namespace dev::db {

class RocksDBSnap;

class RocksDB : public DatabaseFace {
public:
    static rocksdb::ReadOptions defaultReadOptions();
    static rocksdb::WriteOptions defaultWriteOptions();
    static rocksdb::Options defaultDBOptions();
    static rocksdb::ReadOptions defaultSnapshotReadOptions();
    static rocksdb::Options defaultSnapshotDBOptions();

    explicit RocksDB( boost::filesystem::path const& _path,
        rocksdb::ReadOptions _readOptions = defaultReadOptions(),
        rocksdb::WriteOptions _writeOptions = defaultWriteOptions(),
        rocksdb::Options _dbOptions = defaultDBOptions(), int64_t _reopenPeriodMs = -1 );

    ~RocksDB();

    std::string lookup( Slice _key ) const override;
    bool exists( Slice _key ) const override;

    std::string lookup( Slice _key, const std::shared_ptr<RocksDBSnap>& _snap ) const;
    bool exists( Slice _key, const std::shared_ptr<RocksDBSnap>& _snap ) const;
    void insert( Slice _key, Slice _value ) override;
    void kill( Slice _key ) override;

    std::unique_ptr< WriteBatchFace > createWriteBatch() const override;
    void commit( std::unique_ptr< WriteBatchFace > _batch ) override;

    void forEach( std::function< bool( Slice, Slice ) > f ) const override;

    void forEachWithPrefix(
        std::string& _prefix, std::function< bool( Slice, Slice ) > f ) const override;

    void forEachWithPrefix(
        std::string& _prefix, std::function< bool( Slice, Slice ) > f,
        const std::shared_ptr<RocksDBSnap>& _snap ) const;

    h256 hashBase() const override;
    h256 hashBaseWithPrefix( char _prefix ) const;

    bool hashBasePartially( secp256k1_sha256_t* ctx, std::string& lastHashedKey ) const;

    void doCompaction() const;

    void createBlockSnap(uint64_t _blockNumber );

    // Return the total count of key deletes  since the start
    static uint64_t getKeyDeletesStats();
    // count of the keys that were deleted since the start of skaled
    static std::atomic< uint64_t > g_keyDeletesStats;
    // count of the keys that are scheduled to be deleted but are not yet deleted
    static std::atomic< uint64_t > g_keysToBeDeletedStats;
    static uint64_t getCurrentTimeMs();
    std::shared_ptr< RocksDBSnap > getLastBlockSnap() const;
private:
    std::unique_ptr< rocksdb::DB > m_db;
    rocksdb::ReadOptions const m_readOptions;
    rocksdb::WriteOptions const m_writeOptions;
    rocksdb::Options m_options;
    boost::filesystem::path const m_path;
    std::atomic<uint64_t> m_dbReopenId = 0;

    // periodic reopen is disabled by default
    int64_t m_reopenPeriodMs = -1;
    uint64_t m_lastDBOpenTimeMs;
    mutable std::shared_mutex m_dbMutex;

    RocksDBSnapManager m_snapManager;

    static const size_t BATCH_CHUNK_SIZE;

    class SharedDBGuard {
        const RocksDB& m_rocksDB;

    public:
        explicit SharedDBGuard( const RocksDB& _rocksDB ) : m_rocksDB( _rocksDB ) {
            if ( m_rocksDB.m_reopenPeriodMs < 0 )
                return;
            m_rocksDB.m_dbMutex.lock_shared();
        }

        ~SharedDBGuard() {
            if ( m_rocksDB.m_reopenPeriodMs < 0 )
                return;
            m_rocksDB.m_dbMutex.unlock_shared();
        }
    };

    class ExclusiveDBGuard {
        RocksDB& m_rocksDB;

    public:
        ExclusiveDBGuard( RocksDB& _rocksDB ) : m_rocksDB( _rocksDB ) {
            if ( m_rocksDB.m_reopenPeriodMs < 0 )
                return;
            m_rocksDB.m_dbMutex.lock();
        }

        ~ExclusiveDBGuard() {
            if ( m_rocksDB.m_reopenPeriodMs < 0 )
                return;
            m_rocksDB.m_dbMutex.unlock();
        }
    };
    void openDBInstanceUnsafe();
    void reopenDataBaseIfNeeded();
    rocksdb::Status getValue( rocksdb::ReadOptions _readOptions, const rocksdb::Slice& _key,
        std::string& _value, const std::shared_ptr< RocksDBSnap >& _snap ) const;
    void reopen();
};

}  // namespace dev::db

#endif  // ROCKSDB_H
