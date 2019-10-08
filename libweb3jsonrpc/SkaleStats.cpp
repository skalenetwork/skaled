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

#include <csignal>

#include <skutils/console_colors.h>
#include <skutils/eth_utils.h>

namespace dev {
namespace rpc {

SkaleStats::SkaleStats( eth::Interface& _eth ) : m_eth( _eth ) {}

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
    }
}

};  // namespace rpc
};  // namespace dev
