
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
    if ( !m_enablePrivilegedApis ) {
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
#ifdef HISTORIC_STATE
    auto bN = jsToBlockNumber( _blockNumber );

    if ( bN == LatestBlock || bN == PendingBlock ) {
        bN = m_eth.number();
    }

    if ( !m_eth.isKnown( bN ) ) {
        BOOST_THROW_EXCEPTION(
            jsonrpc::JsonRpcException( "Unknown block number:" + _blockNumber ) );
    }

    if ( bN == 0 ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( "Block number must be more than zero" ) );
    }

    try {
        return m_eth.traceBlock( bN, _jsonTraceConfig );
    } catch ( Exception const& _e ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( _e.what() ) );
    }
#else
    BOOST_THROW_EXCEPTION(
        jsonrpc::JsonRpcException( "This API call is only supported on archive nodes" ) );
#endif
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

#ifdef HISTORIC_STATE
    h256 h = jsToFixed< 32 >( _blockHash );

    if ( !m_eth.isKnown( h ) ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( "Unknown block hash" ) );
    }

    BlockNumber bN = m_eth.numberFromHash( h );

    if ( bN == 0 ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( "Block number must be more than zero" ) );
    }

    try {
        return m_eth.traceBlock( bN, _jsonTraceConfig );
    } catch ( Exception const& _e ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( _e.what() ) );
    }
#else
    BOOST_THROW_EXCEPTION(
        jsonrpc::JsonRpcException( "This API call is only supported on archive nodes" ) );
#endif
}


Json::Value Debug::debug_traceTransaction( string const&
#ifdef HISTORIC_STATE
                                               _txHashStr
#endif
    ,
    Json::Value const&
#ifdef HISTORIC_STATE
        _jsonTraceConfig
#endif
) {

    checkHistoricStateEnabled();
#ifdef HISTORIC_STATE
    auto txHash = h256( _txHashStr );

    LocalisedTransaction localisedTransaction = m_eth.localisedTransaction( txHash );

    if ( localisedTransaction.blockHash() == h256( 0 ) ) {
        BOOST_THROW_EXCEPTION(
            jsonrpc::JsonRpcException( "no committed transaction with this hash" ) );
    }

    auto blockNumber = localisedTransaction.blockNumber();

    if ( !m_eth.isKnown( blockNumber ) ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( "Unknown block number" ) );
    }

    if ( blockNumber == 0 ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( "Block number must be more than zero" ) );
    }


    try {
        Json::Value tracedBlock;
        tracedBlock = m_eth.traceBlock( blockNumber, _jsonTraceConfig );
        STATE_CHECK( tracedBlock.isArray() )
        STATE_CHECK( !tracedBlock.empty() )


        string lowerCaseTxStr = _txHashStr;
        for ( auto& c : lowerCaseTxStr ) {
            c = std::tolower( static_cast< unsigned char >( c ) );
        }


        for ( Json::Value::ArrayIndex i = 0; i < tracedBlock.size(); i++ ) {
            Json::Value& transactionTrace = tracedBlock[i];
            STATE_CHECK( transactionTrace.isObject() );
            STATE_CHECK( transactionTrace.isMember( "txHash" ) );
            if ( transactionTrace["txHash"] == lowerCaseTxStr ) {
                STATE_CHECK( transactionTrace.isMember( "result" ) );
                return transactionTrace["result"];
            }
        }

        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( "No transaction in block" ) );

    } catch ( Exception const& _e ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( _e.what() ) );
    }
#else
    BOOST_THROW_EXCEPTION(
        jsonrpc::JsonRpcException( "This API call is only supported on archive nodes" ) );
#endif
}

Json::Value Debug::debug_traceCall( Json::Value const&
#ifdef HISTORIC_STATE
                                        _call
#endif
    ,
    std::string const&
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

#ifdef HISTORIC_STATE

    try {
        auto bN = jsToBlockNumber( _blockNumber );

        if ( bN == LatestBlock || bN == PendingBlock ) {
            bN = m_eth.number();
        }

        if ( !m_eth.isKnown( bN ) ) {
            BOOST_THROW_EXCEPTION(
                jsonrpc::JsonRpcException( "Unknown block number:" + _blockNumber ) );
        }

        if ( bN == 0 ) {
            BOOST_THROW_EXCEPTION(
                jsonrpc::JsonRpcException( "Block number must be more than zero" ) );
        }

        TransactionSkeleton ts = toTransactionSkeleton( _call );

        return m_eth.traceCall(
            ts.from, ts.value, ts.to, ts.data, ts.gas, ts.gasPrice, bN, _jsonTraceConfig );
    } catch ( Exception const& _e ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( _e.what() ) );
    }

#else
    BOOST_THROW_EXCEPTION(
        jsonrpc::JsonRpcException( "This API call is only supported on archive nodes" ) );
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
    checkPrivilegedAccess();
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
    checkPrivilegedAccess();
    return m_eth.chainParams().sChain.name;
}

uint64_t Debug::debug_getSnapshotCalculationTime() {
    checkPrivilegedAccess();
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

Json::Value Debug::debug_getFutureTransactions() {
    try {
        checkPrivilegedAccess();
        auto res = toJson( m_eth.debugGetFutureTransactions() );
        for ( auto& t : res )
            t.removeMember( "data" );
        return res;
    } catch ( std::exception const& _e ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( _e.what() ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException( "Unknown error" ) );
    }  // catch
}
