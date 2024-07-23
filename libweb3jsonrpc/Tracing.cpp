#include "Tracing.h"

#include <libethcore/CommonJS.h>
#include <libethereum/Client.h>
#include <libweb3jsonrpc/JsonHelper.h>

#ifdef HISTORIC_STATE

#include <libhistoric/AlethExecutive.h>
#include <libhistoric/AlethStandardTrace.h>
#endif

using namespace std;
using namespace dev;
using namespace dev::rpc;
using namespace dev::eth;


#define THROW_TRACE_JSON_EXCEPTION( __MSG__ )                            \
    throw jsonrpc::JsonRpcException( std::string( __FUNCTION__ ) + ":" + \
                                     std::to_string( __LINE__ ) + ":" + std::string( __MSG__ ) )

void Tracing::checkHistoricStateEnabled() const {
#ifndef HISTORIC_STATE
    BOOST_THROW_EXCEPTION(
        jsonrpc::JsonRpcException( "This API call is available on archive nodes only" ) );
#endif
}

Tracing::Tracing( eth::Client& _eth, const string& argv )
    : m_eth( _eth ),
      m_argvOptions( argv ),
      m_blockTraceCache( MAX_BLOCK_TRACES_CACHE_ITEMS, MAX_BLOCK_TRACES_CACHE_SIZE ) {}

h256 Tracing::blockHash( string const& _blockNumberOrHash ) const {
    if ( isHash< h256 >( _blockNumberOrHash ) )
        return h256( _blockNumberOrHash.substr( _blockNumberOrHash.size() - 64, 64 ) );
    try {
        return m_eth.blockChain().numberHash( stoul( _blockNumberOrHash ) );
    } catch ( ... ) {
        THROW_TRACE_JSON_EXCEPTION( "Invalid argument" );
    }
}

Json::Value Tracing::tracing_traceBlockByNumber( const string&
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
        THROW_TRACE_JSON_EXCEPTION( "Unknown block number:" + _blockNumber );
    }

    if ( bN == 0 ) {
        THROW_TRACE_JSON_EXCEPTION( "Block number must be more than zero" );
    }

    try {
        return m_eth.traceBlock( bN, _jsonTraceConfig );
    } catch ( std::exception const& _e ) {
        THROW_TRACE_JSON_EXCEPTION( _e.what() );
    } catch ( ... ) {
        THROW_TRACE_JSON_EXCEPTION( "Unknown server error" );
    }
#else
    THROW_TRACE_JSON_EXCEPTION( "This API call is only supported on archive nodes" );
#endif
}

Json::Value Tracing::tracing_traceBlockByHash( string const&
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
        THROW_TRACE_JSON_EXCEPTION( "Unknown block hash" + _blockHash );
    }

    BlockNumber bN = m_eth.numberFromHash( h );

    if ( bN == 0 ) {
        THROW_TRACE_JSON_EXCEPTION( "Block number must be more than zero" );
    }

    try {
        return m_eth.traceBlock( bN, _jsonTraceConfig );
    } catch ( std::exception const& _e ) {
        THROW_TRACE_JSON_EXCEPTION( _e.what() );
    } catch ( ... ) {
        THROW_TRACE_JSON_EXCEPTION( "Unknown server error" );
    }
#else
    THROW_TRACE_JSON_EXCEPTION( "This API call is only supported on archive nodes" );
#endif
}


Json::Value Tracing::tracing_traceTransaction( string const&
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
        THROW_TRACE_JSON_EXCEPTION(
            "Can't find committed transaction with this hash:" + _txHashStr );
    }

    auto blockNumber = localisedTransaction.blockNumber();


    if ( !m_eth.isKnown( blockNumber ) ) {
        THROW_TRACE_JSON_EXCEPTION( "Unknown block number:" + to_string( blockNumber ) );
    }

    if ( blockNumber == 0 ) {
        THROW_TRACE_JSON_EXCEPTION( "Block number must be more than zero" );
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

        THROW_TRACE_JSON_EXCEPTION( "Transaction not found in block" );

    } catch ( jsonrpc::JsonRpcException& ) {
        throw;
    } catch ( std::exception const& _e ) {
        THROW_TRACE_JSON_EXCEPTION( _e.what() );
    } catch ( ... ) {
        THROW_TRACE_JSON_EXCEPTION( "Unknown server error" );
    }
#else
    BOOST_THROW_EXCEPTION(
        jsonrpc::JsonRpcException( "This API call is only supported on archive nodes" ) );
#endif
}

Json::Value Tracing::tracing_traceCall( Json::Value const&
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
            THROW_TRACE_JSON_EXCEPTION( "Unknown block number:" + _blockNumber );
        }

        if ( bN == 0 ) {
            THROW_TRACE_JSON_EXCEPTION( "Block number must be more than zero" );
        }

        TransactionSkeleton ts = toTransactionSkeleton( _call );

        return m_eth.traceCall(
            ts.from, ts.value, ts.to, ts.data, ts.gas, ts.gasPrice, bN, _jsonTraceConfig );
    } catch ( jsonrpc::JsonRpcException& ) {
        throw;
    } catch ( std::exception const& _e ) {
        THROW_TRACE_JSON_EXCEPTION( _e.what() );
    } catch ( ... ) {
        THROW_TRACE_JSON_EXCEPTION( "Unknown server error" );
    }

#else
    BOOST_THROW_EXCEPTION(
        jsonrpc::JsonRpcException( "This API call is only supported on archive nodes" ) );
#endif
}
