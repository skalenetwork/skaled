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

SkaleServerOverride::SkaleServerOverride( const std::string& http_addr, int http_port,
    const std::string& pathSslKey, const std::string& pathSslCert )
    : AbstractServerConnector(),
      address_http_( http_addr ),
      port_http_( http_port ),
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


bool SkaleServerOverride::StartListening() {
    auto retVal = startListeningHTTP() ? true : false;
    return retVal;
}

bool SkaleServerOverride::StopListening() {
    auto retVal = stopListeningHTTP() ? true : false;
    return retVal;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
