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
 * @file broadcaster.cpp
 * @author Dima Litvinov
 * @date 2019
 */

#include "broadcaster.h"

#include <libethereum/Client.h>
#include <libethereum/SkaleHost.h>
#include <libskale/SkaleClient.h>

#include <jsonrpccpp/client/connectors/httpclient.h>

#include <zmq.h>

#include <string>

Broadcaster::~Broadcaster() {}

HttpBroadcaster::HttpBroadcaster( dev::eth::Client& _client ) : m_client( _client ) {
    const dev::eth::ChainParams& ch = _client.chainParams();
    initClients( ch.sChain, ch.nodeInfo );
}

void HttpBroadcaster::initClients( dev::eth::SChain sChain, dev::eth::NodeInfo nodeInfo ) {
    for ( const auto& node : sChain.nodes ) {
        if ( nodeInfo.id == node.id ) {
            continue;
        }
        auto c = new jsonrpc::HttpClient( getHttpUrl( node ) );

        auto skaleClient = std::make_shared< SkaleClient >( *c );
        m_nodeClients.push_back( skaleClient );
    }
}

std::string HttpBroadcaster::getHttpUrl( const dev::eth::sChainNode& node ) {
    std::string url =
        "http://" + node.ip + ":" + ( node.port + 3 ).str();  // HACK +0 +1 +2 are used by consensus
    std::cout << url << std::endl;                            // todo
    return url;
}

void HttpBroadcaster::broadcast( const std::string& _rlp ) {
    if ( _rlp.empty() )
        return;

    for ( const auto& node : m_nodeClients ) {
        node->skale_receiveTransaction( _rlp );
    }
}

/////////////////////////////////////////////////////////////////////////

ZmqBroadcaster::ZmqBroadcaster( dev::eth::Client& _client, SkaleHost& _skaleHost )
    : m_client( _client ),
      m_skaleHost( _skaleHost ),
      m_zmq_server_socket( nullptr ),
      m_zmq_client_socket( nullptr ),
      m_need_exit( false ) {
    m_zmq_context = zmq_ctx_new();

    startService();
}

std::string ZmqBroadcaster::getZmqUrl( const dev::eth::sChainNode& node ) const {
    std::string url = "tcp://" + node.ip + ":" + ( node.port + 5 ).str();  // HACK +5
    std::cout << url << std::endl;                                         // todo
    return url;
}

void* ZmqBroadcaster::server_socket() const {
    if ( !m_zmq_server_socket ) {
        m_zmq_server_socket = zmq_socket( m_zmq_context, ZMQ_PUB );

        const dev::eth::ChainParams& ch = m_client.chainParams();

        // connect server to clients
        for ( const auto& node : ch.sChain.nodes ) {
            if ( node.id == ch.nodeInfo.id )
                continue;
            int res = zmq_connect( m_zmq_server_socket, getZmqUrl( node ).c_str() );
            if ( res != 0 ) {
                throw std::runtime_error( "Zmq can't connect" );
            }
        }
        sleep( 1 );  // HACK to overcome zmq "slow joiner". see SKALE-742
    }
    return m_zmq_server_socket;
}

void* ZmqBroadcaster::client_socket() const {
    if ( !m_zmq_client_socket ) {
        m_zmq_client_socket = zmq_socket( m_zmq_context, ZMQ_SUB );

        const dev::eth::ChainParams& ch = m_client.chainParams();

        // start listen as client
        std::string listen_addr =
            "tcp://" + ch.nodeInfo.ip + ":" + std::to_string( ch.nodeInfo.port + 5 );
        int res = zmq_bind( m_zmq_client_socket, listen_addr.c_str() );
        if ( res ) {
            throw StartupException(
                "ZmqBroadcaster: " + std::string( zmq_strerror( errno ) ) + " " + listen_addr );
        }  // if error

        res = zmq_setsockopt( m_zmq_client_socket, ZMQ_SUBSCRIBE, "", 0 );
        assert( res == 0 );

        sleep( 1 );  // HACK to overcome zmq "slow joiner". see SKALE-742
    }
    return m_zmq_client_socket;
}

ZmqBroadcaster::~ZmqBroadcaster() {
    if ( m_thread.joinable() )
        stopService();
}

void ZmqBroadcaster::startService() {
    assert( !m_thread.joinable() );

    int timeo = 100;  // 100 milliseconds
    int res = zmq_setsockopt( client_socket(), ZMQ_RCVTIMEO, &timeo, sizeof( timeo ) );
    if ( res != 0 ) {
        throw runtime_error( "zmq_setsockopt has failed" );
    }

    auto func = [this]() {
        setThreadName( "ZmqBroadcaster" );

        while ( true ) {
            zmq_msg_t msg;

            try {
                int res = zmq_msg_init( &msg );
                assert( res == 0 );
                res = zmq_msg_recv( &msg, client_socket(), 0 );

                if ( m_need_exit ) {
                    zmq_msg_close( &msg );
                    int linger = 1;
                    zmq_setsockopt( client_socket(), ZMQ_LINGER, &linger, sizeof( linger ) );
                    zmq_close( client_socket() );
                    break;
                }

                if ( res < 0 && errno == EAGAIN ) {
                    zmq_msg_close( &msg );
                    continue;
                }

                if ( res < 0 ) {
                    clog( dev::VerbosityWarning, "skale-host" )
                        << "Received bad message on ZmqBroadcaster port. errno = " << errno;
                    continue;
                }

                size_t size = zmq_msg_size( &msg );
                void* data = zmq_msg_data( &msg );

                std::string str( static_cast< char* >( data ), size );

                try {
                    m_skaleHost.receiveTransaction( str );
                } catch ( const std::exception& ex ) {
                    clog( dev::VerbosityInfo, "skale-host" )
                        << "Received bad transaction through broadcast: " << ex.what();
                }

            } catch ( const std::exception& ex ) {
                cerror << "CRITICAL " << ex.what() << " (restarting ZmqBroadcaster)";
                sleep( 2 );
            } catch ( ... ) {
                cerror << "CRITICAL unknown exception (restarting ZmqBroadcaster)";
                sleep( 2 );
            }

            zmq_msg_close( &msg );
        }  // while
    };

    m_thread = std::thread( func );
}

// HACK this should be called strictly from thread that calls broadcast()
void ZmqBroadcaster::stopService() {
    assert( !m_need_exit );
    assert( m_thread.joinable() );

    int linger = 1;
    zmq_setsockopt( server_socket(), ZMQ_LINGER, &linger, sizeof( linger ) );
    zmq_close( server_socket() );

    m_need_exit = true;
    zmq_ctx_term( m_zmq_context );
    m_thread.join();
}

void ZmqBroadcaster::broadcast( const std::string& _rlp ) {
    if ( _rlp.empty() ) {
        server_socket();
        return;
    }

    int res = zmq_send( server_socket(), const_cast< char* >( _rlp.c_str() ), _rlp.size(), 0 );
    if ( res <= 0 ) {
        throw runtime_error( "Zmq can't send data" );
    }
}
