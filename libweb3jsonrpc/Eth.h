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
/** @file Eth.h
 * @authors:
 *   Gav Wood <i@gavwood.com>
 *   Marek Kotewicz <marek@ethdev.com>
 * @date 2014
 */

#pragma once

#include "EthFace.h"
#include "SessionManager.h"
#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server.h>
#include <libdevcore/Common.h>
#include <iosfwd>
#include <memory>

#include <libconsensus/thirdparty/lrucache.hpp>

#include <skutils/utils.h>

namespace dev {
class NetworkFace;
class KeyPair;
namespace eth {
class AccountHolder;
struct TransactionSkeleton;
class Client;
class LocalisedTransactionReceipt;
}  // namespace eth
}  // namespace dev

namespace dev {

namespace rpc {

#ifdef HISTORIC_STATE
namespace _detail {
// cache for transaction index mapping
class GappedTransactionIndexCache {
public:
    GappedTransactionIndexCache( size_t _cacheSize, const dev::eth::Client& _client )
        : client( _client ), cacheSize( _cacheSize ) {
        assert( _cacheSize > 0 );
    }

    size_t realBlockTransactionCount( dev::eth::BlockNumber _bn ) const {
        std::shared_lock< std::shared_mutex > readLock( mtx );
        std::unique_lock< std::shared_mutex > writeLock( mtx, std::defer_lock );
        ensureCached( _bn, readLock, writeLock );

        return real2gappedCache[_bn].size();
    }
    size_t gappedBlockTransactionCount( dev::eth::BlockNumber _bn ) const {
        std::shared_lock< std::shared_mutex > readLock( mtx );
        std::unique_lock< std::shared_mutex > writeLock( mtx, std::defer_lock );
        ensureCached( _bn, readLock, writeLock );

        return gapped2realCache[_bn].size();
    }
    // can throw
    size_t realIndexFromGapped( dev::eth::BlockNumber _bn, size_t _gappedIndex ) const {
        std::shared_lock< std::shared_mutex > readLock( mtx );
        std::unique_lock< std::shared_mutex > writeLock( mtx, std::defer_lock );
        ensureCached( _bn, readLock, writeLock );

        // throws out_of_range!
        return gapped2realCache[_bn].at( _gappedIndex );
    }
    // can throw
    size_t gappedIndexFromReal( dev::eth::BlockNumber _bn, size_t _realIndex ) const {
        std::shared_lock< std::shared_mutex > readLock( mtx );
        std::unique_lock< std::shared_mutex > writeLock( mtx, std::defer_lock );
        ensureCached( _bn, readLock, writeLock );

        // throws out_of_range!
        size_t res = real2gappedCache[_bn].at( _realIndex );
        if ( res == UNDEFINED )
            throw std::out_of_range( "Transaction at index " + std::to_string( _realIndex ) +
                                     " in block " + to_string( _bn ) +
                                     " is invalid and should have been ignored!" );
        return res;
    }
    // can throw
    // TODO rename to valid
    bool transactionPresent( dev::eth::BlockNumber _bn, size_t _realIndex ) const {
        std::shared_lock< std::shared_mutex > readLock( mtx );
        std::unique_lock< std::shared_mutex > writeLock( mtx, std::defer_lock );
        ensureCached( _bn, readLock, writeLock );

        return real2gappedCache[_bn].at( _realIndex ) != UNDEFINED;
    }

private:
    void ensureCached( dev::eth::BlockNumber _bn, std::shared_lock< std::shared_mutex >& _readLock,
        std::unique_lock< std::shared_mutex >& _writeLock ) const;

private:
    mutable std::shared_mutex mtx;

    const dev::eth::Client& client;
    const size_t cacheSize;

    enum { UNDEFINED = ( size_t ) -1 };

    mutable std::map< dev::eth::BlockNumber, std::vector< size_t > > real2gappedCache;
    mutable std::map< dev::eth::BlockNumber, std::vector< size_t > > gapped2realCache;
};
}  // namespace _detail
#endif

// Should only be called within a catch block
std::string exceptionToErrorMessage();

/**
 * @brief JSON-RPC api implementation
 */
class Eth : public dev::rpc::EthFace, public skutils::json_config_file_accessor {
public:
    Eth( const std::string& configPath, eth::Client& _eth, eth::AccountHolder& _ethAccounts );

    virtual RPCModules implementedModules() const override {
        return RPCModules{ RPCModule{ "eth", "1.0" } };
    }

    bool isEnabledTransactionSending() const;

    eth::AccountHolder const& ethAccounts() const { return m_ethAccounts; }

    virtual std::string eth_protocolVersion() override;
    virtual std::string eth_hashrate() override;
    virtual std::string eth_coinbase() override;
    virtual bool eth_mining() override;
    virtual std::string eth_gasPrice() override;
    virtual Json::Value eth_accounts() override;
    virtual std::string eth_blockNumber( const Json::Value& request ) override;
    virtual std::string eth_getBalance(
        std::string const& _address, std::string const& _blockNumber ) override;
    virtual std::string eth_getStorageAt( std::string const& _address, std::string const& _position,
        std::string const& _blockNumber ) override;
    virtual std::string eth_getStorageRoot(
        std::string const& _address, std::string const& _blockNumber ) override;
    virtual std::string eth_getTransactionCount(
        std::string const& _address, std::string const& _blockNumber ) override;
    virtual Json::Value eth_pendingTransactions() override;
    virtual Json::Value eth_getBlockTransactionCountByHash(
        std::string const& _blockHash ) override;
    virtual Json::Value eth_getBlockTransactionCountByNumber(
        std::string const& _blockNumber ) override;
    virtual Json::Value eth_getUncleCountByBlockHash( std::string const& _blockHash ) override;
    virtual Json::Value eth_getUncleCountByBlockNumber( std::string const& _blockNumber ) override;
    virtual std::string eth_getCode(
        std::string const& _address, std::string const& _blockNumber ) override;
    virtual std::string eth_sendTransaction( Json::Value const& _json ) override;
    virtual std::string eth_call(
        eth::TransactionSkeleton& _t, std::string const& _blockNumber ) override;
    virtual std::string eth_estimateGas( Json::Value const& _json ) override;
    virtual bool eth_flush() override;
    virtual Json::Value eth_getBlockByHash(
        std::string const& _blockHash, bool _includeTransactions ) override;
    virtual Json::Value eth_getBlockByNumber(
        std::string const& _blockNumber, bool _includeTransactions ) override;
    virtual Json::Value eth_getTransactionByHash( std::string const& _transactionHash ) override;
    virtual Json::Value eth_getTransactionByBlockHashAndIndex(
        std::string const& _blockHash, std::string const& _transactionIndex ) override;
    virtual Json::Value eth_getTransactionByBlockNumberAndIndex(
        std::string const& _blockNumber, std::string const& _transactionIndex ) override;
    virtual eth::LocalisedTransactionReceipt eth_getTransactionReceipt(
        std::string const& _transactionHash ) override;
    virtual Json::Value eth_getUncleByBlockHashAndIndex(
        std::string const& _blockHash, std::string const& _uncleIndex ) override;
    virtual Json::Value eth_getUncleByBlockNumberAndIndex(
        std::string const& _blockNumber, std::string const& _uncleIndex ) override;
    virtual std::string eth_newFilter( Json::Value const& _json ) override;
    //    virtual std::string eth_newFilterEx( Json::Value const& _json ) override;
    virtual std::string eth_newBlockFilter() override;
    virtual std::string eth_newPendingTransactionFilter() override;
    virtual bool eth_uninstallFilter( std::string const& _filterId ) override;
    virtual Json::Value eth_getFilterChanges( std::string const& _filterId ) override;
    virtual Json::Value eth_getFilterChangesEx( std::string const& _filterId ) override;
    virtual Json::Value eth_getFilterLogs( std::string const& _filterId ) override;
    //    virtual Json::Value eth_getFilterLogsEx( std::string const& _filterId ) override;
    virtual Json::Value eth_getLogs( Json::Value const& _json ) override;
    //    virtual Json::Value eth_getLogsEx( Json::Value const& _json ) override;
    virtual Json::Value eth_getWork() override;
    virtual bool eth_submitWork(
        std::string const& _nonce, std::string const&, std::string const& _mixHash ) override;
    virtual bool eth_submitHashrate( std::string const& _hashes, std::string const& _id ) override;
    virtual std::string eth_register( std::string const& _address ) override;
    virtual bool eth_unregister( std::string const& _accountId ) override;
    virtual Json::Value eth_fetchQueuedTransactions( std::string const& _accountId ) override;
    virtual Json::Value eth_signTransaction( Json::Value const& _transaction ) override;
    virtual Json::Value eth_inspectTransaction( std::string const& _rlp ) override;
    virtual std::string eth_sendRawTransaction( std::string const& _rlp ) override;
    virtual bool eth_notePassword( std::string const& ) override { return false; }
    virtual Json::Value eth_syncing() override;
    virtual std::string eth_chainId() override;
    virtual Json::Value eth_subscribe( Json::Value const& _transaction ) override;
    virtual Json::Value eth_unsubscribe( Json::Value const& _transaction ) override;
    virtual Json::Value setSchainExitTime( Json::Value const& _transaction ) override;
    virtual Json::Value eth_createAccessList(
        const Json::Value& param1, const std::string& param2 ) override;
    virtual Json::Value eth_feeHistory(
        dev::u256 param1, const std::string& param2, const Json::Value& param3 ) override;
    virtual std::string eth_maxPriorityFeePerGas() override;

    void setTransactionDefaults( eth::TransactionSkeleton& _t );

protected:
    eth::Client* client() { return &m_eth; }

    eth::Client& m_eth;
    eth::AccountHolder& m_ethAccounts;

    // a cache that maps the call request to the pair of response string and block number
    // for which the request has been executed
    cache::lru_cache< string, string > m_callCache;

    // a cache that maps a transaction receipt to the block number where
    // the transaction was not yet ready
    // for which the request has been executed
    cache::lru_cache< string, ptr< dev::eth::LocalisedTransactionReceipt > > m_receiptsCache;

#ifdef HISTORIC_STATE
    std::unique_ptr< _detail::GappedTransactionIndexCache > m_gapCache;
#endif
};

}  // namespace rpc
}  // namespace dev
