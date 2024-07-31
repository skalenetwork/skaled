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

#define PG_LOG( __EXPRESSION__ )  \
    if ( pg_logging_get() ) {     \
        pg_log( __EXPRESSION__ ); \
    }

namespace skutils {
namespace http_pg {


using std::string;
using std::to_string;

void init_logging( const char* _programName ) {
    static std::atomic_bool g_bWasCalled = false;
    // run once
    if ( g_bWasCalled.exchange( true ) )
        return;
    google::InitGoogleLogging( _programName );
}

void install_logging_fail_func( logging_fail_func_t _fn ) {
    static std::atomic_bool g_wasCalled = false;
    if ( g_wasCalled.exchange( true ) )
        return;
    google::InstallFailureFunction( reinterpret_cast< google::logging_fail_func_t >( _fn ) );
}

void pg_log( const string& _s ) {
    if ( !pg_logging_get() )
        return;
    if ( _s.empty() )
        return;
    std::cerr << _s << std::endl;
}


request_sink::request_sink() {}

request_sink::~request_sink() {}

uint64_t request_sink::getRequestCount() {
    return m_reqCount;
}

void request_sink::OnRecordRequestCountIncrement() {
    ++m_reqCount;
}


std::atomic_uint64_t request_site::g_instanceCounter = 0;

request_site::request_site( request_sink& _aSink, server_side_request_handler* _SSRQ )
    : m_sink( _aSink ), m_SSRQ( _SSRQ ), m_instanceNumber( g_instanceCounter++ ) {
    m_strLogPrefix =
        string( "PG" ) + "/" + "rq" + "/" + "site" + "/" + to_string( m_instanceNumber ) + " ";
    PG_LOG( m_strLogPrefix + "constructor" );
}

request_site::~request_site() {
    PG_LOG( m_strLogPrefix + "destructor" );
}

void request_site::onRequest( std::unique_ptr< proxygen::HTTPMessage > _req ) noexcept {
    m_sink.OnRecordRequestCountIncrement();
    m_httpMethod = skutils::tools::to_upper( skutils::tools::trim_copy( _req->getMethodString() ) );

    PG_LOG( m_strLogPrefix + m_httpMethod + " request query" );

    const folly::SocketAddress& origin_address = _req->getClientAddress();
    string strClientAddress = origin_address.getAddressStr();
    m_ipVer =
        ( skutils::is_ipv6( strClientAddress ) && skutils::is_valid_ipv6( strClientAddress ) ) ? 6 :
                                                                                                 4;
    string strAddressPart =
        ( ( m_ipVer == 6 ) ? "[" : "" ) + strClientAddress + ( ( m_ipVer == 6 ) ? "]" : "" );
    m_origin = _req->getScheme() + "://" + strAddressPart + ":" + _req->getClientPort();
    m_path = _req->getPath();
    m_dstAddress_ = _req->getDstAddress().getAddressStr();  // getFullyQualified()
    string strDstPort = _req->getDstPort();
    m_dstPort = ( !strDstPort.empty() ) ? atoi( strDstPort.c_str() ) : 0;

    PG_LOG( m_strLogPrefix + "request query " + m_httpMethod + " from origin " + m_origin +
            ", path " + m_path );

    if (pg_logging_get()) {
        size_t nHeaderIdx = 0;
        _req->getHeaders().forEach( [&]( string& _name, string& _value ) {
            PG_LOG( m_strLogPrefix + "header " + to_string( nHeaderIdx ) + " " + _name + "=" + _value );
            ++nHeaderIdx;
        } );
    }
    if ( m_httpMethod == "OPTIONS" ) {
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

void request_site::onBody( std::unique_ptr< folly::IOBuf > _body ) noexcept {
    PG_LOG( m_strLogPrefix + m_httpMethod + " body query" );

    if ( m_httpMethod == "OPTIONS" )
        return;

    auto cnt = _body->computeChainDataLength();
    auto pData = _body->data();

    m_strBody.insert( m_strBody.end(), pData, pData + cnt );

    PG_LOG( m_strLogPrefix + __FUNCTION__ + "part number " + to_string( m_bodyPartNumber ) );

    PG_LOG(
        m_strLogPrefix + __FUNCTION__ + " accumulated body size " + to_string( m_strBody.size() ) );
    PG_LOG( m_strLogPrefix + __FUNCTION__ + " accumulated body content part(s) " + m_strBody );
    ++m_bodyPartNumber;
}

void request_site::onEOM() noexcept {
    PG_LOG( m_strLogPrefix + m_httpMethod + __FUNCTION__);

    if ( m_httpMethod == "OPTIONS" ) {
        proxygen::ResponseBuilder( downstream_ ).sendWithEOM();
        return;
    }
    PG_LOG( m_strLogPrefix + __FUNCTION__ + " body part(s): " + to_string( m_bodyPartNumber ) );
    PG_LOG( m_strLogPrefix + __FUNCTION__ + " body size: " + to_string( m_strBody.size() ) );
    PG_LOG( m_strLogPrefix + __FUNCTION__ + " body content: " + m_strBody );

    nlohmann::json joID = "0xBADF00D", joIn;
    skutils::result_of_http_request rslt;
    rslt.isBinary_ = false;
    try {
        joIn = nlohmann::json::parse( m_strBody );
        PG_LOG( m_strLogPrefix + "body JSON " + joIn.dump() );
        if ( joIn.count( "id" ) > 0 )
            joID = joIn["id"];

        rslt = m_SSRQ->onRequest( joIn, m_origin, m_ipVer, m_dstAddress_, m_dstPort );

        if ( rslt.isBinary_ ) {
            PG_LOG( m_strLogPrefix + "binary answer " +
                    cc::binary_table( ( const void* ) ( void* ) rslt.vecBytes_.data(),
                        size_t( rslt.vecBytes_.size() ) ) );
        } else {
            PG_LOG( m_strLogPrefix + "answer JSON " + rslt.joOut_.dump() );
        }
    } catch ( const std::exception& ex ) {
        PG_LOG( m_strLogPrefix + "problem with body " + m_strBody + ", error info: " + ex.what() );
        rslt.isBinary_ = false;
        rslt.joOut_ = server_side_request_handler::json_from_error_text( ex.what(), joID );
        PG_LOG( m_strLogPrefix + "got error answer JSON " + rslt.joOut_.dump() );
    } catch ( ... ) {
        PG_LOG( m_strLogPrefix + "problem with body " + m_strBody +
                ", error info: " + "unknown exception in HTTP handler" );
        rslt.isBinary_ = false;
        rslt.joOut_ = server_side_request_handler::json_from_error_text(
            "unknown exception in HTTP handler", joID );
        PG_LOG( m_strLogPrefix + "got error answer JSON " + rslt.joOut_.dump() );
    }
    proxygen::ResponseBuilder bldr( downstream_ );
    bldr.status( 200, "OK" );
    bldr.header( "access-control-allow-origin", "*" );
    if ( rslt.isBinary_ ) {
        bldr.header( "content-length", skutils::tools::format( "%zu", rslt.vecBytes_.size() ) );
        bldr.header( "Content-Type", "application/octet-stream" );
        string buffer( rslt.vecBytes_.begin(), rslt.vecBytes_.end() );
        bldr.body( buffer );
    } else {
        string strOut = rslt.joOut_.dump();
        bldr.header( "content-length", skutils::tools::format( "%zu", strOut.size() ) );
        bldr.header( "Content-Type", "application/json" );
        bldr.body( strOut );
    }
    bldr.sendWithEOM();
}

void request_site::onUpgrade( proxygen::UpgradeProtocol ) noexcept {
    // handler doesn't support upgrades
    PG_LOG( m_strLogPrefix + __FUNCTION__ );
}

void request_site::requestComplete() noexcept {
    PG_LOG( m_strLogPrefix + __FUNCTION__ );
    delete this;
}

void request_site::onError( proxygen::ProxygenError _err ) noexcept {
    PG_LOG( m_strLogPrefix + __FUNCTION__ + to_string( size_t( _err ) ));
    delete this;
}

request_site_factory::request_site_factory( server_side_request_handler* _SSRQ )
    : m_SSRQ( _SSRQ ) {}

request_site_factory::~request_site_factory() {}

void request_site_factory::onServerStart( folly::EventBase* /*evb*/ ) noexcept {
    m_sink.reset( new request_sink );
}

void request_site_factory::onServerStop() noexcept {
    m_sink.reset();
}

proxygen::RequestHandler* request_site_factory::onRequest(
    proxygen::RequestHandler*, proxygen::HTTPMessage* ) noexcept {
    return new request_site( *m_sink.get(), m_SSRQ );
}


server_side_request_handler::server_side_request_handler() {}

server_side_request_handler::~server_side_request_handler() {}

nlohmann::json server_side_request_handler::json_from_error_text(
    const char* _errorDescription, const nlohmann::json& _joID ) {
    if ( _errorDescription == nullptr || ( *_errorDescription ) == '\0' )
        _errorDescription = "unknown error";
    nlohmann::json jo = nlohmann::json::object();
    jo["error"] = skutils::tools::safe_ascii( _errorDescription );
    jo["id"] = _joID;
    return jo;
}

string server_side_request_handler::answer_from_error_text(
    const char* _errorDescription, const nlohmann::json& _joID ) {
    nlohmann::json jo = json_from_error_text( _errorDescription, _joID );
    string s = jo.dump();
    return s;
}


server::server( pg_on_request_handler_t _h, const pg_accumulate_entries& _entries, int32_t _threads,
    int32_t _threadsLimit )
    : m_h( _h ), m_entries( _entries ), m_threads( _threads ), m_threads_limit( _threadsLimit ) {
    m_logPrefix = "PG/server ";
    PG_LOG( m_logPrefix + "constructor" );
}

server::~server() {
    PG_LOG( m_logPrefix + "destructor" );
    stop();
}

bool server::start() {
    stop();

    PG_LOG( m_logPrefix + "starting server thread" );

    std::vector< proxygen::HTTPServer::IPConfig > IPs;
    pg_accumulate_entries::const_iterator itWalk = m_entries.cbegin(), itEnd = m_entries.cend();
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


    if ( m_threads <= 0 ) {
        m_threads = 1;
    }

    if ( m_threads_limit > 0 && m_threads > m_threads_limit )
        m_threads = m_threads_limit;

    proxygen::HTTPServerOptions options;
    options.threads = static_cast< size_t >( m_threads );
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
    m_server.reset( new proxygen::HTTPServer( std::move( options ) ) );
    m_server->bind( IPs );
    // start HTTPServer main loop in a separate thread
    m_thread = std::move( std::thread( [&]() {
        skutils::multithreading::setThreadName(
            skutils::tools::format( "sklm-%p", ( void* ) this ) );
        m_server->start();
    } ) );

    PG_LOG( m_logPrefix + "did started server thread" );
    return true;
}

void server::stop() {
    if ( m_server ) {
        PG_LOG( m_logPrefix + "stopping server instance" );
        m_server->stop();
        PG_LOG( m_logPrefix + "stopped server instance" );
    }
    if ( m_thread.joinable() ) {
        PG_LOG( m_logPrefix + "stopping server thread" );
        m_thread.join();
        PG_LOG( m_logPrefix + "stopped server thread" );
    }
    if ( m_server ) {
        PG_LOG( m_logPrefix + "releasing server instance" );
        m_server.reset();
        PG_LOG( m_logPrefix + "released server instance" );
    }
}

skutils::result_of_http_request server::onRequest( const nlohmann::json& _joIn,
    const string& _origin, int _ipVer, const string& _dstAddress, int _dstPort ) {
    skutils::result_of_http_request rslt = m_h( _joIn, _origin, _ipVer, _dstAddress, _dstPort );
    return rslt;
}


bool g_pbLogging = false;

bool pg_logging_get() {
    return g_pbLogging;
}

void pg_logging_set( bool _isLoggingMode ) {
    g_pbLogging = _isLoggingMode;
}

wrapped_proxygen_server_handle pg_start( pg_on_request_handler_t _h,
    const pg_accumulate_entry& _pge, int32_t _threads, int32_t _threadsLimit ) {
    pg_accumulate_entries entries;
    entries.push_back( _pge );
    return pg_start( _h, entries, _threads, _threadsLimit );
}

wrapped_proxygen_server_handle pg_start( pg_on_request_handler_t _h,
    const pg_accumulate_entries& _entries, int32_t _threads, int32_t _threadsLimit ) {
    skutils::http_pg::server* ptrServer =
        new skutils::http_pg::server( _h, _entries, _threads, _threadsLimit );
    ptrServer->start();
    return wrapped_proxygen_server_handle( ptrServer );
}

void pg_stop( wrapped_proxygen_server_handle _hServer ) {
    if ( !_hServer )
        return;
    skutils::http_pg::server* ptrServer = ( skutils::http_pg::server* ) _hServer;
    ptrServer->stop();
    delete ptrServer;
}

static pg_accumulate_entries g_accumulatedEntries;

void pg_accumulate_clear() {
    g_accumulatedEntries.clear();
}

size_t pg_accumulate_size() {
    size_t cnt = g_accumulatedEntries.size();
    return cnt;
}

void pg_accumulate_add( int ipVer, string strBindAddr, int nPort, const char* cert_path,
    const char* private_key_path, const char* ca_path ) {
    pg_accumulate_entry pge = { ipVer, strBindAddr, nPort, cert_path ? cert_path : "",
        private_key_path ? private_key_path : "", ca_path ? ca_path : "" };
    pg_accumulate_add( pge );
}

void pg_accumulate_add( const pg_accumulate_entry& pge ) {
    g_accumulatedEntries.push_back( pge );
}

wrapped_proxygen_server_handle pg_accumulate_start(
    pg_on_request_handler_t h, int32_t threads, int32_t threads_limit ) {
    skutils::http_pg::server* ptrServer =
        new skutils::http_pg::server( h, g_accumulatedEntries, threads, threads_limit );
    ptrServer->start();
    return wrapped_proxygen_server_handle( ptrServer );
}


};  // namespace http_pg
};  // namespace skutils
