#include <skutils/http_pg.h>

#include <proxygen/httpserver/RequestHandler.h>
#include <proxygen/httpserver/ResponseBuilder.h>

#include <skutils/console_colors.h>

namespace skutils {
namespace http_pg {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void pg_log( const char* s ) {
    if ( s == nullptr || s[0] == '\0' )
        return;
    if ( !pg_logging_get() )
        return;
    std::cout << s;
}

void pg_log( const std::string& s ) {
    if ( s.empty() )
        return;
    return pg_log( s.c_str() );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

request_sink::request_sink() {}

request_sink::~request_sink() {}

uint64_t request_sink::getRequestCount() {
    return reqCount_;
}

void request_sink::OnRecordRequestCountIncrement() {
    ++reqCount_;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::atomic_uint64_t request_site::g_instance_counter = 0;

request_site::request_site( request_sink& a_sink, server_side_request_handler* pSSRQ )
    : sink_( a_sink ), pSSRQ_( pSSRQ ), nInstanceNumber_( g_instance_counter++ ) {
    strLogPrefix_ = cc::notice( "PG" ) + cc::normal( "/" ) + cc::notice( "rq" ) +
                    cc::normal( "/" ) + cc::notice( "site" ) + cc::normal( "/" ) +
                    cc::size10( nInstanceNumber_ ) + " ";
    pg_log( strLogPrefix_ + cc::debug( "constructor" ) + "\n" );
}

request_site::~request_site() {
    pg_log( strLogPrefix_ + cc::debug( "destructor" ) + "\n" );
}

void request_site::onRequest( std::unique_ptr< proxygen::HTTPMessage > req ) noexcept {
    sink_.OnRecordRequestCountIncrement();
    strHttpMethod_ =
        skutils::tools::to_upper( skutils::tools::trim_copy( req->getMethodString() ) );
    const folly::SocketAddress& origin_address = req->getClientAddress();
    std::string strClientAddress = origin_address.getAddressStr();  // getFullyQualified()
    ipVer_ = ( is_ipv6( strClientAddress ) && is_valid_ipv6( strClientAddress ) ) ? 6 : 4;
    std::string strAddressPart =
        ( ( ipVer_ == 6 ) ? "[" : "" ) + strClientAddress + ( ( ipVer_ == 6 ) ? "]" : "" );
    strOrigin_ = req->getScheme() + "://" + strAddressPart + ":" + req->getClientPort();
    strPath_ = req->getPath();
    pg_log( strLogPrefix_ + cc::debug( "request query " ) + cc::sunny( strHttpMethod_ ) +
            cc::debug( " from origin " ) + cc::info( strOrigin_ ) + cc::debug( ", path " ) +
            cc::p( strPath_ ) + "\n" );
    size_t nHeaderIdx = 0;
    req->getHeaders().forEach( [&]( std::string& name, std::string& value ) {
        pg_log( strLogPrefix_ + cc::debug( "header " ) + cc::num10( nHeaderIdx ) + " " +
                cc::attention( name ) + cc::debug( "=" ) + cc::attention( value ) + "\n" );
        ++nHeaderIdx;
    } );
    if ( strHttpMethod_ == "OPTIONS" ) {
        proxygen::ResponseBuilder( downstream_ )
            .status( 200, "OK" )
            .header( "access-control-allow-headers", "Content-Type" )
            .header( "access-control-allow-methods", "POST" )
            .header( "access-control-allow-origin", "*" )
            .header( "content-length", "0" )
            .header(
                "vary", "Origin, Access-Control-request-Method, Access-Control-request-Headers" )
            .send();
        return;
    }
    //    proxygen::ResponseBuilder( downstream_ )
    //        .status( 200, "OK" )
    //        .header( "access-control-allow-origin", "*" )
    //        .send();
}

void request_site::onBody( std::unique_ptr< folly::IOBuf > body ) noexcept {
    pg_log( strLogPrefix_ + cc::debug( "body query" ) + "\n" );
    if ( strHttpMethod_ == "OPTIONS" )
        return;
    auto cnt = body->computeChainDataLength();
    auto pData = body->data();
    std::string strIn, strOut;
    strIn.insert( strIn.end(), pData, pData + cnt );
    nlohmann::json joID = "0xBADF00D", joIn, joOut;
    try {
        joIn = nlohmann::json::parse( strIn );
        pg_log( strLogPrefix_ + cc::debug( "got body JSON " ) + cc::j( joIn ) + "\n" );
        if ( joIn.count( "id" ) > 0 )
            joID = joIn["id"];
        joOut = pSSRQ_->onRequest( joIn, strOrigin_ );
        pg_log( strLogPrefix_ + cc::debug( "got answer JSON " ) + cc::j( joOut ) + "\n" );
    } catch ( const std::exception& ex ) {
        pg_log( strLogPrefix_ + cc::error( "problem with body " ) + cc::warn( strIn ) +
                cc::error( ", error info: " ) + cc::warn( ex.what() ) + "\n" );
        joOut = server_side_request_handler::json_from_error_text( ex.what(), joID );
        pg_log( strLogPrefix_ + cc::error( "got error answer JSON " ) + cc::j( joOut ) + "\n" );
    } catch ( ... ) {
        pg_log( strLogPrefix_ + cc::error( "problem with body " ) + cc::warn( strIn ) +
                cc::error( ", error info: " ) + cc::warn( "unknown exception in HTTP handler" ) +
                "\n" );
        joOut = server_side_request_handler::json_from_error_text(
            "unknown exception in HTTP handler", joID );
        pg_log( strLogPrefix_ + cc::error( "got error answer JSON " ) + cc::j( joOut ) + "\n" );
    }
    strOut = joOut.dump();
    proxygen::ResponseBuilder( downstream_ )
        .status( 200, "OK" )
        .header( "access-control-allow-origin", "*" )
        .header( "content-length", skutils::tools::format( "%zu", strOut.size() ) )
        .body( strOut )
        .send();
}

void request_site::onEOM() noexcept {
    pg_log( strLogPrefix_ + cc::debug( "EOM query" ) + "\n" );
    proxygen::ResponseBuilder( downstream_ ).sendWithEOM();
}

void request_site::onUpgrade( proxygen::UpgradeProtocol /*protocol*/ ) noexcept {
    // handler doesn't support upgrades
    pg_log( strLogPrefix_ + cc::debug( "upgrade query" ) + "\n" );
}

void request_site::requestComplete() noexcept {
    pg_log( strLogPrefix_ + cc::debug( "complete notification" ) + "\n" );
    delete this;
}

void request_site::onError( proxygen::ProxygenError err ) noexcept {
    pg_log(
        strLogPrefix_ + cc::error( "error notification: " ) + cc::size10( size_t( err ) ) + "\n" );
    delete this;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

request_site_factory::request_site_factory( server_side_request_handler* pSSRQ )
    : pSSRQ_( pSSRQ ) {}

request_site_factory::~request_site_factory() {}

void request_site_factory::onServerStart( folly::EventBase* /*evb*/ ) noexcept {
    sink_.reset( new request_sink );
}

void request_site_factory::onServerStop() noexcept {
    sink_.reset();
}

proxygen::RequestHandler* request_site_factory::onRequest(
    proxygen::RequestHandler*, proxygen::HTTPMessage* ) noexcept {
    return new request_site( *sink_.get(), pSSRQ_ );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

server_side_request_handler::server_side_request_handler() {}

server_side_request_handler::~server_side_request_handler() {}

nlohmann::json server_side_request_handler::json_from_error_text(
    const char* strErrorDescription, const nlohmann::json& joID ) {
    if ( strErrorDescription == nullptr || ( *strErrorDescription ) == '\0' )
        strErrorDescription = "unknown error";
    nlohmann::json jo = nlohmann::json::object();
    jo["error"] = strErrorDescription;
    jo["id"] = joID;
    return jo;
}

std::string server_side_request_handler::answer_from_error_text(
    const char* strErrorDescription, const nlohmann::json& joID ) {
    nlohmann::json jo = json_from_error_text( strErrorDescription, joID );
    std::string s = jo.dump();
    return s;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

server::server( pg_on_request_handler_t h ) : h_( h ) {
    strLogPrefix_ = cc::notice( "PG" ) + cc::normal( "/" ) + cc::notice( "server" ) + " ";
    pg_log( strLogPrefix_ + cc::debug( "constructor" ) + "\n" );
}

server::~server() {
    pg_log( strLogPrefix_ + cc::debug( "destructor" ) + "\n" );
    stop();
}

bool server::start() {
    stop();

    pg_log( strLogPrefix_ + cc::debug( "will start server thread" ) + "\n" );

    int32_t http_port = 11000;
    int32_t http_port6 = 12000;
    int32_t https_port = 11001;
    int32_t https_port6 = 12001;
    //    int32_t spdy_port = 11001;      // SPDY protocol
    //    int32_t h2_port = 11002;        // HTTP/2 protocol
    // std::string ip( "localhost" );  // IP/Hostname to bind to
    int32_t threads =
        0;  // Number of threads to listen on, if <= 0 will use the number ofcores on this machine


    wangle::SSLContextConfig sslCfg;
    sslCfg.isDefault = true;
    sslCfg.setCertificate( "./cert.pem", "./key.pem", "" );
    // sslCfg.clientCAFile = "./ca_cert.pem";
    sslCfg.clientVerification =
        folly::SSLContext::VerifyClientCertificate::DO_NOT_REQUEST;  // IF_PRESENTED
    //    cfg.sslConfigs.push_back( sslCfg );


    proxygen::HTTPServer::IPConfig cfg_http( folly::SocketAddress( "127.0.0.1", http_port, true ),
        proxygen::HTTPServer::Protocol::HTTP );
    proxygen::HTTPServer::IPConfig cfg_http6(
        folly::SocketAddress( "::1", http_port6, true ), proxygen::HTTPServer::Protocol::HTTP );
    proxygen::HTTPServer::IPConfig cfg_https( folly::SocketAddress( "127.0.0.1", https_port, true ),
        proxygen::HTTPServer::Protocol::HTTP );
    proxygen::HTTPServer::IPConfig cfg_https6(
        folly::SocketAddress( "::1", https_port6, true ), proxygen::HTTPServer::Protocol::HTTP );
    cfg_https.sslConfigs.push_back( sslCfg );
    cfg_https6.sslConfigs.push_back( sslCfg );

    std::vector< proxygen::HTTPServer::IPConfig > IPs = {
        cfg_http, cfg_http6, cfg_https, cfg_https6,
        // {folly::SocketAddress( ip, http_port, true ), proxygen::HTTPServer::Protocol::HTTP},
        // {folly::SocketAddress( ip, spdy_port, true ),
        // proxygen::HTTPServer::Protocol::SPDY}, {folly::SocketAddress( ip, h2_port, true ),
        // proxygen::HTTPServer::Protocol::HTTP2},
    };

    if ( threads <= 0 ) {
        threads = sysconf( _SC_NPROCESSORS_ONLN );
        if ( threads <= 0 ) {
            stop();
            return false;
        }
    }

    proxygen::HTTPServerOptions options;
    options.threads = static_cast< size_t >( threads );
    options.idleTimeout = std::chrono::milliseconds( 60000 );
    options.shutdownOn = {SIGINT, SIGTERM};
    options.enableContentCompression = false;
    options.handlerFactories =
        proxygen::RequestHandlerChain().addThen< request_site_factory >( this ).build();
    // increase the default flow control to 1MB/10MB
    options.initialReceiveWindow = uint32_t( 1 << 20 );
    options.receiveStreamWindowSize = uint32_t( 1 << 20 );
    options.receiveSessionWindowSize = 10 * ( 1 << 20 );
    options.h2cEnabled = true;
    //
    server_.reset( new proxygen::HTTPServer( std::move( options ) ) );
    server_->bind( IPs );
    // start HTTPServer mainloop in a separate thread
    thread_ = std::move( std::thread( [&]() { server_->start(); } ) );

    pg_log( strLogPrefix_ + cc::debug( "did started server thread" ) + "\n" );
    return true;
}

void server::stop() {
    if ( thread_.joinable() ) {
        pg_log( strLogPrefix_ + cc::debug( "will stop server thread" ) + "\n" );
        thread_.join();
        pg_log( strLogPrefix_ + cc::debug( "did stopped server thread" ) + "\n" );
    }
    if ( server_ ) {
        pg_log( strLogPrefix_ + cc::debug( "will release server instance" ) + "\n" );
        server_.reset();
        pg_log( strLogPrefix_ + cc::debug( "did released server instance" ) + "\n" );
    }
}

nlohmann::json server::onRequest( const nlohmann::json& joIn, const std::string& strOrigin ) {
    nlohmann::json joOut = h_( joIn, strOrigin );
    return joOut;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool g_b_pb_logging = true;

bool pg_logging_get() {
    return g_b_pb_logging;
}

void pg_logging_set( bool bIsLoggingMode ) {
    g_b_pb_logging = bIsLoggingMode;
}

std::unique_ptr< skutils::http_pg::server > g_pServer;

void pg_start( pg_on_request_handler_t h ) {
    if ( !g_pServer ) {
        g_pServer.reset( new skutils::http_pg::server( h ) );
        g_pServer->start();
    }
}

void pg_stop() {
    if ( g_pServer ) {
        g_pServer->stop();
        g_pServer.reset();
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace http_pg
};  // namespace skutils

/*

curl -X POST --data '{"jsonrpc":"2.0","method":"eth_blockNumber","params":[],"id":83}'
http://127.0.0.1:11000 curl -X POST --data
'{"jsonrpc":"2.0","method":"eth_chainId","params":[],"id":83}' http://127.0.0.1:11000 curl -X POST
--data
'[{"jsonrpc":"2.0","method":"eth_blockNumber","params":[],"id":83},{"jsonrpc":"2.0","method":"eth_chainId","params":[],"id":83}]'
http://127.0.0.1:11000

*/
