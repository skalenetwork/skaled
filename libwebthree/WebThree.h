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
/** @file WebThree.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/Exceptions.h>
#include <libdevcore/Guards.h>
#include <libethereum/ChainParams.h>
#include <libethereum/Client.h>
#include <boost/asio.hpp>  // Make sure boost/asio.hpp is included before windows.h.
#include <boost/utility.hpp>
#include <atomic>
#include <list>
#include <mutex>
#include <thread>

namespace dev {

enum WorkState { Active = 0, Deleting, Deleted };

namespace eth {
class Interface;
}
namespace shh {
class Interface;
}
namespace bzz {
class Interface;
class Client;
}  // namespace bzz

/**
 * @brief Main API hub for interfacing with Web 3 components. This doesn't do any local
 * multiplexing, so you can only have one running on any given machine for the provided DB path.
 *
 * Keeps a libp2p Host going (administering the work thread with m_workNet).
 *
 * Encapsulates a bunch of P2P protocols (interfaces), each using the same underlying libp2p Host.
 *
 * Provides a baseline for the multiplexed multi-protocol session class, WebThree.
 */
class WebThreeDirect {
public:
    class CreationException : public std::exception {
        virtual const char* what() const noexcept { return "Error creating WebThreeDirect"; }
    };

    /// Constructor for private instance. If there is already another process on the machine using
    /// @a _dbPath, then this will throw an exception. ethereum() may be safely static_cast()ed to a
    /// eth::Client*.
    WebThreeDirect( std::string const& _clientVersion, boost::filesystem::path const& _dbPath,
        boost::filesystem::path const& _snapshotPath, eth::ChainParams const& _params,
        WithExisting _we = WithExisting::Trust,
        std::set< std::string > const& _interfaces = {"eth", "shh", "bzz"}, bool _testing = false );

    /// Destructor.
    ~WebThreeDirect();

    // The mainline interfaces:

    eth::Client* ethereum() const {
        if ( !m_ethereum )
            BOOST_THROW_EXCEPTION( InterfaceNotSupported() << errinfo_interface( "eth" ) );
        return m_ethereum.get();
    }

    // Misc stuff:

    static std::string composeClientVersion( std::string const& _client );
    std::string const& clientVersion() const { return m_clientVersion; }

private:
    std::string m_clientVersion;  ///< Our end-application client's name/version.

    std::unique_ptr< eth::Client > m_ethereum;  ///< Client for Ethereum ("eth") protocol.
};


}  // namespace dev
