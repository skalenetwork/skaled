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

#pragma once

#include "db.h"

#include <leveldb/db.h>
#include <leveldb/filter_policy.h>
#include <leveldb/write_batch.h>
#include <boost/filesystem.hpp>

#include <secp256k1_sha256.h>
#include <shared_mutex>

#define LDB_CHECK( _EXPRESSION_ )                                                              \
    if ( !( _EXPRESSION_ ) ) {                                                                 \
        auto __msg__ = std::string( "State check failed::" ) + #_EXPRESSION_ + " " +           \
                       std::string( __FILE__ ) + ":" + std::to_string( __LINE__ );             \
        BOOST_THROW_EXCEPTION(DatabaseError() << errinfo_comment( __msg__ ));   \
    }

namespace dev::db {

class LevelDBSnap;

class LevelDB : public DatabaseFace {
public:
    static leveldb::ReadOptions defaultReadOptions();
    static leveldb::WriteOptions defaultWriteOptions();
    static leveldb::Options defaultDBOptions();
    static leveldb::ReadOptions defaultSnapshotReadOptions();
    static leveldb::Options defaultSnapshotDBOptions();

    explicit LevelDB( boost::filesystem::path const& _path,
        leveldb::ReadOptions _readOptions = defaultReadOptions(),
        leveldb::WriteOptions _writeOptions = defaultWriteOptions(),
        leveldb::Options _dbOptions = defaultDBOptions(), int64_t _reopenPeriodMs = -1 );

    ~LevelDB();

    std::string lookup( Slice _key ) const override;
    bool exists( Slice _key ) const override;

    std::string lookup( Slice _key, const std::shared_ptr<LevelDBSnap>& _snap  ) const;
    bool exists( Slice _key, const std::shared_ptr<LevelDBSnap>& _snap ) const;



    void insert( Slice _key, Slice _value ) override;
    void kill( Slice _key ) override;

    std::unique_ptr< WriteBatchFace > createWriteBatch() const override;
    void commit( std::unique_ptr< WriteBatchFace > _batch ) override;

    void forEach( std::function< bool( Slice, Slice ) > f ) const override;

    void forEachWithPrefix(
        std::string& _prefix, std::function< bool( Slice, Slice ) > f ) const override;

    h256 hashBase() const override;
    h256 hashBaseWithPrefix( char _prefix ) const;

    bool hashBasePartially( secp256k1_sha256_t* ctx, std::string& lastHashedKey ) const;

    void doCompaction() const;

    void createBlockSnap(uint64_t _blockId);

    std::shared_ptr<LevelDBSnap> getLastBlockSnap(uint64_t _blockId);

    void closeSnap(LevelDBSnap& _snap);


    // Return the total count of key deletes  since the start
    static uint64_t getKeyDeletesStats();
    // count of the keys that were deleted since the start of skaled
    static std::atomic< uint64_t > g_keyDeletesStats;
    // count of the keys that are scheduled to be deleted but are not yet deleted
    static std::atomic< uint64_t > g_keysToBeDeletedStats;
    static uint64_t getCurrentTimeMs();

private:
    std::unique_ptr< leveldb::DB > m_db;
    // this identify is guaranteed to be unique for each m_db reference
    // so it can be used to compare to references
    std::atomic<uint64_t> m_dbIdentifier = 0;
    leveldb::ReadOptions const m_readOptions;
    leveldb::WriteOptions const m_writeOptions;
    leveldb::Options m_options;
    boost::filesystem::path const m_path;

    // periodic reopen is disabled by default
    int64_t m_reopenPeriodMs = -1;
    uint64_t m_lastDBOpenTimeMs;
    mutable std::shared_mutex m_dbMutex;

    // old snaps contains snap objects for older blocks
    // these objects are alive untils the
    // corresponding eth_calls complete
    std::map<std::uint64_t , std::shared_ptr<LevelDBSnap>> oldSnaps;
    std::shared_ptr<LevelDBSnap> m_lastBlockSnap;
    // mutex to protect oldSnaps and m_lastBlockSnap;
    std::shared_mutex m_snapMutex;


    static constexpr size_t BATCH_CHUNK_SIZE = 10000;
    // time after an existing old snap will be closed if no-one is using it
    static const size_t OLD_SNAP_LIFETIME_MS = 10000;
    // time after an existing old snap will be closed it is used in eth_call
    // this will cause the eth_call to return an error
    static const size_t FORCE_SNAP_CLOSE_TIME_MS = 3000;

    class SharedDBGuard {
        const LevelDB& m_levedlDB;


    public:
        explicit SharedDBGuard( const LevelDB& _levedDB ) : m_levedlDB( _levedDB ) {
            if ( m_levedlDB.m_reopenPeriodMs < 0 )
                return;
            m_levedlDB.m_dbMutex.lock_shared();
        }


        ~SharedDBGuard() {
            if ( m_levedlDB.m_reopenPeriodMs < 0 )
                return;
            m_levedlDB.m_dbMutex.unlock_shared();
        }
    };

    class ExclusiveDBGuard {
        LevelDB& m_levedlDB;

    public:
        ExclusiveDBGuard( LevelDB& _levedDB ) : m_levedlDB( _levedDB ) {
            if ( m_levedlDB.m_reopenPeriodMs < 0 )
                return;
            m_levedlDB.m_dbMutex.lock();
        }

        ~ExclusiveDBGuard() {
            if ( m_levedlDB.m_reopenPeriodMs < 0 )
                return;
            m_levedlDB.m_dbMutex.unlock();
        }
    };
    void openDBInstanceUnsafe();
    void reopenDataBaseIfNeeded();
    void cleanUnusedOldSnapsUnsafe( uint64_t _maxSnapLifetimeMs );
};

}  // namespace dev::db
