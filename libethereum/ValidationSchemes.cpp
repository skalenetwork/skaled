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
#include "ValidationSchemes.h"

#include <libethereum/SchainPatchEnum.h>

#include <libdevcore/JsonUtils.h>

#include <boost/algorithm/string.hpp>

#include <string>

using namespace std;
namespace js = json_spirit;

namespace dev {
namespace eth {
namespace validation {
string const c_sealEngine = "sealEngine";
string const c_params = "params";
string const c_unddos = "unddos";
string const c_genesis = "genesis";
string const c_accounts = "accounts";
string const c_balance = "balance";
string const c_wei = "wei";
string const c_finney = "finney";
string const c_author = "author";
string const c_coinbase = "coinbase";
string const c_nonce = "nonce";
string const c_gasLimit = "gasLimit";
string const c_timestamp = "timestamp";
string const c_difficulty = "difficulty";
string const c_extraData = "extraData";
string const c_mixHash = "mixHash";
string const c_parentHash = "parentHash";
string const c_precompiled = "precompiled";
string const c_restrictAccess = "restrictAccess";
string const c_code = "code";
string const c_storage = "storage";
string const c_gasUsed = "gasUsed";
string const c_codeFromFile = "codeFromFile";  ///< A file containg a code as bytes.
string const c_shouldnotexist = "shouldnotexist";

string const c_minGasLimit = "minGasLimit";
string const c_maxGasLimit = "maxGasLimit";
string const c_gasLimitBoundDivisor = "gasLimitBoundDivisor";
string const c_homesteadForkBlock = "homesteadForkBlock";
string const c_daoHardforkBlock = "daoHardforkBlock";
string const c_EIP150ForkBlock = "EIP150ForkBlock";
string const c_EIP158ForkBlock = "EIP158ForkBlock";
string const c_byzantiumForkBlock = "byzantiumForkBlock";
string const c_eWASMForkBlock = "eWASMForkBlock";
string const c_constantinopleForkBlock = "constantinopleForkBlock";
string const c_constantinopleFixForkBlock = "constantinopleFixForkBlock";
string const c_istanbulForkBlock = "istanbulForkBlock";
string const c_experimentalForkBlock = "experimentalForkBlock";

string const c_skale16ForkBlock = "skale16ForkBlock";
string const c_skale32ForkBlock = "skale32ForkBlock";
string const c_skale64ForkBlock = "skale64ForkBlock";
string const c_skale128ForkBlock = "skale128ForkBlock";
string const c_skale256ForkBlock = "skale256ForkBlock";
string const c_skale512ForkBlock = "skale512ForkBlock";
string const c_skale1024ForkBlock = "skale1024ForkBlock";
string const c_skaleUnlimitedForkBlock = "skaleUnlimitedForkBlock";
string const c_skaleDisableChainIdCheck = "skaleDisableChainIdCheck";

string const c_accountStartNonce = "accountStartNonce";
string const c_maximumExtraDataSize = "maximumExtraDataSize";
string const c_tieBreakingGas = "tieBreakingGas";
string const c_blockReward = "blockReward";
string const c_difficultyBoundDivisor = "difficultyBoundDivisor";
string const c_minimumDifficulty = "minimumDifficulty";
string const c_durationLimit = "durationLimit";
string const c_chainID = "chainID";
string const c_networkID = "networkID";
string const c_allowFutureBlocks = "allowFutureBlocks";


string const c_skaleConfig = "skaleConfig";
string const c_stateRoot = "stateRoot";
string const c_accountInitialFunds = "accountInitialFunds";
string const c_externalGasDifficulty = "externalGasDifficulty";

void validateConfigJson( js::mObject const& _obj ) {
    requireJsonFields( _obj, "ChainParams::loadConfig",
        { { c_sealEngine, { { js::str_type }, JsonFieldPresence::Required } },
            { c_params, { { js::obj_type }, JsonFieldPresence::Required } },
            { c_unddos, { { js::obj_type }, JsonFieldPresence::Optional } },
            { c_genesis, { { js::obj_type }, JsonFieldPresence::Required } },
            { c_skaleConfig, { { js::obj_type }, JsonFieldPresence::Optional } },
            { c_accounts, { { js::obj_type }, JsonFieldPresence::Required } } } );

    requireJsonFields( _obj.at( c_genesis ).get_obj(), "ChainParams::loadConfig::genesis",
        { { c_author, { { js::str_type }, JsonFieldPresence::Required } },
            { c_nonce, { { js::str_type }, JsonFieldPresence::Required } },
            { c_gasLimit, { { js::str_type }, JsonFieldPresence::Required } },
            { c_timestamp, { { js::str_type }, JsonFieldPresence::Required } },
            { c_difficulty, { { js::str_type }, JsonFieldPresence::Required } },
            { c_extraData, { { js::str_type }, JsonFieldPresence::Required } },
            { c_mixHash, { { js::str_type }, JsonFieldPresence::Required } },
            { c_parentHash, { { js::str_type }, JsonFieldPresence::Optional } },
            { c_stateRoot, { { js::str_type }, JsonFieldPresence::Optional } } } );


    js::mObject const& accounts = _obj.at( c_accounts ).get_obj();
    for ( auto const& acc : accounts )
        validateAccountObj( acc.second.get_obj() );

    if ( _obj.count( c_skaleConfig ) == 0 )
        return;

    requireJsonFields( _obj.at( c_skaleConfig ).get_obj(), "ChainParams::loadConfig::skaleConfig",
        { { "nodeInfo", { { js::obj_type }, JsonFieldPresence::Required } },
            { "sChain", { { js::obj_type }, JsonFieldPresence::Required } },
            { "contractSettings", { { js::obj_type }, JsonFieldPresence::Optional } } } );

    const js::mObject& nodeInfo = _obj.at( c_skaleConfig ).get_obj().at( "nodeInfo" ).get_obj();
    requireJsonFields( nodeInfo, "ChainParams::loadConfig::skaleConfig::nodeInfo",
        { { "nodeName", { { js::str_type }, JsonFieldPresence::Required } },
            { "nodeID", { { js::int_type }, JsonFieldPresence::Required } },
            { "ipc", { { js::bool_type }, JsonFieldPresence::Optional } },
            { "bindIP", { { js::str_type }, JsonFieldPresence::Optional } },
            { "basePort", { { js::int_type }, JsonFieldPresence::Optional } },
            { "bindIP6", { { js::str_type }, JsonFieldPresence::Optional } },
            { "basePort6", { { js::int_type }, JsonFieldPresence::Optional } },
            { "httpRpcPort", { { js::int_type }, JsonFieldPresence::Optional } },
            { "httpRpcPort6", { { js::int_type }, JsonFieldPresence::Optional } },
            { "httpsRpcPort", { { js::int_type }, JsonFieldPresence::Optional } },
            { "httpsRpcPort6", { { js::int_type }, JsonFieldPresence::Optional } },
            { "wsRpcPort", { { js::int_type }, JsonFieldPresence::Optional } },
            { "wsRpcPort6", { { js::int_type }, JsonFieldPresence::Optional } },
            { "wssRpcPort", { { js::int_type }, JsonFieldPresence::Optional } },
            { "wssRpcPort6", { { js::int_type }, JsonFieldPresence::Optional } },
            { "infoHttpRpcPort", { { js::int_type }, JsonFieldPresence::Optional } },
            { "infoHttpRpcPort6", { { js::int_type }, JsonFieldPresence::Optional } },
            { "infoHttpsRpcPort", { { js::int_type }, JsonFieldPresence::Optional } },
            { "infoHttpsRpcPort6", { { js::int_type }, JsonFieldPresence::Optional } },
            { "infoWsRpcPort", { { js::int_type }, JsonFieldPresence::Optional } },
            { "infoWsRpcPort6", { { js::int_type }, JsonFieldPresence::Optional } },
            { "infoWssRpcPort", { { js::int_type }, JsonFieldPresence::Optional } },
            { "infoWssRpcPort6", { { js::int_type }, JsonFieldPresence::Optional } },
            { "imaMonitoringPort", { { js::int_type }, JsonFieldPresence::Optional } },
            { "emptyBlockIntervalMs", { { js::int_type }, JsonFieldPresence::Optional } },
            { "emptyBlockIntervalAfterCatchupMs",
                { { js::int_type }, JsonFieldPresence::Optional } },
            { "leveldbReopenIntervalMs", { { js::int_type }, JsonFieldPresence::Optional } },
            { "snapshotIntervalSec", { { js::int_type }, JsonFieldPresence::Optional } },
            { "rotateAfterBlock", { { js::int_type }, JsonFieldPresence::Optional } },
            { "wallets", { { js::obj_type }, JsonFieldPresence::Optional } },
            { "ecdsaKeyName", { { js::str_type }, JsonFieldPresence::Optional } },
            { "verifyImaMessagesViaLogsSearch",
                { { js::bool_type }, JsonFieldPresence::Optional } },
            { "verifyImaMessagesViaContractCall",
                { { js::bool_type }, JsonFieldPresence::Optional } },
            { "verifyImaMessagesViaEnvelopeAnalysis",
                { { js::bool_type }, JsonFieldPresence::Optional } },
            { "imaDebugSkipMessageProxyLogsSearch",
                { { js::bool_type }, JsonFieldPresence::Optional } },
            { "minCacheSize", { { js::int_type }, JsonFieldPresence::Optional } },
            { "maxCacheSize", { { js::int_type }, JsonFieldPresence::Optional } },
            { "collectionQueueSize", { { js::int_type }, JsonFieldPresence::Optional } },
            { "collectionDuration", { { js::int_type }, JsonFieldPresence::Optional } },
            { "transactionQueueSize", { { js::int_type }, JsonFieldPresence::Optional } },
            { "futureTransactionQueueSize", { { js::int_type }, JsonFieldPresence::Optional } },
            { "transactionQueueLimitBytes", { { js::int_type }, JsonFieldPresence::Optional } },
            { "futureTransactionQueueLimitBytes",
                { { js::int_type }, JsonFieldPresence::Optional } },
            { "maxOpenLeveldbFiles", { { js::int_type }, JsonFieldPresence::Optional } },
            { "logLevel", { { js::str_type }, JsonFieldPresence::Optional } },
            { "logLevelConfig", { { js::str_type }, JsonFieldPresence::Optional } },
            { "logLevelProposal", { { js::str_type }, JsonFieldPresence::Optional } },
            { "aa", { { js::str_type }, JsonFieldPresence::Optional } },
            { "acceptors", { { js::int_type }, JsonFieldPresence::Optional } },
            { "info-acceptors", { { js::int_type }, JsonFieldPresence::Optional } },
            { "adminOrigins", { { js::array_type }, JsonFieldPresence::Optional } },
            { "db-path", { { js::str_type }, JsonFieldPresence::Optional } },
            { "block-rotation-period", { { js::int_type }, JsonFieldPresence::Optional } },
            { "ipcpath", { { js::str_type }, JsonFieldPresence::Optional } },
            { "enable-personal-apis", { { js::bool_type }, JsonFieldPresence::Optional } },
            { "enable-admin-apis", { { js::bool_type }, JsonFieldPresence::Optional } },
            { "enable-debug-behavior-apis", { { js::bool_type }, JsonFieldPresence::Optional } },
            { "enable-performance-tracker-apis",
                { { js::bool_type }, JsonFieldPresence::Optional } },
            { "unsafe-transactions", { { js::bool_type }, JsonFieldPresence::Optional } },
            { "pg-trace", { { js::bool_type }, JsonFieldPresence::Optional } },
            { "pg-threads", { { js::int_type }, JsonFieldPresence::Optional } },
            { "pg-threads-limit", { { js::int_type }, JsonFieldPresence::Optional } },
            { "web3-trace", { { js::bool_type }, JsonFieldPresence::Optional } },
            { "web3-shutdown", { { js::bool_type }, JsonFieldPresence::Optional } },
            { "unsafe-transactions", { { js::bool_type }, JsonFieldPresence::Optional } },
            { "max-connections", { { js::int_type }, JsonFieldPresence::Optional } },
            { "max-http-queues", { { js::int_type }, JsonFieldPresence::Optional } },
            { "ws-mode", { { js::str_type }, JsonFieldPresence::Optional } },
            { "ws-log", { { js::str_type }, JsonFieldPresence::Optional } },
            { "log-value-size-limit", { { js::int_type }, JsonFieldPresence::Optional } },
            { "log-json-string-limit", { { js::int_type }, JsonFieldPresence::Optional } },
            { "log-tx-params-limit", { { js::int_type }, JsonFieldPresence::Optional } },
            { "no-ima-signing", { { js::bool_type }, JsonFieldPresence::Optional } },
            { "skale-manager", { { js::obj_type }, JsonFieldPresence::Optional } },
            { "imaMainNet", { { js::str_type }, JsonFieldPresence::Optional } },
            { "imaMessageProxySChain", { { js::str_type }, JsonFieldPresence::Optional } },
            { "imaMessageProxyMainNet", { { js::str_type }, JsonFieldPresence::Optional } },
            { "imaCallerAddressSChain", { { js::str_type }, JsonFieldPresence::Optional } },
            { "imaCallerAddressMainNet", { { js::str_type }, JsonFieldPresence::Optional } },
            { "syncNode", { { js::bool_type }, JsonFieldPresence::Optional } },
            { "archiveMode", { { js::bool_type }, JsonFieldPresence::Optional } },
            { "syncFromCatchup", { { js::bool_type }, JsonFieldPresence::Optional } },
            { "testSignatures", { { js::bool_type }, JsonFieldPresence::Optional } },
            { "wallets", { { js::obj_type }, JsonFieldPresence::Optional } },
            { "catchupTimeoutSec", { { js::int_type }, JsonFieldPresence::Optional } },
            { "syncNodeCatchupTimeoutSec", { { js::int_type }, JsonFieldPresence::Optional } },
            { "readJsonHeaderTimeoutSec", { { js::int_type }, JsonFieldPresence::Optional } },
            { "syncNodeReadJsonHeaderTimeoutSec",
                { { js::int_type }, JsonFieldPresence::Optional } } } );

    std::string keyShareName = "";
    try {
        nodeInfo.at( "wallets" ).get_obj().at( "ima" ).get_obj().at( "keyShareName" ).get_str();
    } catch ( ... ) {
    }

    if ( !keyShareName.empty() ) {
        requireJsonFields( nodeInfo.at( "wallets" ).get_obj().at( "ima" ).get_obj(),
            "ChainParams::loadConfig::skaleConfig::nodeInfo::wallets::ima",
            { { "t", { { js::int_type }, JsonFieldPresence::Required } },
                { "BLSPublicKey0", { { js::str_type }, JsonFieldPresence::Required } },
                { "BLSPublicKey1", { { js::str_type }, JsonFieldPresence::Required } },
                { "BLSPublicKey2", { { js::str_type }, JsonFieldPresence::Required } },
                { "BLSPublicKey3", { { js::str_type }, JsonFieldPresence::Required } },
                { "commonBLSPublicKey0", { { js::str_type }, JsonFieldPresence::Required } },
                { "commonBLSPublicKey1", { { js::str_type }, JsonFieldPresence::Required } },
                { "commonBLSPublicKey2", { { js::str_type }, JsonFieldPresence::Required } },
                { "commonBLSPublicKey3", { { js::str_type }, JsonFieldPresence::Required } } } );
    }  // keyShareName

    const js::mObject& sChain = _obj.at( c_skaleConfig ).get_obj().at( "sChain" ).get_obj();
    requireJsonFields( sChain, "ChainParams::loadConfig::skaleConfig::sChain",
        { { "schainName", { { js::str_type }, JsonFieldPresence::Required } },
            { "schainID", { { js::int_type }, JsonFieldPresence::Required } },
            { "schainOwner", { { js::str_type }, JsonFieldPresence::Optional } },
            { "blockAuthor", { { js::str_type }, JsonFieldPresence::Optional } },
            { "emptyBlockIntervalMs", { { js::int_type }, JsonFieldPresence::Optional } },
            { "emptyBlockIntervalAfterCatchupMs",
                { { js::int_type }, JsonFieldPresence::Optional } },
            { "levelDBReopenIntervalMs", { { js::int_type }, JsonFieldPresence::Optional } },
            { "snapshotIntervalSec", { { js::int_type }, JsonFieldPresence::Optional } },
            { "snapshotDownloadTimeout", { { js::int_type }, JsonFieldPresence::Optional } },
            { "snapshotDownloadInactiveTimeout",
                { { js::int_type }, JsonFieldPresence::Optional } },
            { "rotateAfterBlock", { { js::int_type }, JsonFieldPresence::Optional } },
            { "contractStorageLimit", { { js::int_type }, JsonFieldPresence::Optional } },
            { "dbStorageLimit", { { js::int_type }, JsonFieldPresence::Optional } },
            { "nodes", { { js::array_type }, JsonFieldPresence::Required } },
            { "maxConsensusStorageBytes", { { js::int_type }, JsonFieldPresence::Optional } },
            { "maxFileStorageBytes", { { js::int_type }, JsonFieldPresence::Optional } },
            { "maxReservedStorageBytes", { { js::int_type }, JsonFieldPresence::Optional } },
            { "maxSkaledLeveldbStorageBytes", { { js::int_type }, JsonFieldPresence::Optional } },
            { "freeContractDeployment", { { js::bool_type }, JsonFieldPresence::Optional } },
            { "multiTransactionMode", { { js::bool_type }, JsonFieldPresence::Optional } },
            { "nodeGroups", { { js::obj_type }, JsonFieldPresence::Optional } } },
        []( const string& _key ) {
            // function fow allowing fields
            // exception means bad name
            try {
                string patchName = boost::algorithm::erase_last_copy( _key, "Timestamp" );
                patchName[0] = toupper( patchName[0] );
                getEnumForPatchName( patchName );
                return true;
            } catch ( const std::out_of_range& ) {
                return false;
            }
        } );

    js::mArray const& nodes = sChain.at( "nodes" ).get_array();
    for ( auto const& obj : nodes ) {
        const js::mObject node = obj.get_obj();

        requireJsonFields( node, "ChainParams::loadConfig::skaleConfig::sChain::nodes",
            { { "nodeName", { { js::str_type }, JsonFieldPresence::Optional } },
                { "nodeID", { { js::int_type }, JsonFieldPresence::Required } },
                { "ip", { { js::str_type }, JsonFieldPresence::Required } },
                { "publicIP", { { js::str_type }, JsonFieldPresence::Optional } },  // TODO not used
                { "basePort", { { js::int_type }, JsonFieldPresence::Required } },
                { "ip6", { { js::str_type }, JsonFieldPresence::Optional } },
                { "basePort6", { { js::int_type }, JsonFieldPresence::Optional } },
                { "httpRpcPort", { { js::int_type }, JsonFieldPresence::Optional } },
                { "httpRpcPort6", { { js::int_type }, JsonFieldPresence::Optional } },
                { "httpsRpcPort", { { js::int_type }, JsonFieldPresence::Optional } },
                { "httpsRpcPort6", { { js::int_type }, JsonFieldPresence::Optional } },
                { "wsRpcPort", { { js::int_type }, JsonFieldPresence::Optional } },
                { "wsRpcPort6", { { js::int_type }, JsonFieldPresence::Optional } },
                { "wssRpcPort", { { js::int_type }, JsonFieldPresence::Optional } },
                { "wssRpcPort6", { { js::int_type }, JsonFieldPresence::Optional } },
                { "acceptors", { { js::int_type }, JsonFieldPresence::Optional } },
                { "infoHttpRpcPort", { { js::int_type }, JsonFieldPresence::Optional } },
                { "infoHttpRpcPort6", { { js::int_type }, JsonFieldPresence::Optional } },
                { "infoHttpsRpcPort", { { js::int_type }, JsonFieldPresence::Optional } },
                { "infoHttpsRpcPort6", { { js::int_type }, JsonFieldPresence::Optional } },
                { "infoWsRpcPort", { { js::int_type }, JsonFieldPresence::Optional } },
                { "infoWsRpcPort6", { { js::int_type }, JsonFieldPresence::Optional } },
                { "infoWssRpcPort", { { js::int_type }, JsonFieldPresence::Optional } },
                { "infoWssRpcPort6", { { js::int_type }, JsonFieldPresence::Optional } },
                { "info-acceptors", { { js::int_type }, JsonFieldPresence::Optional } },
                { "schainIndex", { { js::int_type }, JsonFieldPresence::Required } },
                { "publicKey", { { js::str_type }, JsonFieldPresence::Optional } },
                { "blsPublicKey0", { { js::str_type }, JsonFieldPresence::Optional } },
                { "blsPublicKey1", { { js::str_type }, JsonFieldPresence::Optional } },
                { "blsPublicKey2", { { js::str_type }, JsonFieldPresence::Optional } },
                { "blsPublicKey3", { { js::str_type }, JsonFieldPresence::Optional } },
                { "owner", { { js::str_type }, JsonFieldPresence::Optional } },
                { "blockAuthor", { { js::str_type }, JsonFieldPresence::Optional } } } );
    }
}

void validateAccountMaskObj( js::mObject const& _obj ) {
    // A map object with possibly defined fields
    requireJsonFields( _obj, "validateAccountMaskObj",
        { { c_storage, { { js::obj_type }, JsonFieldPresence::Optional } },
            { c_balance, { { js::str_type }, JsonFieldPresence::Optional } },
            { c_nonce, { { js::str_type }, JsonFieldPresence::Optional } },
            { c_code, { { js::str_type }, JsonFieldPresence::Optional } },
            { c_precompiled, { { js::obj_type }, JsonFieldPresence::Optional } },
            { c_shouldnotexist, { { js::str_type }, JsonFieldPresence::Optional } },
            { c_wei, { { js::str_type }, JsonFieldPresence::Optional } } } );
}

void validateAccountObj( js::mObject const& _obj ) {
    if ( _obj.count( c_precompiled ) ) {
        // A precompiled contract
        requireJsonFields( _obj, "validateAccountObj",
            { { c_precompiled, { { js::obj_type }, JsonFieldPresence::Required } },
                { c_wei, { { js::str_type }, JsonFieldPresence::Optional } },
                { c_balance, { { js::str_type }, JsonFieldPresence::Optional } } } );
    } else if ( _obj.size() == 1 ) {
        // A genesis account with only balance set
        if ( _obj.count( c_balance ) )
            requireJsonFields( _obj, "validateAccountObj",
                { { c_balance, { { js::str_type }, JsonFieldPresence::Required } } } );
        else
            requireJsonFields( _obj, "validateAccountObj",
                { { c_wei, { { js::str_type }, JsonFieldPresence::Required } } } );
    } else {
        if ( _obj.count( c_codeFromFile ) ) {
            // A standart account with external code
            requireJsonFields( _obj, "validateAccountObj",
                { { c_codeFromFile, { { js::str_type }, JsonFieldPresence::Required } },
                    { c_nonce, { { js::str_type }, JsonFieldPresence::Required } },
                    { c_storage, { { js::obj_type }, JsonFieldPresence::Required } },
                    { c_balance, { { js::str_type }, JsonFieldPresence::Required } } } );
        } else {
            // A standart account with all fields
            requireJsonFields( _obj, "validateAccountObj",
                { { c_code, { { js::str_type }, JsonFieldPresence::Required } },
                    { c_nonce, { { js::str_type }, JsonFieldPresence::Required } },
                    { c_storage, { { js::obj_type }, JsonFieldPresence::Required } },
                    { c_balance, { { js::str_type }, JsonFieldPresence::Required } } } );
        }
    }
}
}  // namespace validation
}  // namespace eth
}  // namespace dev
