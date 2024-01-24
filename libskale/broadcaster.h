/*
    Copyright (C) 2019-present, SKALE Labs

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
 * @file broadcaster.h
 * @author Dima Litvinov
 * @date 2019
 */

#ifndef BROADCASTER_H
#define BROADCASTER_H


#include <libethereum/ChainParams.h>

#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace dev {
namespace eth {
class Client;
}
}  // namespace dev

class SkaleClient;
class SkaleHost;

class Broadcaster {
public:
    class StartupException : public std::runtime_error {
    public:
        StartupException( const std::string& what ) : std::runtime_error( what ) {}
    };

    Broadcaster() {}
    virtual ~Broadcaster();

    virtual void broadcast( const std::vector< std::string >& _rlps ) = 0;

    virtual void startService() = 0;
    virtual void stopService() = 0;
};

class HttpBroadcaster : public Broadcaster {
public:
    HttpBroadcaster( dev::eth::Client& _client );
    virtual ~HttpBroadcaster() {}

    virtual void broadcast( const std::vector< std::string >& _rlps );
    virtual void startService() {}
    virtual void stopService() {}

private:
    dev::eth::Client& m_client;
    std::vector< std::shared_ptr< SkaleClient > > m_nodeClients;

    void initClients( dev::eth::SChain, dev::eth::NodeInfo );
    std::string getHttpUrl( const dev::eth::sChainNode& );
};

class ZmqBroadcaster : public Broadcaster {
public:
    ZmqBroadcaster( dev::eth::Client& _client, SkaleHost& _skaleHost );
    virtual ~ZmqBroadcaster();

    virtual void broadcast( const std::vector< std::string >& _rlps );

    virtual void startService();
    virtual void stopService();

private:
    dev::eth::Client& m_client;
    SkaleHost& m_skaleHost;

    void* m_zmq_context;
    mutable void* m_zmq_server_socket;
    mutable void* m_zmq_client_socket;

    std::string getZmqUrl( const dev::eth::sChainNode& ) const;
    void* server_socket() const;
    void* client_socket() const;

    // threading
    std::atomic_bool m_need_exit;
    std::thread m_thread;
};

#endif  // BROADCASTER_H
