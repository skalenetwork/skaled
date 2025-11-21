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

#include "LevelDBSnapManager.h"
#include "db.h"

#include <leveldb/db.h>
#include <leveldb/filter_policy.h>
#include <leveldb/write_batch.h>
#include <boost/filesystem.hpp>

#include <secp256k1_sha256.h>
#include <shared_mutex>

#define LDB_CHECK( _EXPRESSION_ )                                                             \
    if ( !( _EXPRESSION_ ) ) {                                                                \
        auto __msg__ = std::string( "State check failed::" ) + #_EXPRESSION_ + " " +          \
                       std::string( __FILE__ ) + ":" + std::to_string( __LINE__ );            \
        BOOST_THROW_EXCEPTION( dev::db::DatabaseError() << dev::errinfo_comment( __msg__ ) ); \
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

    /// Options regarding levelDB database
    struct LevelDBOptions {
        leveldb::ReadOptions readOptions = defaultReadOptions();
        leveldb::WriteOptions writeOptions = defaultWriteOptions();
        leveldb::Options dbOptions = defaultDBOptions();
    };

    /// Options regarding the wrapper of levelDB
    struct WrapperOptions {
        int64_t reopenPeriodMs = -1;
        bool enableLogger = true;
    };

    explicit LevelDB( boost::filesystem::path const& _path,
        LevelDBOptions _levelDBOptions = defaultLevelDBOptions(),
        WrapperOptions _wrapperOptions = defaultWrapperOptions() );

    ~LevelDB();

    std::string lookup( Slice _key ) const override;
    bool exists( Slice _key ) const override;
    void insert( Slice _key, Slice _value ) override;
    void kill( Slice _key ) override;

    std::unique_ptr< WriteBatchFace > createWriteBatch() const override;
    void commit( std::unique_ptr< WriteBatchFace > _batch ) override;

    void forEach( std::function< bool( Slice, Slice ) > f ) const override;

    void forEachWithPrefix(
        std::string& _prefix, std::function< bool( Slice, Slice ) > f ) const override;

    // create a read only snap after blockl processing
    void createBlockSnap( uint64_t _blockNumber );

    // get block snap for the lasty block
    std::shared_ptr< LevelDBSnap > getLastBlockSnap() const;

    // perform operations with respect to a particular read only snap
    std::string lookup( Slice _key, const std::shared_ptr< LevelDBSnap >& _snap ) const;
    bool exists( Slice _key, const std::shared_ptr< LevelDBSnap >& _snap ) const;
    void forEachWithPrefix( std::string& _prefix, std::function< bool( Slice, Slice ) > f,
        const std::shared_ptr< LevelDBSnap >& _snap ) const;

    h256 hashBase() const override;
    h256 hashBaseWithPrefix( char _prefix ) const;

    bool hashBasePartially( secp256k1_sha256_t* ctx, std::string& lastHashedKey ) const;

    void doCompaction() const;
    bool getProperty( std::string const& _name, std::string& _value ) const;

    // Return the total count of key deletes  since the start
    static uint64_t getKeyDeletesStats();
    // count of the keys that were deleted since the start of skaled
    static std::atomic< uint64_t > g_keyDeletesStats;
    // count of the keys that are scheduled to be deleted but are not yet deleted
    static std::atomic< uint64_t > g_keysToBeDeletedStats;
    static uint64_t getCurrentTimeMs();

private:
    std::unique_ptr< leveldb::DB > m_db;

    // stores and manages snap objects
    LevelDBSnapManager m_snapManager;
    // this is incremented each time this LevelDB instance is reopened
    // we reopen states LevelDB every day on archive nodes to avoid
    // meta file getting too large
    // in other cases LevelDB is never reopened to this stays zero
    std::atomic< uint64_t > m_dbReopenId = 0;
    leveldb::ReadOptions const m_readOptions;
    leveldb::WriteOptions const m_writeOptions;
    leveldb::Options m_options;
    boost::filesystem::path const m_path;

    // periodic reopen is disabled by default
    int64_t m_reopenPeriodMs = -1;
    uint64_t m_lastDBOpenTimeMs;
    mutable std::shared_mutex m_dbMutex;


    static const size_t BATCH_CHUNK_SIZE;

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
    void openDBInstanceUnsafe( bool _enableLogger = true );
    void reopenDataBaseIfNeeded();
    leveldb::Status getValue( leveldb::ReadOptions _readOptions, const leveldb::Slice& _key,
        std::string& _value, const std::shared_ptr< LevelDBSnap >& _snap ) const;
    void reopen();

    static LevelDBOptions defaultLevelDBOptions();
    static WrapperOptions defaultWrapperOptions();
};

}  // namespace dev::db
