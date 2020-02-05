/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skaled.

    skaled is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skaled is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skaled.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file Eth.cpp
 * @authors:
 *   Sergiy Lavrynenko <sergiy@skalelabs.com>
 * @date 2019
 */

#include "SkaleStats.h"
#include "Eth.h"
#include "libethereum/Client.h"
#include <jsonrpccpp/common/exception.h>
#include <libweb3jsonrpc/JsonHelper.h>

#include <libdevcore/BMPBN.h>
#include <libdevcore/Common.h>
#include <libdevcore/CommonJS.h>

#include <skutils/console_colors.h>
#include <skutils/eth_utils.h>
#include <skutils/rest_call.h>

#include <array>
#include <csignal>
#include <exception>
#include <fstream>
#include <set>

//#include "../libconsensus/libBLS/bls/bls.h"

#include <bls/bls.h>

#include <bls/BLSutils.h>

#include <skutils/console_colors.h>
#include <skutils/utils.h>

namespace dev {
namespace rpc {

SkaleStats::SkaleStats( const std::string& configPath, eth::Interface& _eth )
    : skutils::json_config_file_accessor( configPath ), m_eth( _eth ) {
    nThisNodeIndex_ = findThisNodeIndex();
}

int SkaleStats::findThisNodeIndex() {
    try {
        nlohmann::json joConfig = getConfigJSON();
        if ( joConfig.count( "skaleConfig" ) == 0 )
            throw std::runtime_error( "error config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        //
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        //
        if ( joSkaleConfig.count( "sChain" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find \"skaleConfig\"/\"sChain\"" );
        const nlohmann::json& joSkaleConfig_sChain = joSkaleConfig["sChain"];
        //
        if ( joSkaleConfig_sChain.count( "nodes" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find \"skaleConfig\"/\"sChain\"/\"nodes\"" );
        const nlohmann::json& joSkaleConfig_sChain_nodes = joSkaleConfig_sChain["nodes"];
        //
        int nID = joSkaleConfig_nodeInfo["nodeID"].get< int >();
        const nlohmann::json& jarrNodes = joSkaleConfig_sChain_nodes;
        size_t i, cnt = jarrNodes.size();
        for ( i = 0; i < cnt; ++i ) {
            const nlohmann::json& joNC = jarrNodes[i];
            try {
                int nWalkID = joNC["nodeID"].get< int >();
                if ( nID == nWalkID )
                    return joNC["schainIndex"].get< int >();
            } catch ( ... ) {
                continue;
            }
        }
    } catch ( ... ) {
    }
    return -1;
}

Json::Value SkaleStats::skale_stats() {
    try {
        nlohmann::json joStats = consumeSkaleStats();

        // HACK Add stats from SkaleDebug
        // TODO Why we need all this absatract infrastructure?
        const dev::eth::Client* c = dynamic_cast< dev::eth::Client* const >( this->client() );
        if ( c ) {
            nlohmann::json joTrace;
            std::shared_ptr< SkaleHost > h = c->skaleHost();
            std::istringstream list( h->debugCall( "trace list" ) );
            std::string key;
            while ( list >> key ) {
                std::string count_str = h->debugCall( "trace count " + key );
                joTrace[key] = atoi( count_str.c_str() );
            }  // while

            joStats["tracepoints"] = joTrace;

        }  // if client

        std::string strStatsJson = joStats.dump();
        Json::Value ret;
        Json::Reader().parse( strStatsJson, ret );
        return ret;
    } catch ( Exception const& ) {
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        throw jsonrpc::JsonRpcException( ex.what() );
    }
}

Json::Value SkaleStats::skale_nodesRpcInfo() {
    try {
        nlohmann::json joConfig = getConfigJSON();
        if ( joConfig.count( "skaleConfig" ) == 0 )
            throw std::runtime_error( "error config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        //
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        //
        if ( joSkaleConfig.count( "sChain" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find \"skaleConfig\"/\"sChain\"" );
        const nlohmann::json& joSkaleConfig_sChain = joSkaleConfig["sChain"];
        //
        if ( joSkaleConfig_sChain.count( "nodes" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find \"skaleConfig\"/\"sChain\"/\"nodes\"" );
        const nlohmann::json& joSkaleConfig_sChain_nodes = joSkaleConfig_sChain["nodes"];
        //
        nlohmann::json jo = nlohmann::json::object();
        //
        nlohmann::json joThisNode = nlohmann::json::object();
        joThisNode["thisNodeIndex"] = nThisNodeIndex_;  // 1-based "schainIndex"
        try {
            joThisNode["nodeID"] = joSkaleConfig_nodeInfo["nodeID"].get< int >();
        } catch ( ... ) {
            joThisNode["nodeID"] = -1;
        }
        try {
            joThisNode["bindIP"] = joSkaleConfig_nodeInfo["bindIP"].get< std::string >();
        } catch ( ... ) {
            joThisNode["bindIP"] = "";
        }
        try {
            joThisNode["bindIP6"] = joSkaleConfig_nodeInfo["bindIP6"].get< std::string >();
        } catch ( ... ) {
            joThisNode["bindIP6"] = "";
        }
        try {
            joThisNode["httpRpcPort"] = joSkaleConfig_nodeInfo["httpRpcPort"].get< int >();
        } catch ( ... ) {
            joThisNode["httpRpcPort"] = 0;
        }
        try {
            joThisNode["httpsRpcPort"] = joSkaleConfig_nodeInfo["httpsRpcPort"].get< int >();
        } catch ( ... ) {
            joThisNode["httpsRpcPort"] = 0;
        }
        try {
            joThisNode["wsRpcPort"] = joSkaleConfig_nodeInfo["wsRpcPort"].get< int >();
        } catch ( ... ) {
            joThisNode["wsRpcPort"] = 0;
        }
        try {
            joThisNode["wssRpcPort"] = joSkaleConfig_nodeInfo["wssRpcPort"].get< int >();
        } catch ( ... ) {
            joThisNode["wssRpcPort"] = 0;
        }
        try {
            joThisNode["httpRpcPort6"] = joSkaleConfig_nodeInfo["httpRpcPort6"].get< int >();
        } catch ( ... ) {
            joThisNode["httpRpcPort6"] = 0;
        }
        try {
            joThisNode["httpsRpcPort6"] = joSkaleConfig_nodeInfo["httpsRpcPort6"].get< int >();
        } catch ( ... ) {
            joThisNode["httpsRpcPort6"] = 0;
        }
        try {
            joThisNode["wsRpcPort6"] = joSkaleConfig_nodeInfo["wsRpcPort6"].get< int >();
        } catch ( ... ) {
            joThisNode["wsRpcPort6"] = 0;
        }
        try {
            joThisNode["wssRpcPort6"] = joSkaleConfig_nodeInfo["wssRpcPort6"].get< int >();
        } catch ( ... ) {
            joThisNode["wssRpcPort6"] = 0;
        }
        try {
            joThisNode["acceptors"] = joSkaleConfig_nodeInfo["acceptors"].get< int >();
        } catch ( ... ) {
            joThisNode["acceptors"] = 0;
        }
        try {
            joThisNode["enable-debug-behavior-apis"] =
                joSkaleConfig_nodeInfo["enable-debug-behavior-apis"].get< bool >();
        } catch ( ... ) {
            joThisNode["enable-debug-behavior-apis"] = false;
        }
        try {
            joThisNode["unsafe-transactions"] =
                joSkaleConfig_nodeInfo["unsafe-transactions"].get< bool >();
        } catch ( ... ) {
            joThisNode["unsafe-transactions"] = false;
        }
        //
        try {
            jo["schainID"] = joSkaleConfig_sChain["schainID"].get< int >();
        } catch ( ... ) {
            joThisNode["schainID"] = -1;
        }
        try {
            jo["schainName"] = joSkaleConfig_sChain["schainName"].get< std::string >();
        } catch ( ... ) {
            joThisNode["schainName"] = "";
        }
        nlohmann::json jarrNetwork = nlohmann::json::array();
        size_t i, cnt = joSkaleConfig_sChain_nodes.size();
        for ( i = 0; i < cnt; ++i ) {
            const nlohmann::json& joNC = joSkaleConfig_sChain_nodes[i];
            nlohmann::json joNode = nlohmann::json::object();
            try {
                joNode["nodeID"] = joNC["nodeID"].get< int >();
            } catch ( ... ) {
                joNode["nodeID"] = 1;
            }
            try {
                joNode["ip"] = joNC["ip"].get< std::string >();
            } catch ( ... ) {
                joNode["ip"] = "";
            }
            try {
                joNode["ip6"] = joNC["ip6"].get< std::string >();
            } catch ( ... ) {
                joNode["ip6"] = "";
            }
            try {
                joNode["schainIndex"] = joNC["schainIndex"].get< int >();
            } catch ( ... ) {
                joNode["schainIndex"] = -1;
            }
            try {
                joNode["httpRpcPort"] = joNC["httpRpcPort"].get< int >();
            } catch ( ... ) {
                joNode["httpRpcPort"] = 0;
            }
            try {
                joNode["httpsRpcPort"] = joNC["httpsRpcPort"].get< int >();
            } catch ( ... ) {
                joNode["httpsRpcPort"] = 0;
            }
            try {
                joNode["wsRpcPort"] = joNC["wsRpcPort"].get< int >();
            } catch ( ... ) {
                joNode["wsRpcPort"] = 0;
            }
            try {
                joNode["wssRpcPort"] = joNC["wssRpcPort"].get< int >();
            } catch ( ... ) {
                joNode["wssRpcPort"] = 0;
            }
            try {
                joNode["httpRpcPort6"] = joNC["httpRpcPort6"].get< int >();
            } catch ( ... ) {
                joNode["httpRpcPort6"] = 0;
            }
            try {
                joNode["httpsRpcPort6"] = joNC["httpsRpcPort6"].get< int >();
            } catch ( ... ) {
                joNode["httpsRpcPort6"] = 0;
            }
            try {
                joNode["wsRpcPort6"] = joNC["wsRpcPort6"].get< int >();
            } catch ( ... ) {
                joNode["wsRpcPort6"] = 0;
            }
            try {
                joNode["wssRpcPort6"] = joNC["wssRpcPort6"].get< int >();
            } catch ( ... ) {
                joNode["wssRpcPort6"] = 0;
            }
            //
            jarrNetwork.push_back( joNode );
        }
        //
        jo["node"] = joThisNode;
        jo["network"] = jarrNetwork;
        jo["node"] = joThisNode;
        std::string s = jo.dump();
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ) {
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        throw jsonrpc::JsonRpcException( ex.what() );
    }
}

Json::Value SkaleStats::skale_imaInfo() {
    try {
        nlohmann::json joConfig = getConfigJSON();
        if ( joConfig.count( "skaleConfig" ) == 0 )
            throw std::runtime_error( "error config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        //
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        //
        if ( joSkaleConfig_nodeInfo.count( "wallets" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"/\"wallets\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_wallets = joSkaleConfig_nodeInfo["wallets"];
        //
        if ( joSkaleConfig_nodeInfo_wallets.count( "ima" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"ima\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_wallets_ima =
            joSkaleConfig_nodeInfo_wallets["ima"];
        //
        // validate wallet description
        static const char* g_arrMustHaveWalletFields[] = {"url", "keyShareName", "t", "n",
            "insecureCommonBLSPublicKey0", "insecureCommonBLSPublicKey1",
            "insecureCommonBLSPublicKey2", "insecureCommonBLSPublicKey3"};
        size_t i, cnt =
                      sizeof( g_arrMustHaveWalletFields ) / sizeof( g_arrMustHaveWalletFields[0] );
        for ( i = 0; i < cnt; ++i ) {
            std::string strFieldName = g_arrMustHaveWalletFields[i];
            if ( joSkaleConfig_nodeInfo_wallets_ima.count( strFieldName ) == 0 )
                throw std::runtime_error(
                    "error config.json file, cannot find field "
                    "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"ima\"/" +
                    strFieldName );
            const nlohmann::json& joField = joSkaleConfig_nodeInfo_wallets_ima[strFieldName];
            if ( strFieldName == "t" || strFieldName == "n" ) {
                if ( !joField.is_number() )
                    throw std::runtime_error(
                        "error config.json file, field "
                        "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"ima\"/" +
                        strFieldName + " must be a number" );
                continue;
            }
            if ( !joField.is_string() )
                throw std::runtime_error(
                    "error config.json file, field "
                    "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"ima\"/" +
                    strFieldName + " must be a string" );
        }
        //
        nlohmann::json jo = nlohmann::json::object();
        //
        jo["thisNodeIndex"] = nThisNodeIndex_;  // 1-based "schainIndex"
        //
        jo["t"] = joSkaleConfig_nodeInfo_wallets_ima["t"];
        jo["n"] = joSkaleConfig_nodeInfo_wallets_ima["n"];
        //
        jo["insecureBLSPublicKey0"] = joSkaleConfig_nodeInfo_wallets_ima["insecureBLSPublicKey0"];
        jo["insecureBLSPublicKey1"] = joSkaleConfig_nodeInfo_wallets_ima["insecureBLSPublicKey1"];
        jo["insecureBLSPublicKey2"] = joSkaleConfig_nodeInfo_wallets_ima["insecureBLSPublicKey2"];
        jo["insecureBLSPublicKey3"] = joSkaleConfig_nodeInfo_wallets_ima["insecureBLSPublicKey3"];
        //
        jo["insecureCommonBLSPublicKey0"] =
            joSkaleConfig_nodeInfo_wallets_ima["insecureCommonBLSPublicKey0"];
        jo["insecureCommonBLSPublicKey1"] =
            joSkaleConfig_nodeInfo_wallets_ima["insecureCommonBLSPublicKey1"];
        jo["insecureCommonBLSPublicKey2"] =
            joSkaleConfig_nodeInfo_wallets_ima["insecureCommonBLSPublicKey2"];
        jo["insecureCommonBLSPublicKey3"] =
            joSkaleConfig_nodeInfo_wallets_ima["insecureCommonBLSPublicKey3"];
        //
        std::string s = jo.dump();
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ) {
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        throw jsonrpc::JsonRpcException( ex.what() );
    }
}


Json::Value SkaleStats::skale_imaVerifyAndSign( const Json::Value& request ) {
    std::string strLogPrefix = cc::deep_info( "IMA Verify+Sign" );
    try {
        nlohmann::json joConfig = getConfigJSON();
        Json::FastWriter fastWriter;
        const std::string strRequest = fastWriter.write( request );
        const nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        strLogPrefix = cc::bright( "Startup" ) + " " + cc::deep_info( "IMA Verify+Sign" );
        std::cout << strLogPrefix << cc::debug( " Processing " )
                  << cc::notice( "IMA Verify and Sign" ) << cc::debug( " request: " )
                  << cc::j( joRequest ) << "\n";
        //
        if ( joRequest.count( "direction" ) == 0 )
            throw std::runtime_error( "missing \"messages\"/\"direction\" in call parameters" );
        const nlohmann::json& joDirection = joRequest["direction"];
        if ( !joDirection.is_string() )
            throw std::runtime_error(
                "bad value type of \"messages\"/\"direction\" must be string" );
        const std::string strDirection = skutils::tools::to_upper(
            skutils::tools::trim_copy( joDirection.get< std::string >() ) );
        if ( !( strDirection == "M2S" || strDirection == "S2M" ) )
            throw std::runtime_error(
                "value of \"messages\"/\"direction\" must be \"M2S\" or \"S2M\"" );
        // from now on strLogPrefix includes strDirection
        strLogPrefix = cc::bright( strDirection ) + " " + cc::deep_info( "IMA Verify+Sign" );
        //
        //
        // Extract needed config.json parameters, ensure they are all present and valid
        //
        if ( joConfig.count( "skaleConfig" ) == 0 )
            throw std::runtime_error( "error config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        //
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        //
        //
        if ( joSkaleConfig_nodeInfo.count( "imaMessageProxySChain" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"imaMessageProxySChain\"" );
        const nlohmann::json& joAddressImaMessageProxySChain =
            joSkaleConfig_nodeInfo["imaMessageProxySChain"];
        if ( !joAddressImaMessageProxySChain.is_string() )
            throw std::runtime_error(
                "error config.json file, bad type of value in "
                "\"skaleConfig\"/\"nodeInfo\"/\"imaMessageProxySChain\"" );
        std::string strAddressImaMessageProxySChain =
            joAddressImaMessageProxySChain.get< std::string >();
        if ( strAddressImaMessageProxySChain.empty() )
            throw std::runtime_error(
                "error config.json file, bad empty value in "
                "\"skaleConfig\"/\"nodeInfo\"/\"imaMessageProxySChain\"" );
        std::cout << strLogPrefix << cc::debug( " Using " )
                  << cc::notice( "IMA Message Proxy/S-Chain" )
                  << cc::debug( " contract at address " )
                  << cc::info( strAddressImaMessageProxySChain ) << "\n";
        const std::string strAddressImaMessageProxySChainLC =
            skutils::tools::to_lower( strAddressImaMessageProxySChain );
        //
        //
        if ( joSkaleConfig_nodeInfo.count( "imaMessageProxyMainNet" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"imaMessageProxyMainNet\"" );
        const nlohmann::json& joAddressImaMessageProxyMainNet =
            joSkaleConfig_nodeInfo["imaMessageProxyMainNet"];
        if ( !joAddressImaMessageProxyMainNet.is_string() )
            throw std::runtime_error(
                "error config.json file, bad type of value in "
                "\"skaleConfig\"/\"nodeInfo\"/\"imaMessageProxyMainNet\"" );
        std::string strAddressImaMessageProxyMainNet =
            joAddressImaMessageProxyMainNet.get< std::string >();
        if ( strAddressImaMessageProxyMainNet.empty() )
            throw std::runtime_error(
                "error config.json file, bad empty value in "
                "\"skaleConfig\"/\"nodeInfo\"/\"imaMessageProxyMainNet\"" );
        std::cout << strLogPrefix << cc::debug( " Using " )
                  << cc::notice( "IMA Message Proxy/MainNet" )
                  << cc::debug( " contract at address " )
                  << cc::info( strAddressImaMessageProxyMainNet ) << "\n";
        const std::string strAddressImaMessageProxyMainNetLC =
            skutils::tools::to_lower( strAddressImaMessageProxyMainNet );
        //
        //
        std::string strAddressImaMessageProxy = ( strDirection == "M2S" ) ?
                                                    strAddressImaMessageProxyMainNet :
                                                    strAddressImaMessageProxySChain;
        std::string strAddressImaMessageProxyLC = ( strDirection == "M2S" ) ?
                                                      strAddressImaMessageProxyMainNetLC :
                                                      strAddressImaMessageProxySChainLC;
        //
        //
        if ( joSkaleConfig_nodeInfo.count( "imaMainNet" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"imaMainNet\"" );
        const nlohmann::json& joImaMainNetURL = joSkaleConfig_nodeInfo["imaMainNet"];
        if ( !joImaMainNetURL.is_string() )
            throw std::runtime_error(
                "error config.json file, bad type of value in "
                "\"skaleConfig\"/\"nodeInfo\"/\"imaMainNet\"" );
        std::string strImaMainNetURL = joImaMainNetURL.get< std::string >();
        if ( strImaMainNetURL.empty() )
            throw std::runtime_error(
                "error config.json file, bad empty value in "
                "\"skaleConfig\"/\"nodeInfo\"/\"imaMainNet\"" );
        std::cout << strLogPrefix << cc::debug( " Using " ) << cc::notice( "Main Net URL" )
                  << cc::debug( " " ) << cc::info( strImaMainNetURL ) << "\n";
        skutils::url urlMainNet;
        try {
            urlMainNet = skutils::url( strImaMainNetURL );
            if ( urlMainNet.scheme().empty() || urlMainNet.host().empty() )
                throw std::runtime_error( "bad IMA Main Net url" );
        } catch ( ... ) {
            throw std::runtime_error(
                "error config.json file, bad URL value in "
                "\"skaleConfig\"/\"nodeInfo\"/\"imaMainNet\"" );
        }
        //
        //
        if ( joSkaleConfig_nodeInfo.count( "wallets" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_wallets = joSkaleConfig_nodeInfo["wallets"];
        //
        if ( joSkaleConfig_nodeInfo_wallets.count( "ima" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"ima\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_wallets_ima =
            joSkaleConfig_nodeInfo_wallets["ima"];
        //
        //
        // Extract needed request arguments, ensure they are all present and valid
        //
        if ( joRequest.count( "startMessageIdx" ) == 0 )
            throw std::runtime_error(
                "missing \"messages\"/\"startMessageIdx\" in call parameters" );
        const nlohmann::json& joStartMessageIdx = joRequest["startMessageIdx"];
        if ( !joStartMessageIdx.is_number_unsigned() )
            throw std::runtime_error(
                "bad value type of \"messages\"/\"startMessageIdx\" must be unsigned number" );
        const size_t nStartMessageIdx = joStartMessageIdx.get< size_t >();
        std::cout << strLogPrefix << " "
                  << cc::notice( "Start message index" ) + cc::debug( " is " )
                  << cc::size10( nStartMessageIdx ) << "\n";
        //
        if ( joRequest.count( "srcChainID" ) == 0 )
            throw std::runtime_error( "missing \"messages\"/\"srcChainID\" in call parameters" );
        const nlohmann::json& joSrcChainID = joRequest["srcChainID"];
        if ( !joSrcChainID.is_string() )
            throw std::runtime_error(
                "bad value type of \"messages\"/\"srcChainID\" must be string" );
        const std::string strSrcChainID = joSrcChainID.get< std::string >();
        if ( strSrcChainID.empty() )
            throw std::runtime_error(
                "value of \"messages\"/\"dstChainID\" must be non-empty string" );
        std::cout << strLogPrefix << " " << cc::notice( "Source Chain ID" ) + cc::debug( " is " )
                  << cc::info( strSrcChainID ) << "\n";
        //
        if ( joRequest.count( "dstChainID" ) == 0 )
            throw std::runtime_error( "missing \"messages\"/\"dstChainID\" in call parameters" );
        const nlohmann::json& joDstChainID = joRequest["dstChainID"];
        if ( !joDstChainID.is_string() )
            throw std::runtime_error(
                "bad value type of \"messages\"/\"dstChainID\" must be string" );
        const std::string strDstChainID = joDstChainID.get< std::string >();
        if ( strDstChainID.empty() )
            throw std::runtime_error(
                "value of \"messages\"/\"dstChainID\" must be non-empty string" );
        std::cout << strLogPrefix << " "
                  << cc::notice( "Destination Chain ID" ) + cc::debug( " is " )
                  << cc::info( strDstChainID ) << "\n";
        //
        std::string strDstChainID_hex_32;
        size_t tmp = 0;
        for ( const char& c : strDstChainID ) {
            strDstChainID_hex_32 += skutils::tools::format( "%02x", int( c ) );
            ++tmp;
            if ( tmp == 32 )
                break;
        }
        while ( tmp < 32 ) {
            strDstChainID_hex_32 += "00";
            ++tmp;
        }
        dev::u256 uDestinationChainID_32_max( "0x" + strDstChainID_hex_32 );
        //
        if ( joRequest.count( "messages" ) == 0 )
            throw std::runtime_error( "missing \"messages\" in call parameters" );
        const nlohmann::json& jarrMessags = joRequest["messages"];
        if ( !jarrMessags.is_array() )
            throw std::runtime_error( "parameter \"messages\" must be array" );
        const size_t cntMessagesToSign = jarrMessags.size();
        if ( cntMessagesToSign == 0 )
            throw std::runtime_error(
                "parameter \"messages\" is empty array, nothing to verify and sign" );
        std::cout << strLogPrefix << cc::debug( " Composing summary message to sign from " )
                  << cc::size10( cntMessagesToSign )
                  << cc::debug( " message(s), IMA index of first message is " )
                  << cc::size10( nStartMessageIdx ) << cc::debug( ", src chain id is " )
                  << cc::info( strSrcChainID ) << cc::debug( ", dst chain id is " )
                  << cc::info( strDstChainID ) << cc::debug( "(" )
                  << cc::info( dev::toJS( uDestinationChainID_32_max ) ) << cc::debug( ")..." )
                  << "\n";
        //
        //
        // Perform basic validation of arrived messages we will sign
        //
        for ( size_t idxMessage = 0; idxMessage < cntMessagesToSign; ++idxMessage ) {
            const nlohmann::json& joMessageToSign = jarrMessags[idxMessage];
            if ( !joMessageToSign.is_object() )
                throw std::runtime_error(
                    "parameter \"messages\" must be array containing message objects" );
            // each message in array looks like
            // {
            //     "amount": joValues.amount,
            //     "data": joValues.data,
            //     "destinationContract": joValues.dstContract,
            //     "sender": joValues.srcContract,
            //     "to": joValues.to
            // }
            if ( joMessageToSign.count( "amount" ) == 0 )
                throw std::runtime_error(
                    "parameter \"messages\" contains message object without field \"amount\"" );
            if ( joMessageToSign.count( "data" ) == 0 || ( !joMessageToSign["data"].is_string() ) ||
                 joMessageToSign["data"].get< std::string >().empty() )
                throw std::runtime_error(
                    "parameter \"messages\" contains message object without field \"data\"" );
            if ( joMessageToSign.count( "destinationContract" ) == 0 ||
                 ( !joMessageToSign["destinationContract"].is_string() ) ||
                 joMessageToSign["destinationContract"].get< std::string >().empty() )
                throw std::runtime_error(
                    "parameter \"messages\" contains message object without field "
                    "\"destinationContract\"" );
            if ( joMessageToSign.count( "sender" ) == 0 ||
                 ( !joMessageToSign["sender"].is_string() ) ||
                 joMessageToSign["sender"].get< std::string >().empty() )
                throw std::runtime_error(
                    "parameter \"messages\" contains message object without field \"sender\"" );
            if ( joMessageToSign.count( "to" ) == 0 || ( !joMessageToSign["to"].is_string() ) ||
                 joMessageToSign["to"].get< std::string >().empty() )
                throw std::runtime_error(
                    "parameter \"messages\" contains message object without field \"to\"" );
            const std::string strData = joMessageToSign["data"].get< std::string >();
            if ( strData.empty() )
                throw std::runtime_error(
                    "parameter \"messages\" contains message object with empty field "
                    "\"data\"" );
            if ( joMessageToSign.count( "amount" ) == 0 ||
                 ( !joMessageToSign["amount"].is_string() ) ||
                 joMessageToSign["amount"].get< std::string >().empty() )
                throw std::runtime_error(
                    "parameter \"messages\" contains message object without field \"amount\"" );
        }
        //
        // Check wallet URL and keyShareName for future use,
        // fetch SSL options for SGX
        //
        skutils::url u;
        skutils::http::SSL_client_options optsSSL;
        try {
            const std::string strWalletURL =
                joSkaleConfig_nodeInfo_wallets_ima["url"].get< std::string >();
            if ( strWalletURL.empty() )
                throw std::runtime_error( "empty wallet url" );
            u = skutils::url( strWalletURL );
            if ( u.scheme().empty() || u.host().empty() )
                throw std::runtime_error( "bad wallet url" );
            //
            //
            try {
                optsSSL.ca_file = skutils::tools::trim_copy(
                    joSkaleConfig_nodeInfo_wallets_ima["caFile"].get< std::string >() );
            } catch ( ... ) {
                optsSSL.ca_file.clear();
            }
            std::cout << strLogPrefix << cc::debug( " SGX Wallet CA file " )
                      << cc::info( optsSSL.ca_file ) << "\n";
            try {
                optsSSL.client_cert = skutils::tools::trim_copy(
                    joSkaleConfig_nodeInfo_wallets_ima["certFile"].get< std::string >() );
            } catch ( ... ) {
                optsSSL.client_cert.clear();
            }
            std::cout << strLogPrefix << cc::debug( " SGX Wallet client certificate file " )
                      << cc::info( optsSSL.client_cert ) << "\n";
            try {
                optsSSL.client_key = skutils::tools::trim_copy(
                    joSkaleConfig_nodeInfo_wallets_ima["keyFile"].get< std::string >() );
            } catch ( ... ) {
                optsSSL.client_key.clear();
            }
            std::cout << strLogPrefix << cc::debug( " SGX Wallet client key file " )
                      << cc::info( optsSSL.client_key ) << "\n";
        } catch ( ... ) {
            throw std::runtime_error(
                "error config.json file, cannot find valid value for "
                "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"url\" parameter" );
        }
        const std::string keyShareName =
            joSkaleConfig_nodeInfo_wallets_ima["keyShareName"].get< std::string >();
        if ( keyShareName.empty() )
            throw std::runtime_error(
                "error config.json file, cannot find valid value for "
                "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"keyShareName\" parameter" );
        //
        //
        // Walk through all messages, parse and validate data of each message, then verify each
        // message present in contract events
        //
        dev::bytes vecAllTogetherMessages;
        for ( size_t idxMessage = 0; idxMessage < cntMessagesToSign; ++idxMessage ) {
            const nlohmann::json& joMessageToSign = jarrMessags[idxMessage];
            const std::string strMessageSender =
                skutils::tools::trim_copy( joMessageToSign["sender"].get< std::string >() );
            const std::string strMessageSenderLC =
                skutils::tools::to_lower( skutils::tools::trim_copy( strMessageSender ) );
            const dev::u256 uMessageSender( strMessageSenderLC );
            const std::string strMessageData = joMessageToSign["data"].get< std::string >();
            const std::string strMessageData_linear_LC = skutils::tools::to_lower(
                skutils::tools::trim_copy( skutils::tools::replace_all_copy(
                    strMessageData, std::string( "0x" ), std::string( "" ) ) ) );
            const std::string strDestinationContract = skutils::tools::trim_copy(
                joMessageToSign["destinationContract"].get< std::string >() );
            const dev::u256 uDestinationContract( strDestinationContract );
            const std::string strDestinationAddressTo =
                skutils::tools::trim_copy( joMessageToSign["to"].get< std::string >() );
            const dev::u256 uDestinationAddressTo( strDestinationAddressTo );
            const std::string strMessageAmount = joMessageToSign["amount"].get< std::string >();
            const dev::u256 uMessageAmount( strMessageAmount );
            //
            // here strMessageData must be disassembled and validated
            // it must be valid transfer reference
            //
            std::cout << strLogPrefix << cc::debug( " Verifying message " )
                      << cc::size10( idxMessage ) << cc::debug( " of " )
                      << cc::size10( cntMessagesToSign ) << cc::debug( " with content: " )
                      << cc::info( strMessageData ) << "\n";
            const bytes vecBytes = dev::jsToBytes( strMessageData, dev::OnFailed::Throw );
            const size_t cntMessageBytes = vecBytes.size();
            if ( cntMessageBytes == 0 )
                throw std::runtime_error( "bad empty message data to sign" );
            const _byte_ b0 = vecBytes[0];
            size_t nPos = 1, nFiledSize = 0;
            switch ( b0 ) {
            case 1: {
                // ETH transfer, see
                // https://github.com/skalenetwork/IMA/blob/develop/proxy/contracts/DepositBox.sol
                // Data is:
                // --------------------------------------------------------------
                // Offset | Size     | Description
                // --------------------------------------------------------------
                // 0      | 1        | Value 1
                // --------------------------------------------------------------
                static const char strImaMessageTypeName[] = "ETH";
                std::cout << strLogPrefix << cc::debug( " Verifying " )
                          << cc::sunny( strImaMessageTypeName ) << cc::debug( " transfer..." )
                          << "\n";
                //
            } break;
            case 3: {
                // ERC20 transfer, see source code of encodeData() function here:
                // https://github.com/skalenetwork/IMA/blob/develop/proxy/contracts/ERC20ModuleForMainnet.sol
                // Data is:
                // --------------------------------------------------------------
                // Offset | Size     | Description
                // --------------------------------------------------------------
                // 0      | 1        | Value 3
                // 1      | 32       | contractPosition, address
                // 33     | 32       | to, address
                // 65     | 32       | amount, number
                // 97     | 32       | size of name (next field)
                // 129    | variable | name, string memory
                //        | 32       | size of symbol (next field)
                //        | variable | symbol, string memory
                //        | 1        | decimals, uint8
                //        | 32       | totalSupply, uint
                // --------------------------------------------------------------
                static const char strImaMessageTypeName[] = "ERC20";
                std::cout << strLogPrefix << cc::debug( " Verifying " )
                          << cc::sunny( strImaMessageTypeName ) << cc::debug( " transfer..." )
                          << "\n";
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error( "IMA message to short" );
                const dev::u256 contractPosition =
                    BMPBN::decode< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error( "IMA message to short" );
                const dev::u256 addressTo =
                    BMPBN::decode< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error( "IMA message to short" );
                const dev::u256 amount =
                    BMPBN::decode< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error( "IMA message to short" );
                const dev::u256 sizeOfName =
                    BMPBN::decode< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                nFiledSize = sizeOfName.convert_to< size_t >();
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error( "IMA message to short" );
                std::string strName( "" );
                strName.insert( strName.end(), ( ( char* ) ( vecBytes.data() ) ) + nPos,
                    ( ( char* ) ( vecBytes.data() ) ) + nPos + nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error( "IMA message to short" );
                const dev::u256 sizeOfSymbol =
                    BMPBN::decode< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += 32;
                nFiledSize = sizeOfSymbol.convert_to< size_t >();
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error( "IMA message to short" );
                std::string strSymbol( "" );
                strSymbol.insert( strSymbol.end(), ( ( char* ) ( vecBytes.data() ) ) + nPos,
                    ( ( char* ) ( vecBytes.data() ) ) + nPos + nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 1;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error( "IMA message to short" );
                const uint8_t nDecimals = uint8_t( vecBytes[nPos] );
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error( "IMA message to short" );
                const dev::u256 totalSupply =
                    BMPBN::decode< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                //
                if ( nPos > cntMessageBytes ) {
                    const size_t nExtra = cntMessageBytes - nPos;
                    std::cout << strLogPrefix << cc::warn( " Extra " ) << cc::size10( nExtra )
                              << cc::warn( " unused bytes found in message." ) << "\n";
                }
                std::cout << strLogPrefix << cc::debug( " Extracted " )
                          << cc::sunny( strImaMessageTypeName ) << cc::debug( " data fields:" )
                          << "\n";
                std::cout << "    " << cc::info( "contractPosition" ) << cc::debug( "......." )
                          << cc::info( contractPosition.str() ) << "\n";
                std::cout << "    " << cc::info( "to" ) << cc::debug( "....................." )
                          << cc::info( addressTo.str() ) << "\n";
                std::cout << "    " << cc::info( "amount" ) << cc::debug( "................." )
                          << cc::info( amount.str() ) << "\n";
                std::cout << "    " << cc::info( "name" ) << cc::debug( "..................." )
                          << cc::info( strName ) << "\n";
                std::cout << "    " << cc::info( "symbol" ) << cc::debug( "................." )
                          << cc::info( strSymbol ) << "\n";
                std::cout << "    " << cc::info( "decimals" ) << cc::debug( "..............." )
                          << cc::num10( nDecimals ) << "\n";
                std::cout << "    " << cc::info( "totalSupply" ) << cc::debug( "............" )
                          << cc::info( totalSupply.str() ) << "\n";
            } break;
            case 5: {
                // ERC 721 transfer, see source code of encodeData() function here:
                // https://github.com/skalenetwork/IMA/blob/develop/proxy/contracts/ERC721ModuleForMainnet.sol
                // Data is:
                // --------------------------------------------------------------
                // Offset | Size     | Description
                // --------------------------------------------------------------
                // 0      | 1        | Value 5
                // 1      | 32       | contractPosition, address
                // 33     | 32       | to, address
                // 65     | 32       | tokenId
                // 97     | 32       | size of name (next field)
                // 129    | variable | name, string memory
                //        | 32       | size of symbol (next field)
                //        | variable | symbol, string memory
                // --------------------------------------------------------------
                static const char strImaMessageTypeName[] = "ERC721";
                std::cout << strLogPrefix << cc::debug( " Verifying " )
                          << cc::sunny( strImaMessageTypeName ) << cc::debug( " transfer..." )
                          << "\n";
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error( "IMA message to short" );
                const dev::u256 contractPosition =
                    BMPBN::decode< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error( "IMA message to short" );
                const dev::u256 addressTo =
                    BMPBN::decode< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error( "IMA message to short" );
                const dev::u256 tokenID =
                    BMPBN::decode< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error( "IMA message to short" );
                const dev::u256 sizeOfName =
                    BMPBN::decode< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                nFiledSize = sizeOfName.convert_to< size_t >();
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error( "IMA message to short" );
                std::string strName( "" );
                strName.insert( strName.end(), ( ( char* ) ( vecBytes.data() ) ) + nPos,
                    ( ( char* ) ( vecBytes.data() ) ) + nPos + nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error( "IMA message to short" );
                const dev::u256 sizeOfSymbol =
                    BMPBN::decode< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += 32;
                nFiledSize = sizeOfSymbol.convert_to< size_t >();
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error( "IMA message to short" );
                std::string strSymbol( "" );
                strSymbol.insert( strSymbol.end(), ( ( char* ) ( vecBytes.data() ) ) + nPos,
                    ( ( char* ) ( vecBytes.data() ) ) + nPos + nFiledSize );
                nPos += nFiledSize;
                //
                if ( nPos > cntMessageBytes ) {
                    size_t nExtra = cntMessageBytes - nPos;
                    std::cout << strLogPrefix << cc::warn( " Extra " ) << cc::size10( nExtra )
                              << cc::warn( " unused bytes found in message." ) << "\n";
                }
                std::cout << strLogPrefix << cc::debug( " Extracted " )
                          << cc::sunny( strImaMessageTypeName ) << cc::debug( " data fields:" )
                          << "\n";
                std::cout << "    " << cc::info( "contractPosition" ) << cc::debug( "......." )
                          << cc::info( contractPosition.str() ) << "\n";
                std::cout << "    " << cc::info( "to" ) << cc::debug( "....................." )
                          << cc::info( addressTo.str() ) << "\n";
                std::cout << "    " << cc::info( "tokenID" ) << cc::debug( "................" )
                          << cc::info( tokenID.str() ) << "\n";
                std::cout << "    " << cc::info( "name" ) << cc::debug( "..................." )
                          << cc::info( strName ) << "\n";
                std::cout << "    " << cc::info( "symbol" ) << cc::debug( "................." )
                          << cc::info( strSymbol ) << "\n";
            } break;
            default: {
                std::cout << strLogPrefix << " " << cc::fatal( " UNKNOWN IMA MESSAGE: " )
                          << cc::error( " Message code is " ) << cc::num10( b0 )
                          << cc::error( ", message binary data is:\n" )
                          << cc::binary_table( ( void* ) vecBytes.data(), vecBytes.size() ) << "\n";
                throw std::runtime_error( "bad IMA message type " + std::to_string( b0 ) );
            } break;
            }  // switch( b0 )
            //
            //
            static const std::string strSignature_event_OutgoingMessage(
                "OutgoingMessage(string,bytes32,uint256,address,address,address,uint256,bytes,"
                "uint256)" );
            static const std::string strTopic_event_OutgoingMessage =
                dev::toJS( dev::sha3( strSignature_event_OutgoingMessage ) );
            static const dev::u256 uTopic_event_OutgoingMessage( strTopic_event_OutgoingMessage );
            //
            const std::string strTopic_dstChainHash = dev::toJS( dev::sha3( strDstChainID ) );
            const dev::u256 uTopic_dstChainHash( strTopic_dstChainHash );
            static const size_t nPaddoingZeroesForUint256 = 64;
            const std::string strTopic_msgCounter = dev::BMPBN::toHexStringWithPadding< dev::u256 >(
                dev::u256( nStartMessageIdx + idxMessage ), nPaddoingZeroesForUint256 );
            const dev::u256 uTopic_msgCounter( strTopic_msgCounter );
            nlohmann::json jarrTopic_dstChainHash = nlohmann::json::array();
            nlohmann::json jarrTopic_msgCounter = nlohmann::json::array();
            jarrTopic_dstChainHash.push_back( strTopic_dstChainHash );
            jarrTopic_msgCounter.push_back( strTopic_msgCounter );
            //
            //
            //
            // Forming eth_getLogs query similar to web3's getPastEvents, see details here:
            // https://solidity.readthedocs.io/en/v0.4.24/abi-spec.html
            // Here is example
            // {
            //    "address": "0x4c6ad417e3bf7f3d623bab87f29e119ef0f28059",
            //    "fromBlock": "0x0",
            //    "toBlock": "latest",
            //    "topics":
            //    ["0xa701ebe76260cb49bb2dc03cf8cf6dacbc4c59a5d615c4db34a7dfdf36e6b6dc",
            //    ["0x8d646f556e5d9d6f1edcf7a39b77f5ac253776eb34efcfd688aacbee518efc26"],
            //    ["0x0000000000000000000000000000000000000000000000000000000000000010"], null
            //    ]
            // }
            //
            nlohmann::json jarrTopics = nlohmann::json::array();
            jarrTopics.push_back( strTopic_event_OutgoingMessage );
            jarrTopics.push_back( jarrTopic_dstChainHash );
            jarrTopics.push_back( jarrTopic_msgCounter );
            jarrTopics.push_back( nullptr );
            nlohmann::json joLogsQuery = nlohmann::json::object();
            joLogsQuery["address"] = strAddressImaMessageProxy;
            joLogsQuery["fromBlock"] = "0x0";
            joLogsQuery["toBlock"] = "latest";
            joLogsQuery["topics"] = jarrTopics;
            std::cout << strLogPrefix << cc::debug( " Will execute logs search query: " )
                      << cc::j( joLogsQuery ) << "\n";
            //
            //
            //
            nlohmann::json jarrFoundLogRecords;
            if ( strDirection == "M2S" ) {
                nlohmann::json joCall = nlohmann::json::object();
                joCall["jsonrpc"] = "2.0";
                joCall["method"] = "eth_getLogs";
                joCall["params"] = joLogsQuery;
                skutils::rest::client cli( urlMainNet );
                skutils::rest::data_t d = cli.call( joCall );
                if ( d.empty() )
                    throw std::runtime_error( "Main Net call to eth_getLogs failed" );
                jarrFoundLogRecords = nlohmann::json::parse( d.s_ )["result"];
            } else {
                Json::Value jvLogsQuery;
                Json::Reader().parse( joLogsQuery.dump(), jvLogsQuery );
                Json::Value jvLogs = dev::toJson(
                    this->client()->logs( toLogFilter( jvLogsQuery, *this->client() ) ) );
                jarrFoundLogRecords = nlohmann::json::parse( Json::FastWriter().write( jvLogs ) );
            }  // else from if( strDirection == "M2S" )
            std::cout << strLogPrefix << cc::debug( " Got logs search query result: " )
                      << cc::j( jarrFoundLogRecords ) << "\n";
            /* exammple of jarrFoundLogRecords value:
                [{
                    "address": "0x4c6ad417e3bf7f3d623bab87f29e119ef0f28059",

                    "blockHash":
               "0x4bcb4bba159b42d1d3dd896a563ca426140fe9d5d1b4e0ed8f3472a681b0f5ea",

                    "blockNumber": 82640,

                    "data":
               "0x00000000000000000000000000000000000000000000000000000000000000c000000000000000000000000088a5edcf315599ade5b6b4cc0991a23bf9e88f650000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f0000000000000000000000000000000000000000000000000de0b6b3a76400000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000000000000074d61696e6e65740000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010100000000000000000000000000000000000000000000000000000000000000",

                    "logIndex": 0,

                    "polarity": true,

                    "topics":
               ["0xa701ebe76260cb49bb2dc03cf8cf6dacbc4c59a5d615c4db34a7dfdf36e6b6dc",
               "0x8d646f556e5d9d6f1edcf7a39b77f5ac253776eb34efcfd688aacbee518efc26",
               "0x0000000000000000000000000000000000000000000000000000000000000000",
               "0x000000000000000000000000c2fe505c79c82bb8cef48709816480ff6e1e0379"],

                    "transactionHash":
               "0x8013af1333055df9f291a58d2da58c912b5326972b1c981b73b854625e904c91",

                    "transactionIndex": 0,

                    "type": "mined"
                }]            */
            bool bIsVerified = false;
            if ( jarrFoundLogRecords.is_array() && jarrFoundLogRecords.size() > 0 )
                bIsVerified = true;
            if ( !bIsVerified )
                throw std::runtime_error( "IMA message " +
                                          std::to_string( nStartMessageIdx + idxMessage ) +
                                          " verification failed - not found in logs" );
            //
            //
            // Find transaction, simlar to call tp eth_getTransactionByHash
            //
            bool bTransactionWasFound = false;
            size_t idxFoundLogRecord = 0, cntFoundLogRecords = jarrFoundLogRecords.size();
            for ( idxFoundLogRecord = 0; idxFoundLogRecord < cntFoundLogRecords;
                  ++idxFoundLogRecord ) {
                const nlohmann::json& joFoundLogRecord = jarrFoundLogRecords[idxFoundLogRecord];
                if ( joFoundLogRecord.count( "transactionHash" ) == 0 )
                    continue;  // bad log record??? this should never happen
                const nlohmann::json& joTransactionHash = joFoundLogRecord["transactionHash"];
                if ( !joTransactionHash.is_string() )
                    continue;  // bad log record??? this should never happen
                const std::string strTransactionHash = joTransactionHash.get< std::string >();
                if ( strTransactionHash.empty() )
                    continue;  // bad log record??? this should never happen
                std::cout << strLogPrefix << cc::debug( " Analyzing transaction " )
                          << cc::notice( strTransactionHash ) << cc::debug( "..." ) << "\n";
                nlohmann::json joTransaction;
                try {
                    if ( strDirection == "M2S" ) {
                        nlohmann::json jarrParams = nlohmann::json::array();
                        jarrParams.push_back( strTransactionHash );
                        nlohmann::json joCall = nlohmann::json::object();
                        joCall["jsonrpc"] = "2.0";
                        joCall["method"] = "eth_getTransactionByHash";
                        joCall["params"] = jarrParams;
                        skutils::rest::client cli( urlMainNet );
                        skutils::rest::data_t d = cli.call( joCall );
                        if ( d.empty() )
                            throw std::runtime_error(
                                "Main Net call to eth_getTransactionByHash failed" );
                        joTransaction = nlohmann::json::parse( d.s_ )["result"];
                    } else {
                        Json::Value jvTransaction;
                        h256 h = dev::jsToFixed< 32 >( strTransactionHash );
                        if ( !this->client()->isKnownTransaction( h ) )
                            jvTransaction = Json::Value( Json::nullValue );
                        else
                            jvTransaction = toJson( this->client()->localisedTransaction( h ) );
                        joTransaction =
                            nlohmann::json::parse( Json::FastWriter().write( jvTransaction ) );
                    }  // else from if ( strDirection == "M2S" )
                } catch ( const std::exception& ex ) {
                    std::cout << strLogPrefix << " " << cc::fatal( "FATAL:" )
                              << cc::error( " Transaction verification failed: " )
                              << cc::warn( ex.what() ) << "\n";
                    continue;
                } catch ( ... ) {
                    std::cout << strLogPrefix << " " << cc::fatal( "FATAL:" )
                              << cc::error( " Transaction verification failed: " )
                              << cc::warn( "unknown exception" ) << "\n";
                    continue;
                }
                std::cout << strLogPrefix << cc::debug( " Reviewing transaction:" )
                          << cc::j( joTransaction ) << cc::debug( "..." ) << "\n";
                // extract "to" address from transaction, then compare it with "sender" from IMA
                // message
                const std::string strTransactionTo = skutils::tools::trim_copy(
                    ( joTransaction.count( "to" ) > 0 && joTransaction["to"].is_string() ) ?
                        joTransaction["to"].get< std::string >() :
                        "" );
                if ( strTransactionTo.empty() )
                    continue;
                const std::string strTransactionTorLC =
                    skutils::tools::to_lower( strTransactionTo );
                if ( strMessageSenderLC != strTransactionTorLC ) {
                    std::cout << strLogPrefix << cc::debug( " Skipping transaction " )
                              << cc::notice( strTransactionHash ) << cc::debug( " because " )
                              << cc::warn( "to" ) << cc::debug( "=" )
                              << cc::notice( strTransactionTo )
                              << cc::debug( " is different than " )
                              << cc::warn( "IMA message sender" ) << cc::debug( "=" )
                              << cc::notice( strMessageSender ) << "\n";
                    continue;
                }
                //
                //
                // Find more transaction details, simlar to call tp eth_getTransactionReceipt
                //
                /* Receipt should look like:
                    {
                        "blockHash":
                   "0x995cb104795b28c16f3be075fbf08afd69753a6c1b16df3758e570342fd3dadf",
                        "blockNumber": 115508,
                        "contractAddress": "0x1bbde22a5d43d59883c1befd474eff2ec51519d2",
                        "cumulativeGasUsed": "0xf055",
                        "gasUsed": "0xf055",
                        "logs": [{
                            "address": "0xfd02fc34219dc1dc923127062543c9522373d895",
                            "blockHash":
                   "0x995cb104795b28c16f3be075fbf08afd69753a6c1b16df3758e570342fd3dadf",
                            "blockNumber": 115508,
                            "data":
                   "0x0000000000000000000000000000000000000000000000000de0b6b3a7640000",
                   "logIndex": 0, "polarity": false, "topics":
                   ["0xddf252ad1be2c89b69c2b068fc378daa952ba7f163c4a11628f55a4df523b3ef",
                   "0x00000000000000000000000066c5a87f4a49dd75e970055a265e8dd5c3f8f852",
                   "0x0000000000000000000000000000000000000000000000000000000000000000"],
                            "transactionHash":
                   "0xab241b07a2b7a8a59aafb5e25fdc5750a8c195ee42b3503e65ff737c514dde71",
                            "transactionIndex": 0,
                            "type": "mined"
                        }, {
                            "address": "0x4c6ad417e3bf7f3d623bab87f29e119ef0f28059",
                            "blockHash":
                   "0x995cb104795b28c16f3be075fbf08afd69753a6c1b16df3758e570342fd3dadf",
                            "blockNumber": 115508,
                            "data":
                   "0x00000000000000000000000000000000000000000000000000000000000000c000000000000000000000000088a5edcf315599ade5b6b4cc0991a23bf9e88f650000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f0000000000000000000000000000000000000000000000000de0b6b3a76400000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000000000000074d61696e6e65740000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010100000000000000000000000000000000000000000000000000000000000000",
                            "logIndex": 1,
                            "polarity": false,
                            "topics":
                   ["0xa701ebe76260cb49bb2dc03cf8cf6dacbc4c59a5d615c4db34a7dfdf36e6b6dc",
                   "0x8d646f556e5d9d6f1edcf7a39b77f5ac253776eb34efcfd688aacbee518efc26",
                   "0x0000000000000000000000000000000000000000000000000000000000000021",
                   "0x000000000000000000000000c2fe505c79c82bb8cef48709816480ff6e1e0379"],
                            "transactionHash":
                   "0xab241b07a2b7a8a59aafb5e25fdc5750a8c195ee42b3503e65ff737c514dde71",
                            "transactionIndex": 0,
                            "type": "mined"
                        }],
                        "logsBloom":
                   "0x00000000000000000000000000200000000000000000000000000800000000020000000000000000000000000400000000000000000000000000000000100000000000000000000000000008000000000000000000000000000000000000020000000400020000400000000000000800000000000000000000000010040000000000000000000000000000000000000000000000000040000000000000000000000040000080000000000000000000000000001800000000200000000000000000000002000000000800000000000000000000000000000000020000200020000000000000000004000000000000000000000000000000003000000000000000",
                        "status": "1",
                        "transactionHash":
                   "0xab241b07a2b7a8a59aafb5e25fdc5750a8c195ee42b3503e65ff737c514dde71",
                        "transactionIndex": 0
                    }

                    The last log record in receipt abaove contains "topics" and "data" field we
                   should verify by comparing fields of IMA message
                */
                //
                nlohmann::json joTransactionReceipt;
                try {
                    if ( strDirection == "M2S" ) {
                        nlohmann::json jarrParams = nlohmann::json::array();
                        jarrParams.push_back( strTransactionHash );
                        nlohmann::json joCall = nlohmann::json::object();
                        joCall["jsonrpc"] = "2.0";
                        joCall["method"] = "eth_getTransactionReceipt";
                        joCall["params"] = jarrParams;
                        skutils::rest::client cli( urlMainNet );
                        skutils::rest::data_t d = cli.call( joCall );
                        if ( d.empty() )
                            throw std::runtime_error(
                                "Main Net call to eth_getTransactionReceipt failed" );
                        joTransactionReceipt = nlohmann::json::parse( d.s_ )["result"];
                    } else {
                        Json::Value jvTransactionReceipt;
                        const h256 h = dev::jsToFixed< 32 >( strTransactionHash );
                        if ( !this->client()->isKnownTransaction( h ) )
                            jvTransactionReceipt = Json::Value( Json::nullValue );
                        else
                            jvTransactionReceipt = dev::eth::toJson(
                                this->client()->localisedTransactionReceipt( h ) );
                        joTransactionReceipt = nlohmann::json::parse(
                            Json::FastWriter().write( jvTransactionReceipt ) );
                    }  // else from if ( strDirection == "M2S" )
                } catch ( const std::exception& ex ) {
                    std::cout << strLogPrefix << " " << cc::fatal( "FATAL:" )
                              << cc::error( " Receipt verification failed: " )
                              << cc::warn( ex.what() ) << "\n";
                    continue;
                } catch ( ... ) {
                    std::cout << strLogPrefix << " " << cc::fatal( "FATAL:" )
                              << cc::error( " Receipt verification failed: " )
                              << cc::warn( "unknown exception" ) << "\n";
                    continue;
                }
                std::cout << strLogPrefix << cc::debug( " Reviewing transaction receipt:" )
                          << cc::j( joTransactionReceipt ) << cc::debug( "..." ) << "\n";
                if ( joTransactionReceipt.count( "logs" ) == 0 )
                    continue;  // ???
                const nlohmann::json& jarrLogsReceipt = joTransactionReceipt["logs"];
                if ( !jarrLogsReceipt.is_array() )
                    continue;  // ???
                bool bReceiptVerified = false;
                size_t idxReceiptLogRecord = 0, cntReceiptLogRecords = jarrLogsReceipt.size();
                for ( idxReceiptLogRecord = 0; idxReceiptLogRecord < cntReceiptLogRecords;
                      ++idxReceiptLogRecord ) {
                    const nlohmann::json& joReceiptLogRecord = jarrLogsReceipt[idxReceiptLogRecord];
                    if ( joReceiptLogRecord.count( "address" ) == 0 ||
                         ( !joReceiptLogRecord["address"].is_string() ) )
                        continue;
                    const std::string strReceiptLogRecord =
                        joReceiptLogRecord["address"].get< std::string >();
                    if ( strReceiptLogRecord.empty() )
                        continue;
                    const std::string strReceiptLogRecordLC =
                        skutils::tools::to_lower( strReceiptLogRecord );
                    if ( strAddressImaMessageProxyLC != strReceiptLogRecordLC )
                        continue;
                    //
                    // find needed entries in "topics"
                    if ( joReceiptLogRecord.count( "topics" ) == 0 ||
                         ( !joReceiptLogRecord["topics"].is_array() ) )
                        continue;
                    bool bTopicSignatureFound = false, bTopicMsgCounterFound = false,
                         bTopicDstChainHashFound = false;
                    const nlohmann::json& jarrReceiptTopics = joReceiptLogRecord["topics"];
                    size_t idxReceiptTopic = 0, cntReceiptTopics = jarrReceiptTopics.size();
                    for ( idxReceiptTopic = 0; idxReceiptTopic < cntReceiptTopics;
                          ++idxReceiptTopic ) {
                        const nlohmann::json& joReceiptTopic = jarrReceiptTopics[idxReceiptTopic];
                        if ( !joReceiptTopic.is_string() )
                            continue;
                        const dev::u256 uTopic( joReceiptTopic.get< std::string >() );
                        if ( uTopic == uTopic_event_OutgoingMessage )
                            bTopicSignatureFound = true;
                        if ( uTopic == uTopic_msgCounter )
                            bTopicMsgCounterFound = true;
                        if ( uTopic == uTopic_dstChainHash )
                            bTopicDstChainHashFound = true;
                    }
                    if ( !( bTopicSignatureFound && bTopicMsgCounterFound &&
                             bTopicDstChainHashFound ) )
                        continue;
                    //
                    // analyze "data"
                    if ( joReceiptLogRecord.count( "data" ) == 0 ||
                         ( !joReceiptLogRecord["data"].is_string() ) )
                        continue;
                    const std::string strData = joReceiptLogRecord["data"].get< std::string >();
                    if ( strData.empty() )
                        continue;
                    const std::string strDataLC_linear = skutils::tools::trim_copy(
                        skutils::tools::replace_all_copy( skutils::tools::to_lower( strData ),
                            std::string( "0x" ), std::string( "" ) ) );
                    const size_t nDataLength = strDataLC_linear.size();
                    if ( strDataLC_linear.find( strMessageData_linear_LC ) == std::string::npos )
                        continue;  // no IMA messahe data
                    // std::set< std::string > setChunksLC;
                    std::set< dev::u256 > setChunksU256;
                    static const size_t nChunkSize = 64;
                    const size_t cntChunks = nDataLength / nChunkSize +
                                             ( ( ( nDataLength % nChunkSize ) != 0 ) ? 1 : 0 );
                    for ( size_t idxChunk = 0; idxChunk < cntChunks; ++idxChunk ) {
                        const size_t nChunkStart = idxChunk * nChunkSize;
                        size_t nChunkEnd = nChunkStart + nChunkSize;
                        if ( nChunkEnd > nDataLength )
                            nChunkEnd = nDataLength;
                        const size_t nChunkSize = nChunkEnd - nChunkStart;
                        const std::string strChunk =
                            strDataLC_linear.substr( nChunkStart, nChunkSize );
                        std::cout << strLogPrefix << cc::debug( "    chunk " )
                                  << cc::info( strChunk ) << "\n";
                        try {
                            const dev::u256 uChunk( "0x" + strChunk );
                            // setChunksLC.insert( strChunk );
                            setChunksU256.insert( uChunk );
                        } catch ( ... ) {
                            std::cout << strLogPrefix << cc::debug( "            skipped chunk " )
                                      << "\n";
                            continue;
                        }
                    }
                    if ( setChunksU256.find( uDestinationContract ) == setChunksU256.end() )
                        continue;
                    if ( setChunksU256.find( uDestinationAddressTo ) == setChunksU256.end() )
                        continue;
                    if ( setChunksU256.find( uMessageAmount ) == setChunksU256.end() )
                        continue;
                    if ( setChunksU256.find( uDestinationChainID_32_max ) == setChunksU256.end() )
                        continue;
                    //
                    bReceiptVerified = true;
                    break;
                }
                if ( !bReceiptVerified ) {
                    std::cout << strLogPrefix << cc::debug( " Skipping transaction " )
                              << cc::notice( strTransactionHash )
                              << cc::debug( " because no appropriate receipt was found" ) << "\n";
                    continue;
                }
                //
                //
                //
                std::cout << strLogPrefix << cc::success( " Found transaction for IMA message " )
                          << cc::size10( nStartMessageIdx + idxMessage ) << cc::success( ": " )
                          << cc::j( joTransaction ) << "\n";
                bTransactionWasFound = true;
                break;
            }
            if ( !bTransactionWasFound ) {
                std::cout << strLogPrefix << " "
                          << cc::error( "No transaction was found for IMA message " )
                          << cc::size10( nStartMessageIdx + idxMessage ) << cc::error( "." )
                          << "\n";
                throw std::runtime_error( "No transaction was found for IMA message " +
                                          std::to_string( nStartMessageIdx + idxMessage ) );
            }
            //
            //
            // One more message is valid, concatenate it for furthes in-wallet signing
            //
            //
            //
            //
            //
            //
            //
            //
            //
            //
            //
            std::cout << strLogPrefix << cc::success( " Success, IMA message " )
                      << cc::size10( nStartMessageIdx + idxMessage )
                      << cc::success( " was found in logs." ) << "\n";
            //
            // compose message to sign
            //
            static auto fnInvert = []( uint8_t* arr, size_t cnt ) -> void {
                size_t n = cnt / 2;
                for ( size_t i = 0; i < n; ++i ) {
                    uint8_t b1 = arr[i];
                    uint8_t b2 = arr[cnt - i - 1];
                    arr[i] = b2;
                    arr[cnt - i - 1] = b1;
                }
            };
            static auto fnAlignRight = []( bytes& v, size_t cnt ) -> void {
                while ( v.size() < cnt )
                    v.push_back( 0 );
            };
            uint8_t arr[32];
            bytes v;
            const size_t cntArr = sizeof( arr ) / sizeof( arr[0] );
            //
            v = dev::BMPBN::encode2vec< dev::u256 >( uMessageSender, true );
            fnAlignRight( v, 32 );
            vecAllTogetherMessages.insert( vecAllTogetherMessages.end(), v.begin(), v.end() );
            //
            v = dev::BMPBN::encode2vec< dev::u256 >( uDestinationContract, true );
            fnAlignRight( v, 32 );
            vecAllTogetherMessages.insert( vecAllTogetherMessages.end(), v.begin(), v.end() );
            //
            v = dev::BMPBN::encode2vec< dev::u256 >( uDestinationAddressTo, true );
            fnAlignRight( v, 32 );
            vecAllTogetherMessages.insert( vecAllTogetherMessages.end(), v.begin(), v.end() );
            //
            dev::BMPBN::encode< dev::u256 >( uMessageAmount, arr, cntArr );
            fnInvert( arr, cntArr );
            vecAllTogetherMessages.insert( vecAllTogetherMessages.end(), arr + 0, arr + cntArr );
            //
            v = dev::fromHex( strMessageData, dev::WhenError::DontThrow );
            fnInvert( v.data(), v.size() );
            vecAllTogetherMessages.insert( vecAllTogetherMessages.end(), v.begin(), v.end() );
        }
        //
        //
        const dev::h256 h = dev::sha3( vecAllTogetherMessages );
        const std::string sh = h.hex();
        std::cout << strLogPrefix << cc::debug( " Got hash to sign " ) << cc::info( sh ) << "\n";
        //        //
        //        // G1 helper
        //        //
        //        // std::pair<libff::alt_bn128_G1, std::string> HashtoG1withHint(std::shared_ptr<
        //        std::array<
        //        // uint8_t, 32>>);
        //        std::array< uint8_t, 32 > tmpArr;
        //        std::shared_ptr< std::array< uint8_t, 32 > > pHashData =
        //            std::make_shared< std::array< uint8_t, 32 > >( tmpArr );
        //        dev::u256 uh( "0x" + sh );
        //        std::cout << strLogPrefix << cc::debug( " Got U of hash to sign " )
        //                  << cc::info( dev::toJS( uh ) ) << "\n";
        //        dev::BMPBN::encode( uh, pHashData->data(), 32 );
        //        std::cout << strLogPrefix << cc::debug( " Got U of hash to sign " )
        //                  << cc::binary_singleline( ( void* ) pHashData->data(), 32, "," ) <<
        //                  "\n";
        //        //
        //        auto t = joSkaleConfig_nodeInfo_wallets_ima["t"].get< int >();
        //        std::cout << strLogPrefix << cc::debug( " Got  " ) << cc::info( "t" ) <<
        //        cc::debug( "=" )
        //                  << cc::num10( t ) << "\n";
        //        auto n = joSkaleConfig_nodeInfo_wallets_ima["n"].get< int >();
        //        std::cout << strLogPrefix << cc::debug( " Got  " ) << cc::info( "n" ) <<
        //        cc::debug( "=" )
        //                  << cc::num10( n ) << "\n";
        //        signatures::Bls aBls( t, n );
        //        std::cout << strLogPrefix << cc::debug( " BLS instance constructed" ) << "\n";
        //        ////////////////////////////std::pair< libff::alt_bn128_G1, std::string > p2vals =
        //        /// aBls.HashtoG1withHint( pHashData );
        //        std::pair< libff::alt_bn128_G1, std::string > p2vals;
        //        std::cout << strLogPrefix << cc::debug( " G1 computation passed" ) << "\n";
        //        std::string str_G1_X = BLSutils::ConvertToString< libff::alt_bn128_Fq >(
        //        p2vals.first.X ); std::string str_G1_Y = BLSutils::ConvertToString<
        //        libff::alt_bn128_Fq >( p2vals.first.Y ); std::cout << strLogPrefix << cc::debug( "
        //        Got G1 point with " ) << cc::info( "X" )
        //                  << cc::debug( "=" ) << cc::info( str_G1_X ) << cc::debug( ", " )
        //                  << cc::info( "Y" ) << cc::debug( "=" ) << cc::info( str_G1_Y )
        //                  << cc::debug( ", " ) << cc::info( "hint" ) << cc::debug( "=" )
        //                  << cc::info( p2vals.second ) << "\n";
        //

        //
        // If we are here, then all IMA messages are valid
        // Perform call to wallet to sign messages
        //
        std::cout << strLogPrefix << cc::debug( " Calling wallet to sign " ) << cc::notice( sh )
                  << cc::debug( " composed from " )
                  << cc::binary_singleline( ( void* ) vecAllTogetherMessages.data(),
                         vecAllTogetherMessages.size(), "" )
                  << cc::debug( "...`" ) << "\n";
        //
        nlohmann::json jo = nlohmann::json::object();
        //
        nlohmann::json joCall = nlohmann::json::object();
        joCall["jsonrpc"] = "2.0";
        joCall["method"] = "blsSignMessageHash";
        joCall["params"] = nlohmann::json::object();
        joCall["params"]["keyShareName"] = keyShareName;
        joCall["params"]["messageHash"] = sh;
        joCall["params"]["n"] = joSkaleConfig_nodeInfo_wallets_ima["n"];
        joCall["params"]["t"] = joSkaleConfig_nodeInfo_wallets_ima["t"];
        joCall["params"]["signerIndex"] = nThisNodeIndex_;  // 1-based
        std::cout << strLogPrefix << cc::debug( " Contacting " ) << cc::notice( "SGX Wallet" )
                  << cc::debug( " server at " ) << cc::u( u ) << "\n";
        std::cout << strLogPrefix << cc::debug( " Will send " ) << cc::notice( "sign query" )
                  << cc::debug( " to wallet: " ) << cc::j( joCall ) << "\n";
        skutils::rest::client cli;
        cli.optsSSL = optsSSL;
        cli.open( u );
        skutils::rest::data_t d = cli.call( joCall );
        if ( d.empty() )
            throw std::runtime_error( "failed to sign message(s) with wallet" );
        nlohmann::json joSignResult = nlohmann::json::parse( d.s_ )["result"];
        jo["signResult"] = joSignResult;
        //
        // Done, provide result to caller
        //
        std::string s = jo.dump();
        std::cout << strLogPrefix << cc::success( " Success, got " ) << cc::notice( "sign result" )
                  << cc::success( " from wallet: " ) << cc::j( joSignResult ) << "\n";
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        std::cout << strLogPrefix << " " << cc::fatal( "FATAL:" )
                  << cc::error( " Exception while processing " )
                  << cc::info( "IMA Verify and Sign" ) << cc::error( " request: " )
                  << cc::warn( ex.what() ) << "\n";
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        std::cout << strLogPrefix << " " << cc::fatal( "FATAL:" )
                  << cc::error( " Exception while processing " )
                  << cc::info( "IMA Verify and Sign" ) << cc::error( " request: " )
                  << cc::warn( ex.what() ) << "\n";
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        std::cout << strLogPrefix << " " << cc::fatal( "FATAL:" )
                  << cc::error( " Exception while processing " )
                  << cc::info( "IMA Verify and Sign" ) << cc::error( " request: " )
                  << cc::warn( "unknown exception" ) << "\n";
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

};  // namespace rpc
};  // namespace dev
