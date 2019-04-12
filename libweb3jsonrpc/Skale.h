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
 * @file Skale.h
 * @author Bogdan Bliznyuk
 * @date 2018
 */

#ifndef CPP_ETHEREUM_SKALE_H
#define CPP_ETHEREUM_SKALE_H

#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server.h>
#include <libethereum/Interface.h>
#include <libweb3jsonrpc/SkaleFace.h>
#include <functional>
#include <iosfwd>
#include <list>
#include <memory>

class SkaleHost;

namespace dev {

namespace rpc {

/**
 * @brief Skale JSON-RPC api implementation
 */
class Skale : public dev::rpc::SkaleFace {
public:
    explicit Skale( SkaleHost& _skale );

    virtual RPCModules implementedModules() const override {
        return RPCModules{RPCModule{"skale", "0.1"}};
    }


    std::string skale_protocolVersion() override;
    std::string skale_receiveTransaction( std::string const& _rlp ) override;
    std::string skale_shutdownInstance() noexcept( false ) override;

    static bool isWeb3ShutdownEnabled();
    static void enableWeb3Shutdown( bool bEnable = true );
    static bool isShutdownNeeded();

    typedef std::function< void() > fn_on_shutdown_t;
    static void onShutdownInvoke( fn_on_shutdown_t fn );

private:
    static volatile bool g_bShutdownViaWeb3Enabled;
    static volatile bool g_bNodeInstanceShouldShutdown;
    typedef std::list< fn_on_shutdown_t > list_fn_on_shutdown_t;
    static list_fn_on_shutdown_t g_list_fn_on_shutdown;

    SkaleHost& m_skaleHost;
};
}  // namespace rpc
}  // namespace dev

#endif  // CPP_ETHEREUM_SKALE_H
