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
/** @file SkaleStats.cpp
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
#include <libdevcore/FileSystem.h>

#include <skutils/console_colors.h>
#include <skutils/eth_utils.h>
#include <skutils/rest_call.h>
#include <skutils/task_performance.h>
#include <skutils/url.h>

#include <algorithm>
#include <array>
#include <csignal>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>

#include <stdio.h>
#include <time.h>

//#include "../libconsensus/libBLS/bls/bls.h"

#include <bls/bls.h>

#include <skutils/console_colors.h>
#include <skutils/utils.h>

#include <libconsensus/SkaleCommon.h>
#include <libconsensus/crypto/OpenSSLECDSAKey.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace dev {

static dev::u256 stat_str2u256( const std::string& saIn ) {
    std::string sa;
    if ( !( saIn.length() > 2 && saIn[0] == '0' && ( saIn[1] == 'x' || saIn[1] == 'X' ) ) )
        sa = "0x" + saIn;
    else
        sa = saIn;
    dev::u256 u( sa.c_str() );
    return u;
}

static nlohmann::json stat_parse_json_with_error_conversion(
    const std::string& s, bool isThrowException = false ) {
    nlohmann::json joAnswer;
    std::string strError;
    try {
        joAnswer = nlohmann::json::parse( s );
        return joAnswer;
    } catch ( std::exception const& ex ) {
        strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
    } catch ( ... ) {
        strError = "unknown exception";
    }
    if ( strError.empty() )
        strError = "unknown error";
    std::string strErrorDescription =
        "JSON parser error \"" + strError + "\" while parsing JSON text \"" + s + "\"";
    if ( isThrowException )
        throw std::runtime_error( strErrorDescription );
    joAnswer = nlohmann::json::object();
    joAnswer["error"] = strErrorDescription;
    return joAnswer;
}

static bool stat_trim_func_with_quotes( unsigned char ch ) {
    return skutils::tools::default_trim_what( ch ) || ch == '\"' || ch == '\'';
}

static void stat_check_rpc_call_error_and_throw(
    const nlohmann::json& joAnswer, const std::string& strMethodName ) {
    if ( joAnswer.count( "error" ) > 0 ) {
        std::string strError = joAnswer["error"].dump();
        strError = skutils::tools::trim_copy( strError, stat_trim_func_with_quotes );
        if ( !strError.empty() )
            throw std::runtime_error(
                "Got \"" + strMethodName + "\" call error \"" + strError + "\"" );
    }
    if ( joAnswer.count( "errorMessage" ) > 0 ) {
        std::string strError = joAnswer["errorMessage"].dump();
        strError = skutils::tools::trim_copy( strError, stat_trim_func_with_quotes );
        if ( !strError.empty() )
            throw std::runtime_error(
                "Got \"" + strMethodName + "\" call error \"" + strError + "\"" );
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace rpc {

SkaleStats::SkaleStats(
    const std::string& configPath, eth::Interface& _eth, const dev::eth::ChainParams& chainParams )
    : skutils::json_config_file_accessor( configPath ), chainParams_( chainParams ), m_eth( _eth ) {
    nThisNodeIndex_ = findThisNodeIndex();
    //
    try {
        skutils::url urlMainNet = getImaMainNetURL();
    } catch ( const std::exception& ex ) {
        clog( VerbosityInfo, std::string( "IMA disabled: " ) + ex.what() );
    }  // catch
}

int SkaleStats::findThisNodeIndex() {
    try {
        nlohmann::json joConfig = getConfigJSON();
        if ( joConfig.count( "skaleConfig" ) == 0 )
            throw std::runtime_error( "error in config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        //
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        //
        if ( joSkaleConfig.count( "sChain" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"sChain\"" );
        const nlohmann::json& joSkaleConfig_sChain = joSkaleConfig["sChain"];
        //
        if ( joSkaleConfig_sChain.count( "nodes" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"sChain\"/\"nodes\"" );
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

        const dev::eth::Client* c = dynamic_cast< dev::eth::Client* const >( client() );
        if ( c ) {
            nlohmann::json joTrace;
            std::shared_ptr< SkaleHost > h = c->skaleHost();

            if (!h) {
                throw jsonrpc::JsonRpcException("No skaleHost in client()");
            }

            std::istringstream list( h->getDebugHandler()( "trace list" ) );
            std::string key;
            while ( list >> key ) {
                std::string count_str = h->getDebugHandler()( "trace count " + key );
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
            throw std::runtime_error( "error in config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        //
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        //
        if ( joSkaleConfig.count( "sChain" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"sChain\"" );
        const nlohmann::json& joSkaleConfig_sChain = joSkaleConfig["sChain"];
        //
        if ( joSkaleConfig_sChain.count( "nodes" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"sChain\"/\"nodes\"" );
        const nlohmann::json& joSkaleConfig_sChain_nodes = joSkaleConfig_sChain["nodes"];
        //
        nlohmann::json jo = nlohmann::json::object();
        //
        nlohmann::json joThisNode = nlohmann::json::object();
        joThisNode["thisNodeIndex"] = nThisNodeIndex_;  // 1-based "schainIndex"
        //
        if ( joSkaleConfig_nodeInfo.count( "nodeID" ) > 0 &&
             joSkaleConfig_nodeInfo["nodeID"].is_number() )
            joThisNode["nodeID"] = joSkaleConfig_nodeInfo["nodeID"].get< int >();
        else
            joThisNode["nodeID"] = -1;
        //
        if ( joSkaleConfig_nodeInfo.count( "bindIP" ) > 0 &&
             joSkaleConfig_nodeInfo["bindIP"].is_string() )
            joThisNode["bindIP"] = joSkaleConfig_nodeInfo["bindIP"].get< std::string >();
        else
            joThisNode["bindIP"] = "";
        //
        if ( joSkaleConfig_nodeInfo.count( "bindIP6" ) > 0 &&
             joSkaleConfig_nodeInfo["bindIP6"].is_string() )
            joThisNode["bindIP6"] = joSkaleConfig_nodeInfo["bindIP6"].get< std::string >();
        else
            joThisNode["bindIP6"] = "";
        //
        if ( joSkaleConfig_nodeInfo.count( "httpRpcPort" ) > 0 &&
             joSkaleConfig_nodeInfo["httpRpcPort"].is_number() )
            joThisNode["httpRpcPort"] = joSkaleConfig_nodeInfo["httpRpcPort"].get< int >();
        else
            joThisNode["httpRpcPort"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "httpsRpcPort" ) > 0 &&
             joSkaleConfig_nodeInfo["httpsRpcPort"].is_number() )
            joThisNode["httpsRpcPort"] = joSkaleConfig_nodeInfo["httpsRpcPort"].get< int >();
        else
            joThisNode["httpsRpcPort"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "wsRpcPort" ) > 0 &&
             joSkaleConfig_nodeInfo["wsRpcPort"].is_number() )
            joThisNode["wsRpcPort"] = joSkaleConfig_nodeInfo["wsRpcPort"].get< int >();
        else
            joThisNode["wsRpcPort"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "wssRpcPort" ) > 0 &&
             joSkaleConfig_nodeInfo["wssRpcPort"].is_number() )
            joThisNode["wssRpcPort"] = joSkaleConfig_nodeInfo["wssRpcPort"].get< int >();
        else
            joThisNode["wssRpcPort"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "httpRpcPort6" ) > 0 &&
             joSkaleConfig_nodeInfo["httpRpcPort6"].is_number() )
            joThisNode["httpRpcPort6"] = joSkaleConfig_nodeInfo["httpRpcPort6"].get< int >();
        else
            joThisNode["httpRpcPort6"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "httpsRpcPort6" ) > 0 &&
             joSkaleConfig_nodeInfo["httpsRpcPort6"].is_number() )
            joThisNode["httpsRpcPort6"] = joSkaleConfig_nodeInfo["httpsRpcPort6"].get< int >();
        else
            joThisNode["httpsRpcPort6"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "wsRpcPort6" ) > 0 &&
             joSkaleConfig_nodeInfo["wsRpcPort6"].is_number() )
            joThisNode["wsRpcPort6"] = joSkaleConfig_nodeInfo["wsRpcPort6"].get< int >();
        else
            joThisNode["wsRpcPort6"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "wssRpcPort6" ) > 0 &&
             joSkaleConfig_nodeInfo["wssRpcPort6"].is_number() )
            joThisNode["wssRpcPort6"] = joSkaleConfig_nodeInfo["wssRpcPort6"].get< int >();
        else
            joThisNode["wssRpcPort6"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "acceptors" ) > 0 &&
             joSkaleConfig_nodeInfo["acceptors"].is_number() )
            joThisNode["acceptors"] = joSkaleConfig_nodeInfo["acceptors"].get< int >();
        else
            joThisNode["acceptors"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "enable-personal-apis" ) > 0 &&
             joSkaleConfig_nodeInfo["enable-personal-apis"].is_boolean() )
            joThisNode["enable-personal-apis"] =
                joSkaleConfig_nodeInfo["enable-personal-apis"].get< bool >();
        else
            joThisNode["enable-personal-apis"] = false;
        //
        if ( joSkaleConfig_nodeInfo.count( "enable-admin-apis" ) > 0 &&
             joSkaleConfig_nodeInfo["enable-admin-apis"].is_boolean() )
            joThisNode["enable-admin-apis"] =
                joSkaleConfig_nodeInfo["enable-admin-apis"].get< bool >();
        else
            joThisNode["enable-admin-apis"] = false;
        //
        if ( joSkaleConfig_nodeInfo.count( "enable-debug-behavior-apis" ) > 0 &&
             joSkaleConfig_nodeInfo["enable-debug-behavior-apis"].is_boolean() )
            joThisNode["enable-debug-behavior-apis"] =
                joSkaleConfig_nodeInfo["enable-debug-behavior-apis"].get< bool >();
        else
            joThisNode["enable-debug-behavior-apis"] = false;
        //
        if ( joSkaleConfig_nodeInfo.count( "enable-performance-tracker-apis" ) > 0 &&
             joSkaleConfig_nodeInfo["enable-performance-tracker-apis"].is_boolean() )
            joThisNode["enable-performance-tracker-apis"] =
                joSkaleConfig_nodeInfo["enable-performance-tracker-apis"].get< bool >();
        else
            joThisNode["enable-performance-tracker-apis"] = false;
        //
        if ( joSkaleConfig_nodeInfo.count( "unsafe-transactions" ) > 0 &&
             joSkaleConfig_nodeInfo["unsafe-transactions"].is_boolean() )
            joThisNode["unsafe-transactions"] =
                joSkaleConfig_nodeInfo["unsafe-transactions"].get< bool >();
        else
            joThisNode["unsafe-transactions"] = false;
        //
        if ( joSkaleConfig_sChain.count( "schainID" ) > 0 &&
             joSkaleConfig_sChain["schainID"].is_number() )
            jo["schainID"] = joSkaleConfig_sChain["schainID"].get< int >();
        else
            joThisNode["schainID"] = -1;
        //
        if ( joSkaleConfig_sChain.count( "schainName" ) > 0 &&
             joSkaleConfig_sChain["schainName"].is_number() )
            jo["schainName"] = joSkaleConfig_sChain["schainName"].get< std::string >();
        else
            joThisNode["schainName"] = "";
        //
        nlohmann::json jarrNetwork = nlohmann::json::array();
        size_t i, cnt = joSkaleConfig_sChain_nodes.size();
        for ( i = 0; i < cnt; ++i ) {
            const nlohmann::json& joNC = joSkaleConfig_sChain_nodes[i];
            nlohmann::json joNode = nlohmann::json::object();
            //
            if ( joNC.count( "nodeID" ) > 0 && joNC["nodeID"].is_number() )
                joNode["nodeID"] = joNC["nodeID"].get< int >();
            else
                joNode["nodeID"] = 1;
            //
            if ( joNC.count( "publicIP" ) > 0 && joNC["publicIP"].is_string() )
                joNode["ip"] = joNC["publicIP"].get< std::string >();
            else {
                if ( joNC.count( "ip" ) > 0 && joNC["ip"].is_string() )
                    joNode["ip"] = joNC["ip"].get< std::string >();
                else
                    joNode["ip"] = "";
            }
            //
            if ( joNC.count( "publicIP6" ) > 0 && joNC["publicIP6"].is_string() )
                joNode["ip6"] = joNC["publicIP6"].get< std::string >();
            else {
                if ( joNC.count( "ip6" ) > 0 && joNC["ip6"].is_string() )
                    joNode["ip6"] = joNC["ip6"].get< std::string >();
                else
                    joNode["ip6"] = "";
            }
            //
            if ( joNC.count( "schainIndex" ) > 0 && joNC["schainIndex"].is_number() )
                joNode["schainIndex"] = joNC["schainIndex"].get< int >();
            else
                joNode["schainIndex"] = -1;
            //
            if ( joNC.count( "httpRpcPort" ) > 0 && joNC["httpRpcPort"].is_number() )
                joNode["httpRpcPort"] = joNC["httpRpcPort"].get< int >();
            else
                joNode["httpRpcPort"] = 0;
            //
            if ( joNC.count( "httpsRpcPort" ) > 0 && joNC["httpsRpcPort"].is_number() )
                joNode["httpsRpcPort"] = joNC["httpsRpcPort"].get< int >();
            else
                joNode["httpsRpcPort"] = 0;
            //
            if ( joNC.count( "wsRpcPort" ) > 0 && joNC["wsRpcPort"].is_number() )
                joNode["wsRpcPort"] = joNC["wsRpcPort"].get< int >();
            else
                joNode["wsRpcPort"] = 0;
            //
            if ( joNC.count( "wssRpcPort" ) > 0 && joNC["wssRpcPort"].is_number() )
                joNode["wssRpcPort"] = joNC["wssRpcPort"].get< int >();
            else
                joNode["wssRpcPort"] = 0;
            //
            if ( joNC.count( "httpRpcPort6" ) > 0 && joNC["httpRpcPort6"].is_number() )
                joNode["httpRpcPort6"] = joNC["httpRpcPort6"].get< int >();
            else
                joNode["httpRpcPort6"] = 0;
            //
            if ( joNC.count( "httpsRpcPort6" ) > 0 && joNC["httpsRpcPort6"].is_number() )
                joNode["httpsRpcPort6"] = joNC["httpsRpcPort6"].get< int >();
            else
                joNode["httpsRpcPort6"] = 0;
            //
            if ( joNC.count( "wsRpcPort6" ) > 0 && joNC["wsRpcPort6"].is_number() )
                joNode["wsRpcPort6"] = joNC["wsRpcPort6"].get< int >();
            else
                joNode["wsRpcPort6"] = 0;
            //
            if ( joNC.count( "wssRpcPort6" ) > 0 && joNC["wssRpcPort6"].is_number() )
                joNode["wssRpcPort6"] = joNC["wssRpcPort6"].get< int >();
            else
                joNode["wssRpcPort6"] = 0;
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
            throw std::runtime_error( "error in config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        //
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        //
        if ( joSkaleConfig_nodeInfo.count( "wallets" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"/\"wallets\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_wallets = joSkaleConfig_nodeInfo["wallets"];
        //
        if ( joSkaleConfig_nodeInfo_wallets.count( "ima" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"ima\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_wallets_ima =
            joSkaleConfig_nodeInfo_wallets["ima"];
        //
        // validate wallet description
        static const char* g_arrMustHaveWalletFields[] = { // "url",
            "keyShareName", "t", "n", "BLSPublicKey0", "BLSPublicKey1", "BLSPublicKey2",
            "BLSPublicKey3", "commonBLSPublicKey0", "commonBLSPublicKey1", "commonBLSPublicKey2",
            "commonBLSPublicKey3"
        };
        size_t i, cnt =
                      sizeof( g_arrMustHaveWalletFields ) / sizeof( g_arrMustHaveWalletFields[0] );
        for ( i = 0; i < cnt; ++i ) {
            std::string strFieldName = g_arrMustHaveWalletFields[i];
            if ( joSkaleConfig_nodeInfo_wallets_ima.count( strFieldName ) == 0 )
                throw std::runtime_error(
                    "error in config.json file, cannot find field "
                    "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"ima\"/" +
                    strFieldName );
            const nlohmann::json& joField = joSkaleConfig_nodeInfo_wallets_ima[strFieldName];
            if ( strFieldName == "t" || strFieldName == "n" ) {
                if ( !joField.is_number() )
                    throw std::runtime_error(
                        "error in config.json file, field "
                        "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"ima\"/" +
                        strFieldName + " must be a number" );
                continue;
            }
            if ( !joField.is_string() )
                throw std::runtime_error(
                    "error in config.json file, field "
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
        jo["BLSPublicKey0"] =
            joSkaleConfig_nodeInfo_wallets_ima["BLSPublicKey0"].get< std::string >();
        jo["BLSPublicKey1"] =
            joSkaleConfig_nodeInfo_wallets_ima["BLSPublicKey1"].get< std::string >();
        jo["BLSPublicKey2"] =
            joSkaleConfig_nodeInfo_wallets_ima["BLSPublicKey2"].get< std::string >();
        jo["BLSPublicKey3"] =
            joSkaleConfig_nodeInfo_wallets_ima["BLSPublicKey3"].get< std::string >();
        //
        jo["commonBLSPublicKey0"] =
            joSkaleConfig_nodeInfo_wallets_ima["commonBLSPublicKey0"].get< std::string >();
        jo["commonBLSPublicKey1"] =
            joSkaleConfig_nodeInfo_wallets_ima["commonBLSPublicKey1"].get< std::string >();
        jo["commonBLSPublicKey2"] =
            joSkaleConfig_nodeInfo_wallets_ima["commonBLSPublicKey2"].get< std::string >();
        jo["commonBLSPublicKey3"] =
            joSkaleConfig_nodeInfo_wallets_ima["commonBLSPublicKey3"].get< std::string >();
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

static std::string stat_prefix_align( const std::string& strSrc, size_t n, char ch ) {
    std::string strDst = strSrc;
    while ( strDst.length() < n )
        strDst.insert( 0, 1, ch );
    return strDst;
}

static std::string stat_encode_eth_call_data_chunck_address(
    const std::string& strSrc, size_t alignWithZerosTo = 64 ) {
    std::string strDst = strSrc;
    strDst = skutils::tools::replace_all_copy( strDst, "0x", "" );
    strDst = skutils::tools::replace_all_copy( strDst, "0X", "" );
    strDst = stat_prefix_align( strDst, alignWithZerosTo, '0' );
    return strDst;
}

static std::string stat_encode_eth_call_data_chunck_size_t(
    const dev::u256& uSrc, size_t alignWithZerosTo = 64 ) {
    std::string strDst = dev::toJS( uSrc );
    strDst = skutils::tools::replace_all_copy( strDst, "0x", "" );
    strDst = skutils::tools::replace_all_copy( strDst, "0X", "" );
    strDst = stat_prefix_align( strDst, alignWithZerosTo, '0' );
    return strDst;
}

static std::string stat_encode_eth_call_data_chunck_size_t(
    const std::string& strSrc, size_t alignWithZerosTo = 64 ) {
    dev::u256 uSrc( strSrc );
    return stat_encode_eth_call_data_chunck_size_t( uSrc, alignWithZerosTo );
}

static std::string stat_encode_eth_call_data_chunck_size_t(
    const size_t nSrc, size_t alignWithZerosTo = 64 ) {
    dev::u256 uSrc( nSrc );
    return stat_encode_eth_call_data_chunck_size_t( uSrc, alignWithZerosTo );
}


static void stat_array_invert( uint8_t* arr, size_t cnt ) {
    size_t n = cnt / 2;
    for ( size_t i = 0; i < n; ++i ) {
        uint8_t b1 = arr[i];
        uint8_t b2 = arr[cnt - i - 1];
        arr[i] = b2;
        arr[cnt - i - 1] = b1;
    }
}

static dev::bytes& stat_bytes_align_left( dev::bytes& vec, size_t cnt ) {
    while ( vec.size() < cnt )
        vec.insert( vec.begin(), 0 );
    return vec;
}

static dev::bytes& stat_array_align_right( dev::bytes& vec, size_t cnt ) {
    while ( vec.size() < cnt )
        vec.push_back( 0 );
    return vec;
}

static std::string stat_data_2_hex_string( const uint8_t* p, size_t cnt ) {
    std::string s;
    if ( p == nullptr || cnt == 0 )
        return s;
    char hs[10];
    for ( size_t i = 0; i < cnt; ++i ) {
        sprintf( hs, "%02x", p[i] );
        s += hs;
    }
    return s;  // there is no "0x" prefix at start of return value
}

static std::string stat_bytes_2_hex_string( const dev::bytes& vec ) {
    return stat_data_2_hex_string(
        ( uint8_t* ) vec.data(), vec.size() );  // there is no "0x" prefix at start of return value
}

static dev::bytes stat_hex_string_2_bytes( const std::string& src ) {
    std::string s = src;
    if ( ( s.length() % 2 ) != 0 )
        s = "0" + s;
    bytes vec = dev::fromHex( s );
    return vec;
}

static std::string& stat_ensure_have_0x_at_start( std::string& s ) {
    if ( s.length() < 2 || ( !( s[0] == '0' && ( s[1] == 'x' || s[1] == 'X' ) ) ) )
        s = "0x" + s;
    return s;
}

static std::string& stat_remove_0x_from_start( std::string& s ) {
    if ( s.length() >= 2 && s[0] == '0' && ( s[1] == 'x' || s[1] == 'X' ) )
        s = s.substr( 2 );
    return s;
}

static dev::bytes& stat_remove_leading_zeros( dev::bytes& vec ) {
    while ( vec.size() > 0 ) {
        if ( vec[0] != 0 )
            break;
        vec.erase( vec.begin(), vec.begin() + 1 );
    }
    return vec;
}

static dev::bytes& stat_append_hash_str_2_vec( dev::bytes& vec, const std::string& s ) {
    dev::u256 val( s );
    bytes v = dev::BMPBN::encode2vec< dev::u256 >( val, true );
    stat_bytes_align_left( v, 32 );
    vec.insert( vec.end(), v.begin(), v.end() );
    return vec;
}

static dev::bytes& stat_append_u256_2_vec( dev::bytes& vec, const dev::u256& val ) {
    bytes v = dev::BMPBN::encode2vec< dev::u256 >( val, true );
    stat_bytes_align_left( v, 32 );
    vec.insert( vec.end(), v.begin(), v.end() );
    return vec;
}

static dev::bytes& stat_append_address_2_vec( dev::bytes& vec, const dev::u256& val ) {
    std::string s = dev::toJS( val );
    s = stat_remove_0x_from_start( s );
    dev::bytes v = stat_hex_string_2_bytes( s );
    stat_remove_leading_zeros( v );
    stat_bytes_align_left( v, 32 );
    vec.insert( vec.end(), v.begin(), v.end() );
    return vec;
}

static dev::bytes stat_re_compute_vec_2_h256vec( dev::bytes& vec ) {
    dev::h256 h = dev::sha3( vec );
    std::string s = h.hex();
    stat_remove_0x_from_start( s );
    vec.clear();
    vec = stat_hex_string_2_bytes( s );
    return vec;
}

std::string SkaleStats::pick_own_s_chain_url_s() {
    std::string strURL;
    try {
        nlohmann::json joConfig = getConfigJSON();
        //
        if ( joConfig.count( "skaleConfig" ) == 0 )
            throw std::runtime_error( "error in config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        //
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        //
        if ( joSkaleConfig_nodeInfo.count( "bindIP" ) > 0 ) {
            std::string strIpAddress =
                skutils::tools::trim_copy( joSkaleConfig_nodeInfo["bindIP"].get< std::string >() );
            if ( !strIpAddress.empty() ) {
                if ( joSkaleConfig_nodeInfo.count( "httpRpcPort" ) > 0 ) {
                    int nPort = joSkaleConfig_nodeInfo["httpRpcPort"].get< int >();
                    if ( 0 < nPort && nPort <= 65535 )
                        return std::string( "http://" ) + strIpAddress + ":" +
                               skutils::tools::format( "%d", nPort );
                }
                if ( joSkaleConfig_nodeInfo.count( "wsRpcPort" ) > 0 ) {
                    int nPort = joSkaleConfig_nodeInfo["wsRpcPort"].get< int >();
                    if ( 0 < nPort && nPort <= 65535 )
                        return std::string( "ws://" ) + strIpAddress + ":" +
                               skutils::tools::format( "%d", nPort );
                }
                if ( joSkaleConfig_nodeInfo.count( "httpsRpcPort" ) > 0 ) {
                    int nPort = joSkaleConfig_nodeInfo["httpsRpcPort"].get< int >();
                    if ( 0 < nPort && nPort <= 65535 )
                        return std::string( "https://" ) + strIpAddress + ":" +
                               skutils::tools::format( "%d", nPort );
                }
                if ( joSkaleConfig_nodeInfo.count( "wssRpcPort" ) > 0 ) {
                    int nPort = joSkaleConfig_nodeInfo["wssRpcPort"].get< int >();
                    if ( 0 < nPort && nPort <= 65535 )
                        return std::string( "wss://" ) + strIpAddress + ":" +
                               skutils::tools::format( "%d", nPort );
                }
            }  // if ( !strIpAddress.empty() )
        } else if ( joSkaleConfig_nodeInfo.count( "bindIP6" ) > 0 ) {
            std::string strIpAddress =
                skutils::tools::trim_copy( joSkaleConfig_nodeInfo["bindIP"].get< std::string >() );
            if ( !strIpAddress.empty() ) {
                if ( joSkaleConfig_nodeInfo.count( "httpRpcPort6" ) > 0 ) {
                    int nPort = joSkaleConfig_nodeInfo["httpRpcPort6"].get< int >();
                    if ( 0 < nPort && nPort <= 65535 )
                        return std::string( "http://[" ) + strIpAddress +
                               "]:" + skutils::tools::format( "%d", nPort );
                }
                if ( joSkaleConfig_nodeInfo.count( "wsRpcPort6" ) > 0 ) {
                    int nPort = joSkaleConfig_nodeInfo["wsRpcPort6"].get< int >();
                    if ( 0 < nPort && nPort <= 65535 )
                        return std::string( "ws://[" ) + strIpAddress +
                               "]:" + skutils::tools::format( "%d", nPort );
                }
                if ( joSkaleConfig_nodeInfo.count( "httpsRpcPort6" ) > 0 ) {
                    int nPort = joSkaleConfig_nodeInfo["httpsRpcPort6"].get< int >();
                    if ( 0 < nPort && nPort <= 65535 )
                        return std::string( "https://[" ) + strIpAddress +
                               "]:" + skutils::tools::format( "%d", nPort );
                }
                if ( joSkaleConfig_nodeInfo.count( "wssRpcPort6" ) > 0 ) {
                    int nPort = joSkaleConfig_nodeInfo["wssRpcPort6"].get< int >();
                    if ( 0 < nPort && nPort <= 65535 )
                        return std::string( "wss://[" ) + strIpAddress +
                               "]:" + skutils::tools::format( "%d", nPort );
                }
            }  // if ( !strIpAddress.empty() )
        }
    } catch ( ... ) {
    }
    strURL.clear();
    return strURL;
}

skutils::url SkaleStats::pick_own_s_chain_url() {
    std::string strURL = pick_own_s_chain_url_s();
    skutils::url u( strURL );
    return u;
}


std::map<std::string, StatsCounter> SkaleStats::statsCounters;



};  // namespace rpc
};  // namespace dev

