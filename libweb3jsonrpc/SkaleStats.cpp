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

#include <skutils/console_colors.h>
#include <skutils/eth_utils.h>
#include <skutils/rest_call.h>
#include <skutils/task_performance.h>

#include <algorithm>
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

namespace tracking {

txn_entry::txn_entry() {
    clear();
}

txn_entry::txn_entry( dev::u256 hash ) {
    clear();
    hash_ = hash;
    setNowTimeStamp();
}

txn_entry::txn_entry( const txn_entry& other ) {
    assign( other );
}

txn_entry::txn_entry( txn_entry&& other ) {
    assign( other );
    other.clear();
}
txn_entry::~txn_entry() {
    clear();
}

bool txn_entry::operator!() const {
    return empty() ? false : true;
}

txn_entry& txn_entry::operator=( const txn_entry& other ) {
    return assign( other );
}

bool txn_entry::operator==( const txn_entry& other ) const {
    return ( compare( other ) == 0 ) ? true : false;
}
bool txn_entry::operator!=( const txn_entry& other ) const {
    return ( compare( other ) != 0 ) ? true : false;
}
bool txn_entry::operator<( const txn_entry& other ) const {
    return ( compare( other ) < 0 ) ? true : false;
}
bool txn_entry::operator<=( const txn_entry& other ) const {
    return ( compare( other ) <= 0 ) ? true : false;
}
bool txn_entry::operator>( const txn_entry& other ) const {
    return ( compare( other ) > 0 ) ? true : false;
}
bool txn_entry::operator>=( const txn_entry& other ) const {
    return ( compare( other ) >= 0 ) ? true : false;
}

bool txn_entry::operator==( dev::u256 hash ) const {
    return ( compare( hash ) == 0 ) ? true : false;
}
bool txn_entry::operator!=( dev::u256 hash ) const {
    return ( compare( hash ) != 0 ) ? true : false;
}
bool txn_entry::operator<( dev::u256 hash ) const {
    return ( compare( hash ) < 0 ) ? true : false;
}
bool txn_entry::operator<=( dev::u256 hash ) const {
    return ( compare( hash ) <= 0 ) ? true : false;
}
bool txn_entry::operator>( dev::u256 hash ) const {
    return ( compare( hash ) > 0 ) ? true : false;
}
bool txn_entry::operator>=( dev::u256 hash ) const {
    return ( compare( hash ) >= 0 ) ? true : false;
}

bool txn_entry::empty() const {
    if ( hash_ == 0 )
        return true;
    return false;
}

void txn_entry::clear() {
    hash_ = 0;
    ts_ = 0;
}

txn_entry& txn_entry::assign( const txn_entry& other ) {
    hash_ = other.hash_;
    ts_ = other.ts_;
    return ( *this );
}

int txn_entry::compare( dev::u256 hash ) const {
    if ( hash_ < hash )
        return -1;
    if ( hash_ > hash )
        return 0;
    return 0;
}

int txn_entry::compare( const txn_entry& other ) const {
    return compare( other.hash_ );
}

void txn_entry::setNowTimeStamp() {
    ts_ = ::time( nullptr );
}

static dev::u256 stat_s2a( const std::string& saIn ) {
    std::string sa;
    if ( !( saIn.length() > 2 && saIn[0] == '0' && ( saIn[1] == 'x' || saIn[1] == 'X' ) ) )
        sa = "0x" + saIn;
    else
        sa = saIn;
    dev::u256 u( sa.c_str() );
    return u;
}

nlohmann::json txn_entry::toJSON() const {
    nlohmann::json jo = nlohmann::json::object();
    jo["hash"] = dev::toJS( hash_ );
    jo["timestamp"] = ts_;
    return jo;
}

bool txn_entry::fromJSON( const nlohmann::json& jo ) {
    if ( !jo.is_object() )
        return false;
    try {
        std::string strHash;
        if ( jo.count( "hash" ) > 0 && jo["hash"].is_string() )
            strHash = jo["hash"].get< std::string >();
        else
            throw std::runtime_error( "\"hash\" is must-have field of tracked TXN" );
        dev::u256 h = stat_s2a( strHash );
        int ts = 0;
        try {
            if ( jo.count( "timestamp" ) > 0 && jo["timestamp"].is_number() )
                ts = jo["timestamp"].get< int >();
        } catch ( ... ) {
            ts = 0;
        }
        hash_ = h;
        ts_ = ts;
        return true;
    } catch ( ... ) {
        return false;
    }
}

std::atomic_size_t pending_ima_txns::g_nMaxPendingTxns = 512;
std::string pending_ima_txns::g_strDispatchQueueID = "IMA-txn-tracker";

pending_ima_txns::pending_ima_txns( const std::string& configPath )
    : skutils::json_config_file_accessor( configPath ) {}

pending_ima_txns::~pending_ima_txns() {
    tracking_stop();
    clear();
}

bool pending_ima_txns::empty() const {
    lock_type lock( mtx() );
    if ( !set_txns_.empty() )
        return false;
    return true;
}
void pending_ima_txns::clear() {
    lock_type lock( mtx() );
    set_txns_.clear();
    list_txns_.clear();
    tracking_auto_start_stop();
}

size_t pending_ima_txns::max_txns() const {
    return size_t( g_nMaxPendingTxns );
}

void pending_ima_txns::adjust_limits_impl( bool isEnableBroadcast ) {
    const size_t nMax = max_txns();
    if ( nMax < 1 )
        return;  // no limits
    size_t cnt = list_txns_.size();
    while ( cnt > nMax ) {
        txn_entry txe = list_txns_.front();
        if ( !erase( txe.hash_, isEnableBroadcast ) )
            break;
        cnt = list_txns_.size();
    }
    tracking_auto_start_stop();
}
void pending_ima_txns::adjust_limits( bool isEnableBroadcast ) {
    lock_type lock( mtx() );
    adjust_limits_impl( isEnableBroadcast );
}

bool pending_ima_txns::insert( txn_entry& txe, bool isEnableBroadcast ) {
    lock_type lock( mtx() );
    set_txns_t::iterator itFindS = set_txns_.find( txe ), itEndS = set_txns_.end();
    if ( itFindS != itEndS )
        return false;
    set_txns_.insert( txe );
    list_txns_.push_back( txe );
    on_txn_insert( txe, isEnableBroadcast );
    adjust_limits_impl( isEnableBroadcast );
    return true;
}
bool pending_ima_txns::insert( dev::u256 hash, bool isEnableBroadcast ) {
    txn_entry txe( hash );
    return insert( txe, isEnableBroadcast );
}

bool pending_ima_txns::erase( txn_entry& txe, bool isEnableBroadcast ) {
    return erase( txe.hash_, isEnableBroadcast );
}
bool pending_ima_txns::erase( dev::u256 hash, bool isEnableBroadcast ) {
    lock_type lock( mtx() );
    set_txns_t::iterator itFindS = set_txns_.find( hash ), itEndS = set_txns_.end();
    if ( itFindS == itEndS )
        return false;
    txn_entry txe = ( *itFindS );
    set_txns_.erase( itFindS );
    list_txns_t::iterator ifFindL = std::find( list_txns_.begin(), list_txns_.end(), hash );
    list_txns_.erase( ifFindL );
    on_txn_erase( txe, isEnableBroadcast );
    return true;
}

bool pending_ima_txns::find( txn_entry& txe ) const {
    return find( txe.hash_ );
}
bool pending_ima_txns::find( dev::u256 hash ) const {
    lock_type lock( mtx() );
    set_txns_t::iterator itFindS = set_txns_.find( hash ), itEndS = set_txns_.end();
    if ( itFindS == itEndS )
        return false;
    return true;
}

void pending_ima_txns::list_all( list_txns_t& lst ) const {
    lst.clear();
    lock_type lock( mtx() );
    lst = list_txns_;
}

void pending_ima_txns::on_txn_insert( const txn_entry& txe, bool isEnableBroadcast ) {
    tracking_auto_start_stop();
    if ( isEnableBroadcast )
        broadcast_txn_insert( txe );
}
void pending_ima_txns::on_txn_erase( const txn_entry& txe, bool isEnableBroadcast ) {
    tracking_auto_start_stop();
    if ( isEnableBroadcast )
        broadcast_txn_erase( txe );
}

void pending_ima_txns::broadcast_txn_insert( const txn_entry& txe ) {
    std::string strLogPrefix = cc::deep_info( "IMA broadcast TXN insert" );
    try {
        size_t nOwnIndex = std::string::npos;
        std::vector< std::string > vecURLs;
        if ( !extract_s_chain_URL_infos( nOwnIndex, vecURLs ) )
            throw std::runtime_error(
                "failed to extract S-Chain node information from config JSON" );
        size_t i, cnt = vecURLs.size();
        for ( i = 0; i < cnt; ++i ) {
            if ( i == nOwnIndex )
                continue;
            std::string strURL = vecURLs[i];
            nlohmann::json joParams = txe.toJSON();
            skutils::dispatch::async( g_strDispatchQueueID, [=]() -> void {
                nlohmann::json joCall = nlohmann::json::object();
                joCall["jsonrpc"] = "2.0";
                joCall["method"] = "skale_imaBroadcastTxnInsert";
                joCall["params"] = joParams;
                skutils::rest::client cli( strURL );
                skutils::rest::data_t d = cli.call( joCall );
                try {
                    if ( d.empty() )
                        throw std::runtime_error( "empty broadcast answer" );
                    nlohmann::json joAnswer = nlohmann::json::parse( d.s_ );
                    if ( !joAnswer.is_object() )
                        throw std::runtime_error( "malformed non-JSON-object broadcast answer" );
                } catch ( const std::exception& ex ) {
                    clog( VerbosityError, "IMA" )
                        << ( strLogPrefix + " " + cc::fatal( "ERROR:" ) +
                               cc::error( " Transaction " ) + cc::info( dev::toJS( txe.hash_ ) ) +
                               cc::error( " to node " ) + cc::u( strURL ) +
                               cc::error( " broadcast failed: " ) + cc::warn( ex.what() ) );
                } catch ( ... ) {
                    clog( VerbosityError, "IMA" )
                        << ( strLogPrefix + " " + cc::fatal( "ERROR:" ) +
                               cc::error( " Transaction " ) + cc::info( dev::toJS( txe.hash_ ) ) +
                               cc::error( " broadcast to node " ) + cc::u( strURL ) +
                               cc::error( " failed: " ) + cc::warn( "unknown exception" ) );
                }
            } );
        }
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "ERROR:" ) + cc::error( " Transaction " ) +
                   cc::info( dev::toJS( txe.hash_ ) ) + cc::error( " broadcast failed: " ) +
                   cc::warn( ex.what() ) );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "ERROR:" ) + cc::error( " Transaction " ) +
                   cc::info( dev::toJS( txe.hash_ ) ) + cc::error( " broadcast failed: " ) +
                   cc::warn( "unknown exception" ) );
    }
}
void pending_ima_txns::broadcast_txn_erase( const txn_entry& txe ) {
    std::string strLogPrefix = cc::deep_info( "IMA broadcast TXN erase" );
    try {
        size_t nOwnIndex = std::string::npos;
        std::vector< std::string > vecURLs;
        if ( !extract_s_chain_URL_infos( nOwnIndex, vecURLs ) )
            throw std::runtime_error(
                "failed to extract S-Chain node information from config JSON" );
        size_t i, cnt = vecURLs.size();
        for ( i = 0; i < cnt; ++i ) {
            if ( i == nOwnIndex )
                continue;
            std::string strURL = vecURLs[i];
            nlohmann::json joParams = txe.toJSON();
            skutils::dispatch::async( g_strDispatchQueueID, [=]() -> void {
                nlohmann::json joCall = nlohmann::json::object();
                joCall["jsonrpc"] = "2.0";
                joCall["method"] = "skale_imaBroadcastTxnErase";
                joCall["params"] = joParams;
                skutils::rest::client cli( strURL );
                skutils::rest::data_t d = cli.call( joCall );
                try {
                    if ( d.empty() )
                        throw std::runtime_error( "empty broadcast answer" );
                    nlohmann::json joAnswer = nlohmann::json::parse( d.s_ );
                    if ( !joAnswer.is_object() )
                        throw std::runtime_error( "malformed non-JSON-object broadcast answer" );
                } catch ( const std::exception& ex ) {
                    clog( VerbosityError, "IMA" )
                        << ( strLogPrefix + " " + cc::fatal( "ERROR:" ) +
                               cc::error( " Transaction " ) + cc::info( dev::toJS( txe.hash_ ) ) +
                               cc::error( " broadcast to node " ) + cc::u( strURL ) +
                               cc::error( " failed: " ) + cc::warn( ex.what() ) );
                } catch ( ... ) {
                    clog( VerbosityError, "IMA" )
                        << ( strLogPrefix + " " + cc::fatal( "ERROR:" ) +
                               cc::error( " Transaction " ) + cc::info( dev::toJS( txe.hash_ ) ) +
                               cc::error( " to node " ) + cc::u( strURL ) +
                               cc::error( " broadcast failed: " ) +
                               cc::warn( "unknown exception" ) );
                }
            } );
        }
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "ERROR:" ) + cc::error( " Transaction " ) +
                   cc::info( dev::toJS( txe.hash_ ) ) + cc::error( " broadcast failed: " ) +
                   cc::warn( ex.what() ) );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "ERROR:" ) + cc::error( " Transaction " ) +
                   cc::info( dev::toJS( txe.hash_ ) ) + cc::error( " broadcast failed: " ) +
                   cc::warn( "unknown exception" ) );
    }
}

std::atomic_size_t pending_ima_txns::g_nTrackingIntervalInSeconds = 90;

size_t pending_ima_txns::tracking_interval_in_seconds() const {
    return size_t( g_nTrackingIntervalInSeconds );
}

bool pending_ima_txns::is_tracking() const {
    return bool( isTracking_ );
}

void pending_ima_txns::tracking_auto_start_stop() {
    lock_type lock( mtx() );
    if ( list_txns_.size() == 0 ) {
        tracking_stop();
    } else {
        tracking_start();
    }
}

void pending_ima_txns::tracking_start() {
    lock_type lock( mtx() );
    if ( is_tracking() )
        return;
    skutils::dispatch::repeat( g_strDispatchQueueID,
        [=]() -> void {
            list_txns_t lst, lstMined;
            list_all( lst );
            for ( const dev::tracking::txn_entry& txe : lst ) {
                if ( !check_txn_is_mined( txe ) )
                    break;
                lstMined.push_back( txe );
            }
            for ( const dev::tracking::txn_entry& txe : lstMined ) {
                erase( txe.hash_, true );
            }
        },
        skutils::dispatch::duration_from_seconds( tracking_interval_in_seconds() ),
        &tracking_job_id_ );
    isTracking_ = true;
}

void pending_ima_txns::tracking_stop() {
    lock_type lock( mtx() );
    if ( !is_tracking() )
        return;
    skutils::dispatch::stop( tracking_job_id_ );
    tracking_job_id_.clear();
    isTracking_ = false;
}

bool pending_ima_txns::check_txn_is_mined( const txn_entry& txe ) {
    return check_txn_is_mined( txe.hash_ );
}

bool pending_ima_txns::check_txn_is_mined( dev::u256 hash ) {
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
        // clog( VerbosityDebug, "IMA" )
        //    << ( cc::debug( " Using " ) + cc::notice( "Main Net URL" ) +
        //           cc::debug( " " ) + cc::info( strImaMainNetURL ) );
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
        nlohmann::json jarr = nlohmann::json::array();
        jarr.push_back( dev::toJS( hash ) );
        nlohmann::json joCall = nlohmann::json::object();
        joCall["jsonrpc"] = "2.0";
        joCall["method"] = "eth_getTransactionReceipt";
        joCall["params"] = jarr;
        skutils::rest::client cli( urlMainNet );
        skutils::rest::data_t d = cli.call( joCall );
        if ( d.empty() )
            throw std::runtime_error( "Main Net call to eth_getLogs failed" );
        nlohmann::json joReceipt = nlohmann::json::parse( d.s_ )["result"];
        if ( joReceipt.is_object() && joReceipt.count( "transactionHash" ) > 0 &&
             joReceipt.count( "blockNumber" ) > 0 && joReceipt.count( "gasUsed" ) > 0 )
            return true;
        return false;
    } catch ( ... ) {
        return false;
    }
}


};  // namespace tracking


namespace rpc {

SkaleStats::SkaleStats( const std::string& configPath, eth::Interface& _eth )
    : pending_ima_txns( configPath ), m_eth( _eth ) {
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
        if ( joSkaleConfig_nodeInfo.count( "enable-debug-behavior-apis" ) > 0 &&
             joSkaleConfig_nodeInfo["enable-debug-behavior-apis"].is_boolean() )
            joThisNode["enable-debug-behavior-apis"] =
                joSkaleConfig_nodeInfo["enable-debug-behavior-apis"].get< bool >();
        else
            joThisNode["enable-debug-behavior-apis"] = false;
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
            "BLSPublicKey0", "BLSPublicKey1", "BLSPublicKey2", "BLSPublicKey3",
            "commonBLSPublicKey0", "commonBLSPublicKey1", "commonBLSPublicKey2",
            "commonBLSPublicKey3"};
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

Json::Value SkaleStats::skale_imaVerifyAndSign( const Json::Value& request ) {
    std::string strLogPrefix = cc::deep_info( "IMA Verify+Sign" );
    try {
        nlohmann::json joConfig = getConfigJSON();
        Json::FastWriter fastWriter;
        const std::string strRequest = fastWriter.write( request );
        const nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        strLogPrefix = cc::bright( "Startup" ) + " " + cc::deep_info( "IMA Verify+Sign" );
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " Processing " ) + cc::notice( "IMA Verify and Sign" ) +
                   cc::debug( " request: " ) + cc::j( joRequest ) );
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
        bool bIsVerifyImaMessagesViaLogsSearch = false;  // default is false
        if ( joSkaleConfig_nodeInfo.count( "verifyImaMessagesViaLogsSearch" ) > 0 )
            bIsVerifyImaMessagesViaLogsSearch =
                joSkaleConfig_nodeInfo["verifyImaMessagesViaLogsSearch"].get< bool >();
        bool bIsImaMessagesViaContractCall = true;  // default is true
        if ( joSkaleConfig_nodeInfo.count( "verifyImaMessagesViaContractCall" ) > 0 )
            bIsImaMessagesViaContractCall =
                joSkaleConfig_nodeInfo["verifyImaMessagesViaContractCall"].get< bool >();
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
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " Using " ) + cc::notice( "IMA Message Proxy/S-Chain" ) +
                   cc::debug( " contract at address " ) +
                   cc::info( strAddressImaMessageProxySChain ) );
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
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " Using " ) + cc::notice( "IMA Message Proxy/MainNet" ) +
                   cc::debug( " contract at address " ) +
                   cc::info( strAddressImaMessageProxyMainNet ) );
        const std::string strAddressImaMessageProxyMainNetLC =
            skutils::tools::to_lower( strAddressImaMessageProxyMainNet );
        //
        //
        // if ( joSkaleConfig_nodeInfo.count( "imaCallerAddressSChain" ) == 0 )
        //    throw std::runtime_error(
        //        "error config.json file, cannot find "
        //        "\"skaleConfig\"/\"nodeInfo\"/\"imaCallerAddressSChain\"" );
        // const nlohmann::json& joAddressimaCallerAddressSChain =
        //    joSkaleConfig_nodeInfo["imaCallerAddressSChain"];
        // if ( !joAddressimaCallerAddressSChain.is_string() )
        //    throw std::runtime_error(
        //        "error config.json file, bad type of value in "
        //        "\"skaleConfig\"/\"nodeInfo\"/\"imaCallerAddressSChain\"" );
        // std::string strImaCallerAddressSChain =
        //    joAddressimaCallerAddressSChain.get< std::string >();
        // if ( strImaCallerAddressSChain.empty() )
        //    throw std::runtime_error(
        //        "error config.json file, bad empty value in "
        //        "\"skaleConfig\"/\"nodeInfo\"/\"imaCallerAddressSChain\"" );
        // clog( VerbosityDebug, "IMA" ) << ( strLogPrefix + cc::notice( "IMA S-Chain caller" )
        //          + cc::debug( " address is " ) + cc::info( strImaCallerAddressSChain ) );
        // const std::string strImaCallerAddressSChainLC =
        //    skutils::tools::to_lower( strImaCallerAddressSChain );
        //
        // if ( joSkaleConfig_nodeInfo.count( "imaCallerAddressMainNet" ) == 0 )
        //    throw std::runtime_error(
        //        "error config.json file, cannot find "
        //        "\"skaleConfig\"/\"nodeInfo\"/\"imaCallerAddressMainNet\"" );
        // const nlohmann::json& joAddressimaCallerAddressMainNet =
        //    joSkaleConfig_nodeInfo["imaCallerAddressMainNet"];
        // if ( !joAddressimaCallerAddressMainNet.is_string() )
        //    throw std::runtime_error(
        //        "error config.json file, bad type of value in "
        //        "\"skaleConfig\"/\"nodeInfo\"/\"imaCallerAddressMainNet\"" );
        // std::string strImaCallerAddressMainNet =
        //    joAddressimaCallerAddressMainNet.get< std::string >();
        // if ( strImaCallerAddressMainNet.empty() )
        //    throw std::runtime_error(
        //        "error config.json file, bad empty value in "
        //        "\"skaleConfig\"/\"nodeInfo\"/\"imaCallerAddressMainNet\"" );
        // clog( VerbosityDebug, "IMA" ) << ( strLogPrefix + cc::notice( "IMA S-Chain caller" )
        //          + cc::debug( " address is " ) + cc::info( strImaCallerAddressMainNet ) );
        // const std::string strImaCallerAddressMainNetLC =
        //    skutils::tools::to_lower( strImaCallerAddressMainNet );
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
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " Using " ) + cc::notice( "Main Net URL" ) +
                   cc::debug( " " ) + cc::info( strImaMainNetURL ) );
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
        if ( joSkaleConfig.count( "sChain" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find "
                "\"skaleConfig\"/\"sChain\"" );
        const nlohmann::json& joSkaleConfig_sChain = joSkaleConfig["sChain"];
        if ( joSkaleConfig_sChain.count( "schainName" ) == 0 )
            throw std::runtime_error(
                "error config.json file, cannot find "
                "\"skaleConfig\"/\"sChain\"/\"schainName\"" );
        std::string strSChainName = joSkaleConfig_sChain["schainName"].get< std::string >();
        //
        //
        // Extract needed request arguments, ensure they are all present and valid
        //
        bool bOnlyVerify = false;
        if ( joRequest.count( "onlyVerify" ) > 0 )
            bOnlyVerify = joRequest["onlyVerify"].get< bool >();
        if ( joRequest.count( "startMessageIdx" ) == 0 )
            throw std::runtime_error(
                "missing \"messages\"/\"startMessageIdx\" in call parameters" );
        const nlohmann::json& joStartMessageIdx = joRequest["startMessageIdx"];
        if ( !joStartMessageIdx.is_number_unsigned() )
            throw std::runtime_error(
                "bad value type of \"messages\"/\"startMessageIdx\" must be unsigned number" );
        const size_t nStartMessageIdx = joStartMessageIdx.get< size_t >();
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + " " + cc::notice( "Start message index" ) + cc::debug( " is " ) +
                   cc::size10( nStartMessageIdx ) );
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
        clog( VerbosityDebug, "IMA" ) << ( strLogPrefix + " " + cc::notice( "Source Chain ID" ) +
                                           cc::debug( " is " ) + cc::info( strSrcChainID ) );
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
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + " " + cc::notice( "Destination Chain ID" ) + cc::debug( " is " ) +
                   cc::info( strDstChainID ) );
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
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " Composing summary message to sign from " ) +
                   cc::size10( cntMessagesToSign ) +
                   cc::debug( " message(s), IMA index of first message is " ) +
                   cc::size10( nStartMessageIdx ) + cc::debug( ", src chain id is " ) +
                   cc::info( strSrcChainID ) + cc::debug( ", dst chain id is " ) +
                   cc::info( strDstChainID ) + cc::debug( "(" ) +
                   cc::info( dev::toJS( uDestinationChainID_32_max ) ) + cc::debug( ")..." ) );
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
                ( joSkaleConfig_nodeInfo_wallets_ima.count( "url" ) > 0 ) ?
                    joSkaleConfig_nodeInfo_wallets_ima["url"].get< std::string >() :
                    "";
            if ( strWalletURL.empty() )
                throw std::runtime_error( "empty wallet url" );
            u = skutils::url( strWalletURL );
            if ( u.scheme().empty() || u.host().empty() )
                throw std::runtime_error( "bad wallet url" );
            //
            //
            try {
                if ( joSkaleConfig_nodeInfo_wallets_ima.count( "caFile" ) > 0 )
                    optsSSL.ca_file = skutils::tools::trim_copy(
                        joSkaleConfig_nodeInfo_wallets_ima["caFile"].get< std::string >() );
            } catch ( ... ) {
                optsSSL.ca_file.clear();
            }
            clog( VerbosityDebug, "IMA" ) << ( strLogPrefix + cc::debug( " SGX Wallet CA file " ) +
                                               cc::info( optsSSL.ca_file ) );
            try {
                if ( joSkaleConfig_nodeInfo_wallets_ima.count( "certFile" ) > 0 )
                    optsSSL.client_cert = skutils::tools::trim_copy(
                        joSkaleConfig_nodeInfo_wallets_ima["certFile"].get< std::string >() );
            } catch ( ... ) {
                optsSSL.client_cert.clear();
            }
            clog( VerbosityDebug, "IMA" )
                << ( strLogPrefix + cc::debug( " SGX Wallet client certificate file " ) +
                       cc::info( optsSSL.client_cert ) );
            try {
                if ( joSkaleConfig_nodeInfo_wallets_ima.count( "keyFile" ) > 0 )
                    optsSSL.client_key = skutils::tools::trim_copy(
                        joSkaleConfig_nodeInfo_wallets_ima["keyFile"].get< std::string >() );
            } catch ( ... ) {
                optsSSL.client_key.clear();
            }
            clog( VerbosityDebug, "IMA" )
                << ( strLogPrefix + cc::debug( " SGX Wallet client key file " ) +
                       cc::info( optsSSL.client_key ) );
        } catch ( ... ) {
            throw std::runtime_error(
                "error config.json file, cannot find valid value for "
                "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"url\" parameter" );
        }
        const std::string keyShareName =
            ( joSkaleConfig_nodeInfo_wallets_ima.count( "keyShareName" ) > 0 ) ?
                joSkaleConfig_nodeInfo_wallets_ima["keyShareName"].get< std::string >() :
                "";
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
            clog( VerbosityDebug, "IMA" )
                << ( strLogPrefix + cc::debug( " Verifying message " ) + cc::size10( idxMessage ) +
                       cc::debug( " of " ) + cc::size10( cntMessagesToSign ) +
                       cc::debug( " with content: " ) + cc::info( strMessageData ) );
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
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + cc::debug( " Verifying " ) +
                           cc::sunny( strImaMessageTypeName ) + cc::debug( " transfer..." ) );
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
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + cc::debug( " Verifying " ) +
                           cc::sunny( strImaMessageTypeName ) + cc::debug( " transfer..." ) );
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(1), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                const dev::u256 contractPosition =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                // std::cout + "\"contractPosition\" is " + toJS( contractPosition ) + std::endl;
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(2), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                const dev::u256 addressTo =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                // std::cout + "\"addressTo\" is " + toJS( addressTo ) + std::endl;
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(3), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                const dev::u256 amount =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                // std::cout + "\"amount\" is " + toJS( amount ) + std::endl;
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(4), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                const dev::u256 sizeOfName =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                // std::cout + "\"sizeOfName\" is " + toJS( sizeOfName ) + std::endl;
                nPos += nFiledSize;
                nFiledSize = sizeOfName.convert_to< size_t >();
                // std::cout + "\"nFiledSize\" is " + nFiledSize + std::endl;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(5), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                std::string strName( "" );
                strName.insert( strName.end(), ( ( char* ) ( vecBytes.data() ) ) + nPos,
                    ( ( char* ) ( vecBytes.data() ) ) + nPos + nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(6), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                const dev::u256 sizeOfSymbol =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                // std::cout + "\"sizeOfSymbol\" is " + toJS( sizeOfSymbol ) + std::endl;
                nPos += 32;
                nFiledSize = sizeOfSymbol.convert_to< size_t >();
                // std::cout + "\"nFiledSize\" is " + nFiledSize + std::endl;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(7), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                std::string strSymbol( "" );
                strSymbol.insert( strSymbol.end(), ( ( char* ) ( vecBytes.data() ) ) + nPos,
                    ( ( char* ) ( vecBytes.data() ) ) + nPos + nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 1;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(8), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                const uint8_t nDecimals = uint8_t( vecBytes[nPos] );
                // std::cout + "\"nDecimals\" is " + nDecimals + std::endl;
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(9), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                const dev::u256 totalSupply =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                // std::cout + "\"totalSupply\" is " + toJS( totalSupply ) + std::endl;
                nPos += nFiledSize;
                //
                if ( nPos > cntMessageBytes ) {
                    const size_t nExtra = cntMessageBytes - nPos;
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::warn( " Extra " ) + cc::size10( nExtra ) +
                               cc::warn( " unused bytes found in message." ) );
                }
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + cc::debug( " Extracted " ) +
                           cc::sunny( strImaMessageTypeName ) + cc::debug( " data fields:" ) );
                clog( VerbosityDebug, "IMA" )
                    << ( "    " + cc::info( "contractPosition" ) + cc::debug( "......." ) +
                           cc::info( contractPosition.str() ) );
                clog( VerbosityDebug, "IMA" )
                    << ( "    " + cc::info( "to" ) + cc::debug( "....................." ) +
                           cc::info( addressTo.str() ) );
                clog( VerbosityDebug, "IMA" )
                    << ( "    " + cc::info( "amount" ) + cc::debug( "................." ) +
                           cc::info( amount.str() ) );
                clog( VerbosityDebug, "IMA" )
                    << ( "    " + cc::info( "name" ) + cc::debug( "..................." ) +
                           cc::info( strName ) );
                clog( VerbosityDebug, "IMA" )
                    << ( "    " + cc::info( "symbol" ) + cc::debug( "................." ) +
                           cc::info( strSymbol ) );
                clog( VerbosityDebug, "IMA" )
                    << ( "    " + cc::info( "decimals" ) + cc::debug( "..............." ) +
                           cc::num10( nDecimals ) );
                clog( VerbosityDebug, "IMA" )
                    << ( "    " + cc::info( "totalSupply" ) + cc::debug( "............" ) +
                           cc::info( totalSupply.str() ) );
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
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + cc::debug( " Verifying " ) +
                           cc::sunny( strImaMessageTypeName ) + cc::debug( " transfer..." ) );
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(1), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                const dev::u256 contractPosition =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(2), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                const dev::u256 addressTo =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(3), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                const dev::u256 tokenID =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(4), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                const dev::u256 sizeOfName =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                nFiledSize = sizeOfName.convert_to< size_t >();
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(5), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                std::string strName( "" );
                strName.insert( strName.end(), ( ( char* ) ( vecBytes.data() ) ) + nPos,
                    ( ( char* ) ( vecBytes.data() ) ) + nPos + nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(6), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                const dev::u256 sizeOfSymbol =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += 32;
                nFiledSize = sizeOfSymbol.convert_to< size_t >();
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(7), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                std::string strSymbol( "" );
                strSymbol.insert( strSymbol.end(), ( ( char* ) ( vecBytes.data() ) ) + nPos,
                    ( ( char* ) ( vecBytes.data() ) ) + nPos + nFiledSize );
                nPos += nFiledSize;
                //
                if ( nPos > cntMessageBytes ) {
                    size_t nExtra = cntMessageBytes - nPos;
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::warn( " Extra " ) + cc::size10( nExtra ) +
                               cc::warn( " unused bytes found in message." ) );
                }
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + cc::debug( " Extracted " ) +
                           cc::sunny( strImaMessageTypeName ) + cc::debug( " data fields:" ) );
                clog( VerbosityDebug, "IMA" )
                    << ( "    " + cc::info( "contractPosition" ) + cc::debug( "......." ) +
                           cc::info( contractPosition.str() ) );
                clog( VerbosityDebug, "IMA" )
                    << ( "    " + cc::info( "to" ) + cc::debug( "....................." ) +
                           cc::info( addressTo.str() ) );
                clog( VerbosityDebug, "IMA" )
                    << ( "    " + cc::info( "tokenID" ) + cc::debug( "................" ) +
                           cc::info( tokenID.str() ) );
                clog( VerbosityDebug, "IMA" )
                    << ( "    " + cc::info( "name" ) + cc::debug( "..................." ) +
                           cc::info( strName ) );
                clog( VerbosityDebug, "IMA" )
                    << ( "    " + cc::info( "symbol" ) + cc::debug( "................." ) +
                           cc::info( strSymbol ) );
            } break;
            case 0x13: {
                // Raw ERC20 transfer
                // --------------------------------------------------------------
                // Offset | Size     | Description
                // --------------------------------------------------------------
                // 0      | 1        | Value 0x13
                // 1      | 32       | contractPosition, address
                // 33     | 32       | to, address
                // 65     | 32       | amount
                static const char strImaMessageTypeName[] = "Raw-ERC20";
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + cc::debug( " Verifying " ) +
                           cc::sunny( strImaMessageTypeName ) + cc::debug( " transfer..." ) );
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(1), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                const dev::u256 contractPosition =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(2), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                const dev::u256 addressTo =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(3), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                const dev::u256 amount =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                //
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + cc::debug( " Extracted " ) +
                           cc::sunny( strImaMessageTypeName ) + cc::debug( " data fields:" ) );
                clog( VerbosityDebug, "IMA" )
                    << ( "    " + cc::info( "contractPosition" ) + cc::debug( "......." ) +
                           cc::info( contractPosition.str() ) );
                clog( VerbosityDebug, "IMA" )
                    << ( "    " + cc::info( "to" ) + cc::debug( "....................." ) +
                           cc::info( addressTo.str() ) );
                clog( VerbosityDebug, "IMA" )
                    << ( "    " + cc::info( "amount" ) + cc::debug( "................." ) +
                           cc::info( amount.str() ) );
            } break;
            case 0x15: {
                // Raw ERC721 transfer (currently no contractPosition but it would be soon)
                // --------------------------------------------------------------
                // Offset | Size     | Description
                // --------------------------------------------------------------
                // 0      | 1        | Value 0x13
                // 1      | 32       | to, address
                // 33     | 32       | amount
                static const char strImaMessageTypeName[] = "Raw-ERC721";
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + cc::debug( " Verifying " ) +
                           cc::sunny( strImaMessageTypeName ) + cc::debug( " transfer..." ) );
                //
                // nFiledSize = 32;
                // if ( ( nPos + nFiledSize ) > cntMessageBytes )
                //    throw std::runtime_error(
                //        skutils::tools::format( "IMA message too short, %s(1), nPos=%zu, "
                //                                "nFiledSize=%zu, cntMessageBytes=%zu",
                //            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                // const dev::u256 contractPosition =
                //    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                // nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(1), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                const dev::u256 addressTo =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                //
                nFiledSize = 32;
                if ( ( nPos + nFiledSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message too short, %s(2), nPos=%zu, "
                                                "nFiledSize=%zu, cntMessageBytes=%zu",
                            strImaMessageTypeName, nPos, nFiledSize, cntMessageBytes ) );
                const dev::u256 tokenID =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFiledSize );
                nPos += nFiledSize;
                //
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + cc::debug( " Extracted " ) +
                           cc::sunny( strImaMessageTypeName ) + cc::debug( " data fields:" ) );
                // clog( VerbosityDebug, "IMA" )
                //    << ( "    " + cc::info( "contractPosition" ) + cc::debug( "......." ) +
                //           cc::info( contractPosition.str() ) );
                clog( VerbosityDebug, "IMA" )
                    << ( "    " + cc::info( "to" ) + cc::debug( "....................." ) +
                           cc::info( addressTo.str() ) );
                clog( VerbosityDebug, "IMA" )
                    << ( "    " + cc::info( "tokenID" ) + cc::debug( "................" ) +
                           cc::info( tokenID.str() ) );
            } break;
            default: {
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + " " + cc::fatal( " UNKNOWN IMA MESSAGE: " ) +
                           cc::error( " Message code is " ) + cc::num10( b0 ) +
                           cc::error( ", message binary data is:\n" ) +
                           cc::binary_table( ( void* ) vecBytes.data(), vecBytes.size() ) );
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
            if ( bIsVerifyImaMessagesViaLogsSearch ) {
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + " " +
                           cc::debug(
                               "Will use contract event based verification of IMA message(s)" ) );
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
                //      ["0x8d646f556e5d9d6f1edcf7a39b77f5ac253776eb34efcfd688aacbee518efc26"],
                //      ["0x0000000000000000000000000000000000000000000000000000000000000010"],
                //      null
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
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + cc::debug( " Will execute logs search query: " ) +
                           cc::j( joLogsQuery ) );
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
                    Json::Value jvLogs =
                        dev::toJson( this->client()->logs( dev::eth::toLogFilter( jvLogsQuery ) ) );
                    jarrFoundLogRecords =
                        nlohmann::json::parse( Json::FastWriter().write( jvLogs ) );
                }  // else from if( strDirection == "M2S" )
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + cc::debug( " Got logs search query result: " ) +
                           cc::j( jarrFoundLogRecords ) );
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
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Analyzing transaction " ) +
                               cc::notice( strTransactionHash ) + cc::debug( "..." ) );
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
                        clog( VerbosityError, "IMA" )
                            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                                   cc::error( " Transaction verification failed: " ) +
                                   cc::warn( ex.what() ) );
                        continue;
                    } catch ( ... ) {
                        clog( VerbosityError, "IMA" )
                            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                                   cc::error( " Transaction verification failed: " ) +
                                   cc::warn( "unknown exception" ) );
                        continue;
                    }
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Reviewing transaction:" ) +
                               cc::j( joTransaction ) + cc::debug( "..." ) );
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
                        clog( VerbosityDebug, "IMA" )
                            << ( strLogPrefix + cc::debug( " Skipping transaction " ) +
                                   cc::notice( strTransactionHash ) + cc::debug( " because " ) +
                                   cc::warn( "to" ) + cc::debug( "=" ) +
                                   cc::notice( strTransactionTo ) +
                                   cc::debug( " is different than " ) +
                                   cc::warn( "IMA message sender" ) + cc::debug( "=" ) +
                                   cc::notice( strMessageSender ) );
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
                            "contractAddress":
                            "0x1bbde22a5d43d59883c1befd474eff2ec51519d2",
                            "cumulativeGasUsed": "0xf055",
                            "gasUsed": "0xf055",
                            "logs": [{
                                "address":
                                "0xfd02fc34219dc1dc923127062543c9522373d895",
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
                                "address":
                                "0x4c6ad417e3bf7f3d623bab87f29e119ef0f28059",
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

                        The last log record in receipt abaove contains "topics" and
                        "data" field we
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
                        clog( VerbosityError, "IMA" )
                            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                                   cc::error( " Receipt verification failed: " ) +
                                   cc::warn( ex.what() ) );
                        continue;
                    } catch ( ... ) {
                        clog( VerbosityError, "IMA" )
                            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                                   cc::error( " Receipt verification failed: " ) +
                                   cc::warn( "unknown exception" ) );
                        continue;
                    }
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Reviewing transaction receipt:" ) +
                               cc::j( joTransactionReceipt ) + cc::debug( "..." ) );
                    if ( joTransactionReceipt.count( "logs" ) == 0 )
                        continue;  // ???
                    const nlohmann::json& jarrLogsReceipt = joTransactionReceipt["logs"];
                    if ( !jarrLogsReceipt.is_array() )
                        continue;  // ???
                    bool bReceiptVerified = false;
                    size_t idxReceiptLogRecord = 0, cntReceiptLogRecords = jarrLogsReceipt.size();
                    for ( idxReceiptLogRecord = 0; idxReceiptLogRecord < cntReceiptLogRecords;
                          ++idxReceiptLogRecord ) {
                        const nlohmann::json& joReceiptLogRecord =
                            jarrLogsReceipt[idxReceiptLogRecord];
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
                            const nlohmann::json& joReceiptTopic =
                                jarrReceiptTopics[idxReceiptTopic];
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
                        if ( strDataLC_linear.find( strMessageData_linear_LC ) ==
                             std::string::npos )
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
                            clog( VerbosityDebug, "IMA" )
                                << ( strLogPrefix + cc::debug( "    chunk " ) +
                                       cc::info( strChunk ) );
                            try {
                                const dev::u256 uChunk( "0x" + strChunk );
                                // setChunksLC.insert( strChunk );
                                setChunksU256.insert( uChunk );
                            } catch ( ... ) {
                                clog( VerbosityDebug, "IMA" )
                                    << ( strLogPrefix + cc::debug( "            skipped chunk " ) );
                                continue;
                            }
                        }
                        if ( setChunksU256.find( uDestinationContract ) == setChunksU256.end() )
                            continue;
                        if ( setChunksU256.find( uDestinationAddressTo ) == setChunksU256.end() )
                            continue;
                        if ( setChunksU256.find( uMessageAmount ) == setChunksU256.end() )
                            continue;
                        if ( setChunksU256.find( uDestinationChainID_32_max ) ==
                             setChunksU256.end() )
                            continue;
                        //
                        bReceiptVerified = true;
                        break;
                    }
                    if ( !bReceiptVerified ) {
                        clog( VerbosityDebug, "IMA" )
                            << ( strLogPrefix + cc::debug( " Skipping transaction " ) +
                                   cc::notice( strTransactionHash ) +
                                   cc::debug( " because no appropriate receipt was found" ) );
                        continue;
                    }
                    //
                    //
                    //
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::success( " Found transaction for IMA message " ) +
                               cc::size10( nStartMessageIdx + idxMessage ) + cc::success( ": " ) +
                               cc::j( joTransaction ) );
                    bTransactionWasFound = true;
                    break;
                }
                if ( !bTransactionWasFound ) {
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + " " +
                               cc::error( "No transaction was found in logs for IMA message " ) +
                               cc::size10( nStartMessageIdx + idxMessage ) + cc::error( "." ) );
                    throw std::runtime_error( "No transaction was found in logs for IMA message " +
                                              std::to_string( nStartMessageIdx + idxMessage ) );
                }  // if ( !bTransactionWasFound )
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + cc::success( " Success, IMA message " ) +
                           cc::size10( nStartMessageIdx + idxMessage ) +
                           cc::success( " was found in logs." ) );
            }  // if( bIsVerifyImaMessagesViaLogsSearch )
            //
            //
            //
            if ( bIsImaMessagesViaContractCall ) {
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + " " +
                           cc::debug(
                               "Will use contract call based verification of IMA message(s)" ) );
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + " " +
                           cc::warn(
                               "Skipped contract event based verification of IMA message(s)" ) );
                bool bTransactionWasVerifed = false;


                // function verifyOutgoingMessageData(
                // string memory chainName,
                // uint256 idxMessage,
                // address sender,
                // address destinationContract,
                // address to,
                // uint256 amount
                //) public view returns ( bool isValidMessage ) { ... ... ...
                //--------------------------------------------------------------------------------
                // 0xb29cc575                                                       // signature
                // 00000000000000000000000000000000000000000000000000000000000000c0 // position for
                // chainName string data
                // 0000000000000000000000000000000000000000000000000000000000000004 // idxMessage
                // 000000000000000000000000977c8115e8c2ab8bc9b6ed76d058c75055f915f9 // sender
                // 000000000000000000000000e410b2469709e878bff2de4b155bf9df5a16f0ea //
                // destinationContract
                // 0000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f // to
                // 0000000000000000000000000000000000000000000000000de0b6b3a7640000 // amount
                // 1000000000000000000
                // 0000000000000000000000000000000000000000000000000000000000000000 // length of
                // chainName string 0000000000000000000000000000000000000000000000000000000000000000
                // // data of chainName string
                // 0----|----1----|----2----|----3----|----4----|----5----|----6----|----7----|----
                // 01234567890123456789012345678901234567890123456789012345678901234567890123456789
                // 0000000000000000000000000000000000000000000000000000000000000000

                // 0xb29cc575                                                       // signature
                // 00000000000000000000000000000000000000000000000000000000000000c0 // position for
                // chainName string data
                // 0000000000000000000000000000000000000000000000000000000000000000 // idxMessage
                // 00000000000000000000000057aD607C6e90Df7D7F158985c3e436007a15d744 // sender
                // 0000000000000000000000001F0eBCf6B0393d7759cd2F9014fc67ef8AF4d702 //
                // destinationContract
                // 0000000000000000000000007aa5E36AA15E93D10F4F26357C30F052DacDde5F // to
                // 0000000000000000000000000000000000000000000000000de0b6b3a7640000 // amount
                // 0000000000000000000000000000000000000000000000000000000000000003 // length of
                // chainName string 426f620000000000000000000000000000000000000000000000000000000000
                // // data of chainName string

                std::string strCallData =
                    "0xb29cc575";  // signature as first 8 symbols of keccak256 from
                                   // "verifyOutgoingMessageData(string,uint256,address,address,address,uint256)"
                // enode position for chainName string data
                strCallData += stat_encode_eth_call_data_chunck_size_t( 0xC0 );
                // encode value of ( nStartMessageIdx + idxMessage ) as "idxMessage" call argument
                strCallData +=
                    stat_encode_eth_call_data_chunck_size_t( nStartMessageIdx + idxMessage );
                // encode value of joMessageToSign.sender as "sender" call argument
                strCallData += stat_encode_eth_call_data_chunck_address(
                    joMessageToSign["sender"].get< std::string >() );
                // encode value of joMessageToSign.destinationContract as "destinationContract" call
                // argument
                strCallData += stat_encode_eth_call_data_chunck_address(
                    joMessageToSign["destinationContract"].get< std::string >() );
                // encode value of joMessageToSign.to as "to" call argument
                strCallData += stat_encode_eth_call_data_chunck_address(
                    joMessageToSign["to"].get< std::string >() );
                // encode value of joMessageToSign.amount as "amount" call argument
                strCallData += stat_encode_eth_call_data_chunck_size_t(
                    joMessageToSign["amount"].get< std::string >() );
                // encode length of chainName string
                std::string strTargetChainName =
                    ( strDirection == "M2S" ) ? strSChainName : "Mainnet";
                size_t nLenTargetName = strTargetChainName.size();
                strCallData += stat_encode_eth_call_data_chunck_size_t( nLenTargetName );
                // encode data of chainName string
                for ( size_t idxChar = 0; idxChar < nLenTargetName; ++idxChar ) {
                    std::string strByte =
                        skutils::tools::format( "%02x", strTargetChainName[idxChar] );
                    strCallData += strByte;
                }
                size_t nLastPart = nLenTargetName % 32;
                if ( nLastPart != 0 ) {
                    size_t nNeededToAdd = 32 - nLastPart;
                    for ( size_t idxChar = 0; idxChar < nNeededToAdd; ++idxChar ) {
                        strCallData += "00";
                    }
                }
                //
                nlohmann::json joCallItem = nlohmann::json::object();
                joCallItem["data"] = strCallData;  // call data
                //
                //
                //
                //
                //
                // joCallItem["from"] = ( strDirection == "M2S" ) ?
                //                         strImaCallerAddressMainNetLC :
                //                         strImaCallerAddressSChainLC;  // caller address
                joCallItem["from"] =
                    ( strDirection == "M2S" ) ?
                        strAddressImaMessageProxyMainNetLC :
                        strAddressImaMessageProxySChainLC;  // message proxy address
                //
                //
                //
                //
                //
                joCallItem["to"] = ( strDirection == "M2S" ) ?
                                       strAddressImaMessageProxyMainNetLC :
                                       strAddressImaMessageProxySChainLC;  // message proxy address
                nlohmann::json jarrParams = nlohmann::json::array();
                jarrParams.push_back( joCallItem );
                jarrParams.push_back( std::string( "latest" ) );
                nlohmann::json joCall = nlohmann::json::object();
                joCall["jsonrpc"] = "2.0";
                joCall["method"] = "eth_call";
                joCall["params"] = jarrParams;
                //
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + cc::debug( " Will send " ) +
                           cc::notice( "message verification query" ) + cc::debug( " to " ) +
                           cc::notice( "message proxy" ) + cc::debug( " smart contract for " ) +
                           cc::info( strDirection ) + cc::debug( " message: " ) + cc::j( joCall ) );
                if ( strDirection == "M2S" ) {
                    skutils::rest::client cli( urlMainNet );
                    skutils::rest::data_t d = cli.call( joCall );
                    if ( d.empty() )
                        throw std::runtime_error(
                            strDirection +
                            " eth_call to MessageProxy failed, empty data returned" );
                    nlohmann::json joResult;
                    try {
                        joResult = nlohmann::json::parse( d.s_ )["result"];
                        if ( joResult.is_string() ) {
                            std::string strResult = joResult.get< std::string >();
                            clog( VerbosityDebug, "IMA" )
                                << ( strLogPrefix + " " +
                                       cc::debug( "Transaction verification got (raw) result: " ) +
                                       cc::info( strResult ) );
                            if ( !strResult.empty() ) {
                                dev::u256 uResult( strResult ), uZero( "0" );
                                if ( uResult != uZero )
                                    bTransactionWasVerifed = true;
                            }
                        }
                        if ( !bTransactionWasVerifed )
                            clog( VerbosityDebug, "IMA" )
                                << ( cc::info( strDirection ) + cc::error( " eth_call to " ) +
                                       cc::info( "MessageProxy" ) +
                                       cc::error( " failed with returned data answer: " ) +
                                       cc::j( joResult ) );
                    } catch ( ... ) {
                        clog( VerbosityDebug, "IMA" )
                            << ( cc::info( strDirection ) + cc::error( " eth_call to " ) +
                                   cc::info( "MessageProxy" ) +
                                   cc::error( " failed with non-parse-able data answer: " ) +
                                   cc::warn( d.s_ ) );
                    }
                }  // if ( strDirection == "M2S" )
                else {
                    try {
                        std::string strCallToConvert = joCallItem.dump();  // joCall.dump();
                        Json::Value _json;
                        Json::Reader().parse( strCallToConvert, _json );
                        // TODO: We ignore block number in order to be compatible with Metamask
                        // (SKALE-430). Remove this temporary fix.
                        std::string blockNumber = "latest";
                        dev::eth::TransactionSkeleton t = dev::eth::toTransactionSkeleton( _json );
                        // setTransactionDefaults( t ); // l_sergiy: we don't need this here for now
                        dev::eth::ExecutionResult er = client()->call( t.from, t.value, t.to,
                            t.data, t.gas, t.gasPrice, dev::eth::FudgeFactor::Lenient );
                        std::string strRevertReason;
                        if ( er.excepted == dev::eth::TransactionException::RevertInstruction ) {
                            strRevertReason = skutils::eth::call_error_message_2_str( er.output );
                            if ( strRevertReason.empty() )
                                strRevertReason =
                                    "EVM revert instruction without description message";
                            clog( VerbosityDebug, "IMA" )
                                << ( cc::info( strDirection ) + cc::error( " eth_call to " ) +
                                       cc::info( "MessageProxy" ) +
                                       cc::error( " failed with revert reason: " ) +
                                       cc::warn( strRevertReason ) + cc::error( ", " ) +
                                       cc::info( "blockNumber" ) + cc::error( "=" ) +
                                       cc::bright( blockNumber ) );
                        } else {
                            std::string strResult = toJS( er.output );
                            clog( VerbosityDebug, "IMA" )
                                << ( strLogPrefix + " " +
                                       cc::debug( "Transaction verification got (raw) result: " ) +
                                       cc::info( strResult ) );
                            if ( !strResult.empty() ) {
                                dev::u256 uResult( strResult ), uZero( "0" );
                                if ( uResult != uZero )
                                    bTransactionWasVerifed = true;
                            }
                        }
                    } catch ( std::exception const& ex ) {
                        clog( VerbosityDebug, "IMA" )
                            << ( cc::info( strDirection ) + cc::error( " eth_call to " ) +
                                   cc::info( "MessageProxy" ) +
                                   cc::error( " failed with exception: " ) +
                                   cc::warn( ex.what() ) );
                    } catch ( ... ) {
                        clog( VerbosityDebug, "IMA" )
                            << ( cc::info( strDirection ) + cc::error( " eth_call to " ) +
                                   cc::info( "MessageProxy" ) +
                                   cc::error( " failed with exception: " ) +
                                   cc::warn( "unknown exception" ) );
                    }
                }  // else from if( strDirection == "M2S" )
                if ( !bTransactionWasVerifed ) {
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + " " +
                               cc::error(
                                   "Transaction verification was not passed for IMA message " ) +
                               cc::size10( nStartMessageIdx + idxMessage ) + cc::error( "." ) );
                    throw std::runtime_error(
                        "Transaction verification was not passed for IMA message " +
                        std::to_string( nStartMessageIdx + idxMessage ) );
                }  // if ( !bTransactionWasVerifed )
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + cc::success( " Success, IMA message " ) +
                           cc::size10( nStartMessageIdx + idxMessage ) +
                           cc::success( " was verified via call to MessageProxy." ) );
            }  // if( bIsImaMessagesViaContractCall )
            else {
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + " " +
                           cc::warn(
                               "Skipped contract call based verification of IMA message(s)" ) );
            }  // else from if( bIsImaMessagesViaContractCall )

            //
            //
            //
            if ( !bOnlyVerify ) {
                // One more message is valid, concatenate it for further in-wallet signing
                // Compose message to sign
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
                vecAllTogetherMessages.insert(
                    vecAllTogetherMessages.end(), arr + 0, arr + cntArr );
                //
                v = dev::fromHex( strMessageData, dev::WhenError::DontThrow );
                // fnInvert( v.data(), v.size() ); // do not invert byte order data field (see
                // SKALE-3554 for details)
                vecAllTogetherMessages.insert( vecAllTogetherMessages.end(), v.begin(), v.end() );
            }  // if( !bOnlyVerify )
        }      // for ( size_t idxMessage = 0; idxMessage < cntMessagesToSign; ++idxMessage ) {

        if ( !bOnlyVerify ) {
            //
            //
            const dev::h256 h = dev::sha3( vecAllTogetherMessages );
            const std::string sh = h.hex();
            clog( VerbosityDebug, "IMA" )
                << ( strLogPrefix + cc::debug( " Got hash to sign " ) + cc::info( sh ) );
            //
            // If we are here, then all IMA messages are valid
            // Perform call to wallet to sign messages
            //
            clog( VerbosityDebug, "IMA" )
                << ( strLogPrefix + cc::debug( " Calling wallet to sign " ) + cc::notice( sh ) +
                       cc::debug( " composed from " ) +
                       cc::binary_singleline( ( void* ) vecAllTogetherMessages.data(),
                           vecAllTogetherMessages.size(), "" ) +
                       cc::debug( "...`" ) );
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
            clog( VerbosityDebug, "IMA" )
                << ( strLogPrefix + cc::debug( " Contacting " ) + cc::notice( "SGX Wallet" ) +
                       cc::debug( " server at " ) + cc::u( u ) );
            clog( VerbosityDebug, "IMA" )
                << ( strLogPrefix + cc::debug( " Will send " ) + cc::notice( "sign query" ) +
                       cc::debug( " to wallet: " ) + cc::j( joCall ) );
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
            clog( VerbosityDebug, "IMA" )
                << ( strLogPrefix + cc::success( " Success, got " ) + cc::notice( "sign result" ) +
                       cc::success( " from wallet: " ) + cc::j( joSignResult ) );
            Json::Value ret;
            Json::Reader().parse( s, ret );
            return ret;
        }  // if ( !bOnlyVerify )
        else {
            nlohmann::json jo = nlohmann::json::object();
            jo["success"] = true;
            std::string s = jo.dump();
            clog( VerbosityDebug, "IMA" )
                << ( strLogPrefix + cc::success( " Success, verification passed" ) );
            Json::Value ret;
            Json::Reader().parse( s, ret );
            return ret;
        }  // else from if ( !bOnlyVerify )
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA Verify and Sign" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA Verify and Sign" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA Verify and Sign" ) +
                   cc::error( ", exception information: " ) + cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}  // skale_imaVerifyAndSign()

Json::Value SkaleStats::skale_imaBroadcastTxnInsert( const Json::Value& request ) {
    std::string strLogPrefix = cc::deep_info( "IMA broadcast TXN insert" );
    try {
        Json::FastWriter fastWriter;
        const std::string strRequest = fastWriter.write( request );
        const nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        //
        dev::tracking::txn_entry txe;
        if ( !txe.fromJSON( joRequest ) )
            throw std::runtime_error(
                std::string( "failed to construct tracked IMA TXN entry from " ) +
                joRequest.dump() );
        bool wasInserted = insert( txe, false );
        //
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = wasInserted;
        std::string s = jo.dump();
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + " " +
                   ( wasInserted ? cc::success( "Inserted new" ) : cc::warn( "Skipped new" ) ) +
                   " " + cc::notice( "broadcasted" ) + cc::debug( " IMA TXN " ) +
                   cc::info( dev::toJS( txe.hash_ ) ) );
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) +
                   cc::info( "IMA broadcast TXN insert" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) +
                   cc::info( "IMA broadcast TXN insert" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) +
                   cc::info( "IMA broadcast TXN insert" ) +
                   cc::error( ", exception information: " ) + cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

Json::Value SkaleStats::skale_imaBroadcastTxnErase( const Json::Value& request ) {
    std::string strLogPrefix = cc::deep_info( "IMA broadcast TXN erase" );
    try {
        Json::FastWriter fastWriter;
        const std::string strRequest = fastWriter.write( request );
        const nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        //
        dev::tracking::txn_entry txe;
        if ( !txe.fromJSON( joRequest ) )
            throw std::runtime_error(
                std::string( "failed to construct tracked IMA TXN entry from " ) +
                joRequest.dump() );
        bool wasErased = erase( txe, false );
        //
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = wasErased;
        std::string s = jo.dump();
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + " " +
                   ( wasErased ? cc::success( "Erased existing" ) :
                                 cc::warn( "Skipped erasing" ) ) +
                   " " + cc::notice( "broadcasted" ) + cc::debug( " IMA TXN " ) +
                   cc::info( dev::toJS( txe.hash_ ) ) );
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) +
                   cc::info( "IMA broadcast TXN erase" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) +
                   cc::info( "IMA broadcast TXN erase" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) +
                   cc::info( "IMA broadcast TXN erase" ) +
                   cc::error( ", exception information: " ) + cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

Json::Value SkaleStats::skale_imaTxnInsert( const Json::Value& request ) {
    std::string strLogPrefix = cc::deep_info( "IMA TXN insert" );
    try {
        Json::FastWriter fastWriter;
        const std::string strRequest = fastWriter.write( request );
        const nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        //
        dev::tracking::txn_entry txe;
        if ( !txe.fromJSON( joRequest ) )
            throw std::runtime_error(
                std::string( "failed to construct tracked IMA TXN entry from " ) +
                joRequest.dump() );
        bool wasInserted = insert( txe, true );
        //
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = wasInserted;
        std::string s = jo.dump();
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + " " +
                   ( wasInserted ? cc::success( "Inserted new" ) : cc::warn( "Skipped new" ) ) +
                   " " + cc::notice( "reported" ) + cc::debug( " IMA TXN " ) +
                   cc::info( dev::toJS( txe.hash_ ) ) );
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN insert" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN insert" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN insert" ) +
                   cc::error( ", exception information: " ) + cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

Json::Value SkaleStats::skale_imaTxnErase( const Json::Value& request ) {
    std::string strLogPrefix = cc::deep_info( "IMA TXN erase" );
    try {
        Json::FastWriter fastWriter;
        const std::string strRequest = fastWriter.write( request );
        const nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        //
        dev::tracking::txn_entry txe;
        if ( !txe.fromJSON( joRequest ) )
            throw std::runtime_error(
                std::string( "failed to construct tracked IMA TXN entry from " ) +
                joRequest.dump() );
        bool wasErased = erase( txe, true );
        //
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = wasErased;
        std::string s = jo.dump();
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + " " +
                   ( wasErased ? cc::success( "Erased existing" ) :
                                 cc::warn( "Skipped erasing" ) ) +
                   " " + cc::notice( "reported" ) + cc::debug( " IMA TXN " ) +
                   cc::info( dev::toJS( txe.hash_ ) ) );
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN erase" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN erase" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN erase" ) +
                   cc::error( ", exception information: " ) + cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

Json::Value SkaleStats::skale_imaTxnClear( const Json::Value& /*request*/ ) {
    std::string strLogPrefix = cc::deep_info( "IMA TXN clear" );
    try {
        clear();
        //
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = true;
        std::string s = jo.dump();
        clog( VerbosityDebug, "IMA" ) << ( strLogPrefix + " " + cc::success( "Cleared all" ) + " " +
                                           cc::notice( "reported" ) + cc::debug( " IMA TXNs" ) );
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN clear" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN clear" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN clear" ) +
                   cc::error( ", exception information: " ) + cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

Json::Value SkaleStats::skale_imaTxnFind( const Json::Value& request ) {
    std::string strLogPrefix = cc::deep_info( "IMA TXN find" );
    try {
        Json::FastWriter fastWriter;
        const std::string strRequest = fastWriter.write( request );
        const nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        //
        dev::tracking::txn_entry txe;
        if ( !txe.fromJSON( joRequest ) )
            throw std::runtime_error(
                std::string( "failed to construct tracked IMA TXN entry from " ) +
                joRequest.dump() );
        bool wasFound = find( txe );
        //
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = wasFound;
        std::string s = jo.dump();
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + " " +
                   ( wasFound ? cc::success( "Found" ) : cc::warn( "Not found" ) ) +
                   cc::debug( " IMA TXN " ) + cc::info( dev::toJS( txe.hash_ ) ) );
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN find" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN find" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN find" ) +
                   cc::error( ", exception information: " ) + cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

Json::Value SkaleStats::skale_imaTxnListAll( const Json::Value& /*request*/ ) {
    std::string strLogPrefix = cc::deep_info( "IMA TXN list-all" );
    try {
        dev::tracking::pending_ima_txns::list_txns_t lst;
        list_all( lst );
        nlohmann::json jarr = nlohmann::json::array();
        for ( const dev::tracking::txn_entry& txe : lst ) {
            jarr.push_back( txe.toJSON() );
        }
        //
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = true;
        jo["allTrackedTXNs"] = jarr;
        std::string s = jo.dump();
        clog( VerbosityDebug, "IMA" ) << ( strLogPrefix + " " + cc::debug( "Listed " ) +
                                           cc::size10( lst.size() ) + cc::debug( " IMA TXN(s)" ) );
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN list-all" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN list-all" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN list-all" ) +
                   cc::error( ", exception information: " ) + cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

};  // namespace rpc
};  // namespace dev
