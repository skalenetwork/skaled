#ifndef HISTORIC_STATE
#define HISTORIC_STATE
#endif

#include "Debug.h"
#include "JsonHelper.h"

#include <libethereum/SkaleHost.h>
#include <libskale/SkaleDebug.h>

#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/CommonJS.h>
#include <libethcore/CommonJS.h>
#include <libethereum/Client.h>
#include <libethereum/Executive.h>
#include <skutils/eth_utils.h>

#ifdef HISTORIC_STATE
#include <libhistoric/AlethExecutive.h>
#include <libhistoric/AlethStandardTrace.h>
#endif

using namespace std;
using namespace dev;
using namespace dev::rpc;
using namespace dev::eth;
using namespace skale;

void Debug::checkPrivilegedAccess() const {
    if ( enablePrivilegedApis ) {
        throw jsonrpc::JsonRpcException( "This API call is not enabled" );
    }
}

void Debug::checkHistoricStateEnabled() const {
#ifndef HISTORIC_STATE
    throw jsonrpc::JsonRpcException( "This API call is available on archive nodes only" );
#endif
}

Debug::Debug( eth::Client& _eth, SkaleDebugInterface* _debugInterface, const string& argv,
    bool _enablePrivilegedApis )
    : m_eth( _eth ),
      m_debugInterface( _debugInterface ),
      argv_options( argv ),
      enablePrivilegedApis( _enablePrivilegedApis ) {}


h256 Debug::blockHash( string const& _blockNumberOrHash ) const {
    checkPrivilegedAccess();
    if ( isHash< h256 >( _blockNumberOrHash ) )
        return h256( _blockNumberOrHash.substr( _blockNumberOrHash.size() - 64, 64 ) );
    try {
        return m_eth.blockChain().numberHash( stoul( _blockNumberOrHash ) );
    } catch ( ... ) {
        throw jsonrpc::JsonRpcException( "Invalid argument" );
    }
}

State Debug::stateAt( std::string const& /*_blockHashOrNumber*/, int _txIndex ) const {
    checkPrivilegedAccess();
    if ( _txIndex < 0 )
        throw jsonrpc::JsonRpcException( "Negative index" );

    throw logic_error( "State at is not supported in Skale state" );
}


Json::Value Debug::debug_traceBlockByNumber( const string&
#ifdef HISTORIC_STATE
                                                 _blockNumber
#endif
    ,
    Json::Value const&
#ifdef HISTORIC_STATE
        _jsonTraceConfig
#endif
    ) {
    Json::Value ret;
    checkHistoricStateEnabled();
    auto bN = jsToBlockNumber( _blockNumber );

    if ( bN == LatestBlock || bN == PendingBlock ) {
        bN = m_eth.number();
    }

    if ( !m_eth.isKnown( bN ) ) {
        throw std::logic_error( "Unknown block number:" + _blockNumber );
    }

    try {
        return m_eth.traceBlock( bN, _jsonTraceConfig);
    } catch ( Exception const& _e ) {
        throw jsonrpc::JsonRpcException( _e.what() );
    }
}


Json::Value Debug::debug_traceTransaction( string const&
#ifdef HISTORIC_STATE
                                               _txHash
#endif
    ,
    Json::Value const&
#ifdef HISTORIC_STATE
        _jsonTraceConfig
#endif
) {

    checkHistoricStateEnabled();

#ifdef HISTORIC_STATE
    LocalisedTransaction t = m_eth.localisedTransaction( h256( _txHash ) );


    if ( t.blockHash() == h256( 0 ) ) {
        throw jsonrpc::JsonRpcException( "no committed transaction with this hash" );
    }

    auto blockNumber = t.blockNumber();

    if ( !m_eth.isKnown( blockNumber ) ) {
        throw jsonrpc::JsonRpcException( "Unknown block number" );
    }


    auto tracer = std::make_shared< AlethStandardTrace >( t, _jsonTraceConfig );

    try {
        return m_eth.trace( t, blockNumber - 1, tracer );
    } catch ( Exception const& _e ) {
        throw jsonrpc::JsonRpcException( _e.what() );
    }
#endif
}


Json::Value Debug::debug_traceBlockByHash(
    string const& /*_blockHash*/, Json::Value const& _json ) {
    checkHistoricStateEnabled();
    Json::Value ret;
    Block block = m_eth.latestBlock();
    return ret;
}


Json::Value Debug::debug_accountRangeAt( string const& _blockHashOrNumber, int _txIndex,
    string const& /*_addressHash*/, int _maxResults ) {
    throw jsonrpc::JsonRpcException( "This API call is not supported" );
}

Json::Value Debug::debug_storageRangeAt( string const& _blockHashOrNumber, int _txIndex,
    string const& /*_address*/, string const& /*_begin*/, int _maxResults ) {
    throw jsonrpc::JsonRpcException( "This API call is not supported" );
}

std::string Debug::debug_preimage( std::string const& /*_hashedKey*/ ) {
    throw jsonrpc::JsonRpcException( "This API call is not supported" );
}

Json::Value Debug::debug_traceCall( Json::Value const& _call, Json::Value const& _options ) {
    Json::Value ret;
    /*
    try {
        Block temp = m_eth.latestBlock();
        TransactionSkeleton ts = toTransactionSkeleton( _call );
        if ( !ts.from ) {
            ts.from = Address();
        }
        u256 nonce = temp.transactionsFrom( ts.from );
        u256 gas = ts.gas == Invalid256 ? m_eth.gasLimitRemaining() : ts.gas;
        u256 gasPrice = ts.gasPrice == Invalid256 ? m_eth.gasBidPrice() : ts.gasPrice;
        temp.mutableState().addBalance( ts.from, gas * gasPrice + ts.value );
        Transaction transaction( ts.value, gasPrice, gas, ts.to, ts.data, nonce );
        transaction.forceSender( ts.from );
        eth::ExecutionResult er;
        // HACK 0 here is for gasPrice
        Executive e( temp, m_eth.blockChain().lastBlockHashes(), 0 );
        e.setResultRecipient( er );
        Json::Value trace = traceTransaction( e, transaction, _options );
        ret["gas"] = toJS( transaction.gas() );
        ret["return"] = toHexPrefixed( er.output );
        ret["structLogs"] = trace;
    } catch ( Exception const& _e ) {
        cwarn << diagnostic_information( _e );
    }
     */
    return ret;
}

void Debug::debug_pauseBroadcast( bool _pause ) {
    checkPrivilegedAccess();
    m_eth.skaleHost()->pauseBroadcast( _pause );
}
void Debug::debug_pauseConsensus( bool _pause ) {
    checkPrivilegedAccess();
    m_eth.skaleHost()->pauseConsensus( _pause );
}
void Debug::debug_forceBlock() {
    checkPrivilegedAccess();
    m_eth.skaleHost()->forceEmptyBlock();
}

void Debug::debug_forceBroadcast( const std::string& _transactionHash ) {
    checkPrivilegedAccess();
    try {
        h256 h = jsToFixed< 32 >( _transactionHash );
        if ( !m_eth.isKnownTransaction( h ) )
            BOOST_THROW_EXCEPTION(
                jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS ) );

        const Transaction tx = m_eth.transaction( h );
        m_eth.skaleHost()->forcedBroadcast( tx );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION(
            jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS ) );
    }
}

std::string Debug::debug_interfaceCall( const std::string& _arg ) {
    checkPrivilegedAccess();
    return m_debugInterface->call( _arg );
}

std::string Debug::debug_getVersion() {
    return Version;
}

std::string Debug::debug_getArguments() {
    checkPrivilegedAccess();
    return argv_options;
}

std::string Debug::debug_getConfig() {
    checkPrivilegedAccess();
    return m_eth.chainParams().getOriginalJson();
}

std::string Debug::debug_getSchainName() {
    return m_eth.chainParams().sChain.name;
}

uint64_t Debug::debug_getSnapshotCalculationTime() {
    return m_eth.getSnapshotCalculationTime();
}

uint64_t Debug::debug_getSnapshotHashCalculationTime() {
    return m_eth.getSnapshotHashCalculationTime();
}

uint64_t Debug::debug_doStateDbCompaction() {
    checkPrivilegedAccess();
    auto t1 = boost::chrono::high_resolution_clock::now();
    m_eth.doStateDbCompaction();
    auto t2 = boost::chrono::high_resolution_clock::now();

    return boost::chrono::duration_cast< boost::chrono::milliseconds >( t2 - t1 ).count();
}

uint64_t Debug::debug_doBlocksDbCompaction() {
    checkPrivilegedAccess();
    auto t1 = boost::chrono::high_resolution_clock::now();
    m_eth.doBlocksDbCompaction();
    auto t2 = boost::chrono::high_resolution_clock::now();

    return boost::chrono::duration_cast< boost::chrono::milliseconds >( t2 - t1 ).count();
}
