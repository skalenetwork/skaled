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

#include <libdevcore/Address.h>
#include <libdevcore/CommonData.h>
#include <libdevcore/Exceptions.h>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <json.hpp>

#include <skutils/multithreading.h>
#include <skutils/utils.h>

class SkaleHost;

namespace skale {
#ifndef FAIR
class OverlayFS;
#endif
}  // namespace skale

namespace dev {
namespace eth {

extern std::shared_ptr< skutils::json_config_file_accessor > g_configAccesssor;
extern std::shared_ptr< SkaleHost > g_skaleHost;

struct PrecompiledCallContext {
    PrecompiledCallContext()
        : blockNumber( 0 ),
          latestBlockTimestamp( 0 ),
#ifdef BITE
          currentTxnIndex( -1 ),
          currentTxnHash( dev::h256( 0 ) ),
          from( dev::ZeroAddress ),
#endif
          isReadOnly( true ) {
    }
    PrecompiledCallContext( const dev::u256& _bn, int64_t _latestBlockTimestamp,
#ifdef BITE
        const dev::u256& _currentTxnIndex, const dev::h256& _currentTxnHash,
        const dev::Address& _from,
#endif
        bool _readOnly )
        : blockNumber( _bn ),
          latestBlockTimestamp( _latestBlockTimestamp ),
#ifdef BITE
          currentTxnIndex( _currentTxnIndex ),
          currentTxnHash( _currentTxnHash ),
          from( _from ),
#endif
          isReadOnly( _readOnly ) {
    }
#ifdef BITE
    // Convenience 3-arg constructor for BITE builds (BITE fields use defaults)
    PrecompiledCallContext( const dev::u256& _bn, int64_t _latestBlockTimestamp, bool _readOnly )
        : blockNumber( _bn ),
          latestBlockTimestamp( _latestBlockTimestamp ),
          currentTxnIndex( -1 ),
          from( dev::ZeroAddress ),
          isReadOnly( _readOnly ) {}
#endif
    dev::u256 blockNumber;
    int64_t latestBlockTimestamp;
#ifdef BITE
    dev::u256 currentTxnIndex;
    dev::h256 currentTxnHash;
    dev::Address from;
#endif
    bool isReadOnly;
};

inline PrecompiledCallContext defaultPrecompiledContext = { 1, 0,
#ifdef BITE
    0, dev::h256( 0 ), dev::ZeroAddress,
#endif
    true };

struct ChainOperationParams;

// allow call both with overlayFS and without it
class PrecompiledExecutor {
public:
#ifdef FAIR
    std::pair< bool, bytes > operator()(
        bytesConstRef _in, const PrecompiledCallContext& _ctx ) const {
        return proxy( _in, _ctx );
    }
#else
    std::pair< bool, bytes > operator()( bytesConstRef _in, const PrecompiledCallContext& _ctx,
        skale::OverlayFS* _overlayFS = nullptr ) const {
        return proxy( _in, _ctx, _overlayFS );
    }
#endif
    PrecompiledExecutor() {}
#ifdef FAIR
    PrecompiledExecutor( const std::function< std::pair< bool, bytes >(
            bytesConstRef _in, const PrecompiledCallContext& _ctx ) >& _func )
        : proxy( _func ) {}
#else
    PrecompiledExecutor( const std::function< std::pair< bool, bytes >( bytesConstRef _in,
            const PrecompiledCallContext& _ctx, skale::OverlayFS* _overlayFS ) >& _func )
        : proxy( _func ) {}
#endif

private:
#ifdef FAIR
    std::function< std::pair< bool, bytes >(
        bytesConstRef _in, const PrecompiledCallContext& _ctx ) >
        proxy;
#else
    std::function< std::pair< bool, bytes >(
        bytesConstRef _in, const PrecompiledCallContext& _ctx, skale::OverlayFS* _overlayFS ) >
        proxy;
#endif
};

using PrecompiledPricer = std::function< bigint( bytesConstRef _in,
    ChainOperationParams const& _chainParams, PrecompiledCallContext const& _ctx ) >;

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

#ifdef FAIR
#define ETH_REGISTER_PRECOMPILED( Name )                                                      \
    static std::pair< bool, bytes > __eth_registerPrecompiledFunction##Name(                  \
        bytesConstRef _in, const PrecompiledCallContext& _ctx );                              \
    static PrecompiledExecutor __eth_registerPrecompiledFactory##Name =                       \
        ::dev::eth::PrecompiledRegistrar::registerExecutor(                                   \
            #Name, PrecompiledExecutor(                                                       \
                       []( bytesConstRef _in,                                                 \
                           const PrecompiledCallContext& _ctx ) -> std::pair< bool, bytes > { \
                           return __eth_registerPrecompiledFunction##Name( _in, _ctx );       \
                       } ) );                                                                 \
    static std::pair< bool, bytes > __eth_registerPrecompiledFunction##Name
#else
// ignore _overlayFS param and call registered function with 2 parameters
// TODO: unregister on unload with a static object.
#define ETH_REGISTER_PRECOMPILED( Name )                                                           \
    static std::pair< bool, bytes > __eth_registerPrecompiledFunction##Name(                       \
        bytesConstRef _in, const PrecompiledCallContext& _ctx );                                   \
    static PrecompiledExecutor __eth_registerPrecompiledFactory##Name =                            \
        ::dev::eth::PrecompiledRegistrar::registerExecutor(                                        \
            #Name, PrecompiledExecutor( []( bytesConstRef _in, const PrecompiledCallContext& _ctx, \
                                            skale::OverlayFS* ) -> std::pair< bool, bytes > {      \
                return __eth_registerPrecompiledFunction##Name( _in, _ctx );                       \
            } ) );                                                                                 \
    static std::pair< bool, bytes > __eth_registerPrecompiledFunction##Name

#define ETH_REGISTER_FS_PRECOMPILED( Name )                                                    \
    static std::pair< bool, bytes > __eth_registerPrecompiledFunction##Name(                   \
        bytesConstRef _in, const PrecompiledCallContext& _ctx, skale::OverlayFS* _overlayFS ); \
    static PrecompiledExecutor __eth_registerPrecompiledFactory##Name =                        \
        ::dev::eth::PrecompiledRegistrar::registerExecutor(                                    \
            #Name, PrecompiledExecutor( &__eth_registerPrecompiledFunction##Name ) );          \
    static std::pair< bool, bytes > __eth_registerPrecompiledFunction##Name
#endif

#define ETH_REGISTER_PRECOMPILED_PRICER( Name )                                         \
    static bigint __eth_registerPricerFunction##Name( bytesConstRef _in,                \
        ChainOperationParams const& _chainParams, PrecompiledCallContext const& _ctx ); \
    static PrecompiledPricer __eth_registerPricerFactory##Name =                        \
        ::dev::eth::PrecompiledRegistrar::registerPricer(                               \
            #Name, &__eth_registerPricerFunction##Name );                               \
    static bigint __eth_registerPricerFunction##Name

static constexpr size_t UINT256_SIZE = 32;

}  // namespace eth
}  // namespace dev
