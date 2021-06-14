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

using namespace std;
using namespace dev;
using namespace dev::rpc;
using namespace dev::eth;
using namespace skale;

Debug::Debug( eth::Client const& _eth, SkaleDebugInterface* _debugInterface, const string& argv )
    : m_eth( _eth ), m_debugInterface( _debugInterface ), argv_options( argv ) {}

StandardTrace::DebugOptions dev::eth::debugOptions( Json::Value const& _json ) {
    StandardTrace::DebugOptions op;
    if ( !_json.isObject() || _json.empty() )
        return op;
    if ( !_json["disableStorage"].empty() )
        op.disableStorage = _json["disableStorage"].asBool();
    if ( !_json["disableMemory"].empty() )
        op.disableMemory = _json["disableMemory"].asBool();
    if ( !_json["disableStack"].empty() )
        op.disableStack = _json["disableStack"].asBool();
    if ( !_json["fullStorage"].empty() )
        op.fullStorage = _json["fullStorage"].asBool();
    return op;
}

h256 Debug::blockHash( string const& _blockNumberOrHash ) const {
    if ( isHash< h256 >( _blockNumberOrHash ) )
        return h256( _blockNumberOrHash.substr( _blockNumberOrHash.size() - 64, 64 ) );
    try {
        return m_eth.blockChain().numberHash( stoul( _blockNumberOrHash ) );
    } catch ( ... ) {
        throw jsonrpc::JsonRpcException( "Invalid argument" );
    }
}

State Debug::stateAt( std::string const& /*_blockHashOrNumber*/, int _txIndex ) const {
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

Json::Value Debug::traceTransaction(
    Executive& _e, Transaction const& _t, Json::Value const& _json ) {
    Json::Value trace;
    StandardTrace st;
    st.setShowMnemonics();
    st.setOptions( debugOptions( _json ) );
    _e.initialize( _t );
    if ( !_e.execute() )
        _e.go( st.onOp() );
    _e.finalize();
    Json::Reader().parse( st.json(), trace );
    return trace;
}

Json::Value Debug::traceBlock( Block const& _block, Json::Value const& _json ) {
    State s( _block.state() );
    //    s.setRoot(_block.stateRootBeforeTx(0));

    Json::Value traces( Json::arrayValue );
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
    return traces;
}

Json::Value Debug::debug_traceTransaction(
    string const& /*_txHash*/, Json::Value const& /*_json*/ ) {
    Json::Value ret;
    try {
        throw std::logic_error( "Historical state is not supported in Skale" );
        //        Executive e(s, block, t.transactionIndex(), m_eth.blockChain());
        //        e.setResultRecipient(er);
        //        Json::Value trace = traceTransaction(e, t, _json);
        //        ret["gas"] = toJS(t.gas());
        //        ret["return"] = toHexPrefixed(er.output);
        //        ret["structLogs"] = trace;
    } catch ( Exception const& _e ) {
        cwarn << diagnostic_information( _e );
    }
    return ret;
}

Json::Value Debug::debug_traceBlock( string const& _blockRLP, Json::Value const& _json ) {
    bytes bytes = fromHex( _blockRLP );
    BlockHeader blockHeader( bytes );
    return debug_traceBlockByHash( blockHeader.hash().hex(), _json );
}

// TODO Make function without "block" parameter
Json::Value Debug::debug_traceBlockByHash(
    string const& /*_blockHash*/, Json::Value const& _json ) {
    Json::Value ret;
    Block block = m_eth.latestBlock();
    ret["structLogs"] = traceBlock( block, _json );
    return ret;
}

// TODO Make function without "block" parameter
Json::Value Debug::debug_traceBlockByNumber( int /*_blockNumber*/, Json::Value const& _json ) {
    Json::Value ret;
    Block block = m_eth.latestBlock();
    ret["structLogs"] = traceBlock( block, _json );
    return ret;
}

Json::Value Debug::debug_accountRangeAt( string const& _blockHashOrNumber, int _txIndex,
    string const& /*_addressHash*/, int _maxResults ) {
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
    throw std::logic_error( "Preimages do not exist in Skale state" );
    //    h256 const hashedKey(h256fromHex(_hashedKey));
    //    bytes const key = m_eth.state().lookupAux(hashedKey);

    //    return key.empty() ? std::string() : toHexPrefixed(key);
}

Json::Value Debug::debug_traceCall( Json::Value const& _call, Json::Value const& _options ) {
    Json::Value ret;
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
    return ret;
}

void Debug::debug_pauseBroadcast( bool _pause ) {
    m_eth.skaleHost()->pauseBroadcast( _pause );
}
void Debug::debug_pauseConsensus( bool _pause ) {
    m_eth.skaleHost()->pauseConsensus( _pause );
}
void Debug::debug_forceBlock() {
    m_eth.skaleHost()->forceEmptyBlock();
}

void Debug::debug_forceBroadcast( const std::string& _transactionHash ) {
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
    return m_debugInterface->call( _arg );
}

std::string Debug::debug_getVersion() {
    return Version;
}

std::string Debug::debug_getArguments() {
    return argv_options;
}

std::string Debug::debug_getConfig() {
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
