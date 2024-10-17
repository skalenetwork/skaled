/*
    Copyright (C) 2018 SKALE Labs

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
/** @file SkaleStats.h
 * @authors:
 *   Sergiy Lavrynenko <sergiy@skalelabs.com>
 * @date 2019
 */

#pragma once

#include "SkaleStatsFace.h"
#include "SkaleStatsSite.h"
#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server.h>
#include <libdevcore/Common.h>

#include <libethereum/ChainParams.h>

//#include <nlohmann/json.hpp>
#include <json.hpp>

#include <time.h>

#include <skutils/dispatch.h>
#include <skutils/multithreading.h>
#include <skutils/utils.h>

#include <atomic>
#include <list>
#include <set>
#include <shared_mutex>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace dev {

class NetworkFace;
class KeyPair;

namespace eth {

class AccountHolder;
struct TransactionSkeleton;

class Interface;
};  // namespace eth

// if following is defined then pending IMA transactions will be tracked in dispatch timer based job
//#define __IMA_PTX_ENABLE_TRACKING_PARALLEL 1

// if following is defined then pending IMA transactions will be tracked on-the-fly during
// insert/erase
//#define __IMA_PTX_ENABLE_TRACKING_ON_THE_FLY 1

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace rpc {

constexpr uint64_t ANSWER_TIME_HISTORY_SIZE = 128;

class StatsCounter {
public:
    StatsCounter() = default;

    void reset() {
        calls = 0;
        answers = 0;
        errors = 0;
    }

    std::atomic< uint64_t > calls;
    std::atomic< uint64_t > answers;
    std::atomic< uint64_t > errors;
    std::array< std::atomic< uint64_t >, ANSWER_TIME_HISTORY_SIZE > answerTimeHistory;
};


/**
 * @brief JSON-RPC api implementation
 */
class SkaleStats : public dev::rpc::SkaleStatsFace,
                   public dev::rpc::SkaleStatsConsumerImpl,
                   public skutils::json_config_file_accessor {
    int nThisNodeIndex_ = -1;  // 1-based "schainIndex"
    int findThisNodeIndex();

    const dev::eth::ChainParams& chainParams_;

public:
    bool isExposeAllDebugInfo_ = false;

    SkaleStats( const std::string& configPath, eth::Interface& _eth,
        const dev::eth::ChainParams& chainParams );

    virtual RPCModules implementedModules() const override {
        return RPCModules{ RPCModule{ "skaleStats", "1.0" } };
    }


    Json::Value skale_stats() override;
    Json::Value skale_nodesRpcInfo() override;
    Json::Value skale_imaInfo() override;

    static void countCall( const std::string& _origin, const std::string& _method ) {
        auto iterator = statsCounters.find( getProtocol( _origin ) + _method );

        if ( iterator != statsCounters.end() ) {
            ++iterator->second.calls;
        }
    }

    static void countAnswer( const std::string& _origin, const std::string& _method,
        std::chrono::microseconds _beginTime ) {
        auto iterator = statsCounters.find( getProtocol( _origin ) + _method );

        if ( iterator != statsCounters.end() ) {
            StatsCounter& statsCounter = iterator->second;
            int64_t answerTime = ( std::chrono::duration_cast< std::chrono::microseconds >(
                                       std::chrono::system_clock::now().time_since_epoch() ) -
                                   _beginTime )
                                     .count();
            statsCounter.answerTimeHistory.at( statsCounter.answers % ANSWER_TIME_HISTORY_SIZE )
                .store( answerTime );
            ++statsCounter.answers;
        }
    }

    static void countError( const std::string& _origin, const std::string& _method ) {
        auto iterator = statsCounters.find( getProtocol( _origin ) + _method );

        if ( iterator != statsCounters.end() ) {
            ++iterator->second.errors;
        }
    }

    // return http: https: ws: or wss:
    static std::string getProtocol( const std::string& _origin ) {
        auto pos = _origin.find( ':' );

        if ( pos == std::string::npos ) {
            return "";
        }
        return _origin.substr( 0, pos + 1 );
    }


protected:
    eth::Interface* client() const { return &m_eth; }
    eth::Interface& m_eth;


public:
    static std::unordered_map< std::string, StatsCounter > statsCounters;
    static void initStatsCounters();
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace rpc
};  // namespace dev
