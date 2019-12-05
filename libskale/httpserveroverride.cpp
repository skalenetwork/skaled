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

#include <jsonrpccpp/common/exception.h>
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

#include <libethcore/CommonJS.h>

#if ( defined MSIZE )
#undef MSIZE
#endif

#include <libethereum/Block.h>
#include <libethereum/Transaction.h>
#include <libweb3jsonrpc/JsonHelper.h>
#include <libweb3jsonrpc/Skale.h>

#include <skutils/multithreading.h>
#include <skutils/url.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
        dv = dev::Verbosity::VerbosityInfo;
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
        filter.withEarliest( dev::jsToFixed< 32 >( jo["fromBlock"].get< std::string >() ) );
    if ( jo.count( "toBlock" ) > 0 )
        filter.withLatest( dev::jsToFixed< 32 >( jo["toBlock"].get< std::string >() ) );
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

dev::eth::LogFilter toLogFilter( const nlohmann::json& jo, dev::eth::Interface const& _client ) {
    dev::eth::LogFilter filter;
    if ( ( !jo.is_object() ) || jo.size() == 0 )
        return filter;
    // check only !empty. it should throw exceptions if input params are incorrect
    if ( jo.count( "fromBlock" ) > 0 )
        filter.withEarliest( _client.hashFromNumber(
            dev::eth::jsToBlockNumber( jo["fromBlock"].get< std::string >() ) ) );
    if ( jo.count( "toBlock" ) > 0 )
        filter.withLatest( _client.hashFromNumber(
            dev::eth::jsToBlockNumber( jo["toBlock"].get< std::string >() ) ) );
    if ( jo.count( "address" ) > 0 ) {
        if ( jo.count( "address" ) > 0 )
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

nlohmann::json nljsBlockNumber( dev::eth::BlockNumber bn ) {
    if ( bn == dev::eth::LatestBlock )
        return nlohmann::json( "latest" );
    if ( bn == 0 )
        return nlohmann::json( "earliest" );
    if ( bn == dev::eth::PendingBlock )
        return nlohmann::json( "pending" );
    return nlohmann::json( unsigned( bn ) );
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

};  // namespace helper
};  // namespace server
};  // namespace skale


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
static size_t g_nDefaultQueueSize = 10;

static skutils::stats::named_event_stats& stat_sybsystem_call_queue( const char* strSubSystem ) {
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
static skutils::stats::named_event_stats& stat_sybsystem_answer_queue( const char* strSubSystem ) {
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
static skutils::stats::named_event_stats& stat_sybsystem_error_queue( const char* strSubSystem ) {
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
static skutils::stats::named_event_stats& stat_sybsystem_exception_queue(
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

static skutils::stats::named_event_stats& stat_sybsystem_traffic_queue_in(
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
static skutils::stats::named_event_stats& stat_sybsystem_traffic_queue_out(
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
    skutils::stats::named_event_stats& cq = stat_sybsystem_call_queue( strSubSystem );
    cq.event_queue_add( strMethodName, g_nDefaultQueueSize );
    cq.event_add( strMethodName );
    skutils::stats::named_event_stats& tq = stat_sybsystem_traffic_queue_in( strSubSystem );
    tq.event_queue_add( strMethodName, g_nDefaultQueueSize );
    tq.event_add( strMethodName, nJsonSize );
}
void register_stats_answer(
    const char* strSubSystem, const char* strMethodName, const size_t nJsonSize ) {
    lock_type_stats lock( g_mtx_stats );
    skutils::stats::named_event_stats& aq = stat_sybsystem_answer_queue( strSubSystem );
    aq.event_queue_add( strMethodName, g_nDefaultQueueSize );
    aq.event_add( strMethodName );

    skutils::stats::named_event_stats& tq = stat_sybsystem_traffic_queue_out( strSubSystem );
    tq.event_queue_add( strMethodName, g_nDefaultQueueSize );
    tq.event_add( strMethodName, nJsonSize );
}
void register_stats_error( const char* strSubSystem, const char* strMethodName ) {
    lock_type_stats lock( g_mtx_stats );
    skutils::stats::named_event_stats& eq = stat_sybsystem_error_queue( strSubSystem );
    eq.event_queue_add( strMethodName, g_nDefaultQueueSize );
    eq.event_add( strMethodName );
}
void register_stats_exception( const char* strSubSystem, const char* strMethodName ) {
    lock_type_stats lock( g_mtx_stats );
    skutils::stats::named_event_stats& eq = stat_sybsystem_exception_queue( strSubSystem );
    eq.event_queue_add( strMethodName, g_nDefaultQueueSize );
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
    skutils::stats::named_event_stats& cq = stat_sybsystem_call_queue( strSubSystem );
    skutils::stats::named_event_stats& aq = stat_sybsystem_answer_queue( strSubSystem );
    skutils::stats::named_event_stats& erq = stat_sybsystem_error_queue( strSubSystem );
    skutils::stats::named_event_stats& exq = stat_sybsystem_exception_queue( strSubSystem );
    skutils::stats::named_event_stats& tq_in = stat_sybsystem_traffic_queue_in( strSubSystem );
    skutils::stats::named_event_stats& tq_out = stat_sybsystem_traffic_queue_out( strSubSystem );
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
        size_t nCalls = 0, nAnswers = 0, nErrors = 0, nExceptopns = 0;
        skutils::stats::bytes_count_t nBytesRecv = 0, nBytesSent = 0;
        skutils::stats::time_point tpNow = skutils::stats::clock::now();
        double lfCallsPerSecond = cq.compute_eps( strMethodName, tpNow, nullptr, &nCalls );
        double lfAnswersPerSecond = aq.compute_eps( strMethodName, tpNow, nullptr, &nAnswers );
        double lfErrorsPerSecond = erq.compute_eps( strMethodName, tpNow, nullptr, &nErrors );
        double lfExceptopnsPerSecond =
            exq.compute_eps( strMethodName, tpNow, nullptr, &nExceptopns );
        double lfBytesPerSecondRecv = tq_in.compute_eps( strMethodName, tpNow, &nBytesRecv );
        double lfBytesPerSecondSent = tq_out.compute_eps( strMethodName, tpNow, &nBytesSent );
        nlohmann::json joMethod = nlohmann::json::object();
        joMethod["cps"] = lfCallsPerSecond;
        joMethod["aps"] = lfAnswersPerSecond;
        joMethod["erps"] = lfErrorsPerSecond;
        joMethod["exps"] = lfExceptopnsPerSecond;
        joMethod["bps_recv"] = lfBytesPerSecondRecv;
        joMethod["bps_sent"] = lfBytesPerSecondSent;
        joMethod["calls"] = nCalls;
        joMethod["answers"] = nAnswers;
        joMethod["errors"] = nErrors;
        joMethod["exceptions"] = nExceptopns;
        joMethod["bytes_recv"] = nBytesRecv;
        joMethod["bytes_sent"] = nBytesSent;
        jo[strMethodName] = joMethod;
    }
    return jo;
}

};  // namespace stats

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
    subscriptionData.m_pPeer->ref_retain();  // mamual retain-release(subscription map)
    subscriptionData.m_pPeer->ref_retain();  // mamual retain-release(async job)
    skutils::dispatch::repeat( subscriptionData.m_pPeer->m_strPeerQueueID,
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
                if ( getSSO().m_bTraceCalls )
                    clog( dev::VerbosityInfo,
                        cc::info( subscriptionData.m_pPeer->getRelay().nfoGetSchemeUC() ) )
                        << ( cc::ws_tx_inv( " <<< " +
                                            subscriptionData.m_pPeer->getRelay().nfoGetSchemeUC() +
                                            "/TX <<< " ) +
                               subscriptionData.m_pPeer->desc() + cc::ws_tx( " <<< " ) +
                               cc::j( strNotification ) );
                skutils::dispatch::async( subscriptionData.m_pPeer->m_strPeerQueueID,
                    [subscriptionData, strNotification, idSubscription, this]() -> void {
                        bool bMessageSentOK = false;
                        try {
                            bMessageSentOK = subscriptionData.m_pPeer->sendMessage(
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
            subscriptionData.m_pPeer->ref_release();  // mamual retain-release(async job)
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
        if ( subscriptionData.m_pPeer )
            subscriptionData.m_pPeer->ref_release();  // mamual retain-release(subscription map)
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SkaleServerConnectionsTrackHelper::SkaleServerConnectionsTrackHelper( SkaleServerOverride& sso )
    : m_sso( sso ) {
    m_sso.connection_counter_inc();
}

SkaleServerConnectionsTrackHelper::~SkaleServerConnectionsTrackHelper() {
    m_sso.connection_counter_dec();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SkaleWsPeer::SkaleWsPeer( skutils::ws::server& srv, const skutils::ws::hdl_t& hdl )
    : skutils::ws::peer( srv, hdl ),
      m_strPeerQueueID( skutils::dispatch::generate_id( this, "relay_peer" ) ) {
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::VerbosityTrace, cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                                       cc::num10( getRelay().serverIndex() ) )
            << ( desc() + cc::notice( " peer ctor" ) );
}
SkaleWsPeer::~SkaleWsPeer() {
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::VerbosityTrace, cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                                       cc::num10( getRelay().serverIndex() ) )
            << ( desc() + cc::notice( " peer dctor" ) );
    uninstallAllWatches();
    skutils::dispatch::remove( m_strPeerQueueID );
}

void SkaleWsPeer::onPeerRegister() {
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::VerbosityInfo, cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                                      cc::num10( getRelay().serverIndex() ) )
            << ( desc() + cc::notice( " peer registered" ) );
    skutils::ws::peer::onPeerRegister();
}
void SkaleWsPeer::onPeerUnregister() {  // peer will no longer receive onMessage after call to
                                        // this
    m_pSSCTH.reset();
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::VerbosityInfo, cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                                      cc::num10( getRelay().serverIndex() ) )
            << ( desc() + cc::notice( " peer unregistered" ) );
    skutils::ws::peer::onPeerUnregister();
    uninstallAllWatches();
    skutils::dispatch::remove( m_strPeerQueueID );  // remove queue earlier
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
    SkaleWsPeer* pThis = this;
    pThis->ref_retain();  // mamual retain-release
    skutils::dispatch::async( pThis->m_strPeerQueueID, [pThis, msg, pSO]() -> void {
        std::string strRequest( msg );
        if ( pSO->m_bTraceCalls )
            clog( dev::VerbosityInfo, cc::info( pThis->getRelay().nfoGetSchemeUC() ) +
                                          cc::debug( "/" ) +
                                          cc::num10( pThis->getRelay().serverIndex() ) )
                << ( cc::ws_rx_inv( " >>> " + pThis->getRelay().nfoGetSchemeUC() + "/" +
                                    std::to_string( pThis->getRelay().serverIndex() ) +
                                    "/RX >>> " ) +
                       pThis->desc() + cc::ws_rx( " >>> " ) + cc::j( strRequest ) );
        nlohmann::json joID = "-1";
        std::string strResponse, strMethod;
        bool bPassed = false;
        try {
            nlohmann::json joRequest;
            joRequest = nlohmann::json::parse( strRequest );
            strMethod = skutils::tools::getFieldSafe< std::string >( joRequest, "method" );
            stats::register_stats_message(
                pThis->getRelay().nfoGetSchemeUC().c_str(), "messages", strRequest.size() );
            stats::register_stats_message(
                ( std::string( "RPC/" ) + pThis->getRelay().nfoGetSchemeUC() ).c_str(), joRequest );
            stats::register_stats_message( "RPC", joRequest );
            if ( !( const_cast< SkaleWsPeer* >( pThis ) )
                      ->handleWebSocketSpecificRequest( joRequest, strResponse ) ) {
                joID = joRequest["id"];
                jsonrpc::IClientConnectionHandler* handler = pSO->GetHandler( "/" );
                if ( handler == nullptr )
                    throw std::runtime_error( "No client connection handler found" );
                handler->HandleRequest( strRequest, strResponse );
            }
            nlohmann::json joResponse = nlohmann::json::parse( strResponse );
            stats::register_stats_answer(
                pThis->getRelay().nfoGetSchemeUC().c_str(), "messages", strResponse.size() );
            stats::register_stats_answer(
                ( std::string( "RPC/" ) + pThis->getRelay().nfoGetSchemeUC() ).c_str(), joRequest,
                joResponse );
            stats::register_stats_answer( "RPC", joRequest, joResponse );
            bPassed = true;
        } catch ( const std::exception& ex ) {
            clog( dev::VerbosityError, cc::info( pThis->getRelay().nfoGetSchemeUC() ) +
                                           cc::debug( "/" ) +
                                           cc::num10( pThis->getRelay().serverIndex() ) )
                << ( cc::ws_tx_inv( " !!! " + pThis->getRelay().nfoGetSchemeUC() + "/" +
                                    std::to_string( pThis->getRelay().serverIndex() ) +
                                    "/ERR !!! " ) +
                       pThis->desc() + cc::ws_tx( " !!! " ) + cc::warn( ex.what() ) );
            nlohmann::json joErrorResponce;
            joErrorResponce["id"] = joID;
            joErrorResponce["result"] = "error";
            joErrorResponce["error"] = std::string( ex.what() );
            strResponse = joErrorResponce.dump();
            stats::register_stats_exception(
                ( std::string( "RPC/" ) + pThis->getRelay().nfoGetSchemeUC() ).c_str(), "" );
            if ( !strMethod.empty() ) {
                stats::register_stats_exception(
                    pThis->getRelay().nfoGetSchemeUC().c_str(), "messages" );
                stats::register_stats_exception( "RPC", strMethod.c_str() );
            }
        } catch ( ... ) {
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
            joErrorResponce["result"] = "error";
            joErrorResponce["error"] = std::string( e );
            strResponse = joErrorResponce.dump();
            stats::register_stats_exception(
                ( std::string( "RPC/" ) + pThis->getRelay().nfoGetSchemeUC() ).c_str(),
                "messages" );
            if ( !strMethod.empty() ) {
                stats::register_stats_exception(
                    pThis->getRelay().nfoGetSchemeUC().c_str(), "messages" );
                stats::register_stats_exception( "RPC", strMethod.c_str() );
            }
        }
        if ( pSO->m_bTraceCalls )
            clog( dev::VerbosityInfo, cc::info( pThis->getRelay().nfoGetSchemeUC() ) +
                                          cc::debug( "/" ) +
                                          cc::num10( pThis->getRelay().serverIndex() ) )
                << ( cc::ws_tx_inv( " <<< " + pThis->getRelay().nfoGetSchemeUC() + "/" +
                                    std::to_string( pThis->getRelay().serverIndex() ) +
                                    "/TX <<< " ) +
                       pThis->desc() + cc::ws_tx( " <<< " ) + cc::j( strResponse ) );
        ( const_cast< SkaleWsPeer* >( pThis ) )
            ->sendMessage( skutils::tools::trim_copy( strResponse ) );
        if ( !bPassed )
            stats::register_stats_answer(
                pThis->getRelay().nfoGetSchemeUC().c_str(), "messages", strResponse.size() );
        pThis->ref_release();  // mamual retain-release
    } );
    // skutils::ws::peer::onMessage( msg, eOpCode );
}

void SkaleWsPeer::onClose(
    const std::string& reason, int local_close_code, const std::string& local_close_code_as_str ) {
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::VerbosityInfo, cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                                      cc::num10( getRelay().serverIndex() ) )
            << ( desc() + cc::warn( " peer close event with code=" ) + cc::c( local_close_code ) +
                   cc::debug( ", reason=" ) + cc::info( reason ) );
    skutils::ws::peer::onClose( reason, local_close_code, local_close_code_as_str );
    uninstallAllWatches();
}

void SkaleWsPeer::onFail() {
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::VerbosityError, cc::fatal( getRelay().nfoGetSchemeUC() ) )
            << ( desc() + cc::error( " peer fail event" ) );
    skutils::ws::peer::onFail();
    uninstallAllWatches();
}

void SkaleWsPeer::onLogMessage(
    skutils::ws::e_ws_log_message_type_t eWSLMT, const std::string& msg ) {
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
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

bool SkaleWsPeer::handleRequestWithBinaryAnswer( const nlohmann::json& joRequest ) {
    SkaleServerOverride* pSO = pso();
    std::vector< uint8_t > buffer;
    if ( pSO->handleRequestWithBinaryAnswer( joRequest, buffer ) ) {
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
    const nlohmann::json& joRequest, std::string& strResponse ) {
    strResponse.clear();
    nlohmann::json joResponse = nlohmann::json::object();
    joResponse["jsonrpc"] = "2.0";
    if ( joRequest.count( "id" ) > 0 )
        joResponse["id"] = joRequest["id"];
    joResponse["result"] = nullptr;
    if ( !handleWebSocketSpecificRequest( joRequest, joResponse ) )
        return false;
    strResponse = joResponse.dump();
    return true;
}

bool SkaleWsPeer::handleWebSocketSpecificRequest(
    const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    std::string strMethod = joRequest["method"].get< std::string >();
    rpc_map_t::const_iterator itFind = g_rpc_map.find( strMethod );
    if ( itFind == g_rpc_map.end() )
        return false;
    ( ( *this ).*( itFind->second ) )( joRequest, joResponse );
    return true;
}

const SkaleWsPeer::rpc_map_t SkaleWsPeer::g_rpc_map = {
    {"eth_subscribe", &SkaleWsPeer::eth_subscribe},
    {"eth_unsubscribe", &SkaleWsPeer::eth_unsubscribe},
};

bool SkaleWsPeer::checkParamsPresent(
    const char* strMethodName, const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( joRequest.count( "params" ) > 0 )
        return true;
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::Verbosity::VerbosityError, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                  cc::debug( "/" ) +
                                                  cc::num10( getRelay().serverIndex() ) )
            << ( desc() + " " + cc::error( "error in " ) + cc::warn( strMethodName ) +
                   cc::error( " rpc method, json entry " ) + cc::warn( "params" ) +
                   cc::error( " is missing" ) );
    nlohmann::json joError = nlohmann::json::object();
    joError["code"] = -32602;
    joError["message"] = std::string( "error in \"" ) + strMethodName +
                         "\" rpc method, json entry \"params\" is missing";
    joResponse["error"] = joError;
    return false;
}

bool SkaleWsPeer::checkParamsIsArray(
    const char* strMethodName, const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( !checkParamsPresent( strMethodName, joRequest, joResponse ) )
        return false;
    const nlohmann::json& jarrParams = joRequest["params"];
    if ( jarrParams.is_array() )
        return true;
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::Verbosity::VerbosityError, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                  cc::debug( "/" ) +
                                                  cc::num10( getRelay().serverIndex() ) )
            << ( desc() + " " + cc::error( "error in " ) + cc::warn( strMethodName ) +
                   cc::error( " rpc method, json entry " ) + cc::warn( "params" ) +
                   cc::error( " must be array" ) );
    nlohmann::json joError = nlohmann::json::object();
    joError["code"] = -32602;
    joError["message"] = std::string( "error in \"" ) + strMethodName +
                         "\" rpc method, json entry \"params\" must be array";
    joResponse["error"] = joError;
    return false;
}

void SkaleWsPeer::eth_subscribe( const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( !checkParamsIsArray( "eth_subscribe", joRequest, joResponse ) )
        return;
    const nlohmann::json& jarrParams = joRequest["params"];
    std::string strSubcscriptionType;
    size_t idxParam, cntParams = jarrParams.size();
    for ( idxParam = 0; idxParam < cntParams; ++idxParam ) {
        const nlohmann::json& joParamItem = jarrParams[idxParam];
        if ( !joParamItem.is_string() )
            continue;
        strSubcscriptionType = skutils::tools::trim_copy( joParamItem.get< std::string >() );
        break;
    }
    if ( strSubcscriptionType == "logs" ) {
        eth_subscribe_logs( joRequest, joResponse );
        return;
    }
    if ( strSubcscriptionType == "newPendingTransactions" ||
         strSubcscriptionType == "pendingTransactions" ) {
        eth_subscribe_newPendingTransactions( joRequest, joResponse );
        return;
    }
    if ( strSubcscriptionType == "newHeads" || strSubcscriptionType == "newBlockHeaders" ) {
        eth_subscribe_newHeads( joRequest, joResponse, false );
        return;
    }
    if ( strSubcscriptionType == "skaleStats" ) {
        eth_subscribe_skaleStats( joRequest, joResponse );
        return;
    }
    if ( strSubcscriptionType.empty() )
        strSubcscriptionType = "<empty>";
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::Verbosity::VerbosityError, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                  cc::debug( "/" ) +
                                                  cc::num10( getRelay().serverIndex() ) )
            << ( desc() + " " + cc::error( "error in " ) + cc::warn( "eth_subscribe" ) +
                   cc::error( " rpc method, missing valid subscription type in parameters, was "
                              "specifiedL " ) +
                   cc::warn( strSubcscriptionType ) );
    nlohmann::json joError = nlohmann::json::object();
    joError["code"] = -32603;
    joError["message"] =
        "error in \"eth_subscribe\" rpc method, missing valid subscription type in parameters, was "
        "specified: " +
        strSubcscriptionType;
    joResponse["error"] = joError;
}

void SkaleWsPeer::eth_subscribe_logs(
    const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
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
                    logFilter = skale::server::helper::toLogFilter( joParamItem, *ethereum() );
                }
            }
        }  // for ( idxParam = 0; idxParam < cntParams; ++idxParam )
        skutils::retain_release_ptr< SkaleWsPeer > pThis( this );
        dev::eth::fnClientWatchHandlerMulti_t fnOnSunscriptionEvent;
        fnOnSunscriptionEvent += [pThis]( unsigned iw ) -> void {
            skutils::dispatch::async( "logs-rethread", [=]() -> void {
                skutils::dispatch::async( pThis->m_strPeerQueueID, [pThis, iw]() -> void {
                    dev::eth::LocalisedLogEntries le = pThis->ethereum()->logs( iw );
                    nlohmann::json joResult = skale::server::helper::toJsonByBlock( le );
                    if ( joResult.is_array() ) {
                        for ( const auto& joRW : joResult ) {
                            if ( joRW.is_object() && joRW.count( "logs" ) > 0 &&
                                 joRW.count( "blockHash" ) > 0 &&
                                 joRW.count( "blockNumber" ) > 0 ) {
                                std::string strBlockHash = joRW["blockHash"].get< std::string >();
                                unsigned nBlockBumber = joRW["blockNumber"].get< unsigned >();
                                const nlohmann::json& joResultLogs = joRW["logs"];
                                if ( joResultLogs.is_array() ) {
                                    for ( const auto& joWalk : joResultLogs ) {
                                        if ( !joWalk.is_object() )
                                            continue;
                                        nlohmann::json joLog = joWalk;  // copy
                                        joLog["blockHash"] = strBlockHash;
                                        joLog["blockNumber"] = nBlockBumber;
                                        nlohmann::json joParams = nlohmann::json::object();
                                        joParams["subscription"] = dev::toJS( iw );
                                        joParams["result"] = joLog;
                                        nlohmann::json joNotification = nlohmann::json::object();
                                        joNotification["jsonrpc"] = "2.0";
                                        joNotification["method"] = "eth_subscription";
                                        joNotification["params"] = joParams;
                                        std::string strNotification = joNotification.dump();
                                        const SkaleServerOverride* pSO = pThis->pso();
                                        if ( pSO->m_bTraceCalls )
                                            clog( dev::VerbosityInfo,
                                                cc::info( pThis->getRelay().nfoGetSchemeUC() ) +
                                                    cc::ws_tx_inv(
                                                        " <<< " +
                                                        pThis->getRelay().nfoGetSchemeUC() +
                                                        "/TX <<< " ) )
                                                << ( pThis->desc() + cc::ws_tx( " <<< " ) +
                                                       cc::j( strNotification ) );
                                        // skutils::dispatch::async( pThis->m_strPeerQueueID,
                                        // [pThis, strNotification]() -> void {
                                        bool bMessageSentOK = false;
                                        try {
                                            bMessageSentOK =
                                                const_cast< SkaleWsPeer* >( pThis.get() )
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
                                            stats::register_stats_answer( "RPC",
                                                "eth_subscription/logs", strNotification.size() );
                                        } catch ( std::exception& ex ) {
                                            clog( dev::Verbosity::VerbosityError,
                                                cc::info( pThis->getRelay().nfoGetSchemeUC() ) +
                                                    cc::debug( "/" ) +
                                                    cc::num10( pThis->getRelay().serverIndex() ) )
                                                << ( pThis->desc() + " " +
                                                       cc::error( "error in " ) +
                                                       cc::warn( "eth_subscription/logs" ) +
                                                       cc::error(
                                                           " will uninstall watcher callback "
                                                           "because of exception: " ) +
                                                       cc::warn( ex.what() ) );
                                        } catch ( ... ) {
                                            clog( dev::Verbosity::VerbosityError,
                                                cc::info( pThis->getRelay().nfoGetSchemeUC() ) +
                                                    cc::debug( "/" ) +
                                                    cc::num10( pThis->getRelay().serverIndex() ) )
                                                << ( pThis->desc() + " " +
                                                       cc::error( "error in " ) +
                                                       cc::warn( "eth_subscription/logs" ) +
                                                       cc::error(
                                                           " will uninstall watcher callback "
                                                           "because of unknown exception" ) );
                                        }
                                        if ( !bMessageSentOK ) {
                                            stats::register_stats_error(
                                                ( std::string( "RPC/" ) +
                                                    pThis->getRelay().nfoGetSchemeUC() )
                                                    .c_str(),
                                                "eth_subscription/logs" );
                                            stats::register_stats_error(
                                                "RPC", "eth_subscription/logs" );
                                            pThis->ethereum()->uninstallWatch( iw );
                                        }
                                        //    } );
                                    }  // for ( const auto& joWalk : joResultLogs )
                                }      // if ( joResultLogs.is_array() )
                            }
                        }  // for ( const auto& joRW : joResult )
                    }      // if ( joResult.is_array() )
                } );
            } );
        };
        unsigned iw = ethereum()->installWatch(
            logFilter, dev::eth::Reaping::Automatic, fnOnSunscriptionEvent, true );  // isWS = true
        setInstalledWatchesLogs_.insert( iw );
        std::string strIW = dev::toJS( iw );
        if ( pSO->m_bTraceCalls )
            clog( dev::Verbosity::VerbosityTrace, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << ( desc() + " " + cc::info( "eth_subscribe/logs" ) +
                       cc::debug( " rpc method did installed watch " ) + cc::info( strIW ) );
        joResponse["result"] = strIW;
    } catch ( const std::exception& ex ) {
        if ( pSO->m_bTraceCalls )
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
        if ( pSO->m_bTraceCalls )
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
    const nlohmann::json& /*joRequest*/, nlohmann::json& joResponse ) {
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
                if ( pSO->m_bTraceCalls )
                    clog( dev::VerbosityInfo, cc::info( pThis->getRelay().nfoGetSchemeUC() ) )
                        << ( cc::ws_tx_inv(
                                 " <<< " + pThis->getRelay().nfoGetSchemeUC() + "/TX <<< " ) +
                               pThis->desc() + cc::ws_tx( " <<< " ) + cc::j( strNotification ) );
                // skutils::dispatch::async( pThis->m_strPeerQueueID, [pThis, strNotification]() ->
                // void {
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
        if ( pSO->m_bTraceCalls )
            clog( dev::Verbosity::VerbosityTrace, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << ( desc() + " " + cc::info( "eth_subscribe/newPendingTransactions" ) +
                       cc::debug( " rpc method did installed watch " ) + cc::info( strIW ) );
        joResponse["result"] = strIW;
    } catch ( const std::exception& ex ) {
        if ( pSO->m_bTraceCalls )
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
        if ( pSO->m_bTraceCalls )
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

void SkaleWsPeer::eth_subscribe_newHeads(
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
                if ( pSO->m_bTraceCalls )
                    clog( dev::VerbosityInfo, cc::info( pThis->getRelay().nfoGetSchemeUC() ) )
                        << ( cc::ws_tx_inv(
                                 " <<< " + pThis->getRelay().nfoGetSchemeUC() + "/TX <<< " ) +
                               pThis->desc() + cc::ws_tx( " <<< " ) + cc::j( strNotification ) );
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
        if ( pSO->m_bTraceCalls )
            clog( dev::Verbosity::VerbosityTrace, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << ( desc() + " " + cc::info( "eth_subscribe/newHeads" ) +
                       cc::debug( " rpc method did installed watch " ) + cc::info( strIW ) );
        joResponse["result"] = strIW;
    } catch ( const std::exception& ex ) {
        if ( pSO->m_bTraceCalls )
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
        if ( pSO->m_bTraceCalls )
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
    const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
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
        if ( pSO->m_bTraceCalls )
            clog( dev::Verbosity::VerbosityTrace, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << ( desc() + " " + cc::info( "eth_subscribe/skaleStats" ) +
                       cc::debug( " rpc method did installed watch " ) + cc::info( strIW ) );
        joResponse["result"] = strIW;
    } catch ( const std::exception& ex ) {
        if ( pSO->m_bTraceCalls )
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
        if ( pSO->m_bTraceCalls )
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

void SkaleWsPeer::eth_unsubscribe( const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( !checkParamsIsArray( "eth_unsubscribe", joRequest, joResponse ) )
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
            if ( pSO->m_bTraceCalls )
                clog( dev::Verbosity::VerbosityError, cc::info( getRelay().nfoGetSchemeUC() ) +
                                                          cc::debug( "/" ) +
                                                          cc::num10( getRelay().serverIndex() ) )
                    << ( desc() + " " + cc::error( "error in " ) + cc::warn( "eth_unsubscribe" ) +
                           cc::error( " rpc method, bad subsription ID " ) + cc::j( joParamItem ) );
            nlohmann::json joError = nlohmann::json::object();
            joError["code"] = -32602;
            joError["message"] =
                "error in \"eth_unsubscribe\" rpc method, ad subsription ID " + joParamItem.dump();
            joResponse["error"] = joError;
            return;
        }
        unsigned x = ( iw & SKALED_WS_SUBSCRIPTION_TYPE_MASK );
        if ( x == SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION ) {
            if ( setInstalledWatchesNewPendingTransactions_.find(
                     iw & ( ~( SKALED_WS_SUBSCRIPTION_TYPE_MASK ) ) ) ==
                 setInstalledWatchesNewPendingTransactions_.end() ) {
                std::string strIW = dev::toJS( iw );
                if ( pSO->m_bTraceCalls )
                    clog( dev::Verbosity::VerbosityError,
                        cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                            cc::num10( getRelay().serverIndex() ) )
                        << ( desc() + " " + cc::error( "error in " ) +
                               cc::warn( "eth_unsubscribe/newPendingTransactionWatch" ) +
                               cc::error( " rpc method, bad subsription ID " ) +
                               cc::warn( strIW ) );
                nlohmann::json joError = nlohmann::json::object();
                joError["code"] = -32602;
                joError["message"] =
                    "error in \"eth_unsubscribe/newPendingTransactionWatch\" rpc method, ad "
                    "subsription ID " +
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
                if ( pSO->m_bTraceCalls )
                    clog( dev::Verbosity::VerbosityError,
                        cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                            cc::num10( getRelay().serverIndex() ) )
                        << ( desc() + " " + cc::error( "error in " ) +
                               cc::warn( "eth_unsubscribe/newHeads" ) +
                               cc::error( " rpc method, bad subsription ID " ) +
                               cc::warn( strIW ) );
                nlohmann::json joError = nlohmann::json::object();
                joError["code"] = -32602;
                joError["message"] =
                    "error in \"eth_unsubscribe/newHeads\" rpc method, ad subsription ID " + strIW;
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
                if ( pSO->m_bTraceCalls )
                    clog( dev::Verbosity::VerbosityError,
                        cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                            cc::num10( getRelay().serverIndex() ) )
                        << ( desc() + " " + cc::error( "error in " ) +
                               cc::warn( "eth_unsubscribe/newHeads" ) +
                               cc::error( " rpc method, bad subsription ID " ) +
                               cc::warn( strIW ) );
                nlohmann::json joError = nlohmann::json::object();
                joError["code"] = -32602;
                joError["message"] =
                    "error in \"eth_unsubscribe/skaleStats\" rpc method, ad subsription ID " +
                    strIW;
                joResponse["error"] = joError;
                return;
            }  // if ( !bWasUnsubscribed )
        } else {
            if ( setInstalledWatchesLogs_.find( iw ) == setInstalledWatchesLogs_.end() ) {
                std::string strIW = dev::toJS( iw );
                if ( pSO->m_bTraceCalls )
                    clog( dev::Verbosity::VerbosityError,
                        cc::info( getRelay().nfoGetSchemeUC() ) + cc::debug( "/" ) +
                            cc::num10( getRelay().serverIndex() ) )
                        << ( desc() + " " + cc::error( "error in " ) +
                               cc::warn( "eth_unsubscribe/logs" ) +
                               cc::error( " rpc method, bad subsription ID " ) +
                               cc::warn( strIW ) );
                nlohmann::json joError = nlohmann::json::object();
                joError["code"] = -32602;
                joError["message"] =
                    "error in \"eth_unsubscribe/logs\" rpc method, ad subsription ID " + strIW;
                joResponse["error"] = joError;
                return;
            }
            ethereum()->uninstallWatch( iw );
            setInstalledWatchesLogs_.erase( iw );
        }
    }  // for ( idxParam = 0; idxParam < cntParams; ++idxParam )
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SkaleRelayWS::SkaleRelayWS( int ipVer, const char* strBindAddr,
    const char* strScheme,  // "ws" or "wss"
    int nPort, int nServerIndex )
    : SkaleServerHelper( nServerIndex ),
      ipVer_( ipVer ),
      strBindAddr_( strBindAddr ),
      m_strScheme_( skutils::tools::to_lower( strScheme ) ),
      m_strSchemeUC( skutils::tools::to_upper( strScheme ) ),
      m_nPort( nPort ) {
    onPeerInstantiate_ = [&]( skutils::ws::server& srv,
                             skutils::ws::hdl_t hdl ) -> skutils::ws::peer_ptr_t {
        SkaleWsPeer* pSkalePeer = nullptr;
        SkaleServerOverride* pSO = pso();
        if ( pSO->m_bTraceCalls )
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
                ipVer, m_strSchemeUC.c_str(), serverIndex(), nPort );
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
    clog( dev::VerbosityInfo, cc::info( m_strSchemeUC ) )
        << ( cc::notice( "Will start server on port " ) + cc::c( m_nPort ) );
    if ( !open( m_strScheme_, m_nPort ) ) {
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
    } )
        .detach();
    clog( dev::VerbosityInfo, cc::info( m_strSchemeUC ) )
        << ( cc::success( "OK, server started on port " ) + cc::c( m_nPort ) );
    return true;
}
void SkaleRelayWS::stop() {
    if ( !isRunning() )
        return;
    clog( dev::VerbosityInfo, cc::info( m_strSchemeUC ) )
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SkaleRelayHTTP::SkaleRelayHTTP( int ipVer, const char* strBindAddr, int nPort,
    const char* cert_path, const char* private_key_path, int nServerIndex,
    size_t a_max_http_handler_queues )
    : SkaleServerHelper( nServerIndex ),
      ipVer_( ipVer ),
      strBindAddr_( strBindAddr ),
      nPort_( nPort ),
      m_bHelperIsSSL( ( cert_path && cert_path[0] && private_key_path && private_key_path[0] ) ?
                          true :
                          false ) {
    if ( m_bHelperIsSSL )
        m_pServer.reset( new skutils::http::SSL_server( cert_path, private_key_path ) );
    else
        m_pServer.reset( new skutils::http::server );
    m_pServer->ipVer_ = ipVer_;  // not known before listen
}

SkaleRelayHTTP::~SkaleRelayHTTP() {
    m_pServer.reset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SkaleServerOverride::SkaleServerOverride( dev::eth::ChainParams& chainParams,
    fn_binary_snapshot_download_t fn_binary_snapshot_download, size_t cntServers,
    dev::eth::Interface* pEth, const std::string& strAddrHTTP4, int nBasePortHTTP4,
    const std::string& strAddrHTTP6, int nBasePortHTTP6, const std::string& strAddrHTTPS4,
    int nBasePortHTTPS4, const std::string& strAddrHTTPS6, int nBasePortHTTPS6,
    const std::string& strAddrWS4, int nBasePortWS4, const std::string& strAddrWS6,
    int nBasePortWS6, const std::string& strAddrWSS4, int nBasePortWSS4,
    const std::string& strAddrWSS6, int nBasePortWSS6, const std::string& strPathSslKey,
    const std::string& strPathSslCert )
    : AbstractServerConnector(),
      m_cntServers( ( cntServers < 1 ) ? 1 : cntServers ),
      pEth_( pEth ),
      chainParams_( chainParams ),
      fn_binary_snapshot_download_( fn_binary_snapshot_download ),
      m_strAddrHTTP4( strAddrHTTP4 ),
      m_nBasePortHTTP4( nBasePortHTTP4 ),
      m_strAddrHTTP6( strAddrHTTP6 ),
      m_nBasePortHTTP6( nBasePortHTTP6 ),
      m_strAddrHTTPS4( strAddrHTTPS4 ),
      m_nBasePortHTTPS4( nBasePortHTTPS4 ),
      m_strAddrHTTPS6( strAddrHTTPS6 ),
      m_nBasePortHTTPS6( nBasePortHTTPS6 ),
      m_strAddrWS4( strAddrWS4 ),
      m_nBasePortWS4( nBasePortWS4 ),
      m_strAddrWS6( strAddrWS6 ),
      m_nBasePortWS6( nBasePortWS6 ),
      m_strAddrWSS4( strAddrWSS4 ),
      m_nBasePortWSS4( nBasePortWSS4 ),
      m_strAddrWSS6( strAddrWSS6 ),
      m_nBasePortWSS6( nBasePortWSS6 ),
      m_bTraceCalls( false ),
      m_strPathSslKey( strPathSslKey ),
      m_strPathSslCert( strPathSslCert ),
      m_cntConnections( 0 ),
      m_cntConnectionsMax( 0 ) {}

SkaleServerOverride::~SkaleServerOverride() {
    StopListening();
}

dev::eth::Interface* SkaleServerOverride::ethereum() const {
    if ( !pEth_ ) {
        std::cerr << "SLAKLE server fatal error: no eth interface\n";
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

void SkaleServerOverride::logTraceServerEvent( bool isError, int ipVer, const char* strProtocol,
    int nServerIndex, const std::string& strMessage ) {
    if ( strMessage.empty() )
        return;
    std::stringstream ssProtocol;
    strProtocol = ( strProtocol && strProtocol[0] ) ? strProtocol : "Unknown network protocol";
    ssProtocol << cc::info( strProtocol ) << cc::debug( "/" ) << cc::notice( "IPv" )
               << cc::num10( ipVer );
    if ( nServerIndex >= 0 )
        ssProtocol << cc::debug( "/" ) << cc::num10( nServerIndex );
    ;
    if ( isError )
        ssProtocol << cc::fatal( std::string( " ERROR:" ) );
    else
        ssProtocol << cc::info( std::string( ":" ) );
    std::string strProtocolDescription = ssProtocol.str();
    if ( isError )
        clog( dev::VerbosityError, strProtocolDescription ) << strMessage;
    else
        clog( dev::VerbosityInfo, strProtocolDescription ) << strMessage;
}

void SkaleServerOverride::logTraceServerTraffic( bool isRX, bool isError, int ipVer,
    const char* strProtocol, int nServerIndex, const char* strOrigin,
    const std::string& strPayload ) {
    std::stringstream ssProtocol;
    std::string strProto =
        ( strProtocol && strProtocol[0] ) ? strProtocol : "Unknown network protocol";
    strOrigin = ( strOrigin && strOrigin[0] ) ? strOrigin : "unknown origin";
    std::string strErrorSuffix, strOriginSuffix, strDirect;
    if ( isRX ) {
        strDirect = cc::ws_rx( " >>> " );
        ssProtocol << cc::ws_rx_inv( " >>> " + strProto ) << cc::debug( "/" ) << cc::notice( "IPv" )
                   << cc::num10( ipVer );
        if ( nServerIndex >= 0 )
            ssProtocol << cc::ws_rx_inv( "/" + std::to_string( nServerIndex ) );
        ssProtocol << cc::ws_rx_inv( "/RX >>> " );
    } else {
        strDirect = cc::ws_tx( " <<< " );
        ssProtocol << cc::ws_tx_inv( " <<< " + strProto ) << cc::debug( "/" ) << cc::notice( "IPv" )
                   << cc::num10( ipVer );
        if ( nServerIndex >= 0 )
            ssProtocol << cc::ws_tx_inv( "/" + std::to_string( nServerIndex ) );
        ssProtocol << cc::ws_tx_inv( "/TX <<< " );
    }
    strOriginSuffix = cc::u( strOrigin );
    if ( isError )
        strErrorSuffix = cc::fatal( " ERROR " );
    std::string strProtocolDescription = ssProtocol.str();
    if ( isError )
        clog( dev::VerbosityError, strProtocolDescription )
            << ( strErrorSuffix + strOriginSuffix + strDirect + strPayload );
    else
        clog( dev::VerbosityInfo, strProtocolDescription )
            << ( strErrorSuffix + strOriginSuffix + strDirect + strPayload );
}

bool SkaleServerOverride::implStartListening( std::shared_ptr< SkaleRelayHTTP >& pSrv, int ipVer,
    const std::string& strAddr, int nPort, const std::string& strPathSslKey,
    const std::string& strPathSslCert, int nServerIndex, size_t a_max_http_handler_queues ) {
    bool bIsSSL = false;
    if ( ( !strPathSslKey.empty() ) && ( !strPathSslCert.empty() ) )
        bIsSSL = true;
    try {
        implStopListening( pSrv, ipVer, bIsSSL );
        if ( strAddr.empty() || nPort <= 0 )
            return true;
        logTraceServerEvent( false, ipVer, bIsSSL ? "HTTPS" : "HTTP", -1,
            cc::debug( "starting " ) + cc::info( bIsSSL ? "HTTPS" : "HTTP" ) + cc::debug( "/" ) +
                cc::num10( nServerIndex ) + cc::debug( " server on address " ) +
                cc::info( strAddr ) + cc::debug( " and port " ) + cc::c( nPort ) +
                cc::debug( "..." ) );
        if ( bIsSSL )
            pSrv.reset( new SkaleRelayHTTP( ipVer, strAddr.c_str(), nPort, strPathSslCert.c_str(),
                strPathSslKey.c_str(), nServerIndex, a_max_http_handler_queues ) );
        else
            pSrv.reset( new SkaleRelayHTTP( ipVer, strAddr.c_str(), nPort, nullptr, nullptr,
                nServerIndex, a_max_http_handler_queues ) );
        pSrv->m_pServer->Options(
            "/", [=]( const skutils::http::request& req, skutils::http::response& res ) {
                stats::register_stats_message(
                    bIsSSL ? "HTTPS" : "HTTP", "query options", req.body_.size() );
                if ( m_bTraceCalls )
                    logTraceServerTraffic( true, false, ipVer, bIsSSL ? "HTTPS" : "HTTP",
                        pSrv->serverIndex(), req.origin_.c_str(),
                        cc::info( "OPTTIONS" ) + cc::debug( " request handler" ) );
                res.set_header( "access-control-allow-headers", "Content-Type" );
                res.set_header( "access-control-allow-methods", "POST" );
                res.set_header( "access-control-allow-origin", "*" );
                res.set_header( "content-length", "0" );
                res.set_header( "vary",
                    "Origin, Access-Control-request-Method, Access-Control-request-Headers" );
                stats::register_stats_answer(
                    bIsSSL ? "HTTPS" : "HTTP", "query options", res.body_.size() );
            } );
        pSrv->m_pServer->Post( "/", [=]( const skutils::http::request& req,
                                        skutils::http::response& res ) {
            if ( isShutdownMode() ) {
                logTraceServerEvent( false, ipVer, bIsSSL ? "HTTPS" : "HTTP", pSrv->serverIndex(),
                    cc::notice( bIsSSL ? "HTTPS" : "HTTP" ) + cc::debug( "/" ) +
                        cc::num10( pSrv->serverIndex() ) + " " + cc::warn( "" ) );
                pSrv->m_pServer->close_all_handler_queues();  // remove queues earlier
                return true;
            }
            SkaleServerConnectionsTrackHelper sscth( *this );
            if ( m_bTraceCalls )
                logTraceServerTraffic( true, false, ipVer, bIsSSL ? "HTTPS" : "HTTP",
                    pSrv->serverIndex(), req.origin_.c_str(), cc::j( req.body_ ) );
            nlohmann::json joID = "-1";
            std::string strResponse, strMethod;
            bool bPassed = false;
            try {
                if ( is_connection_limit_overflow() ) {
                    on_connection_overflow_peer_closed(
                        ipVer, bIsSSL ? "HTTPS" : "HTTP", pSrv->serverIndex(), nPort );
                    throw std::runtime_error( "server too busy" );
                }
                nlohmann::json joRequest = nlohmann::json::parse( req.body_ );
                joID = joRequest["id"];
                strMethod = skutils::tools::getFieldSafe< std::string >( joRequest, "method" );
                if ( !handleAdminOriginFilter( strMethod, req.origin_ ) ) {
                    throw std::runtime_error( "origin not allowed for call attempt" );
                }
                jsonrpc::IClientConnectionHandler* handler = this->GetHandler( "/" );
                if ( handler == nullptr )
                    throw std::runtime_error( "No client connection handler found" );
                //
                stats::register_stats_message(
                    bIsSSL ? "HTTPS" : "HTTP", "POST", req.body_.size() );
                stats::register_stats_message(
                    ( std::string( "RPC/" ) + ( bIsSSL ? "HTTPS" : "HTTP" ) ).c_str(), joRequest );
                stats::register_stats_message( "RPC", joRequest );
                //
                std::vector< uint8_t > buffer;
                if ( handleRequestWithBinaryAnswer( joRequest, buffer ) ) {
                    res.set_header( "access-control-allow-origin", "*" );
                    res.set_header( "vary", "Origin" );
                    res.set_content(
                        ( char* ) buffer.data(), buffer.size(), "application/octet-stream" );
                    stats::register_stats_answer(
                        bIsSSL ? "HTTPS" : "HTTP", "POST", buffer.size() );
                    return true;
                }
                handler->HandleRequest( req.body_.c_str(), strResponse );
                //
                stats::register_stats_answer(
                    bIsSSL ? "HTTPS" : "HTTP", "POST", strResponse.size() );
                nlohmann::json joResponse = nlohmann::json::parse( strResponse );
                stats::register_stats_answer(
                    ( std::string( "RPC/" ) + ( bIsSSL ? "HTTPS" : "HTTP" ) ).c_str(), joRequest,
                    joResponse );
                stats::register_stats_answer( "RPC", joRequest, joResponse );
                //
                bPassed = true;
            } catch ( const std::exception& ex ) {
                logTraceServerTraffic( false, true, ipVer, bIsSSL ? "HTTPS" : "HTTP",
                    pSrv->serverIndex(), req.origin_.c_str(), cc::warn( ex.what() ) );
                nlohmann::json joErrorResponce;
                joErrorResponce["id"] = joID;
                joErrorResponce["result"] = "error";
                joErrorResponce["error"] = std::string( ex.what() );
                strResponse = joErrorResponce.dump();
                stats::register_stats_exception( bIsSSL ? "HTTPS" : "HTTP", "POST" );
                if ( !strMethod.empty() ) {
                    stats::register_stats_exception( bIsSSL ? "HTTPS" : "HTTP", strMethod.c_str() );
                    stats::register_stats_exception( "RPC", strMethod.c_str() );
                }
            } catch ( ... ) {
                const char* e = "unknown exception in SkaleServerOverride";
                logTraceServerTraffic( false, true, ipVer, bIsSSL ? "HTTPS" : "HTTP",
                    pSrv->serverIndex(), req.origin_.c_str(), cc::warn( e ) );
                nlohmann::json joErrorResponce;
                joErrorResponce["id"] = joID;
                joErrorResponce["result"] = "error";
                joErrorResponce["error"] = std::string( e );
                strResponse = joErrorResponce.dump();
                stats::register_stats_exception( bIsSSL ? "HTTPS" : "HTTP", "POST" );
                if ( !strMethod.empty() ) {
                    stats::register_stats_exception( bIsSSL ? "HTTPS" : "HTTP", strMethod.c_str() );
                    stats::register_stats_exception( "RPC", strMethod.c_str() );
                }
            }
            if ( m_bTraceCalls )
                logTraceServerTraffic( false, false, ipVer, bIsSSL ? "HTTPS" : "HTTP",
                    pSrv->serverIndex(), req.origin_.c_str(), cc::j( strResponse ) );
            res.set_header( "access-control-allow-origin", "*" );
            res.set_header( "vary", "Origin" );
            res.set_content( strResponse.c_str(), "application/json" );
            if ( !bPassed )
                stats::register_stats_answer( bIsSSL ? "HTTPS" : "HTTP", "POST", res.body_.size() );
            return true;
        } );
        std::thread( [=]() {
            skutils::multithreading::threadNameAppender tn(
                "/" + std::string( bIsSSL ? "HTTPS" : "HTTP" ) + "-listener" );
            if ( !pSrv->m_pServer->listen( ipVer, strAddr.c_str(), nPort ) ) {
                stats::register_stats_error( bIsSSL ? "HTTPS" : "HTTP", "LISTEN" );
                return;
            }
            stats::register_stats_message( bIsSSL ? "HTTPS" : "HTTP", "LISTEN" );
        } )
            .detach();
        logTraceServerEvent( false, ipVer, bIsSSL ? "HTTPS" : "HTTP", pSrv->serverIndex(),
            cc::success( "OK, started " ) + cc::info( bIsSSL ? "HTTPS" : "HTTP" ) +
                cc::debug( "/" ) + cc::num10( pSrv->serverIndex() ) +
                cc::success( " server on address " ) + cc::info( strAddr ) +
                cc::success( " and port " ) + cc::c( nPort ) );
        return true;
    } catch ( const std::exception& ex ) {
        logTraceServerEvent( false, ipVer, bIsSSL ? "HTTPS" : "HTTP", pSrv->serverIndex(),
            cc::error( "FAILED to start " ) + cc::warn( bIsSSL ? "HTTPS" : "HTTP" ) +
                cc::error( " server: " ) + cc::warn( ex.what() ) );
    } catch ( ... ) {
        logTraceServerEvent( false, ipVer, bIsSSL ? "HTTPS" : "HTTP", pSrv->serverIndex(),
            cc::error( "FAILED to start " ) + cc::warn( bIsSSL ? "HTTPS" : "HTTP" ) +
                cc::error( " server: " ) + cc::warn( "unknown exception" ) );
    }
    try {
        implStopListening( pSrv, ipVer, bIsSSL );
    } catch ( ... ) {
    }
    return false;
}

bool SkaleServerOverride::implStartListening( std::shared_ptr< SkaleRelayWS >& pSrv, int ipVer,
    const std::string& strAddr, int nPort, const std::string& strPathSslKey,
    const std::string& strPathSslCert, int nServerIndex ) {
    bool bIsSSL = false;
    if ( ( !strPathSslKey.empty() ) && ( !strPathSslCert.empty() ) )
        bIsSSL = true;
    try {
        implStopListening( pSrv, ipVer, bIsSSL );
        if ( strAddr.empty() || nPort <= 0 )
            return true;
        logTraceServerEvent( false, ipVer, bIsSSL ? "WSS" : "WS", nServerIndex,
            cc::debug( "starting " ) + cc::info( bIsSSL ? "WSS" : "WS" ) + cc::debug( "/" ) +
                cc::num10( nServerIndex ) + cc::debug( " server on address " ) +
                cc::info( strAddr ) + cc::debug( " and port " ) + cc::c( nPort ) +
                cc::debug( "..." ) );
        pSrv.reset( new SkaleRelayWS(
            ipVer, strAddr.c_str(), bIsSSL ? "wss" : "ws", nPort, nServerIndex ) );
        if ( bIsSSL ) {
            pSrv->strCertificateFile_ = strPathSslCert;
            pSrv->strPrivateKeyFile_ = strPathSslKey;
        }
        if ( !pSrv->start( this ) )
            throw std::runtime_error( "Failed to start server" );
        logTraceServerEvent( false, ipVer, bIsSSL ? "WSS" : "WS", pSrv->serverIndex(),
            cc::success( "OK, started " ) + cc::info( bIsSSL ? "WSS" : "WS" ) + cc::debug( "/" ) +
                cc::num10( pSrv->serverIndex() ) + cc::success( " server on address " ) +
                cc::info( strAddr ) + cc::success( " and port " ) + cc::c( nPort ) +
                cc::debug( "..." ) );
        return true;
    } catch ( const std::exception& ex ) {
        logTraceServerEvent( false, ipVer, bIsSSL ? "WSS" : "WS", pSrv->serverIndex(),
            cc::error( "FAILED to start " ) + cc::warn( bIsSSL ? "WSS" : "WS" ) +
                cc::error( " server: " ) + cc::warn( ex.what() ) );
    } catch ( ... ) {
        logTraceServerEvent( false, ipVer, bIsSSL ? "WSS" : "WS", pSrv->serverIndex(),
            cc::error( "FAILED to start " ) + cc::warn( bIsSSL ? "WSS" : "WS" ) +
                cc::error( " server: " ) + cc::warn( "unknown exception" ) );
    }
    try {
        implStopListening( pSrv, ipVer, bIsSSL );
    } catch ( ... ) {
    }
    return false;
}

bool SkaleServerOverride::implStopListening(
    std::shared_ptr< SkaleRelayHTTP >& pSrv, int ipVer, bool bIsSSL ) {
    try {
        if ( !pSrv )
            return true;
        int nServerIndex = pSrv->serverIndex();
        std::string strAddr = ( ipVer == 4 ) ? ( bIsSSL ? m_strAddrHTTPS4 : m_strAddrHTTP4 ) :
                                               ( bIsSSL ? m_strAddrHTTPS6 : m_strAddrHTTP6 );
        int nPort = ( ( ipVer == 4 ) ? ( bIsSSL ? m_nBasePortHTTPS4 : m_nBasePortHTTP4 ) :
                                       ( bIsSSL ? m_nBasePortHTTPS6 : m_nBasePortHTTP6 ) ) +
                    nServerIndex;
        logTraceServerEvent( false, ipVer, bIsSSL ? "HTTPS" : "HTTP", nServerIndex,
            cc::notice( "Will stop " ) + cc::info( bIsSSL ? "HTTPS" : "HTTP" ) +
                cc::notice( " server on address " ) + cc::info( strAddr ) +
                cc::success( " and port " ) + cc::c( nPort ) + cc::notice( "..." ) );
        if ( pSrv->m_pServer && pSrv->m_pServer->is_running() ) {
            pSrv->m_pServer->stop();
            stats::register_stats_message( bIsSSL ? "HTTPS" : "HTTP", "STOP" );
        }
        pSrv.reset();
        logTraceServerEvent( false, ipVer, bIsSSL ? "HTTPS" : "HTTP", nServerIndex,
            cc::success( "OK, stopped " ) + cc::info( bIsSSL ? "HTTPS" : "HTTP" ) +
                cc::success( " server on address " ) + cc::info( strAddr ) +
                cc::success( " and port " ) + cc::c( nPort ) );
    } catch ( ... ) {
    }
    return true;
}

bool SkaleServerOverride::implStopListening(
    std::shared_ptr< SkaleRelayWS >& pSrv, int ipVer, bool bIsSSL ) {
    try {
        if ( !pSrv )
            return true;
        int nServerIndex = pSrv->serverIndex();
        std::string strAddr = ( ipVer == 4 ) ? ( bIsSSL ? m_strAddrWSS4 : m_strAddrWS4 ) :
                                               ( bIsSSL ? m_strAddrWSS6 : m_strAddrWS6 );
        int nPort = ( ( ipVer == 4 ) ? ( bIsSSL ? m_nBasePortWSS4 : m_nBasePortWS4 ) :
                                       ( bIsSSL ? m_nBasePortWSS6 : m_nBasePortWS6 ) ) +
                    nServerIndex;
        logTraceServerEvent( false, ipVer, bIsSSL ? "WSS" : "WS", nServerIndex,
            cc::notice( "Will stop " ) + cc::info( bIsSSL ? "WSS" : "WS" ) +
                cc::notice( " server on address " ) + cc::info( strAddr ) +
                cc::success( " and port " ) + cc::c( nPort ) + cc::notice( "..." ) );
        if ( pSrv->isRunning() )
            pSrv->stop();
        pSrv.reset();
        logTraceServerEvent( false, ipVer, bIsSSL ? "WSS" : "WS", nServerIndex,
            cc::success( "OK, stopped " ) + cc::info( bIsSSL ? "WSS" : "WS" ) +
                cc::success( " server on address " ) + cc::info( strAddr ) +
                cc::success( " and port " ) + cc::c( nPort ) );
    } catch ( ... ) {
    }
    return true;
}

bool SkaleServerOverride::StartListening() {
    m_bShutdownMode = false;
    size_t nServerIndex, cntServers;
    if ( 0 <= m_nBasePortHTTP4 && m_nBasePortHTTP4 <= 65535 ) {
        cntServers = m_cntServers;
        for ( nServerIndex = 0; nServerIndex < cntServers; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayHTTP > pServer;
            if ( !implStartListening( pServer, 4, m_strAddrHTTP4, m_nBasePortHTTP4 + nServerIndex,
                     "", "", nServerIndex, max_http_handler_queues_ ) )
                return false;
            m_serversHTTP4.push_back( pServer );
        }
    }
    if ( 0 <= m_nBasePortHTTP6 && m_nBasePortHTTP6 <= 65535 ) {
        cntServers = m_cntServers;
        for ( nServerIndex = 0; nServerIndex < cntServers; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayHTTP > pServer;
            if ( !implStartListening( pServer, 6, m_strAddrHTTP6, m_nBasePortHTTP6 + nServerIndex,
                     "", "", nServerIndex, max_http_handler_queues_ ) )
                return false;
            m_serversHTTP6.push_back( pServer );
        }
    }
    if ( 0 <= m_nBasePortHTTPS4 && m_nBasePortHTTPS4 <= 65535 && ( !m_strPathSslKey.empty() ) &&
         ( !m_strPathSslCert.empty() ) && m_nBasePortHTTPS4 != m_nBasePortHTTP4 ) {
        cntServers = m_cntServers;
        for ( nServerIndex = 0; nServerIndex < cntServers; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayHTTP > pServer;
            if ( !implStartListening( pServer, 4, m_strAddrHTTPS4, m_nBasePortHTTPS4 + nServerIndex,
                     m_strPathSslKey, m_strPathSslCert, nServerIndex, max_http_handler_queues_ ) )
                return false;
            m_serversHTTPS4.push_back( pServer );
        }
    }
    if ( 0 <= m_nBasePortHTTPS6 && m_nBasePortHTTPS6 <= 65535 && ( !m_strPathSslKey.empty() ) &&
         ( !m_strPathSslCert.empty() ) && m_nBasePortHTTPS6 != m_nBasePortHTTP6 ) {
        cntServers = m_cntServers;
        for ( nServerIndex = 0; nServerIndex < cntServers; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayHTTP > pServer;
            if ( !implStartListening( pServer, 6, m_strAddrHTTPS6, m_nBasePortHTTPS6 + nServerIndex,
                     m_strPathSslKey, m_strPathSslCert, nServerIndex, max_http_handler_queues_ ) )
                return false;
            m_serversHTTPS6.push_back( pServer );
        }
    }
    if ( 0 <= m_nBasePortWS4 && m_nBasePortWS4 <= 65535 && m_nBasePortWS4 != m_nBasePortHTTP4 &&
         m_nBasePortWS4 != m_nBasePortHTTPS4 ) {
        cntServers = m_cntServers;
        for ( nServerIndex = 0; nServerIndex < cntServers; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayWS > pServer;
            if ( !implStartListening( pServer, 4, m_strAddrWS4, m_nBasePortWS4 + nServerIndex, "",
                     "", nServerIndex ) )
                return false;
            m_serversWS4.push_back( pServer );
        }
    }
    if ( 0 <= m_nBasePortWS6 && m_nBasePortWS6 <= 65535 && m_nBasePortWS6 != m_nBasePortHTTP6 &&
         m_nBasePortWS6 != m_nBasePortHTTPS6 ) {
        cntServers = m_cntServers;
        for ( nServerIndex = 0; nServerIndex < cntServers; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayWS > pServer;
            if ( !implStartListening( pServer, 6, m_strAddrWS6, m_nBasePortWS6 + nServerIndex, "",
                     "", nServerIndex ) )
                return false;
            m_serversWS6.push_back( pServer );
        }
    }
    if ( 0 <= m_nBasePortWSS4 && m_nBasePortWSS4 <= 65535 && ( !m_strPathSslKey.empty() ) &&
         ( !m_strPathSslCert.empty() ) && m_nBasePortWSS4 != m_nBasePortWS4 &&
         m_nBasePortWSS4 != m_nBasePortHTTP4 && m_nBasePortWSS4 != m_nBasePortHTTPS4 ) {
        cntServers = m_cntServers;
        for ( nServerIndex = 0; nServerIndex < cntServers; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayWS > pServer;
            if ( !implStartListening( pServer, 4, m_strAddrWSS4, m_nBasePortWSS4 + nServerIndex,
                     m_strPathSslKey, m_strPathSslCert, nServerIndex ) )
                return false;
            m_serversWSS4.push_back( pServer );
        }
    }
    if ( 0 <= m_nBasePortWSS6 && m_nBasePortWSS6 <= 65535 && ( !m_strPathSslKey.empty() ) &&
         ( !m_strPathSslCert.empty() ) && m_nBasePortWSS6 != m_nBasePortWS6 &&
         m_nBasePortWSS6 != m_nBasePortHTTP6 && m_nBasePortWSS6 != m_nBasePortHTTPS6 ) {
        cntServers = m_cntServers;
        for ( nServerIndex = 0; nServerIndex < cntServers; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayWS > pServer;
            if ( !implStartListening( pServer, 6, m_strAddrWSS6, m_nBasePortWSS6 + nServerIndex,
                     m_strPathSslKey, m_strPathSslCert, nServerIndex ) )
                return false;
            m_serversWSS6.push_back( pServer );
        }
    }
    return true;
}

bool SkaleServerOverride::StopListening() {
    m_bShutdownMode = true;
    bool bRetVal = true;
    for ( auto pServer : m_serversHTTP4 ) {
        if ( !implStopListening( pServer, 4, false ) )
            bRetVal = false;
    }
    m_serversHTTP4.clear();
    for ( auto pServer : m_serversHTTP6 ) {
        if ( !implStopListening( pServer, 6, false ) )
            bRetVal = false;
    }
    m_serversHTTP6.clear();
    //
    for ( auto pServer : m_serversHTTPS4 ) {
        if ( !implStopListening( pServer, 4, true ) )
            bRetVal = false;
    }
    m_serversHTTPS4.clear();
    for ( auto pServer : m_serversHTTPS6 ) {
        if ( !implStopListening( pServer, 6, true ) )
            bRetVal = false;
    }
    m_serversHTTPS6.clear();
    //
    for ( auto pServer : m_serversWS4 ) {
        if ( !implStopListening( pServer, 4, false ) )
            bRetVal = false;
    }
    m_serversWS4.clear();
    for ( auto pServer : m_serversWS6 ) {
        if ( !implStopListening( pServer, 6, false ) )
            bRetVal = false;
    }
    m_serversWS6.clear();
    //
    for ( auto pServer : m_serversWSS4 ) {
        if ( !implStopListening( pServer, 4, true ) )
            bRetVal = false;
    }
    m_serversWSS4.clear();
    for ( auto pServer : m_serversWSS6 ) {
        if ( !implStopListening( pServer, 6, true ) )
            bRetVal = false;
    }
    m_serversWSS6.clear();
    return bRetVal;
}

int SkaleServerOverride::getServerPortStatusHTTP( int ipVer ) const {
    for ( auto pServer : ( ( ipVer == 4 ) ? m_serversHTTP4 : m_serversHTTP6 ) ) {
        if ( pServer && pServer->m_pServer && pServer->m_pServer->is_running() )
            return ( ( ipVer == 4 ) ? m_nBasePortHTTP4 : m_nBasePortHTTP6 ) +
                   pServer->serverIndex();
    }
    return -1;
}
int SkaleServerOverride::getServerPortStatusHTTPS( int ipVer ) const {
    for ( auto pServer : ( ( ipVer == 4 ) ? m_serversHTTPS4 : m_serversHTTPS6 ) ) {
        if ( pServer && pServer->m_pServer && pServer->m_pServer->is_running() )
            return ( ( ipVer == 4 ) ? m_nBasePortHTTPS4 : m_nBasePortHTTPS6 ) +
                   pServer->serverIndex();
    }
    return -1;
}
int SkaleServerOverride::getServerPortStatusWS( int ipVer ) const {
    for ( auto pServer : ( ( ipVer == 4 ) ? m_serversWS4 : m_serversWS6 ) ) {
        if ( pServer && pServer->isRunning() )
            return ( ( ipVer == 4 ) ? m_nBasePortWS4 : m_nBasePortWS6 ) + pServer->serverIndex();
    }
    return -1;
}
int SkaleServerOverride::getServerPortStatusWSS( int ipVer ) const {
    for ( auto pServer : ( ( ipVer == 4 ) ? m_serversWSS4 : m_serversWSS6 ) ) {
        if ( pServer && pServer->isRunning() )
            return ( ( ipVer == 4 ) ? m_nBasePortWSS4 : m_nBasePortWSS6 ) + pServer->serverIndex();
    }
    return -1;
}

bool SkaleServerOverride::is_connection_limit_overflow() const {
    size_t cntConnectionsMax = size_t( m_cntConnectionsMax );
    if ( cntConnectionsMax == 0 )
        return false;
    size_t cntConnections = size_t( m_cntConnections );
    if ( cntConnections <= cntConnectionsMax )
        return false;
    return true;
}
void SkaleServerOverride::connection_counter_inc() {
    ++m_cntConnections;
}
void SkaleServerOverride::connection_counter_dec() {
    --m_cntConnections;
}
size_t SkaleServerOverride::max_connection_get() const {
    size_t cntConnectionsMax = size_t( m_cntConnectionsMax );
    return cntConnectionsMax;
}
void SkaleServerOverride::max_connection_set( size_t cntConnectionsMax ) {
    m_cntConnectionsMax = cntConnectionsMax;
}

void SkaleServerOverride::on_connection_overflow_peer_closed(
    int ipVer, const char* strProtocol, int nServerIndex, int nPort ) {
    std::string strMessage = cc::info( strProtocol ) + cc::debug( "/" ) +
                             cc::num10( nServerIndex ) + cc::warn( " server on port " ) +
                             cc::num10( nPort ) +
                             cc::warn( " did closed peer because of connection limit overflow" );
    logTraceServerEvent( false, ipVer, strProtocol, nServerIndex, strMessage );
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
    joStats["protocols"]["http"]["listenerCount"] = m_serversHTTP4.size() + m_serversHTTP6.size();
    joStats["protocols"]["http"]["stats"] = stats::generate_subsystem_stats( "HTTP" );
    joStats["protocols"]["http"]["rpc"] = stats::generate_subsystem_stats( "RPC/HTTP" );
    joStats["protocols"]["https"]["listenerCount"] =
        m_serversHTTPS4.size() + m_serversHTTPS6.size();
    joStats["protocols"]["https"]["stats"] = stats::generate_subsystem_stats( "HTTPS" );
    joStats["protocols"]["https"]["rpc"] = stats::generate_subsystem_stats( "RPC/HTTPS" );
    joStats["protocols"]["ws"]["listenerCount"] = m_serversWS4.size() + m_serversWS6.size();
    joStats["protocols"]["ws"]["stats"] = stats::generate_subsystem_stats( "WS" );
    joStats["protocols"]["ws"]["rpc"] = stats::generate_subsystem_stats( "RPC/WS" );
    joStats["protocols"]["wss"]["listenerCount"] = m_serversWSS4.size() + m_serversWSS6.size();
    joStats["protocols"]["wss"]["stats"] = stats::generate_subsystem_stats( "WSS" );
    joStats["protocols"]["wss"]["rpc"] = stats::generate_subsystem_stats( "RPC/WSS" );
    joStats["rpc"] = stats::generate_subsystem_stats( "RPC" );
    //
    skutils::tools::load_monitor& lm = stat_get_load_monitor();
    double lfCpuLoad = lm.last_cpu_load();
    joStats["system"]["cpu_load"] = lfCpuLoad;
    joStats["system"]["disk_usage"] = lm.last_disk_load();
    double lfMemUsage = skutils::tools::mem_usage();
    joStats["system"]["mem_usage"] = lfMemUsage;
    return joStats;
}

bool SkaleServerOverride::handleRequestWithBinaryAnswer(
    const nlohmann::json& joRequest, std::vector< uint8_t >& buffer ) {
    buffer.clear();
    std::string strMethodName = skutils::tools::getFieldSafe< std::string >( joRequest, "method" );
    if ( strMethodName == "skale_downloadSnapshotFragment" && fn_binary_snapshot_download_ ) {
        //        std::cout << cc::attention( "------------ " )
        //                  << cc::info( "skale_downloadSnapshotFragment" ) << cc::normal( " call
        //                  with " )
        //                  << cc::j( joRequest ) << "\n";
        const nlohmann::json& joParams = joRequest["params"];
        if ( joParams.count( "isBinary" ) > 0 ) {
            bool isBinary = joParams["isBinary"].get< bool >();
            if ( isBinary ) {
                buffer = fn_binary_snapshot_download_( joParams );
                return true;
            }
        }
    }
    return false;
}

bool SkaleServerOverride::handleAdminOriginFilter(
    const std::string& strMethod, const std::string& strOriginURL ) {
    // std::cout << cc::attention( "------------ " ) << cc::info( strOriginURL ) <<
    // cc::attention( "
    // ------------> " ) << cc::info( strMethod ) << "\n";
    static const std::set< std::string > g_setAdminMethods = {
        "skale_getSnapshot", "skale_downloadSnapshotFragment"};
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
