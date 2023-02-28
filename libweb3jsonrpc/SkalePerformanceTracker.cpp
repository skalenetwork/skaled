/*
    Copyright (C) 2020-present SKALE Labs

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
/** @file SkalePerformanceTracker.cpp
 * @authors:
 *   Oleh Nikolaiev <oleg@skalelabs.com>
 * @date 2020
 */

#include "SkalePerformanceTracker.h"
#include "Eth.h"
#include <jsonrpccpp/common/exception.h>
#include <libweb3jsonrpc/JsonHelper.h>

#include <libdevcore/BMPBN.h>
#include <libdevcore/Common.h>
#include <libdevcore/CommonJS.h>

#include <skutils/console_colors.h>
#include <skutils/eth_utils.h>
#include <skutils/rest_call.h>
#include <skutils/task_performance.h>

#include <array>
#include <csignal>
#include <exception>
#include <fstream>
#include <set>

#include <skutils/console_colors.h>
#include <skutils/utils.h>

namespace dev {
namespace rpc {

SkalePerformanceTracker::SkalePerformanceTracker( const std::string& configPath )
    : skutils::json_config_file_accessor( configPath ) {}

Json::Value SkalePerformanceTracker::skale_performanceTrackingStatus(
    const Json::Value& /*request*/ ) {
    std::string strLogPrefix = cc::deep_info( "Performance tracking status" );
    try {
        skutils::task::performance::tracker_ptr pTracker =
            skutils::task::performance::get_default_tracker();
        bool bTrackerIsRunning = pTracker->is_running();
        //
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = true;
        jo["trackerIsRunning"] = bTrackerIsRunning;
        jo["maxItemCount"] = pTracker->get_safe_max_item_count();
        jo["sessionMaxItemCount"] = pTracker->get_session_max_item_count();
        jo["sessionStopReason"] = pTracker->get_first_encountered_stop_reason();
        //
        std::string s = jo.dump();
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing request: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing request: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" ) << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                                           cc::error( " Exception while processing request: " ) +
                                           cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

Json::Value SkalePerformanceTracker::skale_performanceTrackingStart( const Json::Value& request ) {
    std::string strLogPrefix = cc::deep_info( "Performance tracking start" );
    try {
        Json::FastWriter fastWriter;
        const std::string strRequest = fastWriter.write( request );
        const nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        //
        bool bIsRestart = true;
        if ( joRequest.count( "isRestart" ) > 0 )
            bIsRestart = joRequest["isRestart"].get< bool >();
        //
        skutils::task::performance::tracker_ptr pTracker =
            skutils::task::performance::get_default_tracker();
        if ( bIsRestart )
            pTracker->cancel();
        pTracker->start();
        bool bTrackerIsRunning = pTracker->is_running();
        //
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = true;
        jo["trackerIsRunning"] = bTrackerIsRunning;
        jo["maxItemCount"] = pTracker->get_safe_max_item_count();
        jo["sessionMaxItemCount"] = pTracker->get_session_max_item_count();
        jo["sessionStopReason"] = pTracker->get_first_encountered_stop_reason();
        //
        std::string s = jo.dump();
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing request: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing request: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" ) << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                                           cc::error( " Exception while processing request: " ) +
                                           cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

Json::Value SkalePerformanceTracker::skale_performanceTrackingStop(
    const Json::Value& /*request*/ ) {
    std::string strLogPrefix = cc::deep_info( "Performance tracking stop" );
    try {
        skutils::task::performance::tracker_ptr pTracker =
            skutils::task::performance::get_default_tracker();
        bool bTrackerIsRunning = pTracker->is_running();
        //
        nlohmann::json joPerformance =
            bTrackerIsRunning ? pTracker->stop() : nlohmann::json::object();
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = bTrackerIsRunning;
        jo["trackerIsRunning"] = false;
        jo["maxItemCount"] = pTracker->get_safe_max_item_count();
        jo["sessionMaxItemCount"] = pTracker->get_session_max_item_count();
        jo["performance"] = joPerformance;
        jo["sessionStopReason"] = pTracker->get_first_encountered_stop_reason();
        //
        std::string s = jo.dump();
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing request: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing request: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" ) << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                                           cc::error( " Exception while processing request: " ) +
                                           cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

Json::Value SkalePerformanceTracker::skale_performanceTrackingFetch( const Json::Value& request ) {
    std::string strLogPrefix = cc::deep_info( "Performance tracking fetch" );
    try {
        Json::FastWriter fastWriter;
        const std::string strRequest = fastWriter.write( request );
        const nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        //
        skutils::task::performance::index_type minIndexT = 0;
        if ( joRequest.count( "minIndex" ) > 0 )
            minIndexT = joRequest["minIndex"].get< size_t >();
        //
        skutils::task::performance::tracker_ptr pTracker =
            skutils::task::performance::get_default_tracker();
        bool bTrackerIsRunning = pTracker->is_running();
        nlohmann::json joPerformance =
            bTrackerIsRunning ? pTracker->compose_json( minIndexT ) : nlohmann::json::object();
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = bTrackerIsRunning;
        jo["trackerIsRunning"] = bTrackerIsRunning;
        jo["maxItemCount"] = pTracker->get_safe_max_item_count();
        jo["sessionMaxItemCount"] = pTracker->get_session_max_item_count();
        jo["sessionStopReason"] = pTracker->get_first_encountered_stop_reason();
        jo["performance"] = joPerformance;
        //
        std::string s = jo.dump();
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing request: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing request: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" ) << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                                           cc::error( " Exception while processing request: " ) +
                                           cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}


};  // namespace rpc
};  // namespace dev
