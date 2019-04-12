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
 * @file Skale.cpp
 * @author Bogdan Bliznyuk
 * @date 2018
 */

#include "Skale.h"

#include <libskale/SkaleClient.h>

#include <libethereum/SkaleHost.h>

#include "JsonHelper.h"
#include <libethcore/Common.h>
#include <libethcore/CommonJS.h>

//#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>

using namespace dev::eth;

namespace dev {
namespace rpc {
std::string exceptionToErrorMessage();
}
}  // namespace dev

dev::rpc::Skale::Skale( SkaleHost& _skale ) : m_skaleHost( _skale ) {}

volatile bool dev::rpc::Skale::g_bShutdownViaWeb3Enabled = false;
volatile bool dev::rpc::Skale::g_bNodeInstanceShouldShutdown = false;
dev::rpc::Skale::list_fn_on_shutdown_t dev::rpc::Skale::g_list_fn_on_shutdown;

bool dev::rpc::Skale::isWeb3ShutdownEnabled() {
    return g_bShutdownViaWeb3Enabled;
}
void dev::rpc::Skale::enableWeb3Shutdown( bool bEnable /*= true*/ ) {
    if ( ( g_bShutdownViaWeb3Enabled && bEnable ) ||
         ( ( !g_bShutdownViaWeb3Enabled ) && ( !bEnable ) ) )
        return;
    g_bShutdownViaWeb3Enabled = bEnable;
    if ( !g_bShutdownViaWeb3Enabled )
        g_list_fn_on_shutdown.clear();
}

bool dev::rpc::Skale::isShutdownNeeded() {
    return g_bNodeInstanceShouldShutdown;
}
void dev::rpc::Skale::onShutdownInvoke( fn_on_shutdown_t fn ) {
    if ( !fn )
        return;
    g_list_fn_on_shutdown.push_back( fn );
}

std::string dev::rpc::Skale::skale_shutdownInstance() {
    if ( !g_bShutdownViaWeb3Enabled ) {
        std::cout << "\nINSTANCE SHUTDOWN ATTEMPT WHEN DISABLED\n\n";
        return toJS( "disabled" );
    }
    if ( g_bNodeInstanceShouldShutdown ) {
        std::cout << "\nSECONDARY INSTANCE SHUTDOWN EVENT\n\n";
        return toJS( "in progress(secondary attempt)" );
    }
    g_bNodeInstanceShouldShutdown = true;
    std::cout << "\nINSTANCE SHUTDOWN EVENT\n\n";
    for ( auto& fn : g_list_fn_on_shutdown ) {
        if ( !fn )
            continue;
        try {
            fn();
        } catch ( std::exception& ex ) {
            std::string s = ex.what();
            if ( s.empty() )
                s = "no description";
            std::cout << "Exception in shutdown event handler: " << s << "\n";
        } catch ( ... ) {
            std::cout << "Unknown exception in shutdown event handler\n";
        }
    }  // for( auto & fn : g_list_fn_on_shutdown )
    g_list_fn_on_shutdown.clear();
    return toJS( "will shutdown" );
}

std::string dev::rpc::Skale::skale_protocolVersion() {
    return toJS( "0.2" );
}

std::string dev::rpc::Skale::skale_receiveTransaction( std::string const& _rlp ) {
    try {
        return toJS( m_skaleHost.receiveTransaction( _rlp ) );
    } catch ( Exception const& ) {
        throw jsonrpc::JsonRpcException( dev::rpc::exceptionToErrorMessage() );  // TODO test!
    }
}
