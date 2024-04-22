/*
    Copyright (C) 2018-present, SKALE Labs

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
/**
 * @file httpserveroverride.cpp
 * @author Dima Litvinov
 * @date 2018
 */

#include "httpserveroverride.h"

#include <libdevcore/microprofile.h>

#include <libdevcore/Common.h>
#include <libdevcore/Log.h>

#include <jsonrpccpp/common/specificationparser.h>

#include <cassert>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <list>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <arpa/inet.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <libdevcore/CommonData.h>
#include <libethashseal/EthashClient.h>
#include <libethcore/CommonJS.h>
#include <libethereum/Client.h>
#include <libweb3jsonrpc/JsonHelper.h>

#if ( defined MSIZE )
#undef MSIZE
#endif

#include <libethereum/Block.h>
#include <libethereum/Transaction.h>
#include <libweb3jsonrpc/Eth.h>
#include <libweb3jsonrpc/Skale.h>

#include <skutils/eth_utils.h>
#include <skutils/multithreading.h>
#include <skutils/network.h>
#include <skutils/task_performance.h>
#include <skutils/url.h>

#include <iostream>


#if INT_MAX == 32767
// 16 bits
#define SKALED_WS_SUBSCRIPTION_TYPE_MASK 0xF000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION 0x1000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK 0x2000
#define SKALED_WS_SUBSCRIPTION_TYPE_SKALE_STATS 0x3000
#elif INT_MAX == 2147483647
// 32 bits
#define SKALED_WS_SUBSCRIPTION_TYPE_MASK 0xF0000000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION 0x10000000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK 0x20000000
#define SKALED_WS_SUBSCRIPTION_TYPE_SKALE_STATS 0x30000000
#elif INT_MAX == 9223372036854775807
// 64 bits
#define SKALED_WS_SUBSCRIPTION_TYPE_MASK 0xF000000000000000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION 0x1000000000000000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK 0x2000000000000000
#define SKALED_WS_SUBSCRIPTION_TYPE_SKALE_STATS 0x3000000000000000
#else
#error "What kind of weird system are you on? We cannot detect size of int"
#endif

namespace fs = boost::filesystem;

namespace skale {
namespace server {
namespace helper {

dev::Verbosity dv_from_ws_msg_type( skutils::ws::e_ws_log_message_type_t eWSLMT ) {
    dev::Verbosity dv = dev::Verbosity::VerbosityTrace;
    switch ( eWSLMT ) {
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_debug:
        dv = dev::Verbosity::VerbosityDebug;
        break;
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_info:
        dv = dev::Verbosity::VerbosityDebug;  // VerbosityInfo
        break;
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_warning:
        dv = dev::Verbosity::VerbosityWarning;
        break;
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_error:
        dv = dev::Verbosity::VerbosityWarning;
        break;
    default:
        dv = dev::Verbosity::VerbosityError;
        break;
    }
    return dv;
}

dev::eth::LogFilter toLogFilter( const nlohmann::json& jo ) {
    dev::eth::LogFilter filter;
    if ( ( !jo.is_object() ) || jo.size() == 0 )
        return filter;

    // check only !empty. it should throw exceptions if input params are incorrect
    if ( jo.count( "fromBlock" ) > 0 )
        filter.withEarliest( dev::eth::jsToBlockNumber( jo["fromBlock"].get< std::string >() ) );
    if ( jo.count( "toBlock" ) > 0 )
        filter.withLatest( dev::eth::jsToBlockNumber( jo["toBlock"].get< std::string >() ) );
    if ( jo.count( "address" ) > 0 ) {
        if ( jo["address"].is_array() )
            for ( auto i : jo["address"] )
                filter.address( dev::jsToAddress( i.get< std::string >() ) );
        else
            filter.address( dev::jsToAddress( jo["address"].get< std::string >() ) );
    }
    if ( jo.count( "topics" ) > 0 )
        for ( unsigned i = 0; i < jo["topics"].size(); i++ ) {
            if ( jo["topics"][i].is_array() ) {
                for ( auto t : jo["topics"][i] )
                    if ( !t.is_null() )
                        filter.topic( i, dev::jsToFixed< 32 >( t.get< std::string >() ) );
            } else if ( !jo["topics"][i].is_null() )  // if it is anything else then string, it
                                                      // should and will fail
                filter.topic( i, dev::jsToFixed< 32 >( jo["topics"][i].get< std::string >() ) );
        }
    return filter;
}

nlohmann::json nljsBlockNumber( dev::eth::BlockNumber uBlockNumber ) {
    if ( uBlockNumber == dev::eth::LatestBlock )
        return nlohmann::json( "latest" );
    if ( uBlockNumber == 0 )
        return nlohmann::json( "earliest" );
    if ( uBlockNumber == dev::eth::PendingBlock )
        return nlohmann::json( "pending" );
    return dev::toJS( uBlockNumber );
}

nlohmann::json toJson( std::unordered_map< dev::h256, dev::eth::LocalisedLogEntries > const& eb,
    std::vector< dev::h256 > const& order ) {
    nlohmann::json res = nlohmann::json::array();
    for ( auto const& i : order ) {
        auto entries = eb.at( i );
        nlohmann::json currentBlock = nlohmann::json::object();
        dev::eth::LocalisedLogEntry entry = entries[0];
        if ( entry.mined ) {
            currentBlock["blockNumber"] = nljsBlockNumber( entry.blockNumber );
            currentBlock["blockHash"] = toJS( entry.blockHash );
            currentBlock["type"] = "mined";
        } else
            currentBlock["type"] = "pending";
        currentBlock["polarity"] = entry.polarity == dev::eth::BlockPolarity::Live ? true : false;
        currentBlock["logs"] = nlohmann::json::array();
        for ( dev::eth::LocalisedLogEntry const& e : entries ) {
            nlohmann::json log = nlohmann::json::object();
            log["logIndex"] = e.logIndex;
            log["transactionIndex"] = e.transactionIndex;
            log["transactionHash"] = toJS( e.transactionHash );
            log["address"] = dev::toJS( e.address );
            log["data"] = dev::toJS( e.data );
            log["topics"] = nlohmann::json::array();
            for ( auto const& t : e.topics )
                log["topics"].push_back( dev::toJS( t ) );
            currentBlock["logs"].push_back( log );
        }
        res.push_back( currentBlock );
    }
    return res;
}

nlohmann::json toJsonByBlock( dev::eth::LocalisedLogEntries const& le ) {
    std::vector< dev::h256 > order;
    std::unordered_map< dev::h256, dev::eth::LocalisedLogEntries > entriesByBlock;
    for ( dev::eth::LocalisedLogEntry const& e : le ) {
        if ( e.isSpecial )  // skip special log
            continue;
        if ( entriesByBlock.count( e.blockHash ) == 0 ) {
            entriesByBlock[e.blockHash] = dev::eth::LocalisedLogEntries();
            order.push_back( e.blockHash );
        }
        entriesByBlock[e.blockHash].push_back( e );
    }
    return toJson( entriesByBlock, order );
}

bool checkParamsPresent(
    const char* strMethodName, const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( joRequest.count( "params" ) > 0 )
        return true;
    nlohmann::json joError = nlohmann::json::object();
    joError["code"] = -32602;
    joError["message"] = std::string( "error in \"" ) + strMethodName +
                         "\" rpc method, json entry \"params\" is missing";
    joResponse["error"] = joError;
    return false;
}

bool checkParamsIsArray(
    const char* strMethodName, const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( !checkParamsPresent( strMethodName, joRequest, joResponse ) )
        return false;
    const nlohmann::json& jarrParams = joRequest["params"];
    if ( jarrParams.is_array() )
        return true;
    nlohmann::json joError = nlohmann::json::object();
    joError["code"] = -32602;
    joError["message"] = std::string( "error in \"" ) + strMethodName +
                         "\" rpc method, json entry \"params\" must be array";
    joResponse["error"] = joError;
    return false;
}

bool checkParamsIsObject( const char* strMethodName, const rapidjson::Document& joRequest,
    rapidjson::Document& joResponse ) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer< rapidjson::StringBuffer > writer( buffer );
    joRequest.Accept( writer );
    std::string strRequest = buffer.GetString();
    nlohmann::json objRequest = nlohmann::json::parse( strRequest );

    nlohmann::json objResponse;
    if ( !checkParamsPresent( strMethodName, objRequest, objResponse ) ) {
        std::string strResponse = objResponse.dump();
        joResponse.Parse( strResponse.data() );
        return false;
    }
    if ( joRequest["params"].IsObject() )
        return true;
    rapidjson::Value joError;
    joError.SetObject();
    joError.AddMember( "code", -32602, joResponse.GetAllocator() );
    joError.AddMember( "message",
        rapidjson::StringRef( std::string( std::string( "error in \"" ) + strMethodName +
                                           "\" rpc method, json entry \"params\" must be object" )
                                  .c_str() ),
        joResponse.GetAllocator() );
    joError.AddMember( "error", joError, joResponse.GetAllocator() );
    return false;
}

};  // namespace helper
};  // namespace server
};  // namespace skale


namespace stats {

typedef skutils::multithreading::recursive_mutex_type mutex_type_stats;
typedef std::lock_guard< mutex_type_stats > lock_type_stats;
static skutils::multithreading::recursive_mutex_type g_mtx_stats( "RMTX-NMA-PEER-ALL" );

struct map_method_call_stats_t
    : public std::map< std::string, skutils::stats::named_event_stats* > {
    typedef std::map< std::string, skutils::stats::named_event_stats* > my_base_t;
    void clear() {
        iterator itWalk = begin(), itEnd = end();
        for ( ; itWalk != itEnd; ++itWalk ) {
            skutils::stats::named_event_stats* x = itWalk->second;
            if ( x )
                delete x;
        }
        my_base_t::clear();
    }
    map_method_call_stats_t() = default;
    ~map_method_call_stats_t() { clear(); }
    map_method_call_stats_t( const map_method_call_stats_t& ) = delete;
    map_method_call_stats_t( map_method_call_stats_t&& ) = delete;
    map_method_call_stats_t& operator=( const map_method_call_stats_t& ) = delete;
    map_method_call_stats_t& operator=( map_method_call_stats_t&& ) = delete;
};

map_method_call_stats_t g_map_method_call_stats;
map_method_call_stats_t g_map_method_answer_stats;
map_method_call_stats_t g_map_method_error_stats;
map_method_call_stats_t g_map_method_exception_stats;
map_method_call_stats_t g_map_method_traffic_stats_in;
map_method_call_stats_t g_map_method_traffic_stats_out;
static size_t g_nSizeDefaultOnQueueAdd = 10;

static skutils::stats::named_event_stats& stat_subsystem_call_queue( const char* strSubSystem ) {
    lock_type_stats lock( g_mtx_stats );
    map_method_call_stats_t::iterator itFind = g_map_method_call_stats.find( strSubSystem ),
                                      itEnd = g_map_method_call_stats.end();
    if ( itFind != itEnd ) {
        skutils::stats::named_event_stats* x = itFind->second;
        if ( x )
            return ( *x );
    }
    skutils::stats::named_event_stats* x = new skutils::stats::named_event_stats;
    g_map_method_call_stats[strSubSystem] = x;
    return ( *x );
}
static skutils::stats::named_event_stats& stat_subsystem_answer_queue( const char* strSubSystem ) {
    lock_type_stats lock( g_mtx_stats );
    map_method_call_stats_t::iterator itFind = g_map_method_answer_stats.find( strSubSystem ),
                                      itEnd = g_map_method_answer_stats.end();
    if ( itFind != itEnd ) {
        skutils::stats::named_event_stats* x = itFind->second;
        if ( x )
            return ( *x );
    }
    skutils::stats::named_event_stats* x = new skutils::stats::named_event_stats;
    g_map_method_answer_stats[strSubSystem] = x;
    return ( *x );
}
static skutils::stats::named_event_stats& stat_subsystem_error_queue( const char* strSubSystem ) {
    lock_type_stats lock( g_mtx_stats );
    map_method_call_stats_t::iterator itFind = g_map_method_error_stats.find( strSubSystem ),
                                      itEnd = g_map_method_error_stats.end();
    if ( itFind != itEnd ) {
        skutils::stats::named_event_stats* x = itFind->second;
        if ( x )
            return ( *x );
    }
    skutils::stats::named_event_stats* x = new skutils::stats::named_event_stats;
    g_map_method_error_stats[strSubSystem] = x;
    return ( *x );
}
static skutils::stats::named_event_stats& stat_subsystem_exception_queue(
    const char* strSubSystem ) {
    lock_type_stats lock( g_mtx_stats );
    map_method_call_stats_t::iterator itFind = g_map_method_exception_stats.find( strSubSystem ),
                                      itEnd = g_map_method_exception_stats.end();
    if ( itFind != itEnd ) {
        skutils::stats::named_event_stats* x = itFind->second;
        if ( x )
            return ( *x );
    }
    skutils::stats::named_event_stats* x = new skutils::stats::named_event_stats;
    g_map_method_exception_stats[strSubSystem] = x;
    return ( *x );
}

static skutils::stats::named_event_stats& stat_subsystem_traffic_queue_in(
    const char* strSubSystem ) {
    lock_type_stats lock( g_mtx_stats );
    const auto itFind = g_map_method_traffic_stats_in.find( strSubSystem );
    if ( itFind != std::end( g_map_method_traffic_stats_in ) ) {
        skutils::stats::named_event_stats* x = itFind->second;
        if ( x )
            return ( *x );
    }
    skutils::stats::named_event_stats* x = new skutils::stats::named_event_stats;
    g_map_method_traffic_stats_in[strSubSystem] = x;
    return ( *x );
}
static skutils::stats::named_event_stats& stat_subsystem_traffic_queue_out(
    const char* strSubSystem ) {
    lock_type_stats lock( g_mtx_stats );
    const auto itFind = g_map_method_traffic_stats_out.find( strSubSystem );
    if ( itFind != std::end( g_map_method_traffic_stats_out ) ) {
        skutils::stats::named_event_stats* x = itFind->second;
        if ( x )
            return ( *x );
    }
    skutils::stats::named_event_stats* x = new skutils::stats::named_event_stats;
    g_map_method_traffic_stats_out[strSubSystem] = x;
    return ( *x );
}


void register_stats_message(
    const char* strSubSystem, const char* strMethodName, const size_t nJsonSize ) {
    lock_type_stats lock( g_mtx_stats );
    skutils::stats::named_event_stats& cq = stat_subsystem_call_queue( strSubSystem );
    cq.event_queue_add( strMethodName, g_nSizeDefaultOnQueueAdd );
    cq.event_add( strMethodName );
    skutils::stats::named_event_stats& tq = stat_subsystem_traffic_queue_in( strSubSystem );
    tq.event_queue_add( strMethodName, g_nSizeDefaultOnQueueAdd );
    tq.event_add( strMethodName, nJsonSize );
}
void register_stats_answer(
    const char* strSubSystem, const char* strMethodName, const size_t nJsonSize ) {
    lock_type_stats lock( g_mtx_stats );
    skutils::stats::named_event_stats& aq = stat_subsystem_answer_queue( strSubSystem );
    aq.event_queue_add( strMethodName, g_nSizeDefaultOnQueueAdd );
    aq.event_add( strMethodName );

    skutils::stats::named_event_stats& tq = stat_subsystem_traffic_queue_out( strSubSystem );
    tq.event_queue_add( strMethodName, g_nSizeDefaultOnQueueAdd );
    tq.event_add( strMethodName, nJsonSize );
}
void register_stats_error( const char* strSubSystem, const char* strMethodName ) {
    lock_type_stats lock( g_mtx_stats );
    skutils::stats::named_event_stats& eq = stat_subsystem_error_queue( strSubSystem );
    eq.event_queue_add( strMethodName, g_nSizeDefaultOnQueueAdd );
    eq.event_add( strMethodName );
}
void register_stats_exception( const char* strSubSystem, const char* strMethodName ) {
    lock_type_stats lock( g_mtx_stats );
    skutils::stats::named_event_stats& eq = stat_subsystem_exception_queue( strSubSystem );
    eq.event_queue_add( strMethodName, g_nSizeDefaultOnQueueAdd );
    eq.event_add( strMethodName );
}

void register_stats_message( const char* strSubSystem, const nlohmann::json& joMessage ) {
    std::string strMethodName = skutils::tools::getFieldSafe< std::string >( joMessage, "method" );
    std::string txt = joMessage.dump();
    size_t txt_len = txt.length();
    register_stats_message( strSubSystem, strMethodName.c_str(), txt_len );
}
void register_stats_answer(
    const char* strSubSystem, const nlohmann::json& joMessage, const nlohmann::json& joAnswer ) {
    std::string strMethodName = skutils::tools::getFieldSafe< std::string >( joMessage, "method" );
    std::string txt = joAnswer.dump();
    size_t txt_len = txt.length();
    register_stats_answer( strSubSystem, strMethodName.c_str(), txt_len );
}
void register_stats_error( const char* strSubSystem, const nlohmann::json& joMessage ) {
    std::string strMethodName = skutils::tools::getFieldSafe< std::string >( joMessage, "method" );
    register_stats_error( strSubSystem, strMethodName.c_str() );
}
void register_stats_exception( const char* strSubSystem, const nlohmann::json& joMessage ) {
    std::string strMethodName = skutils::tools::getFieldSafe< std::string >( joMessage, "method" );
    register_stats_exception( strSubSystem, strMethodName.c_str() );
}

static nlohmann::json generate_subsystem_stats( const char* strSubSystem ) {
    lock_type_stats lock( g_mtx_stats );
    nlohmann::json jo = nlohmann::json::object();
    skutils::stats::named_event_stats& cq = stat_subsystem_call_queue( strSubSystem );
    skutils::stats::named_event_stats& aq = stat_subsystem_answer_queue( strSubSystem );
    skutils::stats::named_event_stats& erq = stat_subsystem_error_queue( strSubSystem );
    skutils::stats::named_event_stats& exq = stat_subsystem_exception_queue( strSubSystem );
    skutils::stats::named_event_stats& tq_in = stat_subsystem_traffic_queue_in( strSubSystem );
    skutils::stats::named_event_stats& tq_out = stat_subsystem_traffic_queue_out( strSubSystem );
    std::set< std::string > setNames = cq.all_queue_names(), setNames_aq = aq.all_queue_names(),
                            setNames_erq = erq.all_queue_names(),
                            setNames_exq = exq.all_queue_names(),
                            setNames_tq_in = tq_in.all_queue_names(),
                            setNames_tq_out = tq_out.all_queue_names();
    std::set< std::string >::const_iterator itNameWalk, itNameEnd;
    for ( itNameWalk = setNames_aq.cbegin(), itNameEnd = setNames_aq.cend();
          itNameWalk != itNameEnd; ++itNameWalk )
        setNames.insert( *itNameWalk );
    for ( itNameWalk = setNames_erq.cbegin(), itNameEnd = setNames_erq.cend();
          itNameWalk != itNameEnd; ++itNameWalk )
        setNames.insert( *itNameWalk );
    for ( itNameWalk = setNames_exq.cbegin(), itNameEnd = setNames_exq.cend();
          itNameWalk != itNameEnd; ++itNameWalk )
        setNames.insert( *itNameWalk );
    for ( itNameWalk = setNames_tq_in.cbegin(), itNameEnd = setNames_tq_in.cend();
          itNameWalk != itNameEnd; ++itNameWalk )
        setNames.insert( *itNameWalk );
    for ( itNameWalk = setNames_tq_out.cbegin(), itNameEnd = setNames_tq_out.cend();
          itNameWalk != itNameEnd; ++itNameWalk )
        setNames.insert( *itNameWalk );
    for ( itNameWalk = setNames.cbegin(), itNameEnd = setNames.cend(); itNameWalk != itNameEnd;
          ++itNameWalk ) {
        const std::string& strMethodName = ( *itNameWalk );
        size_t nCalls = 0, nAnswers = 0, nErrors = 0, nExceptions = 0;
        skutils::stats::bytes_count_t nBytesRecv = 0, nBytesSent = 0;
        skutils::stats::time_point tpNow = skutils::stats::clock::now();
        double lfCallsPerSecond = cq.compute_eps_smooth( strMethodName, tpNow, nullptr, &nCalls );
        double lfAnswersPerSecond =
            aq.compute_eps_smooth( strMethodName, tpNow, nullptr, &nAnswers );
        double lfErrorsPerSecond =
            erq.compute_eps_smooth( strMethodName, tpNow, nullptr, &nErrors );
        double lfExceptionsPerSecond =
            exq.compute_eps_smooth( strMethodName, tpNow, nullptr, &nExceptions );
        double lfBytesPerSecondRecv = tq_in.compute_eps_smooth( strMethodName, tpNow, &nBytesRecv );
        double lfBytesPerSecondSent =
            tq_out.compute_eps_smooth( strMethodName, tpNow, &nBytesSent );
        nlohmann::json joMethod = nlohmann::json::object();
        joMethod["cps"] = lfCallsPerSecond / g_nSizeDefaultOnQueueAdd;
        joMethod["aps"] = lfAnswersPerSecond / g_nSizeDefaultOnQueueAdd;
        joMethod["erps"] = lfErrorsPerSecond / g_nSizeDefaultOnQueueAdd;
        joMethod["exps"] = lfExceptionsPerSecond / g_nSizeDefaultOnQueueAdd;
        joMethod["bps_recv"] = lfBytesPerSecondRecv;
        joMethod["bps_sent"] = lfBytesPerSecondSent;
        joMethod["calls"] = nCalls;
        joMethod["answers"] = nAnswers;
        joMethod["errors"] = nErrors;
        joMethod["exceptions"] = nExceptions;
        joMethod["bytes_recv"] = nBytesRecv;
        joMethod["bytes_sent"] = nBytesSent;
        jo[strMethodName] = joMethod;
    }
    return jo;
}

};  // namespace stats


const char* esm2str( e_server_mode_t esm ) {
    switch ( esm ) {
    case e_server_mode_t::esm_informational:
        return "std";
    case e_server_mode_t::esm_standard:
    default:
        return "nfo";
    }
}


SkaleStatsSubscriptionManager::SkaleStatsSubscriptionManager() : next_subscription_( 1 ) {}
SkaleStatsSubscriptionManager::~SkaleStatsSubscriptionManager() {
    unsubscribeAll();
}

SkaleStatsSubscriptionManager::subscription_id_t
SkaleStatsSubscriptionManager::nextSubscriptionID() {
    lock_type lock( mtx_ );
    subscription_id_t idSubscription = next_subscription_;
    ++next_subscription_;
    return idSubscription;
}

bool SkaleStatsSubscriptionManager::subscribe(
    SkaleStatsSubscriptionManager::subscription_id_t& idSubscription, SkaleWsPeer* pPeer,
    size_t nIntervalMilliseconds ) {
    idSubscription = 0;
    if ( !pPeer )
        return false;
    if ( !pPeer->isConnected() )
        return false;
    static const size_t g_nIntervalMillisecondsMin = 500;
    if ( nIntervalMilliseconds < g_nIntervalMillisecondsMin )
        nIntervalMilliseconds = g_nIntervalMillisecondsMin;
    lock_type lock( mtx_ );
    idSubscription = nextSubscriptionID();
    subscription_data_t subscriptionData;
    subscriptionData.m_idSubscription = idSubscription;
    subscriptionData.m_pPeer = pPeer;
    subscriptionData.m_nIntervalMilliseconds = nIntervalMilliseconds;
    skutils::dispatch::repeat(
        subscriptionData.m_pPeer->m_strPeerQueueID,
        [=]() -> void {
            if ( subscriptionData.m_pPeer && subscriptionData.m_pPeer->isConnected() ) {
                nlohmann::json joParams = nlohmann::json::object();
                joParams["subscription"] = dev::toJS(
                    subscriptionData.m_idSubscription | SKALED_WS_SUBSCRIPTION_TYPE_SKALE_STATS );
                joParams["stats"] = getSSO().provideSkaleStats();
                nlohmann::json joNotification = nlohmann::json::object();
                joNotification["jsonrpc"] = "2.0";
                joNotification["method"] = "eth_subscription";
                joNotification["params"] = joParams;
                std::string strNotification = joNotification.dump();
                if ( getSSO().opts_.isTraceCalls_ )
                    clog( dev::VerbosityDebug,
                        cc::info( subscriptionData.m_pPeer->getRelay().nfoGetSchemeUC() ) )
                        << ( cc::ws_tx_inv( " <<< " +
                                            subscriptionData.m_pPeer->getRelay().nfoGetSchemeUC() +
                                            "/TX <<< " ) +
                               subscriptionData.m_pPeer->desc() + cc::ws_tx( " <<< " ) +
                               subscriptionData.m_pPeer->implPreformatTrafficJsonMessage(
                                   strNotification, false ) );
                skutils::dispatch::async( subscriptionData.m_pPeer->m_strPeerQueueID,
                    [subscriptionData, strNotification, idSubscription, this]() -> void {
                        bool bMessageSentOK = false;
                        try {
                            bMessageSentOK = subscriptionData.m_pPeer.get_unconst()->sendMessage(
                                skutils::tools::trim_copy( strNotification ) );
                            if ( !bMessageSentOK )
                                throw std::runtime_error(
                                    "eth_subscription/skaleStats failed to sent message" );
                            stats::register_stats_answer(
                                ( std::string( "RPC/" ) +
                                    subscriptionData.m_pPeer->getRelay().nfoGetSchemeUC() )
                                    .c_str(),
                                "eth_subscription/skaleStats", strNotification.size() );
                            stats::register_stats_answer(
                                "RPC", "eth_subscription/skaleStats", strNotification.size() );
                        } catch ( std::exception& ex ) {
                            clog( dev::Verbosity::VerbosityError,
                                cc::info( subscriptionData.m_pPeer->getRelay().nfoGetSchemeUC() ) +
                                    cc::debug( "/" ) +
                                    cc::num10(
                                        subscriptionData.m_pPeer->getRelay().serverIndex() ) )
                                << ( subscriptionData.m_pPeer->desc() + " " +
                                       cc::error( "error in " ) +
                                       cc::warn( "eth_subscription/skaleStats" ) +
                                       cc::error( " will uninstall watcher callback because of "
                                                  "exception: " ) +
                                       cc::warn( ex.what() ) );
                        } catch ( ... ) {
                            clog( dev::Verbosity::VerbosityError,
                                cc::info( subscriptionData.m_pPeer->getRelay().nfoGetSchemeUC() ) +
                                    cc::debug( "/" ) +
                                    cc::num10(
                                        subscriptionData.m_pPeer->getRelay().serverIndex() ) )
                                << ( subscriptionData.m_pPeer->desc() + " " +
                                       cc::error( "error in " ) +
                                       cc::warn( "eth_subscription/skaleStats" ) +
                                       cc::error( " will uninstall watcher callback because of "
                                                  "unknown exception" ) );
                        }
                        if ( !bMessageSentOK ) {
                            stats::register_stats_error(
                                ( std::string( "RPC/" ) +
                                    subscriptionData.m_pPeer->getRelay().nfoGetSchemeUC() )
                                    .c_str(),
                                "eth_subscription/skaleStats" );
                            stats::register_stats_error( "RPC", "eth_subscription/skaleStats" );
                            unsubscribe( idSubscription );
                        }
                    } );
                return;
            }
            if ( !subscriptionData.m_idDispatchJob.empty() )
                skutils::dispatch::stop( subscriptionData.m_idDispatchJob );
        },
        skutils::dispatch::duration_from_milliseconds( nIntervalMilliseconds ),
        &subscriptionData.m_idDispatchJob );
    map_subscriptions_[idSubscription] = subscriptionData;
    return true;
}

bool SkaleStatsSubscriptionManager::unsubscribe(
    const SkaleStatsSubscriptionManager::subscription_id_t& idSubscription ) {
    try {
        lock_type lock( mtx_ );
        map_subscriptions_t::iterator itFind = map_subscriptions_.find( idSubscription ),
                                      itEnd = map_subscriptions_.end();
        if ( itFind == itEnd )
            return false;
        subscription_data_t subscriptionData = itFind->second;
        map_subscriptions_.erase( itFind );
        if ( !subscriptionData.m_idDispatchJob.empty() )
            skutils::dispatch::stop( subscriptionData.m_idDispatchJob );
        return true;
    } catch ( ... ) {
        return false;
    }
}

void SkaleStatsSubscriptionManager::unsubscribeAll() {
    lock_type lock( mtx_ );
    std::list< subscription_id_t > lst;
    map_subscriptions_t::iterator itWalk = map_subscriptions_.begin();
    map_subscriptions_t::iterator itEnd = map_subscriptions_.end();
    for ( ; itWalk != itEnd; ++itWalk ) {
        subscription_id_t idSubscription = itWalk->first;
        lst.push_back( idSubscription );
    }
    for ( const subscription_id_t& idSubscription : lst ) {
        unsubscribe( idSubscription );
    }
}


SkaleServerConnectionsTrackHelper::SkaleServerConnectionsTrackHelper( SkaleServerOverride& sso )
    : m_sso( sso ) {
    m_sso.connection_counter_inc();
}

SkaleServerConnectionsTrackHelper::~SkaleServerConnectionsTrackHelper() {
    m_sso.connection_counter_dec();
}


SkaleWsPeer::SkaleWsPeer( skutils::ws::server& srv, const skutils::ws::hdl_t& hdl )
    : skutils::ws::peer( srv, hdl ),
      m_strPeerQueueID( skutils::dispatch::generate_id( this, "relay_peer" ) ) {
    SkaleServerOverride* pSO = pso();
    if ( pSO->opts_.isTraceCalls_ )
        clog( dev::VerbosityTrace, cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                                       cc::num10( getRelay().serverIndex() ) )
            << ( desc() + cc::notice( " peer ctor" ) );
}
SkaleWsPeer::~SkaleWsPeer() {
    SkaleServerOverride* pSO = pso();
    if ( pSO->opts_.isTraceCalls_ )
        clog( dev::VerbosityTrace, cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                                       cc::num10( getRelay().serverIndex() ) )
            << ( desc() + cc::notice( " peer dtor" ) );
    uninstallAllWatches();
    skutils::dispatch::remove( m_strPeerQueueID );
}

void SkaleWsPeer::register_ws_conn_for_origin() {
    if ( m_strUnDdosOrigin.empty() ) {
        SkaleServerOverride* pSO = pso();
        skutils::url url_unddos_origin( getRemoteIp() );
        try {
            m_strUnDdosOrigin = url_unddos_origin.host();
        } catch ( ... ) {
        }
        if ( m_strUnDdosOrigin.empty() )
            m_strUnDdosOrigin = "N/A";
        skutils::unddos::e_high_load_detection_result_t ehldr =
            pSO->unddos_.register_ws_conn_for_origin( m_strUnDdosOrigin );
        if ( ehldr != skutils::unddos::e_high_load_detection_result_t::ehldr_no_error ) {
            m_strUnDdosOrigin.clear();
            clog( dev::VerbosityError, cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                                           cc::num10( getRelay().serverIndex() ) )
                << ( desc() + " " + cc::fatal( "UN-DDOS:" ) + " " +
                       cc::error( " cannot accept connection - UN-DDOS protection reported "
                                  "connection count overflow" ) );
            close( "UN-DDOS protection reported connection count overflow" );
            throw std::runtime_error( "Cannot accept " + getRelay().nfoGetSchemeUC() +
                                      " connection from " + url_unddos_origin.str() +
                                      " - UN-DDOS protection reported connection count overflow" );
        }
    }
}
void SkaleWsPeer::unregister_ws_conn_for_origin() {
    if ( !m_strUnDdosOrigin.empty() ) {
        SkaleServerOverride* pSO = pso();
        pSO->unddos_.unregister_ws_conn_for_origin( m_strUnDdosOrigin );
        m_strUnDdosOrigin.clear();
    }
}

void SkaleWsPeer::onPeerRegister() {
    SkaleServerOverride* pSO = pso();
    if ( pSO->opts_.isTraceCalls_ )
        clog( dev::VerbosityDebug, cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                                       cc::num10( getRelay().serverIndex() ) )
            << ( desc() + cc::notice( " peer registered" ) );
    skutils::ws::peer::onPeerRegister();
    //
    // unddos
    register_ws_conn_for_origin();
}
void SkaleWsPeer::onPeerUnregister() {  // peer will no longer receive onMessage after call to
                                        // this
    m_pSSCTH.reset();
    SkaleServerOverride* pSO = pso();
    if ( pSO->opts_.isTraceCalls_ )
        clog( dev::VerbosityDebug, cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                                       cc::num10( getRelay().serverIndex() ) )
            << ( desc() + cc::notice( " peer unregistered" ) );
    skutils::ws::peer::onPeerUnregister();
    uninstallAllWatches();
    std::string strQueueIdToRemove = m_strPeerQueueID;
    skutils::dispatch::async( "ws-queue-remover", [strQueueIdToRemove]() -> void {
        skutils::dispatch::remove( strQueueIdToRemove );  // remove queue earlier
    } );
    // unddos
    unregister_ws_conn_for_origin();
}

void SkaleWsPeer::onMessage( const std::string& msg, skutils::ws::opcv eOpCode ) {
    SkaleServerOverride* pSO = pso();
    if ( pSO->isShutdownMode() ) {
        clog( dev::VerbosityWarning, cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                                         cc::num10( getRelay().serverIndex() ) )
            << ( cc::ws_rx_inv( " >>> " + getRelay().nfoGetSchemeUC() + "/" +
                                std::to_string( getRelay().serverIndex() ) + "/RX >>> " ) +
                   desc() + cc::ws_rx( " >>> " ) + cc::warn( "" ) );
        skutils::dispatch::remove( m_strPeerQueueID );  // remove queue earlier
        return;
    }
    if ( eOpCode != skutils::ws::opcv::text ) {
        // throw std::runtime_error( "only ws text messages are supported" );
        clog( dev::VerbosityWarning, cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                                         cc::num10( getRelay().serverIndex() ) )
            << ( cc::ws_rx_inv( " >>> " + getRelay().nfoGetSchemeUC() + "/" +
                                std::to_string( getRelay().serverIndex() ) + "/RX >>> " ) +
                   desc() + cc::ws_rx( " >>> " ) +
                   cc::error( " got binary message and will try to interpret it as text: " ) +
                   cc::warn( msg ) );
    }
    skutils::retain_release_ptr< SkaleWsPeer > pThis = this;
    nlohmann::json jarrRequest;
    std::string strMethod;
    nlohmann::json joID = "-1";
    bool isBatch = false;
    try {
        // fetch method name and id earlier
        nlohmann::json joRequestOriginal = nlohmann::json::parse( msg );
        if ( joRequestOriginal.is_array() ) {
            isBatch = true;
            jarrRequest = joRequestOriginal;
        } else {
            jarrRequest = nlohmann::json::array();
            jarrRequest.push_back( joRequestOriginal );
        }
        for ( const nlohmann::json& joRequest : jarrRequest ) {
            std::string strMethodWalk =
                skutils::tools::getFieldSafe< std::string >( joRequest, "method" );
            if ( strMethodWalk.empty() )
                throw std::runtime_error( "Bad JSON RPC request, \"method\" name is missing" );
            strMethod = strMethodWalk;
            if ( joRequest.count( "id" ) == 0 )
                throw std::runtime_error( "Bad JSON RPC request, \"id\" name is missing" );
            joID = joRequest["id"];
        }  // for( const nlohmann::json & joRequest : jarrRequest )
        if ( isBatch ) {
            size_t cntInBatch = jarrRequest.size();
            if ( cntInBatch > pSO->maxCountInBatchJsonRpcRequest_ )
                throw std::runtime_error( "Bad JSON RPC request, too much requests in batch" );
        }
    } catch ( ... ) {
        if ( strMethod.empty() ) {
            if ( isBatch )
                strMethod = "batch_json_rpc_request";
            else
                strMethod = "unknown_json_rpc_method";
        }
        std::string e = "Bad JSON RPC request: " + msg;
        clog( dev::VerbosityError, cc::info( pThis->getRelay().nfoGetSchemeUC() ) +
                                       cc::debug( "/" ) +
                                       cc::num10( pThis->getRelay().serverIndex() ) )
            << ( cc::ws_tx_inv( " !!! " + pThis->getRelay().nfoGetSchemeUC() + "/" +
                                std::to_string( pThis->getRelay().serverIndex() ) + "/ERR !!! " ) +
                   pThis->desc() + cc::ws_tx( " !!! " ) + cc::warn( e ) );
        nlohmann::json joErrorResponce;
        joErrorResponce["id"] = joID;
        nlohmann::json joErrorObj;
        joErrorObj["code"] = -32000;
        joErrorObj["message"] = std::string( e );
        joErrorResponce["error"] = joErrorObj;
        std::string strResponse = joErrorResponce.dump();
        stats::register_stats_exception(
            ( std::string( "RPC/" ) + pThis->getRelay().nfoGetSchemeUC() ).c_str(), "messages" );
        stats::register_stats_exception( pThis->getRelay().nfoGetSchemeUC().c_str(), "messages" );
        pThis.get_unconst()->sendMessage( skutils::tools::trim_copy( strResponse ) );
        stats::register_stats_answer(
            pThis->getRelay().nfoGetSchemeUC().c_str(), "messages", strResponse.size() );
        return;
    }
    //
    // unddos
    skutils::unddos::e_high_load_detection_result_t ehldr =
        pSO->unddos_.register_call_from_origin( m_strUnDdosOrigin, strMethod );
    switch ( ehldr ) {
    case skutils::unddos::e_high_load_detection_result_t::ehldr_peak:  // ban by too high load per
                                                                       // minute
    case skutils::unddos::e_high_load_detection_result_t::ehldr_lengthy:  // ban by too high load
                                                                          // per second
    case skutils::unddos::e_high_load_detection_result_t::ehldr_ban:      // still banned
    case skutils::unddos::e_high_load_detection_result_t::ehldr_bad_origin: {
        if ( strMethod.empty() )
            strMethod = isBatch ? "batch_json_rpc_request" : "unknown_json_rpc_method";
        std::string reason_part =
            ( ehldr == skutils::unddos::e_high_load_detection_result_t::ehldr_bad_origin &&
                ( !m_strUnDdosOrigin.empty() ) ) ?
                "bad origin" :
                "high load";
        std::string e = "Banned due to " + reason_part + " JSON RPC request: " + msg;
        clog( dev::VerbosityError, cc::info( pThis->getRelay().nfoGetSchemeUC() ) +
                                       cc::debug( "/" ) +
                                       cc::num10( pThis->getRelay().serverIndex() ) )
            << ( cc::ws_tx_inv( " !!! " + pThis->getRelay().nfoGetSchemeUC() + "/" +
                                std::to_string( pThis->getRelay().serverIndex() ) + "/ERR !!! " ) +
                   pThis->desc() + cc::ws_tx( " !!! " ) + cc::warn( e ) );
        nlohmann::json joErrorResponce;
        joErrorResponce["id"] = joID;
        nlohmann::json joErrorObj;
        joErrorObj["code"] = -32000;
        joErrorObj["message"] = std::string( e );
        joErrorResponce["error"] = joErrorObj;
        std::string strResponse = joErrorResponce.dump();
        stats::register_stats_exception(
            ( std::string( "RPC/" ) + pThis->getRelay().nfoGetSchemeUC() ).c_str(), "messages" );
        stats::register_stats_exception( pThis->getRelay().nfoGetSchemeUC().c_str(), "messages" );
        // stats::register_stats_exception( "RPC", strMethod.c_str() );
        pThis.get_unconst()->sendMessage( skutils::tools::trim_copy( strResponse ) );
        stats::register_stats_answer(
            pThis->getRelay().nfoGetSchemeUC().c_str(), "messages", strResponse.size() );
    }
        return;
    case skutils::unddos::e_high_load_detection_result_t::ehldr_no_error:
    default: {
        // no error
    } break;
    }  // switch( ehldr )
    //
    // WS-processing-lambda
    auto fnAsyncMessageHandler = [pThis, jarrRequest, pSO,
                                     isBatch]() -> void {  // WS-processing-lambda
        nlohmann::json jarrBatchAnswer;
        if ( isBatch )
            jarrBatchAnswer = nlohmann::json::array();
        for ( const nlohmann::json& joRequest : jarrRequest ) {
            std::string strRequest = joRequest.dump();
            std::string strMethod =
                skutils::tools::getFieldSafe< std::string >( joRequest, "method" );
            nlohmann::json joID = joRequest["id"];
            //
            std::string strPerformanceQueueName =
                skutils::tools::format( "rpc/%s/%zu/%s", pThis->getRelay().nfoGetSchemeUC().c_str(),
                    pThis->getRelay().serverIndex(), pThis->desc( false ).c_str() );
            std::string strPerformanceActionName =
                skutils::tools::format( "%s task %zu", pThis->getRelay().nfoGetSchemeUC().c_str(),
                    pThis.get_unconst()->nTaskNumberInPeer_++ );
            //
            skutils::stats::time_tracker::element_ptr_t rttElement;
            rttElement.emplace( "RPC", pThis->getRelay().nfoGetSchemeUC().c_str(),
                strMethod.c_str(), pThis->getRelay().serverIndex(), -1 );
            size_t nRequestSize = strRequest.size();
            //
            skutils::task::performance::action a(
                strPerformanceQueueName, strPerformanceActionName );
            if ( pSO->methodTraceVerbosity( strMethod ) != dev::VerbositySilent )
                clog( pSO->methodTraceVerbosity( strMethod ),
                    cc::info( pThis->getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                        cc::num10( pThis->getRelay().serverIndex() ) )
                    << ( cc::ws_rx_inv( " >>> " + pThis->getRelay().nfoGetSchemeUC() + "/" +
                                        std::to_string( pThis->getRelay().serverIndex() ) +
                                        "/RX >>> " ) +
                           pThis->desc() + cc::ws_rx( " >>> " ) +
                           pThis->implPreformatTrafficJsonMessage( joRequest, true ) );
            std::string strResponse;
            bool bPassed = false;
            try {
                stats::register_stats_message(
                    pThis->getRelay().nfoGetSchemeUC().c_str(), "messages", nRequestSize );
                stats::register_stats_message(
                    ( std::string( "RPC/" ) + pThis->getRelay().nfoGetSchemeUC() ).c_str(),
                    joRequest );
                stats::register_stats_message( "RPC", joRequest );

                if ( !pThis.get_unconst()->handleWebSocketSpecificRequest(
                         pThis->getRelay().esm_, joRequest, strResponse ) ) {
                    jsonrpc::IClientConnectionHandler* handler = pSO->GetHandler( "/" );
                    if ( handler == nullptr )
                        throw std::runtime_error( "No client connection handler found" );
                    handler->HandleRequest( strRequest, strResponse );
                }

                nlohmann::json joResponse = nlohmann::json::parse( strResponse );
                stats::register_stats_answer(
                    pThis->getRelay().nfoGetSchemeUC().c_str(), "messages", strResponse.size() );
                stats::register_stats_answer(
                    ( std::string( "RPC/" ) + pThis->getRelay().nfoGetSchemeUC() ).c_str(),
                    joRequest, joResponse );
                stats::register_stats_answer( "RPC", joRequest, joResponse );
                a.set_json_out( joResponse );
                bPassed = true;
            } catch ( const std::exception& ex ) {
                rttElement->setError();
                clog( dev::VerbosityError, cc::info( pThis->getRelay().nfoGetSchemeUC() ) +
                                               cc::debug( "/" ) +
                                               cc::num10( pThis->getRelay().serverIndex() ) )
                    << ( cc::ws_tx_inv( " !!! " + pThis->getRelay().nfoGetSchemeUC() + "/" +
                                        std::to_string( pThis->getRelay().serverIndex() ) +
                                        "/ERR !!! " ) +
                           pThis->desc() + cc::ws_tx( " !!! " ) + cc::warn( ex.what() ) );
                nlohmann::json joErrorResponce;
                joErrorResponce["id"] = joID;
                nlohmann::json joErrorObj;
                joErrorObj["code"] = -32000;
                joErrorObj["message"] = std::string( ex.what() );
                joErrorResponce["error"] = joErrorObj;
                strResponse = joErrorResponce.dump();
                stats::register_stats_exception(
                    ( std::string( "RPC/" ) + pThis->getRelay().nfoGetSchemeUC() ).c_str(), "" );
                if ( !strMethod.empty() ) {
                    stats::register_stats_exception(
                        pThis->getRelay().nfoGetSchemeUC().c_str(), "messages" );
                    stats::register_stats_exception( "RPC", strMethod.c_str() );
                }
                a.set_json_err( joErrorResponce );
            } catch ( ... ) {
                rttElement->setError();
                const char* e = "unknown exception in SkaleServerOverride";
                clog( dev::VerbosityError, cc::info( pThis->getRelay().nfoGetSchemeUC() ) +
                                               cc::debug( "/" ) +
                                               cc::num10( pThis->getRelay().serverIndex() ) )
                    << ( cc::ws_tx_inv( " !!! " + pThis->getRelay().nfoGetSchemeUC() + "/" +
                                        std::to_string( pThis->getRelay().serverIndex() ) +
                                        "/ERR !!! " ) +
                           pThis->desc() + cc::ws_tx( " !!! " ) + cc::warn( e ) );
                nlohmann::json joErrorResponce;
                joErrorResponce["id"] = joID;
                nlohmann::json joErrorObj;
                joErrorObj["code"] = -32000;
                joErrorObj["message"] = std::string( e );
                joErrorResponce["error"] = joErrorObj;
                strResponse = joErrorResponce.dump();
                stats::register_stats_exception(
                    ( std::string( "RPC/" ) + pThis->getRelay().nfoGetSchemeUC() ).c_str(),
                    "messages" );
                if ( !strMethod.empty() ) {
                    stats::register_stats_exception(
                        pThis->getRelay().nfoGetSchemeUC().c_str(), "messages" );
                    stats::register_stats_exception( "RPC", strMethod.c_str() );
                }
                a.set_json_err( joErrorResponce );
            }
            if ( pSO->methodTraceVerbosity( strMethod ) != dev::VerbositySilent )
                clog( pSO->methodTraceVerbosity( strMethod ),
                    cc::info( pThis->getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                        cc::num10( pThis->getRelay().serverIndex() ) )
                    << ( cc::ws_tx_inv( " <<< " + pThis->getRelay().nfoGetSchemeUC() + "/" +
                                        std::to_string( pThis->getRelay().serverIndex() ) +
                                        "/TX <<< " ) +
                           pThis->desc() + cc::ws_tx( " <<< " ) +
                           pThis->implPreformatTrafficJsonMessage( strResponse, false ) );
            if ( isBatch ) {
                nlohmann::json joAnswerPart = nlohmann::json::parse( strResponse );
                jarrBatchAnswer.push_back( joAnswerPart );
            } else
                pThis.get_unconst()->sendMessage( skutils::tools::trim_copy( strResponse ) );
            if ( !bPassed )
                stats::register_stats_answer(
                    pThis->getRelay().nfoGetSchemeUC().c_str(), "messages", strResponse.size() );
            rttElement->stop();
            double lfExecutionDuration = rttElement->getDurationInSeconds();  // in seconds
            if ( lfExecutionDuration >= pSO->opts_.lfExecutionDurationMaxForPerformanceWarning_ )
                pSO->logPerformanceWarning( lfExecutionDuration, -1,
                    pThis->getRelay().nfoGetSchemeUC().c_str(), pThis->getRelay().serverIndex(),
                    pThis->getRelay().esm_, pThis->getOrigin().c_str(), strMethod.c_str(), joID );
        }  // for( const nlohmann::json & joRequest : jarrRequest )
        if ( isBatch ) {
            std::string strResponse = jarrBatchAnswer.dump();
            pThis.get_unconst()->sendMessage( skutils::tools::trim_copy( strResponse ) );
        }
    };  // WS-processing-lambda
    skutils::dispatch::async( pThis->m_strPeerQueueID, fnAsyncMessageHandler );
}

void SkaleWsPeer::onClose(
    const std::string& reason, int local_close_code, const std::string& local_close_code_as_str ) {
    SkaleServerOverride* pSO = pso();
    if ( pSO->opts_.isTraceCalls_ )
        clog( dev::VerbosityDebug, cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                                       cc::num10( getRelay().serverIndex() ) )
            << ( desc() + cc::warn( " peer close event with code=" ) + cc::c( local_close_code ) +
                   cc::debug( ", reason=" ) + cc::info( reason ) );
    skutils::ws::peer::onClose( reason, local_close_code, local_close_code_as_str );
    uninstallAllWatches();
    // unddos
    unregister_ws_conn_for_origin();
}

void SkaleWsPeer::onFail() {
    SkaleServerOverride* pSO = pso();
    if ( pSO->opts_.isTraceCalls_ )
        clog( dev::VerbosityError, cc::fatal( getRelay().nfoGetSchemeUC() ) )
            << ( desc() + cc::error( " peer fail event" ) );
    skutils::ws::peer::onFail();
    uninstallAllWatches();
    // unddos
    unregister_ws_conn_for_origin();
}

void SkaleWsPeer::onLogMessage(
    skutils::ws::e_ws_log_message_type_t eWSLMT, const std::string& msg ) {
    SkaleServerOverride* pSO = pso();
    if ( pSO->opts_.isTraceCalls_ )
        clog( skale::server::helper::dv_from_ws_msg_type( eWSLMT ),
            cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                cc::num10( getRelay().serverIndex() ) )
            << ( desc() + cc::debug( " peer log: " ) + cc::info( msg ) );
    skutils::ws::peer::onLogMessage( eWSLMT, msg );
}

SkaleRelayWS& SkaleWsPeer::getRelay() {
    return static_cast< SkaleRelayWS& >( srv() );
}
SkaleServerOverride* SkaleWsPeer::pso() {
    SkaleServerOverride* pSO = getRelay().pso();
    return pSO;
}
dev::eth::Interface* SkaleWsPeer::ethereum() const {
    const SkaleServerOverride* pSO = pso();
    return pSO->ethereum();
}

void SkaleWsPeer::uninstallAllWatches() {
    set_watche_ids_t sw;
    //
    sw = setInstalledWatchesLogs_;
    setInstalledWatchesLogs_.clear();
    auto pEthereum = ethereum();
    for ( auto iw : sw ) {
        try {
            pEthereum->uninstallWatch( iw );
        } catch ( ... ) {
        }
    }
    //
    sw = setInstalledWatchesNewPendingTransactions_;
    setInstalledWatchesNewPendingTransactions_.clear();
    for ( auto iw : sw ) {
        try {
            pEthereum->uninstallNewPendingTransactionWatch( iw );
        } catch ( ... ) {
        }
    }
    //
    sw = setInstalledWatchesNewBlocks_;
    setInstalledWatchesNewBlocks_.clear();
    for ( auto iw : sw ) {
        try {
            pEthereum->uninstallNewBlockWatch( iw );
        } catch ( ... ) {
        }
    }
}

bool SkaleWsPeer::handleRequestWithBinaryAnswer(
    e_server_mode_t esm, const nlohmann::json& joRequest ) {
    SkaleServerOverride* pSO = pso();
    std::vector< uint8_t > buffer;
    if ( pSO->handleRequestWithBinaryAnswer( esm, joRequest, buffer ) ) {
        std::string strMethodName =
            skutils::tools::getFieldSafe< std::string >( joRequest, "method" );
        std::string s( buffer.begin(), buffer.end() );
        sendMessage( s, skutils::ws::opcv::binary );
        stats::register_stats_answer( "RPC", strMethodName, buffer.size() );
        return true;
    }
    return false;
}

bool SkaleWsPeer::handleWebSocketSpecificRequest(
    e_server_mode_t esm, const nlohmann::json& joRequest, std::string& strResponse ) {
    strResponse.clear();
    nlohmann::json joResponse = nlohmann::json::object();
    joResponse["jsonrpc"] = "2.0";
    if ( joRequest.count( "id" ) > 0 )
        joResponse["id"] = joRequest["id"];
    joResponse["result"] = nullptr;

    rapidjson::Document joRequestRapidjson;
    joRequestRapidjson.SetObject();
    std::string strRequest = joRequest.dump();
    joRequestRapidjson.Parse( strRequest.data() );

    rapidjson::Document joResponseRapidjson;
    joResponseRapidjson.SetObject();
    std::string strResponseCopy = joResponse.dump();
    joResponseRapidjson.Parse( strResponseCopy.data() );

    if ( handleWebSocketSpecificRequest( esm, joRequest, joResponse ) ) {
        strResponse = joResponse.dump();
        return true;
    }

    bool isSkipProtocolSpecfic = false;
    std::string strMethod = joRequest["method"].get< std::string >();

    if ( esm == e_server_mode_t::esm_informational && strMethod == "eth_getBalance" )
        isSkipProtocolSpecfic = true;

    if ( ( !isSkipProtocolSpecfic ) && pso()->handleProtocolSpecificRequest( getRemoteIp(),
                                           joRequestRapidjson, joResponseRapidjson ) ) {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer< rapidjson::StringBuffer > writer( buffer );
        joResponseRapidjson.Accept( writer );
        strResponse = buffer.GetString();

        return true;
    }

    return false;
}

bool SkaleWsPeer::handleWebSocketSpecificRequest(
    e_server_mode_t esm, const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( esm == e_server_mode_t::esm_informational &&
         pso()->handleInformationalRequest( joRequest, joResponse ) ) {
        return true;
    }
    std::string strMethod = joRequest["method"].get< std::string >();
    ws_rpc_map_t::const_iterator itFind = g_ws_rpc_map.find( strMethod );
    if ( itFind == g_ws_rpc_map.end() ) {
        return false;
    }
    ( ( *this ).*( itFind->second ) )( esm, joRequest, joResponse );
    return true;
}

const SkaleWsPeer::ws_rpc_map_t SkaleWsPeer::g_ws_rpc_map = {
    { "eth_subscribe", &SkaleWsPeer::eth_subscribe },
    { "eth_unsubscribe", &SkaleWsPeer::eth_unsubscribe },
};

void SkaleWsPeer::eth_subscribe(
    e_server_mode_t esm, const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( !skale::server::helper::checkParamsIsArray( "eth_subscribe", joRequest, joResponse ) )
        return;
    const nlohmann::json& jarrParams = joRequest["params"];
    std::string strSubscriptionType;
    size_t idxParam, cntParams = jarrParams.size();
    for ( idxParam = 0; idxParam < cntParams; ++idxParam ) {
        const nlohmann::json& joParamItem = jarrParams[idxParam];
        if ( !joParamItem.is_string() )
            continue;
        strSubscriptionType = skutils::tools::trim_copy( joParamItem.get< std::string >() );
        break;
    }
    if ( strSubscriptionType == "logs" ) {
        eth_subscribe_logs( esm, joRequest, joResponse );
        return;
    }
    if ( strSubscriptionType == "newPendingTransactions" ||
         strSubscriptionType == "pendingTransactions" ) {
        eth_subscribe_newPendingTransactions( esm, joRequest, joResponse );
        return;
    }
    if ( strSubscriptionType == "newHeads" || strSubscriptionType == "newBlockHeaders" ) {
        eth_subscribe_newHeads( esm, joRequest, joResponse, false );
        return;
    }
    if ( strSubscriptionType == "skaleStats" ) {
        eth_subscribe_skaleStats( esm, joRequest, joResponse );
        return;
    }
    if ( strSubscriptionType.empty() )
        strSubscriptionType = "<empty>";
    SkaleServerOverride* pSO = pso();
    if ( pSO->opts_.isTraceCalls_ )
        clog( dev::Verbosity::VerbosityError, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                  cc::debug( "/" ) +
                                                  cc::num10( getRelay().serverIndex() ) )
            << ( desc() + " " + cc::error( "error in " ) + cc::warn( "eth_subscribe" ) +
                   cc::error( " rpc method, missing valid subscription type in parameters, was "
                              "specifiedL " ) +
                   cc::warn( strSubscriptionType ) );
    nlohmann::json joError = nlohmann::json::object();
    joError["code"] = -32603;
    joError["message"] =
        "error in \"eth_subscribe\" rpc method, missing valid subscription type in parameters, was "
        "specified: " +
        strSubscriptionType;
    joResponse["error"] = joError;
}

void SkaleWsPeer::eth_subscribe_logs(
    e_server_mode_t /*esm*/, const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    SkaleServerOverride* pSO = pso();
    try {
        const nlohmann::json& jarrParams = joRequest["params"];
        dev::eth::LogFilter logFilter;
        bool bHaveLogFilter = false;
        size_t idxParam, cntParams = jarrParams.size();
        for ( idxParam = 0; idxParam < cntParams; ++idxParam ) {
            const nlohmann::json& joParamItem = jarrParams[idxParam];
            if ( joParamItem.is_string() )
                continue;
            if ( joParamItem.is_object() ) {
                if ( !bHaveLogFilter ) {
                    bHaveLogFilter = true;
                    logFilter = skale::server::helper::toLogFilter( joParamItem );
                }
            }
        }  // for ( idxParam = 0; idxParam < cntParams; ++idxParam )
        skutils::retain_release_ptr< SkaleWsPeer > pThis( this );
        dev::eth::fnClientWatchHandlerMulti_t fnOnSunscriptionEvent;
        fnOnSunscriptionEvent += [pThis]( unsigned iw ) -> void {
            skutils::dispatch::async( "logs-rethread", [=]() -> void {
                skutils::dispatch::async( pThis->m_strPeerQueueID, [pThis, iw]() -> void {
                    dev::eth::LocalisedLogEntries le = pThis->ethereum()->checkWatch( iw );
                    nlohmann::json joResult = skale::server::helper::toJsonByBlock( le );

                    if ( !joResult.is_array() )
                        throw std::runtime_error( "Log entries should be array" );
                    for ( const auto& joRW : joResult ) {
                        if ( joRW.count( "logs" ) > 0 && joRW.count( "blockHash" ) > 0 &&
                             joRW.count( "blockNumber" ) > 0 ) {
                            const std::string strBlockHash = joRW["blockHash"].get< std::string >();
                            std::string strBlockNumber = joRW["blockNumber"].get< std::string >();
                            const nlohmann::json& joResultLogs = joRW["logs"];
                            if ( !joResultLogs.is_array() )
                                throw std::runtime_error( "Result logs should be array" );
                            for ( const auto& joWalk : joResultLogs ) {
                                if ( !joWalk.is_object() )
                                    continue;
                                nlohmann::json joLog = joWalk;  // copy
                                joLog["blockHash"] = strBlockHash;
                                joLog["blockNumber"] = strBlockNumber;
                                nlohmann::json joParams = nlohmann::json::object();
                                joParams["subscription"] = dev::toJS( iw );
                                joParams["result"] = joLog;
                                nlohmann::json joNotification = nlohmann::json::object();
                                joNotification["jsonrpc"] = "2.0";
                                joNotification["method"] = "eth_subscription";
                                joNotification["params"] = joParams;
                                std::string strNotification = joNotification.dump();
                                const SkaleServerOverride* pSO = pThis->pso();
                                if ( pSO->opts_.isTraceCalls_ )
                                    clog( dev::VerbosityDebug,
                                        cc::info( pThis->getRelay().nfoGetSchemeUC() ) +
                                            cc::ws_tx_inv( " <<< " +
                                                           pThis->getRelay().nfoGetSchemeUC() +
                                                           "/TX <<< " ) )
                                        << ( pThis->desc() + cc::ws_tx( " <<< " ) +
                                               pThis->implPreformatTrafficJsonMessage(
                                                   strNotification, false ) );
                                bool bMessageSentOK = false;
                                try {
                                    bMessageSentOK = const_cast< SkaleWsPeer* >( pThis.get() )
                                                         ->sendMessage( skutils::tools::trim_copy(
                                                             strNotification ) );
                                    if ( !bMessageSentOK )
                                        throw std::runtime_error(
                                            "eth_subscription/logs failed to sent "
                                            "message" );
                                    stats::register_stats_answer(
                                        ( std::string( "RPC/" ) +
                                            pThis->getRelay().nfoGetSchemeUC() )
                                            .c_str(),
                                        "eth_subscription/logs", strNotification.size() );
                                    stats::register_stats_answer(
                                        "RPC", "eth_subscription/logs", strNotification.size() );
                                } catch ( std::exception& ex ) {
                                    clog( dev::Verbosity::VerbosityError,
                                        cc::info( pThis->getRelay().nfoGetSchemeUC() ) +
                                            cc::debug( "/" ) +
                                            cc::num10( pThis->getRelay().serverIndex() ) )
                                        << ( pThis->desc() + " " + cc::error( "error in " ) +
                                               cc::warn( "eth_subscription/logs" ) +
                                               cc::error( " will uninstall watcher callback "
                                                          "because of exception: " ) +
                                               cc::warn( ex.what() ) );
                                } catch ( ... ) {
                                    clog( dev::Verbosity::VerbosityError,
                                        cc::info( pThis->getRelay().nfoGetSchemeUC() ) +
                                            cc::debug( "/" ) +
                                            cc::num10( pThis->getRelay().serverIndex() ) )
                                        << ( pThis->desc() + " " + cc::error( "error in " ) +
                                               cc::warn( "eth_subscription/logs" ) +
                                               cc::error( " will uninstall watcher callback "
                                                          "because of unknown exception" ) );
                                }
                                if ( !bMessageSentOK ) {
                                    stats::register_stats_error(
                                        ( std::string( "RPC/" ) +
                                            pThis->getRelay().nfoGetSchemeUC() )
                                            .c_str(),
                                        "eth_subscription/logs" );
                                    stats::register_stats_error( "RPC", "eth_subscription/logs" );
                                    pThis->ethereum()->uninstallWatch( iw );
                                }
                            }
                        }
                    }
                } );
            } );
        };
        unsigned iw = ethereum()->installWatch(
            logFilter, dev::eth::Reaping::Automatic, fnOnSunscriptionEvent, true );  // isWS = true
        setInstalledWatchesLogs_.insert( iw );
        std::string strIW = dev::toJS( iw );
        if ( pSO->opts_.isTraceCalls_ )
            clog( dev::Verbosity::VerbosityTrace, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << ( desc() + " " + cc::info( "eth_subscribe/logs" ) +
                       cc::debug( " rpc method did installed watch " ) + cc::info( strIW ) );
        joResponse["result"] = strIW;
    } catch ( const std::exception& ex ) {
        if ( pSO->opts_.isTraceCalls_ )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << ( desc() + " " + cc::error( "error in " ) + cc::warn( "eth_subscribe/logs" ) +
                       cc::error( " rpc method, exception " ) + cc::warn( ex.what() ) );
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] =
            std::string( "error in \"eth_subscribe/logs\" rpc method, exception: " ) + ex.what();
        joResponse["error"] = joError;
        return;
    } catch ( ... ) {
        if ( pSO->opts_.isTraceCalls_ )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << ( desc() + " " + cc::error( "error in " ) + cc::warn( "eth_subscribe/logs" ) +
                       cc::error( " rpc method, unknown exception " ) );
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] = "error in \"eth_subscribe/logs\" rpc method, unknown exception";
        joResponse["error"] = joError;
        return;
    }
}

void SkaleWsPeer::eth_subscribe_newPendingTransactions(
    e_server_mode_t /*esm*/, const nlohmann::json& /*joRequest*/, nlohmann::json& joResponse ) {
    SkaleServerOverride* pSO = pso();
    try {
        skutils::retain_release_ptr< SkaleWsPeer > pThis( this );
        std::function< void( const unsigned& iw, const dev::eth::Transaction& t ) >
            fnOnSunscriptionEvent =
                [pThis]( const unsigned& iw, const dev::eth::Transaction& t ) -> void {
            skutils::dispatch::async( pThis->m_strPeerQueueID, [pThis, iw, t]() -> void {
                const SkaleServerOverride* pSO = pThis->pso();
                dev::h256 h = t.sha3();
                //
                nlohmann::json joParams = nlohmann::json::object();
                joParams["subscription"] =
                    dev::toJS( iw | SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION );
                joParams["result"] = dev::toJS( h );  // h.hex()
                nlohmann::json joNotification = nlohmann::json::object();
                joNotification["jsonrpc"] = "2.0";
                joNotification["method"] = "eth_subscription";
                joNotification["params"] = joParams;
                std::string strNotification = joNotification.dump();
                if ( pSO->opts_.isTraceCalls_ )
                    clog( dev::VerbosityDebug, cc::info( pThis->getRelay().nfoGetSchemeUC() ) )
                        << ( cc::ws_tx_inv(
                                 " <<< " + pThis->getRelay().nfoGetSchemeUC() + "/TX <<< " ) +
                               pThis->desc() + cc::ws_tx( " <<< " ) +
                               pThis->implPreformatTrafficJsonMessage( strNotification, false ) );
                bool bMessageSentOK = false;
                try {
                    bMessageSentOK =
                        const_cast< SkaleWsPeer* >( pThis.get() )
                            ->sendMessage( skutils::tools::trim_copy( strNotification ) );
                    if ( !bMessageSentOK )
                        throw std::runtime_error(
                            "eth_subscription/newPendingTransactions failed to sent message" );
                    stats::register_stats_answer(
                        ( std::string( "RPC/" ) + pThis->getRelay().nfoGetSchemeUC() ).c_str(),
                        "eth_subscription/newPendingTransactions", strNotification.size() );
                    stats::register_stats_answer(
                        "RPC", "eth_subscription/newPendingTransactions", strNotification.size() );
                } catch ( std::exception& ex ) {
                    clog( dev::Verbosity::VerbosityError,
                        cc::info( pThis->getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                            cc::num10( pThis->getRelay().serverIndex() ) )
                        << ( pThis->desc() + " " + cc::error( "error in " ) +
                               cc::warn( "eth_subscription/newPendingTransactions" ) +
                               cc::error(
                                   " will uninstall watcher callback because of exception: " ) +
                               cc::warn( ex.what() ) );
                } catch ( ... ) {
                    clog( dev::Verbosity::VerbosityError,
                        cc::info( pThis->getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                            cc::num10( pThis->getRelay().serverIndex() ) )
                        << ( pThis->desc() + " " + cc::error( "error in " ) +
                               cc::warn( "eth_subscription/newPendingTransactions" ) +
                               cc::error( " will uninstall watcher callback because of unknown "
                                          "exception" ) );
                }
                if ( !bMessageSentOK ) {
                    stats::register_stats_error(
                        ( std::string( "RPC/" ) + pThis->getRelay().nfoGetSchemeUC() ).c_str(),
                        "eth_subscription/newPendingTransactions" );
                    stats::register_stats_error( "RPC", "eth_subscription/newPendingTransactions" );
                    pThis->ethereum()->uninstallNewPendingTransactionWatch( iw );
                }
                //} );
            } );
        };
        unsigned iw = ethereum()->installNewPendingTransactionWatch( fnOnSunscriptionEvent );
        setInstalledWatchesNewPendingTransactions_.insert( iw );
        iw |= SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION;
        std::string strIW = dev::toJS( iw );
        if ( pSO->opts_.isTraceCalls_ )
            clog( dev::Verbosity::VerbosityTrace, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << ( desc() + " " + cc::info( "eth_subscribe/newPendingTransactions" ) +
                       cc::debug( " rpc method did installed watch " ) + cc::info( strIW ) );
        joResponse["result"] = strIW;
    } catch ( const std::exception& ex ) {
        if ( pSO->opts_.isTraceCalls_ )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << ( desc() + " " + cc::error( "error in " ) +
                       cc::warn( "eth_subscribe/newPendingTransactions" ) +
                       cc::error( " rpc method, exception " ) + cc::warn( ex.what() ) );
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] =
            std::string(
                "error in \"eth_subscribe/newPendingTransactions\" rpc method, exception: " ) +
            ex.what();
        joResponse["error"] = joError;
        return;
    } catch ( ... ) {
        if ( pSO->opts_.isTraceCalls_ )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << ( desc() + " " + cc::error( "error in " ) +
                       cc::warn( "eth_subscribe/newPendingTransactions" ) +
                       cc::error( " rpc method, unknown exception " ) );
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] =
            "error in \"eth_subscribe/newPendingTransactions\" rpc method, unknown exception";
        joResponse["error"] = joError;
        return;
    }
}

void SkaleWsPeer::eth_subscribe_newHeads( e_server_mode_t /*esm*/,
    const nlohmann::json& /*joRequest*/, nlohmann::json& joResponse, bool bIncludeTransactions ) {
    SkaleServerOverride* pSO = pso();
    try {
        skutils::retain_release_ptr< SkaleWsPeer > pThis( this );
        std::function< void( const unsigned& iw, const dev::eth::Block& block ) >
            fnOnSunscriptionEvent = [pThis, bIncludeTransactions](
                                        const unsigned& iw, const dev::eth::Block& block ) -> void {
            skutils::dispatch::async( [pThis, iw, block, bIncludeTransactions]() -> void {
                const SkaleServerOverride* pSO = pThis->pso();
                dev::h256 h = block.info().hash();
                Json::Value jv;
                if ( bIncludeTransactions )
                    jv = dev::eth::toJson( pThis->ethereum()->blockInfo( h ),
                        pThis->ethereum()->blockDetails( h ), pThis->ethereum()->uncleHashes( h ),
                        pThis->ethereum()->transactions( h ), pThis->ethereum()->sealEngine() );
                else
                    jv = dev::eth::toJson( pThis->ethereum()->blockInfo( h ),
                        pThis->ethereum()->blockDetails( h ), pThis->ethereum()->uncleHashes( h ),
                        pThis->ethereum()->transactionHashes( h ),
                        pThis->ethereum()->sealEngine() );
                Json::FastWriter fastWriter;
                std::string s = fastWriter.write( jv );
                nlohmann::json joBlockDescription = nlohmann::json::parse( s );
                //
                nlohmann::json joParams = nlohmann::json::object();
                joParams["subscription"] = dev::toJS( iw | SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK );
                joParams["result"] = joBlockDescription;
                nlohmann::json joNotification = nlohmann::json::object();
                joNotification["jsonrpc"] = "2.0";
                joNotification["method"] = "eth_subscription";
                joNotification["params"] = joParams;
                std::string strNotification = joNotification.dump();
                if ( pSO->opts_.isTraceCalls_ )
                    clog( dev::VerbosityDebug, cc::info( pThis->getRelay().nfoGetSchemeUC() ) )
                        << ( cc::ws_tx_inv(
                                 " <<< " + pThis->getRelay().nfoGetSchemeUC() + "/TX <<< " ) +
                               pThis->desc() + cc::ws_tx( " <<< " ) +
                               pThis->implPreformatTrafficJsonMessage( strNotification, false ) );
                // skutils::dispatch::async( pThis->m_strPeerQueueID, [pThis, strNotification]() ->
                // void {
                bool bMessageSentOK = false;
                try {
                    bMessageSentOK =
                        const_cast< SkaleWsPeer* >( pThis.get() )
                            ->sendMessage( skutils::tools::trim_copy( strNotification ) );
                    if ( !bMessageSentOK )
                        throw std::runtime_error(
                            "eth_subscription/newHeads failed to sent message" );
                    stats::register_stats_answer(
                        ( std::string( "RPC/" ) + pThis->getRelay().nfoGetSchemeUC() ).c_str(),
                        "eth_subscription/newHeads", strNotification.size() );
                    stats::register_stats_answer(
                        "RPC", "eth_subscription/newHeads", strNotification.size() );
                } catch ( std::exception& ex ) {
                    clog( dev::Verbosity::VerbosityError,
                        cc::info( pThis->getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                            cc::num10( pThis->getRelay().serverIndex() ) )
                        << ( pThis->desc() + " " + cc::error( "error in " ) +
                               cc::warn( "eth_subscription/newHeads" ) +
                               cc::error(
                                   " will uninstall watcher callback because of exception: " ) +
                               cc::warn( ex.what() ) );
                } catch ( ... ) {
                    clog( dev::Verbosity::VerbosityError,
                        cc::info( pThis->getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                            cc::num10( pThis->getRelay().serverIndex() ) )
                        << ( pThis->desc() + " " + cc::error( "error in " ) +
                               cc::warn( "eth_subscription/newHeads" ) +
                               cc::error( " will uninstall watcher callback because of unknown "
                                          "exception" ) );
                }
                if ( !bMessageSentOK ) {
                    stats::register_stats_error(
                        ( std::string( "RPC/" ) + pThis->getRelay().nfoGetSchemeUC() ).c_str(),
                        "eth_subscription/newHeads" );
                    stats::register_stats_error( "RPC", "eth_subscription/newHeads" );
                    pThis->ethereum()->uninstallNewBlockWatch( iw );
                }
                //} );
            } );
        };
        unsigned iw = ethereum()->installNewBlockWatch( fnOnSunscriptionEvent );
        setInstalledWatchesNewBlocks_.insert( iw );
        iw |= SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK;
        std::string strIW = dev::toJS( iw );
        if ( pSO->opts_.isTraceCalls_ )
            clog( dev::Verbosity::VerbosityTrace, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << ( desc() + " " + cc::info( "eth_subscribe/newHeads" ) +
                       cc::debug( " rpc method did installed watch " ) + cc::info( strIW ) );
        joResponse["result"] = strIW;
    } catch ( const std::exception& ex ) {
        if ( pSO->opts_.isTraceCalls_ )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << ( desc() + " " + cc::error( "error in " ) +
                       cc::warn( "eth_subscribe/newHeads(" ) +
                       cc::error( " rpc method, exception " ) + cc::warn( ex.what() ) );
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] =
            std::string( "error in \"eth_subscribe/newHeads(\" rpc method, exception: " ) +
            ex.what();
        joResponse["error"] = joError;
        return;
    } catch ( ... ) {
        if ( pSO->opts_.isTraceCalls_ )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << ( desc() + " " + cc::error( "error in " ) +
                       cc::warn( "eth_subscribe/newHeads(" ) +
                       cc::error( " rpc method, unknown exception " ) );
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] = "error in \"eth_subscribe/newHeads(\" rpc method, unknown exception";
        joResponse["error"] = joError;
        return;
    }
}

void SkaleWsPeer::eth_subscribe_skaleStats(
    e_server_mode_t /*esm*/, const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    SkaleServerOverride* pSO = pso();
    try {
        // skutils::retain_release_ptr< SkaleWsPeer > pThis( this );
        SkaleStatsSubscriptionManager::subscription_id_t idSubscription = 0;
        size_t nIntervalMilliseconds = 1000;
        if ( joRequest.count( "intervalMilliseconds" ) )
            nIntervalMilliseconds =
                skutils::tools::getFieldSafe< size_t >( joRequest, "intervalMilliseconds", 1000 );
        bool bWasSubscribed = pSO->subscribe( idSubscription, this, nIntervalMilliseconds );
        if ( !bWasSubscribed )
            throw std::runtime_error( "internal subscription error" );
        std::string strIW = dev::toJS( idSubscription | SKALED_WS_SUBSCRIPTION_TYPE_SKALE_STATS );
        if ( pSO->opts_.isTraceCalls_ )
            clog( dev::Verbosity::VerbosityTrace, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << ( desc() + " " + cc::info( "eth_subscribe/skaleStats" ) +
                       cc::debug( " rpc method did installed watch " ) + cc::info( strIW ) );
        joResponse["result"] = strIW;
    } catch ( const std::exception& ex ) {
        if ( pSO->opts_.isTraceCalls_ )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << ( desc() + " " + cc::error( "error in " ) +
                       cc::warn( "eth_subscribe/newHeads(" ) +
                       cc::error( " rpc method, exception " ) + cc::warn( ex.what() ) );
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] =
            std::string( "error in \"eth_subscribe/SkaleStats(\" rpc method, exception: " ) +
            ex.what();
        joResponse["error"] = joError;
        return;
    } catch ( ... ) {
        if ( pSO->opts_.isTraceCalls_ )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << ( desc() + " " + cc::error( "error in " ) +
                       cc::warn( "eth_subscribe/newHeads(" ) +
                       cc::error( " rpc method, unknown exception " ) );
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] = "error in \"eth_subscribe/SkaleStats(\" rpc method, unknown exception";
        joResponse["error"] = joError;
        return;
    }
}

void SkaleWsPeer::eth_unsubscribe(
    e_server_mode_t /*esm*/, const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( !skale::server::helper::checkParamsIsArray( "eth_unsubscribe", joRequest, joResponse ) )
        return;
    SkaleServerOverride* pSO = pso();
    const nlohmann::json& jarrParams = joRequest["params"];
    size_t idxParam, cntParams = jarrParams.size();
    for ( idxParam = 0; idxParam < cntParams; ++idxParam ) {
        const nlohmann::json& joParamItem = jarrParams[idxParam];
        unsigned iw = unsigned( -1 );
        if ( joParamItem.is_string() ) {
            std::string strIW = skutils::tools::trim_copy( joParamItem.get< std::string >() );
            if ( !strIW.empty() )
                iw = unsigned( std::stoul( strIW.c_str(), nullptr, 0 ) );
        } else if ( joParamItem.is_number_integer() ) {
            iw = joParamItem.get< unsigned >();
        }
        if ( iw == unsigned( -1 ) ) {
            if ( pSO->opts_.isTraceCalls_ )
                clog( dev::Verbosity::VerbosityError, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                          cc::debug( "/" ) +
                                                          cc::num10( getRelay().serverIndex() ) )
                    << ( desc() + " " + cc::error( "error in " ) + cc::warn( "eth_unsubscribe" ) +
                           cc::error( " rpc method, bad subscription ID " ) +
                           cc::j( joParamItem ) );
            nlohmann::json joError = nlohmann::json::object();
            joError["code"] = -32602;
            joError["message"] =
                "error in \"eth_unsubscribe\" rpc method, ad subscription ID " + joParamItem.dump();
            joResponse["error"] = joError;
            return;
        }
        unsigned x = ( iw & SKALED_WS_SUBSCRIPTION_TYPE_MASK );
        if ( x == SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION ) {
            if ( setInstalledWatchesNewPendingTransactions_.find(
                     iw & ( ~( SKALED_WS_SUBSCRIPTION_TYPE_MASK ) ) ) ==
                 setInstalledWatchesNewPendingTransactions_.end() ) {
                std::string strIW = dev::toJS( iw );
                if ( pSO->opts_.isTraceCalls_ )
                    clog( dev::Verbosity::VerbosityError,
                        cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                            cc::num10( getRelay().serverIndex() ) )
                        << ( desc() + " " + cc::error( "error in " ) +
                               cc::warn( "eth_unsubscribe/newPendingTransactionWatch" ) +
                               cc::error( " rpc method, bad subscription ID " ) +
                               cc::warn( strIW ) );
                nlohmann::json joError = nlohmann::json::object();
                joError["code"] = -32602;
                joError["message"] =
                    "error in \"eth_unsubscribe/newPendingTransactionWatch\" rpc method, ad "
                    "subscription ID " +
                    strIW;
                joResponse["error"] = joError;
                return;
            }
            ethereum()->uninstallNewPendingTransactionWatch( iw );
            setInstalledWatchesNewPendingTransactions_.erase(
                iw & ( ~( SKALED_WS_SUBSCRIPTION_TYPE_MASK ) ) );
        } else if ( x == SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK ) {
            if ( setInstalledWatchesNewBlocks_.find(
                     iw & ( ~( SKALED_WS_SUBSCRIPTION_TYPE_MASK ) ) ) ==
                 setInstalledWatchesNewBlocks_.end() ) {
                std::string strIW = dev::toJS( iw );
                if ( pSO->opts_.isTraceCalls_ )
                    clog( dev::Verbosity::VerbosityError,
                        cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                            cc::num10( getRelay().serverIndex() ) )
                        << ( desc() + " " + cc::error( "error in " ) +
                               cc::warn( "eth_unsubscribe/newHeads" ) +
                               cc::error( " rpc method, bad subscription ID " ) +
                               cc::warn( strIW ) );
                nlohmann::json joError = nlohmann::json::object();
                joError["code"] = -32602;
                joError["message"] =
                    "error in \"eth_unsubscribe/newHeads\" rpc method, ad subscription ID " + strIW;
                joResponse["error"] = joError;
                return;
            }
            ethereum()->uninstallNewBlockWatch( iw );
            setInstalledWatchesNewBlocks_.erase( iw & ( ~( SKALED_WS_SUBSCRIPTION_TYPE_MASK ) ) );
        } else if ( x == SKALED_WS_SUBSCRIPTION_TYPE_SKALE_STATS ) {
            SkaleStatsSubscriptionManager::subscription_id_t idSubscription =
                SkaleStatsSubscriptionManager::subscription_id_t(
                    iw & ( ~( SKALED_WS_SUBSCRIPTION_TYPE_SKALE_STATS ) ) );
            bool bWasUnsubscribed = pSO->unsubscribe( idSubscription );
            if ( !bWasUnsubscribed ) {
                std::string strIW = dev::toJS( iw );
                if ( pSO->opts_.isTraceCalls_ )
                    clog( dev::Verbosity::VerbosityError,
                        cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                            cc::num10( getRelay().serverIndex() ) )
                        << ( desc() + " " + cc::error( "error in " ) +
                               cc::warn( "eth_unsubscribe/newHeads" ) +
                               cc::error( " rpc method, bad subscription ID " ) +
                               cc::warn( strIW ) );
                nlohmann::json joError = nlohmann::json::object();
                joError["code"] = -32602;
                joError["message"] =
                    "error in \"eth_unsubscribe/skaleStats\" rpc method, ad subscription ID " +
                    strIW;
                joResponse["error"] = joError;
                return;
            }  // if ( !bWasUnsubscribed )
        } else {
            if ( setInstalledWatchesLogs_.find( iw ) == setInstalledWatchesLogs_.end() ) {
                std::string strIW = dev::toJS( iw );
                if ( pSO->opts_.isTraceCalls_ )
                    clog( dev::Verbosity::VerbosityError,
                        cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                            cc::num10( getRelay().serverIndex() ) )
                        << ( desc() + " " + cc::error( "error in " ) +
                               cc::warn( "eth_unsubscribe/logs" ) +
                               cc::error( " rpc method, bad subscription ID " ) +
                               cc::warn( strIW ) );
                nlohmann::json joError = nlohmann::json::object();
                joError["code"] = -32602;
                joError["message"] =
                    "error in \"eth_unsubscribe/logs\" rpc method, ad subscription ID " + strIW;
                joResponse["error"] = joError;
                return;
            }
            ethereum()->uninstallWatch( iw );
            setInstalledWatchesLogs_.erase( iw );
        }
    }  // for ( idxParam = 0; idxParam < cntParams; ++idxParam )
}

std::string SkaleWsPeer::implPreformatTrafficJsonMessage(
    const std::string& strJSON, bool isRequest ) const {
    try {
        nlohmann::json jo = nlohmann::json::parse( strJSON );
        return implPreformatTrafficJsonMessage( jo, isRequest );
    } catch ( ... ) {
    }
    return cc::error( isRequest ? "bad JSON request" : "bad JSON response" ) + " " +
           cc::warn( strJSON );
}

std::string SkaleWsPeer::implPreformatTrafficJsonMessage(
    const nlohmann::json& jo, bool isRequest ) const {
    const SkaleServerOverride* pSO = pso();
    if ( pSO )
        return pSO->implPreformatTrafficJsonMessage( jo, isRequest );
    nlohmann::json jo2 = jo;
    SkaleServerOverride::stat_transformJsonForLogOutput( jo2, isRequest,
        SkaleServerOverride::g_nMaxStringValueLengthForJsonLogs,
        SkaleServerOverride::g_nMaxStringValueLengthForTransactionParams );
    return cc::j( jo2 );
}


SkaleRelayWS::SkaleRelayWS( int ipVer, const char* strBindAddr,
    const char* strScheme,  // "ws" or "wss"
    int nPort, e_server_mode_t esm, int nServerIndex, skutils::ws::basic_network_settings* pBNS )
    : skutils::ws::server( pBNS ),
      SkaleServerHelper( nServerIndex ),
      ipVer_( ipVer ),
      strBindAddr_( strBindAddr ),
      m_strScheme_( skutils::tools::to_lower( strScheme ) ),
      m_strSchemeUC( skutils::tools::to_upper( strScheme ) ),
      m_nPort( nPort ),
      esm_( esm ) {
    //
    if ( !strBindAddr_.empty() ) {
        if ( ipVer_ == 6 ) {
            if ( strBindAddr_ == "*" || strBindAddr_ == "::" || strBindAddr_ == "0:0:0:0:0:0:0:0" )
                strBindAddr_.clear();
        } else if ( ipVer_ == 4 ) {
            if ( strBindAddr_ == "*" || strBindAddr_ == "0.0.0.0" )
                strBindAddr_.clear();
        } else
            strBindAddr_.clear();
    }
    if ( !strBindAddr_.empty() ) {
        std::list< std::pair< std::string, std::string > > listIfaceInfos =
            skutils::network::get_machine_ip_addresses(
                ( ipVer_ == 6 ) ? false : true, ( ipVer_ == 6 ) ? true : false );
        strInterfaceName_ =
            skutils::network::find_iface_or_ip_address( listIfaceInfos, strBindAddr_.c_str() )
                .first;  // first-interface name, second-address
    }
    //
    onPeerInstantiate_ = [&]( skutils::ws::server& srv,
                             skutils::ws::hdl_t hdl ) -> skutils::ws::peer_ptr_t {
        SkaleWsPeer* pSkalePeer = nullptr;
        SkaleServerOverride* pSO = pso();
        if ( pSO->opts_.isTraceCalls_ )
            clog( dev::VerbosityTrace, cc::info( m_strSchemeUC ) )
                << ( cc::notice( "Will instantiate new peer" ) );
        if ( pSO->isShutdownMode() ) {
            clog( dev::VerbosityWarning,
                cc::info( m_strSchemeUC ) + cc::debug( "/" ) + cc::num10( serverIndex() ) )
                << ( cc::ws_rx_inv( " >>> " + m_strSchemeUC + "/" +
                                    std::to_string( serverIndex() ) + "/RX >>> " ) +
                       cc::warn( "Skipping connection accept while in shutdown mode" ) );
            return pSkalePeer;
        }
        pSkalePeer = new SkaleWsPeer( srv, hdl );
        pSkalePeer->m_pSSCTH = std::make_unique< SkaleServerConnectionsTrackHelper >( *pSO );
        if ( pSO->is_connection_limit_overflow() ) {
            delete pSkalePeer;
            pSkalePeer = nullptr;  // WS will just close connection after we return nullptr here
            pSO->on_connection_overflow_peer_closed(
                ipVer, m_strSchemeUC.c_str(), serverIndex(), nPort, esm_ );
        }
        return pSkalePeer;
    };
    onPeerRegister_ = [&]( skutils::ws::peer_ptr_t& pPeer ) -> void {
        SkaleWsPeer* pSkalePeer = dynamic_cast< SkaleWsPeer* >( pPeer );
        if ( pSkalePeer == nullptr ) {
            pPeer->close( "server too busy" );
            return;
        }
        lock_type lock( mtxAllPeers() );
        m_mapAllPeers[pSkalePeer->m_strPeerQueueID] = pSkalePeer;
    };
    onPeerUnregister_ = [&]( skutils::ws::peer_ptr_t& pPeer ) -> void {
        SkaleWsPeer* pSkalePeer = dynamic_cast< SkaleWsPeer* >( pPeer );
        if ( pSkalePeer == nullptr )
            return;
        lock_type lock( mtxAllPeers() );
        m_mapAllPeers.erase( pSkalePeer->m_strPeerQueueID );
    };
}

SkaleRelayWS::~SkaleRelayWS() {
    stop();
}

void SkaleRelayWS::run( skutils::ws::fn_continue_status_flag_t fnContinueStatusFlag ) {
    m_isInLoop = true;
    try {
        if ( service_mode_supported() )
            service( fnContinueStatusFlag );
        else {
            while ( true ) {
                poll( fnContinueStatusFlag );
                if ( fnContinueStatusFlag ) {
                    if ( !fnContinueStatusFlag() )
                        break;
                }
            }
        }
    } catch ( ... ) {
    }
    m_isInLoop = false;
}

void SkaleRelayWS::waitWhileInLoop() {
    while ( isInLoop() )
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
}

bool SkaleRelayWS::start( SkaleServerOverride* pSO ) {
    SkaleRelayWS* pThis = this;
    stop();
    m_pSO = pSO;
    server_disable_ipv6_ = ( ipVer_ == 6 ) ? false : true;
    clog( dev::VerbosityDebug, cc::info( m_strSchemeUC ) )
        << ( cc::notice( "Will start server on port " ) + cc::c( m_nPort ) );
    if ( !open( m_strScheme_, m_nPort,
             ( !strInterfaceName_.empty() ) ? strInterfaceName_.c_str() : nullptr ) ) {
        clog( dev::VerbosityError, cc::fatal( m_strSchemeUC + " ERROR:" ) )
            << ( cc::error( "Failed to start server on port " ) + cc::c( m_nPort ) );
        return false;
    }
    std::thread( [pThis]() {
        pThis->m_isRunning = true;
        skutils::multithreading::threadNameAppender tn( "/" + pThis->m_strSchemeUC + "-listener" );
        try {
            pThis->run( [pThis]() -> bool {
                if ( !pThis->m_isRunning )
                    return false;
                const SkaleServerOverride* pSO = pThis->pso();
                if ( pSO->isShutdownMode() )
                    return false;
                return true;
            } );
        } catch ( ... ) {
        }
        // pThis->m_isRunning = false;
    } ).detach();
    clog( dev::VerbosityDebug, cc::info( m_strSchemeUC ) )
        << ( cc::success( "OK, server started on port " ) + cc::c( m_nPort ) );
    return true;
}
void SkaleRelayWS::stop() {
    if ( !isRunning() )
        return;
    clog( dev::VerbosityDebug, cc::info( m_strSchemeUC ) )
        << ( cc::notice( "Will stop on port " ) + cc::c( m_nPort ) + cc::notice( "..." ) );
    m_isRunning = false;
    waitWhileInLoop();
    close();
    clog( dev::VerbosityInfo, cc::info( m_strSchemeUC ) )
        << ( cc::success( "OK, server stopped on port " ) + cc::c( m_nPort ) );
}

dev::eth::Interface* SkaleRelayWS::ethereum() const {
    const SkaleServerOverride* pSO = pso();
    return pSO->ethereum();
}


SkaleRelayProxygenHTTP::SkaleRelayProxygenHTTP( SkaleServerOverride* pSO, int ipVer,
    const char* strBindAddr, int nPort, const char* cert_path, const char* private_key_path,
    const char* ca_path, int nServerIndex, e_server_mode_t esm, int32_t threads,
    int32_t threads_limit )
    : SkaleServerHelper( nServerIndex ),
      m_pSO( pSO ),
      ipVer_( ipVer ),
      strBindAddr_( strBindAddr ),
      nPort_( nPort ),
      m_bHelperIsSSL(
          ( cert_path && cert_path[0] && private_key_path && private_key_path[0] ) ? true : false ),
      esm_( esm ),
      cert_path_( cert_path ? cert_path : "" ),
      private_key_path_( private_key_path ? private_key_path : "" ),
      ca_path_( ca_path ? ca_path : "" ),
      threads_( threads ),
      threads_limit_( threads_limit ) {
    skutils::http_pg::pg_accumulate_entry pge = { ipVer_, strBindAddr_, nPort_,
        m_bHelperIsSSL ? cert_path_.c_str() : "", m_bHelperIsSSL ? private_key_path_.c_str() : "",
        m_bHelperIsSSL ? ca_path_.c_str() : "" };
    skutils::http_pg::pg_accumulate_add( pge );
}

SkaleRelayProxygenHTTP::~SkaleRelayProxygenHTTP() {
    stop();
}

bool SkaleRelayProxygenHTTP::is_running() const {
    return true;
}

void SkaleRelayProxygenHTTP::stop() {}


const double SkaleServerOverride::g_lfDefaultExecutionDurationMaxForPerformanceWarning =
    1.0;  // in seconds, default 1 second

SkaleServerOverride::SkaleServerOverride(
    dev::eth::ChainParams& chainParams, dev::eth::Interface* pEth, const opts_t& opts )
    : AbstractServerConnector(), chainParams_( chainParams ), pEth_( pEth ), opts_( opts ) {
    //
    //
    // proxygen-related init
    skutils::http_pg::init_logging( "skaled" );
    skutils::http_pg::install_logging_fail_func( []() -> void {
        clog( dev::VerbosityError, "generic" ) << ( cc::fatal( "CRITICAL ERROR:" ) + " " +
                                                    cc::error( "Proxygen abort handler called." ) );
    } );
    //
    //

    {  // block
        std::function< void( const unsigned& iw, const dev::eth::Block& block ) >
            fnOnSunscriptionEvent =
                [this]( const unsigned& /*iw*/, const dev::eth::Block& block ) -> void {
            dev::h256 h = block.info().hash();
            dev::eth::TransactionHashes arrTxHashes = ethereum()->transactionHashes( h );
            size_t cntTXs = arrTxHashes.size();
            lock_type lock( mtxStats_ );
            statsBlocks_.event_add( "blocks", 1 );
            statsTransactions_.event_add( "transactions", cntTXs );
        };
        statsBlocks_.event_queue_add( "blocks",
            0  // stats::g_nSizeDefaultOnQueueAdd
        );
        statsTransactions_.event_queue_add( "transactions",
            0  // stats::g_nSizeDefaultOnQueueAdd
        );
        iwBlockStats_ = ethereum()->installNewBlockWatch( fnOnSunscriptionEvent );
    }  // block
    {  // block
        std::function< void( const unsigned& iw, const dev::eth::Transaction& tx ) >
            fnOnSunscriptionEvent =
                [this]( const unsigned& /*iw*/, const dev::eth::Transaction& /*tx*/ ) -> void {
            lock_type lock( mtxStats_ );
            statsPendingTx_.event_add( "transactionsPending", 1 );
        };
        statsPendingTx_.event_queue_add( "transactionsPending",
            0  // stats::g_nSizeDefaultOnQueueAdd
        );
        iwPendingTransactionStats_ =
            ethereum()->installNewPendingTransactionWatch( fnOnSunscriptionEvent );
    }  // block
}

SkaleServerOverride::~SkaleServerOverride() {
    if ( iwBlockStats_ != unsigned( -1 ) ) {
        ethereum()->uninstallNewBlockWatch( iwBlockStats_ );
        iwBlockStats_ = unsigned( -1 );
    }
    if ( iwPendingTransactionStats_ != unsigned( -1 ) ) {
        ethereum()->uninstallNewPendingTransactionWatch( iwPendingTransactionStats_ );
        iwPendingTransactionStats_ = unsigned( -1 );
    }
    StopListening();
}

nlohmann::json SkaleServerOverride::generateBlocksStats() {
    lock_type lock( mtxStats_ );
    nlohmann::json joStats = nlohmann::json::object();
    skutils::stats::time_point tpNow = skutils::stats::clock::now();
    const double lfBlocksPerSecond = statsBlocks_.compute_eps_smooth( "blocks", tpNow );
    double lfTransactionsPerSecond = statsTransactions_.compute_eps_smooth( "transactions", tpNow );
    if ( lfTransactionsPerSecond >= 1.0 )
        lfTransactionsPerSecond -= 1.0;  // workaround for UnitsPerSecond in skutils::stats
    if ( lfTransactionsPerSecond <= 1.0 )
        lfTransactionsPerSecond = 0.0;
    double lfTransactionsPerBlock =
        ( lfBlocksPerSecond > 0.0 ) ? ( lfTransactionsPerSecond / lfBlocksPerSecond ) : 0.0;
    if ( lfTransactionsPerBlock >= 1.0 )
        lfTransactionsPerBlock -= 1.0;  // workaround for UnitsPerSecond in skutils::stats
    if ( lfTransactionsPerBlock <= 1.0 )
        lfTransactionsPerBlock = 0.0;
    if ( lfTransactionsPerBlock == 0.0 )
        lfTransactionsPerSecond = 0.0;
    if ( lfTransactionsPerSecond == 0.0 )
        lfTransactionsPerBlock = 0.0;
    double lfPendingTxPerSecond =
        statsPendingTx_.compute_eps_smooth( "transactionsPending", tpNow );
    if ( lfPendingTxPerSecond >= 1.0 )
        lfPendingTxPerSecond -= 1.0;  // workaround for UnitsPerSecond in skutils::stats
    if ( lfPendingTxPerSecond <= 1.0 )
        lfPendingTxPerSecond = 0.0;
    joStats["blocksPerSecond"] = lfBlocksPerSecond;
    joStats["transactionsPerSecond"] = lfTransactionsPerSecond;
    joStats["transactionsPerBlock"] = lfTransactionsPerBlock;
    joStats["pendingTxPerSecond"] = lfPendingTxPerSecond;
    return joStats;
}

dev::eth::Interface* SkaleServerOverride::ethereum() const {
    if ( !pEth_ ) {
        cerror << "SKALE server fatal error: no eth interface\n";
        cerror << DETAILED_ERROR;
        std::cerr.flush();
        std::terminate();
    }
    return pEth_;
}

dev::eth::ChainParams& SkaleServerOverride::chainParams() {
    return chainParams_;
}
const dev::eth::ChainParams& SkaleServerOverride::chainParams() const {
    return chainParams_;
}

dev::Verbosity SkaleServerOverride::methodTraceVerbosity( const std::string& strMethod ) const {
    // skip if disabled completely
    if ( !this->opts_.isTraceCalls_ && !this->opts_.isTraceSpecialCalls_ )
        return dev::VerbositySilent;

    // skip these in any case
    if ( strMethod == "skale_stats" || strMethod == "skale_performanceTrackingStatus" ||
         strMethod == "skale_performanceTrackingStart" ||
         strMethod == "skale_performanceTrackingStop" ||
         strMethod == "skale_performanceTrackingFetch" )
        return dev::VerbositySilent;

    // print special
    if ( strMethod.find( "admin_" ) == 0 || strMethod.find( "miner_" ) == 0 ||
         strMethod.find( "personal_" ) == 0 || strMethod.find( "debug_" ) ) {
        return dev::VerbosityDebug;
    }

    if ( this->opts_.isTraceCalls_ )
        return dev::VerbosityTrace;

    return dev::VerbositySilent;
}

bool SkaleServerOverride::checkAdminOriginAllowed( const std::string& origin ) const {
    return chainParams().checkAdminOriginAllowed( origin );
}

jsonrpc::IClientConnectionHandler* SkaleServerOverride::GetHandler( const std::string& url ) {
    if ( jsonrpc::AbstractServerConnector::GetHandler() != nullptr )
        return AbstractServerConnector::GetHandler();
    std::map< std::string, jsonrpc::IClientConnectionHandler* >::iterator it =
        this->urlhandler.find( url );
    if ( it != this->urlhandler.end() )
        return it->second;
    return nullptr;
}

void SkaleServerOverride::logPerformanceWarning( double lfExecutionDuration, int ipVer,
    const char* strProtocol, int nServerIndex, e_server_mode_t esm, const char* strOrigin,
    const char* strMethod, nlohmann::json joID ) {
    std::stringstream ssProtocol;
    strProtocol = ( strProtocol && strProtocol[0] ) ? strProtocol : "Unknown network protocol";
    ssProtocol << cc::info( strProtocol );
    if ( ipVer > 0 )
        ssProtocol << cc::debug( "/" ) << cc::notice( "IPv" ) << cc::num10( ipVer );
    if ( nServerIndex >= 0 )
        ssProtocol << cc::debug( "/" ) << cc::num10( nServerIndex );
    ssProtocol << cc::debug( "/" ) << cc::notice( esm2str( esm ) );
    ssProtocol << cc::info( std::string( ":" ) );
    std::string strProtocolDescription = ssProtocol.str();
    //
    std::string strCallID = joID.dump();
    //
    std::stringstream ssMessage;
    ssMessage << cc::deep_warn( "Performance warning:" ) << " " << cc::c( lfExecutionDuration )
              << cc::warn( " seconds execution time for " ) << cc::info( strMethod )
              << cc::warn( " call with " ) << cc::notice( "id" ) << cc::warn( "=" )
              << cc::info( strCallID ) << cc::warn( " when called from origin " )
              << cc::notice( strOrigin );
    if ( nServerIndex >= 0 )
        ssMessage << cc::warn( " through server with " ) << cc::notice( "index" ) << cc::warn( "=" )
                  << cc::num10( nServerIndex );
    std::string strMessage = ssMessage.str();
    clog( dev::VerbosityWarning, strProtocolDescription ) << strMessage;
}

void SkaleServerOverride::logTraceServerEvent( bool isError, int ipVer, const char* strProtocol,
    int nServerIndex, e_server_mode_t esm, const std::string& strMessage ) {
    if ( strMessage.empty() )
        return;
    std::stringstream ssProtocol;
    strProtocol = ( strProtocol && strProtocol[0] ) ? strProtocol : "Unknown network protocol";
    ssProtocol << cc::info( strProtocol );
    if ( ipVer > 0 )
        ssProtocol << cc::debug( "/" ) << cc::notice( "IPv" ) << cc::num10( ipVer );
    if ( nServerIndex >= 0 )
        ssProtocol << cc::debug( "/" ) << cc::num10( nServerIndex );
    ssProtocol << cc::debug( "/" ) << cc::notice( esm2str( esm ) );
    if ( isError )
        ssProtocol << cc::fatal( std::string( " ERROR:" ) );
    else
        ssProtocol << cc::info( std::string( ":" ) );
    std::string strProtocolDescription = ssProtocol.str();
    if ( isError )
        clog( dev::VerbosityError, strProtocolDescription ) << strMessage;
    else
        clog( dev::VerbosityDebug, strProtocolDescription ) << strMessage;
}

void SkaleServerOverride::logTraceServerTraffic( bool isRX, dev::Verbosity verbosity, int ipVer,
    const char* strProtocol, int nServerIndex, e_server_mode_t esm, const char* strOrigin,
    const std::string& strPayload ) {
    bool isError = verbosity == dev::VerbosityError;

    std::stringstream ssProtocol;
    std::string strProto =
        ( strProtocol && strProtocol[0] ) ? strProtocol : "Unknown network protocol";
    strOrigin = ( strOrigin && strOrigin[0] ) ? strOrigin : "unknown origin";
    std::string strErrorSuffix, strOriginSuffix, strDirect;
    if ( isRX ) {
        strDirect = cc::ws_rx( " >>> " );
        ssProtocol << cc::ws_rx_inv( " >>> " + strProto );
        if ( ipVer > 0 )
            ssProtocol << cc::debug( "/" ) << cc::notice( "IPv" ) << cc::num10( ipVer );
        if ( nServerIndex >= 0 )
            ssProtocol << cc::ws_rx_inv( "/" + std::to_string( nServerIndex ) );
        ssProtocol << cc::debug( "/" ) << cc::notice( esm2str( esm ) );
        ssProtocol << cc::ws_rx_inv( "/RX >>> " );
    } else {
        strDirect = cc::ws_tx( " <<< " );
        ssProtocol << cc::ws_tx_inv( " <<< " + strProto );
        if ( ipVer > 0 )
            ssProtocol << cc::debug( "/" ) << cc::notice( "IPv" ) << cc::num10( ipVer );
        if ( nServerIndex >= 0 )
            ssProtocol << cc::ws_tx_inv( "/" + std::to_string( nServerIndex ) );
        ssProtocol << cc::debug( "/" ) << cc::notice( esm2str( esm ) );
        ssProtocol << cc::ws_tx_inv( "/TX <<< " );
    }
    strOriginSuffix = cc::u( strOrigin );
    if ( isError )
        strErrorSuffix = cc::fatal( " ERROR " );
    std::string strProtocolDescription = ssProtocol.str();

    clog( verbosity, strProtocolDescription )
        << ( strErrorSuffix + strOriginSuffix + strDirect + strPayload );
}

static void stat_check_port_availability_for_server_to_start_listen( int ipVer, const char* strAddr,
    int nPort, e_server_mode_t esm, const char* strProtocolName, int nServerIndex,
    SkaleServerOverride* pSO ) {
    pSO->logTraceServerEvent( false, ipVer, strProtocolName, nServerIndex, esm,
        cc::debug( "Will check port " ) + cc::num10( nPort ) +
            cc::debug( "/IPv" + std::to_string( ipVer ) ) + cc::debug( "/" ) +
            cc::notice( esm2str( esm ) ) + cc::debug( " availability for " ) +
            cc::info( strProtocolName ) + cc::debug( " server..." ) );
    skutils::network::sockaddr46 sa46;
    std::string strError =
        skutils::network::resolve_address_for_client_connection( ipVer, strAddr, sa46 );
    if ( !strError.empty() )
        throw std::runtime_error(
            std::string( "Failed to check " ) + std::string( strProtocolName ) +
            std::string( " server listen IP address availability for address \"" ) + strAddr +
            std::string( "\" on IPv" ) + std::to_string( ipVer ) + cc::debug( "/" ) +
            cc::notice( esm2str( esm ) ) +
            std::string(
                ", please check network interface with this IP address exist, error details: " ) +
            strError );
    if ( is_tcp_port_listening( ipVer, sa46, nPort ) )
        throw std::runtime_error(
            std::string( "Cannot start " ) + std::string( strProtocolName ) +
            std::string( " server on address \"" ) + strAddr + std::string( "\", port " ) +
            std::to_string( nPort ) + std::string( ", IPv" ) + std::to_string( ipVer ) +
            std::string( "/" ) + esm2str( esm ) + std::string( " - port is already listening" ) );
    pSO->logTraceServerEvent( false, ipVer, strProtocolName, nServerIndex, esm,
        cc::notice( "Port " ) + cc::num10( nPort ) +
            cc::notice( "/IPv" + std::to_string( ipVer ) ) + cc::debug( "/" ) +
            cc::notice( esm2str( esm ) ) + cc::notice( " is free for " ) +
            cc::info( strProtocolName ) + cc::notice( " server to start" ) );
}

string hostname_to_ip( string hostname ) {
    struct hostent* he;
    struct in_addr** addr_list;
    int i;

    if ( ( he = gethostbyname( hostname.c_str() ) ) == NULL ) {
        return "";
    }

    addr_list = ( struct in_addr** ) he->h_addr_list;

    for ( i = 0; addr_list[i] != NULL; i++ ) {
        // Return the first one;
        return string( inet_ntoa( *addr_list[i] ) );
    }

    return "";
}

skutils::result_of_http_request SkaleServerOverride::implHandleHttpRequest(
    const nlohmann::json& joIn, const std::string& strProtocol, int nServerIndex,
    std::string strOrigin, int ipVer, int nPort, e_server_mode_t esm ) {
    skutils::result_of_http_request rslt;
    rslt.isBinary_ = false;
    std::string strMethod;
    nlohmann::json jarrRequest, joID = "-1";
    bool isBatch = false;
    try {
        // fetch method name and id earlier
        if ( joIn.is_array() ) {
            isBatch = true;
            jarrRequest = joIn;
        } else {
            jarrRequest = nlohmann::json::array();
            jarrRequest.push_back( joIn );
        }
        for ( const nlohmann::json& jsonRpcRequest : jarrRequest ) {
            std::string methodName =
                skutils::tools::getFieldSafe< std::string >( jsonRpcRequest, "method" );
            if ( methodName.empty() )
                throw std::runtime_error( "Bad JSON RPC request, \"method\" name is missing" );

            strMethod = methodName;
            if ( jsonRpcRequest.count( "id" ) == 0 )
                throw std::runtime_error( "Bad JSON RPC request, \"id\" name is missing" );
            joID = jsonRpcRequest["id"];
        }
        if ( isBatch ) {
            size_t cntInBatch = jarrRequest.size();
            if ( cntInBatch > maxCountInBatchJsonRpcRequest_ )
                throw std::runtime_error( "Bad JSON RPC request, too much requests in batch" );
        }
    } catch ( ... ) {
        std::string e = "Bad JSON RPC request: " + joIn.dump();
        throw std::runtime_error( e );
    }
    //
    // unddos
    skutils::url url_unddos_origin( strOrigin );
    const std::string str_unddos_origin = url_unddos_origin.host();

    static string mainnet_proxy_ip_address = hostname_to_ip( "api.skalenodes.com" );
    static string testnet_proxy_ip_address = hostname_to_ip( "testnet-api.skalenodes.com" );

    skutils::unddos::e_high_load_detection_result_t ehldr;
    if ( str_unddos_origin == mainnet_proxy_ip_address ||
         str_unddos_origin == testnet_proxy_ip_address ) {
        ehldr = skutils::unddos::e_high_load_detection_result_t::ehldr_no_error;
    } else {
        ehldr = unddos_.register_call_from_origin( str_unddos_origin, strMethod );
    }
    switch ( ehldr ) {
    case skutils::unddos::e_high_load_detection_result_t::ehldr_peak:     // ban by too high
                                                                          // load per minute
    case skutils::unddos::e_high_load_detection_result_t::ehldr_lengthy:  // ban by too high
                                                                          // load per second
    case skutils::unddos::e_high_load_detection_result_t::ehldr_ban:      // still banned
    case skutils::unddos::e_high_load_detection_result_t::ehldr_bad_origin: {
        if ( strMethod.empty() )
            strMethod = isBatch ? "batch_json_rpc_request" : "unknown_json_rpc_method";
        std::string reason_part =
            ( ehldr == skutils::unddos::e_high_load_detection_result_t::ehldr_bad_origin ) ?
                "bad origin" :
                "high load";
        std::string e = "Banned due to " + reason_part + " JSON RPC request: " + joIn.dump();
        throw std::runtime_error( e );
    }
        // break;
    case skutils::unddos::e_high_load_detection_result_t::ehldr_no_error:
    default: {
        // no error
    } break;
    }  // switch( ehldr )
    //
    //
    nlohmann::json jarrBatchAnswer;
    if ( isBatch )
        jarrBatchAnswer = nlohmann::json::array();
    for ( const nlohmann::json& joRequest : jarrRequest ) {
        std::string strBody = joRequest.dump();  // = req.body_;
        std::string strPerformanceQueueName =
            skutils::tools::format( "rpc/%s/%zu", strProtocol.c_str(), nServerIndex );
        std::string strPerformanceActionName = skutils::tools::format(
            "%s task %zu, %s", strProtocol.c_str(), nTaskNumberCall_++, strMethod.c_str() );
        skutils::task::performance::action a( strPerformanceQueueName, strPerformanceActionName );
        //
        skutils::stats::time_tracker::element_ptr_t rttElement;
        rttElement.emplace( "RPC", strProtocol.c_str(), strMethod.c_str(), nServerIndex, ipVer );
        //
        SkaleServerConnectionsTrackHelper sscth( *this );
        if ( methodTraceVerbosity( strMethod ) != dev::VerbositySilent )
            logTraceServerTraffic( true, methodTraceVerbosity( strMethod ), ipVer,
                strProtocol.c_str(), nServerIndex, esm, strOrigin.c_str(),
                implPreformatTrafficJsonMessage( strBody, true ) );
        std::string strResponse;
        bool bPassed = false;
        try {
            if ( is_connection_limit_overflow() ) {
                on_connection_overflow_peer_closed(
                    ipVer, strProtocol.c_str(), nServerIndex, nPort, esm );
                throw std::runtime_error( "server too busy" );
            }
            strMethod = skutils::tools::getFieldSafe< std::string >( joRequest, "method" );
            if ( !handleAdminOriginFilter( strMethod, strOrigin ) ) {
                throw std::runtime_error( "origin not allowed for call attempt" );
            }
            jsonrpc::IClientConnectionHandler* handler = GetHandler( "/" );
            if ( handler == nullptr )
                throw std::runtime_error( "No client connection handler found" );
            //
            stats::register_stats_message( strProtocol.c_str(), "POST", strBody.size() );
            stats::register_stats_message( ( "RPC/" + strProtocol ).c_str(), joRequest );
            stats::register_stats_message( "RPC", joRequest );
            //
            std::vector< uint8_t > buffer;
            if ( handleRequestWithBinaryAnswer( esm, joRequest, buffer ) ) {
                stats::register_stats_answer( strProtocol.c_str(), "POST", buffer.size() );
                rttElement->stop();
                rslt.isBinary_ = true;
                rslt.vecBytes_ = buffer;
                return rslt;
            }
            if ( !handleHttpSpecificRequest( strOrigin, esm, strBody, strResponse ) ) {
                handler->HandleRequest( strBody.c_str(), strResponse );
            }
            //
            stats::register_stats_answer( strProtocol.c_str(), "POST", strResponse.size() );
            nlohmann::json joResponse = nlohmann::json::parse( strResponse );
            stats::register_stats_answer( ( "RPC/" + strProtocol ).c_str(), joRequest, joResponse );
            stats::register_stats_answer( "RPC", joRequest, joResponse );
            //
            if ( !isBatch ) {
                rslt.isBinary_ = false;
                rslt.joOut_ = nlohmann::json::parse( strResponse );
            }
            a.set_json_out( joResponse );
            bPassed = true;
        } catch ( const std::exception& ex ) {
            rttElement->setError();
            logTraceServerTraffic( false, dev::VerbosityError, ipVer, strProtocol.c_str(),
                nServerIndex, esm, strOrigin.c_str(), cc::warn( ex.what() ) );
            nlohmann::json joErrorResponce;
            joErrorResponce["id"] = joID;
            nlohmann::json joErrorObj;
            joErrorObj["code"] = -32000;
            joErrorObj["message"] = std::string( ex.what() );
            joErrorResponce["error"] = joErrorObj;
            strResponse = joErrorResponce.dump();
            stats::register_stats_exception( strProtocol.c_str(), "POST" );
            if ( !strMethod.empty() ) {
                stats::register_stats_exception( strProtocol.c_str(), strMethod.c_str() );
                stats::register_stats_exception( "RPC", strMethod.c_str() );
            }
            if ( !isBatch ) {
                rslt.isBinary_ = false;
                rslt.joOut_ = joErrorResponce;
            }
            a.set_json_err( joErrorResponce );
        } catch ( ... ) {
            rttElement->setError();
            const char* e = "unknown exception in SkaleServerOverride";
            logTraceServerTraffic( false, dev::VerbosityError, ipVer, strProtocol.c_str(),
                nServerIndex, esm, strOrigin.c_str(), cc::warn( e ) );
            nlohmann::json joErrorResponce;
            joErrorResponce["id"] = joID;
            nlohmann::json joErrorObj;
            joErrorObj["code"] = -32000;
            joErrorObj["message"] = std::string( e );
            joErrorResponce["error"] = joErrorObj;
            strResponse = joErrorResponce.dump();
            stats::register_stats_exception( strProtocol.c_str(), "POST" );
            if ( !strMethod.empty() ) {
                stats::register_stats_exception( strProtocol.c_str(), strMethod.c_str() );
                stats::register_stats_exception( "RPC", strMethod.c_str() );
            }
            if ( !isBatch ) {
                rslt.isBinary_ = false;
                rslt.joOut_ = joErrorResponce;
            }
            a.set_json_err( joErrorResponce );
        }
        if ( methodTraceVerbosity( strMethod ) != dev::VerbositySilent )
            logTraceServerTraffic( false, methodTraceVerbosity( strMethod ), ipVer,
                strProtocol.c_str(), nServerIndex, esm, strOrigin.c_str(),
                implPreformatTrafficJsonMessage( strResponse, false ) );
        if ( isBatch ) {
            nlohmann::json joAnswerPart = nlohmann::json::parse( strResponse );
            jarrBatchAnswer.push_back( joAnswerPart );
        } else {
            rslt.isBinary_ = false;
            rslt.joOut_ = nlohmann::json::parse( strResponse );
        }
        if ( !bPassed )
            stats::register_stats_answer( strProtocol.c_str(), "POST", strResponse.size() );
        rttElement->stop();
        double lfExecutionDuration = rttElement->getDurationInSeconds();  // in seconds
        if ( lfExecutionDuration >= opts_.lfExecutionDurationMaxForPerformanceWarning_ )
            logPerformanceWarning( lfExecutionDuration, ipVer, strProtocol.c_str(), nServerIndex,
                esm, strOrigin.c_str(), strMethod.c_str(), joID );
    }  // for( const nlohmann::json & joRequest : jarrRequest )
    if ( isBatch ) {
        rslt.isBinary_ = false;  // batch request can be only text/JSON
        rslt.joOut_ = jarrBatchAnswer;
    }
    return rslt;
}


bool SkaleServerOverride::implStartListening(  // web socket
    std::shared_ptr< SkaleRelayWS >& pSrv, int ipVer, const std::string& strAddr, int nPort,
    const std::string& strPathSslKey, const std::string& strPathSslCert,
    const std::string& /*strPathSslCA*/, int nServerIndex, e_server_mode_t esm ) {
    bool bIsSSL = false;
    if ( ( !strPathSslKey.empty() ) && ( !strPathSslCert.empty() ) )
        bIsSSL = true;
    try {
        implStopListening( pSrv, ipVer, bIsSSL, esm );
        if ( strAddr.empty() || nPort <= 0 )
            return true;
        logTraceServerEvent( false, ipVer, bIsSSL ? "WSS" : "WS", nServerIndex, esm,
            cc::debug( "starting " ) + cc::info( bIsSSL ? "WSS" : "WS" ) + cc::debug( "/" ) +
                cc::num10( nServerIndex ) + cc::debug( "/" ) + cc::notice( esm2str( esm ) ) +
                cc::debug( " server on address " ) + cc::info( strAddr ) +
                cc::debug( " and port " ) + cc::c( nPort ) + cc::debug( "..." ) );
        pSrv.reset( new SkaleRelayWS(
            ipVer, strAddr.c_str(), bIsSSL ? "wss" : "ws", nPort, esm, nServerIndex, &bns4ws_ ) );
        if ( bIsSSL ) {
            pSrv->strCertificateFile_ = strPathSslCert;
            pSrv->strPrivateKeyFile_ = strPathSslKey;
        }
        // check if somebody is already listening
        stat_check_port_availability_for_server_to_start_listen(
            ipVer, strAddr.c_str(), nPort, esm, bIsSSL ? "WSS" : "WS", pSrv->serverIndex(), this );
        // make server listen in its dedicated thread
        if ( !pSrv->start( this ) )
            throw std::runtime_error( "Failed to start server" );
        logTraceServerEvent( false, ipVer, bIsSSL ? "WSS" : "WS", pSrv->serverIndex(), esm,
            cc::success( "OK, started " ) + cc::info( bIsSSL ? "WSS" : "WS" ) + cc::debug( "/" ) +
                cc::num10( pSrv->serverIndex() ) + cc::success( " server on address " ) +
                cc::info( strAddr ) + cc::success( " and port " ) + cc::c( nPort ) +
                cc::debug( "..." ) );
        return true;
    } catch ( const std::exception& ex ) {
        logTraceServerEvent( false, ipVer, bIsSSL ? "WSS" : "WS", pSrv->serverIndex(), esm,
            cc::fatal( "FAILED" ) + cc::error( " to start " ) + cc::warn( bIsSSL ? "WSS" : "WS" ) +
                cc::error( " server: " ) + cc::warn( ex.what() ) );
    } catch ( ... ) {
        logTraceServerEvent( false, ipVer, bIsSSL ? "WSS" : "WS", pSrv->serverIndex(), esm,
            cc::fatal( "FAILED" ) + cc::error( " to start " ) + cc::warn( bIsSSL ? "WSS" : "WS" ) +
                cc::error( " server: " ) + cc::warn( "unknown exception" ) );
    }
    try {
        implStopListening( pSrv, ipVer, bIsSSL, esm );
    } catch ( ... ) {
    }
    return false;
}

bool SkaleServerOverride::implStartListening(  // proxygen HTTP
    std::shared_ptr< SkaleRelayProxygenHTTP >& pSrv, int ipVer, const std::string& strAddr,
    int nPort, const std::string& strPathSslKey, const std::string& strPathSslCert,
    const std::string& strPathSslCA, int nServerIndex, e_server_mode_t esm, int32_t threads,
    int32_t threads_limit ) {
    bool bIsSSL = false;
    SkaleServerOverride* pSO = this;
    if ( ( !strPathSslKey.empty() ) && ( !strPathSslCert.empty() ) )
        bIsSSL = true;
    try {
        implStopListening( pSrv, ipVer, bIsSSL, esm );
        if ( strAddr.empty() || nPort <= 0 )
            return true;
        logTraceServerEvent( false, ipVer, bIsSSL ? "HTTPS" : "HTTP", -1, esm,
            cc::debug( "starting " ) + cc::attention( "proxygen" ) + cc::debug( "/" ) +
                cc::info( bIsSSL ? "HTTPS" : "HTTP" ) + cc::debug( "/" ) +
                cc::num10( nServerIndex ) + cc::debug( "/" ) + cc::notice( esm2str( esm ) ) +
                cc::debug( " server on address " ) + cc::info( strAddr ) +
                cc::debug( " and port " ) + cc::c( nPort ) + cc::debug( "..." ) );


        // check if somebody is already listening
        stat_check_port_availability_for_server_to_start_listen(
            ipVer, strAddr.c_str(), nPort, esm, bIsSSL ? "HTTPS" : "HTTP", nServerIndex, this );
        //
        pSrv.reset( new SkaleRelayProxygenHTTP( pSO, ipVer, strAddr.c_str(), nPort,
            strPathSslCert.c_str(), strPathSslKey.c_str(), strPathSslCA.c_str(), nServerIndex, esm,
            threads, threads_limit ) );
        // cher server listen in its dedicated thread(s)
        if ( pSrv->is_running() )
            stats::register_stats_message( bIsSSL ? "HTTPS" : "HTTP", "LISTEN" );
        else
            throw std::runtime_error( "failed to start proxygen server instance" );
        logTraceServerEvent( false, ipVer, bIsSSL ? "HTTPS" : "HTTP", pSrv->serverIndex(), esm,
            cc::success( "OK, started " ) + cc::attention( "proxygen" ) + cc::debug( "/" ) +
                cc::info( bIsSSL ? "HTTPS" : "HTTP" ) + cc::debug( "/" ) +
                cc::num10( pSrv->serverIndex() ) + cc::success( " server on address " ) +
                cc::info( strAddr ) + cc::success( " and port " ) + cc::c( nPort ) +
                cc::success( "/" ) + cc::notice( esm2str( esm ) ) + " " );
        return true;
    } catch ( const std::exception& ex ) {
        logTraceServerEvent( false, ipVer, bIsSSL ? "HTTPS" : "HTTP",
            pSrv ? pSrv->serverIndex() : -1, esm,
            cc::fatal( "FAILED" ) + cc::error( " to start " ) + cc::attention( "proxygen" ) +
                cc::debug( "/" ) + cc::warn( bIsSSL ? "HTTPS" : "HTTP" ) +
                cc::error( " server: " ) + cc::warn( ex.what() ) );
    } catch ( ... ) {
        logTraceServerEvent( false, ipVer, bIsSSL ? "HTTPS" : "HTTP",
            pSrv ? pSrv->serverIndex() : -1, esm,
            cc::fatal( "FAILED" ) + cc::error( " to start " ) + cc::attention( "proxygen" ) +
                cc::debug( "/" ) + cc::warn( bIsSSL ? "HTTPS" : "HTTP" ) +
                cc::error( " server: " ) + cc::warn( "unknown exception" ) );
    }
    try {
        implStopListening( pSrv, ipVer, bIsSSL, esm );
    } catch ( ... ) {
    }
    return false;
}


bool SkaleServerOverride::implStopListening(  // web socket
    std::shared_ptr< SkaleRelayWS >& pSrv, int ipVer, bool bIsSSL, e_server_mode_t esm ) {
    try {
        if ( !pSrv )
            return true;
        const net_bind_opts_t& bo = ( esm == e_server_mode_t::esm_standard ) ?
                                        opts_.netOpts_.bindOptsStandard_ :
                                        opts_.netOpts_.bindOptsInformational_;
        int nServerIndex = pSrv->serverIndex();
        std::string strAddr = ( ipVer == 4 ) ? ( bIsSSL ? bo.strAddrWSS4_ : bo.strAddrWS4_ ) :
                                               ( bIsSSL ? bo.strAddrWSS6_ : bo.strAddrWS6_ );
        int nPort = ( ( ipVer == 4 ) ? ( bIsSSL ? bo.nBasePortWSS4_ : bo.nBasePortWS4_ ) :
                                       ( bIsSSL ? bo.nBasePortWSS6_ : bo.nBasePortWS6_ ) ) +
                    nServerIndex;
        logTraceServerEvent( false, ipVer, bIsSSL ? "WSS" : "WS", nServerIndex, esm,
            cc::notice( "Will stop " ) + cc::info( bIsSSL ? "WSS" : "WS" ) +
                cc::notice( " server on address " ) + cc::info( strAddr ) +
                cc::success( " and port " ) + cc::c( nPort ) + cc::debug( "/" ) +
                cc::notice( esm2str( esm ) ) + cc::notice( "..." ) );
        if ( pSrv->isRunning() )
            pSrv->stop();
        pSrv.reset();
        logTraceServerEvent( false, ipVer, bIsSSL ? "WSS" : "WS", nServerIndex, esm,
            cc::success( "OK, stopped " ) + cc::info( bIsSSL ? "WSS" : "WS" ) +
                cc::success( " server on address " ) + cc::info( strAddr ) +
                cc::success( " and port " ) + cc::c( nPort ) + cc::debug( "/" ) +
                cc::notice( esm2str( esm ) ) );
    } catch ( ... ) {
    }
    return true;
}

bool SkaleServerOverride::implStopListening(  // proxygen HTTP
    std::shared_ptr< SkaleRelayProxygenHTTP >& pSrv, int ipVer, bool bIsSSL, e_server_mode_t esm ) {
    try {
        if ( !pSrv )
            return true;
        if ( !pSrv->is_running() )
            return true;
        const net_bind_opts_t& bo = ( esm == e_server_mode_t::esm_standard ) ?
                                        opts_.netOpts_.bindOptsStandard_ :
                                        opts_.netOpts_.bindOptsInformational_;
        int nServerIndex = pSrv->serverIndex();
        std::string strAddr = ( ipVer == 4 ) ? ( bIsSSL ? bo.strAddrHTTPS4_ : bo.strAddrHTTP4_ ) :
                                               ( bIsSSL ? bo.strAddrHTTPS6_ : bo.strAddrHTTP6_ );
        int nPort = ( ( ipVer == 4 ) ? ( bIsSSL ? bo.nBasePortHTTPS4_ : bo.nBasePortHTTP4_ ) :
                                       ( bIsSSL ? bo.nBasePortHTTPS6_ : bo.nBasePortHTTP6_ ) ) +
                    nServerIndex;
        logTraceServerEvent( false, ipVer, bIsSSL ? "HTTPS" : "HTTP", nServerIndex, esm,
            cc::notice( "Will stop " ) + cc::attention( "proxygen" ) + cc::debug( "/" ) +
                cc::info( bIsSSL ? "HTTPS" : "HTTP" ) + cc::notice( " server on address " ) +
                cc::info( strAddr ) + cc::success( " and port " ) + cc::c( nPort ) +
                cc::debug( "/" ) + cc::notice( esm2str( esm ) ) + cc::notice( "..." ) );
        pSrv->stop();
        stats::register_stats_message( bIsSSL ? "HTTPS" : "HTTP", "STOP" );
        pSrv.reset();
        logTraceServerEvent( false, ipVer, bIsSSL ? "HTTPS" : "HTTP", nServerIndex, esm,
            cc::success( "OK, stopped " ) + cc::attention( "proxygen" ) + cc::debug( "/" ) +
                cc::info( bIsSSL ? "HTTPS" : "HTTP" ) + cc::success( " server on address " ) +
                cc::info( strAddr ) + cc::success( " and port " ) + cc::c( nPort ) +
                cc::debug( "/" ) + cc::notice( esm2str( esm ) ) );
    } catch ( ... ) {
    }
    return true;
}

bool SkaleServerOverride::StartListening( e_server_mode_t esm ) {
    m_bShutdownMode = false;
    const net_bind_opts_t& bo = ( esm == e_server_mode_t::esm_standard ) ?
                                    opts_.netOpts_.bindOptsStandard_ :
                                    opts_.netOpts_.bindOptsInformational_;
    size_t nServerIndex;

    std::list< std::shared_ptr< SkaleRelayWS > >& serversWS4 =
        ( esm == e_server_mode_t::esm_standard ) ? serversWS4std_ : serversWS4nfo_;
    if ( 0 <= bo.nBasePortWS4_ && bo.nBasePortWS4_ <= 65535 &&
         bo.nBasePortWS4_ != bo.nBasePortHTTP4_ && bo.nBasePortWS4_ != bo.nBasePortHTTPS4_ ) {
        for ( nServerIndex = 0; nServerIndex < bo.cntServers_; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayWS > pServer;
            if ( !implStartListening(  // web socket
                     pServer, 4, bo.strAddrWS4_, bo.nBasePortWS4_ + nServerIndex, "", "", "",
                     nServerIndex, esm ) )
                return false;
            serversWS4.push_back( pServer );
        }
    }
    std::list< std::shared_ptr< SkaleRelayWS > >& serversWS6 =
        ( esm == e_server_mode_t::esm_standard ) ? serversWS6std_ : serversWS6nfo_;
    if ( 0 <= bo.nBasePortWS6_ && bo.nBasePortWS6_ <= 65535 &&
         bo.nBasePortWS6_ != bo.nBasePortHTTP6_ && bo.nBasePortWS6_ != bo.nBasePortHTTPS6_ ) {
        for ( nServerIndex = 0; nServerIndex < bo.cntServers_; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayWS > pServer;
            if ( !implStartListening(  // web socket
                     pServer, 6, bo.strAddrWS6_, bo.nBasePortWS6_ + nServerIndex, "", "", "",
                     nServerIndex, esm ) )
                return false;
            serversWS6.push_back( pServer );
        }
    }
    std::list< std::shared_ptr< SkaleRelayWS > >& serversWSS4 =
        ( esm == e_server_mode_t::esm_standard ) ? serversWSS4std_ : serversWSS4nfo_;
    if ( 0 <= bo.nBasePortWSS4_ && bo.nBasePortWSS4_ <= 65535 &&
         ( !opts_.netOpts_.strPathSslKey_.empty() ) &&
         ( !opts_.netOpts_.strPathSslCert_.empty() ) && bo.nBasePortWSS4_ != bo.nBasePortWS4_ &&
         bo.nBasePortWSS4_ != bo.nBasePortHTTP4_ && bo.nBasePortWSS4_ != bo.nBasePortHTTPS4_ ) {
        for ( nServerIndex = 0; nServerIndex < bo.cntServers_; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayWS > pServer;
            if ( !implStartListening(  // web socket
                     pServer, 4, bo.strAddrWSS4_, bo.nBasePortWSS4_ + nServerIndex,
                     opts_.netOpts_.strPathSslKey_, opts_.netOpts_.strPathSslCert_,
                     opts_.netOpts_.strPathSslCA_, nServerIndex, esm ) )
                return false;
            serversWSS4.push_back( pServer );
        }
    }
    std::list< std::shared_ptr< SkaleRelayWS > >& serversWSS6 =
        ( esm == e_server_mode_t::esm_standard ) ? serversWSS6std_ : serversWSS6nfo_;
    if ( 0 <= bo.nBasePortWSS6_ && bo.nBasePortWSS6_ <= 65535 &&
         ( !opts_.netOpts_.strPathSslKey_.empty() ) &&
         ( !opts_.netOpts_.strPathSslCert_.empty() ) && bo.nBasePortWSS6_ != bo.nBasePortWS6_ &&
         bo.nBasePortWSS6_ != bo.nBasePortHTTP6_ && bo.nBasePortWSS6_ != bo.nBasePortHTTPS6_ ) {
        for ( nServerIndex = 0; nServerIndex < bo.cntServers_; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayWS > pServer;
            if ( !implStartListening(  // web socket
                     pServer, 6, bo.strAddrWSS6_, bo.nBasePortWSS6_ + nServerIndex,
                     opts_.netOpts_.strPathSslKey_, opts_.netOpts_.strPathSslCert_,
                     opts_.netOpts_.strPathSslCA_, nServerIndex, esm ) )
                return false;
            serversWSS6.push_back( pServer );
        }
    }
    //
    //
    std::list< std::shared_ptr< SkaleRelayProxygenHTTP > >& serversProxygenHTTP4 =
        ( esm == e_server_mode_t::esm_standard ) ? serversProxygenHTTP4std_ :
                                                   serversProxygenHTTP4nfo_;
    if ( 0 <= bo.nBasePortHTTP4_ && bo.nBasePortHTTP4_ <= 65535 ) {
        for ( nServerIndex = 0; nServerIndex < bo.cntServers_; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayProxygenHTTP > pServer;
            if ( !implStartListening(  // proxygen HTTP
                     pServer, 4, bo.strAddrHTTP4_, bo.nBasePortHTTP4_ + nServerIndex, "", "", "",
                     nServerIndex, esm, pg_threads_, pg_threads_limit_ ) )
                return false;
            serversProxygenHTTP4.push_back( pServer );
        }
    }
    std::list< std::shared_ptr< SkaleRelayProxygenHTTP > >& serversProxygenHTTP6 =
        ( esm == e_server_mode_t::esm_standard ) ? serversProxygenHTTP6std_ :
                                                   serversProxygenHTTP6nfo_;
    if ( 0 <= bo.nBasePortHTTP6_ && bo.nBasePortHTTP6_ <= 65535 ) {
        for ( nServerIndex = 0; nServerIndex < bo.cntServers_; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayProxygenHTTP > pServer;
            if ( !implStartListening(  // proxygen HTTP
                     pServer, 6, bo.strAddrHTTP6_, bo.nBasePortHTTP6_ + nServerIndex, "", "", "",
                     nServerIndex, esm, pg_threads_, pg_threads_limit_ ) )
                return false;
            serversProxygenHTTP6.push_back( pServer );
        }
    }
    std::list< std::shared_ptr< SkaleRelayProxygenHTTP > >& serversProxygenHTTPS4 =
        ( esm == e_server_mode_t::esm_standard ) ? serversProxygenHTTPS4std_ :
                                                   serversProxygenHTTPS4nfo_;
    if ( 0 <= bo.nBasePortHTTPS4_ && bo.nBasePortHTTPS4_ <= 65535 &&
         ( !opts_.netOpts_.strPathSslKey_.empty() ) &&
         ( !opts_.netOpts_.strPathSslCert_.empty() ) &&
         bo.nBasePortHTTPS4_ != bo.nBasePortHTTP4_ ) {
        for ( nServerIndex = 0; nServerIndex < bo.cntServers_; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayProxygenHTTP > pServer;
            if ( !implStartListening(  // proxygen HTTP
                     pServer, 4, bo.strAddrHTTPS4_, bo.nBasePortHTTPS4_ + nServerIndex,
                     opts_.netOpts_.strPathSslKey_, opts_.netOpts_.strPathSslCert_,
                     opts_.netOpts_.strPathSslCA_, nServerIndex, esm, pg_threads_,
                     pg_threads_limit_ ) )
                return false;
            serversProxygenHTTPS4.push_back( pServer );
        }
    }
    std::list< std::shared_ptr< SkaleRelayProxygenHTTP > >& serversProxygenHTTPS6 =
        ( esm == e_server_mode_t::esm_standard ) ? serversProxygenHTTPS6std_ :
                                                   serversProxygenHTTPS6nfo_;
    if ( 0 <= bo.nBasePortHTTPS6_ && bo.nBasePortHTTPS6_ <= 65535 &&
         ( !opts_.netOpts_.strPathSslKey_.empty() ) &&
         ( !opts_.netOpts_.strPathSslCert_.empty() ) &&
         bo.nBasePortHTTPS6_ != bo.nBasePortHTTP6_ ) {
        for ( nServerIndex = 0; nServerIndex < bo.cntServers_; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayProxygenHTTP > pServer;
            if ( !implStartListening(  // proxygen HTTP
                     pServer, 6, bo.strAddrHTTPS6_, bo.nBasePortHTTPS6_ + nServerIndex,
                     opts_.netOpts_.strPathSslKey_, opts_.netOpts_.strPathSslCert_,
                     opts_.netOpts_.strPathSslCA_, nServerIndex, esm, pg_threads_,
                     pg_threads_limit_ ) )
                return false;
            serversProxygenHTTPS6.push_back( pServer );
        }
    }
    //
    //
    return true;
}

e_server_mode_t SkaleServerOverride::implGuessProxygenRequestESM(
    const std::string& strDstAddress, int nDstPort ) {
    e_server_mode_t esm = e_server_mode_t::esm_standard;
    if ( implGuessProxygenRequestESM( serversProxygenHTTP4std_, strDstAddress, nDstPort, esm ) )
        return esm;
    if ( implGuessProxygenRequestESM( serversProxygenHTTP6std_, strDstAddress, nDstPort, esm ) )
        return esm;
    if ( implGuessProxygenRequestESM( serversProxygenHTTPS4std_, strDstAddress, nDstPort, esm ) )
        return esm;
    if ( implGuessProxygenRequestESM( serversProxygenHTTPS6std_, strDstAddress, nDstPort, esm ) )
        return esm;
    if ( implGuessProxygenRequestESM( serversProxygenHTTP4nfo_, strDstAddress, nDstPort, esm ) )
        return esm;
    if ( implGuessProxygenRequestESM( serversProxygenHTTP6nfo_, strDstAddress, nDstPort, esm ) )
        return esm;
    if ( implGuessProxygenRequestESM( serversProxygenHTTPS4nfo_, strDstAddress, nDstPort, esm ) )
        return esm;
    if ( implGuessProxygenRequestESM( serversProxygenHTTPS6nfo_, strDstAddress, nDstPort, esm ) )
        return esm;
    clog( dev::VerbosityWarning, cc::fatal( "WARNING:" ) )
        << ( cc::warn( "Failed to lookup ESM for " ) + cc::attention( strDstAddress ) +
               cc::warn( ":" ) + cc::num10( nDstPort ) );
    return e_server_mode_t::esm_standard;
}
bool SkaleServerOverride::implGuessProxygenRequestESM(
    std::list< std::shared_ptr< SkaleRelayProxygenHTTP > >& lst, const std::string& strDstAddress,
    int nDstPort, e_server_mode_t& esm ) {
    auto itWalk = lst.cbegin(), itEnd = lst.cend();
    for ( ; itWalk != itEnd; ++itWalk ) {
        auto pServer = ( *itWalk );
        if ( ( pServer->strBindAddr_ == strDstAddress || pServer->strBindAddr_ == "0.0.0.0" ||
                 pServer->strBindAddr_ == "::" ) &&
             pServer->nPort_ == nDstPort ) {
            esm = pServer->esm_;
            return true;
        }
    }
    return false;
}

bool SkaleServerOverride::StartListening() {
    if ( StartListening( e_server_mode_t::esm_standard ) &&
         StartListening( e_server_mode_t::esm_informational ) ) {
        if ( skutils::http_pg::pg_accumulate_size() > 0 ) {
            skutils::http_pg::pg_on_request_handler_t fnHandler =
                [=]( const nlohmann::json& joIn, const std::string& strOrigin, int ipVer,
                    const std::string& strDstAddress,
                    int nDstPort ) -> skutils::result_of_http_request {
                if ( isShutdownMode() )
                    throw std::runtime_error( "query was cancelled due to server shutdown mode" );
                skutils::url u( strOrigin );
                std::string strSchemeUC =
                    skutils::tools::to_upper( skutils::tools::trim_copy( u.scheme() ) );
                std::string strPort = skutils::tools::trim_copy( u.port() );
                int nPort = 0;
                if ( strPort.empty() ) {
                    if ( strSchemeUC == "HTTPS" )
                        nPort = 443;
                    else
                        nPort = 80;
                } else
                    nPort = atoi( u.port().c_str() );
                int nServerIndex = 0;  // TO-FIX: detect server index here"
                e_server_mode_t esm = implGuessProxygenRequestESM( strDstAddress, nDstPort );
                skutils::result_of_http_request rslt = implHandleHttpRequest(
                    joIn, strSchemeUC, nServerIndex, strOrigin, ipVer, nPort, esm );
                return rslt;
            };
            hProxygenServer_ =
                skutils::http_pg::pg_accumulate_start( fnHandler, pg_threads_, pg_threads_limit_ );
            skutils::http_pg::pg_accumulate_clear();
            if ( !hProxygenServer_ ) {
                clog( dev::VerbosityError, cc::fatal( "PROXYGEN ERROR:" ) )
                    << ( cc::error( "Failed to start server" ) );
                return false;
            }
        }
        return true;
    }
    return false;
}

bool SkaleServerOverride::StopListening( e_server_mode_t esm ) {
    bool bRetVal = true;
    if ( hProxygenServer_ ) {
        skutils::http_pg::pg_stop( hProxygenServer_ );
        hProxygenServer_ = nullptr;
    }

    std::list< std::shared_ptr< SkaleRelayWS > >& serversWS4 =
        ( esm == e_server_mode_t::esm_standard ) ? serversWS4std_ : serversWS4nfo_;
    for ( auto pServer : serversWS4 ) {
        if ( !implStopListening( pServer, 4, false, esm ) )
            bRetVal = false;
    }
    serversWS4.clear();
    std::list< std::shared_ptr< SkaleRelayWS > >& serversWS6 =
        ( esm == e_server_mode_t::esm_standard ) ? serversWS6std_ : serversWS6nfo_;
    for ( auto pServer : serversWS6 ) {
        if ( !implStopListening( pServer, 6, false, esm ) )
            bRetVal = false;
    }
    serversWS6.clear();
    std::list< std::shared_ptr< SkaleRelayWS > >& serversWSS4 =
        ( esm == e_server_mode_t::esm_standard ) ? serversWSS4std_ : serversWSS4nfo_;
    for ( auto pServer : serversWSS4 ) {
        if ( !implStopListening( pServer, 4, true, esm ) )
            bRetVal = false;
    }
    serversWSS4.clear();
    std::list< std::shared_ptr< SkaleRelayWS > >& serversWSS6 =
        ( esm == e_server_mode_t::esm_standard ) ? serversWSS6std_ : serversWSS6nfo_;
    for ( auto pServer : serversWSS6 ) {
        if ( !implStopListening( pServer, 6, true, esm ) )
            bRetVal = false;
    }
    serversWSS6.clear();
    //
    //
    std::list< std::shared_ptr< SkaleRelayProxygenHTTP > >& serversProxygenHTTP4 =
        ( esm == e_server_mode_t::esm_standard ) ? serversProxygenHTTP4std_ :
                                                   serversProxygenHTTP4nfo_;
    for ( auto pServer : serversProxygenHTTP4 ) {
        if ( !implStopListening( pServer, 4, false, esm ) )
            bRetVal = false;
    }
    serversProxygenHTTP4.clear();
    std::list< std::shared_ptr< SkaleRelayProxygenHTTP > >& serversProxygenHTTP6 =
        ( esm == e_server_mode_t::esm_standard ) ? serversProxygenHTTP6std_ :
                                                   serversProxygenHTTP6nfo_;
    for ( auto pServer : serversProxygenHTTP6 ) {
        if ( !implStopListening( pServer, 6, false, esm ) )
            bRetVal = false;
    }
    serversProxygenHTTP6.clear();
    std::list< std::shared_ptr< SkaleRelayProxygenHTTP > >& serversProxygenHTTPS4 =
        ( esm == e_server_mode_t::esm_standard ) ? serversProxygenHTTPS4std_ :
                                                   serversProxygenHTTPS4nfo_;
    for ( auto pServer : serversProxygenHTTPS4 ) {
        if ( !implStopListening( pServer, 4, true, esm ) )
            bRetVal = false;
    }
    serversProxygenHTTPS4.clear();
    std::list< std::shared_ptr< SkaleRelayProxygenHTTP > >& serversProxygenHTTPS6 =
        ( esm == e_server_mode_t::esm_standard ) ? serversProxygenHTTPS6std_ :
                                                   serversProxygenHTTPS6nfo_;
    for ( auto pServer : serversProxygenHTTPS6 ) {
        if ( !implStopListening( pServer, 6, true, esm ) )
            bRetVal = false;
    }
    serversProxygenHTTPS6.clear();
    return bRetVal;
}
bool SkaleServerOverride::StopListening() {
    m_bShutdownMode = true;
    bool b1 = StopListening( e_server_mode_t::esm_standard );
    bool b2 = StopListening( e_server_mode_t::esm_informational );
    bool b = ( b1 && b2 ) ? true : false;
    return b;
}


int SkaleServerOverride::getServerPortStatusWS( int ipVer, e_server_mode_t esm ) const {
    const net_bind_opts_t& bo = ( esm == e_server_mode_t::esm_standard ) ?
                                    opts_.netOpts_.bindOptsStandard_ :
                                    opts_.netOpts_.bindOptsInformational_;
    const std::list< std::shared_ptr< SkaleRelayWS > >& serversWS4 =
        ( esm == e_server_mode_t::esm_standard ) ? serversWS4std_ : serversWS4nfo_;
    const std::list< std::shared_ptr< SkaleRelayWS > >& serversWS6 =
        ( esm == e_server_mode_t::esm_standard ) ? serversWS6std_ : serversWS6nfo_;
    for ( auto pServer : ( ( ipVer == 4 ) ? serversWS4 : serversWS6 ) ) {
        if ( pServer && pServer->isRunning() )
            return ( ( ipVer == 4 ) ? bo.nBasePortWS4_ : bo.nBasePortWS6_ ) +
                   pServer->serverIndex();
    }
    return -1;
}
int SkaleServerOverride::getServerPortStatusWSS( int ipVer, e_server_mode_t esm ) const {
    const net_bind_opts_t& bo = ( esm == e_server_mode_t::esm_standard ) ?
                                    opts_.netOpts_.bindOptsStandard_ :
                                    opts_.netOpts_.bindOptsInformational_;
    const std::list< std::shared_ptr< SkaleRelayWS > >& serversWSS4 =
        ( esm == e_server_mode_t::esm_standard ) ? serversWSS4std_ : serversWSS4nfo_;
    const std::list< std::shared_ptr< SkaleRelayWS > >& serversWSS6 =
        ( esm == e_server_mode_t::esm_standard ) ? serversWSS6std_ : serversWSS6nfo_;
    for ( auto pServer : ( ( ipVer == 4 ) ? serversWSS4 : serversWSS6 ) ) {
        if ( pServer && pServer->isRunning() )
            return ( ( ipVer == 4 ) ? bo.nBasePortWSS4_ : bo.nBasePortWSS6_ ) +
                   pServer->serverIndex();
    }
    return -1;
}

int SkaleServerOverride::getServerPortStatusProxygenHTTP( int ipVer, e_server_mode_t esm ) const {
    const net_bind_opts_t& bo = ( esm == e_server_mode_t::esm_standard ) ?
                                    opts_.netOpts_.bindOptsStandard_ :
                                    opts_.netOpts_.bindOptsInformational_;
    const std::list< std::shared_ptr< SkaleRelayProxygenHTTP > >& serversProxygenHTTP4 =
        ( esm == e_server_mode_t::esm_standard ) ? serversProxygenHTTP4std_ :
                                                   serversProxygenHTTP4nfo_;
    const std::list< std::shared_ptr< SkaleRelayProxygenHTTP > >& serversProxygenHTTP6 =
        ( esm == e_server_mode_t::esm_standard ) ? serversProxygenHTTP6std_ :
                                                   serversProxygenHTTP6nfo_;
    for ( auto pServer : ( ( ipVer == 4 ) ? serversProxygenHTTP4 : serversProxygenHTTP6 ) ) {
        if ( pServer && pServer->is_running() )
            return ( ( ipVer == 4 ) ? bo.nBasePortHTTP4_ : bo.nBasePortHTTP6_ ) +
                   pServer->serverIndex();
    }
    return -1;
}
int SkaleServerOverride::getServerPortStatusProxygenHTTPS( int ipVer, e_server_mode_t esm ) const {
    const net_bind_opts_t& bo = ( esm == e_server_mode_t::esm_standard ) ?
                                    opts_.netOpts_.bindOptsStandard_ :
                                    opts_.netOpts_.bindOptsInformational_;
    const std::list< std::shared_ptr< SkaleRelayProxygenHTTP > >& serversProxygenHTTPS4 =
        ( esm == e_server_mode_t::esm_standard ) ? serversProxygenHTTPS4std_ :
                                                   serversProxygenHTTPS4nfo_;
    const std::list< std::shared_ptr< SkaleRelayProxygenHTTP > >& serversProxygenHTTPS6 =
        ( esm == e_server_mode_t::esm_standard ) ? serversProxygenHTTPS6std_ :
                                                   serversProxygenHTTPS6nfo_;
    for ( auto pServer : ( ( ipVer == 4 ) ? serversProxygenHTTPS4 : serversProxygenHTTPS6 ) ) {
        if ( pServer && pServer->is_running() )
            return ( ( ipVer == 4 ) ? bo.nBasePortHTTPS4_ : bo.nBasePortHTTPS6_ ) +
                   pServer->serverIndex();
    }
    return -1;
}

bool SkaleServerOverride::is_connection_limit_overflow() const {
    size_t cntConnectionsMax = size_t( opts_.netOpts_.cntConnectionsMax_ );
    if ( cntConnectionsMax == 0 )
        return false;
    size_t cntConnections = size_t( opts_.netOpts_.cntConnections_ );
    if ( cntConnections <= cntConnectionsMax )
        return false;
    return true;
}
void SkaleServerOverride::connection_counter_inc() {
    ++opts_.netOpts_.cntConnections_;
}
void SkaleServerOverride::connection_counter_dec() {
    --opts_.netOpts_.cntConnections_;
}
size_t SkaleServerOverride::max_connection_get() const {
    size_t cntConnectionsMax = size_t( opts_.netOpts_.cntConnectionsMax_ );
    return cntConnectionsMax;
}
void SkaleServerOverride::max_connection_set( size_t cntConnectionsMax ) {
    opts_.netOpts_.cntConnectionsMax_ = cntConnectionsMax;
}

void SkaleServerOverride::on_connection_overflow_peer_closed(
    int ipVer, const char* strProtocol, int nServerIndex, int nPort, e_server_mode_t esm ) {
    std::string strMessage = cc::info( strProtocol ) + cc::debug( "/" ) +
                             cc::num10( nServerIndex ) + cc::warn( " server on port " ) +
                             cc::num10( nPort ) +
                             cc::warn( " did closed peer because of connection limit overflow" );
    logTraceServerEvent( false, ipVer, strProtocol, nServerIndex, esm, strMessage );
}

skutils::tools::load_monitor& stat_get_load_monitor() {
    static skutils::tools::load_monitor g_lm;
    return g_lm;
}

SkaleServerOverride& SkaleServerOverride::getSSO() {  // abstract in SkaleStatsSubscriptionManager
    return ( *this );
}

nlohmann::json SkaleServerOverride::provideSkaleStats() {  // abstract from
                                                           // dev::rpc::SkaleStatsProviderImpl
    nlohmann::json joStats = nlohmann::json::object();
    //
    joStats["blocks"] = generateBlocksStats();
    //
    nlohmann::json joExecutionPerformance = nlohmann::json::object();
    joExecutionPerformance["RPC"] =
        skutils::stats::time_tracker::queue::getQueueForSubsystem( "RPC" ).getAllStats();
    joStats["executionPerformance"] = joExecutionPerformance;
    joStats["protocols"]["http"]["listenerCount"] =
        serversProxygenHTTP4std_.size() + serversProxygenHTTP4nfo_.size() +
        serversProxygenHTTP6std_.size() + serversProxygenHTTP6nfo_.size();
    joStats["protocols"]["https"]["listenerCount"] =
        serversProxygenHTTPS4std_.size() + serversProxygenHTTPS4nfo_.size() +
        serversProxygenHTTPS6std_.size() + serversProxygenHTTPS6nfo_.size();
    joStats["protocols"]["wss"]["listenerCount"] = serversWSS4std_.size() + serversWSS4nfo_.size() +
                                                   serversWSS6std_.size() + serversWSS6nfo_.size();
    {  // block for subsystem stats using optimized locking only once
        stats::lock_type_stats lock( stats::g_mtx_stats );
        joStats["protocols"]["http"]["stats"] = stats::generate_subsystem_stats( "HTTP" );
        joStats["protocols"]["http"]["rpc"] = stats::generate_subsystem_stats( "RPC/HTTP" );
        joStats["protocols"]["https"]["stats"] = stats::generate_subsystem_stats( "HTTPS" );
        joStats["protocols"]["https"]["rpc"] = stats::generate_subsystem_stats( "RPC/HTTPS" );
        joStats["protocols"]["ws"]["listenerCount"] = serversWS4std_.size() +
                                                      serversWS4nfo_.size() +
                                                      serversWS6std_.size() + serversWS6nfo_.size();
        joStats["protocols"]["ws"]["stats"] = stats::generate_subsystem_stats( "WS" );
        joStats["protocols"]["ws"]["rpc"] = stats::generate_subsystem_stats( "RPC/WS" );
        joStats["protocols"]["wss"]["stats"] = stats::generate_subsystem_stats( "WSS" );
        joStats["protocols"]["wss"]["rpc"] = stats::generate_subsystem_stats( "RPC/WSS" );
        joStats["rpc"] = stats::generate_subsystem_stats( "RPC" );
    }  // block for subsystem stats using optimized locking only once
    //
    skutils::tools::load_monitor& lm = stat_get_load_monitor();
    double lfCpuLoad = lm.last_cpu_load();
    joStats["system"]["cpu_load"] = lfCpuLoad;
    joStats["system"]["disk_usage"] = lm.last_disk_load();
    double lfMemUsage = skutils::tools::mem_usage();
    joStats["system"]["mem_usage"] = lfMemUsage;
    joStats["unddos"] = unddos_.stats();
    return joStats;
}

bool SkaleServerOverride::handleInformationalRequest(
    const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    std::string strMethod = joRequest["method"].get< std::string >();
    informational_rpc_map_t::const_iterator itFind = g_informational_rpc_map.find( strMethod );
    if ( itFind == g_informational_rpc_map.end() ) {
        return false;
    }

    ( ( *this ).*( itFind->second ) )( joRequest, joResponse );
    return true;
}

const SkaleServerOverride::informational_rpc_map_t SkaleServerOverride::g_informational_rpc_map = {
    { "eth_getBalance", &SkaleServerOverride::informational_eth_getBalance },
};

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

void SkaleServerOverride::informational_eth_getBalance(
    const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    std::cout << ( cc::debug( "Got call to informational version of " ) +
                   cc::info( "eth_getBalance" ) + cc::debug( " JSON RPC API with request as " ) +
                   cc::j( joRequest ) + "\n" );
    auto pEthereum = ethereum();
    if ( !pEthereum )
        throw std::runtime_error( "internal error, no Ethereum interface found" );
    dev::eth::Client* pClient = dynamic_cast< dev::eth::Client* >( pEthereum );
    if ( !pClient )
        throw std::runtime_error( "internal error, no client interface found" );
    const nlohmann::json& joParams = joRequest["params"];
    if ( !joParams.is_array() )
        throw std::runtime_error( "\"params\" must be array for \"eth_getBalance\"" );
    size_t cntParams = joParams.size();
    if ( cntParams < 1 )
        throw std::runtime_error( "\"params\" must be non-empty array for \"eth_getBalance\"" );
    const nlohmann::json& joAddress = joParams[0];
    if ( !joAddress.is_string() )
        throw std::runtime_error( "\"params[0]\" must be address string for \"eth_getBalance\"" );
    std::string strAddress = joAddress.get< std::string >();


    try {
        skutils::tools::replace_all( strAddress, "0x", "" );
        skutils::tools::replace_all( strAddress, "0X", "" );
        strAddress = skutils::tools::trim_copy( strAddress );
        // keccak256( "balanceOf(address)" ) =
        // "0x70a08231b98ef4ca268c9cc3f6b4590e4bfec28280db06bb5d45e689f2a360be", so function
        // signature is "0x70a08231"
        std::string strCallData = "0x70a08231";
        strCallData += stat_encode_eth_call_data_chunck_address( strAddress );
        nlohmann::json joCallArgs = nlohmann::json::object();
        joCallArgs["data"] = strCallData;
        joCallArgs["to"] = opts_.strEthErc20Address_;
        std::string strCallArgs = joCallArgs.dump();
        Json::Value _jsonCallArgs;
        Json::Reader().parse( strCallArgs, _jsonCallArgs );

        // TODO: We ignore block number in order to be compatible with Metamask (SKALE-430).
        // Remove this temporary fix.
        string blockNumber = "latest";


#ifdef HISTORIC_STATE
        if ( cntParams > 1 ) {
            blockNumber = joParams[1];
        };
        auto bNumber = dev::eth::jsToBlockNumber( blockNumber );
#endif

        dev::eth::TransactionSkeleton t = dev::eth::toTransactionSkeleton( _jsonCallArgs );
        // setTransactionDefaults( t );
        dev::eth::ExecutionResult er =
            pClient->call( t.from, t.value, t.to, t.data, t.gas, t.gasPrice,
#ifdef HISTORIC_STATE
                bNumber,
#endif
                dev::eth::FudgeFactor::Lenient );

        std::string strRevertReason;
        if ( er.excepted == dev::eth::TransactionException::RevertInstruction ) {
            strRevertReason = skutils::eth::call_error_message_2_str( er.output );
            if ( strRevertReason.empty() )
                strRevertReason = "EVM revert instruction without description message";
            Json::FastWriter fastWriter;
            std::string strJSON = fastWriter.write( _jsonCallArgs );
            std::string strOut = cc::fatal( "Error message from eth_call():" ) + cc::error( " " ) +
                                 cc::warn( strRevertReason ) +
                                 cc::error( ", with call arguments: " ) + cc::j( strJSON ) +
                                 cc::error( ", and using " ) + cc::info( "blockNumber" ) +
                                 cc::error( "=" ) + cc::bright( blockNumber );
            cerror << strOut;
            cerror << DETAILED_ERROR;
            throw std::runtime_error( strRevertReason );
        }

        std::string strBallance = er.output.empty() ? "0x0" : dev::toJS( er.output );
        joResponse["result"] = strBallance;
    } catch ( const std::exception& ex ) {
        const char* strError = ex.what();
        if ( strError == nullptr || strError[0] == '\0' )
            strError = "Error without description in informational version of \"eth_getBalance\"";
        std::cout << ( cc::fatal( "ERROR:" ) +
                       cc::error( " Got error in informational version of " ) +
                       cc::info( "eth_getBalance" ) + cc::debug( " with description: " ) +
                       cc::error( strError ) + "\n" );
        throw ex;
    } catch ( ... ) {
        const char* strError = "Unknown error in informational version of \"eth_getBalance\"";
        std::cout << ( cc::fatal( "ERROR:" ) +
                       cc::error( " Got error in informational version of " ) +
                       cc::info( "eth_getBalance" ) + cc::debug( " with description: " ) +
                       cc::error( strError ) + "\n" );
        throw std::runtime_error( strError );
    }
}

bool SkaleServerOverride::handleRequestWithBinaryAnswer(
    e_server_mode_t /*esm*/, const nlohmann::json& joRequest, std::vector< uint8_t >& buffer ) {
    buffer.clear();
    std::string strMethodName = skutils::tools::getFieldSafe< std::string >( joRequest, "method" );
    if ( strMethodName == "skale_downloadSnapshotFragment" && opts_.fn_binary_snapshot_download_ ) {
        const nlohmann::json& joParams = joRequest["params"];
        if ( joParams.count( "isBinary" ) > 0 ) {
            bool isBinary = joParams["isBinary"].get< bool >();
            if ( isBinary ) {
                buffer = opts_.fn_binary_snapshot_download_( joParams );
                return true;
            }
        }
    }
    return false;
}

bool SkaleServerOverride::handleAdminOriginFilter(
    const std::string& strMethod, const std::string& strOriginURL ) {
    static const std::set< std::string > g_setAdminMethods = { "skale_getSnapshot",
        "skale_downloadSnapshotFragment" };
    if ( g_setAdminMethods.find( strMethod ) == g_setAdminMethods.end() )
        return true;  // not an admin methhod
    std::string origin = strOriginURL;
    try {
        skutils::url u( strOriginURL.c_str() );
        origin = u.host();
    } catch ( ... ) {
    }
    if ( !checkAdminOriginAllowed( origin ) )
        return false;
    return true;
}


bool SkaleServerOverride::handleProtocolSpecificRequest( const std::string& strOrigin,
    const rapidjson::Document& joRequest, rapidjson::Document& joResponse ) {
    std::string strMethod = joRequest["method"].GetString();
    protocol_rpc_map_t::const_iterator itFind = g_protocol_rpc_map.find( strMethod );
    if ( itFind == g_protocol_rpc_map.end() )
        return false;
    ( ( *this ).*( itFind->second ) )( strOrigin, joRequest, joResponse );
    return true;
}

const SkaleServerOverride::protocol_rpc_map_t SkaleServerOverride::g_protocol_rpc_map = {
    { "setSchainExitTime", &SkaleServerOverride::setSchainExitTime },
    { "eth_sendRawTransaction", &SkaleServerOverride::eth_sendRawTransaction },
    { "eth_getTransactionReceipt", &SkaleServerOverride::eth_getTransactionReceipt },
    { "eth_call", &SkaleServerOverride::eth_call },
    { "eth_getBalance", &SkaleServerOverride::eth_getBalance },
    { "eth_getStorageAt", &SkaleServerOverride::eth_getStorageAt },
    { "eth_getTransactionCount", &SkaleServerOverride::eth_getTransactionCount },
    { "eth_getCode", &SkaleServerOverride::eth_getCode }
};


void SkaleServerOverride::setSchainExitTime( const std::string& strOrigin,
    const rapidjson::Document& joRequest, rapidjson::Document& joResponse ) {
    SkaleServerOverride* pSO = this;
    try {
        if ( !joRequest.HasMember( "params" ) ) {
            rapidjson::Value joError;
            joError.SetObject();
            joError.AddMember( "code", -32602, joResponse.GetAllocator() );
            std::string errorMessage = std::string( "error in \"" ) + "setSchainExitTime" +
                                       "\" rpc method, json entry \"params\" must be object";
            rapidjson::Value v;
            v.SetString( errorMessage.c_str(), errorMessage.size(), joResponse.GetAllocator() );
            joError.AddMember( "message", v, joResponse.GetAllocator() );
            joResponse.AddMember( "error", joError, joResponse.GetAllocator() );
        } else {
            const rapidjson::Value& param = joRequest["params"];
            if ( !param.IsObject() ) {
                rapidjson::Value joError;
                joError.SetObject();
                joError.AddMember( "code", -32602, joResponse.GetAllocator() );
                std::string errorMessage = std::string( "error in \"" ) + "setSchainExitTime" +
                                           "\" rpc method, json entry \"params\" must be object";
                rapidjson::Value v;
                v.SetString( errorMessage.c_str(), errorMessage.size(), joResponse.GetAllocator() );
                joError.AddMember( "message", v, joResponse.GetAllocator() );
                joResponse.AddMember( "error", joError, joResponse.GetAllocator() );
            }
        }
        const rapidjson::Value& joParams = joRequest["params"];
        // parse value of "finishTime"
        size_t finishTime = 0;
        if ( joParams.HasMember( "finishTime" ) > 0 ) {
            const rapidjson::Value& joValue = joParams["finishTime"];
            if ( joValue.IsNumber() )
                finishTime = joValue.GetUint();
            else
                throw std::runtime_error(
                    "invalid value in the \"finishTime\" parameter, need number" );
        } else {
            // no "finishTime" present in "params"
            throw std::runtime_error( "missing the \"finishTime\" parameter" );
        }

        // get connection info
        skutils::url u( strOrigin );
        std::string strIP = u.host();
        bool isLocalAddress = skutils::is_local_private_network_address(
            strIP );  // NOTICE: supports both IPv4 and IPv6
        // print info about this method call into log output
        clog( dev::VerbosityDebug, cc::warn( "ADMIN-CALL" ) )
            << ( cc::debug( "Got " ) + cc::info( "setSchainExitTime" ) +
                   cc::debug( " call with " ) + cc::notice( "finishTime" ) + cc::debug( "=" ) +
                   cc::size10( finishTime ) + cc::debug( ", " ) + cc::notice( "origin" ) +
                   cc::debug( "=" ) + cc::notice( strOrigin ) + cc::debug( ", " ) +
                   cc::notice( "remote IP" ) + cc::debug( "=" ) + cc::notice( strIP ) +
                   cc::debug( ", " ) + cc::notice( "isLocalAddress" ) + cc::debug( "=" ) +
                   cc::yn( isLocalAddress ) );
        // return call error if call from outside of local network
        if ( !isLocalAddress )
            throw std::runtime_error(
                "caller have no permition for this action" );  // NOTICE: just throw exception and
                                                               // RPC call will extract text from it
                                                               // and return it as call error
        //
        // TO-DO: optionally, put some data into joResponse which represents return value JSON
        //
        // TESTING: you can use wascat console
        // npm install -g
        // wscat -c 127.0.0.1:15020
        // {"jsonrpc":"2.0","id":12345,"method":"setSchainExitTime","params":{}}
        // {"jsonrpc":"2.0","id":12345,"method":"setSchainExitTime","params":{"finishTime":123}}
        //
        // or specify JSON right in command line...
        //
        // wscat -c ws://127.0.0.1:15020 -w 1 -x
        // '{"jsonrpc":"2.0","id":12345,"method":"setSchainExitTime","params":{"finishTime":123}}'

        // Result
        rapidjson::StringBuffer buffer;
        rapidjson::Writer< rapidjson::StringBuffer > writer( buffer );
        joResponse.Accept( writer );
        std::string strResponse = buffer.GetString();
        auto pEthereum = pSO->ethereum();
        if ( !pEthereum )
            throw std::runtime_error( "internal error, no Ethereum interface found" );
        dev::eth::Client* pClient = dynamic_cast< dev::eth::Client* >( pEthereum );
        if ( !pClient )
            throw std::runtime_error( "internal error, no client interface found" );
        pClient->setSchainExitTime( uint64_t( finishTime ) );
        joResponse.Parse( strResponse.data() );
    } catch ( const std::exception& ex ) {
        if ( pSO->opts_.isTraceCalls_ )
            clog( dev::Verbosity::VerbosityError,
                cc::debug( " during call from " ) + cc::u( strOrigin ) )
                << ( " " + cc::error( "error in " ) + cc::warn( "setSchainExitTime" ) +
                       cc::error( " rpc method, exception " ) + cc::warn( ex.what() ) );
        rapidjson::Value joError;
        joError.SetObject();
        joError.AddMember( "code", -32602, joResponse.GetAllocator() );
        std::string errorMessage =
            std::string( "error in \"setSchainExitTime\" rpc method, exception: " ) + ex.what();
        rapidjson::Value v;
        v.SetString( errorMessage.c_str(), errorMessage.size(), joResponse.GetAllocator() );
        joError.AddMember( "message", v, joResponse.GetAllocator() );
        joResponse.AddMember( "error", joError, joResponse.GetAllocator() );
    } catch ( ... ) {
        if ( pSO->opts_.isTraceCalls_ )
            clog( dev::Verbosity::VerbosityError,
                cc::debug( " during call from " ) + cc::u( strOrigin ) )
                << ( " " + cc::error( "error in " ) + cc::warn( "setSchainExitTime" ) +
                       cc::error( " rpc method, unknown exception " ) );
        rapidjson::Value joError;
        joError.SetObject();
        joError.AddMember( "code", -32602, joResponse.GetAllocator() );
        joError.AddMember( "message",
            "error in \"setSchainExitTime\" rpc method, unknown exception",
            joResponse.GetAllocator() );
        joResponse.AddMember( "error", joError, joResponse.GetAllocator() );
    }
}

void SkaleServerOverride::eth_sendRawTransaction( const std::string& /*strOrigin*/,
    const rapidjson::Document& joRequest, rapidjson::Document& joResponse ) {
    opts_.fn_eth_sendRawTransaction_( joRequest, joResponse );
}

void SkaleServerOverride::eth_getTransactionReceipt( const std::string& /*strOrigin*/,
    const rapidjson::Document& joRequest, rapidjson::Document& joResponse ) {
    opts_.fn_eth_getTransactionReceipt_( joRequest, joResponse );
}

void SkaleServerOverride::eth_call( const std::string& /*strOrigin*/,
    const rapidjson::Document& joRequest, rapidjson::Document& joResponse ) {
    opts_.fn_eth_call_( joRequest, joResponse );
}

void SkaleServerOverride::eth_getBalance( const std::string& /*strOrigin*/,
    const rapidjson::Document& joRequest, rapidjson::Document& joResponse ) {
    opts_.fn_eth_getBalance_( joRequest, joResponse );
}

void SkaleServerOverride::eth_getStorageAt( const std::string& /*strOrigin*/,
    const rapidjson::Document& joRequest, rapidjson::Document& joResponse ) {
    opts_.fn_eth_getStorageAt_( joRequest, joResponse );
}

void SkaleServerOverride::eth_getTransactionCount( const std::string& /*strOrigin*/,
    const rapidjson::Document& joRequest, rapidjson::Document& joResponse ) {
    opts_.fn_eth_getTransactionCount_( joRequest, joResponse );
}

void SkaleServerOverride::eth_getCode( const std::string& /*strOrigin*/,
    const rapidjson::Document& joRequest, rapidjson::Document& joResponse ) {
    opts_.fn_eth_getCode_( joRequest, joResponse );
}

bool SkaleServerOverride::handleHttpSpecificRequest( const std::string& strOrigin,
    e_server_mode_t esm, const std::string& strRequest, std::string& strResponse ) {
    strResponse.clear();
    rapidjson::Document joRequest;
    joRequest.SetObject();
    try {
        joRequest.Parse( strRequest.data() );
    } catch ( ... ) {
        return false;
    }
    rapidjson::Document joResponse;
    joResponse.SetObject();
    joResponse.AddMember( "jsonrpc", "2.0", joResponse.GetAllocator() );
    if ( joRequest.HasMember( "id" ) ) {
        joResponse.AddMember( "id", rapidjson::Value(), joResponse.GetAllocator() );
        joResponse["id"] = joRequest["id"];
    }
    rapidjson::Value d;
    d.SetObject();
    joResponse.AddMember( "result", d, joResponse.GetAllocator() );


    rapidjson::StringBuffer bufferResponse;
    rapidjson::Writer< rapidjson::StringBuffer > writerResponse( bufferResponse );
    joResponse.Accept( writerResponse );
    std::string strResponseCopy = bufferResponse.GetString();
    nlohmann::json joResponseObj = nlohmann::json::parse( strResponseCopy );
    nlohmann::json objRequest = nlohmann::json::parse( strRequest );
    if ( handleHttpSpecificRequest( strOrigin, esm, objRequest, joResponseObj ) ) {
        strResponse = joResponseObj.dump();
        return true;
    }
    if ( !handleProtocolSpecificRequest( strOrigin, joRequest, joResponse ) ) {
        return false;
    } else {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer< rapidjson::StringBuffer > writer( buffer );
        joResponse.Accept( writer );
        strResponse = buffer.GetString();
    }
    return true;
}

bool SkaleServerOverride::handleHttpSpecificRequest( const std::string& strOrigin,
    e_server_mode_t esm, const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( esm == e_server_mode_t::esm_informational &&
         handleInformationalRequest( joRequest, joResponse ) )
        return true;
    std::string strMethod = joRequest["method"].get< std::string >();
    http_rpc_map_t::const_iterator itFind = g_http_rpc_map.find( strMethod );
    if ( itFind == g_http_rpc_map.end() )
        return false;
    ( ( *this ).*( itFind->second ) )( strOrigin, esm, joRequest, joResponse );
    return true;
}

const SkaleServerOverride::http_rpc_map_t SkaleServerOverride::g_http_rpc_map = {};

std::string SkaleServerOverride::implPreformatTrafficJsonMessage(
    const std::string& strJSON, bool isRequest ) const {
    try {
        nlohmann::json jo = nlohmann::json::parse( strJSON );
        return implPreformatTrafficJsonMessage( jo, isRequest );
    } catch ( ... ) {
    }
    return cc::error( isRequest ? "bad JSON request" : "bad JSON response" ) + " " +
           cc::warn( strJSON );
}

std::string SkaleServerOverride::implPreformatTrafficJsonMessage(
    const nlohmann::json& jo, bool isRequest ) const {
    nlohmann::json jo2 = jo;
    SkaleServerOverride::stat_transformJsonForLogOutput( jo2, isRequest,
        SkaleServerOverride::g_nMaxStringValueLengthForJsonLogs,
        SkaleServerOverride::g_nMaxStringValueLengthForTransactionParams );
    return cc::j( jo2 );
}

size_t SkaleServerOverride::g_nMaxStringValueLengthForJsonLogs = 1024 * 32;
size_t SkaleServerOverride::g_nMaxStringValueLengthForTransactionParams = 64;

void SkaleServerOverride::stat_transformJsonForLogOutput( nlohmann::json& jo, bool isRequest,
    size_t nMaxStringValueLengthForJsonLogs, size_t nMaxStringValueLengthForTransactionParams,
    size_t nCallIndent ) {
    if ( ( nMaxStringValueLengthForJsonLogs == 0 ||
             nMaxStringValueLengthForJsonLogs == std::string::npos ) &&
         ( nMaxStringValueLengthForTransactionParams == 0 ||
             nMaxStringValueLengthForTransactionParams == std::string::npos ) )
        return;
    if ( jo.is_string() ) {
        std::string strValue = jo.get< std::string >();
        if ( strValue.size() > nMaxStringValueLengthForJsonLogs )
            jo = strValue.substr( 0, nMaxStringValueLengthForJsonLogs ) + "...";
        return;
    }
    if ( jo.is_array() ) {
        if ( nMaxStringValueLengthForJsonLogs == 0 ||
             nMaxStringValueLengthForJsonLogs == std::string::npos ) {
            ++nCallIndent;
            size_t cnt = jo.size();
            for ( size_t i = 0; i < cnt; ++i )
                stat_transformJsonForLogOutput( jo.at( i ), isRequest,
                    nMaxStringValueLengthForJsonLogs, nMaxStringValueLengthForTransactionParams,
                    nCallIndent );
        }
        return;
    }
    if ( !jo.is_object() )
        return;
    ++nCallIndent;
    bool bSkipParams = false;
    if ( ( !( nMaxStringValueLengthForTransactionParams == 0 ||
              nMaxStringValueLengthForTransactionParams == std::string::npos ) ) &&
         nCallIndent == 1 && isRequest && jo.count( "method" ) > 0 && jo["method"].is_string() &&
         jo["method"].get< std::string >() == "eth_sendRawTransaction" &&
         jo.count( "params" ) > 0 && jo["params"].is_array() ) {
        bSkipParams = true;
        nlohmann::json jarrNewParams = nlohmann::json::array();
        for ( auto it : jo["params"].items() ) {
            if ( it.value().is_string() ) {
                std::string strValue = it.value().get< std::string >();
                if ( strValue.size() > nMaxStringValueLengthForTransactionParams )
                    jarrNewParams.push_back(
                        strValue.substr( 0, nMaxStringValueLengthForTransactionParams ) + "..." );
                else
                    jarrNewParams.push_back( strValue );
            } else
                jarrNewParams.push_back( it.value() );
        }
        jo["params"] = jarrNewParams;
    }
    if ( nMaxStringValueLengthForJsonLogs == 0 ||
         nMaxStringValueLengthForJsonLogs == std::string::npos ) {
        for ( auto it : jo.items() ) {
            if ( bSkipParams && it.key() == "params" )
                continue;
            stat_transformJsonForLogOutput( it.value(), isRequest, nMaxStringValueLengthForJsonLogs,
                nMaxStringValueLengthForTransactionParams, nCallIndent );
        }
    }
}
