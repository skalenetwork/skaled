#ifndef HISTORIC_STATE
#define HISTORIC_STATE
#endif

#include "Debug.h"
#include "JsonHelper.h"


#include <libskale/SkaleDebug.h>

#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/CommonJS.h>
#include <libethcore/CommonJS.h>
#include <libethereum/Client.h>
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
    if ( m_enablePrivilegedApis ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( "This API call is not enabled" ) );
    }
}

void Debug::checkHistoricStateEnabled() const {
#ifndef HISTORIC_STATE
    BOOST_THROW_EXCEPTION(
        jsonrpc::JsonRpcException( "This API call is available on archive nodes only" ) );
#endif
}

Debug::Debug( eth::Client& _eth, SkaleDebugInterface* _debugInterface, const string& argv,
    bool _enablePrivilegedApis )
    : m_eth( _eth ),
      m_debugInterface( _debugInterface ),
      m_argvOptions( argv ),
      m_blockTraceCache( MAX_BLOCK_TRACES_CACHE_ITEMS, MAX_BLOCK_TRACES_CACHE_SIZE ),
      m_enablePrivilegedApis( _enablePrivilegedApis ) {}


h256 Debug::blockHash( string const& _blockNumberOrHash ) const {
    checkPrivilegedAccess();
    if ( isHash< h256 >( _blockNumberOrHash ) )
        return h256( _blockNumberOrHash.substr( _blockNumberOrHash.size() - 64, 64 ) );
    try {
        return m_eth.blockChain().numberHash( stoul( _blockNumberOrHash ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( "Invalid argument" ) );
    }
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
        BOOST_THROW_EXCEPTION(
            jsonrpc::JsonRpcException( "Unknown block number:" + _blockNumber ) );
    }

    try {
        return m_eth.traceBlock( bN, _jsonTraceConfig );
    } catch ( Exception const& _e ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( _e.what() ) );
    }
}

Json::Value Debug::debug_traceBlockByHash( string const&
#ifdef HISTORIC_STATE
                                               _blockHash
#endif
    ,
    Json::Value const&
#ifdef HISTORIC_STATE
        _jsonTraceConfig
#endif
) {
    checkHistoricStateEnabled();

    h256 h = jsToFixed< 32 >( _blockHash );

    if ( !m_eth.isKnown( h ) ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( "Unknown block hash" ) );
    }

    BlockNumber bN = m_eth.numberFromHash( h );

    try {
        return m_eth.traceBlock( bN, _jsonTraceConfig );
    } catch ( Exception const& _e ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( _e.what() ) );
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
        BOOST_THROW_EXCEPTION(
            jsonrpc::JsonRpcException( "no committed transaction with this hash" ) );
    }

    auto blockNumber = t.blockNumber();

    if ( !m_eth.isKnown( blockNumber ) ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( "Unknown block number" ) );
    }


    auto tracer = make_shared< AlethStandardTrace >( t, _jsonTraceConfig );

    try {
        return m_eth.trace( t, blockNumber - 1, tracer );
    } catch ( Exception const& _e ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( _e.what() ) );
    }
#endif
}


Json::Value Debug::debug_accountRangeAt( string const&, int, string const&, int ) {
    BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( "This API call is not supported" ) );
}

Json::Value Debug::debug_storageRangeAt( string const&, int, string const&, string const&, int ) {
    BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( "This API call is not supported" ) );
}

string Debug::debug_preimage( string const& ) {
    BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( "This API call is not supported" ) );
}

Json::Value Debug::debug_traceCall( Json::Value const&, Json::Value const& ) {
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

void Debug::debug_forceBroadcast( const string& _transactionHash ) {
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

string Debug::debug_interfaceCall( const string& _arg ) {
    checkPrivilegedAccess();
    return m_debugInterface->call( _arg );
}

string Debug::debug_getVersion() {
    return Version;
}

string Debug::debug_getArguments() {
    checkPrivilegedAccess();
    return m_argvOptions;
}

string Debug::debug_getConfig() {
    checkPrivilegedAccess();
    return m_eth.chainParams().getOriginalJson();
}

string Debug::debug_getSchainName() {
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
