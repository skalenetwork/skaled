
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


Debug::Debug( eth::Client& _eth, SkaleDebugInterface* _debugInterface, const string& argv )
    : m_eth( _eth ), m_debugInterface( _debugInterface ), m_argvOptions( argv ) {}


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
    m_eth.skaleHost()->pauseBroadcast( _pause );
}
void Debug::debug_pauseConsensus( bool _pause ) {
    m_eth.skaleHost()->pauseConsensus( _pause );
}
void Debug::debug_forceBlock() {
    m_eth.skaleHost()->forceEmptyBlock();
}

void Debug::debug_forceBroadcast( const string& _transactionHash ) {
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
    return m_debugInterface->call( _arg );
}

string Debug::debug_getVersion() {
    return Version;
}

string Debug::debug_getArguments() {
    return m_argvOptions;
}

string Debug::debug_getConfig() {
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
    auto t1 = boost::chrono::high_resolution_clock::now();
    m_eth.doStateDbCompaction();
    auto t2 = boost::chrono::high_resolution_clock::now();

    return boost::chrono::duration_cast< boost::chrono::milliseconds >( t2 - t1 ).count();
}

uint64_t Debug::debug_doBlocksDbCompaction() {
    auto t1 = boost::chrono::high_resolution_clock::now();
    m_eth.doBlocksDbCompaction();
    auto t2 = boost::chrono::high_resolution_clock::now();

    return boost::chrono::duration_cast< boost::chrono::milliseconds >( t2 - t1 ).count();
}

Json::Value Debug::debug_getFutureTransactions() {
    auto res = toJson( m_eth.debugGetFutureTransactions() );
    for ( auto& t : res )
        t.removeMember( "data" );
    return res;
}

Json::Value Debug::debug_getPatchTimestamps() {
    nlohmann::json joPatchedTimestamps = nlohmann::json::object();
    ChainParams chainParams = m_eth.chainParams();
    
    joPatchedTimestamps[getPatchNameForEnum(SchainPatchEnum::ContractStoragePatch)] = 
        chainParams.getPatchTimestamp( SchainPatchEnum::ContractStoragePatch );

    joPatchedTimestamps[getPatchNameForEnum(SchainPatchEnum::ContractStorageZeroValuePatch)] = 
        chainParams.getPatchTimestamp( SchainPatchEnum::ContractStorageZeroValuePatch );

    joPatchedTimestamps[getPatchNameForEnum(SchainPatchEnum::CorrectForkInPowPatch)] = 
        chainParams.getPatchTimestamp( SchainPatchEnum::CorrectForkInPowPatch );

    joPatchedTimestamps[getPatchNameForEnum(SchainPatchEnum::EIP1559TransactionsPatch)] = 
        chainParams.getPatchTimestamp( SchainPatchEnum::EIP1559TransactionsPatch );

    joPatchedTimestamps[getPatchNameForEnum(SchainPatchEnum::FastConsensusPatch)] = 
        chainParams.getPatchTimestamp( SchainPatchEnum::FastConsensusPatch );

    joPatchedTimestamps[getPatchNameForEnum(SchainPatchEnum::FlexibleDeploymentPatch)] = 
        chainParams.getPatchTimestamp( SchainPatchEnum::FlexibleDeploymentPatch );

    joPatchedTimestamps[getPatchNameForEnum(SchainPatchEnum::PowCheckPatch)] = 
        chainParams.getPatchTimestamp( SchainPatchEnum::PowCheckPatch );

    joPatchedTimestamps[getPatchNameForEnum(SchainPatchEnum::PrecompiledConfigPatch)] = 
        chainParams.getPatchTimestamp( SchainPatchEnum::PrecompiledConfigPatch );

    joPatchedTimestamps[getPatchNameForEnum(SchainPatchEnum::PushZeroPatch)] = 
        chainParams.getPatchTimestamp( SchainPatchEnum::PushZeroPatch );

    joPatchedTimestamps[getPatchNameForEnum(SchainPatchEnum::RevertableFSPatch)] = 
        chainParams.getPatchTimestamp( SchainPatchEnum::RevertableFSPatch );

    joPatchedTimestamps[getPatchNameForEnum(SchainPatchEnum::SkipInvalidTransactionsPatch)] = 
        chainParams.getPatchTimestamp( SchainPatchEnum::SkipInvalidTransactionsPatch );

    joPatchedTimestamps[getPatchNameForEnum(SchainPatchEnum::StorageDestructionPatch)] = 
        chainParams.getPatchTimestamp( SchainPatchEnum::StorageDestructionPatch );

    joPatchedTimestamps[getPatchNameForEnum(SchainPatchEnum::VerifyBlsSyncPatch)] = 
        chainParams.getPatchTimestamp( SchainPatchEnum::VerifyBlsSyncPatch );
        
    joPatchedTimestamps[getPatchNameForEnum(SchainPatchEnum::VerifyDaSigsPatch)] = 
        chainParams.getPatchTimestamp( SchainPatchEnum::VerifyDaSigsPatch );

    std::string strResponse = joPatchedTimestamps.dump();
    Json::Value response;
    Json::Reader().parse( strResponse, response );
    return response;
}
