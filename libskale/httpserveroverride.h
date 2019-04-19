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
#include <memory>
#include <string>

#include <skutils/console_colors.h>
#include <skutils/http.h>
#include <skutils/utils.h>
#include <skutils/ws.h>
#include <json.hpp>

class SkaleWsPeer;
class SkaleWsRelay;
class SkaleServerOverride;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SkaleWsPeer : public skutils::ws::peer {
public:
    std::string strPeerQueueID_;
    SkaleWsPeer( skutils::ws::server& srv, const skutils::ws::hdl_t& hdl );
    ~SkaleWsPeer() override;
    void onPeerRegister() override;
    void onPeerUnregister() override;  // peer will no longer receive onMessage after call to this
    void onMessage( const std::string& msg, skutils::ws::opcv eOpCode ) override;
    void onClose( const std::string& reason, int local_close_code,
        const std::string& local_close_code_as_str ) override;
    void onFail() override;
    void onLogMessage(
        skutils::ws::e_ws_log_message_type_t eWSLMT, const std::string& msg ) override;

    std::string desc( bool isColored = true ) const {
        return getShortPeerDescription( isColored, false, false );
    }
    SkaleWsRelay& getRelay();
    const SkaleWsRelay& getRelay() const { return const_cast< SkaleWsPeer* >( this )->getRelay(); }
    SkaleServerOverride* pso();
    const SkaleServerOverride* pso() const { return const_cast< SkaleWsPeer* >( this )->pso(); }
    friend class SkaleWsRelay;
};  /// class SkaleWsPeer

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SkaleWsRelay : public skutils::ws::server {
protected:
    volatile bool isRunning_ = false;
    volatile bool isInLoop_ = false;
    std::string scheme_;
    std::string scheme_uc_;
    int nPort_ = -1;
    SkaleServerOverride* pso_ = nullptr;

public:
    SkaleWsRelay( const char* strScheme,  // "ws" or "wss"
        int nPort );
    ~SkaleWsRelay() override;
    void run( skutils::ws::fn_continue_status_flag_t fnContinueStatusFlag );
    bool isRunning() const { return isRunning_; }
    bool isInLoop() const { return isInLoop_; }
    void waitWhileInLoop();
    bool start( SkaleServerOverride* pso );
    void stop();
    SkaleServerOverride* pso() { return pso_; }
    const SkaleServerOverride* pso() const { return pso_; }
    friend class SkaleWsPeer;
};  /// class SkaleWsRelay

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SkaleServerOverride : public jsonrpc::AbstractServerConnector {
public:
    SkaleServerOverride( const std::string& http_addr, int http_port,
        const std::string& web_socket_addr, int web_socket_port, const std::string& pathSslKey = "",
        const std::string& pathSslCert = "" );
    ~SkaleServerOverride() override;

private:
    bool startListeningHTTP();
    bool startListeningWebSocket();
    bool stopListeningHTTP();
    bool stopListeningWebSocket();

public:
    virtual bool StartListening() override;
    virtual bool StopListening() override;

    void SetUrlHandler( const std::string& url, jsonrpc::IClientConnectionHandler* handler );

private:
    void logTraceServerEvent(
        bool isError, const char* strProtocol, const std::string& strMessage );
    void logTraceServerTraffic( bool isRX, bool isError, const char* strProtocol,
        const char* strOrigin, const std::string& strPayload );
    const std::string address_http_, address_web_socket_;
    int port_http_, port_web_socket_;

    std::map< std::string, jsonrpc::IClientConnectionHandler* > urlhandler;
    jsonrpc::IClientConnectionHandler* GetHandler( const std::string& url );

public:
    bool bTraceCalls_;

private:
    std::shared_ptr< skutils::http::server > pServerHTTP_;
    std::string pathSslKey_, pathSslCert_;
    bool bIsSSL_;

    std::shared_ptr< SkaleWsRelay > pServerWS_;

public:
    bool isSSL() const { return bIsSSL_; }

    friend class SkaleWsRelay;
    friend class SkaleWsPeer;
};  /// class SkaleServerOverride

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif  ///(!defined __HTTP_SERVER_OVERRIDE_H)
