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
#include <libdevcore/CommonData.h>
#include <libethashseal/EthashClient.h>
#include <libethcore/CommonJS.h>
#include <libethereum/Client.h>
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

Eth::Eth( const std::string& configPath, eth::Interface& _eth, eth::AccountHolder& _ethAccounts )
    : skutils::json_config_file_accessor( configPath ),
      m_eth( _eth ),
      m_ethAccounts( _ethAccounts ) {}

bool Eth::isEnabledTransactionSending() const {
    bool isEnabled = true;
    try {
        nlohmann::json joConfig = getConfigJSON();
        if ( joConfig.count( "skaleConfig" ) == 0 )
            throw std::runtime_error( "error config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        if ( joSkaleConfig_nodeInfo.count( "syncNode" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"syncNode\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_syncNode = joSkaleConfig_nodeInfo["syncNode"];
        isEnabled = joSkaleConfig_nodeInfo_syncNode.get< bool >() ? false : true;
    } catch ( ... ) {
    }
    return isEnabled;
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

string Eth::eth_blockNumber() {
    return toJS( client()->number() );
}


string Eth::eth_getBalance( string const& _address, string const&
#ifndef  NO_ALETH_STATE
_blockNumber
#endif
) {
    try {
#ifndef  NO_ALETH_STATE
        if (_blockNumber != "latest" && _blockNumber != "pending") {
            return toJS( client()->alethStateBalanceAt(
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


string Eth::eth_getStorageAt(
    string const& _address, string const& _position, string const&
#ifndef  NO_ALETH_STATE
    _blockNumber
#endif
    ) {
    try {
#ifndef  NO_ALETH_STATE
        if (_blockNumber != "latest" && _blockNumber != "pending") {
            return toJS(
                toCompactBigEndian( client()->alethStateAt( jsToAddress( _address ),
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
#ifndef  NO_ALETH_STATE
_address
#endif
, string const&
#ifndef  NO_ALETH_STATE
_blockNumber
#endif
) {
    try {
#ifndef  NO_ALETH_STATE
                return toString(
                    client()->alethStateRootAt(jsToAddress(_address), jsToBlockNumber(_blockNumber)));
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
#ifndef  NO_ALETH_STATE
       _blockNumber
#endif
) {
    try {

#ifndef  NO_ALETH_STATE
        if (_blockNumber != "latest" && _blockNumber != "pending") {
            return toString(client()->alethStateCountAt(jsToAddress(_address), jsToBlockNumber(_blockNumber)));
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

        return toJS( client()->transactionCount( blockHash ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_getBlockTransactionCountByNumber( string const& _blockNumber ) {
    try {
        BlockNumber blockNumber = jsToBlockNumber( _blockNumber );
        if ( !client()->isKnown( blockNumber ) )
            return Json::Value( Json::nullValue );

        return toJS( client()->transactionCount( jsToBlockNumber( _blockNumber ) ) );
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
#ifndef  NO_ALETH_STATE
_blockNumber
#endif
) {
    try {
#ifndef  NO_ALETH_STATE

                if (_blockNumber != "latest" && _blockNumber != "pending") {
                    return toJS( client()->alethStateCodeAt(
                        jsToAddress( _address ), jsToBlockNumber( _blockNumber ) ) );
                } else {

                    return toJS( client()->codeAt( jsToAddress( _address ) ) );
                }
#else
        return toJS( client()->codeAt( jsToAddress( _address ) ) );
#endif
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
        if ( !isEnabledTransactionSending() )
            throw std::runtime_error( "transacton sending feature is disabled on this instance" );
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
        Transaction t( ts, ar.second );
        RLPStream s;
        t.streamRLP( s );
        return toJson( t, s.out() );
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
        return toJson(
            Transaction( jsToBytes( _rlp, OnFailed::Throw ), CheckTransaction::Everything ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

// TODO Catch exceptions for all calls other eth_-calls in outer scope!
/// skale
string Eth::eth_sendRawTransaction( std::string const& _rlp ) {
    if ( !isEnabledTransactionSending() )
        throw JsonRpcException( "transacton sending feature is disabled on this instance" );
    // Don't need to check the transaction signature (CheckTransaction::None) since it
    // will be checked as a part of transaction import
    Transaction t( jsToBytes( _rlp, OnFailed::Throw ), CheckTransaction::None );
    return toJS( client()->importTransaction( t ) );
}

string Eth::eth_call( TransactionSkeleton& t, string const&
_blockNumber
) {
    // TODO: We ignore block number in order to be compatible with Metamask (SKALE-430).
    // Remove this temporary fix.
    string blockNumber = "latest";
    setTransactionDefaults( t );

#ifndef NO_ALETH_STATE
    blockNumber = _blockNumber;
    auto bN = jsToBlockNumber(blockNumber);
    if ( !client()->isKnown( bN ) ) {
        throw std::logic_error( "Unknown block number:" + blockNumber);
    }
#endif


    ExecutionResult er =
        client()->call( t.from, t.value, t.to, t.data, t.gas, t.gasPrice,
#ifndef NO_ALETH_STATE
                        bN,
#endif
                        FudgeFactor::Lenient );

    std::string strRevertReason;
    if ( er.excepted == dev::eth::TransactionException::RevertInstruction ) {
        strRevertReason = skutils::eth::call_error_message_2_str( er.output );
        if ( strRevertReason.empty() )
            strRevertReason = "EVM revert instruction without description message";
        std::string strTx = t.toString();
        std::string strOut = cc::fatal( "Error message from eth_call():" ) + cc::error( " " ) +
                             cc::warn( strRevertReason ) + cc::error( ", with call arguments: " ) +
                             cc::j( strTx ) + cc::error( ", and using " ) +
                             cc::info( "blockNumber" ) + cc::error( "=" ) +
                             cc::bright( blockNumber );
        cerror << strOut;
        throw std::logic_error( strRevertReason );
    }

    return toJS( er.output );
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
            throw std::logic_error( strRevertReason );
        }
        return toJS( result.first );
    } catch ( std::logic_error& error ) {
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

        if ( _includeTransactions )
            return toJson( client()->blockInfo( h ), client()->blockDetails( h ),
                client()->uncleHashes( h ), client()->transactions( h ), client()->sealEngine() );
        else
            return toJson( client()->blockInfo( h ), client()->blockDetails( h ),
                client()->uncleHashes( h ), client()->transactionHashes( h ),
                client()->sealEngine() );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_getBlockByNumber( string const& _blockNumber, bool _includeTransactions ) {
    try {
        BlockNumber h = jsToBlockNumber( _blockNumber );
        if ( !client()->isKnown( h ) )
            return Json::Value( Json::nullValue );

        if ( _includeTransactions )
            return toJson( client()->blockInfo( h ), client()->blockDetails( h ),
                client()->uncleHashes( h ), client()->transactions( h ), client()->sealEngine() );
        else
            return toJson( client()->blockInfo( h ), client()->blockDetails( h ),
                client()->uncleHashes( h ), client()->transactionHashes( h ),
                client()->sealEngine() );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

Json::Value Eth::eth_getTransactionByHash( string const& _transactionHash ) {
    try {
        h256 h = jsToFixed< 32 >( _transactionHash );
        if ( !client()->isKnownTransaction( h ) )
            return Json::Value( Json::nullValue );

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
        if ( !client()->isKnownTransaction( bh, ti ) )
            return Json::Value( Json::nullValue );

        return toJson( client()->localisedTransaction( bh, ti ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( JsonRpcException( Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

LocalisedTransactionReceipt Eth::eth_getTransactionReceipt( string const& _transactionHash ) {
    h256 h = jsToFixed< 32 >( _transactionHash );
    if ( !client()->isKnownTransaction( h ) ) {
        throw std::invalid_argument( "Not known transaction" );
    }
    auto cli = client();
    auto rcp = cli->localisedTransactionReceipt( h );
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
        return toJson( client()->logs( toLogFilter( _json ) ) );
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
    dev::eth::SyncStatus sync = client()->syncStatus();
    if ( sync.state == SyncState::Idle || !sync.majorSyncing )
        return Json::Value( false );

    Json::Value info( Json::objectValue );
    info["startingBlock"] = sync.startBlockNumber;
    info["highestBlock"] = sync.highestBlockNumber;
    info["currentBlock"] = sync.currentBlockNumber;
    return info;
}

string Eth::eth_chainId() {
    return toJS( client()->chainId() );
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
