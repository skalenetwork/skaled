/*
    Modifications Copyright (C) 2018-2019 SKALE Labs

    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file Precompiled.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include <libdevcore/CommonData.h>
#include <libdevcore/Exceptions.h>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

//#include <nlohmann/json.hpp>
#include <json.hpp>

#include <skutils/multithreading.h>
#include <skutils/utils.h>

class SkaleHost;

namespace skale {
class State;
class OverlayFS;
}  // namespace skale

using skale::State;

namespace dev {
namespace eth {

extern std::shared_ptr< skutils::json_config_file_accessor > g_configAccesssor;
extern std::shared_ptr< SkaleHost > g_skaleHost;
extern skale::State g_state;

struct ChainOperationParams;

// allow call both with overlayFS and without it
class PrecompiledExecutor {
public:
    std::pair< bool, bytes > operator()(
        bytesConstRef _in, skale::OverlayFS* _overlayFS = nullptr ) const {
        return proxy( _in, _overlayFS );
    }
    PrecompiledExecutor() {}
    PrecompiledExecutor( const std::function< std::pair< bool, bytes >(
            bytesConstRef _in, skale::OverlayFS* _overlayFS ) >& _func )
        : proxy( _func ) {}

private:
    std::function< std::pair< bool, bytes >( bytesConstRef _in, skale::OverlayFS* _overlayFS ) >
        proxy;
};

using PrecompiledPricer = std::function< bigint(
    bytesConstRef _in, ChainOperationParams const& _chainParams, u256 const& _blockNumber ) >;

DEV_SIMPLE_EXCEPTION( ExecutorNotFound );
DEV_SIMPLE_EXCEPTION( PricerNotFound );

class PrecompiledRegistrar {
public:
    /// Get the executor object for @a _name function or @throw ExecutorNotFound if not found.
    static PrecompiledExecutor const& executor( std::string const& _name );

    /// Get the price calculator object for @a _name function or @throw PricerNotFound if not found.
    static PrecompiledPricer const& pricer( std::string const& _name );

    /// Register an executor. In general just use ETH_REGISTER_PRECOMPILED.
    static PrecompiledExecutor registerExecutor(
        std::string const& _name, PrecompiledExecutor const& _exec ) {
        return ( get()->m_execs[_name] = _exec );
    }
    /// Unregister an executor. Shouldn't generally be necessary.
    static void unregisterExecutor( std::string const& _name ) { get()->m_execs.erase( _name ); }

    /// Register a pricer. In general just use ETH_REGISTER_PRECOMPILED_PRICER.
    static PrecompiledPricer registerPricer(
        std::string const& _name, PrecompiledPricer const& _exec ) {
        return ( get()->m_pricers[_name] = _exec );
    }
    /// Unregister a pricer. Shouldn't generally be necessary.
    static void unregisterPricer( std::string const& _name ) { get()->m_pricers.erase( _name ); }

private:
    static PrecompiledRegistrar* get() {
        if ( !s_this )
            s_this = new PrecompiledRegistrar;
        return s_this;
    }

    std::unordered_map< std::string, PrecompiledExecutor > m_execs;
    std::unordered_map< std::string, PrecompiledPricer > m_pricers;
    static PrecompiledRegistrar* s_this;
};

// ignore _overlayFS param and call registered function with 1 parameter
// TODO: unregister on unload with a static object.
#define ETH_REGISTER_PRECOMPILED( Name )                                                          \
    static std::pair< bool, bytes > __eth_registerPrecompiledFunction##Name( bytesConstRef _in ); \
    static PrecompiledExecutor __eth_registerPrecompiledFactory##Name =                           \
        ::dev::eth::PrecompiledRegistrar::registerExecutor(                                       \
            #Name, PrecompiledExecutor(                                                           \
                       []( bytesConstRef _in, skale::OverlayFS* ) -> std::pair< bool, bytes > {   \
                           return __eth_registerPrecompiledFunction##Name( _in );                 \
                       } ) );                                                                     \
    static std::pair< bool, bytes > __eth_registerPrecompiledFunction##Name

#define ETH_REGISTER_FS_PRECOMPILED( Name )                                           \
    static std::pair< bool, bytes > __eth_registerPrecompiledFunction##Name(          \
        bytesConstRef _in, skale::OverlayFS* _overlayFS );                            \
    static PrecompiledExecutor __eth_registerPrecompiledFactory##Name =               \
        ::dev::eth::PrecompiledRegistrar::registerExecutor(                           \
            #Name, PrecompiledExecutor( &__eth_registerPrecompiledFunction##Name ) ); \
    static std::pair< bool, bytes > __eth_registerPrecompiledFunction##Name

#define ETH_REGISTER_PRECOMPILED_PRICER( Name )                                                  \
    static bigint __eth_registerPricerFunction##Name(                                            \
        bytesConstRef _in, ChainOperationParams const& _chainParams, u256 const& _blockNumber ); \
    static PrecompiledPricer __eth_registerPricerFactory##Name =                                 \
        ::dev::eth::PrecompiledRegistrar::registerPricer(                                        \
            #Name, &__eth_registerPricerFunction##Name );                                        \
    static bigint __eth_registerPricerFunction##Name

static constexpr size_t UINT256_SIZE = 32;

}  // namespace eth
}  // namespace dev
