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

#include <stdio.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SkaleWsPeer::SkaleWsPeer( skutils::ws::server& srv, const skutils::ws::hdl_t& hdl )
    : skutils::ws::peer( srv, hdl ) {
    strPeerQueueID_ = skutils::dispatch::generate_id( this, "relay_peer" );
    if ( pso()->bTraceCalls_ )
        clog( dev::VerbosityInfo, cc::info( getRelay().scheme_uc_ ) )
            << desc() << cc::notice( " peer ctor" );
}
SkaleWsPeer::~SkaleWsPeer() {
    if ( pso()->bTraceCalls_ )
        clog( dev::VerbosityInfo, cc::info( getRelay().scheme_uc_ ) )
            << desc() << cc::notice( " peer dctor" );
    skutils::dispatch::remove( strPeerQueueID_ );
}

void SkaleWsPeer::onPeerRegister() {
    if ( pso()->bTraceCalls_ )
        clog( dev::VerbosityInfo, cc::info( getRelay().scheme_uc_ ) )
            << desc() << cc::notice( " peer registered" );
    skutils::ws::peer::onPeerRegister();
}
void SkaleWsPeer::onPeerUnregister() {  // peer will no longer receive onMessage after call to
                                        // this
    if ( pso()->bTraceCalls_ )
        clog( dev::VerbosityInfo, cc::info( getRelay().scheme_uc_ ) )
            << desc() << cc::notice( " peer unregistered" );
    skutils::ws::peer::onPeerUnregister();
}

void SkaleWsPeer::onMessage( const std::string& msg, skutils::ws::opcv eOpCode ) {
    if ( eOpCode != skutils::ws::opcv::text )
        throw std::runtime_error( "only ws text messages are supported" );
    skutils::dispatch::async( strPeerQueueID_, [=]() -> void {
        if ( pso()->bTraceCalls_ )
            clog( dev::VerbosityInfo, cc::info( getRelay().scheme_uc_ ) )
                << cc::ws_rx_inv( " >>> " + getRelay().scheme_uc_ + "/RX >>> " ) << desc()
                << cc::ws_rx( " >>> " ) << cc::j( msg );
        int nID = -1;
        std::string strResponse;
        try {
            nlohmann::json joRequest = nlohmann::json::parse( msg );
            nID = joRequest["id"].get< int >();
            jsonrpc::IClientConnectionHandler* handler = pso()->GetHandler( "/" );
            if ( handler == nullptr )
                throw std::runtime_error( "No client connection handler found" );
            handler->HandleRequest( msg, strResponse );
        } catch ( const std::exception& ex ) {
            clog( dev::VerbosityInfo, cc::info( getRelay().scheme_uc_ ) )
                << cc::ws_tx_inv( " !!! " + getRelay().scheme_uc_ + "/ERR !!! " ) << desc()
                << cc::ws_tx( " !!! " ) << cc::warn( ex.what() );
            nlohmann::json joErrorResponce;
            joErrorResponce["id"] = nID;
            joErrorResponce["result"] = "error";
            joErrorResponce["error"] = std::string( ex.what() );
            strResponse = joErrorResponce.dump();
        } catch ( ... ) {
            const char* e = "unknown exception in SkaleServerOverride";
            clog( dev::VerbosityInfo, cc::info( getRelay().scheme_uc_ ) )
                << cc::ws_tx_inv( " !!! " + getRelay().scheme_uc_ + "/ERR !!! " ) << desc()
                << cc::ws_tx( " !!! " ) << cc::warn( e );
            nlohmann::json joErrorResponce;
            joErrorResponce["id"] = nID;
            joErrorResponce["result"] = "error";
            joErrorResponce["error"] = std::string( e );
            strResponse = joErrorResponce.dump();
        }
        if ( pso()->bTraceCalls_ )
            clog( dev::VerbosityInfo, cc::info( getRelay().scheme_uc_ ) )
                << cc::ws_tx_inv( " <<< " + getRelay().scheme_uc_ + "/TX <<< " ) << desc()
                << cc::ws_tx( " <<< " ) << cc::j( strResponse );
        sendMessage( strResponse );
    } );
    skutils::ws::peer::onMessage( msg, eOpCode );
}

void SkaleWsPeer::onClose(
    const std::string& reason, int local_close_code, const std::string& local_close_code_as_str ) {
    if ( pso()->bTraceCalls_ )
        clog( dev::VerbosityInfo, cc::info( getRelay().scheme_uc_ ) )
            << desc() << cc::warn( " peer close event with code=" ) << cc::c( local_close_code )
            << cc::debug( ", reason=" ) << cc::info( reason ) << "\n";
    skutils::ws::peer::onClose( reason, local_close_code, local_close_code_as_str );
}

void SkaleWsPeer::onFail() {
    if ( pso()->bTraceCalls_ )
        clog( dev::VerbosityError, cc::fatal( getRelay().scheme_uc_ ) )
            << desc() << cc::error( " peer fail event" ) << "\n";
    skutils::ws::peer::onFail();
}

void SkaleWsPeer::onLogMessage(
    skutils::ws::e_ws_log_message_type_t eWSLMT, const std::string& msg ) {
    if ( pso()->bTraceCalls_ )
        clog( dev::VerbosityInfo, cc::info( getRelay().scheme_uc_ ) )
            << desc() << cc::debug( " peer log: " ) << msg << "\n";
    skutils::ws::peer::onLogMessage( eWSLMT, msg );
}

SkaleWsRelay& SkaleWsPeer::getRelay() {
    return static_cast< SkaleWsRelay& >( srv() );
}
SkaleServerOverride* SkaleWsPeer::pso() {
    return getRelay().pso();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SkaleWsRelay::SkaleWsRelay( const char* strScheme,  // "ws" or "wss"
    int nPort )
    : nPort_( nPort ) {
    scheme_ = skutils::tools::to_lower( strScheme );
    scheme_uc_ = skutils::tools::to_upper( strScheme );
    onPeerInstantiate_ = [&]( skutils::ws::server& srv,
                             skutils::ws::hdl_t hdl ) -> skutils::ws::peer_ptr_t {
        if ( pso()->bTraceCalls_ )
            clog( dev::VerbosityInfo, cc::info( scheme_uc_ ) )
                << cc::notice( "Will instantiate new peer" );
        return new SkaleWsPeer( srv, hdl );
    };
    // onPeerRegister_ =
    // onPeerUnregister_ =
}

SkaleWsRelay::~SkaleWsRelay() {
    stop();
}

void SkaleWsRelay::run( skutils::ws::fn_continue_status_flag_t fnContinueStatusFlag ) {
    isInLoop_ = true;
    try {
        if ( service_mode_supported() )
            service( fnContinueStatusFlag );
        else {
            while ( true ) {
                poll( fnContinueStatusFlag );
                if ( fnContinueStatusFlag ) {
                    if ( !fnContinueStatusFlag() )
                        break;
                }
            }
        }
    } catch ( ... ) {
    }
    isInLoop_ = false;
}

void SkaleWsRelay::waitWhileInLoop() {
    while ( isInLoop() )
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
}

bool SkaleWsRelay::start( SkaleServerOverride* pso ) {
    stop();
    pso_ = pso;
    clog( dev::VerbosityInfo, cc::info( scheme_uc_ ) )
        << cc::notice( "Will start server on port " ) << cc::c( nPort_ );
    if ( !open( scheme_, nPort_ ) ) {
        clog( dev::VerbosityError, cc::fatal( scheme_uc_ + " ERRORL" ) )
            << cc::error( "Failed to start serv on port " ) << cc::c( nPort_ );
        return false;
    }
    std::thread( [&]() {
        isRunning_ = true;
        try {
            run( [&]() -> bool { return isRunning_; } );
        } catch ( ... ) {
        }
        // isRunning_ = false;
    } )
        .detach();
    clog( dev::VerbosityInfo, cc::info( scheme_uc_ ) )
        << cc::success( "OK, server started on port " ) << cc::c( nPort_ );
    return true;
}
void SkaleWsRelay::stop() {
    if ( !isRunning() )
        return;
    clog( dev::VerbosityInfo, cc::info( scheme_uc_ ) )
        << cc::notice( "Will stop on port " ) << cc::c( nPort_ ) << cc::notice( "..." );
    isRunning_ = false;
    waitWhileInLoop();
    close();
    clog( dev::VerbosityInfo, cc::info( scheme_uc_ ) )
        << cc::success( "OK, server stopped on port " ) << cc::c( nPort_ );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SkaleServerOverride::SkaleServerOverride( const std::string& http_addr, int http_port,
    const std::string& web_socket_addr, int web_socket_port, const std::string& pathSslKey,
    const std::string& pathSslCert )
    : AbstractServerConnector(),
      address_http_( http_addr ),
      address_web_socket_( web_socket_addr ),
      port_http_( http_port ),
      port_web_socket_( web_socket_port ),
      bTraceCalls_( false ),
      pathSslKey_( pathSslKey ),
      pathSslCert_( pathSslCert ),
      bIsSSL_( false ) {}

SkaleServerOverride::~SkaleServerOverride() {
    StopListening();
}

jsonrpc::IClientConnectionHandler* SkaleServerOverride::GetHandler( const std::string& url ) {
    if ( jsonrpc::AbstractServerConnector::GetHandler() != nullptr )
        return AbstractServerConnector::GetHandler();
    std::map< std::string, jsonrpc::IClientConnectionHandler* >::iterator it =
        this->urlhandler.find( url );
    if ( it != this->urlhandler.end() )
        return it->second;
    return nullptr;
}

void SkaleServerOverride::logTraceServerEvent(
    bool isError, const char* strProtocol, const std::string& strMessage ) {
    if ( strMessage.empty() )
        return;
    std::stringstream ssProtocol;
    strProtocol = ( strProtocol && strProtocol[0] ) ? strProtocol : "Unknown network protocol";
    if ( isError )
        ssProtocol << cc::fatal( strProtocol + std::string( " ERROR:" ) );
    else
        ssProtocol << cc::info( strProtocol + std::string( ":" ) );
    if ( isError )
        clog( dev::VerbosityError, ssProtocol.str() ) << strMessage;
    else
        clog( dev::VerbosityInfo, ssProtocol.str() ) << strMessage;
}

void SkaleServerOverride::logTraceServerTraffic( bool isRX, bool isError, const char* strProtocol,
    const char* strOrigin, const std::string& strPayload ) {
    std::stringstream ssProtocol;
    std::string strProto =
        ( strProtocol && strProtocol[0] ) ? strProtocol : "Unknown network protocol";
    strOrigin = ( strOrigin && strOrigin[0] ) ? strOrigin : "unknown origin";
    std::string strErrorSuffix, strOriginSuffix, strDirect;
    if ( isRX ) {
        strDirect = cc::ws_rx( " >>> " );
        ssProtocol << cc::ws_rx_inv( " >>> " + strProto + "/RX >>> " );
    } else {
        strDirect = cc::ws_tx( " <<< " );
        ssProtocol << cc::ws_tx_inv( " <<< " + strProto + "/TX <<< " );
    }
    strOriginSuffix = cc::u( strOrigin );
    if ( isError )
        strErrorSuffix = cc::fatal( " ERROR " );
    if ( isError )
        clog( dev::VerbosityError, ssProtocol.str() )
            << strErrorSuffix << strOriginSuffix << strDirect << strPayload;
    else
        clog( dev::VerbosityInfo, ssProtocol.str() )
            << strErrorSuffix << strOriginSuffix << strDirect << strPayload;
}

bool SkaleServerOverride::startListeningHTTP() {
    try {
        stopListeningHTTP();
        if ( address_http_.empty() || port_http_ <= 0 )
            return true;
        bIsSSL_ = false;
        if ( ( !pathSslKey_.empty() ) && ( !pathSslCert_.empty() ) )
            bIsSSL_ = true;
        logTraceServerEvent( false, bIsSSL_ ? "HTTPS" : "HTTP",
            cc::debug( "starting " ) + cc::info( bIsSSL_ ? "HTTPS" : "HTTP" ) +
                cc::debug( " server on address " ) + cc::info( address_http_ ) +
                cc::debug( " and port " ) + cc::c( port_http_ ) + cc::debug( "..." ) );
        if ( bIsSSL_ )
            this->pServerHTTP_.reset(
                new skutils::http::SSL_server( pathSslCert_.c_str(), pathSslKey_.c_str() ) );
        else
            this->pServerHTTP_.reset( new skutils::http::server );
        this->pServerHTTP_->Options( "/", [&]( const skutils::http::request& req,
                                              skutils::http::response& res ) {
            if ( bTraceCalls_ )
                logTraceServerTraffic( true, false, bIsSSL_ ? "HTTPS" : "HTTP", req.origin_.c_str(),
                    cc::info( "OPTTIONS" ) + cc::debug( " request handler" ) );
            res.set_header( "access-control-allow-headers", "Content-Type" );
            res.set_header( "access-control-allow-methods", "POST" );
            res.set_header( "access-control-allow-origin", "*" );
            res.set_header( "content-length", "0" );
            res.set_header(
                "vary", "Origin, Access-Control-request-Method, Access-Control-request-Headers" );
        } );
        this->pServerHTTP_->Post(
            "/", [this]( const skutils::http::request& req, skutils::http::response& res ) {
                if ( bTraceCalls_ )
                    logTraceServerTraffic( true, false, bIsSSL_ ? "HTTPS" : "HTTP",
                        req.origin_.c_str(), cc::j( req.body_ ) );
                int nID = -1;
                std::string strResponse;
                try {
                    nlohmann::json joRequest = nlohmann::json::parse( req.body_ );
                    nID = joRequest["id"].get< int >();
                    jsonrpc::IClientConnectionHandler* handler = this->GetHandler( "/" );
                    if ( handler == nullptr )
                        throw std::runtime_error( "No client connection handler found" );
                    handler->HandleRequest( req.body_.c_str(), strResponse );
                } catch ( const std::exception& ex ) {
                    logTraceServerTraffic( false, true, bIsSSL_ ? "HTTPS" : "HTTP",
                        req.origin_.c_str(), cc::warn( ex.what() ) );
                    nlohmann::json joErrorResponce;
                    joErrorResponce["id"] = nID;
                    joErrorResponce["result"] = "error";
                    joErrorResponce["error"] = std::string( ex.what() );
                    strResponse = joErrorResponce.dump();
                } catch ( ... ) {
                    const char* e = "unknown exception in SkaleServerOverride";
                    logTraceServerTraffic( false, true, bIsSSL_ ? "HTTPS" : "HTTP",
                        req.origin_.c_str(), cc::warn( e ) );
                    nlohmann::json joErrorResponce;
                    joErrorResponce["id"] = nID;
                    joErrorResponce["result"] = "error";
                    joErrorResponce["error"] = std::string( e );
                    strResponse = joErrorResponce.dump();
                }
                if ( bTraceCalls_ )
                    logTraceServerTraffic( false, false, bIsSSL_ ? "HTTPS" : "HTTP",
                        req.origin_.c_str(), cc::j( strResponse ) );
                res.set_header( "access-control-allow-origin", "*" );
                res.set_header( "vary", "Origin" );
                res.set_content( strResponse.c_str(), "application/json" );
            } );
        std::thread( [this]() {
            this->pServerHTTP_->listen( this->address_http_.c_str(), this->port_http_ );
        } )
            .detach();
        logTraceServerEvent( false, bIsSSL_ ? "HTTPS" : "HTTP",
            cc::success( "OK, started " ) + cc::info( bIsSSL_ ? "HTTPS" : "HTTP" ) +
                cc::success( " server on address " ) + cc::info( address_http_ ) +
                cc::success( " and port " ) + cc::c( port_http_ ) );
        return true;
    } catch ( const std::exception& ex ) {
        logTraceServerEvent( false, bIsSSL_ ? "HTTPS" : "HTTP",
            cc::error( "FAILED to start " ) + cc::warn( bIsSSL_ ? "HTTPS" : "HTTP" ) +
                cc::error( " server: " ) + cc::warn( ex.what() ) );
    } catch ( ... ) {
        logTraceServerEvent( false, bIsSSL_ ? "HTTPS" : "HTTP",
            cc::error( "FAILED to start " ) + cc::warn( bIsSSL_ ? "HTTPS" : "HTTP" ) +
                cc::error( " server: " ) + cc::warn( "unknown exception" ) );
    }
    return false;
}

bool SkaleServerOverride::stopListeningHTTP() {
    try {
        if ( pServerHTTP_ ) {
            logTraceServerEvent( false, bIsSSL_ ? "HTTPS" : "HTTP",
                cc::notice( "Will stop " ) + cc::info( bIsSSL_ ? "HTTPS" : "HTTP" ) +
                    cc::notice( " server on address " ) + cc::info( address_http_ ) +
                    cc::success( " and port " ) + cc::c( port_http_ ) + cc::notice( "..." ) );
            if ( pServerHTTP_->is_running() )
                pServerHTTP_->stop();
            pServerHTTP_.reset();
            logTraceServerEvent( false, bIsSSL_ ? "HTTPS" : "HTTP",
                cc::success( "OK, stopped " ) + cc::info( bIsSSL_ ? "HTTPS" : "HTTP" ) +
                    cc::success( " server on address " ) + cc::info( address_http_ ) +
                    cc::success( " and port " ) + cc::c( port_http_ ) );
        }
    } catch ( ... ) {
    }
    return true;
}

bool SkaleServerOverride::startListeningWebSocket() {
    try {
        stopListeningWebSocket();
        if ( address_web_socket_.empty() || port_web_socket_ <= 0 )
            return true;
        bIsSSL_ = false;
        if ( ( !pathSslKey_.empty() ) && ( !pathSslCert_.empty() ) )
            bIsSSL_ = true;
        logTraceServerEvent( false, bIsSSL_ ? "WSS" : "WS",
            cc::debug( "starting " ) + cc::info( bIsSSL_ ? "WSS" : "WS" ) +
                cc::debug( " server on address " ) + cc::info( address_web_socket_ ) +
                cc::debug( " and port " ) + cc::c( port_web_socket_ ) + cc::debug( "..." ) );
        pServerWS_.reset( new SkaleWsRelay( bIsSSL_ ? "wss" : "ws", port_web_socket_ ) );
        if ( bIsSSL_ ) {
            pServerWS_->strCertificateFile_ = pathSslCert_;
            pServerWS_->strPrivateKeyFile_ = pathSslKey_;
        }
        if ( !pServerWS_->start( this ) )
            throw std::runtime_error( "Failed to start server" );
        logTraceServerEvent( false, bIsSSL_ ? "WSS" : "WS",
            cc::success( "OK, started " ) + cc::info( bIsSSL_ ? "WSS" : "WS" ) +
                cc::success( " server on address " ) + cc::info( address_web_socket_ ) +
                cc::success( " and port " ) + cc::c( port_web_socket_ ) + cc::debug( "..." ) );
        return true;
    } catch ( const std::exception& ex ) {
        logTraceServerEvent( false, bIsSSL_ ? "WSS" : "WS",
            cc::error( "FAILED to start " ) + cc::warn( bIsSSL_ ? "WSS" : "WS" ) +
                cc::error( " server: " ) + cc::warn( ex.what() ) );
    } catch ( ... ) {
        logTraceServerEvent( false, bIsSSL_ ? "WSS" : "WS",
            cc::error( "FAILED to start " ) + cc::warn( bIsSSL_ ? "WSS" : "WS" ) +
                cc::error( " server: " ) + cc::warn( "unknown exception" ) );
    }
    return false;
}
bool SkaleServerOverride::stopListeningWebSocket() {
    try {
        if ( pServerWS_ ) {
            logTraceServerEvent( false, bIsSSL_ ? "WSS" : "WS",
                cc::notice( "Will stop " ) + cc::info( bIsSSL_ ? "WSS" : "WS" ) +
                    cc::notice( " server on address " ) + cc::info( address_web_socket_ ) +
                    cc::success( " and port " ) + cc::c( port_web_socket_ ) + cc::notice( "..." ) );
            if ( pServerWS_->isRunning() )
                pServerWS_->stop();
            pServerWS_.reset();
            logTraceServerEvent( false, bIsSSL_ ? "WSS" : "WS",
                cc::success( "OK, stopped " ) + cc::info( bIsSSL_ ? "WSS" : "WS" ) +
                    cc::success( " server on address " ) + cc::info( address_web_socket_ ) +
                    cc::success( " and port " ) + cc::c( port_web_socket_ ) );
        }
    } catch ( ... ) {
    }
    return true;
}

bool SkaleServerOverride::StartListening() {
    auto retVal = startListeningHTTP() && startListeningWebSocket() ? true : false;
    return retVal;
}

bool SkaleServerOverride::StopListening() {
    auto retVal = stopListeningHTTP() && stopListeningWebSocket() ? true : false;
    return retVal;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
