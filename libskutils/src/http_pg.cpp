#include <skutils/http_pg.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#pragma GCC diagnostic ignored "-Waddress"
#pragma GCC diagnostic ignored "-Wnonnull-compare"
#pragma GCC diagnostic ignored "-Wsign-compare"

#include <proxygen/httpserver/RequestHandler.h>
#include <proxygen/httpserver/ResponseBuilder.h>

#include <glog/logging.h>

#pragma GCC diagnostic pop

#include <skutils/console_colors.h>
#include <skutils/multithreading.h>
#include <skutils/rest_call.h>

namespace skutils {
namespace http_pg {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void init_logging( const char* strProgramName ) {
    static bool g_bWasCalled = false;
    if ( g_bWasCalled )
        return;
    g_bWasCalled = true;
    google::InitGoogleLogging( strProgramName );
}

void install_logging_fail_func( logging_fail_func_t fn ) {
    static bool g_bWasCalled = false;
    if ( g_bWasCalled )
        return;
    g_bWasCalled = true;
    google::InstallFailureFunction( reinterpret_cast< google::logging_fail_func_t >( fn ) );
}

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
    pg_log( strLogPrefix_ + cc::info( strHttpMethod_ ) + cc::debug( " request query" ) + "\n" );
    const folly::SocketAddress& origin_address = req->getClientAddress();
    std::string strClientAddress = origin_address.getAddressStr();  // getFullyQualified()
    ipVer_ =
        ( skutils::is_ipv6( strClientAddress ) && skutils::is_valid_ipv6( strClientAddress ) ) ? 6 :
                                                                                                 4;
    std::string strAddressPart =
        ( ( ipVer_ == 6 ) ? "[" : "" ) + strClientAddress + ( ( ipVer_ == 6 ) ? "]" : "" );
    strOrigin_ = req->getScheme() + "://" + strAddressPart + ":" + req->getClientPort();
    strPath_ = req->getPath();
    strDstAddress_ = req->getDstAddress().getAddressStr();  // getFullyQualified()
    std::string strDstPort = req->getDstPort();
    nDstPort_ = ( !strDstPort.empty() ) ? atoi( strDstPort.c_str() ) : 0;
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
}

void request_site::onBody( std::unique_ptr< folly::IOBuf > body ) noexcept {
    pg_log( strLogPrefix_ + cc::info( strHttpMethod_ ) + cc::debug( " body query" ) + "\n" );
    if ( strHttpMethod_ == "OPTIONS" )
        return;
    auto cnt = body->computeChainDataLength();
    auto pData = body->data();
    std::string strIn;
    strIn.insert( strIn.end(), pData, pData + cnt );
    pg_log( strLogPrefix_ + cc::debug( "got body part number " ) + cc::size10( nBodyPartNumber_ ) +
            "\n" );
    pg_log(
        strLogPrefix_ + cc::debug( "got body part size " ) + cc::size10( strIn.size() ) + "\n" );
    pg_log( strLogPrefix_ + cc::debug( "got body part content " ) + cc::normal( strIn ) + "\n" );
    strBody_ += strIn;
    pg_log( strLogPrefix_ + cc::debug( "accumulated so far body size " ) +
            cc::size10( strBody_.size() ) + "\n" );
    pg_log( strLogPrefix_ + cc::debug( "accumulated so far body content part(s) " ) +
            cc::normal( strBody_ ) + "\n" );
    ++nBodyPartNumber_;
}

void request_site::onEOM() noexcept {
    pg_log( strLogPrefix_ + cc::info( strHttpMethod_ ) + cc::debug( "EOM query" ) + "\n" );

    if ( strHttpMethod_ == "OPTIONS" ) {
        proxygen::ResponseBuilder( downstream_ ).sendWithEOM();
        return;
    }
    pg_log( strLogPrefix_ + cc::debug( "finally got " ) + cc::size10( nBodyPartNumber_ ) +
            cc::debug( " body part(s)" ) + "\n" );
    pg_log( strLogPrefix_ + cc::debug( "finally got body size " ) + cc::size10( strBody_.size() ) +
            "\n" );
    pg_log(
        strLogPrefix_ + cc::debug( "finally got body content " ) + cc::normal( strBody_ ) + "\n" );
    nlohmann::json joID = "0xBADF00D", joIn;
    skutils::result_of_http_request rslt;
    rslt.isBinary_ = false;
    try {
        joIn = nlohmann::json::parse( strBody_ );
        pg_log( strLogPrefix_ + cc::debug( "got body JSON " ) + cc::j( joIn ) + "\n" );
        if ( joIn.count( "id" ) > 0 )
            joID = joIn["id"];
        rslt = pSSRQ_->onRequest( joIn, strOrigin_, ipVer_, strDstAddress_, nDstPort_ );
        if ( rslt.isBinary_ )
            pg_log( strLogPrefix_ + cc::debug( "got binary answer " ) +
                    cc::binary_table( ( const void* ) ( void* ) rslt.vecBytes_.data(),
                        size_t( rslt.vecBytes_.size() ) ) +
                    "\n" );
        else
            pg_log( strLogPrefix_ + cc::debug( "got answer JSON " ) + cc::j( rslt.joOut_ ) + "\n" );
    } catch ( const std::exception& ex ) {
        pg_log( strLogPrefix_ + cc::error( "problem with body " ) + cc::warn( strBody_ ) +
                cc::error( ", error info: " ) + cc::warn( ex.what() ) + "\n" );
        rslt.isBinary_ = false;
        rslt.joOut_ = server_side_request_handler::json_from_error_text( ex.what(), joID );
        pg_log(
            strLogPrefix_ + cc::error( "got error answer JSON " ) + cc::j( rslt.joOut_ ) + "\n" );
    } catch ( ... ) {
        pg_log( strLogPrefix_ + cc::error( "problem with body " ) + cc::warn( strBody_ ) +
                cc::error( ", error info: " ) + cc::warn( "unknown exception in HTTP handler" ) +
                "\n" );
        rslt.isBinary_ = false;
        rslt.joOut_ = server_side_request_handler::json_from_error_text(
            "unknown exception in HTTP handler", joID );
        pg_log(
            strLogPrefix_ + cc::error( "got error answer JSON " ) + cc::j( rslt.joOut_ ) + "\n" );
    }
    proxygen::ResponseBuilder bldr( downstream_ );
    bldr.status( 200, "OK" );
    bldr.header( "access-control-allow-origin", "*" );
    if ( rslt.isBinary_ ) {
        bldr.header( "content-length", skutils::tools::format( "%zu", rslt.vecBytes_.size() ) );
        bldr.header( "Content-Type", "application/octet-stream" );
        std::string buffer( rslt.vecBytes_.begin(), rslt.vecBytes_.end() );
        bldr.body( buffer );
    } else {
        std::string strOut = rslt.joOut_.dump();
        bldr.header( "content-length", skutils::tools::format( "%zu", strOut.size() ) );
        bldr.header( "Content-Type", "application/json" );
        bldr.body( strOut );
    }
    bldr.sendWithEOM();
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
    jo["error"] = skutils::tools::safe_ascii( strErrorDescription );
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

server::server( pg_on_request_handler_t h, const pg_accumulate_entries& entries, int32_t threads,
    int32_t threads_limit )
    : h_( h ), entries_( entries ), threads_( threads ), threads_limit_( threads_limit ) {
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

    /*
        int32_t http_port = 11000;
        int32_t http_port6 = 12000;
        int32_t https_port = 11001;
        int32_t https_port6 = 12001;

        wangle::SSLContextConfig sslCfg;
        sslCfg.isDefault = true;
        sslCfg.setCertificate( "./cert.pem", "./key.pem", "" );
        // sslCfg.clientCAFile = "./ca_cert.pem";
        sslCfg.clientVerification = folly::SSLContext::VerifyClientCertificate::DO_NOT_REQUEST;  //
       IF_PRESENTED

        proxygen::HTTPServer::IPConfig cfg_http( folly::SocketAddress( "127.0.0.1", http_port, true
       ), proxygen::HTTPServer::Protocol::HTTP ); proxygen::HTTPServer::IPConfig cfg_http6(
       folly::SocketAddress( "::1", http_port6, true ), proxygen::HTTPServer::Protocol::HTTP );
        proxygen::HTTPServer::IPConfig cfg_https( folly::SocketAddress( "127.0.0.1", https_port,
       true ), proxygen::HTTPServer::Protocol::HTTP ); proxygen::HTTPServer::IPConfig cfg_https6(
       folly::SocketAddress( "::1", https_port6, true ), proxygen::HTTPServer::Protocol::HTTP );
        cfg_https.sslConfigs.push_back( sslCfg );
        cfg_https6.sslConfigs.push_back( sslCfg );

        std::vector< proxygen::HTTPServer::IPConfig > IPs = { cfg_http, cfg_http6, cfg_https,
       cfg_https6, };
    */

    /*
    wangle::SSLContextConfig sslCfg;
    if ( m_bHelperIsSSL ) {
        sslCfg.isDefault = true;
        sslCfg.setCertificate( cert_path_.c_str(), private_key_path_.c_str(), "" );
        sslCfg.clientVerification =
            folly::SSLContext::VerifyClientCertificate::DO_NOT_REQUEST;  // IF_PRESENTED
        if ( !ca_path_.empty() )
            sslCfg.clientCAFile = ca_path_.c_str();
    }
    proxygen::HTTPServer::IPConfig cfg_ip(
        folly::SocketAddress( strBindAddr_.c_str(), nPort_, true ),
        proxygen::HTTPServer::Protocol::HTTP );
    if ( m_bHelperIsSSL ) {
        cfg_ip.sslConfigs.push_back( sslCfg );
    }
    std::vector< proxygen::HTTPServer::IPConfig > IPs = {cfg_ip};
    */

    std::vector< proxygen::HTTPServer::IPConfig > IPs;
    pg_accumulate_entries::const_iterator itWalk = entries_.cbegin(), itEnd = entries_.cend();
    for ( ; itWalk != itEnd; ++itWalk ) {
        const pg_accumulate_entry& pge = ( *itWalk );
        proxygen::HTTPServer::IPConfig cfg_ip(
            folly::SocketAddress( pge.strBindAddr_.c_str(), pge.nPort_, true ),
            proxygen::HTTPServer::Protocol::HTTP );
        bool bHelperIsSSL =
            ( ( !pge.cert_path_.empty() ) && ( !pge.private_key_path_.empty() ) ) ? true : false;
        if ( bHelperIsSSL ) {
            wangle::SSLContextConfig sslCfg;
            sslCfg.isDefault = true;
            sslCfg.setCertificate( pge.cert_path_.c_str(), pge.private_key_path_.c_str(), "" );
            sslCfg.clientVerification =
                folly::SSLContext::VerifyClientCertificate::DO_NOT_REQUEST;  // IF_PRESENTED
            if ( !pge.ca_path_.empty() )
                sslCfg.clientCAFile = pge.ca_path_.c_str();
            cfg_ip.sslConfigs.push_back( sslCfg );
        }
        IPs.push_back( cfg_ip );
    }


    if ( threads_ <= 0 ) {
        threads_ = 1;
    }

    if ( threads_limit_ > 0 && threads_ > threads_limit_ )
        threads_ = threads_limit_;

    proxygen::HTTPServerOptions options;
    options.threads = static_cast< size_t >( threads_ );
    options.idleTimeout = std::chrono::milliseconds( skutils::rest::g_nClientConnectionTimeoutMS );
    // // // options.shutdownOn = {SIGINT, SIGTERM}; // experimental only, not needed in `skaled`
    // here
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
    // start HTTPServer main loop in a separate thread
    thread_ = std::move( std::thread( [&]() {
        skutils::multithreading::setThreadName(
            skutils::tools::format( "sklm-%p", ( void* ) this ) );
        server_->start();
    } ) );

    pg_log( strLogPrefix_ + cc::debug( "did started server thread" ) + "\n" );
    return true;
}

void server::stop() {
    if ( server_ ) {
        pg_log( strLogPrefix_ + cc::debug( "will stop server instance" ) + "\n" );
        server_->stop();
        pg_log( strLogPrefix_ + cc::debug( "did stopped server instance" ) + "\n" );
    }
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

skutils::result_of_http_request server::onRequest( const nlohmann::json& joIn,
    const std::string& strOrigin, int ipVer, const std::string& strDstAddress, int nDstPort ) {
    skutils::result_of_http_request rslt = h_( joIn, strOrigin, ipVer, strDstAddress, nDstPort );
    return rslt;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool g_b_pb_logging = false;

bool pg_logging_get() {
    return g_b_pb_logging;
}

void pg_logging_set( bool bIsLoggingMode ) {
    g_b_pb_logging = bIsLoggingMode;
}

wrapped_proxygen_server_handle pg_start( pg_on_request_handler_t h, const pg_accumulate_entry& pge,
    int32_t threads, int32_t threads_limit ) {
    pg_accumulate_entries entries;
    entries.push_back( pge );
    return pg_start( h, entries, threads, threads_limit );
}

wrapped_proxygen_server_handle pg_start( pg_on_request_handler_t h,
    const pg_accumulate_entries& entries, int32_t threads, int32_t threads_limit ) {
    skutils::http_pg::server* ptrServer =
        new skutils::http_pg::server( h, entries, threads, threads_limit );
    ptrServer->start();
    return wrapped_proxygen_server_handle( ptrServer );
}

void pg_stop( wrapped_proxygen_server_handle hServer ) {
    if ( !hServer )
        return;
    skutils::http_pg::server* ptrServer = ( skutils::http_pg::server* ) hServer;
    ptrServer->stop();
    delete ptrServer;
}

static pg_accumulate_entries g_accumulated_entries;

void pg_accumulate_clear() {
    g_accumulated_entries.clear();
}

size_t pg_accumulate_size() {
    size_t cnt = g_accumulated_entries.size();
    return cnt;
}

void pg_accumulate_add( int ipVer, std::string strBindAddr, int nPort, const char* cert_path,
    const char* private_key_path, const char* ca_path ) {
    pg_accumulate_entry pge = { ipVer, strBindAddr, nPort, cert_path ? cert_path : "",
        private_key_path ? private_key_path : "", ca_path ? ca_path : "" };
    pg_accumulate_add( pge );
}

void pg_accumulate_add( const pg_accumulate_entry& pge ) {
    g_accumulated_entries.push_back( pge );
}

wrapped_proxygen_server_handle pg_accumulate_start(
    pg_on_request_handler_t h, int32_t threads, int32_t threads_limit ) {
    skutils::http_pg::server* ptrServer =
        new skutils::http_pg::server( h, g_accumulated_entries, threads, threads_limit );
    ptrServer->start();
    return wrapped_proxygen_server_handle( ptrServer );
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace http_pg
};  // namespace skutils
