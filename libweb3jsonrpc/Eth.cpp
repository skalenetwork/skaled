/*
    Modifications Copyright (C) 2018-2019 SKALE Labs

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
/** @file Eth.cpp
 * @authors:
 *   Gav Wood <i@gavwood.com>
 *   Marek Kotewicz <marek@ethdev.com>
 * @date 2014
 */

#include "Eth.h"
#include "AccountHolder.h"
#include <jsonrpccpp/common/exception.h>
#include <libconsensus/utils/Time.h>
#include <libdevcore/CommonData.h>
#include <libethashseal/EthashClient.h>
#include <libethcore/CommonJS.h>
#include <libethereum/Client.h>
#include <libskale/SkipInvalidTransactionsPatch.h>
#include <libweb3jsonrpc/JsonHelper.h>

#include <csignal>
#include <exception>

#include <skutils/console_colors.h>
#include <skutils/eth_utils.h>

using namespace std;
using namespace jsonrpc;
using namespace dev;
using namespace eth;
using namespace dev::rpc;

const uint64_t MAX_CALL_CACHE_ENTRIES = 1024;
const uint64_t MAX_RECEIPT_CACHE_ENTRIES = 1024;
const u256 MAX_BLOCK_RANGE = 1024;

// Geth compatible error code for a revert
const uint64_t REVERT_RPC_ERROR_CODE = 3;

#ifdef HISTORIC_STATE

using namespace dev::rpc::_detail;

// TODO Check LatestBlock number - update
// Needs external locks to exchange read one to write one
void GappedTransactionIndexCache::ensureCached( BlockNumber _bn,
    std::shared_lock< std::shared_mutex >& _readLock,
    std::unique_lock< std::shared_mutex >& _writeLock ) const {
    if ( _bn != PendingBlock && _bn != LatestBlock && real2gappedCache.count( _bn ) )
        return;

    // change read lock for write lock
    // they both will be destroyed externally
    _readLock.unlock();
    _writeLock.lock();


    unsigned realBn = _bn;
    if ( _bn == LatestBlock )
        realBn = client.number();
    else if ( _bn == PendingBlock )
        realBn = client.number() + 1;

    if ( real2gappedCache.size() > cacheSize ) {
        throw std::runtime_error( "real2gappedCache.size() > cacheSize" );
    }

    if ( real2gappedCache.size() >= cacheSize ) {
        real2gappedCache.erase( real2gappedCache.begin() );
        gapped2realCache.erase( gapped2realCache.begin() );
    }

    // can be empty for absent blocks
    h256s transactions = client.transactionHashes( _bn );

    real2gappedCache[_bn] = vector< size_t >( transactions.size(), UNDEFINED );
    gapped2realCache[_bn] = vector< size_t >();

    u256 gasBefore = 0;
    for ( size_t realIndex = 0; realIndex < transactions.size(); ++realIndex ) {
        // find transaction gas usage
        const h256& th = transactions[realIndex];
        u256 gasAfter = client.transactionReceipt( th ).cumulativeGasUsed();
        u256 diff = gasAfter - gasBefore;
        gasBefore = gasAfter;

        pair< h256, unsigned > loc = client.transactionLocation( th );

        // ignore transactions with 0 gas usage OR different location
        if ( diff == 0 || client.numberFromHash( loc.first ) != realBn || loc.second != realIndex )
            continue;

        // cache it
        size_t gappedIndex = gapped2realCache[_bn].size();
        gapped2realCache[_bn].push_back( realIndex );
        real2gappedCache[_bn][realIndex] = gappedIndex;

    }  // for
}

#endif

Eth::Eth( const std::string& configPath, eth::Client& _eth, eth::AccountHolder& _ethAccounts )
    : skutils::json_config_file_accessor( configPath ),
      m_eth( _eth ),
      m_ethAccounts( _ethAccounts ),
      m_callCache( MAX_CALL_CACHE_ENTRIES ),
      m_receiptsCache( MAX_RECEIPT_CACHE_ENTRIES )
#ifdef HISTORIC_STATE
      ,
      m_gapCache( std::make_unique< GappedTransactionIndexCache >( 16, *client() ) )
#endif
{
}

string Eth::eth_protocolVersion() {
    return toJS( eth::c_protocolVersion );
}

string Eth::eth_coinbase() {
    return toJS( client()->author() );
}

string Eth::eth_hashrate() {
    try {
        return toJS( asEthashClient( client() )->hashrate() );
    } catch ( InvalidSealEngine& ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

bool Eth::eth_mining() {
    try {
        return asEthashClient( client() )->isMining();
    } catch ( InvalidSealEngine& ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

string Eth::eth_gasPrice() {
    return toJS( client()->gasBidPrice() );
}

Json::Value Eth::eth_accounts() {
    return toJson( m_ethAccounts.allAccounts() );
}

string Eth::eth_blockNumber( const Json::Value& request ) {
    if ( !request.empty() ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }

    return toJS( client()->number() );
}


string Eth::eth_getBalance( string const& _address, string const&
#ifdef HISTORIC_STATE
                                                        _blockNumber
#endif
) {
    try {
#ifdef HISTORIC_STATE
        if ( _blockNumber != "latest" && _blockNumber != "pending" ) {
            return toJS( client()->historicStateBalanceAt(
                jsToAddress( _address ), jsToBlockNumber( _blockNumber ) ) );
        } else {
            return toJS( client()->balanceAt( jsToAddress( _address ) ) );
        }
#else
        return toJS( client()->balanceAt( jsToAddress( _address ) ) );
#endif
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}


string Eth::eth_getStorageAt( string const& _address, string const& _position,
    string const&
#ifdef HISTORIC_STATE
        _blockNumber
#endif
) {
    try {
#ifdef HISTORIC_STATE
        if ( _blockNumber != "latest" && _blockNumber != "pending" ) {
            return toJS(
                toCompactBigEndian( client()->historicStateAt( jsToAddress( _address ),
                                        jsToU256( _position ), jsToBlockNumber( _blockNumber ) ),
                    32 ) );
        }
#endif
        return toJS( toCompactBigEndian(
            client()->stateAt( jsToAddress( _address ), jsToU256( _position ) ), 32 ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

string Eth::eth_getStorageRoot( string const&
#ifdef HISTORIC_STATE
                                    _address
#endif
    ,
    string const&
#ifdef HISTORIC_STATE
        _blockNumber
#endif
) {
    try {
#ifdef HISTORIC_STATE
        return toString( client()->historicStateRootAt(
            jsToAddress( _address ), jsToBlockNumber( _blockNumber ) ) );
#else
        throw std::logic_error( "Storage root is not exist in Skale state" );
#endif
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_pendingTransactions() {
    // Return list of transaction that being sent by local accounts
    Transactions ours;
    for ( Transaction const& pending : client()->pending() ) {
        // for ( Address const& account : m_ethAccounts.allAccounts() ) {
        //    if ( pending.sender() == account ) {
        ours.push_back( pending );
        //        break;
        //    }
        //}
    }

    return toJson( ours );
}
string Eth::eth_getTransactionCount( string const& _address, string const&
#ifdef HISTORIC_STATE
                                                                 _blockNumber
#endif
) {
    try {
#ifdef HISTORIC_STATE
        if ( _blockNumber != "latest" && _blockNumber != "pending" ) {
            return toString( client()->historicStateCountAt(
                jsToAddress( _address ), jsToBlockNumber( _blockNumber ) ) );
        }
#endif

        return toJS( client()->countAt( jsToAddress( _address ) ) );

    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_getBlockTransactionCountByHash( string const& _blockHash ) {
    try {
        h256 blockHash = jsToFixed< 32 >( _blockHash );
        if ( !client()->isKnown( blockHash ) )
            return Json::Value( Json::nullValue );

#ifdef HISTORIC_STATE
        BlockNumber bn = client()->numberFromHash( blockHash );
        if ( !SkipInvalidTransactionsPatch::hasPotentialInvalidTransactionsInBlock(
                 bn, client()->blockChain() ) )
#endif
            return toJS( client()->transactionCount( blockHash ) );
#ifdef HISTORIC_STATE
        return toJS( m_gapCache->gappedBlockTransactionCount( bn ) );
#endif
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_getBlockTransactionCountByNumber( string const& _blockNumber ) {
    try {
        BlockNumber blockNumber = jsToBlockNumber( _blockNumber );
        if ( !client()->isKnown( blockNumber ) )
            return Json::Value( Json::nullValue );

#ifdef HISTORIC_STATE
        BlockNumber bn = jsToBlockNumber( _blockNumber );
        if ( !SkipInvalidTransactionsPatch::hasPotentialInvalidTransactionsInBlock(
                 bn, client()->blockChain() ) )
#endif
            return toJS( client()->transactionCount( jsToBlockNumber( _blockNumber ) ) );
#ifdef HISTORIC_STATE
        return toJS( m_gapCache->gappedBlockTransactionCount( blockNumber ) );
#endif
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_getUncleCountByBlockHash( string const& _blockHash ) {
    try {
        h256 blockHash = jsToFixed< 32 >( _blockHash );
        if ( !client()->isKnown( blockHash ) )
            return Json::Value( Json::nullValue );

        return toJS( client()->uncleCount( blockHash ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_getUncleCountByBlockNumber( string const& _blockNumber ) {
    try {
        BlockNumber blockNumber = jsToBlockNumber( _blockNumber );
        if ( !client()->isKnown( blockNumber ) )
            return Json::Value( Json::nullValue );

        return toJS( client()->uncleCount( blockNumber ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

string Eth::eth_getCode( string const& _address, string const&
#ifdef HISTORIC_STATE
                                                     _blockNumber
#endif
) {
    try {
#ifdef HISTORIC_STATE
        if ( _blockNumber != "latest" && _blockNumber != "pending" ) {
            return toJS( client()->historicStateCodeAt(
                jsToAddress( _address ), jsToBlockNumber( _blockNumber ) ) );
        }
#endif
        return toJS( client()->codeAt( jsToAddress( _address ) ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

void Eth::setTransactionDefaults( TransactionSkeleton& _t ) {
    if ( !_t.from )
        _t.from = m_ethAccounts.defaultTransactAccount();
}

string Eth::eth_sendTransaction( Json::Value const& _json ) {
    try {
        TransactionSkeleton t = toTransactionSkeleton( _json );
        setTransactionDefaults( t );
        pair< bool, Secret > ar = m_ethAccounts.authenticate( t );
        if ( !ar.first ) {
            h256 txHash = client()->submitTransaction( t, ar.second );
            return toJS( txHash );
        } else {
            m_ethAccounts.queueTransaction( t );
            h256 emptyHash;
            return toJS( emptyHash );  // TODO: give back something more useful than an empty hash.
        }
    } catch ( Exception const& ) {
        throw JsonRpcException( exceptionToErrorMessage() );
    }
}

Json::Value Eth::eth_signTransaction( Json::Value const& _json ) {
    try {
        TransactionSkeleton ts = toTransactionSkeleton( _json );
        setTransactionDefaults( ts );
        ts = client()->populateTransactionWithDefaults( ts );
        pair< bool, Secret > ar = m_ethAccounts.authenticate( ts );
        Transaction t( ts, ar.second );  // always legacy, no prefix byte
        return toJson( t, t.toBytes() );
    } catch ( Exception const& ) {
        throw JsonRpcException( exceptionToErrorMessage() );
    }
}

Json::Value Eth::eth_subscribe( Json::Value const& /*_transaction*/ ) {
    try {
        throw JsonRpcException( "eth_subscribe() API is not supported yet over HTTP(S)" );
    } catch ( Exception const& ) {
        throw JsonRpcException( exceptionToErrorMessage() );
    }
}
Json::Value Eth::eth_unsubscribe( Json::Value const& /*_transaction*/ ) {
    try {
        throw JsonRpcException( "eth_unsubscribe() API is not supported yet" );
    } catch ( Exception const& ) {
        throw JsonRpcException( exceptionToErrorMessage() );
    }
}

Json::Value Eth::setSchainExitTime( Json::Value const& /*_transaction*/ ) {
    try {
        throw JsonRpcException( "setSchainExitTime() API is not supported yet over HTTP(S)" );
    } catch ( Exception const& ) {
        throw JsonRpcException( exceptionToErrorMessage() );
    }
}


Json::Value Eth::eth_inspectTransaction( std::string const& _rlp ) {
    try {
        return toJson( Transaction( jsToBytes( _rlp, OnFailed::Throw ),
            CheckTransaction::Everything, EIP1559TransactionsPatch::isEnabledInWorkingBlock() ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

// TODO Catch exceptions for all calls other eth_-calls in outer scope
/// skale
string Eth::eth_sendRawTransaction( std::string const& _rlp ) {
    // Don't need to check the transaction signature (CheckTransaction::None) since it
    // will be checked as a part of transaction import
    Transaction t( jsToBytes( _rlp, OnFailed::Throw ), CheckTransaction::None, false,
        EIP1559TransactionsPatch::isEnabledInWorkingBlock() );
    return toJS( client()->importTransaction( t ) );
}


string Eth::eth_call( TransactionSkeleton& t, string const&
#ifdef HISTORIC_STATE
                                                  _blockNumber
#endif
) {

    // not used: const uint64_t CALL_CACHE_ENTRY_LIFETIME_MS = 1000;

    // Remove this temporary fix.
    string blockNumber = "latest";

#ifdef HISTORIC_STATE
    blockNumber = _blockNumber;
#endif

    auto bN = jsToBlockNumber( blockNumber );

    // if an identical call has been made for the same block number
    // and the result is in cache, return the result from cache
    // note that lru_cache class is thread safe so there is no need to lock


    // Step 1 Look into the cache for the same request at the same block number
    // no need to lock since cache is synchronized internally
    string key;

    if ( bN == LatestBlock || bN == PendingBlock ) {
        bN = client()->number();
    }

    if ( !client()->isKnown( bN ) ) {
        throw std::logic_error( "Unknown block number:" + blockNumber );
    }


    key = t.toString().append( to_string( bN ) );

    auto result = m_callCache.getIfExists( key );
    if ( result.has_value() ) {
        // found an identical request in cache, return
        return any_cast< string >( result );
    }

    // Step 2. We got a cache miss. Execute the call now.
    setTransactionDefaults( t );

    ExecutionResult er = client()->call( t.from, t.value, t.to, t.data, t.gas, t.gasPrice,
#ifdef HISTORIC_STATE
        bN,
#endif
        FudgeFactor::Lenient );

    std::string strRevertReason;
    if ( er.excepted == dev::eth::TransactionException::RevertInstruction ) {
        strRevertReason = skutils::eth::call_error_message_2_str( er.output );
        if ( strRevertReason.empty() )
            strRevertReason = "EVM revert instruction without description message";

        if ( !er.output.empty() ) {
            Json::Value output = toJS( er.output );
            BOOST_THROW_EXCEPTION(
                JsonRpcException( REVERT_RPC_ERROR_CODE, strRevertReason, output ) );
        }

        throw std::logic_error( strRevertReason );
    }


    string callResult = toJS( er.output );

    // put the result into cache so it can be used by future calls
    m_callCache.put( key, callResult );

    return callResult;
}

string Eth::eth_estimateGas( Json::Value const& _json ) {
    try {
        TransactionSkeleton t = toTransactionSkeleton( _json );
        setTransactionDefaults( t );
        int64_t gas = static_cast< int64_t >( t.gas );
        auto result = client()->estimateGas( t.from, t.value, t.to, t.data, gas, t.gasPrice );

        std::string strRevertReason;
        if ( result.second.excepted == dev::eth::TransactionException::RevertInstruction ) {
            strRevertReason = skutils::eth::call_error_message_2_str( result.second.output );
            if ( strRevertReason.empty() )
                strRevertReason = "EVM revert instruction without description message";

            if ( !result.second.output.empty() ) {
                Json::Value output = toJS( result.second.output );
                BOOST_THROW_EXCEPTION(
                    JsonRpcException( REVERT_RPC_ERROR_CODE, strRevertReason, output ) );
            }

            throw std::logic_error( strRevertReason );
        }
        return toJS( result.first );
    } catch ( std::logic_error& error ) {
        throw error;
    } catch ( jsonrpc::JsonRpcException& error ) {
        throw error;
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

bool Eth::eth_flush() {
    client()->flushTransactions();
    return true;
}

Json::Value Eth::eth_getBlockByHash( string const& _blockHash, bool _includeTransactions ) {
    try {
        h256 h = jsToFixed< 32 >( _blockHash );
        if ( !client()->isKnown( h ) )
            return Json::Value( Json::nullValue );

        u256 baseFeePerGas;
        if ( EIP1559TransactionsPatch::isEnabledWhen(
                 client()->blockInfo( client()->numberFromHash( h ) - 1 ).timestamp() ) )
            try {
                baseFeePerGas = client()->gasBidPrice( client()->numberFromHash( h ) - 1 );
            } catch ( std::invalid_argument& _e ) {
                cdebug << "Cannot get gas price for block " << h;
                cdebug << _e.what();
                // set default gasPrice
                // probably the price was rotated out as we are asking the price for the old block
                baseFeePerGas = client()->gasBidPrice();
            }
        else
            baseFeePerGas = 0;

        if ( _includeTransactions ) {
            Transactions transactions = client()->transactions( h );

#ifdef HISTORIC_STATE
            BlockNumber bn = client()->numberFromHash( h );
            if ( SkipInvalidTransactionsPatch::hasPotentialInvalidTransactionsInBlock(
                     bn, client()->blockChain() ) ) {
                // remove invalid transactions
                size_t index = 0;
                Transactions::iterator newEnd = std::remove_if( transactions.begin(),
                    transactions.end(), [this, &index, bn]( const Transaction& ) -> bool {
                        return !m_gapCache->transactionPresent( bn, index++ );
                    } );
                transactions.erase( newEnd, transactions.end() );
            }
#endif
            return toJson( client()->blockInfo( h ), client()->blockDetails( h ),
                client()->uncleHashes( h ), transactions, client()->sealEngine(), baseFeePerGas );
        } else {
            h256s transactions = client()->transactionHashes( h );

#ifdef HISTORIC_STATE
            BlockNumber bn = client()->numberFromHash( h );
            if ( SkipInvalidTransactionsPatch::hasPotentialInvalidTransactionsInBlock(
                     bn, client()->blockChain() ) ) {
                // remove invalid transactions
                size_t index = 0;
                h256s::iterator newEnd = std::remove_if( transactions.begin(), transactions.end(),
                    [this, &index, bn]( const h256& ) -> bool {
                        return !m_gapCache->transactionPresent( bn, index++ );
                    } );
                transactions.erase( newEnd, transactions.end() );
            }
#endif
            return toJson( client()->blockInfo( h ), client()->blockDetails( h ),
                client()->uncleHashes( h ), transactions, client()->sealEngine(), baseFeePerGas );
        }
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_getBlockByNumber( string const& _blockNumber, bool _includeTransactions ) {
    try {
        BlockNumber h = jsToBlockNumber( _blockNumber );
        if ( !client()->isKnown( h ) )
            return Json::Value( Json::nullValue );

        BlockNumber bn = ( h == LatestBlock || h == PendingBlock ) ? client()->number() : h;

        u256 baseFeePerGas;
        if ( bn > 0 &&
             EIP1559TransactionsPatch::isEnabledWhen( client()->blockInfo( bn - 1 ).timestamp() ) )
            try {
                baseFeePerGas = client()->gasBidPrice( bn - 1 );
            } catch ( std::invalid_argument& _e ) {
                cdebug << "Cannot get gas price for block " << bn;
                cdebug << _e.what();
                // set default gasPrice
                // probably the price was rotated out as we are asking the price for the old block
                baseFeePerGas = client()->gasBidPrice();
            }
        else
            baseFeePerGas = 0;

#ifdef HISTORIC_STATE
        h256 bh = client()->hashFromNumber( h );
        return eth_getBlockByHash( "0x" + bh.hex(), _includeTransactions );
    } catch ( const JsonRpcException& ) {
        throw;
#else

        if ( _includeTransactions )
            return toJson( client()->blockInfo( h ), client()->blockDetails( h ),
                client()->uncleHashes( h ), client()->transactions( h ), client()->sealEngine(),
                baseFeePerGas );
        else
            return toJson( client()->blockInfo( h ), client()->blockDetails( h ),
                client()->uncleHashes( h ), client()->transactionHashes( h ),
                client()->sealEngine(), baseFeePerGas );
#endif
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_getTransactionByHash( string const& _transactionHash ) {
    try {
        h256 h = jsToFixed< 32 >( _transactionHash );
        if ( !client()->isKnownTransaction( h ) )
            return Json::Value( Json::nullValue );

#ifdef HISTORIC_STATE
        // skip invalid
        auto rcp = client()->localisedTransactionReceipt( h );
        if ( rcp.gasUsed() == 0 )
            return Json::Value( Json::nullValue );
#endif

        return toJson( client()->localisedTransaction( h ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_getTransactionByBlockHashAndIndex(
    string const& _blockHash, string const& _transactionIndex ) {
    try {
        h256 bh = jsToFixed< 32 >( _blockHash );
        unsigned int ti = static_cast< unsigned int >( jsToInt( _transactionIndex ) );

#ifdef HISTORIC_STATE
        BlockNumber bn = client()->numberFromHash( bh );
        if ( SkipInvalidTransactionsPatch::hasPotentialInvalidTransactionsInBlock(
                 bn, client()->blockChain() ) )
            try {
                ti = m_gapCache->realIndexFromGapped( bn, ti );
            } catch ( const out_of_range& ) {
                return Json::Value( Json::nullValue );
            }
#endif
        if ( !client()->isKnownTransaction( bh, ti ) )
            return Json::Value( Json::nullValue );

        return toJson( client()->localisedTransaction( bh, ti ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_getTransactionByBlockNumberAndIndex(
    string const& _blockNumber, string const& _transactionIndex ) {
    try {
        BlockNumber bn = jsToBlockNumber( _blockNumber );
        h256 bh = client()->hashFromNumber( bn );
        unsigned int ti = static_cast< unsigned int >( jsToInt( _transactionIndex ) );

#ifdef HISTORIC_STATE
        if ( SkipInvalidTransactionsPatch::hasPotentialInvalidTransactionsInBlock(
                 bn, client()->blockChain() ) )
            try {
                ti = m_gapCache->realIndexFromGapped( bn, ti );
            } catch ( const out_of_range& ) {
                return Json::Value( Json::nullValue );
            }
#endif

        if ( !client()->isKnownTransaction( bh, ti ) )
            return Json::Value( Json::nullValue );

        return toJson( client()->localisedTransaction( bh, ti ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

LocalisedTransactionReceipt Eth::eth_getTransactionReceipt( string const& _transactionHash ) {
    // Step 1. Check receipts cache transactions first. It is faster than
    // calling client()->isKnownTransaction()

    h256 h = jsToFixed< 32 >( _transactionHash );

    uint64_t currentBlockNumber = client()->number();
    string cacheKey = _transactionHash + toString( currentBlockNumber );

    // note that the cache object is thread safe, so we do not need locks
    auto result = m_receiptsCache.getIfExists( cacheKey );
    if ( result.has_value() ) {
        // we hit cache. This means someone already made this call for the same
        // block number
        auto receipt = any_cast< ptr< LocalisedTransactionReceipt > >( result );

        if ( receipt == nullptr ) {
            // no receipt yet at this block number
            throw std::invalid_argument( "Not known transaction" );
        } else {
            // we have receipt in the cache. Return it.
            return *receipt;
        }
    }


    // Step 2. We got cache miss. Do the work and put the result into the cache
    if ( !client()->isKnownTransaction( h ) ) {
        // transaction is not yet in the blockchain. Put null as receipt
        // into the cache
        m_receiptsCache.put( cacheKey, nullptr );
        throw std::invalid_argument( "Not known transaction" );
    }

    auto cli = client();
    auto rcp = cli->localisedTransactionReceipt( h );

#ifdef HISTORIC_STATE
    if ( SkipInvalidTransactionsPatch::hasPotentialInvalidTransactionsInBlock(
             rcp.blockNumber(), client()->blockChain() ) ) {
        // skip invalid
        if ( rcp.gasUsed() == 0 ) {
            m_receiptsCache.put( cacheKey, nullptr );
            throw std::invalid_argument( "Not known transaction" );
        }

        // substitute position, skipping invalid transactions
        size_t newIndex =
            m_gapCache->gappedIndexFromReal( rcp.blockNumber(), rcp.transactionIndex() );
        rcp = LocalisedTransactionReceipt( rcp, rcp.hash(), rcp.blockHash(), rcp.blockNumber(),
            newIndex, rcp.from(), rcp.to(), rcp.gasUsed(), rcp.contractAddress(), rcp.txType(),
            rcp.effectiveGasPrice() );
    }
#endif

    // got a receipt. Put it into the cache before returning
    // so that we have it if anyone asks again
    m_receiptsCache.put( cacheKey, make_shared< LocalisedTransactionReceipt >( rcp ) );

    return rcp;
}

Json::Value Eth::eth_getUncleByBlockHashAndIndex(
    string const& _blockHash, string const& _uncleIndex ) {
    try {
        return toJson( client()->uncle( jsToFixed< 32 >( _blockHash ),
                           static_cast< unsigned int >( jsToInt( _uncleIndex ) ) ),
            client()->sealEngine() );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_getUncleByBlockNumberAndIndex(
    string const& _blockNumber, string const& _uncleIndex ) {
    try {
        return toJson( client()->uncle( jsToBlockNumber( _blockNumber ),
                           static_cast< unsigned int >( jsToInt( _uncleIndex ) ) ),
            client()->sealEngine() );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

string Eth::eth_newFilter( Json::Value const& _json ) {
    try {
        return toJS( client()->installWatch( toLogFilter( _json ) ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

// string Eth::eth_newFilterEx( Json::Value const& _json ) {
//    try {
//        return toJS( client()->installWatch( toLogFilter( _json ) ) );
//    } catch ( ... ) {
//        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
//    }
//}

string Eth::eth_newBlockFilter() {
    h256 filter = dev::eth::ChainChangedFilter;
    return toJS( client()->installWatch( filter ) );
}

string Eth::eth_newPendingTransactionFilter() {
    h256 filter = dev::eth::PendingChangedFilter;
    return toJS( client()->installWatch( filter ) );
}

bool Eth::eth_uninstallFilter( string const& _filterId ) {
    try {
        return client()->uninstallWatch( static_cast< unsigned int >( jsToInt( _filterId ) ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_getFilterChanges( string const& _filterId ) {
    try {
        unsigned int id = static_cast< unsigned int >( jsToInt( _filterId ) );
        auto entries = client()->checkWatch( id );
        //		if (entries.size())
        //			cnote << "FIRING WATCH" << id << entries.size();
        return toJson( entries );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_getFilterChangesEx( string const& _filterId ) {
    try {
        unsigned int id = static_cast< unsigned int >( jsToInt( _filterId ) );
        auto entries = client()->checkWatch( id );
        //		if (entries.size())
        //			cnote << "FIRING WATCH" << id << entries.size();
        return toJsonByBlock( entries );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_getFilterLogs( string const& _filterId ) {
    try {
        return toJson( client()->logs( static_cast< unsigned int >( jsToInt( _filterId ) ) ) );
    } catch ( const TooBigResponse& ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS,
            "Log response size exceeded. Maximum allowed number of requested blocks is " +
                to_string( this->client()->chainParams().getLogsBlocksLimit ) ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

// Json::Value Eth::eth_getFilterLogsEx( string const& _filterId ) {
//    try {
//        return toJsonByBlock(
//            client()->logs( static_cast< unsigned int >( jsToInt( _filterId ) ) ) );
//    } catch ( ... ) {
//        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
//    }
//}

Json::Value Eth::eth_getLogs( Json::Value const& _json ) {
    try {
        LogFilter filter = toLogFilter( _json );
        if ( !_json["blockHash"].isNull() ) {
            if ( !_json["fromBlock"].isNull() || !_json["toBlock"].isNull() )
                BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS,
                    "fromBlock and toBlock are not allowed if blockHash is present" ) );
            string strHash = _json["blockHash"].asString();
            if ( strHash.empty() )
                throw std::invalid_argument( "blockHash cannot be an empty string" );
            uint64_t number = m_eth.numberFromHash( jsToFixed< 32 >( strHash ) );
            if ( number == PendingBlock )
                BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS,
                    "A block with this hash does not exist in the database. If this is an old "
                    "block, try connecting to an archive node" ) );
            filter.withEarliest( number );
            filter.withLatest( number );
        }
        return toJson( client()->logs( filter ) );
    } catch ( const TooBigResponse& ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS,
            "Log response size exceeded. Maximum allowed number of requested blocks is " +
                to_string( this->client()->chainParams().getLogsBlocksLimit ) ) );
    } catch ( const JsonRpcException& ) {
        throw;
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

// Json::Value Eth::eth_getLogsEx( Json::Value const& _json ) {
//    try {
//        return toJsonByBlock( client()->logs( toLogFilter( _json ) ) );
//    } catch ( ... ) {
//        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
//    }
//}

Json::Value Eth::eth_getWork() {
    try {
        Json::Value ret( Json::arrayValue );
        auto r = asEthashClient( client() )->getEthashWork();
        ret.append( toJS( get< 0 >( r ) ) );
        ret.append( toJS( get< 1 >( r ) ) );
        ret.append( toJS( get< 2 >( r ) ) );
        return ret;
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_syncing() {
    try {
        auto client = this->client();
        if ( !client )
            BOOST_THROW_EXCEPTION( std::runtime_error( "Client was not initialized" ) );

        // ask consensus whether the node is in catchup mode
        dev::eth::SyncStatus sync = client->syncStatus();
        if ( !sync.majorSyncing )
            return Json::Value( false );

        Json::Value info( Json::objectValue );
        info["startingBlock"] = sync.startBlockNumber;
        info["highestBlock"] = sync.highestBlockNumber;
        info["currentBlock"] = sync.currentBlockNumber;
        return info;
    } catch ( const Exception& e ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( e.what() ) );
    }
}

string Eth::eth_chainId() {
    return toJS( client()->chainId() );
}

// SKALE ignores gas costs
// make response default, only fill in gasUsed field
Json::Value Eth::eth_createAccessList(
    const Json::Value& _param1, const std::string& /*_param2*/ ) {
    TransactionSkeleton t = toTransactionSkeleton( _param1 );
    setTransactionDefaults( t );

    int64_t gas = static_cast< int64_t >( t.gas );
    auto executionResult = client()->estimateGas( t.from, t.value, t.to, t.data, gas, t.gasPrice );

    auto result = Json::Value( Json::objectValue );
    result["accessList"] = Json::Value( Json::arrayValue );
    result["gasUsed"] = toJS( executionResult.first );

    return result;
}

Json::Value Eth::eth_feeHistory( dev::u256 _blockCount, const std::string& _newestBlock,
    const Json::Value& _rewardPercentiles ) {
    try {
        if ( !_rewardPercentiles.isArray() )
            throw std::runtime_error( "Reward percentiles must be a list" );

        for ( auto p : _rewardPercentiles ) {
            if ( !p.isUInt() || p > 100 ) {
                throw std::runtime_error( "Percentiles must be positive integers less then 100" );
            }
        }

        if ( _blockCount > MAX_BLOCK_RANGE )
            throw std::runtime_error( "Max block range reached. Please try smaller blockCount." );

        auto newestBlock = jsToBlockNumber( _newestBlock );
        if ( newestBlock == dev::eth::LatestBlock )
            newestBlock = client()->number();

        auto result = Json::Value( Json::objectValue );
        dev::u256 oldestBlock;
        if ( _blockCount > newestBlock )
            oldestBlock = 1;
        else
            oldestBlock = dev::u256( newestBlock ) - _blockCount + 1;
        result["oldestBlock"] = toJS( oldestBlock );

        result["baseFeePerGas"] = Json::Value( Json::arrayValue );
        result["gasUsedRatio"] = Json::Value( Json::arrayValue );
        result["reward"] = Json::Value( Json::arrayValue );
        for ( auto bn = newestBlock; bn > oldestBlock - 1; --bn ) {
            auto blockInfo = client()->blockInfo( bn - 1 );

            if ( EIP1559TransactionsPatch::isEnabledWhen( blockInfo.timestamp() ) )
                result["baseFeePerGas"].append( toJS( client()->gasBidPrice( bn - 1 ) ) );
            else
                result["baseFeePerGas"].append( toJS( 0 ) );

            double gasUsedRatio = blockInfo.gasUsed().convert_to< double >() /
                                  blockInfo.gasLimit().convert_to< double >();
            Json::Value gasUsedRatioObj = Json::Value( Json::realValue );
            gasUsedRatioObj = gasUsedRatio;
            result["gasUsedRatio"].append( gasUsedRatioObj );

            Json::Value reward = Json::Value( Json::arrayValue );
            reward.resize( _rewardPercentiles.size() );
            for ( Json::Value::ArrayIndex i = 0; i < reward.size(); ++i ) {
                reward[i] = toJS( 0 );
            }
            result["reward"].append( reward );
        }

        return result;
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

std::string Eth::eth_maxPriorityFeePerGas() {
    return "0x0";
}

bool Eth::eth_submitWork( string const& _nonce, string const&, string const& _mixHash ) {
    try {
        return asEthashClient( client() )
            ->submitEthashWork( jsToFixed< 32 >( _mixHash ), jsToFixed< Nonce::size >( _nonce ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

bool Eth::eth_submitHashrate( string const& _hashes, string const& _id ) {
    try {
        asEthashClient( client() )
            ->submitExternalHashrate( jsToInt< 32 >( _hashes ), jsToFixed< 32 >( _id ) );
        return true;
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

string Eth::eth_register( string const& _address ) {
    try {
        return toJS( m_ethAccounts.addProxyAccount( jsToAddress( _address ) ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

bool Eth::eth_unregister( string const& _accountId ) {
    try {
        return m_ethAccounts.removeProxyAccount(
            static_cast< unsigned int >( jsToInt( _accountId ) ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_fetchQueuedTransactions( string const& _accountId ) {
    try {
        auto id = jsToInt( _accountId );
        Json::Value ret( Json::arrayValue );
        // TODO: throw an error on no account with given id
        for ( TransactionSkeleton const& t : m_ethAccounts.queuedTransactions( id ) )
            ret.append( toJson( t ) );
        m_ethAccounts.clearQueue( id );
        return ret;
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

string dev::rpc::exceptionToErrorMessage() {
    string ret;
    try {
        throw;
    }
    // Transaction submission exceptions
    catch ( ZeroSignatureTransaction const& ) {
        ret = "Zero signature transaction.";
    } catch ( GasPriceTooLow const& ) {
        ret = "Transaction gas price lower than current eth_gasPrice.";
    } catch ( SameNonceAlreadyInQueue const& ) {
        ret = "Pending transaction with same nonce already exists (skale: we ignore gas price).";
    } catch ( OutOfGasIntrinsic const& ) {
        ret =
            "Transaction gas amount is less than the intrinsic gas amount for this transaction "
            "type.";
    } catch ( BlockGasLimitReached const& ) {
        ret = "Block gas limit reached.";
    } catch ( InvalidNonce const& ) {
        ret = "Invalid transaction nonce.";
    } catch ( PendingTransactionAlreadyExists const& ) {
        ret = "Same transaction already exists in the pending transaction queue.";
    } catch ( TransactionAlreadyInChain const& ) {
        ret = "Transaction is already in the blockchain.";
    } catch ( NotEnoughCash const& ) {
        ret = "Account balance is too low (balance < value + gas * gas price).";
    } catch ( InvalidSignature const& ) {
        ret = "Invalid transaction signature.";
    }
    // Account holder exceptions
    catch ( AccountLocked const& ) {
        ret = "Account is locked.";
    } catch ( UnknownAccount const& ) {
        ret = "Unknown account.";
    } catch ( TransactionRefused const& ) {
        ret = "Transaction rejected by user.";
    } catch ( InvalidTransactionFormat const& ) {
        ret = "Invalid transaction format.";
    } catch ( ... ) {
        ret = "Invalid RPC parameters.";
    }
    return ret;
}
