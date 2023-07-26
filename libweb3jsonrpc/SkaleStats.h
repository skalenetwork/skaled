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

    bool isEnabledImaMessageSigning() const;

    virtual Json::Value skale_stats() override;
    virtual Json::Value skale_nodesRpcInfo() override;
    virtual Json::Value skale_imaInfo() override;

protected:
    eth::Interface* client() const { return &m_eth; }
    eth::Interface& m_eth;

    std::string pick_own_s_chain_url_s();
    skutils::url pick_own_s_chain_url();
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace rpc
};  // namespace dev
