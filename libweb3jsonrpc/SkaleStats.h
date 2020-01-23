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

//#include <nlohmann/json.hpp>
#include <json.hpp>

#include <time.h>

#include <skutils/multithreading.h>

namespace dev {
class NetworkFace;
class KeyPair;
namespace eth {
class AccountHolder;
struct TransactionSkeleton;
class Interface;
}  // namespace eth

}  // namespace dev

namespace dev {
namespace rpc {

/**
 * @brief JSON-RPC api implementation
 */
class SkaleStats : public dev::rpc::SkaleStatsFace, public dev::rpc::SkaleStatsConsumerImpl {
    const std::string configPath_;
    time_t configModificationTime_;
    nlohmann::json joConfig_;
    int nThisNodeIndex_ = -1;  // 1-based "schainIndex"
    int findThisNodeIndex();

    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;
    mutex_type mtx_;
    mutex_type& mtx() { return mtx_; }

    void reloadConfigIfNeeded();

public:
    SkaleStats(
        const std::string& configPath, const nlohmann::json& joConfig, eth::Interface& _eth );

    virtual RPCModules implementedModules() const override {
        return RPCModules{RPCModule{"skaleStats", "1.0"}};
    }

    virtual Json::Value skale_stats() override;
    virtual Json::Value skale_nodesRpcInfo() override;
    virtual Json::Value skale_imaInfo() override;
    virtual Json::Value skale_imaVerifyAndSign( const Json::Value& request ) override;

protected:
    eth::Interface* client() const { return &m_eth; }
    eth::Interface& m_eth;
};

};  // namespace rpc
};  // namespace dev
