#include <skutils/http_pg.h>

#include <proxygen/httpserver/RequestHandler.h>
#include <proxygen/httpserver/ResponseBuilder.h>

namespace skutils {
namespace http_pg {

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

request_site::request_site( request_sink& a_sink ) : sink_( a_sink ) {}

request_site::~request_site() {}

void request_site::onRequest( std::unique_ptr< proxygen::HTTPMessage > req ) noexcept {
    sink_.OnRecordRequestCountIncrement();
    proxygen::ResponseBuilder builder( downstream_ );
    builder.status( 200, "OK" );
    builder.header( "Request-Number", folly::to< std::string >( sink_.getRequestCount() ) );
    req->getHeaders().forEach( [&]( std::string& name, std::string& value ) {
        builder.header( folly::to< std::string >( "x-echo-", name ), value );
    } );
    builder.send();
}

void request_site::onBody( std::unique_ptr< folly::IOBuf > body ) noexcept {
    auto cnt = body->computeChainDataLength();
    auto pData = body->data();
    std::string s;
    s.insert( s.end(), pData, pData + cnt );
    std::cout << " ----------------- site::onBody(): " << s << "\n";
    proxygen::ResponseBuilder( downstream_ ).body( std::move( body ) ).send();
}

void request_site::onEOM() noexcept {
    proxygen::ResponseBuilder( downstream_ ).sendWithEOM();
}

void request_site::onUpgrade( proxygen::UpgradeProtocol /*protocol*/ ) noexcept {
    // handler doesn't support upgrades
}

void request_site::requestComplete() noexcept {
    delete this;
}

void request_site::onError( proxygen::ProxygenError /*err*/ ) noexcept {
    delete this;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

request_site_factory::request_site_factory() {}

request_site_factory::~request_site_factory() {}

void request_site_factory::onServerStart( folly::EventBase* /*evb*/ ) noexcept {
    sink_.reset( new request_sink );
}

void request_site_factory::onServerStop() noexcept {
    sink_.reset();
}

proxygen::RequestHandler* request_site_factory::onRequest(
    proxygen::RequestHandler*, proxygen::HTTPMessage* ) noexcept {
    return new request_site( *sink_.get() );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

server::server() {}

server::~server() {
    stop();
}

bool server::start() {
    stop();

    int32_t http_port = 11000;      // HTTP protocol
    int32_t spdy_port = 11001;      // SPDY protocol
    int32_t h2_port = 11002;        // HTTP/2 protocol
    std::string ip( "localhost" );  // IP/Hostname to bind to
    int32_t threads =
        0;  // Number of threads to listen on, if <= 0 will use the number ofcores on this machine

    std::vector< proxygen::HTTPServer::IPConfig > IPs = {
        {folly::SocketAddress( ip, http_port, true ), proxygen::HTTPServer::Protocol::HTTP},
        {folly::SocketAddress( ip, spdy_port, true ), proxygen::HTTPServer::Protocol::SPDY},
        {folly::SocketAddress( ip, h2_port, true ), proxygen::HTTPServer::Protocol::HTTP2},
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
        proxygen::RequestHandlerChain().addThen< request_site_factory >().build();
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
    return true;
}

void server::stop() {
    if ( thread_.joinable() ) {
        thread_.join();
    }
    if ( server_ ) {
        server_.reset();
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void xx_start() {
    static skutils::http_pg::server* pServer = nullptr;
    if ( !pServer ) {
        pServer = new skutils::http_pg::server;
        pServer->start();
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace http_pg
};  // namespace skutils
