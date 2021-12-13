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
/** @file SkalePerformanceTracker.h
 * @authors:
 *   Oleh Nikolaiev <oleg@skalelabs.com>
 * @date 2020
 */

#ifndef SkalePerformanceTracker_H
#define SkalePerformanceTracker_H

#include "SkalePerformanceTrackerFace.h"
#include "SkaleStatsSite.h"
#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server.h>
#include <libdevcore/Common.h>

#include <json.hpp>

#include <time.h>

#include <skutils/multithreading.h>
#include <skutils/utils.h>

namespace dev {
namespace rpc {

/**
 * @brief JSON-RPC api implementation
 */
class SkalePerformanceTracker : public dev::rpc::SkalePerformanceTrackerFace,
                                public dev::rpc::SkaleStatsConsumerImpl,
                                public skutils::json_config_file_accessor {
public:
    SkalePerformanceTracker( const std::string& configPath );

    virtual RPCModules implementedModules() const override {
        return RPCModules{RPCModule{"SkalePerformanceTracker", "1.0"}};
    }

    virtual Json::Value skale_performanceTrackingStatus( const Json::Value& request ) override;
    virtual Json::Value skale_performanceTrackingStart( const Json::Value& request ) override;
    virtual Json::Value skale_performanceTrackingStop( const Json::Value& request ) override;
    virtual Json::Value skale_performanceTrackingFetch( const Json::Value& request ) override;
};

};  // namespace rpc
};  // namespace dev

#endif  // SkalePerformanceTracker_H
