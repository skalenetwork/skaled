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
/** @file ChainParams.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#include "ChainParams.h"

#include <secp256k1_sha256.h>

#include <stdint.h>
#include <stdlib.h>
#include <cctype>

#include <json_spirit/JsonSpiritHeaders.h>
#include <libdevcore/JsonUtils.h>
#include <libdevcore/Log.h>
#include <libdevcore/TrieDB.h>
#include <libethcore/BlockHeader.h>
#include <libethcore/CommonJS.h>
#include <libethcore/SealEngine.h>
#include <libethereum/Precompiled.h>

#include "Account.h"
#include "ValidationSchemes.h"

#include <skutils/utils.h>

#include <boost/algorithm/string.hpp>

using namespace std;
using namespace dev;
using namespace eth;
using namespace eth::validation;
namespace js = json_spirit;

ChainParams::ChainParams() {
    for ( unsigned i = 1; i <= 4; ++i )
        genesisState[Address( i )] = Account( 0, 1 );
    // Setup default precompiled contracts as equal to genesis of Frontier.
    precompiled.insert( make_pair( Address( 1 ),
        PrecompiledContract( 3000, 0, PrecompiledRegistrar::executor( "ecrecover" ) ) ) );
    precompiled.insert( make_pair(
        Address( 2 ), PrecompiledContract( 60, 12, PrecompiledRegistrar::executor( "sha256" ) ) ) );
    precompiled.insert( make_pair( Address( 3 ),
        PrecompiledContract( 600, 120, PrecompiledRegistrar::executor( "ripemd160" ) ) ) );
    precompiled.insert( make_pair( Address( 4 ),
        PrecompiledContract( 15, 3, PrecompiledRegistrar::executor( "identity" ) ) ) );

    // fill empty stateRoot
    secp256k1_sha256_t ctx;
    secp256k1_sha256_initialize( &ctx );

    dev::h256 empty_str = dev::h256( "" );
    secp256k1_sha256_write( &ctx, empty_str.data(), empty_str.size );

    dev::h256 empty_state_root_hash;
    secp256k1_sha256_finalize( &ctx, empty_state_root_hash.data() );

    stateRoot = empty_state_root_hash;
}

ChainParams::ChainParams( string const& _json ) {
    loadConfig( _json );
}

void ChainParams::loadConfig( string const& _json, const boost::filesystem::path& _configPath ) {
    originalJSON = _json;

    js::mValue val;
    json_spirit::read_string_or_throw( _json, val );
    js::mObject obj = val.get_obj();

    validateConfigJson( obj );
    sealEngineName = obj[c_sealEngine].get_str();
    // params
    js::mObject params = obj[c_params].get_obj();
    //    validateFieldNames(params, c_knownParamNames);
    accountStartNonce =
        u256( fromBigEndian< u256 >( fromHex( params[c_accountStartNonce].get_str() ) ) );
    maximumExtraDataSize =
        u256( fromBigEndian< u256 >( fromHex( params[c_maximumExtraDataSize].get_str() ) ) );
    tieBreakingGas = params.count( c_tieBreakingGas ) ? params[c_tieBreakingGas].get_bool() : true;
    // block rewards for FAIR are set in EVMSchedule
#ifndef FAIR
    setBlockReward( u256( fromBigEndian< u256 >( fromHex( params[c_blockReward].get_str() ) ) ) );
#endif
    skaleDisableChainIdCheck = params.count( c_skaleDisableChainIdCheck ) ?
                                   params[c_skaleDisableChainIdCheck].get_bool() :
                                   false;
    logsBlocksLimit =
        params.count( "getLogsBlocksLimit" ) ? params.at( "getLogsBlocksLimit" ).get_int() : -1;
    responseLogCountLimit = params.count( "getResponseLogCountLimit" ) ?
                                params.at( "getResponseLogCountLimit" ).get_int() :
                                -1;

    if ( obj.count( c_skaleConfig ) ) {
        processSkaleConfigItems( obj );
    }

    auto setOptionalU256Parameter = [&params]( u256& _destination, string const& _name ) {
        if ( params.count( _name ) )
            _destination = u256( fromBigEndian< u256 >( fromHex( params.at( _name ).get_str() ) ) );
    };
    setOptionalU256Parameter( minGasLimit, c_minGasLimit );
    setOptionalU256Parameter( maxGasLimit, c_maxGasLimit );
    setOptionalU256Parameter( gasLimitBoundDivisor, c_gasLimitBoundDivisor );
    setOptionalU256Parameter( homesteadForkBlock, c_homesteadForkBlock );
    setOptionalU256Parameter( EIP150ForkBlock, c_EIP150ForkBlock );
    setOptionalU256Parameter( EIP158ForkBlock, c_EIP158ForkBlock );
    setOptionalU256Parameter( byzantiumForkBlock, c_byzantiumForkBlock );
    setOptionalU256Parameter( eWASMForkBlock, c_eWASMForkBlock );
    setOptionalU256Parameter( constantinopleForkBlock, c_constantinopleForkBlock );
    setOptionalU256Parameter( constantinopleFixForkBlock, c_constantinopleFixForkBlock );
    setOptionalU256Parameter( istanbulForkBlock, c_istanbulForkBlock );
    setOptionalU256Parameter( experimentalForkBlock, c_experimentalForkBlock );

    setOptionalU256Parameter( skale16ForkBlock, c_skale16ForkBlock );
    setOptionalU256Parameter( skale32ForkBlock, c_skale32ForkBlock );
    setOptionalU256Parameter( skale64ForkBlock, c_skale64ForkBlock );
    setOptionalU256Parameter( skale128ForkBlock, c_skale128ForkBlock );
    setOptionalU256Parameter( skale256ForkBlock, c_skale256ForkBlock );
    setOptionalU256Parameter( skale512ForkBlock, c_skale512ForkBlock );
    setOptionalU256Parameter( skale1024ForkBlock, c_skale1024ForkBlock );
    setOptionalU256Parameter( skaleUnlimitedForkBlock, c_skaleUnlimitedForkBlock );

    setOptionalU256Parameter( daoHardforkBlock, c_daoHardforkBlock );
    setOptionalU256Parameter( minimumDifficulty, c_minimumDifficulty );
    setOptionalU256Parameter( difficultyBoundDivisor, c_difficultyBoundDivisor );
    setOptionalU256Parameter( durationLimit, c_durationLimit );
    setOptionalU256Parameter( accountInitialFunds, c_accountInitialFunds );

#ifdef FAIR
    allowPreEIP155Txns =
        params.count( c_allowPreEIP155Txns ) ? params[c_allowPreEIP155Txns].get_bool() : true;
#else
    setOptionalU256Parameter( externalGasDifficulty, c_externalGasDifficulty );
#endif

    if ( params.count( c_chainID ) )
        chainID = uint64_t(
            u256( fromBigEndian< u256 >( fromHex( params.at( c_chainID ).get_str() ) ) ) );
    if ( params.count( c_networkID ) )
        networkID =
            int( u256( fromBigEndian< u256 >( fromHex( params.at( c_networkID ).get_str() ) ) ) );

    allowFutureBlocks = params.count( c_allowFutureBlocks );

#ifndef FAIR
    if ( externalGasDifficulty == 0 ) {
        externalGasDifficulty = -1;
    }
#endif

    // genesis
    string genesisStr = json_spirit::write_string( obj[c_genesis], false );
    loadGenesis( genesisStr );
    // genesis state
    string genesisStateStr = json_spirit::write_string( obj[c_accounts], false );

    genesisState =
        jsonToAccountMap( genesisStateStr, accountStartNonce, nullptr, &precompiled, _configPath );
}
void ChainParams::processSkaleConfigItems( json_spirit::mObject& obj ) {
    auto skaleObj = obj[c_skaleConfig].get_obj();

#ifdef FAIR
    // keep original SKL-style config for compatibility (only for tests)
    bool isLegacy =
        skaleObj.at( "sChain" ).get_obj().at( "nodes" ).type() == json_spirit::array_type;
#endif

    auto infoObj = skaleObj.at( "nodeInfo" ).get_obj();

    auto nodeName = infoObj.at( "nodeName" ).get_str();
    auto nodeID = infoObj.at( "nodeID" ).get_uint64();
    bool syncNode = false;
    bool archiveMode = false;
    bool syncFromCatchup = false;
    string ip, ip6, keyShareName, sgxServerUrl;

    size_t t = 0;

    uint64_t port = 0, port6 = 0;
    try {
        ip = infoObj.at( "bindIP" ).get_str();
    } catch ( ... ) {
    }
    try {
        port = infoObj.at( "basePort" ).get_int();
    } catch ( ... ) {
    }
    try {
        ip6 = infoObj.at( "bindIP6" ).get_str();
    } catch ( ... ) {
    }
    try {
        port6 = infoObj.at( "basePort6" ).get_int();
    } catch ( ... ) {
    }
    try {
        syncNode = infoObj.at( "syncNode" ).get_bool();
    } catch ( ... ) {
    }
    try {
        archiveMode = infoObj.at( "archiveMode" ).get_bool();
    } catch ( ... ) {
    }
    try {
        syncFromCatchup = infoObj.at( "syncFromCatchup" ).get_bool();
    } catch ( ... ) {
    }

    try {
        rotateAfterBlock_ = infoObj.at( "rotateAfterBlock" ).get_int();
    } catch ( ... ) {
    }
    if ( rotateAfterBlock_ < 0 )
        rotateAfterBlock_ = 0;

    bool testSignatures = false;
    try {
        testSignatures = infoObj.at( "testSignatures" ).get_bool();
    } catch ( ... ) {
    }

    string ecdsaKeyName;
    array< string, 4 > BLSPublicKeys;
    array< string, 4 > commonBLSPublicKeys;
    if ( !testSignatures ) {
        if ( infoObj.count( "ecdsaKeyName" ) == 0 ) {
            throw std::runtime_error(
                "No ecdsaKeyName in config, and testSignatures is not set to true" );
        }

        ecdsaKeyName = infoObj.at( "ecdsaKeyName" ).get_str();

#ifdef FAIR
        if ( isLegacy ) {
#endif
            if ( infoObj.count( "wallets" ) == 0 ) {
                throw std::runtime_error(
                    "No wallets section in test config, and testSignatures is not set to true" );
            }

            js::mObject ima = infoObj.at( "wallets" ).get_obj().at( "ima" ).get_obj();

            commonBLSPublicKeys[0] = ima["commonBLSPublicKey0"].get_str();
            commonBLSPublicKeys[1] = ima["commonBLSPublicKey1"].get_str();
            commonBLSPublicKeys[2] = ima["commonBLSPublicKey2"].get_str();
            commonBLSPublicKeys[3] = ima["commonBLSPublicKey3"].get_str();

            if ( !syncNode ) {
                keyShareName = ima.at( "keyShareName" ).get_str();

                t = ima.at( "t" ).get_int();

                BLSPublicKeys[0] = ima["BLSPublicKey0"].get_str();
                BLSPublicKeys[1] = ima["BLSPublicKey1"].get_str();
                BLSPublicKeys[2] = ima["BLSPublicKey2"].get_str();
                BLSPublicKeys[3] = ima["BLSPublicKey3"].get_str();
            }
#ifdef FAIR
        }
#endif
    }

    nodeInfo = { nodeName, nodeID, ip, static_cast< uint16_t >( port ), ip6,
        static_cast< uint16_t >( port6 ), sgxServerUrl, ecdsaKeyName,
#ifndef FAIR
        keyShareName, BLSPublicKeys, commonBLSPublicKeys,
#endif
        syncNode, archiveMode, syncFromCatchup, testSignatures };

    auto sChainObj = skaleObj.at( "sChain" ).get_obj();
    SChain s{};
    s.nodes.clear();

    s.name = sChainObj.at( "schainName" ).get_str();
    s.id = sChainObj.at( "schainID" ).get_uint64();
#ifndef FAIR
    s.t = t;
#endif
    if ( sChainObj.count( "schainOwner" ) ) {
        s.owner = jsToAddress( sChainObj.at( "schainOwner" ).get_str() );
        s.blockAuthor = jsToAddress( sChainObj.at( "schainOwner" ).get_str() );
    }
    if ( sChainObj.count( "blockAuthor" ) )
        s.blockAuthor = jsToAddress( sChainObj.at( "blockAuthor" ).get_str() );

    s.snapshotIntervalSec = sChainObj.count( "snapshotIntervalSec" ) ?
                                sChainObj.at( "snapshotIntervalSec" ).get_int() :
                                0;

    s.snapshotDownloadTimeout = sChainObj.count( "snapshotDownloadTimeout" ) ?
                                    sChainObj.at( "snapshotDownloadTimeout" ).get_int() :
                                    3600;

    s.snapshotDownloadInactiveTimeout =
        sChainObj.count( "snapshotDownloadInactiveTimeout" ) ?
            sChainObj.at( "snapshotDownloadInactiveTimeout" ).get_int() :
            3600;

    s.emptyBlockIntervalMs = sChainObj.count( "emptyBlockIntervalMs" ) ?
                                 sChainObj.at( "emptyBlockIntervalMs" ).get_int() :
                                 0;

    // negative levelDBReopenIntervalMs means restarts are disabled
    s.levelDBReopenIntervalMs = sChainObj.count( "levelDBReopenIntervalMs" ) ?
                                    sChainObj.at( "levelDBReopenIntervalMs" ).get_int64() :
                                    c_defaultLevelDBReopenIntervalMs;

#ifdef HISTORIC_STATE
    // negative maxHistoricStateDbSize means rotations are disabled
    s.maxHistoricStateDbSize = sChainObj.count( "maxHistoricStateDbSize" ) ?
                                   sChainObj.at( "maxHistoricStateDbSize" ).get_int64() :
                                   -1;
#endif

#ifndef FAIR
    s.contractStorageLimit = sChainObj.count( "contractStorageLimit" ) ?
                                 sChainObj.at( "contractStorageLimit" ).get_int64() :
                                 0;
#endif

    s.dbStorageLimit =
        sChainObj.count( "dbStorageLimit" ) ? sChainObj.at( "dbStorageLimit" ).get_int64() : 0;


    if ( sChainObj.count( "maxConsensusStorageBytes" ) ) {
        s.consensusStorageLimit = sChainObj.at( "maxConsensusStorageBytes" ).get_int64();
    }

    if ( sChainObj.count( "multiTransactionMode" ) )
        s.multiTransactionMode = sChainObj.at( "multiTransactionMode" ).get_bool();

#ifdef FAIR
    if ( sChainObj.count( "constantGasPrice" ) )
        s.constantGasPrice = sChainObj.at( "constantGasPrice" ).get_uint64();
#endif

    // extract all "*PatchTimestamp" records
    for ( const auto& it : sChainObj ) {
        const string& key = it.first;
        if ( boost::algorithm::ends_with( key, "PatchTimestamp" ) ) {
            string patchName = boost::algorithm::erase_last_copy( key, "Timestamp" );
            patchName[0] = toupper( patchName[0] );
            SchainPatchEnum patchEnum = getEnumForPatchName( patchName );
            s._patchTimestamps[static_cast< size_t >( patchEnum )] =
                it.second.get_int64();  // time_t is signed
        }                               // if
    }                                   // for

    if ( sChainObj.count( "nodeGroups" ) ) {
        vector< NodeGroup > nodeGroups;
        for ( const auto& nodeGroupConf : sChainObj["nodeGroups"].get_obj() ) {
            NodeGroup nodeGroup;
            auto nodeGroupObj = nodeGroupConf.second.get_obj();
            if ( nodeGroupObj["bls_public_key"].is_null() )
                // failed dkg, skip it
                continue;

            vector< GroupNode > groupNodes;
            auto groupNodesObj = nodeGroupObj["nodes"].get_obj();
            for ( const auto& groupNodeConf : groupNodesObj ) {
                auto groupNodeConfObj = groupNodeConf.second.get_array();
                u256 sChainIndex = groupNodeConfObj.at( 0 ).get_uint64();
                u256 id = groupNodeConfObj.at( 1 ).get_uint64();
                string publicKey = groupNodeConfObj.at( 2 ).get_str();
#ifdef FAIR
                Address rewardWalletAddress = ZeroAddress;
                if ( groupNodeConfObj.size() == 4 ) {
                    rewardWalletAddress = Address( groupNodeConfObj.at( 3 ).get_str() );
                }
#endif
                if ( publicKey.empty() ) {
                    BOOST_THROW_EXCEPTION( runtime_error( "Empty public key in config" ) );
                }
                groupNodes.push_back( { id, sChainIndex, publicKey,
#ifdef FAIR
                    rewardWalletAddress
#endif
                } );
            }
            sort( groupNodes.begin(), groupNodes.end(),
                []( const GroupNode& lhs, const GroupNode& rhs ) {
                    return lhs.schainIndex < rhs.schainIndex;
                } );
            nodeGroup.nodes = groupNodes;

            array< string, 4 > nodeGroupBlsPublicKey;
            auto nodeGroupBlsPublicKeyObj = nodeGroupObj["bls_public_key"].get_obj();
            nodeGroupBlsPublicKey[0] = nodeGroupBlsPublicKeyObj["blsPublicKey0"].get_str();
            nodeGroupBlsPublicKey[1] = nodeGroupBlsPublicKeyObj["blsPublicKey1"].get_str();
            nodeGroupBlsPublicKey[2] = nodeGroupBlsPublicKeyObj["blsPublicKey2"].get_str();
            nodeGroupBlsPublicKey[3] = nodeGroupBlsPublicKeyObj["blsPublicKey3"].get_str();
            nodeGroup.blsPublicKey = nodeGroupBlsPublicKey;

            if ( !nodeGroupObj["finish_ts"].is_null() )
                nodeGroup.finishTs = nodeGroupObj["finish_ts"].get_uint64();
            else
                nodeGroup.finishTs = uint64_t( -1 );
            nodeGroups.push_back( nodeGroup );
        }
        sort(
            nodeGroups.begin(), nodeGroups.end(), []( const NodeGroup& lhs, const NodeGroup& rhs ) {
                return lhs.finishTs < rhs.finishTs;
            } );
        s.nodeGroups = nodeGroups;
    }

    auto parseNodeDetails = [
#ifdef FAIR
                                this,
#endif
                                testSignatures]( const auto& jsonNodeObj ) -> sChainNode {
        auto nodeConfObj = jsonNodeObj.get_obj();
        sChainNode node{};
        node.id = nodeConfObj.at( "nodeID" ).get_uint64();
#ifdef FAIR
        try {
            node.owner = jsToAddress( nodeConfObj.at( "owner" ).get_str() );
        } catch ( ... ) {
            BOOST_LOG( m_loggerWarning )
                << "Node " << node.id << ": owner is not set, using zero address as fallback";
            node.owner = ZeroAddress;
        }

        try {
            node.rewardWalletAddress =
                jsToAddress( nodeConfObj.at( "rewardWalletAddress" ).get_str() );
        } catch ( ... ) {
            BOOST_LOG( m_loggerWarning )
                << "Node " << node.id
                << ": rewardWalletAddress is not set, using zero address as fallback";
            node.rewardWalletAddress = ZeroAddress;
        }
#endif
        node.ip = nodeConfObj.at( "ip" ).get_str();
        node.port = nodeConfObj.at( "basePort" ).get_uint64();
        try {
            node.ip6 = nodeConfObj.at( "ip6" ).get_str();
        } catch ( ... ) {
            node.ip6 = "";
        }
        try {
            node.port6 = nodeConfObj.at( "basePort6" ).get_uint64();
        } catch ( ... ) {
            node.port6 = 0;
        }
        node.sChainIndex = nodeConfObj.at( "schainIndex" ).get_uint64();
        try {
            node.publicKey = nodeConfObj.at( "publicKey" ).get_str();
        } catch ( ... ) {
        }
        if ( !testSignatures ) {
            try {
                node.blsPublicKey[0] = nodeConfObj.at( "blsPublicKey0" ).get_str();
                node.blsPublicKey[1] = nodeConfObj.at( "blsPublicKey1" ).get_str();
                node.blsPublicKey[2] = nodeConfObj.at( "blsPublicKey2" ).get_str();
                node.blsPublicKey[3] = nodeConfObj.at( "blsPublicKey3" ).get_str();
            } catch ( ... ) {
                node.blsPublicKey[0] = "";
                node.blsPublicKey[1] = "";
                node.blsPublicKey[2] = "";
                node.blsPublicKey[3] = "";
            }
        }
        return node;
    };

    // read current group(s) details
#ifndef FAIR
    for ( const auto& nodeConf : sChainObj.at( "nodes" ).get_array() ) {
        auto node = parseNodeDetails( nodeConf );
        s.nodes.push_back( node );
    }
#else
    if ( isLegacy ) {
        // read only nodes details here
        // we got BLS related info earlier
        for ( const auto& nodeConf : sChainObj.at( "nodes" ).get_array() ) {
            auto node = parseNodeDetails( nodeConf );
            s.nodes.push_back( node );
        }
        s.t = t;
        s.currentGroups[1] = { s.nodes, 1, keyShareName, BLSPublicKeys, commonBLSPublicKeys,
            dev::ZeroAddress };
        // make it default
        s.currentGroups[0] = { {}, 0, "", {}, {}, dev::Address() };
    } else {
        auto nodesObjects = sChainObj.at( "nodes" ).get_obj();
        if ( nodesObjects.size() != c_currentGroupsSize )
            BOOST_THROW_EXCEPTION( runtime_error( std::string( "Nodes must have exactly " ) +
                                                  std::to_string( c_currentGroupsSize ) +
                                                  std::string( " groups provided." ) ) );
        for ( auto it = nodesObjects.begin(); it != nodesObjects.end(); ++it ) {
            int64_t startTs;
            try {
                startTs = std::stoll( it->first );
            } catch ( const std::exception& ) {
                BOOST_THROW_EXCEPTION( runtime_error( "Invalid startTs in nodes section." ) );
            }

            dev::Address stakingContractAddress = dev::ZeroAddress;

            std::vector< sChainNode > nodes;

            if ( startTs > 0 ) {
                // read nodes details
                for ( const auto& nodeConf : it->second.get_obj().at( "group" ).get_array() ) {
                    auto node = parseNodeDetails( nodeConf );
                    nodes.push_back( node );
                }
                // read bls related info
                if ( !testSignatures ) {
                    const js::mObject& blsKeyInfo = it->second.get_obj().at( "blsKey" ).get_obj();
                    commonBLSPublicKeys[0] = blsKeyInfo.at( "commonBLSPublicKey0" ).get_str();
                    commonBLSPublicKeys[1] = blsKeyInfo.at( "commonBLSPublicKey1" ).get_str();
                    commonBLSPublicKeys[2] = blsKeyInfo.at( "commonBLSPublicKey2" ).get_str();
                    commonBLSPublicKeys[3] = blsKeyInfo.at( "commonBLSPublicKey3" ).get_str();

                    bool isInCommittee = this->isInCommittee( nodes );
                    if ( isInCommittee ) {
                        keyShareName = blsKeyInfo.at( "keyShareName" ).get_str();

                        t = blsKeyInfo.at( "t" ).get_int();

                        BLSPublicKeys[0] = blsKeyInfo.at( "BLSPublicKey0" ).get_str();
                        BLSPublicKeys[1] = blsKeyInfo.at( "BLSPublicKey1" ).get_str();
                        BLSPublicKeys[2] = blsKeyInfo.at( "BLSPublicKey2" ).get_str();
                        BLSPublicKeys[3] = blsKeyInfo.at( "BLSPublicKey3" ).get_str();
                    }
                }
                // read staking contract address
                try {
                    stakingContractAddress = dev::Address(
                        it->second.get_obj().at( "stakingContractAddress" ).get_str() );
                } catch ( ... ) {
                }
            } else {
                // timestamp is set to 0 for BOOT group
                startTs = 0;
            }
            s.currentGroups[std::distance( nodesObjects.begin(), it )] = { nodes,
                ( uint64_t ) startTs, keyShareName, BLSPublicKeys, commonBLSPublicKeys,
                stakingContractAddress };
            s.t = t;
        }
    }

    // for BOOT group timestamp is set to 0
    // invariant here - relevant group MUST BE stored under index 1
    if ( s.currentGroups[0].startTs < s.currentGroups[1].startTs &&
         s.currentGroups[0].startTs != 0 )
        std::swap( s.currentGroups[0], s.currentGroups[1] );

    s.nodes = s.currentGroups.back().nodes;

    switchSyncMode( s.nodes );
#endif

    sChain = s;

    vecAdminOrigins.clear();
    if ( infoObj.count( "adminOrigins" ) ) {
        for ( const auto& nodeOrigin : infoObj.at( "adminOrigins" ).get_array() ) {
            string strOriginWildcardFilter = nodeOrigin.get_str();
            vecAdminOrigins.push_back( strOriginWildcardFilter );
        }
    } else {
        vecAdminOrigins.push_back( "*" );
    }
}

void ChainParams::loadGenesis( string const& _json ) {
    js::mValue val;
    js::read_string( _json, val );
    js::mObject genesis = val.get_obj();

    parentHash = h256( 0 );  // required by the YP
    author = genesis.count( c_coinbase ) ? h160( genesis[c_coinbase].get_str() ) :
                                           h160( genesis[c_author].get_str() );
    difficulty = genesis.count( c_difficulty ) ?
                     u256( fromBigEndian< u256 >( fromHex( genesis[c_difficulty].get_str() ) ) ) :
                     minimumDifficulty;
    gasLimit = u256( fromBigEndian< u256 >( fromHex( genesis[c_gasLimit].get_str() ) ) );
    gasUsed = genesis.count( c_gasUsed ) ?
                  u256( fromBigEndian< u256 >( fromHex( genesis[c_gasUsed].get_str() ) ) ) :
                  0;
    timestamp = u256( fromBigEndian< u256 >( fromHex( genesis[c_timestamp].get_str() ) ) );
    extraData = bytes( fromHex( genesis[c_extraData].get_str() ) );

    if ( genesis.count( c_stateRoot ) )
        stateRoot = h256( fromHex( genesis[c_stateRoot].get_str() ), h256::AlignRight );

    // magic code for handling ethash stuff:
    if ( genesis.count( c_mixHash ) && genesis.count( c_nonce ) ) {
        h256 mixHash( genesis[c_mixHash].get_str() );
        h64 nonce( genesis[c_nonce].get_str() );
        sealFields = 2;
        sealRLP = rlp( mixHash ) + rlp( nonce );
    }
}

SealEngineFace* ChainParams::createSealEngine() {
    SealEngineFace* ret = SealEngineRegistrar::create( sealEngineName );
    assert( ret && "Seal engine not found" );
    if ( !ret )
        return nullptr;
    ret->setChainParams( *this );
    if ( sealRLP.empty() ) {
        sealFields = ret->sealFields();
        sealRLP = ret->sealRLP();
    }
    return ret;
}

void ChainParams::populateFromGenesis( bytes const& _genesisRLP, AccountMap const& _state ) {
    BlockHeader bi( _genesisRLP, RLP( &_genesisRLP )[0].isList() ? BlockData : HeaderData );
    parentHash = bi.parentHash();
    author = bi.author();
    difficulty = bi.difficulty();
    gasLimit = bi.gasLimit();
    gasUsed = bi.gasUsed();
    timestamp = bi.timestamp();
    extraData = bi.extraData();
    genesisState = _state;
    RLP r( _genesisRLP );
    sealFields = r[0].itemCount() - BlockHeader::BasicFields;
    sealRLP.clear();
    for ( unsigned i = BlockHeader::BasicFields; i < r[0].itemCount(); ++i )
        sealRLP += r[0][i].data();

    this->stateRoot = bi.stateRoot();

    auto b = genesisBlock();
    if ( b != _genesisRLP ) {
        BOOST_LOG( m_loggerDebug ) << "Block passed:" << bi.hash() << bi.hash( WithoutSeal );
        BOOST_LOG( m_loggerDebug ) << "Genesis now:" << BlockHeader::headerHashFromBlock( b );
        BOOST_LOG( m_loggerDebug ) << RLP( b );
        BOOST_LOG( m_loggerDebug ) << RLP( _genesisRLP );
        throw 0;
    }
}

bytes ChainParams::genesisBlock() const {
    RLPStream block( 3 );

    block.appendList( BlockHeader::BasicFields + sealFields )
        << parentHash << EmptyListSHA3       // sha3(uncles)
        << author << stateRoot << EmptyTrie  // transactions
        << EmptyTrie                         // receipts
        << LogBloom() << difficulty << 0     // number
        << gasLimit << gasUsed               // gasUsed
        << timestamp << extraData;
    block.appendRaw( sealRLP, sealFields );
    block.appendRaw( RLPEmptyList );
    block.appendRaw( RLPEmptyList );
    return block.out();
}

const std::string& ChainParams::getOriginalJson() const {
    if ( !originalJSON.empty() )
        return originalJSON;

    js::mObject obj;
    obj[c_sealEngine] = sealEngineName;

    // params
    js::mObject params;
    params[c_accountStartNonce] = toHex( toBigEndian( accountStartNonce ) );
    params[c_maximumExtraDataSize] = toHex( toBigEndian( maximumExtraDataSize ) );
    params[c_tieBreakingGas] = tieBreakingGas;
    params[c_blockReward] = toHex( toBigEndian( blockReward( DefaultSchedule ) ) );

    params[c_minGasLimit] = toHex( toBigEndian( minGasLimit ) );
    params[c_maxGasLimit] = toHex( toBigEndian( maxGasLimit ) );
    params[c_gasLimitBoundDivisor] = toHex( toBigEndian( gasLimitBoundDivisor ) );
    params[c_homesteadForkBlock] = toHex( toBigEndian( homesteadForkBlock ) );
    params[c_EIP150ForkBlock] = toHex( toBigEndian( EIP150ForkBlock ) );
    params[c_EIP158ForkBlock] = toHex( toBigEndian( EIP158ForkBlock ) );
    params[c_byzantiumForkBlock] = toHex( toBigEndian( byzantiumForkBlock ) );
    params[c_eWASMForkBlock] = toHex( toBigEndian( eWASMForkBlock ) );
    params[c_constantinopleForkBlock] = toHex( toBigEndian( constantinopleForkBlock ) );
    params[c_eWASMForkBlock] = toHex( toBigEndian( eWASMForkBlock ) );
    params[c_daoHardforkBlock] = toHex( toBigEndian( daoHardforkBlock ) );
    params[c_minimumDifficulty] = toHex( toBigEndian( minimumDifficulty ) );
    params[c_difficultyBoundDivisor] = toHex( toBigEndian( difficultyBoundDivisor ) );
    params[c_durationLimit] = toHex( toBigEndian( durationLimit ) );

    params[c_skale16ForkBlock] = toHex( toBigEndian( skale16ForkBlock ) );
    params[c_skale32ForkBlock] = toHex( toBigEndian( skale32ForkBlock ) );
    params[c_skale64ForkBlock] = toHex( toBigEndian( skale64ForkBlock ) );
    params[c_skale128ForkBlock] = toHex( toBigEndian( skale128ForkBlock ) );
    params[c_skale256ForkBlock] = toHex( toBigEndian( skale256ForkBlock ) );
    params[c_skale512ForkBlock] = toHex( toBigEndian( skale512ForkBlock ) );
    params[c_skale1024ForkBlock] = toHex( toBigEndian( skale1024ForkBlock ) );
    params[c_skaleUnlimitedForkBlock] = toHex( toBigEndian( skaleUnlimitedForkBlock ) );
    params[c_skaleDisableChainIdCheck] = skaleDisableChainIdCheck;

    params[c_chainID] = toHex( toBigEndian( u256( chainID ) ) );
    params[c_networkID] = toHex( toBigEndian( u256( networkID ) ) );
    params[c_allowFutureBlocks] = allowFutureBlocks;

    obj[c_params] = params;

    /////skale
    js::mObject skaleObj;

    js::mObject infoObj;

    infoObj["nodeName"] = nodeInfo.name;
    infoObj["nodeID"] = ( int64_t ) nodeInfo.id;
    infoObj["bindIP"] = nodeInfo.ip;
    infoObj["basePort"] = ( int64_t ) nodeInfo.port;  // TODO not so many bits!
    infoObj["bindIP6"] = nodeInfo.ip6;
    infoObj["basePort6"] = ( int64_t ) nodeInfo.port6;  // TODO not so many bits!
    infoObj["logLevel"] = "trace";
    infoObj["logLevelProposal"] = "trace";
    infoObj["ecdsaKeyName"] = nodeInfo.ecdsaKeyName;

    skaleObj["nodeInfo"] = infoObj;

    js::mObject sChainObj;
    SChain s{};

    sChainObj["schainName"] = sChain.name;
    sChainObj["schainID"] = ( int64_t ) sChain.id;
    sChainObj["emptyBlockIntervalMs"] = sChain.emptyBlockIntervalMs;
    sChainObj["snpshotIntervalMs"] = sChain.snapshotIntervalSec;
    sChainObj["multiTransactionMode"] = sChain.multiTransactionMode;
#ifndef FAIR
    sChainObj["contractStorageLimit"] = ( int64_t ) sChain.contractStorageLimit;
#endif
    sChainObj["dbStorageLimit"] = sChain.dbStorageLimit;

#ifdef FAIR
    auto addNodeToArray = []( const sChainNode& node, js::mArray& nodes ) {
#else
    js::mArray nodes;

    for ( const auto& node : sChain.nodes ) {
#endif
        js::mObject nodeConfObj;
        nodeConfObj["nodeID"] = ( int64_t ) node.id;
        nodeConfObj["ip"] = node.ip;
        nodeConfObj["basePort"] = ( int64_t ) node.port;
        nodeConfObj["ip6"] = node.ip6;
        nodeConfObj["basePort6"] = ( int64_t ) node.port6;
        nodeConfObj["schainIndex"] = ( int64_t ) node.sChainIndex;
        nodeConfObj["publicKey"] = node.publicKey;

        nodes.push_back( nodeConfObj );
    };

#ifdef FAIR
    js::mObject nodes;

    for ( const auto& currentGroup : sChain.currentGroups ) {
        js::mObject group;

        js::mArray currentNodes;
        for ( const auto& schainNode : currentGroup.nodes ) {
            addNodeToArray( schainNode, currentNodes );
        }
        group["group"] = currentNodes;

        js::mObject blsKey;
        blsKey["keyShareName"] = currentGroup.keyShareName;

        group["blsKey"] = blsKey;

        nodes[std::to_string( currentGroup.startTs )] = group;
    }
#endif

    sChainObj["nodes"] = nodes;

    skaleObj["sChain"] = sChainObj;

    if ( !nodeInfo.ip.empty() )  // just test some meaningful field
        obj[c_skaleConfig] = skaleObj;

    //// genesis
    js::mObject genesis;

    genesis[c_author] = toHex( author.asBytes() );
    genesis[c_coinbase] = toHex( h256( author ).asBytes() );

    genesis[c_difficulty] = toHex( toBigEndian( difficulty ) );
    genesis[c_gasLimit] = toHex( toBigEndian( gasLimit ) );
    genesis[c_gasUsed] = toHex( toBigEndian( gasUsed ) );
    genesis[c_timestamp] = toHex( toBigEndian( timestamp ) );
    genesis[c_extraData] = toHex( extraData );
    genesis[c_mixHash] =
        "0x0000000000000000000000000000000000000000000000000000000000000000";  // HACK
    genesis[c_nonce] = "0x0000000000000042";

    obj[c_genesis] = genesis;

    // XXX ignore the account map for now

    originalJSON = js::write_string( ( js::mValue ) obj, true );

    return originalJSON;
}

bool ChainParams::checkAdminOriginAllowed( const std::string& origin ) const {
    if ( vecAdminOrigins.empty() )
        return true;
    for ( const std::string& wild : vecAdminOrigins ) {
        if ( skutils::tools::wildcmp( wild.c_str(), origin.c_str() ) )
            return true;
    }
    return false;
}

std::vector< sChainNode > ChainParams::getSchainNodes() const {
#ifdef FAIR
    std::shared_lock< std::shared_mutex > lock( m_mutex );
#endif
    return sChain.nodes;
}

sChainNode ChainParams::getNodeByIndex( size_t _idx ) const {
#ifdef FAIR
    std::shared_lock< std::shared_mutex > lock( m_mutex );
#endif
    return sChain.nodes.at( _idx );
}

size_t ChainParams::getNodesCount() const {
#ifdef FAIR
    std::shared_lock< std::shared_mutex > lock( m_mutex );
#endif
    return sChain.nodes.size();
}

std::array< std::string, 4 > ChainParams::getSelfBlsPublicKey() const {
#ifndef FAIR
    return nodeInfo.BLSPublicKeys;
#else
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    return sChain.currentGroups.back().BLSPublicKeys;
#endif
}

std::array< std::string, 4 > ChainParams::getCommonBlsPublicKey() const {
#ifndef FAIR
    return nodeInfo.commonBLSPublicKeys;
#else
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    return sChain.currentGroups.back().commonBLSPublicKeys;
#endif
}

std::string ChainParams::getKeyShareName() const {
#ifndef FAIR
    return nodeInfo.keyShareName;
#else
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    return sChain.currentGroups.back().keyShareName;
#endif
}

void ChainParams::fillDefaultTestsParameters( size_t _port ) {
    sealEngineName = NoProof::name();
    allowFutureBlocks = true;
    difficulty = getMinimumDifficulty();
    gasLimit = getMaxGasLimit();
    nodeInfo.port = nodeInfo.port6 = _port;
    sChain.nodes[0].port = sChain.nodes[0].port6 = _port;
}

#ifdef FAIR
Address ChainParams::getStakingContractAddress() const {
    std::shared_lock< std::shared_mutex > lock( m_mutex );
    return sChain.currentGroups.back().stakingContractAddress;
}

std::string ChainParams::getConfigForConsensus() const {
    js::mValue val;
    json_spirit::read_string_or_throw( getOriginalJson(), val );
    js::mObject obj = val.get_obj();

    js::mObject skaleConfigObj = obj["skaleConfig"].get_obj();
    js::mObject sChainObj = skaleConfigObj["sChain"].get_obj();
    js::mObject nodeInfoObj = skaleConfigObj["nodeInfo"].get_obj();

    nodeInfoObj["syncNode"] = isSyncNode();

    js::mArray newNodesObj;
    if ( sChainObj["nodes"].type() == json_spirit::obj_type ) {
        js::mObject nodesObj = sChainObj["nodes"].get_obj();

        if ( sChain.nodes == sChain.currentGroups[0].nodes )
            newNodesObj = nodesObj[std::to_string( sChain.currentGroups[0].startTs )]
                              .get_obj()
                              .at( "group" )
                              .get_array();
        else
            newNodesObj = nodesObj[std::to_string( sChain.currentGroups[1].startTs )]
                              .get_obj()
                              .at( "group" )
                              .get_array();
    } else {
        newNodesObj = sChainObj["nodes"].get_array();
    }

    sChainObj["nodes"] = newNodesObj;
    skaleConfigObj["sChain"] = sChainObj;
    skaleConfigObj["nodeInfo"] = nodeInfoObj;
    obj["skaleConfig"] = skaleConfigObj;

    return js::write_string( js::mValue( obj ), true );
}

bool ChainParams::updateCurrentGroupIfNeeded( uint64_t _latestBlockTimestamp ) {
    // for BOOT group timestamp is set to 0
    // invariant here - relevant group MUST BE stored under index 1
    if ( _latestBlockTimestamp >= sChain.currentGroups[0].startTs &&
         sChain.currentGroups[0].startTs > sChain.currentGroups[1].startTs ) {
        BOOST_LOG( m_loggerInfo ) << "Updating current group";
        std::unique_lock< std::shared_mutex > lock( m_mutex );
        std::swap( sChain.currentGroups[0], sChain.currentGroups[1] );
        BOOST_LOG( m_loggerInfo ) << "Using the group with startTs "
                                  << sChain.currentGroups[1].startTs;
        sChain.nodes = sChain.currentGroups[1].nodes;
        switchSyncMode( sChain.nodes );
        return true;
    }
    return false;
}

CurrentGroup ChainParams::getNewestGroup() const {
    size_t newestIndex = 0;
    if ( sChain.currentGroups[1].startTs >= sChain.currentGroups[0].startTs ) {
        newestIndex = 1;
    }
    return sChain.currentGroups[newestIndex];
}

Address ChainParams::getNodeBeneficiaryInHistoricGroup(
    const unsigned _historicGroupIndex, const uint64_t _sChainIndex ) const {
    // shifting since sChain indices are numbered from 0
    if ( _sChainIndex == 0 ) {
        throw std::runtime_error( "Cannot get beneficiary for zero sChainIndex" );
    }
    uint64_t shiftedIndex = _sChainIndex - 1;
    if ( sChain.nodeGroups.size() > 0 ) {
        return getHistoricNodeRewardWalletAddress( _historicGroupIndex, shiftedIndex );
    } else {
        return sChain.currentGroups.at( 1 ).nodes.at( shiftedIndex ).owner;
    }
}

bool ChainParams::isInCommittee( const std::vector< sChainNode >& _committee ) const {
    u256 thisNodeId = nodeInfo.id;
    auto it = std::find_if( _committee.begin(), _committee.end(),
        [thisNodeId]( const sChainNode& node ) { return node.id == thisNodeId; } );
    return it != _committee.end();
}

// if a node is not in active committee
// it should switch to sync mode
// use it only after sChain.nodes was initialized / updated
void ChainParams::switchSyncMode( const std::vector< sChainNode >& _nodes ) {
    if ( !isInCommittee( _nodes ) ) {
        nodeInfo.syncNode = true;
    } else {
        nodeInfo.syncNode = false;
    }
}
#endif
