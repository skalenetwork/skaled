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
 * @file httpserveroverride.h
 * @author Dima Litvinov
 * @date 2018
 */

#ifndef HTTPSERVEROVERRIDE_H
#define HTTPSERVEROVERRIDE_H

#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>
#if defined( _WIN32 ) && !defined( __CYGWIN__ )
#include <ws2tcpip.h>
#if defined( _MSC_FULL_VER ) && !defined( _SSIZE_T_DEFINED )
#define _SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#endif  // !_SSIZE_T_DEFINED */
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <jsonrpccpp/server/abstractserverconnector.h>
#include <microhttpd.h>
#include <map>

class HttpServerOverride : public jsonrpc::AbstractServerConnector {
public:
    HttpServerOverride( const std::string& address, int port );
    virtual bool StartListening() override;
    virtual bool StopListening() override;

    bool virtual SendResponse( const std::string& response, void* addInfo = NULL );
    bool virtual SendOptionsResponse( void* addInfo );

    void SetUrlHandler( const std::string& url, jsonrpc::IClientConnectionHandler* handler );

private:
    const std::string address;

    static int callback( void* cls, struct MHD_Connection* connection, const char* url,
        const char* method, const char* version, const char* upload_data, size_t* upload_data_size,
        void** con_cls );

    // inherited:
    int port;
    int threads;
    bool running;

    struct MHD_Daemon* daemon;
    std::map< std::string, jsonrpc::IClientConnectionHandler* > urlhandler;
    jsonrpc::IClientConnectionHandler* GetHandler( const std::string& url );

public:
    bool bTraceHttpCalls;
};

#endif  // HTTPSERVEROVERRIDE_H
