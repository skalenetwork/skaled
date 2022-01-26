/*
    Copyright (C) 2018 SKALE Labs

    This file is part of skaled.

    skaled is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skaled is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skaled.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file SkaleStats.h
 * @authors:
 *   Sergiy Lavrynenko <sergiy@skalelabs.com>
 * @date 2019
 */

#pragma once

#include "SkaleStatsFace.h"
#include "SkaleStatsSite.h"
#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server.h>
#include <libdevcore/Common.h>

#include <libethereum/ChainParams.h>

//#include <nlohmann/json.hpp>
#include <json.hpp>

#include <time.h>

#include <skutils/dispatch.h>
#include <skutils/multithreading.h>
#include <skutils/utils.h>

#include <atomic>
#include <list>
#include <set>

namespace dev {
class NetworkFace;
class KeyPair;
namespace eth {
class AccountHolder;
struct TransactionSkeleton;
class Interface;
};  // namespace eth

namespace tracking {

class txn_entry {
public:
    dev::u256 hash_;
    time_t ts_;  // second accuracy used here
    txn_entry();
    txn_entry( dev::u256 hash );
    txn_entry( const txn_entry& other );
    txn_entry( txn_entry&& other );
    ~txn_entry();
    bool operator!() const;
    txn_entry& operator=( const txn_entry& other );
    bool operator==( const txn_entry& other ) const;
    bool operator!=( const txn_entry& other ) const;
    bool operator<( const txn_entry& other ) const;
    bool operator<=( const txn_entry& other ) const;
    bool operator>( const txn_entry& other ) const;
    bool operator>=( const txn_entry& other ) const;
    bool operator==( dev::u256 hash ) const;
    bool operator!=( dev::u256 hash ) const;
    bool operator<( dev::u256 hash ) const;
    bool operator<=( dev::u256 hash ) const;
    bool operator>( dev::u256 hash ) const;
    bool operator>=( dev::u256 hash ) const;
    bool empty() const;
    void clear();
    txn_entry& assign( const txn_entry& other );
    int compare( dev::u256 hash ) const;
    int compare( const txn_entry& other ) const;
    void setNowTimeStamp();
    nlohmann::json toJSON() const;
    bool fromJSON( const nlohmann::json& jo );
};  /// class txn_entry

class pending_ima_txns : public skutils::json_config_file_accessor {
public:
    typedef std::list< txn_entry > list_txns_t;
    typedef std::set< txn_entry > set_txns_t;

private:
    list_txns_t list_txns_;
    set_txns_t set_txns_;

protected:
    const std::string strSgxWalletURL_;

public:
    static std::atomic_size_t g_nMaxPendingTxns;
    static std::string g_strDispatchQueueID;
    pending_ima_txns( const std::string& configPath, const std::string& strSgxWalletURL );
    pending_ima_txns( const pending_ima_txns& ) = delete;
    pending_ima_txns( pending_ima_txns&& ) = delete;
    virtual ~pending_ima_txns();
    pending_ima_txns& operator=( const pending_ima_txns& ) = delete;
    pending_ima_txns& operator=( pending_ima_txns&& ) = delete;
    //
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;

private:
    mutable mutex_type mtx_;

public:
    mutex_type& mtx() const { return mtx_; }
    //
    bool empty() const;
    virtual void clear();
    virtual size_t max_txns() const;

private:
    void adjust_limits_impl( bool isEnableBroadcast );

public:
    void adjust_limits( bool isEnableBroadcast );
    bool insert( txn_entry& txe, bool isEnableBroadcast );
    bool insert( dev::u256 hash, bool isEnableBroadcast );
    bool erase( txn_entry& txe, bool isEnableBroadcast );
    bool erase( dev::u256 hash, bool isEnableBroadcast );
    bool find( txn_entry& txe ) const;
    bool find( dev::u256 hash ) const;
    void list_all( list_txns_t& lst ) const;
    //
    virtual void on_txn_insert( const txn_entry& txe, bool isEnableBroadcast );
    virtual void on_txn_erase( const txn_entry& txe, bool isEnableBroadcast );

    bool broadcast_txn_sign_is_enabled( const std::string& strWalletURL );

private:
    std::string broadcast_txn_sign_string( const char* strToSign );
    std::string broadcast_txn_compose_string( const char* strActionName, const dev::u256& tx_hash );
    std::string broadcast_txn_sign( const char* strActionName, const dev::u256& tx_hash );
    std::string broadcast_txn_get_ecdsa_public_key( int node_id );
    int broadcast_txn_get_node_id();

public:
    bool broadcast_txn_verify_signature( const char* strActionName,
        const std::string& strBroadcastSignature, int node_id, const dev::u256& tx_hash );

public:
    virtual void broadcast_txn_insert( const txn_entry& txe );
    virtual void broadcast_txn_erase( const txn_entry& txe );

private:
    std::atomic_bool isTracking_ = false;
    skutils::dispatch::job_id_t tracking_job_id_;

public:
    static std::atomic_size_t g_nTrackingIntervalInSeconds;
    size_t tracking_interval_in_seconds() const;
    bool is_tracking() const;
    void tracking_auto_start_stop();
    void tracking_start();
    void tracking_stop();
    //
    bool check_txn_is_mined( const txn_entry& txe );
    bool check_txn_is_mined( dev::u256 hash );
};  /// class pending_ima_txns

};  // namespace tracking

namespace rpc {

/**
 * @brief JSON-RPC api implementation
 */
class SkaleStats : public dev::rpc::SkaleStatsFace,
                   public dev::rpc::SkaleStatsConsumerImpl,
                   public dev::tracking::pending_ima_txns {
    int nThisNodeIndex_ = -1;  // 1-based "schainIndex"
    int findThisNodeIndex();

    const dev::eth::ChainParams& chainParams_;

public:
    SkaleStats( const std::string& configPath, eth::Interface& _eth,
        const dev::eth::ChainParams& chainParams, bool isDisableZMQ );

    virtual RPCModules implementedModules() const override {
        return RPCModules{RPCModule{"skaleStats", "1.0"}};
    }

    bool isEnabledImaMessageSigning() const;

    virtual Json::Value skale_stats() override;
    virtual Json::Value skale_nodesRpcInfo() override;
    virtual Json::Value skale_imaInfo() override;
    virtual Json::Value skale_imaVerifyAndSign( const Json::Value& request ) override;
    virtual Json::Value skale_imaBSU256( const Json::Value& request ) override;
    virtual Json::Value skale_imaBroadcastTxnInsert( const Json::Value& request ) override;
    virtual Json::Value skale_imaBroadcastTxnErase( const Json::Value& request ) override;
    virtual Json::Value skale_imaTxnInsert( const Json::Value& request ) override;
    virtual Json::Value skale_imaTxnErase( const Json::Value& request ) override;
    virtual Json::Value skale_imaTxnClear( const Json::Value& request ) override;
    virtual Json::Value skale_imaTxnFind( const Json::Value& request ) override;
    virtual Json::Value skale_imaTxnListAll( const Json::Value& request ) override;

    virtual Json::Value skale_browseEntireNetwork( const Json::Value& request ) override;
    virtual Json::Value skale_cachedEntireNetwork( const Json::Value& request ) override;

protected:
    eth::Interface* client() const { return &m_eth; }
    eth::Interface& m_eth;
};

};  // namespace rpc
};  // namespace dev
