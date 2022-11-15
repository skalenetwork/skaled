/*
    Modifications Copyright (C) 2018 SKALE Labs

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
/** @file Network.cpp
 * @author Alex Leverington <nessence@gmail.com>
 * @author Gav Wood <i@gavwood.com>
 * @author Eric Lombrozo <elombrozo@gmail.com> (Windows version of getInterfaceAddresses())
 * @date 2014
 */

#include <sys/types.h>
#ifndef _WIN32
#include <ifaddrs.h>
#endif

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include <libdevcore/Assertions.h>
#include <libdevcore/Common.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/Exceptions.h>

#include "Network.h"
#include "UPnP.h"

using namespace std;
using namespace dev;
using namespace dev::p2p;

namespace dev {
namespace p2p {
#define NET_GLOBAL_LOGGER( NAME, SEVERITY )                     \
    BOOST_LOG_INLINE_GLOBAL_LOGGER_CTOR_ARGS( g_##NAME##Logger, \
        boost::log::sources::severity_channel_logger_mt<>,      \
        ( boost::log::keywords::severity = SEVERITY )( boost::log::keywords::channel = "net" ) )

NET_GLOBAL_LOGGER( netnote, VerbosityInfo )
#define cnetnote LOG( dev::p2p::g_netnoteLogger::get() )
NET_GLOBAL_LOGGER( netlog, VerbosityDebug )
#define cnetlog LOG( dev::p2p::g_netlogLogger::get() )
NET_GLOBAL_LOGGER( netdetails, VerbosityTrace )
#define cnetdetails LOG( dev::p2p::g_netdetailsLogger::get() )

const unsigned c_defaultIPPort = 30303;
}  // namespace p2p
}  // namespace dev
static_assert( BOOST_VERSION >= 106400, "Wrong boost headers version" );

std::set< bi::address > Network::getInterfaceAddresses() {
    std::set< bi::address > addresses;

#if defined( _WIN32 )
    WSAData wsaData;
    if ( WSAStartup( MAKEWORD( 1, 1 ), &wsaData ) != 0 )
        BOOST_THROW_EXCEPTION( NoNetworking() );

    char ac[80];
    if ( gethostname( ac, sizeof( ac ) ) == SOCKET_ERROR ) {
        cnetlog << "Error " << WSAGetLastError() << " when getting local host name.";
        WSACleanup();
        BOOST_THROW_EXCEPTION( NoNetworking() );
    }

    struct hostent* phe = gethostbyname( ac );
    if ( phe == 0 ) {
        cnetlog << "Bad host lookup.";
        WSACleanup();
        BOOST_THROW_EXCEPTION( NoNetworking() );
    }

    for ( int i = 0; phe->h_addr_list[i] != 0; ++i ) {
        struct in_addr addr;
        memcpy( &addr, phe->h_addr_list[i], sizeof( struct in_addr ) );
        char* addrStr = inet_ntoa( addr );
        bi::address address( bi::address::from_string( addrStr ) );
        if ( !isLocalHostAddress( address ) )
            addresses.insert( address.to_v4() );
    }

    WSACleanup();
#else
    ifaddrs* ifaddr;
    if ( getifaddrs( &ifaddr ) == -1 )
        BOOST_THROW_EXCEPTION( NoNetworking() );

    for ( auto ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next ) {
        if ( !ifa->ifa_addr || string( ifa->ifa_name ) == "lo0" || !( ifa->ifa_flags & IFF_UP ) )
            continue;

        if ( ifa->ifa_addr->sa_family == AF_INET ) {
            in_addr addr = ( ( struct sockaddr_in* ) ifa->ifa_addr )->sin_addr;
            boost::asio::ip::address_v4 address(
                boost::asio::detail::socket_ops::network_to_host_long( addr.s_addr ) );
            if ( !isLocalHostAddress( address ) )
                addresses.insert( address );
        } else if ( ifa->ifa_addr->sa_family == AF_INET6 ) {
            sockaddr_in6* sockaddr = ( ( struct sockaddr_in6* ) ifa->ifa_addr );
            in6_addr addr = sockaddr->sin6_addr;
            boost::asio::ip::address_v6::bytes_type bytes;
            memcpy( &bytes[0], addr.s6_addr, 16 );
            boost::asio::ip::address_v6 address( bytes, sockaddr->sin6_scope_id );
            if ( !isLocalHostAddress( address ) )
                addresses.insert( address );
        }
    }

    if ( ifaddr != NULL )
        freeifaddrs( ifaddr );

#endif

    return addresses;
}

bool p2p::isPublicAddress( std::string const& _addressToCheck ) {
    return _addressToCheck.empty() ? false :
                                     isPublicAddress( bi::address::from_string( _addressToCheck ) );
}

bool p2p::isPublicAddress( bi::address const& _addressToCheck ) {
    return !( isPrivateAddress( _addressToCheck ) || isLocalHostAddress( _addressToCheck ) );
}

// Helper function to determine if an address falls within one of the reserved ranges
// For V4:
// Class A "10.*", Class B "172.[16->31].*", Class C "192.168.*"
bool p2p::isPrivateAddress( bi::address const& _addressToCheck ) {
    if ( _addressToCheck.is_v4() ) {
        bi::address_v4 v4Address = _addressToCheck.to_v4();
        bi::address_v4::bytes_type bytesToCheck = v4Address.to_bytes();
        if ( bytesToCheck[0] == 10 || bytesToCheck[0] == 127 )
            return true;
        if ( bytesToCheck[0] == 172 && ( bytesToCheck[1] >= 16 && bytesToCheck[1] <= 31 ) )
            return true;
        if ( bytesToCheck[0] == 192 && bytesToCheck[1] == 168 )
            return true;
    } else if ( _addressToCheck.is_v6() ) {
        bi::address_v6 v6Address = _addressToCheck.to_v6();
        bi::address_v6::bytes_type bytesToCheck = v6Address.to_bytes();
        if ( bytesToCheck[0] == 0xfd && bytesToCheck[1] == 0 )
            return true;
        if ( !bytesToCheck[0] && !bytesToCheck[1] && !bytesToCheck[2] && !bytesToCheck[3] &&
             !bytesToCheck[4] && !bytesToCheck[5] && !bytesToCheck[6] && !bytesToCheck[7] &&
             !bytesToCheck[8] && !bytesToCheck[9] && !bytesToCheck[10] && !bytesToCheck[11] &&
             !bytesToCheck[12] && !bytesToCheck[13] && !bytesToCheck[14] &&
             ( bytesToCheck[15] == 0 || bytesToCheck[15] == 1 ) )
            return true;
    }
    return false;
}

bool p2p::isPrivateAddress( std::string const& _addressToCheck ) {
    return _addressToCheck.empty() ?
               false :
               isPrivateAddress( bi::address::from_string( _addressToCheck ) );
}

// Helper function to determine if an address is localhost
bool p2p::isLocalHostAddress( bi::address const& _addressToCheck ) {
    // @todo: ivp6 link-local adresses (macos), ex: fe80::1%lo0
    static const set< bi::address > c_rejectAddresses = {
        { bi::address_v4::from_string( "127.0.0.1" ) },
        { bi::address_v4::from_string( "0.0.0.0" ) }, { bi::address_v6::from_string( "::1" ) },
        { bi::address_v6::from_string( "::" ) } };

    return find( c_rejectAddresses.begin(), c_rejectAddresses.end(), _addressToCheck ) !=
           c_rejectAddresses.end();
}

bool p2p::isLocalHostAddress( std::string const& _addressToCheck ) {
    return _addressToCheck.empty() ?
               false :
               isLocalHostAddress( bi::address::from_string( _addressToCheck ) );
}

int Network::tcp4Listen( bi::tcp::acceptor& _acceptor, NetworkPreferences const& _netPrefs ) {
    // Due to the complexities of NAT and network environments (multiple NICs, tunnels, etc)
    // and security concerns automation is the enemy of network configuration.
    // If a preference cannot be accommodate the network must fail to start.
    //
    // Preferred IP: Attempt if set, else, try 0.0.0.0 (all interfaces)
    // Preferred Port: Attempt if set, else, try c_defaultListenPort or 0 (random)
    // TODO: throw instead of returning -1 and rename NetworkPreferences to NetworkConfig

    bi::address listenIP;
    try {
        listenIP = _netPrefs.listenIPAddress.empty() ?
                       bi::address_v4() :
                       bi::address::from_string( _netPrefs.listenIPAddress );
    } catch ( ... ) {
        cwarn << "Couldn't start accepting connections on host. Failed to accept socket on "
              << listenIP << ":" << _netPrefs.listenPort << ".\n"
              << boost::current_exception_diagnostic_information();
        return -1;
    }
    bool requirePort = ( bool ) _netPrefs.listenPort;

    for ( unsigned i = 0; i < 2; ++i ) {
        bi::tcp::endpoint endpoint(
            listenIP, requirePort ? _netPrefs.listenPort : ( i ? 0 : c_defaultListenPort ) );
        try {
#if defined( _WIN32 )
            bool reuse = false;
#else
            bool reuse = true;
#endif
            _acceptor.open( endpoint.protocol() );
            _acceptor.set_option( ba::socket_base::reuse_address( reuse ) );
            _acceptor.bind( endpoint );
            _acceptor.listen();
            return _acceptor.local_endpoint().port();
        } catch ( ... ) {
            // bail if this is first attempt && port was specificed, or second attempt failed
            // (random port)
            if ( i || requirePort ) {
                // both attempts failed
                cwarn << "Couldn't start accepting connections on host. Failed to accept socket on "
                      << listenIP << ":" << _netPrefs.listenPort << ".\n"
                      << boost::current_exception_diagnostic_information();
                _acceptor.close();
                return -1;
            }

            _acceptor.close();
            continue;
        }
    }

    return -1;
}

bi::tcp::endpoint Network::traverseNAT( std::set< bi::address > const& _ifAddresses,
    unsigned short _listenPort, bi::address& o_upnpInterfaceAddr ) {
    asserts( _listenPort != 0 );

    unique_ptr< UPnP > upnp;
    try {
        upnp.reset( new UPnP );
    }
    // let m_upnp continue as null - we handle it properly.
    catch ( ... ) {
    }

    bi::tcp::endpoint upnpEP;
    if ( upnp && upnp->isValid() ) {
        bi::address pAddr;
        int extPort = 0;
        for ( auto const& addr : _ifAddresses )
            if ( addr.is_v4() && isPrivateAddress( addr ) &&
                 ( extPort = upnp->addRedirect( addr.to_string().c_str(), _listenPort ) ) ) {
                pAddr = addr;
                break;
            }

        auto eIP = upnp->externalIP();
        bi::address eIPAddr( bi::address::from_string( eIP ) );
        if ( extPort && eIP != string( "0.0.0.0" ) && !isPrivateAddress( eIPAddr ) ) {
            cnetnote << "Punched through NAT and mapped local port " << _listenPort
                     << " onto external port " << extPort << ".";
            cnetnote << "External addr: " << eIP;
            o_upnpInterfaceAddr = pAddr;
            upnpEP = bi::tcp::endpoint( eIPAddr, ( unsigned short ) extPort );
        } else
            cnetlog << "Couldn't punch through NAT (or no NAT in place).";
    }

    return upnpEP;
}

bi::tcp::endpoint Network::resolveHost( string const& _addr ) {
    static boost::asio::io_service s_resolverIoService;

    vector< string > split;
    boost::split( split, _addr, boost::is_any_of( ":" ) );
    unsigned port = dev::p2p::c_defaultIPPort;

    try {
        if ( split.size() > 1 )
            port = static_cast< unsigned >( stoi( split.at( 1 ) ) );
    } catch ( ... ) {
    }

    boost::system::error_code ec;
    bi::address address = bi::address::from_string( split[0], ec );
    bi::tcp::endpoint ep( bi::address(), port );
    if ( !ec )
        ep.address( address );
    else {
        boost::system::error_code ec;
        // resolve returns an iterator (host can resolve to multiple addresses)
        bi::tcp::resolver r( s_resolverIoService );
        auto it = r.resolve( { bi::tcp::v4(), split[0], toString( port ) }, ec );
        if ( ec ) {
            cnetlog << "Error resolving host address... " << _addr << " : " << ec.message();
            return bi::tcp::endpoint();
        } else
            ep = *it;
    }
    return ep;
}
