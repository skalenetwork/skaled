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

    //    Block block = m_eth.block(blockHash(_blockHashOrNumber));
    //    auto const txCount = block.pending().size();

    //    State state(State::Null);
    //    if (static_cast<size_t>(_txIndex) < txCount)
    //        createIntermediateState(state, block, _txIndex, m_eth.blockChain());
    //    else if (static_cast<size_t>(_txIndex) == txCount)
    //        // the final state of block (after applying rewards)
    //        state = block.state();
    //    else
    //        throw jsonrpc::JsonRpcException("Transaction index " + toString(_txIndex) +
    //                                        " out of range for block " + _blockHashOrNumber);

    //    return state;
}

AlethStandardTrace::DebugOptions dev::eth::debugOptions( Json::Value const& _json ) {
    AlethStandardTrace::DebugOptions op;
    if ( !_json.isObject() || _json.empty() )
        return op;
    if ( !_json["disableStorage"].empty() )
        op.disableStorage = _json["disableStorage"].asBool();
    if ( !_json["enableMemory"].empty() )
        op.enableMemory = _json["enableMemory"].asBool();
    if ( !_json["disableStack"].empty() )
        op.disableStack = _json["disableStack"].asBool();
    if ( !_json["enableReturnData"].empty() )
        op.enableReturnData = _json["enableReturnData"].asBool();
    return op;
}


Json::Value Debug::traceBlock( Block const& _block, Json::Value const& _json ) {
    State s( _block.state() );
    //    s.setRoot(_block.stateRootBeforeTx(0));

    Json::Value traces( Json::arrayValue );
    /*
    for ( unsigned k = 0; k < _block.pending().size(); k++ ) {
        Transaction t = _block.pending()[k];

        u256 const gasUsed = k ? _block.receipt( k - 1 ).cumulativeGasUsed() : 0;
        auto const& bc = m_eth.blockChain();
        EnvInfo envInfo(
            _block.info(), m_eth.blockChain().lastBlockHashes(), gasUsed, bc.chainID() );
        // HACK 0 here is for gasPrice
        Executive e( s, envInfo, *m_eth.blockChain().sealEngine(), 0 );

        eth::ExecutionResult er;
        e.setResultRecipient( er );
        traces.append( traceTransaction( e, t, _json ) );
    }
     */
    return traces;
}


Json::Value Debug::debug_traceTransaction( string const&
#ifdef HISTORIC_STATE
                                               _txHash
#endif
    ,
    Json::Value const&
#ifdef HISTORIC_STATE
        _json
#endif
) {
    Json::Value ret;

    checkHistoricStateEnabled();

#ifdef HISTORIC_STATE
    LocalisedTransaction t = m_eth.localisedTransaction( h256( _txHash ) );


    if ( t.blockHash() == h256( 0 ) ) {
        throw jsonrpc::JsonRpcException( "no committed transaction with this hash" );
    }

    auto blockNumber = t.blockNumber();

    if ( !m_eth.isKnown( blockNumber ) ) {
        throw jsonrpc::JsonRpcException( "Unknown block number" );
        ;
    }

    Json::Value result;
    auto tracer = std::make_shared< AlethStandardTrace >( result );
    tracer->setShowMnemonics();
    auto options = debugOptions( _json );
    tracer->setOptions( options );


    try {
        ExecutionResult er = m_eth.trace( t, blockNumber - 1, tracer );
        ret["gas"] = ( uint64_t ) er.gasUsed;

        ret["structLogs"] = result;
        if ( er.excepted == TransactionException::None ) {
            ret["failed"] = false;
            if ( options.enableReturnData ) {
                ret["returnValue"] = toHex( er.output );
            }
        } else {
            ret["failed"] = true;
            if ( er.excepted == TransactionException::RevertInstruction ) {
                ret["returnValue"] = skutils::eth::call_error_message_2_str( er.output );
                ret["error"] = skutils::eth::call_error_message_2_str( er.output );
            } else {
                ret["returnValue"] = "";
                ret["error"] = "";
            }
        }
    } catch ( Exception const& _e ) {
        throw jsonrpc::JsonRpcException( _e.what() );
    }
    return ret;
#endif
}


Json::Value Debug::debug_traceBlock( string const& _blockRLP, Json::Value const& _json ) {
    checkHistoricStateEnabled();
    bytes bytes = fromHex( _blockRLP );
    BlockHeader blockHeader( bytes );
    return debug_traceBlockByHash( blockHeader.hash().hex(), _json );
}

// TODO Make function without "block" parameter
Json::Value Debug::debug_traceBlockByHash(
    string const& /*_blockHash*/, Json::Value const& _json ) {
    checkHistoricStateEnabled();
    Json::Value ret;
    Block block = m_eth.latestBlock();
    ret["structLogs"] = traceBlock( block, _json );
    return ret;
}

// TODO Make function without "block" parameter
Json::Value Debug::debug_traceBlockByNumber( int /*_blockNumber*/, Json::Value const& _json ) {
    Json::Value ret;
    checkHistoricStateEnabled();
    Block block = m_eth.latestBlock();
    ret["structLogs"] = traceBlock( block, _json );
    return ret;
}

Json::Value Debug::debug_accountRangeAt( string const& _blockHashOrNumber, int _txIndex,
    string const& /*_addressHash*/, int _maxResults ) {
    checkPrivilegedAccess();
    Json::Value ret( Json::objectValue );

    if ( _maxResults <= 0 )
        throw jsonrpc::JsonRpcException( "Nonpositive maxResults" );

    try {
        State const state = stateAt( _blockHashOrNumber, _txIndex );

        throw std::logic_error( "Addresses list is not suppoted in Skale state" );
        //        auto const addressMap = state.addresses(h256(_addressHash), _maxResults);

        //        Json::Value addressList(Json::objectValue);
        //        for (auto const& record : addressMap.first)
        //            addressList[toString(record.first)] = toString(record.second);

        //        ret["addressMap"] = addressList;
        //        ret["nextKey"] = toString(addressMap.second);
    } catch ( Exception const& _e ) {
        cwarn << diagnostic_information( _e );
        throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
    }

    return ret;
}

Json::Value Debug::debug_storageRangeAt( string const& _blockHashOrNumber, int _txIndex,
    string const& /*_address*/, string const& /*_begin*/, int _maxResults ) {
    checkPrivilegedAccess();
    Json::Value ret( Json::objectValue );
    ret["complete"] = true;
    ret["storage"] = Json::Value( Json::objectValue );

    if ( _maxResults <= 0 )
        throw jsonrpc::JsonRpcException( "Nonpositive maxResults" );

    try {
        State const state = stateAt( _blockHashOrNumber, _txIndex );

        throw std::logic_error( "Obtaining of full storage is not suppoted in Skale state" );
        //        map<h256, pair<u256, u256>> const storage(state.storage(Address(_address)));

        //        // begin is inclusive
        //        auto itBegin = storage.lower_bound(h256fromHex(_begin));
        //        for (auto it = itBegin; it != storage.end(); ++it)
        //        {
        //            if (ret["storage"].size() == static_cast<unsigned>(_maxResults))
        //            {
        //                ret["nextKey"] = toCompactHexPrefixed(it->first, 1);
        //                break;
        //            }

        //            Json::Value keyValue(Json::objectValue);
        //            std::string hashedKey = toCompactHexPrefixed(it->first, 1);
        //            keyValue["key"] = toCompactHexPrefixed(it->second.first, 1);
        //            keyValue["value"] = toCompactHexPrefixed(it->second.second, 1);

        //            ret["storage"][hashedKey] = keyValue;
        //        }
    } catch ( Exception const& _e ) {
        cwarn << diagnostic_information( _e );
        throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
    }

    return ret;
}

std::string Debug::debug_preimage( std::string const& /*_hashedKey*/ ) {
    checkPrivilegedAccess();
    throw std::logic_error( "Preimages do not exist in Skale state" );
    //    h256 const hashedKey(h256fromHex(_hashedKey));
    //    bytes const key = m_eth.state().lookupAux(hashedKey);

    //    return key.empty() ? std::string() : toHexPrefixed(key);
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
