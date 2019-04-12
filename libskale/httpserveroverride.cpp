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
 * @file httpserveroverride.cpp
 * @author Dima Litvinov
 * @date 2018
 */

#include "httpserveroverride.h"

#include <libdevcore/microprofile.h>

#include <libdevcore/Log.h>

#include <jsonrpccpp/common/specificationparser.h>

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sstream>

#include <arpa/inet.h>

struct mhd_coninfo {
    struct MHD_PostProcessor* postprocessor;
    MHD_Connection* connection;
    std::stringstream request;
    HttpServerOverride* server;
    int code;
};

HttpServerOverride::HttpServerOverride( const std::string& address_, int port_ )
    : AbstractServerConnector(),
      address( address_ ),
      port( port_ ),
      threads( 16 ),
      running( false ),
      daemon( NULL ),
      bTraceHttpCalls( false ) {}

jsonrpc::IClientConnectionHandler* HttpServerOverride::GetHandler( const std::string& url ) {
    if ( jsonrpc::AbstractServerConnector::GetHandler() != NULL )
        return AbstractServerConnector::GetHandler();
    std::map< std::string, jsonrpc::IClientConnectionHandler* >::iterator it =
        this->urlhandler.find( url );
    if ( it != this->urlhandler.end() )
        return it->second;
    return NULL;
}


bool HttpServerOverride::StartListening() {
    if ( !this->running ) {
        const bool has_epoll = ( MHD_is_feature_supported( MHD_FEATURE_EPOLL ) == MHD_YES );
        const bool has_poll = ( MHD_is_feature_supported( MHD_FEATURE_POLL ) == MHD_YES );
        unsigned int mhd_flags = 0;
        if ( has_epoll )
// In MHD version 0.9.44 the flag is renamed to
// MHD_USE_EPOLL_INTERNALLY_LINUX_ONLY. In later versions both
// are deprecated.
#if defined( MHD_USE_EPOLL_INTERNALLY )
            mhd_flags = MHD_USE_EPOLL_INTERNALLY;
#else
            mhd_flags = MHD_USE_EPOLL_INTERNALLY_LINUX_ONLY;
#endif
        else if ( has_poll )
            mhd_flags = MHD_USE_POLL_INTERNALLY;
        else
            assert( false );

        sockaddr_in sain;
        sain.sin_family = AF_INET;
        sain.sin_port = htons( ( uint16_t ) this->port );
        sain.sin_addr.s_addr = inet_addr( address.c_str() );

        this->daemon = MHD_start_daemon( mhd_flags, this->port, NULL, NULL,
            HttpServerOverride::callback, this, MHD_OPTION_THREAD_POOL_SIZE, this->threads,
            MHD_OPTION_SOCK_ADDR, &sain, MHD_OPTION_END );

        if ( this->daemon != NULL )
            this->running = true;
    }
    return this->running;
}

bool HttpServerOverride::StopListening() {
    if ( this->running ) {
        MHD_stop_daemon( this->daemon );
        this->running = false;
    }
    return true;
}

bool HttpServerOverride::SendResponse( const std::string& response, void* addInfo ) {
    MICROPROFILE_SCOPEI( "HttpServerOverride", "SendResponse", MP_MEDIUMBLUE );
    struct mhd_coninfo* client_connection = static_cast< struct mhd_coninfo* >( addInfo );
    struct MHD_Response* result = MHD_create_response_from_buffer(
        response.size(), ( void* ) response.c_str(), MHD_RESPMEM_MUST_COPY );

    MHD_add_response_header( result, "Content-Type", "application/json" );
    MHD_add_response_header( result, "Access-Control-Allow-Origin", "*" );

    int ret = MHD_queue_response( client_connection->connection, client_connection->code, result );
    MHD_destroy_response( result );
    return ret == MHD_YES;
}

bool HttpServerOverride::SendOptionsResponse( void* addInfo ) {
    struct mhd_coninfo* client_connection = static_cast< struct mhd_coninfo* >( addInfo );
    struct MHD_Response* result = MHD_create_response_from_buffer( 0, NULL, MHD_RESPMEM_MUST_COPY );

    MHD_add_response_header( result, "Allow", "POST, OPTIONS" );
    MHD_add_response_header( result, "Access-Control-Allow-Origin", "*" );
    MHD_add_response_header(
        result, "Access-Control-Allow-Headers", "origin, content-type, accept" );
    MHD_add_response_header( result, "DAV", "1" );

    int ret = MHD_queue_response( client_connection->connection, client_connection->code, result );
    MHD_destroy_response( result );
    return ret == MHD_YES;
}

int HttpServerOverride::callback( void* cls, struct MHD_Connection* connection, const char* url,
    const char* method, const char* version, const char* upload_data, size_t* upload_data_size,
    void** con_cls ) {
    MICROPROFILE_SCOPEI( "HttpServerOverride", "callback", MP_SLATEBLUE );
    ( void ) version;
    if ( *con_cls == NULL ) {
        struct mhd_coninfo* client_connection = new mhd_coninfo;
        client_connection->connection = connection;
        client_connection->server = static_cast< HttpServerOverride* >( cls );
        *con_cls = client_connection;
        return MHD_YES;
    }
    struct mhd_coninfo* client_connection = static_cast< struct mhd_coninfo* >( *con_cls );

    if ( std::string( "POST" ) == method )
        try {
            if ( *upload_data_size != 0 ) {
                client_connection->request.write( upload_data, *upload_data_size );
                *upload_data_size = 0;
                return MHD_YES;
            } else {
                std::string response;
                jsonrpc::IClientConnectionHandler* handler =
                    client_connection->server->GetHandler( std::string( url ) );
                if ( handler == NULL ) {
                    client_connection->code = MHD_HTTP_INTERNAL_SERVER_ERROR;
                    client_connection->server->SendResponse(
                        "No client connection handler found", client_connection );
                } else {
                    client_connection->code = MHD_HTTP_OK;
                    if ( static_cast< HttpServerOverride* >( cls )->bTraceHttpCalls ) {
                        std::cout << "HTTP request: " << client_connection->request.str() << "\n";
                        std::cout.flush();
                    }
                    clog( dev::VerbosityTrace, "rpc" ) << client_connection->request.str();
                    handler->HandleRequest( client_connection->request.str(), response );
                    clog( dev::VerbosityTrace, "rpc" ) << response;
                    if ( static_cast< HttpServerOverride* >( cls )->bTraceHttpCalls ) {
                        std::cout << "HTTP responce: " << response << "\n";
                        std::cout.flush();
                    }
                    client_connection->server->SendResponse( response, client_connection );
                }
            }
        } catch ( const std::exception& ex ) {
            cerror << "CRITICAL " << ex.what() << " in HttpServerOverride";
            client_connection->code = MHD_HTTP_INTERNAL_SERVER_ERROR;
            client_connection->server->SendResponse( ex.what(), client_connection );
        } catch ( ... ) {
            cerror << "CRITICAL unknown exception in HttpServerOverride";
            client_connection->code = MHD_HTTP_INTERNAL_SERVER_ERROR;
            client_connection->server->SendResponse(
                "unknown exception in HttpServerOverride", client_connection );
        }  // catch
    else if ( std::string( "OPTIONS" ) == method ) {
        client_connection->code = MHD_HTTP_OK;
        client_connection->server->SendOptionsResponse( client_connection );
    } else {
        client_connection->code = MHD_HTTP_METHOD_NOT_ALLOWED;
        client_connection->server->SendResponse( "Not allowed HTTP Method", client_connection );
    }
    delete client_connection;
    *con_cls = NULL;

    return MHD_YES;
}
