#include "main.h"

#define __EXIT_SUCCESS 0
#define __EXIT_ERROR_REST_CALL_FAILED 13
#define __EXIT_ERROR_BAD_BIND_ADDRESS 14
#define __EXIT_ERROR_BAD_PROTOCOL_NAME 15
#define __EXIT_ERROR_FAILED_PARSE_CLI_ARGS 16
#define __EXIT_ERROR_BAD_PORT_NUMBER 17
#define __EXIT_ERROR_BAD_SSL_FILE_PATHS 18
#define __EXIT_ERROR_IN_TEST_EXCEPTOION 19
#define __EXIT_ERROR_IN_TEST_UNKNOWN_EXCEPTOION 20

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string generate_new_call_id( const char* strPrefix /*= nullptr*/ ) {
    static skutils::multithreading::mutex_type mtx;
    std::lock_guard< skutils::multithreading::mutex_type > lock( mtx );
    if ( !strPrefix )
        strPrefix = "";
    typedef uint64_t id_t;
    typedef std::map< std::string, id_t > map_prefix2id_t;
    static map_prefix2id_t g_map;
    id_t nextID = 0;
    map_prefix2id_t::iterator itFind = g_map.find( strPrefix ), itEnd = g_map.end();
    if ( itFind != itEnd ) {
        nextID = itFind->second;
        ++nextID;
    }
    g_map[strPrefix] = nextID;
    std::stringstream ss;
    ss << "id";
    if ( strPrefix[0] != '\0' )
        ss << '_' << strPrefix;
    ss << '_' << std::hex << std::setw( sizeof( id_t ) * 2 ) << std::setfill( '0' ) << nextID;
    return ss.str();
}
std::string generate_new_call_id( const std::string& strPrefix ) {
    return generate_new_call_id( strPrefix.c_str() );
}

void ensure_call_id_present( nlohmann::json& joMsg, const char* strCustomPrefix /*= nullptr*/ ) {
    if ( joMsg.count( "id" ) != 0 ) {
        std::string strID = joMsg["id"].get< std::string >();
        if ( !strID.empty() )
            return;
        joMsg.erase( "id" );
    }
    std::string strPrefix = ( strCustomPrefix == nullptr || strCustomPrefix[0] == '\0' ) ?
                                joMsg["method"].get< std::string >() :
                                std::string( strCustomPrefix );
    std::string strID( generate_new_call_id( strPrefix ) );
    joMsg["id"] = strID;
}
nlohmann::json ensure_call_id_present_copy(
    const nlohmann::json& joMsg, const char* strCustomPrefix /*= nullptr*/ ) {
    nlohmann::json joModifiedMsg( joMsg );
    ensure_call_id_present( joModifiedMsg, strCustomPrefix );
    return joModifiedMsg;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string helper_ssl_cert_and_key_holder::g_strFilePathKey;
std::string helper_ssl_cert_and_key_holder::g_strFilePathCert;

helper_ssl_cert_and_key_holder::helper_ssl_cert_and_key_holder() : need_remove_files_( false ) {
    strFilePathKey_ = g_strFilePathKey;
    strFilePathCert_ = g_strFilePathCert;
    auto_init();
}

helper_ssl_cert_and_key_holder::~helper_ssl_cert_and_key_holder() {
    auto_done();
}

void helper_ssl_cert_and_key_holder::auto_init() {
    if ( ( !strFilePathKey_.empty() ) && ( !strFilePathCert_.empty() ) &&
         skutils::tools::file_exists( strFilePathKey_ ) &&
         skutils::tools::file_exists( strFilePathCert_ ) ) {
        std::cout << ( cc::success( "Using externally specified " ) + cc::p( strFilePathKey_ ) +
                       cc::success( " and " ) + cc::p( strFilePathCert_ ) + cc::success( "." ) +
                       "\n" );
        need_remove_files_ = false;
        return;
    }
    std::string strPrefix = skutils::tools::get_tmp_file_path();
    strFilePathKey_ = strPrefix + ".key.pem";
    strFilePathCert_ = strPrefix + ".cert.pem";
    std::cout << ( cc::info( "Will generate " ) + cc::p( strFilePathKey_ ) + cc::info( " and " ) +
                   cc::p( strFilePathCert_ ) + cc::info( "..." ) + "\n" );
    //
    std::cout << ( cc::debug( "Will create " ) + cc::p( strFilePathKey_ ) + cc::debug( " and " ) +
                   cc::p( strFilePathCert_ ) + cc::debug( "..." ) + "\n" );
    std::string strCmd;
    strCmd +=
        "openssl req -new -newkey rsa:4096 -days 365 -nodes -x509 -subj "
        "\"/C=US/ST=Denial/L=Springfield/O=Dis/CN=www.example.com\" -keyout ";
    strCmd += strFilePathKey_;
    strCmd += " -out ";
    strCmd += strFilePathCert_;
    int res = ::system( strCmd.c_str() );
    ( void ) res;
    if ( !( skutils::tools::file_exists( strFilePathKey_ ) &&
             skutils::tools::file_exists( strFilePathCert_ ) ) ) {
        static const char g_err_msg[] = "failed to generate self-signed SSL certificate";
        std::cerr << g_err_msg << "\n";
        throw std::runtime_error( g_err_msg );
    }
    std::cout << ( cc::success( "OKay created " ) + cc::p( strFilePathKey_ ) +
                   cc::success( " and " ) + cc::p( strFilePathCert_ ) + cc::success( "." ) + "\n" );
    need_remove_files_ = true;
}

void helper_ssl_cert_and_key_holder::auto_done() {
    if ( !need_remove_files_ )
        return;
    if ( !strFilePathKey_.empty() ) {
        int r = ::remove( strFilePathKey_.c_str() );
        if ( r != 0 )
            std::cerr << ( skutils::tools::format( "error %d=0x%X removing file \"%s\"", r, r,
                               strFilePathKey_.c_str() ) +
                           "\n" );
    }
    if ( !strFilePathCert_.empty() ) {
        int r = ::remove( strFilePathCert_.c_str() );
        if ( r != 0 )
            std::cerr << ( skutils::tools::format( "error %d=0x%X removing file \"%s\"", r, r,
                               strFilePathCert_.c_str() ) +
                           "\n" );
    }
    need_remove_files_ = false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

helper_ssl_cert_and_key_holder& helper_ssl_cert_and_key_provider::helper_ssl_info() {
    static helper_ssl_cert_and_key_holder g_holder;
    return g_holder;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

helper_server::helper_server(
    const char* strScheme, int nListenPort, const std::string& strBindAddressServer )
    : strScheme_( skutils::tools::to_lower( strScheme ) ),
      strSchemeUC_( skutils::tools::to_upper( strScheme ) ),
      nListenPort_( nListenPort ),
      strBindAddressServer_( strBindAddressServer.empty() ? "localhost" : strBindAddressServer )
{
}

helper_server::~helper_server() {}

void helper_server::run_parallel() {
    if ( thread_is_running_ )
        throw std::runtime_error( "server is already runnig " );
    check_can_listen();
    std::thread( [&]() -> void {
        thread_is_running_ = true;
        std::cout << ( cc::info( strScheme_ ) + cc::debug( " network server thread started" ) +
                       "\n" );
        run();
        std::cout << ( cc::info( strScheme_ ) + cc::debug( " network server thread will exit" ) +
                       "\n" );
        thread_is_running_ = false;
    } )
        .detach();
    while ( !thread_is_running_ )
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
    std::cout << ( cc::debug( "Letting " ) + cc::info( strScheme_ ) +
                   cc::debug( " network server init..." ) );
    std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
}

void helper_server::wait_parallel() {
    while ( thread_is_running_ )
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
}

void helper_server::stat_check_port_availability_to_start_listen(
    int ipVer, const char* strAddr, int nPort, const char* strScheme ) {
    std::cout << ( cc::debug( "Will check port " ) + cc::num10( nPort ) +
                   cc::debug( "/IPv" + std::to_string( ipVer ) ) +
                   cc::debug( " availability for " ) + cc::info( strScheme ) +
                   cc::debug( " server..." ) + "\n" );
    skutils::network::sockaddr46 sa46;
    std::string strError =
        skutils::network::resolve_address_for_client_connection( ipVer, strAddr, sa46 );
    if ( !strError.empty() )
        throw std::runtime_error(
            std::string( "Failed to check " ) + std::string( strScheme ) +
            std::string( " server listen IP address availability for address \"" ) + strAddr +
            std::string( "\" on IPv" ) + std::to_string( ipVer ) +
            std::string( ", please check network interface with this IP address exist, error "
                         "details: " ) +
            strError );
    if ( is_tcp_port_listening( ipVer, sa46, nPort ) )
        throw std::runtime_error( std::string( "Cannot start " ) + std::string( strScheme ) +
                                  std::string( " server on address \"" ) + strAddr +
                                  std::string( "\", port " ) + std::to_string( nPort ) +
                                  std::string( ", IPv" ) + std::to_string( ipVer ) +
                                  std::string( " - port is already listening" ) );
    std::cout << ( cc::notice( "Port " ) + cc::num10( nPort ) +
                   cc::notice( "/IPv" + std::to_string( ipVer ) ) + cc::notice( " is free for " ) +
                   cc::info( strScheme ) + cc::notice( " server to start" ) + "\n" );
}

void helper_server::check_can_listen() {
    stat_check_port_availability_to_start_listen(
        4, strBindAddressServer_.c_str(), nListenPort_, strScheme_.c_str() );
    stat_check_port_availability_to_start_listen( 6, "::1", nListenPort_, strScheme_.c_str() );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

helper_ws_peer::helper_ws_peer( skutils::ws::server& srv, const skutils::ws::hdl_t& hdl )
    : skutils::ws::peer( srv, hdl ) {
    strPeerQueueID_ = skutils::dispatch::generate_id( this, "relay_peer" );
    std::cout << ( desc() + cc::notice( " peer ctor" ) + "\n" );
}
helper_ws_peer::~helper_ws_peer() {
    std::cout << ( desc() + cc::notice( " peer dtor" ) + "\n" );
    skutils::dispatch::remove( strPeerQueueID_ );
}

void helper_ws_peer::onPeerRegister() {
    std::cout << ( desc() + cc::notice( " peer registered" ) + "\n" );
    skutils::ws::peer::onPeerRegister();
}
void helper_ws_peer::onPeerUnregister() {  // peer will no longer receive onMessage after call to
                                           // this
    std::cout << ( desc() + cc::notice( " peer unregistered" ) + "\n" );
    skutils::ws::peer::onPeerUnregister();
}

void helper_ws_peer::onMessage( const std::string& msg, skutils::ws::opcv eOpCode ) {
    if ( eOpCode != skutils::ws::opcv::text )
        throw std::runtime_error( "only ws text messages are supported" );
    skutils::dispatch::async( strPeerQueueID_, [=]() -> void {
        std::cout << ( cc::ws_rx_inv(
                           ">>> " + std::string( get_helper_server().strSchemeUC_ ) + "-RX >>> " ) +
                       desc() + cc::ws_rx( " >>> " ) + cc::j( msg ) + "\n" );
        std::string strResult = msg;
        std::cout << ( cc::ws_tx_inv(
                           "<<< " + std::string( get_helper_server().strSchemeUC_ ) + "-TX <<< " ) +
                       desc() + cc::ws_tx( " <<< " ) + cc::j( strResult ) + "\n" );
        sendMessage( strResult );
    } );
    skutils::ws::peer::onMessage( msg, eOpCode );
}

void helper_ws_peer::onClose(
    const std::string& reason, int local_close_code, const std::string& local_close_code_as_str ) {
    std::cout << ( desc() + cc::warn( " peer close event with code=" ) + cc::c( local_close_code ) +
                   cc::debug( ", reason=" ) + cc::info( reason ) + "\n" );
    skutils::ws::peer::onClose( reason, local_close_code, local_close_code_as_str );
}

void helper_ws_peer::onFail() {
    std::cout << ( desc() + cc::error( " peer fail event" ) + "\n" );
    skutils::ws::peer::onFail();
}

void helper_ws_peer::onLogMessage(
    skutils::ws::e_ws_log_message_type_t eWSLMT, const std::string& msg ) {
    std::cout << ( desc() + cc::debug( " peer log: " ) + msg + "\n" );
    skutils::ws::peer::onLogMessage( eWSLMT, msg );
}

helper_server_ws_base& helper_ws_peer::get_helper_server() {
    return static_cast< helper_server_ws_base& >( srv() );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

helper_server_ws_base::helper_server_ws_base(
    const char* strScheme, int nListenPort, const std::string& strBindAddressServer )
    : helper_server( strScheme, nListenPort, strBindAddressServer ) {
    onPeerInstantiate_ = [&]( skutils::ws::server& srv,
                             skutils::ws::hdl_t hdl ) -> skutils::ws::peer_ptr_t {
        std::cout << ( cc::info( strScheme_ ) + cc::debug( " server will instantiate new peer" ) +
                       "\n" );
        return new helper_ws_peer( srv, hdl );
    };
    // onPeerRegister_ =
    // onPeerUnregister_ =
}
helper_server_ws_base::~helper_server_ws_base() {
    std::cout << ( cc::debug( "Will close " ) + cc::info( strScheme_ ) +
                   cc::debug( " server, was running on port " ) + cc::c( nListenPort_ ) + "\n" );
    stop();
}

void helper_server_ws_base::stop() {
    bStopFlag_ = true;
    if ( service_mode_supported() )
        service_interrupt();
    while ( ws_server_thread_is_running_ )
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
    wait_parallel();
    close();
}

void helper_server_ws_base::run() {
    std::cout << ( cc::debug( "Will start server on port " ) + cc::c( nListenPort_ ) +
                   cc::debug( " using " ) + cc::info( strScheme_ ) + cc::debug( " scheme" ) +
                   "\n" );
    std::atomic_bool bServerOpenComplete( false );
    std::thread( [&]() {
        ws_server_thread_is_running_ = true;
        if ( strScheme_ == "wss" ) {
            auto& ssl_info = helper_ssl_info();
            strCertificateFile_ = ssl_info.strFilePathCert_;
            strPrivateKeyFile_ = ssl_info.strFilePathKey_;
        }
        if ( !open( strScheme_.c_str(), nListenPort_ ) ) {
            std::cerr << ( cc::fatal( "Failed to start server" ) + "\n" );
            throw std::runtime_error( "Failed to start server" );
        }
        std::cout << ( cc::sunny( "Server opened" ) + "\n" );
        bServerOpenComplete = true;
        if ( service_mode_supported() ) {
            std::cout << ( cc::info( "Main loop" ) +
                           cc::debug( " will run in poll in service mode" ) + "\n" );
            service( [&]() -> bool {
                return ( !/*skutils::signal::g_bStop*/ bStopFlag_ ) ? true : false;
            } );
        } else {
            std::cout << ( cc::info( "Main loop" ) + cc::debug( " will run in poll/reset mode" ) +
                           "\n" );
            for ( ; !bStopFlag_; ) {
                poll( [&]() -> bool { return ( !bStopFlag_ ) ? true : false; } );
                if ( bStopFlag_ )
                    break;
                reset();
            }  // for( ; ! bStopFlag_; )
        }
        std::cout << ( cc::info( "Main loop" ) + cc::debug( " finish" ) + "\n" );
        ws_server_thread_is_running_ = false;
    } )
        .detach();
    std::cout << ( cc::debug( "Waiting for " ) + cc::note( "server" ) + cc::debug( " open..." ) +
                   "\n" );
    while ( !bServerOpenComplete )
        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
    std::cout << ( cc::success( "Success, server " ) + cc::info( strScheme_ ) +
                   cc::success( " server started" ) + "\n" );
}

void helper_server_ws_base::onLogMessage(
    skutils::ws::e_ws_log_message_type_t eWSLMT, const std::string& msg ) {
    switch ( eWSLMT ) {
    // case skutils::ws::e_ws_log_message_type_t::eWSLMT_debug:   break;
    // case skutils::ws::e_ws_log_message_type_t::eWSLMT_info:    break;
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_warning:
        std::cout << ( msg + "\n" );
        return;
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_error:
        std::cerr << ( msg + "\n" );
        return;
    default:
        break;
    }  // switch( eWSLMT )
    std::cout << ( msg + "\n" );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

helper_server_ws::helper_server_ws( int nListenPort, const std::string& strBindAddressServer )
    : helper_server_ws_base( "ws", nListenPort, strBindAddressServer ) {}

helper_server_ws::~helper_server_ws() {}

bool helper_server_ws::isSSL() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

helper_server_wss::helper_server_wss( int nListenPort, const std::string& strBindAddressServer )
    : helper_server_ws_base( "wss", nListenPort, strBindAddressServer ) {}

helper_server_wss::~helper_server_wss() {}

bool helper_server_wss::isSSL() const {
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

helper_server_http_base::helper_server_http_base( const char* strScheme, int nListenPort,
    const std::string& strBindAddressServer, bool is_async_http_transfer_mode )
    : helper_server( strScheme, nListenPort, strBindAddressServer ) {
    if ( strScheme_ == "https" ) {
        auto& ssl_info = helper_ssl_info();
        pServer_.reset( new skutils::http::SSL_server( ssl_info.strFilePathCert_.c_str(),
            ssl_info.strFilePathKey_.c_str(), __SKUTILS_HTTP_DEFAULT_MAX_PARALLEL_QUEUES_COUNT__,
            is_async_http_transfer_mode ) );
    } else
        pServer_.reset( new skutils::http::server(
            __SKUTILS_HTTP_DEFAULT_MAX_PARALLEL_QUEUES_COUNT__, is_async_http_transfer_mode ) );
    pServer_->Options(
        "/", [&]( const skutils::http::request& /*req*/, skutils::http::response& res ) {
            std::cout << ( cc::info( "OPTTIONS" ) + cc::debug( " request handler" ) + "\n" );
            // res.set_content( "", "text/plain" );
            res.set_header( "access-control-allow-headers", "Content-Type" );
            res.set_header( "access-control-allow-methods", "POST" );
            res.set_header( "access-control-allow-origin", "*" );
            res.set_header( "content-length", "0" );
            // res.set_header("date", "Thu, 04 Apr 2019 15:23:26 GMT" );
            // res.set_header("status", "200" );
            res.set_header(
                "vary", "Origin, Access-Control-request-Method, Access-Control-request-Headers" );
        } );
    pServer_->Post( "/", [&]( const skutils::http::request& req, skutils::http::response& res ) {
        std::cout << ( cc::ws_rx_inv( ">>> " + std::string( strSchemeUC_ ) + "-RX-POST >>> " ) +
                       cc::j( req.body_ ) + "\n" );
        //
        //
        std::string strResult = req.body_;
        //
        //
        std::cout << ( cc::ws_tx_inv( "<<< " + std::string( strSchemeUC_ ) + "-TX-POST <<< " ) +
                       cc::j( strResult ) + "\n" );
        res.set_header( "access-control-allow-origin", "*" );
        res.set_header( "vary", "Origin" );
        res.set_content( strResult.c_str(), "application/json" );
    } );
}
helper_server_http_base::~helper_server_http_base() {
    stop();
}

void helper_server_http_base::stop() {
    if ( !pServer_ )
        return;
    pServer_->stop();
    wait_parallel();
}

void helper_server_http_base::run() {
    pServer_->listen( 4, strBindAddressServer_.c_str(), nListenPort_ );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

helper_server_http::helper_server_http(
    int nListenPort, const std::string& strBindAddressServer, bool is_async_http_transfer_mode )
    : helper_server_http_base(
          "http", nListenPort, strBindAddressServer, is_async_http_transfer_mode ) {}
helper_server_http::~helper_server_http() {}

bool helper_server_http::isSSL() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

helper_server_https::helper_server_https(
    int nListenPort, const std::string& strBindAddressServer, bool is_async_http_transfer_mode )
    : helper_server_http_base(
          "https", nListenPort, strBindAddressServer, is_async_http_transfer_mode ) {}
helper_server_https::~helper_server_https() {}

bool helper_server_https::isSSL() const {
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

helper_client::helper_client( const char* strClientName, int nTargetPort, const char* strScheme, const std::string& strBindAddressClient )
    : strClientName_( strClientName ),
      nTargetPort_( nTargetPort ),
      strScheme_( skutils::tools::to_lower( strScheme ) ),
      strSchemeUC_( skutils::tools::to_upper( strScheme ) ),
      strBindAddressClient_( strBindAddressClient.empty() ? "localhost" : strBindAddressClient )
{
}
helper_client::~helper_client() {}

nlohmann::json helper_client::call( const nlohmann::json& joMsg ) {
    nlohmann::json joAnswer = nlohmann::json::object();
    if ( !call( joMsg, joAnswer ) ) {
        joAnswer = nlohmann::json::object();
        std::cerr << ( cc::info( strClientName_ ) + cc::debug( ":" ) + " " +
                       cc::fatal( "RPC CALL FAILURE:" ) +
                       cc::error( " call failed on side of client " ) + cc::info( strClientName_ ) +
                       "\n" );
    }
    return joAnswer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

helper_client_ws_base::helper_client_ws_base( const char* strClientName, int nTargetPort,
    const char* strScheme, const std::string& strBindAddressClient, const size_t nConnectAttempts )
    : helper_client( strClientName, nTargetPort, strScheme, strBindAddressClient ), bHaveAnswer_( false ) {
    if ( strScheme_ == "wss" ) {
        auto& ssl_info = helper_ssl_info();
        strCertificateFile_ = ssl_info.strFilePathCert_;
        strPrivateKeyFile_ = ssl_info.strFilePathKey_;
    }
    onClose_ = [this]( skutils::ws::client::basic_socket&, skutils::ws::hdl_t,
                   const std::string& reason, int local_close_code,
                   const std::string& local_close_code_as_str ) -> void {
        ++cntClose_;
        nLocalCloseCode_ = local_close_code;
        strLocalCloseCode_ = local_close_code_as_str;
        strCloseReason_ = reason;
        std::cout << ( cc::info( strClientName_ ) + cc::debug( ":" ) + " " +
                       cc::warn( "client got close event close with " ) + cc::info( "code" ) +
                       cc::warn( "=" ) + cc::c( local_close_code ) +
                       cc::warn( ", code explanation is " ) +
                       ( local_close_code_as_str.empty() ? cc::debug( "empty text" ) :
                                                           cc::info( local_close_code_as_str ) ) +
                       cc::warn( ", reason is " ) +
                       ( reason.empty() ? cc::debug( "empty text" ) : cc::info( reason ) ) + "\n" );
    };
    onFail_ = [this]( skutils::ws::client::basic_socket&, skutils::ws::hdl_t ) -> void {
        ++cntFail_;
        std::cerr << ( cc::info( strClientName_ ) + cc::debug( ":" ) + " " +
                       cc::error( "client got fail event" ) + "\n" );
    };
    std::cout << ( cc::info( strClientName_ ) + cc::debug( ":" ) + " " +
                   cc::debug( "Will initalize client " ) + cc::info( strClientName_ ) +
                   cc::debug( "..." ) + "\n" );
    enableRestartTimer( false );
    std::string strServerUrl = skutils::tools::format(
        "%s://%s:%d", strScheme_.c_str(), strBindAddressClient_.c_str(), nTargetPort_ );
    std::cout << ( cc::info( strClientName_ ) + cc::debug( ":" ) + " " +
                   cc::debug( "client will connect to: " ) + cc::u( strServerUrl ) +
                   cc::debug( "..." ) + "\n" );
    size_t cnt = nConnectAttempts;
    if ( cnt < 1 )
        cnt = 1;
    for ( size_t nClientConnectAttempt = 0;
          nClientConnectAttempt < cnt && ( !open( strServerUrl ) ); ++nClientConnectAttempt ) {
        std::cout << ( cc::info( strClientName_ ) + cc::debug( ":" ) + " " +
                       cc::debug( "Attempt " ) + cc::size10( nClientConnectAttempt + 1 ) +
                       cc::debug( "..." ) + "\n" );
        std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
    }
    std::cout << ( cc::info( strClientName_ ) + cc::debug( ":" ) + " " + cc::success( "Success" ) +
                   cc::debug( ", client " ) + cc::info( strClientName_ ) +
                   cc::debug( " did connected to: " ) + cc::u( strServerUrl ) + cc::debug( "..." ) +
                   "\n" );
    //		std::this_thread::sleep_for( std::chrono::seconds(1) );
    std::cout << ( cc::info( strClientName_ ) + cc::debug( ":" ) + " " + cc::success( "Done" ) +
                   cc::notice( ", did initialized client " ) + cc::info( strClientName_ ) + "\n" );
}
helper_client_ws_base::~helper_client_ws_base() {
    stop();
}

void helper_client_ws_base::stop() {
    std::cout << ( cc::info( strClientName_ ) + cc::debug( ":" ) + " " +
                   cc::notice( "Will close client " ) + cc::info( strClientName_ ) +
                   cc::notice( "..." ) + "\n" );
    pause_reading();
    cancel();
    //		std::this_thread::sleep_for( std::chrono::seconds(1) );
    //
    // wsClient.close( "client connection is about to close",
    // skutils::ws::client::close_status::normal );
    close();
    //
    std::cout << ( cc::info( strClientName_ ) + cc::debug( ":" ) + " " + cc::success( "Success" ) +
                   cc::debug( ", did closed client " ) + cc::info( strClientName_ ) + "\n" );
}

void helper_client_ws_base::run() {}

void helper_client_ws_base::onLogMessage(
    skutils::ws::e_ws_log_message_type_t eWSLMT, const std::string& msg ) {
    switch ( eWSLMT ) {
    // case skutils::ws::e_ws_log_message_type_t::eWSLMT_debug:   break;
    // case skutils::ws::e_ws_log_message_type_t::eWSLMT_info:    break;
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_warning:
        std::cout << ( cc::info( strClientName_ ) + cc::debug( ":" ) + " " + msg + "\n" );
        return;
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_error:
        std::cerr << ( cc::info( strClientName_ ) + cc::debug( ":" ) + " " + msg + "\n" );
        return;
    default:
        break;
    }  // switch( eWSLMT )
    std::cout << ( cc::info( strClientName_ ) + cc::debug( ":" ) + " " + msg + "\n" );
}

bool helper_client_ws_base::sendMessage(
    const std::string& msg, skutils::ws::opcv eOpCode /*= skutils::ws::opcv::text*/ ) {
    std::cout << ( cc::info( strClientName_ ) + cc::debug( ":" ) + " " +
                   cc::ws_tx_inv( "RPC CALL Tx " + strClientName_ ) + cc::ws_tx( " <<< " ) +
                   cc::j( msg ) + "\n" );
    strLastMessage_.clear();
    bHaveAnswer_ = false;
    return skutils::ws::client::client::sendMessage( msg, eOpCode );
}

bool helper_client_ws_base::sendMessage( const char* msg ) {  // text only
    return sendMessage( std::string( msg ), skutils::ws::opcv::text );
}

bool helper_client_ws_base::sendMessage( const nlohmann::json& joMsg ) {
    return sendMessage( joMsg.dump() );
}

void helper_client_ws_base::onMessage(
    skutils::ws::hdl_t /*hdl*/, skutils::ws::opcv /*eOpCode*/, const std::string& msg ) {
    std::cout << ( cc::info( strClientName_ ) + cc::debug( ":" ) + " " +
                   cc::ws_rx_inv( "RPC CALL Rx " + strClientName_ ) + cc::ws_rx( " >>> " ) +
                   cc::j( msg ) + "\n" );
    strLastMessage_ = msg;
    bHaveAnswer_ = true;
}

std::string helper_client_ws_base::waitAnswer() {
    while ( !bHaveAnswer_ )
        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
    return strLastMessage_;
}

void helper_client_ws_base::waitClose() {
    while ( cntClose_ == 0 )
        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
}

void helper_client_ws_base::waitFail() {
    while ( cntFail_ == 0 )
        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
}

void helper_client_ws_base::waitCloseOrFail() {
    while ( cntClose_ == 0 && cntFail_ == 0 )
        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
}

std::string helper_client_ws_base::exchange(
    const std::string& msg, skutils::ws::opcv eOpCode /*= skutils::ws::opcv::text*/ ) {
    if ( !sendMessage( msg, eOpCode ) )
        return strLastMessage_;
    return waitAnswer();
}

nlohmann::json helper_client_ws_base::exchange( const nlohmann::json& joMsg ) {
    std::string strAnswer = exchange( joMsg.dump() );
    nlohmann::json joAnswer = nlohmann::json::parse( strAnswer );
    return joAnswer;
}

bool helper_client_ws_base::call( const nlohmann::json& joMsg, nlohmann::json& joAnswer ) {
    joAnswer = nlohmann::json::object();
    joAnswer = exchange( ensure_call_id_present_copy( joMsg ) );
    return true;
}

bool helper_client_ws_base::call( const nlohmann::json& joMsg ) {
    nlohmann::json joAnswer = nlohmann::json::object();
    return call( joMsg, joAnswer );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

helper_client_ws::helper_client_ws(
    const char* strClientName, int nTargetPort, const std::string& strBindAddressClient, const size_t nConnectAttempts )
    : helper_client_ws_base( strClientName, nTargetPort, "ws", strBindAddressClient, nConnectAttempts ) {}
helper_client_ws::~helper_client_ws() {}

bool helper_client_ws::isSSL() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

helper_client_wss::helper_client_wss(
    const char* strClientName, int nTargetPort, const std::string& strBindAddressClient, const size_t nConnectAttempts )
    : helper_client_ws_base( strClientName, nTargetPort, "wss", strBindAddressClient, nConnectAttempts ) {}
helper_client_wss::~helper_client_wss() {}

bool helper_client_wss::isSSL() const {
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

helper_client_http_base::helper_client_http_base( const char* strClientName, int nTargetPort,
    const char* strScheme, const std::string& strBindAddressClient, const size_t /*nConnectAttempts*/ )
    : helper_client( strClientName, nTargetPort, strScheme, strBindAddressClient ) {
    if ( strScheme_ == "https" )
        pClient_.reset( new skutils::http::SSL_client( -1, strBindAddressClient_.c_str(), nTargetPort_ ) );
    else
        pClient_.reset( new skutils::http::client( -1, strBindAddressClient_.c_str(), nTargetPort_ ) );
}
helper_client_http_base::~helper_client_http_base() {
    stop();
}

void helper_client_http_base::stop() {
    if ( pClient_ )
        pClient_.reset();
}

void helper_client_http_base::run() {}

bool helper_client_http_base::sendMessage( const char* /*msg*/ ) {  // text only
    return true;                                                    // TO-DO:
}

bool helper_client_http_base::sendMessage( const nlohmann::json& joMsg ) {
    return sendMessage( joMsg.dump() );
}

bool helper_client_http_base::call( const std::string& strMsg, std::string& strAnswer ) {
    strAnswer.clear();
    size_t idxAttempt, cntAttempts = 5;
    for ( idxAttempt = 0; idxAttempt < cntAttempts; ++idxAttempt ) {
        skutils::http::map_headers mh;
        std::shared_ptr< skutils::http::response > pResponse =
            pClient_->Post( "/", mh, strMsg, "application/json" );
        if ( pResponse ) {
            if ( pResponse->status_ == 200 ) {
                strAnswer = pResponse->body_;
                return true;
            }
        }
    }
    return false;
}

bool helper_client_http_base::call( const nlohmann::json& joMsg, nlohmann::json& joAnswer ) {
    joAnswer = nlohmann::json::object();
    std::string strAnswer;
    std::cout << ( cc::info( strClientName_ ) + cc::debug( ":" ) + " " +
                   cc::ws_tx_inv( "RPC CALL Tx " + strClientName_ ) + cc::ws_tx( " <<< " ) +
                   cc::j( joMsg ) + "\n" );
    if ( !call( joMsg.dump(), strAnswer ) )
        return false;
    joAnswer = nlohmann::json::parse( strAnswer );
    std::cout << ( cc::info( strClientName_ ) + cc::debug( ":" ) + " " +
                   cc::ws_rx_inv( "RPC CALL Rx " + strClientName_ ) + cc::ws_rx( " >>> " ) +
                   cc::j( joAnswer ) + "\n" );
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

helper_client_http::helper_client_http(
    const char* strClientName, int nTargetPort, const std::string& strBindAddressClient, const size_t nConnectAttempts )
    : helper_client_http_base( strClientName, nTargetPort, "http", strBindAddressClient, nConnectAttempts ) {}
helper_client_http::~helper_client_http() {}

bool helper_client_http::isSSL() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

helper_client_https::helper_client_https(
    const char* strClientName, int nTargetPort, const std::string& strBindAddressClient, const size_t nConnectAttempts )
    : helper_client_http_base( strClientName, nTargetPort, "https", strBindAddressClient, nConnectAttempts ) {}
helper_client_https::~helper_client_https() {}

bool helper_client_https::isSSL() const {
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void with_thread_pool( fn_with_thread_pool_t fn,
    size_t cntThreads,      // = 0 // 0 means used actual CPU thread count, i.e. result of
                            // skutils::tools::cpu_count()
    size_t nCallQueueLimit  // = 1024*100
) {
    if ( cntThreads == 0 ) {
        size_t cntCPUs = skutils::tools::cpu_count();
        cntThreads = cntCPUs;
    }
    skutils::thread_pool pool( cntThreads, nCallQueueLimit );
    fn( pool );
}

namespace tcp_helpers {

int close_socket( socket_t sock ) {
#ifdef _WIN32
    return closesocket( sock );
#else
    return close( sock );
#endif
}

template < typename Fn >
socket_t create_socket( int ipVer, const char* host, int port, Fn fn, int socket_flags = 0,
    bool is_reuse_address = true, bool is_reuse_port = false ) {
#ifdef _WIN32
#define SO_SYNCHRONOUS_NONALERT 0x20
#define SO_OPENTYPE 0x7008
    int opt = SO_SYNCHRONOUS_NONALERT;
    setsockopt( INVALID_SOCKET, SOL_SOCKET, SO_OPENTYPE, ( char* ) &opt, sizeof( opt ) );
#endif
    struct addrinfo hints;
    struct addrinfo* result;
    memset( &hints, 0, sizeof( struct addrinfo ) );
    hints.ai_family = ( ipVer == 4 ) ? AF_INET : AF_INET6;  // AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = socket_flags;
    hints.ai_protocol = 0;  // ( ipVer == 4 ) ? IPPROTO_IPV4 : IPPROTO_IPV6;  // 0
    auto service = std::to_string( port );
    if ( getaddrinfo( host, service.c_str(), &hints, &result ) )
        return INVALID_SOCKET;
    for ( auto rp = result; rp; rp = rp->ai_next ) {
        auto sock = socket( rp->ai_family, rp->ai_socktype, rp->ai_protocol );
        if ( sock == INVALID_SOCKET ) {
            continue;
        }
        int yes = is_reuse_address ? 1 : 0;
        setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, ( char* ) &yes, sizeof( yes ) );
        yes = is_reuse_port ? 1 : 0;
        setsockopt( sock, SOL_SOCKET, SO_REUSEPORT, ( char* ) &yes, sizeof( yes ) );
        if ( fn( sock, *rp ) ) {
            freeaddrinfo( result );
            return sock;
        }
        close_socket( sock );
    }
    freeaddrinfo( result );
    return INVALID_SOCKET;
}

};  // namespace tcp_helpers

void with_busy_tcp_port( fn_with_busy_tcp_port_worker_t fnWorker,
                         fn_with_busy_tcp_port_error_t fnErrorHandler,
                         const std::string& strBindAddressServer,
                         const int nSocketListenPort, bool isIPv4,
                         bool isIPv6, bool is_reuse_address,
                         bool is_reuse_port ) {
    socket_t fd4 = INVALID_SOCKET, fd6 = INVALID_SOCKET;
    try {
        if ( isIPv4 ) {  // "0.0.0.0"
            fd4 = tcp_helpers::create_socket( 4, strBindAddressServer.c_str(), nSocketListenPort,
                [&]( socket_t sock, struct addrinfo& ai ) -> bool {
                    int ret = 0;
                    if (::bind( sock, ai.ai_addr, static_cast< int >( ai.ai_addrlen ) ) )
                        throw std::runtime_error( skutils::tools::format(
                            "Failed to bind IPv4 busy listener to port %d,error=%d=0x%x",
                            nSocketListenPort, ret, ret ) );
                    if (::listen( sock, 5 ) )  // listen through 5 channels
                        throw std::runtime_error( skutils::tools::format(
                            "Failed to start IPv4 busy listener to port %d,error=%d=0x%x",
                            nSocketListenPort, ret, ret ) );
                    return true;
                },
                0, is_reuse_address, is_reuse_port );
            if ( fd4 == INVALID_SOCKET )
                throw std::runtime_error( skutils::tools::format(
                    "Failed to create IPv4 busy listener on port %d", nSocketListenPort ) );
        }
        if ( isIPv6 ) {  // "0:0:0:0:0:0:0:0"
            fd6 = tcp_helpers::create_socket( 6, "::1", nSocketListenPort,
                [&]( socket_t sock, struct addrinfo& ai ) -> bool {
                    int ret = 0;
                    if (::bind( sock, ai.ai_addr, static_cast< int >( ai.ai_addrlen ) ) )
                        throw std::runtime_error( skutils::tools::format(
                            "Failed to bind IPv6 busy listener to port %d,error=%d=0x%x",
                            nSocketListenPort, ret, ret ) );
                    if (::listen( sock, 5 ) )  // listen through 5 channels
                        throw std::runtime_error( skutils::tools::format(
                            "Failed to start IPv6 busy listener to port %d,error=%d=0x%x",
                            nSocketListenPort, ret, ret ) );
                    return true;
                },
                0, is_reuse_address, is_reuse_port );
            if ( fd6 == INVALID_SOCKET )
                throw std::runtime_error( skutils::tools::format(
                    "Failed to create IPv6 busy listener on port %d", nSocketListenPort ) );
        }
        std::cout << ( cc::debug( "Will execute " ) + cc::note( "busy port" ) +
                       cc::debug( " callback..." ) + "\n" );
        if ( fnWorker )
            fnWorker();
        std::cout << ( cc::success( "Success, did executed " ) + cc::note( "busy port" ) +
                       cc::success( " callback" ) + "\n" );
    } catch ( std::exception& ex ) {
        std::string strErrorDescription = ex.what();
        bool isIgnoreError = false;
        if ( fnErrorHandler )
            isIgnoreError =
                fnErrorHandler( strErrorDescription );  // returns true if errror should be ignored
        if ( !isIgnoreError ) {
            std::cerr << ( cc::fatal( "FAILURE:" ) + cc::error( " Got exception from " ) +
                           cc::note( "busy port" ) + cc::error( " callback: " ) +
                           cc::warn( strErrorDescription ) + "\n" );
            // assert( false );
        }
    } catch ( ... ) {
        std::string strErrorDescription = "unknown exception";
        bool isIgnoreError = false;
        if ( fnErrorHandler )
            isIgnoreError =
                fnErrorHandler( strErrorDescription );  // returns true if errror should be ignored
        if ( !isIgnoreError ) {
            std::cerr << ( cc::fatal( "FAILURE:" ) + cc::error( " Got unknown exception from " ) +
                           cc::note( "busy port" ) + cc::error( " callback" ) + "\n" );
            // assert( false );
        }
    }
    if ( fd4 != INVALID_SOCKET )
        tcp_helpers::close_socket( fd4 );
    if ( fd6 != INVALID_SOCKET )
        tcp_helpers::close_socket( fd6 );
}

void with_server( fn_with_server_t fn, const std::string& strServerUrlScheme,
    const std::string& strBindAddressServer, const int nSocketListenPort ) {
    skutils::ws::security_args sa;
    std::string sus = skutils::tools::to_lower( strServerUrlScheme );
    std::shared_ptr< helper_server > pServer;
    if ( sus == "wss" ) {
        pServer.reset( new helper_server_wss( nSocketListenPort, strBindAddressServer ) );
        // assert( pServer->isSSL() );
    } else if ( sus == "ws" ) {
        pServer.reset( new helper_server_ws( nSocketListenPort, strBindAddressServer ) );
        // assert( !pServer->isSSL() );
    } else if ( sus == "https" || sus == "https_async" ) {
        pServer.reset( new helper_server_https( nSocketListenPort, strBindAddressServer, true ) );
        // assert( pServer->isSSL() );
    } else if ( sus == "https_sync" ) {
        pServer.reset( new helper_server_https( nSocketListenPort, strBindAddressServer, false ) );
        // assert( pServer->isSSL() );
    } else if ( sus == "http" || sus == "http_async" ) {
        pServer.reset( new helper_server_http( nSocketListenPort, strBindAddressServer, true ) );
        // assert( !pServer->isSSL() );
    } else if ( sus == "http_sync" ) {
        pServer.reset( new helper_server_http( nSocketListenPort, strBindAddressServer, false ) );
        // assert( !pServer->isSSL() );
    } else {
        std::cerr << ( cc::error( "Unknown server type: " ) + cc::warn( strServerUrlScheme ) +
                       "\n" );
        throw std::runtime_error( "Unknown server type: " + strServerUrlScheme );
    }
    pServer->run_parallel();
    try {
        std::cout << ( cc::debug( "Will execute " ) + cc::note( "server" ) +
                       cc::debug( " callback..." ) + "\n" );
        fn( *pServer.get() );
        std::cout << ( cc::success( "Success, did executed " ) + cc::note( "server" ) +
                       cc::success( " callback" ) + "\n" );
    } catch ( std::exception& ex ) {
        std::cerr << ( cc::fatal( "FAILURE:" ) + cc::error( " Got exception from " ) +
                       cc::note( "server" ) + cc::error( " callback: " ) + cc::warn( ex.what() ) +
                       "\n" );
        // assert( false );
    } catch ( ... ) {
        std::cerr << ( cc::fatal( "FAILURE:" ) + cc::error( " Got unknown exception from " ) +
                       cc::note( "server" ) + cc::error( " callback" ) + "\n" );
        // assert( false );
    }
    pServer->stop();
}

void with_client(
    fn_with_client_t fn, const std::string& strClientName, const std::string& strServerUrlScheme,
    const std::string& strBindAddressClient, const int nSocketListenPort,
    bool runClientInOtherThread,   // = false // typically, this is never needed
    const size_t nConnectAttempts  // = 10
) {
    if ( runClientInOtherThread ) {
        std::cout << ( cc::debug( "Starting client " ) + cc::info( strClientName ) +
                       cc::debug( " thread..." ) + "\n" );
        std::atomic_bool bClientThreadFinished( false );
        std::thread clientThread( [&]() {
            with_client( fn, strClientName, strServerUrlScheme, strBindAddressClient,
                nSocketListenPort, false, nConnectAttempts );
            bClientThreadFinished = true;
            std::cout << ( cc::debug( " client " ) + cc::info( strClientName ) +
                           cc::debug( " thread will exit" ) + "\n" );
        } );
        std::cout << ( cc::success( "Done, client " ) + cc::info( strClientName ) +
                       cc::success( " thread is running" ) + "\n" );
        std::cout << ( cc::debug( "Waiting for client thread to finish..." ) + "\n" );
        while ( !bClientThreadFinished )
            std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
        try {
            if ( clientThread.joinable() )
                clientThread.join();
        } catch ( ... ) {
        }
        if ( clientThread.joinable() ) {
            std::cerr << ( cc::fatal( "Client thread is still running" ) + "\n" );
            throw std::runtime_error( "Client thread is still running" );
        }
        std::cout << ( cc::success( "Done" ) + cc::debug( ",  client " ) +
                       cc::info( strClientName ) + cc::debug( " thread is finished" ) + "\n" );
        return;
    }  // if( runClientInOtherThread )

    std::string sus = skutils::tools::to_lower( strServerUrlScheme );
    std::shared_ptr< helper_client > pClient;
    if ( sus == "wss" ) {
        pClient.reset(
            new helper_client_wss( strClientName.c_str(), nSocketListenPort, strBindAddressClient, nConnectAttempts ) );
        // assert( pClient->isSSL() );
    } else if ( sus == "ws" ) {
        pClient.reset(
            new helper_client_ws( strClientName.c_str(), nSocketListenPort, strBindAddressClient, nConnectAttempts ) );
        // assert( !pClient->isSSL() );
    } else if ( sus == "https" || sus == "https_async" || sus == "https_sync" ) {
        pClient.reset(
            new helper_client_https( strClientName.c_str(), nSocketListenPort, strBindAddressClient, nConnectAttempts ) );
        // assert( pClient->isSSL() );
    } else if ( sus == "http" || sus == "http_async" || sus == "http_sync" ) {
        pClient.reset(
            new helper_client_http( strClientName.c_str(), nSocketListenPort, strBindAddressClient, nConnectAttempts ) );
        // assert( !pClient->isSSL() );
    } else {
        std::cerr << ( cc::error( "Unknown client type: " ) + cc::warn( strServerUrlScheme ) +
                       "\n" );
        throw std::runtime_error( "Unknown client type: " + strServerUrlScheme );
    }
    try {
        std::cerr << ( cc::debug( "Will execute client " ) + cc::info( pClient->strClientName_ ) +
                       cc::debug( " callback..." ) + "\n" );
        fn( *pClient.get() );
        std::cerr << ( cc::success( "Success, did executed client " ) +
                       cc::info( pClient->strClientName_ ) + cc::success( " callback" ) + "\n" );
    } catch ( std::exception& ex ) {
        std::cerr << ( cc::fatal( "FAILURE:" ) + cc::error( " Got exception from client " ) +
                       cc::info( pClient->strClientName_ ) + cc::error( " callback: " ) +
                       cc::warn( ex.what() ) + "\n" );
        // assert( false );
    } catch ( ... ) {
        std::cerr << ( cc::fatal( "FAILURE:" ) +
                       cc::error( " Got unknown exception from client " ) +
                       cc::info( pClient->strClientName_ ) + cc::error( " callback" ) + "\n" );
        // assert( false );
    }
    pClient->stop();
}

extern void with_clients( fn_with_client_t fn, const std::vector< std::string >& vecClientNames,
    const std::string& strServerUrlScheme, const std::string& strBindAddressClient,
    const int nSocketListenPort,
    const size_t nConnectAttempts  // = 10
) {
    size_t i, cnt = vecClientNames.size();
    if ( cnt == 0 )
        return;
    std::vector< std::thread > vecThreads;
    vecThreads.reserve( cnt );
    std::atomic_size_t cntRunningThreads = cnt;
    for ( i = 0; i < cnt; ++i )
        vecThreads.emplace_back(
            std::thread( [fn, i, strServerUrlScheme, strBindAddressClient, nSocketListenPort,
                             nConnectAttempts, &cntRunningThreads, &vecClientNames]() -> void {
                with_client( fn, vecClientNames[i], strServerUrlScheme, strBindAddressClient,
                    nSocketListenPort, false, nConnectAttempts );
                std::cout << ( cc::debug( "  client " ) + cc::info( vecClientNames[i] ) +
                               cc::debug( " thread will exit" ) + "\n" );
                --cntRunningThreads;
            } ) );
    std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
    for ( size_t idxWait = 0; cntRunningThreads > 0; ++idxWait ) {
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
        if ( idxWait % 1000 == 0 && idxWait != 0 )
            std::cout << ( cc::debug( "Waiting for " ) + cc::size10( cntRunningThreads ) +
                           cc::debug( " client thread(s)" ) + "\n" );
    }
    for ( i = 0; i < cnt; ++i ) {
        try {
            vecThreads[i].join();
        } catch ( ... ) {
        }
    }
}

void with_client_server( fn_with_client_server_t fn, const std::string& strClientName,
    const std::string& strServerUrlScheme, const std::string& strBindAddressServer,
    const std::string& strBindAddressClient, const int nSocketListenPort,
    bool runClientInOtherThread,   // = false // typically, this is never needed
    const size_t nConnectAttempts  // = 10
) {
    with_server(
        [&]( helper_server& refServer ) -> void {
            with_client(
                [&]( helper_client& refClient ) -> void {
                    try {
                        std::cout << ( cc::debug( "Will execute " ) + cc::note( "client-server" ) +
                                       cc::debug( " callback..." ) + "\n" );
                        fn( refServer, refClient );
                        std::cout << ( cc::success( "Success, did executed " ) +
                                       cc::note( "client-server" ) + cc::success( " callback" ) +
                                       "\n" );
                    } catch ( std::exception& ex ) {
                        std::cerr << ( cc::fatal( "FAILURE:" ) +
                                       cc::error( " Got exception from " ) +
                                       cc::note( "client-server" ) + cc::error( " callback: " ) +
                                       cc::warn( ex.what() ) + "\n" );
                        // assert( false );
                    } catch ( ... ) {
                        std::cerr << ( cc::fatal( "FAILURE:" ) +
                                       cc::error( " Got unknown exception from " ) +
                                       cc::note( "client-server" ) + cc::error( " callback" ) +
                                       "\n" );
                        // assert( false );
                    }
                },
                strClientName, strServerUrlScheme, strBindAddressClient, nSocketListenPort,
                runClientInOtherThread, nConnectAttempts );
        },
        strServerUrlScheme, strBindAddressServer, nSocketListenPort );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void helper_protocol_busy_port(
    const char* strProtocol, const char* strBindAddressServer, int nPort ) {
    std::cout << ( cc::debug( "Protocol busy port check" ) + "\n" );
    with_busy_tcp_port(
        [&]() -> void {  // fn_with_busy_tcp_port_worker_t
            std::cout << ( cc::sunny( "Busy port allocated" ) + "\n" );
            with_server(
                [&]( helper_server & /*refServer*/ ) -> void {
                    std::cout << ( cc::sunny( "Server startup" ) + "\n" );
                    std::cout << ( cc::sunny( "WE SHOULD NOT REACH THIS EXECUTION POINT" ) + "\n" );
                    // assert( false );
                },
                strProtocol, strBindAddressServer, nPort );
            std::cout << ( cc::sunny( "Server finish" ) + "\n" );
        },
        [&]( const std::string& strErrorDescription ) -> bool {  // fn_with_busy_tcp_port_error_t
            std::cout << ( cc::success( "Busy port detected with message: " ) +
                           cc::bright( strErrorDescription ) + "\n" );
            std::cout << ( cc::success( "SUCCESS - busy port handled" ) + "\n" );
            return true;  // returns true if errror should be ignored
        },
        strBindAddressServer, nPort );
    std::cout << ( cc::sunny( "Busy port de-allocated" ) + "\n" );
}

void helper_protocol_rest_call( const char* strProtocol, const char* strBindAddressServer,
    const char* strBindAddressClient, int nPort, bool isAutoExitOnSuccess ) {
    std::atomic_bool end_reached = false, end_2_reached = false;
    with_server(
        [&]( helper_server & /*refServer*/ ) -> void {
            try {
                std::string strCall(
                    "{ \"id\": \"1234567\", \"method\": \"hello\", \"params\": {} }" );
                nlohmann::json joCall =
                    ensure_call_id_present_copy( nlohmann::json::parse( strCall ) );
                std::cout << ( cc::normal( "Startup" ) + "\n" );
                std::string strURL = skutils::tools::format(
                    "%s://%s:%d", strProtocol, strBindAddressClient, nPort );
                skutils::url u( strURL );
                skutils::rest::client restCall( u );
                std::cout << ( cc::info( "input" ) + cc::debug( "..........." ) +
                               cc::normal( joCall.dump() ) + "\n" );
                skutils::rest::data_t dataOut = restCall.call( strCall );
                // assert( ! dataOut.empty() );
                nlohmann::json joResult = nlohmann::json::parse( dataOut.s_ );
                std::cout << ( cc::info( "output" ) + cc::debug( ".........." ) +
                               cc::normal( joResult.dump() ) + "\n" );
                // assert( joCall.dump() == joResult.dump() );
                end_reached = true;
                std::cout << ( cc::success( "Finish" ) + "\n" );
                if ( isAutoExitOnSuccess )
                    _exit( __EXIT_SUCCESS );
            } catch ( std::exception& ex ) {
                std::string strErrorDescription = ex.what();
                std::cerr << ( cc::fatal( "FAILURE:" ) + cc::error( " Got in-test exception: " ) +
                               cc::warn( strErrorDescription ) + "\n" );
                // assert( false );
                _exit( __EXIT_ERROR_IN_TEST_EXCEPTOION );
            } catch ( ... ) {
                std::string strErrorDescription = "unknown exception";
                std::cerr << ( cc::fatal( "FAILURE:" ) + cc::error( " Got in-test exception: " ) +
                               cc::warn( strErrorDescription ) + "\n" );
                // assert( false );
                _exit( __EXIT_ERROR_IN_TEST_UNKNOWN_EXCEPTOION );
            }
        },
        strProtocol, strBindAddressServer, nPort );
    if ( !end_reached )
        _exit( __EXIT_ERROR_REST_CALL_FAILED );
}

void helper_protocol_echo_server( const char* strProtocol, const char* strBindAddressServer, int nPort ) {
    with_server(
        [&]( helper_server & /*refServer*/ ) -> void {
            std::cout << ( cc::success( "Echo server started" ) + "\n" );
            for( ; true; ) {
                std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
            }
        },
        strProtocol, strBindAddressServer, nPort );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main( int argc, char** argv ) {
    std::string strProtocol = "https", strBindAddressServer = "localhost",
                strBindAddressClient = "localhost", strPathSslKey, strPathSslCert;
    int nPort = 27890;
    bool isEchoServerMode = false;
    skutils::command_line::parser clp( "skaled_ssl_test", "1.0.0" );
    clp.on( "version", "Show version information.",
           [&]() -> void {
               std::cout << clp.banner_text();
               exit( 0 );
           } )
        .on( "help", "Show help.",
            [&]() -> void {
                std::cout << clp.banner_text() << clp.options_text();
                exit( 0 );
            } )
        .on( "ssl-key", "Specifies path to existing valid SSL key file.",
            [&]( const std::string& strValue ) -> void {
                strPathSslKey = skutils::tools::trim_copy( strValue );
            } )
        .on( "ssl-cert", "Specifies path to existing valid SSL certificate file file.",
            [&]( const std::string& strValue ) -> void {
                strPathSslCert = skutils::tools::trim_copy( strValue );
            } )
        .on( "bind",
            "Bind RPC server to specified address and use it in client, default is \"localhost\".",
            [&]( const std::string& strValue ) -> void {
                std::string strBindAddress = skutils::tools::trim_copy( strValue );
                if ( strBindAddress.empty() ) {
                    std::cerr << "Bad bind address specified in command line arguments.\n";
                    _exit( __EXIT_ERROR_BAD_BIND_ADDRESS );
                }
                strBindAddressServer = strBindAddressClient = strBindAddress;
            } )
        .on( "bind-server", "Bind RPC server to specified address, default is \"localhost\".",
            [&]( const std::string& strValue ) -> void {
                strBindAddressServer = skutils::tools::trim_copy( strValue );
                if ( strBindAddressServer.empty() ) {
                    std::cerr << "Bad bind address specified in command line arguments.\n";
                    _exit( __EXIT_ERROR_BAD_BIND_ADDRESS );
                }
            } )
        .on( "bind-client", "Connect client to specified address, default is \"localhost\".",
            [&]( const std::string& strValue ) -> void {
                strBindAddressClient = skutils::tools::trim_copy( strValue );
                if ( strBindAddressClient.empty() ) {
                    std::cerr << "Bad bind address specified in command line arguments.\n";
                    _exit( __EXIT_ERROR_BAD_BIND_ADDRESS );
                }
            } )
        .on( "proto",
            "Run RPC server(s) on specified protocol(\"http\", \"https\", \"ws\", \"wss\", default "
            "is \"https\").",
            [&]( const std::string& strValue ) -> void {
                strProtocol = skutils::tools::trim_copy( strValue );
                if ( !( strProtocol == "http" || strProtocol == "https" || strProtocol == "ws" ||
                         strProtocol == "wss" ) ) {
                    std::cerr << "Bad protocol name specified in command line arguments, need "
                                 "\"http\", \"https\", \"ws\" or \"wss\".\n";
                    _exit( __EXIT_ERROR_BAD_PROTOCOL_NAME );
                }
            } )
        .on( "port", "Run RPC server(s) on specified port(default is 27890).",
            [&]( const std::string& strValue ) -> void {
                std::string strPort = skutils::tools::trim_copy( strValue );
                if ( !strPort.empty() ) {
                    nPort = atoi( strPort.c_str() );
                    if ( !( 0 <= nPort && nPort <= 65535 ) )
                        nPort = -1;
                }
            } )
        .on( "echo", "Run echo server only.",
            [&]() -> void {
                isEchoServerMode = true;
            } )
        .on( "ws-mode",
            "Run \"ws\" or \"wss\" RPC server(s) using specified mode(" +
                skutils::ws::nlws::list_srvmodes_as_str() + "); default mode is " +
                skutils::ws::nlws::srvmode2str( skutils::ws::nlws::g_default_srvmode ) + ".",
            [&]( const std::string& strValue ) -> void {
                skutils::ws::nlws::g_default_srvmode = skutils::ws::nlws::str2srvmode( strValue );
            } )
        .on( "ws-log",
            "Web socket debug logging mode(\"none\", \"basic\", \"detailed\"; default is "
            "\"none\").",
            [&]( const std::string& strValue ) -> void {
                skutils::ws::g_eWSLL = skutils::ws::str2wsll( strValue );
            } );
    if ( !clp.parse( argc, argv ) ) {
        std::cerr << "Failed to parse command line arguments\n";
        _exit( __EXIT_ERROR_FAILED_PARSE_CLI_ARGS );
    }
    if ( nPort <= 0 ) {
        std::cerr << "Valid server port number (--port) must be specified to start RPC server(s). "
                     "See --help\n";
        _exit( __EXIT_ERROR_BAD_PORT_NUMBER );
    }
    bool bNeedSSL = ( strProtocol == "https" || strProtocol == "wss" ) ? true : false;
    if ( bNeedSSL ) {
        bool bHaveSSL =
            ( ( !strPathSslKey.empty() ) && ( !strPathSslCert.empty() ) ) ? true : false;
        if ( !bHaveSSL ) {
            std::cerr
                << "Both SSL certificate(--ssl-cert) and key(--ssl-key) file must be specified. "
                   "See --help\n";
            _exit( __EXIT_ERROR_BAD_SSL_FILE_PATHS );
        }
        helper_ssl_cert_and_key_holder::g_strFilePathKey = strPathSslKey;
        helper_ssl_cert_and_key_holder::g_strFilePathCert = strPathSslCert;
        std::cout << ( "Using specified SSL key file \"" + strPathSslKey + "\"(if available)\n" );
        std::cout << ( "Using specified SSL certificate file \"" + strPathSslCert +
                       "\"(if available)\n" );
    }
    std::string strURLsrv = skutils::tools::format(
        "%s://%s:%d", strProtocol.c_str(), strBindAddressServer.c_str(), nPort );
    std::cout << ( "RPC server URL is \"" + strURLsrv + "\"\n" );

    if( isEchoServerMode ) {
        helper_protocol_echo_server( strProtocol.c_str(), strBindAddressServer.c_str(), nPort );
        _exit( __EXIT_SUCCESS );
    }

    std::string strURLcli = skutils::tools::format(
        "%s://%s:%d", strProtocol.c_str(), strBindAddressClient.c_str(), nPort );
    std::cout << ( "RPC client URL is \"" + strURLcli + "\"\n" );

    helper_protocol_rest_call( strProtocol.c_str(), strBindAddressServer.c_str(),
        strBindAddressClient.c_str(), nPort, true );
    _exit( __EXIT_SUCCESS );
}
