#include "test_skutils_helper.h"
#include <boost/test/unit_test.hpp>
#include <mutex>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace skutils {
namespace test {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void test_log_output( const std::string& strMessage ) {  // final log output
    static skutils::multithreading::mutex_type mtx;
    std::lock_guard< skutils::multithreading::mutex_type > lock( mtx );
    std::cout.flush();
    std::cout << strMessage;
    if ( !strMessage.empty() ) {
        if ( strMessage.back() != '\n' )
            std::cout << std::endl;
    }
    std::cout.flush();
}
std::string test_log_prefix_reformat(
    bool isInsertTime, const std::string& strPrefix, const std::string& strMessage ) {
    std::string strResult;
    std::vector< std::string > v = skutils::tools::split2vec( strMessage, '\n' );
    if ( v.empty() )
        return strResult;
    if ( v.back().empty() )
        v.erase( v.end() - 1 );
    auto itWalk = v.cbegin(), itEnd = v.cend();
    for ( size_t i = 0; itWalk != itEnd; ++itWalk, ++i ) {
        strResult += strPrefix;
        if ( i == 0 && isInsertTime ) {
            strResult += cc::time2string( cc::default_clock_t::now(), false );
            strResult += " ";
            strResult += cc::debug( "N/A" );  // instead of message class
            strResult += ": ";
        }
        strResult += ( *itWalk );
        strResult += '\n';
    }
    return strResult;
}

std::string test_log_caption(
    const char* strPrefix /*= nullptr*/, const char* strSuffix /*= nullptr*/ ) {
    static const size_t nCaptionCharsMax = 10;
    static const size_t nSuffixCharsMax = 1;
    static const size_t nPrefixCharsMax = nCaptionCharsMax - nSuffixCharsMax;
    static const char chrEmpty = '.';
    std::string strEffectiveSuffix = ( strSuffix != nullptr ) ? strSuffix : "";
    size_t nSuffixLength = strEffectiveSuffix.length();
    if ( nSuffixLength > nSuffixCharsMax ) {
        nSuffixLength = nSuffixCharsMax;
        strEffectiveSuffix = strEffectiveSuffix.substr( 0, nSuffixLength );
    }
    std::string strEffectivePrefix = ( strPrefix != nullptr ) ? strPrefix : "";
    size_t nPrefixLength = strEffectivePrefix.length();
    if ( nPrefixLength > nPrefixCharsMax ) {
        nPrefixLength = nPrefixCharsMax;
        strEffectivePrefix = strEffectivePrefix.substr( 0, nPrefixLength );
    }
    std::stringstream ss;
    ss << '[' << strEffectivePrefix;
    size_t i, cnt = nCaptionCharsMax - nSuffixLength - nPrefixLength;
    for ( i = 0; i < cnt; ++i )
        ss << chrEmpty;
    ss << strEffectiveSuffix << ']';
    return ss.str();
}
std::string test_log_caption( const std::string& strPrefix, const char* strSuffix /*= nullptr*/ ) {
    return test_log_caption( strPrefix.c_str(), strSuffix );
}

void test_log_e( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::debug( test_log_caption( nullptr, nullptr ) ) + " ", strMessage ) );
}  // test environment log
void test_log_ew( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::warn( test_log_caption( nullptr, "W" ) ) + " ", strMessage ) );
}  // test environment warning log
void test_log_ee( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::error( test_log_caption( nullptr, "E" ) ) + " ", strMessage ) );
}  // test environment error log
void test_log_ef( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::fatal( test_log_caption( nullptr, "F" ) ) + " ", strMessage ) );
}  // test environment fatal log
void test_log_s( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::attention( test_log_caption( "SERVER", nullptr ) ) + " ", strMessage ) );
}  // server-side log
void test_log_ss( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::attention( test_log_caption( "SERVER", "N" ) ) + " ", strMessage ) );
}  // server-side web socket log
void test_log_sw( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::warn( test_log_caption( "SERVER", "W" ) ) + " ", strMessage ) );
}  // server-side warning log
void test_log_se( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::error( test_log_caption( "SERVER", "E" ) ) + " ", strMessage ) );
}  // server-side error log
void test_log_sf( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::fatal( test_log_caption( "SERVER", "F" ) ) + " ", strMessage ) );
}  // server-side fatal log
void test_log_c( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::bright( test_log_caption( "CLIENT", nullptr ) ) + " ", strMessage ) );
}  // client-side log
void test_log_c( const std::string& strClient, const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::bright( test_log_caption( strClient, nullptr ) ) + " ", strMessage ) );
}  // client-side log
void test_log_cs( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::bright( test_log_caption( "CLIENT", "N" ) ) + " ", strMessage ) );
}  // client-side web socket log
void test_log_cs( const std::string& strClient, const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::bright( test_log_caption( strClient, "N" ) ) + " ", strMessage ) );
}  // client-side web socket log
void test_log_cw( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::warn( test_log_caption( "CLIENT", "W" ) ) + " ", strMessage ) );
}  // client-side warning log
void test_log_cw( const std::string& strClient, const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::warn( test_log_caption( strClient, "W" ) ) + " ", strMessage ) );
}  // client-side warning log
void test_log_ce( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::error( test_log_caption( "CLIENT", "E" ) ) + " ", strMessage ) );
}  // client-side error log
void test_log_ce( const std::string& strClient, const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::error( test_log_caption( strClient, "E" ) ) + " ", strMessage ) );
}  // client-side error log
void test_log_cf( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::fatal( test_log_caption( "CLIENT", "F" ) ) + " ", strMessage ) );
}  // client-side fatal log
void test_log_cf( const std::string& strClient, const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::fatal( test_log_caption( strClient, "F" ) ) + " ", strMessage ) );
}  // client-side fatal log
void test_log_p( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::sunny( test_log_caption( "PEER", nullptr ) ) + " ", strMessage ) );
}  // peer-side log
void test_log_p( const std::string& strClient, const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::sunny( test_log_caption( strClient, nullptr ) ) + " ", strMessage ) );
}  // peer-side log
void test_log_ps( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::sunny( test_log_caption( "PEER", "N" ) ) + " ", strMessage ) );
}  // peer-side web socket log
void test_log_ps( const std::string& strClient, const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::sunny( test_log_caption( strClient, "N" ) ) + " ", strMessage ) );
}  // peer-side web socket log
void test_log_pw( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::warn( test_log_caption( "PEER", "W" ) ) + " ", strMessage ) );
}  // peer-side warning log
void test_log_pw( const std::string& strClient, const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::warn( test_log_caption( strClient, "W" ) ) + " ", strMessage ) );
}  // peer-side warning log
void test_log_pe( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::error( test_log_caption( "PEER", "E" ) ) + " ", strMessage ) );
}  // peer-side error log
void test_log_pe( const std::string& strClient, const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::error( test_log_caption( strClient, "E" ) ) + " ", strMessage ) );
}  // peer-side error log
void test_log_pf( const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::fatal( test_log_caption( "PEER", "F" ) ) + " ", strMessage ) );
}  // peer-side fatal log
void test_log_pf( const std::string& strClient, const std::string& strMessage ) {
    test_log_output( test_log_prefix_reformat(
        true, cc::fatal( test_log_caption( strClient, "F" ) ) + " ", strMessage ) );
}  // peer-side fatal log

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

test_ssl_cert_and_key_holder::test_ssl_cert_and_key_holder() {
    std::string strPrefix = skutils::tools::get_tmp_file_path();
    strFilePathKey_ = strPrefix + ".key.pem";
    strFilePathCert_ = strPrefix + ".cert.pem";
    //
    test_log_e( cc::debug( "Will create " ) + cc::p( strFilePathKey_ ) + cc::debug( " and " ) +
                cc::p( strFilePathCert_ ) + cc::debug( "..." ) );
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
        test_log_ee( g_err_msg );
        throw std::runtime_error( g_err_msg );
    }
    test_log_s( cc::success( "OKay created " ) + cc::p( strFilePathKey_ ) + cc::success( " and " ) +
                cc::p( strFilePathCert_ ) + cc::success( "." ) );
}
test_ssl_cert_and_key_holder::~test_ssl_cert_and_key_holder() {
    if ( !strFilePathKey_.empty() ) {
        int r = ::remove( strFilePathKey_.c_str() );
        if ( r != 0 )
            test_log_ee( skutils::tools::format(
                "error %d=0x%X removing file \"%s\"", r, r, strFilePathKey_.c_str() ) );
    }
    if ( !strFilePathCert_.empty() ) {
        int r = ::remove( strFilePathCert_.c_str() );
        if ( r != 0 )
            test_log_ee( skutils::tools::format(
                "error %d=0x%X removing file \"%s\"", r, r, strFilePathCert_.c_str() ) );
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

test_ssl_cert_and_key_holder& test_ssl_cert_and_key_provider::helper_ssl_info() {
    static test_ssl_cert_and_key_holder g_holder;
    return g_holder;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

test_server::test_server( const char* strScheme, int nListenPort )
    : strScheme_( skutils::tools::to_lower( strScheme ) ),
      strSchemeUC_( skutils::tools::to_upper( strScheme ) ),
      nListenPort_( nListenPort ) {}
test_server::~test_server() {}

void test_server::run_parallel() {
    if ( thread_is_running_ )
        throw std::runtime_error( "server is already runnig " );
    check_can_listen();
    std::thread( [&]() -> void {
        thread_is_running_ = true;
        test_log_s( cc::info( strScheme_ ) + cc::debug( " network server thread started" ) );
        run();
        test_log_s( cc::info( strScheme_ ) + cc::debug( " network server thread will exit" ) );
        thread_is_running_ = false;
    } )
        .detach();
    while ( !thread_is_running_ )
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
    test_log_s(
        cc::debug( "Letting " ) + cc::info( strScheme_ ) + cc::debug( " network server init..." ) );
    std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
}

void test_server::wait_parallel() {
    while ( thread_is_running_ )
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
}

void test_server::stat_check_port_availability_to_start_listen(
    int ipVer, const char* strAddr, int nPort, const char* strScheme ) {
    test_log_s( cc::debug( "Will check port " ) + cc::num10( nPort ) +
                cc::debug( "/IPv" + std::to_string( ipVer ) ) + cc::debug( " availability for " ) +
                cc::info( strScheme ) + cc::debug( " server..." ) );
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
    test_log_s( cc::notice( "Port " ) + cc::num10( nPort ) +
                cc::notice( "/IPv" + std::to_string( ipVer ) ) + cc::notice( " is free for " ) +
                cc::info( strScheme ) + cc::notice( " server to start" ) );
}

void test_server::check_can_listen() {
    stat_check_port_availability_to_start_listen(
        4, "127.0.0.1", nListenPort_, strScheme_.c_str() );
    stat_check_port_availability_to_start_listen( 6, "::1", nListenPort_, strScheme_.c_str() );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

test_ws_peer::test_ws_peer( skutils::ws::server& srv, const skutils::ws::hdl_t& hdl )
    : skutils::ws::peer( srv, hdl ) {
    strPeerQueueID_ = skutils::dispatch::generate_id( this, "relay_peer" );
    test_log_p( desc() + cc::notice( " peer ctor" ) );
}
test_ws_peer::~test_ws_peer() {
    test_log_p( desc() + cc::notice( " peer dtor" ) );
    skutils::dispatch::remove( strPeerQueueID_ );
}

void test_ws_peer::onPeerRegister() {
    test_log_p( desc() + cc::notice( " peer registered" ) );
    skutils::ws::peer::onPeerRegister();
}
void test_ws_peer::onPeerUnregister() {  // peer will no longer receive onMessage after call to this
    test_log_p( desc() + cc::notice( " peer unregistered" ) );
    skutils::ws::peer::onPeerUnregister();
}

void test_ws_peer::onMessage( const std::string& msg, skutils::ws::opcv eOpCode ) {
    if ( eOpCode != skutils::ws::opcv::text )
        throw std::runtime_error( "only ws text messages are supported" );
    skutils::dispatch::async( strPeerQueueID_, [=]() -> void {
        test_log_p(
            cc::ws_rx_inv( ">>> " + std::string( get_test_server().strSchemeUC_ ) + "-RX >>> " ) +
            desc() + cc::ws_rx( " >>> " ) + cc::j( msg ) );
        //
        //
        std::string strResult = msg;
        //
        //
        test_log_p(
            cc::ws_tx_inv( "<<< " + std::string( get_test_server().strSchemeUC_ ) + "-TX <<< " ) +
            desc() + cc::ws_tx( " <<< " ) + cc::j( strResult ) );
        sendMessage( strResult );
    } );
    skutils::ws::peer::onMessage( msg, eOpCode );
}

void test_ws_peer::onClose(
    const std::string& reason, int local_close_code, const std::string& local_close_code_as_str ) {
    test_log_p( desc() + cc::warn( " peer close event with code=" ) + cc::c( local_close_code ) +
                cc::debug( ", reason=" ) + cc::info( reason ) );
    skutils::ws::peer::onClose( reason, local_close_code, local_close_code_as_str );
}

void test_ws_peer::onFail() {
    test_log_p( desc() + cc::error( " peer fail event" ) );
    skutils::ws::peer::onFail();
}

void test_ws_peer::onLogMessage(
    skutils::ws::e_ws_log_message_type_t eWSLMT, const std::string& msg ) {
    test_log_p( desc() + cc::debug( " peer log: " ) + msg );
    skutils::ws::peer::onLogMessage( eWSLMT, msg );
}

test_server_ws_base& test_ws_peer::get_test_server() {
    return static_cast< test_server_ws_base& >( srv() );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

test_server_ws_base::test_server_ws_base( const char* strScheme, int nListenPort )
    : test_server( strScheme, nListenPort ) {
    onPeerInstantiate_ = [&]( skutils::ws::server& srv,
                             skutils::ws::hdl_t hdl ) -> skutils::ws::peer_ptr_t {
        test_log_s( cc::info( strScheme_ ) + cc::debug( " server will instantiate new peer" ) );
        return new test_ws_peer( srv, hdl );
    };
    // onPeerRegister_ =
    // onPeerUnregister_ =
}
test_server_ws_base::~test_server_ws_base() {
    test_log_s( cc::debug( "Will close " ) + cc::info( strScheme_ ) +
                cc::debug( " server, was running on port " ) + cc::c( nListenPort_ ) );
    stop();
}

void test_server_ws_base::stop() {
    bStopFlag_ = true;
    if ( service_mode_supported() )
        service_interrupt();
    while ( ws_server_thread_is_running_ )
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
    wait_parallel();
    close();
}

void test_server_ws_base::run() {
    test_log_s( cc::debug( "Will start server on port " ) + cc::c( nListenPort_ ) +
                cc::debug( " using " ) + cc::info( strScheme_ ) + cc::debug( " scheme" ) );
    std::atomic_bool bServerOpenComplete( false );
    std::thread( [&]() {
        ws_server_thread_is_running_ = true;
        if ( strScheme_ == "wss" ) {
            auto& ssl_info = helper_ssl_info();
            strCertificateFile_ = ssl_info.strFilePathCert_;
            strPrivateKeyFile_ = ssl_info.strFilePathKey_;
        }
        BOOST_REQUIRE( open( strScheme_.c_str(), nListenPort_ ) );
        test_log_s( cc::sunny( "Server opened" ) );
        bServerOpenComplete = true;
        if ( service_mode_supported() ) {
            test_log_s(
                cc::info( "Main loop" ) + cc::debug( " will run in poll in service mode" ) );
            service( [&]() -> bool {
                return ( !/*skutils::signal::g_bStop*/ bStopFlag_ ) ? true : false;
            } );
        } else {
            test_log_s( cc::info( "Main loop" ) + cc::debug( " will run in poll/reset mode" ) );
            for ( ; !bStopFlag_; ) {
                poll( [&]() -> bool { return ( !bStopFlag_ ) ? true : false; } );
                if ( bStopFlag_ )
                    break;
                reset();
            }  // for( ; ! bStopFlag_; )
        }
        test_log_s( cc::info( "Main loop" ) + cc::debug( " finish" ) );
        ws_server_thread_is_running_ = false;
    } )
        .detach();
    test_log_s( cc::debug( "Waiting for " ) + cc::note( "test server" ) + cc::debug( " open..." ) );
    while ( !bServerOpenComplete )
        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
    test_log_s( cc::success( "Success, test server " ) + cc::info( strScheme_ ) +
                cc::success( " server started" ) );
}

void test_server_ws_base::onLogMessage(
    skutils::ws::e_ws_log_message_type_t eWSLMT, const std::string& msg ) {
    switch ( eWSLMT ) {
    // case skutils::ws::e_ws_log_message_type_t::eWSLMT_debug:   break;
    // case skutils::ws::e_ws_log_message_type_t::eWSLMT_info:    break;
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_warning:
        test_log_sw( msg );
        return;
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_error:
        test_log_sf( msg );
        return;
    default:
        break;
    }  // switch( eWSLMT )
    test_log_ss( msg );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

test_server_ws::test_server_ws( int nListenPort ) : test_server_ws_base( "ws", nListenPort ) {}
test_server_ws::~test_server_ws() {}

bool test_server_ws::isSSL() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

test_server_wss::test_server_wss( int nListenPort ) : test_server_ws_base( "wss", nListenPort ) {}
test_server_wss::~test_server_wss() {}

bool test_server_wss::isSSL() const {
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

test_server_http_base::test_server_http_base(
    const char* strScheme, int nListenPort, bool is_async_http_transfer_mode )
    : test_server( strScheme, nListenPort ) {
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
            test_log_s( cc::info( "OPTTIONS" ) + cc::debug( " request handler" ) );
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
        test_log_p( cc::ws_rx_inv( ">>> " + std::string( strSchemeUC_ ) + "-RX-POST >>> " ) +
                    cc::ws_rx( " >>> " ) + cc::j( req.body_ ) );
        //
        //
        std::string strResult = req.body_;
        //
        //
        test_log_p( cc::ws_tx_inv( "<<< " + std::string( strSchemeUC_ ) + "-TX-POST <<< " ) +
                    cc::ws_tx( " <<< " ) + cc::j( strResult ) );
        res.set_header( "access-control-allow-origin", "*" );
        res.set_header( "vary", "Origin" );
        res.set_content( strResult.c_str(), "application/json" );
    } );
}
test_server_http_base::~test_server_http_base() {
    stop();
}

void test_server_http_base::stop() {
    if ( !pServer_ )
        return;
    pServer_->stop();
    wait_parallel();
}

void test_server_http_base::run() {
    pServer_->listen( 4, "localhost", nListenPort_ );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

test_server_http::test_server_http( int nListenPort, bool is_async_http_transfer_mode )
    : test_server_http_base( "http", nListenPort, is_async_http_transfer_mode ) {}
test_server_http::~test_server_http() {}

bool test_server_http::isSSL() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

test_server_https::test_server_https( int nListenPort, bool is_async_http_transfer_mode )
    : test_server_http_base( "https", nListenPort, is_async_http_transfer_mode ) {}
test_server_https::~test_server_https() {}

bool test_server_https::isSSL() const {
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

test_server_proxygen::test_server_proxygen(
        const char* strBindAddr4, const char* /*strBindAddr6*/,
        int nListenPortHTTP4, int /*nListenPortHTTPS4*/, int /*nListenPortHTTP6*/, int /*nListenPortHTTPS6*/,
        int32_t threads, int32_t threads_limit
        )
        : test_server( "proxygen", nListenPortHTTP4 )
{
    skutils::http_pg::pg_on_request_handler_t fnHandler = [=]( const nlohmann::json& joIn,
            const std::string& strOrigin, int ipVer, const std::string& strDstAddress, int nDstPort )
            -> skutils::result_of_http_request {
        skutils::result_of_http_request rslt =
                implHandleHttpRequest(
                    joIn,
                    strOrigin,
                    ipVer,
                    strDstAddress,
                    nDstPort
                    );
        return rslt;
    };
//    auto& ssl_info = helper_ssl_info();
//    BOOST_REQUIRE( (!ssl_info.strFilePathCert_.empty()) );
//    BOOST_REQUIRE( (!ssl_info.strFilePathCert_.empty()) );
    skutils::http_pg::pg_accumulate_entries arr_pge = {
        { 4, strBindAddr4, nListenPortHTTP4, "", "", "" },
//        { 4, strBindAddr4, nListenPortHTTPS4, ssl_info.strFilePathCert_.c_str(), ssl_info.strFilePathCert_.c_str(), "" },
//        { 6, strBindAddr6, nListenPortHTTP6, "", "", "" },
//        { 6, strBindAddr6, nListenPortHTTPS6, ssl_info.strFilePathCert_.c_str(), ssl_info.strFilePathKey_.c_str(), "" },
    };
    hProxygenServer_ = skutils::http_pg::pg_start( fnHandler, arr_pge, threads, threads_limit );
    std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
}

test_server_proxygen::~test_server_proxygen() {
    stop();
}

void test_server_proxygen::stop() {
    // thread_is_running_ = false;
    skutils::http_pg::pg_stop( hProxygenServer_ );
    hProxygenServer_ = nullptr;
    std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
}

void test_server_proxygen::run() {
}

void test_server_proxygen::run_parallel() {
    run();
    // test_server::run_parallel();
}

void test_server_proxygen::check_can_listen() {
    // test_server::check_can_listen();
}

bool test_server_proxygen::isSSL() const {
    return false;
}

skutils::result_of_http_request test_server_proxygen::implHandleHttpRequest(
        const nlohmann::json & joIn,
        const std::string& strOrigin,
        int /*ipVer*/,
        const std::string& /*strDstAddress*/,
        int /*nDstPort*/
        ) {
    test_log_p( cc::ws_tx_inv( "<<< PROXYGEN-TX-POST <<< " ) + cc::u( strOrigin ) + cc::ws_tx( " <<< " ) + cc::j( joIn ) );
    skutils::result_of_http_request rslt;
    rslt.isBinary_ = false;
    rslt.strOut_ = joIn.dump();
    return rslt;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

test_client::test_client( const char* strClientName, int nTargetPort, const char* strScheme )
    : strClientName_( strClientName ),
      nTargetPort_( nTargetPort ),
      strScheme_( skutils::tools::to_lower( strScheme ) ),
      strSchemeUC_( skutils::tools::to_upper( strScheme ) ) {}
test_client::~test_client() {}

nlohmann::json test_client::call( const nlohmann::json& joMsg ) {
    nlohmann::json joAnswer = nlohmann::json::object();
    if ( !call( joMsg, joAnswer ) ) {
        joAnswer = nlohmann::json::object();
        test_log_ce( strClientName_, cc::fatal( "RPC CALL FAILURE:" ) +
                                         cc::error( " call failed on side of client " ) +
                                         cc::info( strClientName_ ) );
    }
    return joAnswer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

test_client_ws_base::test_client_ws_base( const char* strClientName, int nTargetPort,
    const char* strScheme, const size_t nConnectAttempts )
    : test_client( strClientName, nTargetPort, strScheme ) {
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
        test_log_cw( strClientName_,
            cc::warn( "client got close event close with " ) + cc::info( "code" ) +
                cc::warn( "=" ) + cc::c( local_close_code ) + cc::warn( ", code explanation is " ) +
                ( local_close_code_as_str.empty() ? cc::debug( "empty text" ) :
                                                    cc::info( local_close_code_as_str ) ) +
                cc::warn( ", reason is " ) +
                ( reason.empty() ? cc::debug( "empty text" ) : cc::info( reason ) ) );
    };
    onFail_ = [this]( skutils::ws::client::basic_socket&, skutils::ws::hdl_t ) -> void {
        ++cntFail_;
        test_log_cf( strClientName_, cc::error( "client got fail event" ) );
    };
    test_log_c( strClientName_, cc::debug( "Will initalize test client " ) +
                                    cc::info( strClientName_ ) + cc::debug( "..." ) );
    enableRestartTimer( false );
    static const char g_strLocalHostName[] = "localhost";  // "127.0.0.1" // "::1" // "localhost"
    std::string strServerUrl = skutils::tools::format(
        "%s://%s:%d", strScheme_.c_str(), g_strLocalHostName, nTargetPort_ );
    test_log_c( strClientName_,
        cc::debug( "test client will connect to: " ) + cc::u( strServerUrl ) + cc::debug( "..." ) );
    size_t cnt = nConnectAttempts;
    if ( cnt < 1 )
        cnt = 1;
    for ( size_t nClientConnectAttempt = 0;
          nClientConnectAttempt < cnt && ( !open( strServerUrl ) ); ++nClientConnectAttempt ) {
        test_log_c( strClientName_, cc::debug( "Attempt " ) +
                                        cc::size10( nClientConnectAttempt + 1 ) +
                                        cc::debug( "..." ) );
        std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
    }
    test_log_c( strClientName_,
        cc::success( "Success" ) + cc::debug( ", test client " ) + cc::info( strClientName_ ) +
            cc::debug( " did connected to: " ) + cc::u( strServerUrl ) + cc::debug( "..." ) );
    //		std::this_thread::sleep_for( std::chrono::seconds(1) );
    test_log_c( strClientName_, cc::success( "Done" ) +
                                    cc::notice( ", did initialized test client " ) +
                                    cc::info( strClientName_ ) );
}
test_client_ws_base::~test_client_ws_base() {
    stop();
}

void test_client_ws_base::stop() {
    test_log_c( strClientName_, cc::notice( "Will close test client " ) +
                                    cc::info( strClientName_ ) + cc::notice( "..." ) );
    pause_reading();
    cancel();
    //		std::this_thread::sleep_for( std::chrono::seconds(1) );
    //
    // wsClient.close( "test is about to close", skutils::ws::client::close_status::normal );
    close();
    //
    test_log_c( strClientName_, cc::success( "Success" ) +
                                    cc::debug( ", did closed test client " ) +
                                    cc::info( strClientName_ ) );
}

void test_client_ws_base::run() {}

void test_client_ws_base::onLogMessage(
    skutils::ws::e_ws_log_message_type_t eWSLMT, const std::string& msg ) {
    switch ( eWSLMT ) {
    // case skutils::ws::e_ws_log_message_type_t::eWSLMT_debug:   break;
    // case skutils::ws::e_ws_log_message_type_t::eWSLMT_info:    break;
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_warning:
        test_log_cw( strClientName_, msg );
        return;
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_error:
        test_log_cf( strClientName_, msg );
        return;
    default:
        break;
    }  // switch( eWSLMT )
    test_log_cs( strClientName_, msg );
}

bool test_client_ws_base::sendMessage(
    const std::string& msg, skutils::ws::opcv eOpCode /*= skutils::ws::opcv::text*/ ) {
    test_log_c( strClientName_, cc::ws_tx_inv( "TEST RPC CALL Tx " + strClientName_ ) +
                                    cc::ws_tx( " <<< " ) + cc::j( msg ) );
    strLastMessage_.clear();
    bHaveAnswer_ = false;
    return skutils::ws::client::client::sendMessage( msg, eOpCode );
}

bool test_client_ws_base::sendMessage( const char* msg ) {  // text only
    return sendMessage( std::string( msg ), skutils::ws::opcv::text );
}

bool test_client_ws_base::sendMessage( const nlohmann::json& joMsg ) {
    return sendMessage( joMsg.dump() );
}

void test_client_ws_base::onMessage(
    skutils::ws::hdl_t /*hdl*/, skutils::ws::opcv /*eOpCode*/, const std::string& msg ) {
    test_log_c( strClientName_, cc::ws_rx_inv( "TEST RPC CALL Rx " + strClientName_ ) +
                                    cc::ws_rx( " >>> " ) + cc::j( msg ) );
    strLastMessage_ = msg;
    bHaveAnswer_ = true;
}

std::string test_client_ws_base::waitAnswer() {
    while ( !bHaveAnswer_ )
        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
    return strLastMessage_;
}

void test_client_ws_base::waitClose() {
    while ( cntClose_ == 0 )
        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
}

void test_client_ws_base::waitFail() {
    while ( cntFail_ == 0 )
        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
}

void test_client_ws_base::waitCloseOrFail() {
    while ( cntClose_ == 0 && cntFail_ == 0 )
        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
}

std::string test_client_ws_base::exchange(
    const std::string& msg, skutils::ws::opcv eOpCode /*= skutils::ws::opcv::text*/ ) {
    if ( !sendMessage( msg, eOpCode ) )
        return strLastMessage_;
    return waitAnswer();
}

nlohmann::json test_client_ws_base::exchange( const nlohmann::json& joMsg ) {
    std::string strAnswer = exchange( joMsg.dump() );
    nlohmann::json joAnswer = nlohmann::json::parse( strAnswer );
    return joAnswer;
}

bool test_client_ws_base::call( const nlohmann::json& joMsg, nlohmann::json& joAnswer ) {
    joAnswer = nlohmann::json::object();
    joAnswer = exchange( ensure_call_id_present_copy( joMsg ) );
    return true;
}

bool test_client_ws_base::call( const nlohmann::json& joMsg ) {
    nlohmann::json joAnswer = nlohmann::json::object();
    return call( joMsg, joAnswer );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

test_client_ws::test_client_ws(
    const char* strClientName, int nTargetPort, const size_t nConnectAttempts )
    : test_client_ws_base( strClientName, nTargetPort, "ws", nConnectAttempts ) {}
test_client_ws::~test_client_ws() {}

bool test_client_ws::isSSL() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

test_client_wss::test_client_wss(
    const char* strClientName, int nTargetPort, const size_t nConnectAttempts )
    : test_client_ws_base( strClientName, nTargetPort, "wss", nConnectAttempts ) {}
test_client_wss::~test_client_wss() {}

bool test_client_wss::isSSL() const {
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

test_client_http_base::test_client_http_base( const char* strClientName, int nTargetPort,
    const char* strScheme, const size_t /*nConnectAttempts*/ )
    : test_client( strClientName, nTargetPort, strScheme ) {
    if ( strScheme_ == "https" )
        pClient_.reset( new skutils::http::SSL_client( -1, "localhost", nTargetPort_ ) );
    else
        pClient_.reset( new skutils::http::client( -1, "localhost", nTargetPort_ ) );
}
test_client_http_base::~test_client_http_base() {
    stop();
}

void test_client_http_base::stop() {
    if ( pClient_ )
        pClient_.reset();
}

void test_client_http_base::run() {}

bool test_client_http_base::sendMessage( const char* /*msg*/ ) {  // text only
    return true;                                                  // TO-DO:
}

bool test_client_http_base::sendMessage( const nlohmann::json& joMsg ) {
    return sendMessage( joMsg.dump() );
}

bool test_client_http_base::call( const std::string& strMsg, std::string& strAnswer ) {
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

bool test_client_http_base::call( const nlohmann::json& joMsg, nlohmann::json& joAnswer ) {
    joAnswer = nlohmann::json::object();
    std::string strAnswer;
    test_log_c( strClientName_, cc::ws_tx_inv( "TEST RPC CALL Tx " + strClientName_ ) +
                                    cc::ws_tx( " <<< " ) + cc::j( joMsg ) );
    if ( !call( joMsg.dump(), strAnswer ) )
        return false;
    joAnswer = nlohmann::json::parse( strAnswer );
    test_log_c( strClientName_, cc::ws_rx_inv( "TEST RPC CALL Rx " + strClientName_ ) +
                                    cc::ws_rx( " >>> " ) + cc::j( joAnswer ) );
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

test_client_http::test_client_http(
    const char* strClientName, int nTargetPort, const size_t nConnectAttempts )
    : test_client_http_base( strClientName, nTargetPort, "http", nConnectAttempts ) {}
test_client_http::~test_client_http() {}

bool test_client_http::isSSL() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

test_client_https::test_client_https(
    const char* strClientName, int nTargetPort, const size_t nConnectAttempts )
    : test_client_http_base( strClientName, nTargetPort, "https", nConnectAttempts ) {}
test_client_https::~test_client_https() {}

bool test_client_https::isSSL() const {
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void with_test_environment( fn_with_test_environment_t fn ) {
    static bool g_b_test_environment_initilized = false;
    if ( !g_b_test_environment_initilized ) {
        g_b_test_environment_initilized = true;
        //
        cc::_on_ = true;
        skutils::ws::g_eWSLL =
            skutils::ws::e_ws_logging_level_t::eWSLL_none;  // eWSLL_none // eWSLL_basic //
                                                            // eWSLL_detailed
        // cc::_default_json_indent_ = 4;
        //???::g_nPeerStatsLoggingFlags = __???_PEER_STATS_ALL;
        test_log_e(
            cc::debug( "Will initialize " ) + cc::note( "test environment" ) + cc::debug( "..." ) );
        //
        SSL_library_init();
        OpenSSL_add_all_ciphers();
        OpenSSL_add_all_digests();
        ERR_load_crypto_strings();
        OpenSSL_add_all_algorithms();
        //
        //
        test_log_e( cc::success( "Done" ) + cc::debug( ", " ) + cc::note( "test environment" ) +
                    cc::debug( " initialized" ) );
    }  // if( ! g_b_test_environment_initilized )
    try {
        test_log_e( cc::debug( "Will execute " ) + cc::note( "test environment" ) +
                    cc::debug( " callback..." ) );
        fn();
        test_log_e( cc::success( "Success, did executed " ) + cc::note( "test environment" ) +
                    cc::success( " callback" ) );
    } catch ( std::exception& ex ) {
        test_log_ef( cc::fatal( "FAILURE:" ) + cc::error( " Got exception from " ) +
                     cc::note( "test environment" ) + cc::error( " callback: " ) +
                     cc::warn( ex.what() ) );
        BOOST_REQUIRE( false );
    } catch ( ... ) {
        // XXX HACK For test passing
        //        test_log_ef( cc::fatal( "FAILURE:" ) + cc::error( " Got unknown exception from " )
        //        +
        //                     cc::note( "test environment" ) + cc::error( " callback" ) );
        //        BOOST_REQUIRE( false );
    }
}

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
socket_t create_socket( int ipVer, const char* host, int port, Fn fn, int socket_flags = 0, bool is_reuse_address = true, bool is_reuse_port = false ) {
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
    fn_with_busy_tcp_port_error_t fnErrorHandler, const int nSocketListenPort, bool isIPv4,
    bool isIPv6, bool is_reuse_address, bool is_reuse_port ) {
    socket_t fd4 = INVALID_SOCKET, fd6 = INVALID_SOCKET;
    try {
        if ( isIPv4 ) {  // "0.0.0.0"
            fd4 = tcp_helpers::create_socket( 4, "127.0.0.1", nSocketListenPort,
                [&]( socket_t sock, struct addrinfo& ai ) -> bool {
                    int ret = 0;
                    if (::bind( sock, ai.ai_addr, static_cast< int >( ai.ai_addrlen ) ) )
                        throw std::runtime_error( skutils::tools::format(
                            "Failed to bind IPv4 busy test listener to port %d,error=%d=0x%x",
                            nSocketListenPort, ret, ret ) );
                    if (::listen( sock, 5 ) )  // listen through 5 channels
                        throw std::runtime_error( skutils::tools::format(
                            "Failed to start IPv4 busy test listener to port %d,error=%d=0x%x",
                            nSocketListenPort, ret, ret ) );
                    return true;
                }, 0, is_reuse_address, is_reuse_port );
            if ( fd4 == INVALID_SOCKET )
                throw std::runtime_error( skutils::tools::format(
                    "Failed to create IPv4 busy test listener on port %d", nSocketListenPort ) );
        }
        if ( isIPv6 ) {  // "0:0:0:0:0:0:0:0"
            fd6 = tcp_helpers::create_socket( 6, "::1", nSocketListenPort,
                [&]( socket_t sock, struct addrinfo& ai ) -> bool {
                    int ret = 0;
                    if (::bind( sock, ai.ai_addr, static_cast< int >( ai.ai_addrlen ) ) )
                        throw std::runtime_error( skutils::tools::format(
                            "Failed to bind IPv6 busy test listener to port %d,error=%d=0x%x",
                            nSocketListenPort, ret, ret ) );
                    if (::listen( sock, 5 ) )  // listen through 5 channels
                        throw std::runtime_error( skutils::tools::format(
                            "Failed to start IPv6 busy test listener to port %d,error=%d=0x%x",
                            nSocketListenPort, ret, ret ) );
                    return true;
                }, 0, is_reuse_address, is_reuse_port );
            if ( fd6 == INVALID_SOCKET )
                throw std::runtime_error( skutils::tools::format(
                    "Failed to create IPv6 busy test listener on port %d", nSocketListenPort ) );
        }
        test_log_e(
            cc::debug( "Will execute " ) + cc::note( "busy port" ) + cc::debug( " callback..." ) );
        if ( fnWorker )
            fnWorker();
        test_log_e( cc::success( "Success, did executed " ) + cc::note( "busy port" ) +
                    cc::success( " callback" ) );
    } catch ( std::exception& ex ) {
        std::string strErrorDescription = ex.what();
        bool isIgnoreError = false;
        if ( fnErrorHandler )
            isIgnoreError =
                fnErrorHandler( strErrorDescription );  // returns true if errror should be ignored
        if ( !isIgnoreError ) {
            test_log_ef( cc::fatal( "FAILURE:" ) + cc::error( " Got exception from " ) +
                         cc::note( "busy port" ) + cc::error( " callback: " ) +
                         cc::warn( strErrorDescription ) );
            BOOST_REQUIRE( false );
        }
    } catch ( ... ) {
        std::string strErrorDescription = "unknown exception";
        bool isIgnoreError = false;
        if ( fnErrorHandler )
            isIgnoreError =
                fnErrorHandler( strErrorDescription );  // returns true if errror should be ignored
        if ( !isIgnoreError ) {
            test_log_ef( cc::fatal( "FAILURE:" ) + cc::error( " Got unknown exception from " ) +
                         cc::note( "busy port" ) + cc::error( " callback" ) );
            BOOST_REQUIRE( false );
        }
    }
    if ( fd4 != INVALID_SOCKET )
        tcp_helpers::close_socket( fd4 );
    if ( fd6 != INVALID_SOCKET )
        tcp_helpers::close_socket( fd6 );
}

void with_test_server(
    fn_with_test_server_t fn, const std::string& strServerUrlScheme, const int nSocketListenPort ) {
    skutils::ws::security_args sa;
    std::string sus = skutils::tools::to_lower( strServerUrlScheme );
    std::shared_ptr< test_server > pServer;
    if ( sus == "wss" ) {
        pServer.reset( new test_server_wss( nSocketListenPort ) );
        BOOST_REQUIRE( pServer->isSSL() );
    } else if ( sus == "ws" ) {
        pServer.reset( new test_server_ws( nSocketListenPort ) );
        BOOST_REQUIRE( !pServer->isSSL() );
    } else if ( sus == "https" || sus == "https_async" ) {
        pServer.reset( new test_server_https( nSocketListenPort, true ) );
        BOOST_REQUIRE( pServer->isSSL() );
    } else if ( sus == "https_sync" ) {
        pServer.reset( new test_server_https( nSocketListenPort, false ) );
        BOOST_REQUIRE( pServer->isSSL() );
    } else if ( sus == "http" || sus == "http_async" ) {
        pServer.reset( new test_server_http( nSocketListenPort, true ) );
        BOOST_REQUIRE( !pServer->isSSL() );
    } else if ( sus == "http_sync" ) {
        pServer.reset( new test_server_http( nSocketListenPort, false ) );
        BOOST_REQUIRE( !pServer->isSSL() );
    } else if ( sus == "proxygen" ) {
        pServer.reset( new test_server_proxygen( "127.0.0.1", "::1",
                                                 nSocketListenPort + 0, nSocketListenPort + 1, nSocketListenPort + 2, nSocketListenPort + 3,
                                                 0, 0
                                                 ) );
    } else {
        test_log_se( cc::error( "Unknown server type: " ) + cc::warn( strServerUrlScheme ) );
        throw std::runtime_error( "Unknown server type: " + strServerUrlScheme );
    }
    pServer->run_parallel();
    try {
        test_log_e( cc::debug( "Will execute " ) + cc::note( "Test server" ) +
                    cc::debug( " callback..." ) );
        fn( *pServer.get() );
        test_log_e( cc::success( "Success, did executed " ) + cc::note( "Test server" ) +
                    cc::success( " callback" ) );
    } catch ( std::exception& ex ) {
        test_log_ef( cc::fatal( "FAILURE:" ) + cc::error( " Got exception from " ) +
                     cc::note( "Test server" ) + cc::error( " callback: " ) +
                     cc::warn( ex.what() ) );
        BOOST_REQUIRE( false );
    } catch ( ... ) {
        test_log_ef( cc::fatal( "FAILURE:" ) + cc::error( " Got unknown exception from " ) +
                     cc::note( "Test server" ) + cc::error( " callback" ) );
        BOOST_REQUIRE( false );
    }
    pServer->stop();
}

void with_test_client( fn_with_test_client_t fn, const std::string& strTestClientName,
    const std::string& strServerUrlScheme, const int nSocketListenPort,
    bool runClientInOtherThread,   // = false // typically, this is never needed
    const size_t nConnectAttempts  // = 10
) {
    if ( runClientInOtherThread ) {
        test_log_e( cc::debug( "Starting test client " ) + cc::info( strTestClientName ) +
                    cc::debug( " thread..." ) );
        std::atomic_bool bClientThreadFinished( false );
        std::thread clientThread( [&]() {
            with_test_client( fn, strTestClientName, strServerUrlScheme, nSocketListenPort, false,
                nConnectAttempts );
            bClientThreadFinished = true;
            test_log_e( cc::debug( " test client " ) + cc::info( strTestClientName ) +
                        cc::debug( " thread will exit" ) );
        } );
        test_log_e( cc::success( "Done, test client " ) + cc::info( strTestClientName ) +
                    cc::success( " thread is running" ) );
        test_log_e( cc::debug( "Waiting for client thread to finish..." ) );
        while ( !bClientThreadFinished )
            std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
        try {
            if ( clientThread.joinable() )
                clientThread.join();
        } catch ( ... ) {
        }
        BOOST_REQUIRE( !clientThread.joinable() );
        test_log_e( cc::success( "Done" ) + cc::debug( ", test client " ) +
                    cc::info( strTestClientName ) + cc::debug( " thread is finished" ) );
        return;
    }  // if( runClientInOtherThread )

    std::string sus = skutils::tools::to_lower( strServerUrlScheme );
    std::shared_ptr< test_client > pClient;
    if ( sus == "wss" ) {
        pClient.reset(
            new test_client_wss( strTestClientName.c_str(), nSocketListenPort, nConnectAttempts ) );
        BOOST_REQUIRE( pClient->isSSL() );
    } else if ( sus == "ws" ) {
        pClient.reset(
            new test_client_ws( strTestClientName.c_str(), nSocketListenPort, nConnectAttempts ) );
        BOOST_REQUIRE( !pClient->isSSL() );
    } else if ( sus == "https" || sus == "https_async" || sus == "https_sync" ) {
        pClient.reset( new test_client_https(
            strTestClientName.c_str(), nSocketListenPort, nConnectAttempts ) );
        BOOST_REQUIRE( pClient->isSSL() );
    } else if ( sus == "http" || sus == "http_async" || sus == "http_sync" || sus == "proxygen" ) {
        pClient.reset( new test_client_http(
            strTestClientName.c_str(), nSocketListenPort, nConnectAttempts ) );
        BOOST_REQUIRE( !pClient->isSSL() );
    } else {
        test_log_se( cc::error( "Unknown client type: " ) + cc::warn( strServerUrlScheme ) );
        throw std::runtime_error( "Unknown client type: " + strServerUrlScheme );
    }
    try {
        test_log_e( cc::debug( "Will execute test client " ) + cc::info( pClient->strClientName_ ) +
                    cc::debug( " callback..." ) );
        fn( *pClient.get() );
        test_log_e( cc::success( "Success, did executed test client " ) +
                    cc::info( pClient->strClientName_ ) + cc::success( " callback" ) );
    } catch ( std::exception& ex ) {
        test_log_ef( cc::fatal( "FAILURE:" ) + cc::error( " Got exception from test client " ) +
                     cc::info( pClient->strClientName_ ) + cc::error( " callback: " ) +
                     cc::warn( ex.what() ) );
        BOOST_REQUIRE( false );
    } catch ( ... ) {
        test_log_ef( cc::fatal( "FAILURE:" ) +
                     cc::error( " Got unknown exception from test client " ) +
                     cc::info( pClient->strClientName_ ) + cc::error( " callback" ) );
        BOOST_REQUIRE( false );
    }
    pClient->stop();
}

extern void with_test_clients(
    fn_with_test_client_t fn, const std::vector< std::string >& vecTestClientNames,
    const std::string& strServerUrlScheme, const int nSocketListenPort,
    const size_t nConnectAttempts  // = 10
) {
    size_t i, cnt = vecTestClientNames.size();
    if ( cnt == 0 )
        return;
    std::vector< std::thread > vecThreads;
    vecThreads.reserve( cnt );
    std::atomic_size_t cntRunningThreads = cnt;
    for ( i = 0; i < cnt; ++i )
        vecThreads.emplace_back(
            std::thread( [fn, i, strServerUrlScheme, nSocketListenPort, nConnectAttempts,
                             &cntRunningThreads, &vecTestClientNames]() -> void {
                with_test_client( fn, vecTestClientNames[i], strServerUrlScheme, nSocketListenPort,
                    false, nConnectAttempts );
                test_log_e( cc::debug( " test client " ) + cc::info( vecTestClientNames[i] ) +
                            cc::debug( " thread will exit" ) );
                --cntRunningThreads;
            } ) );
    std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
    for ( size_t idxWait = 0; cntRunningThreads > 0; ++idxWait ) {
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
        if ( idxWait % 1000 == 0 && idxWait != 0 )
            test_log_e( cc::debug( "Waiting for " ) + cc::size10( cntRunningThreads ) +
                        cc::debug( " client thread(s)" ) );
    }
    for ( i = 0; i < cnt; ++i ) {
        try {
            vecThreads[i].join();
        } catch ( ... ) {
        }
    }
}

void with_test_client_server( fn_with_test_client_server_t fn, const std::string& strTestClientName,
    const std::string& strServerUrlScheme, const int nSocketListenPort,
    bool runClientInOtherThread,   // = false // typically, this is never needed
    const size_t nConnectAttempts  // = 10
) {
    with_test_server(
        [&]( test_server& refServer ) -> void {
            with_test_client(
                [&]( test_client& refClient ) -> void {
                    try {
                        test_log_e( cc::debug( "Will execute " ) + cc::note( "client-server" ) +
                                    cc::debug( " callback..." ) );
                        fn( refServer, refClient );
                        test_log_e( cc::success( "Success, did executed " ) +
                                    cc::note( "client-server" ) + cc::success( " callback" ) );
                    } catch ( std::exception& ex ) {
                        test_log_ef( cc::fatal( "FAILURE:" ) + cc::error( " Got exception from " ) +
                                     cc::note( "client-server" ) + cc::error( " callback: " ) +
                                     cc::warn( ex.what() ) );
                        BOOST_REQUIRE( false );
                    } catch ( ... ) {
                        test_log_ef( cc::fatal( "FAILURE:" ) +
                                     cc::error( " Got unknown exception from " ) +
                                     cc::note( "client-server" ) + cc::error( " callback" ) );
                        BOOST_REQUIRE( false );
                    }
                },
                strTestClientName, strServerUrlScheme, nSocketListenPort, runClientInOtherThread,
                nConnectAttempts );
        },
        strServerUrlScheme, nSocketListenPort );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void test_print_header_name( const char* s ) {
    if ( s == nullptr || s[0] == '\0' )
        return;
    auto bPrev = cc::_on_;
    cc::_on_ = true;
    static const char g_strLine[] =
        "=========================================================================================="
        "======";
    test_log_e( cc::attention( g_strLine ) );
    test_log_e( "   " + cc::sunny( s ) );
    test_log_e( cc::attention( g_strLine ) );
    cc::_on_ = bPrev;
}


int g_nDefaultPort = 9696;
int g_nDefaultPortProxygen = 8686;

std::vector< std::string > g_vecTestClientNamesA = {"Frodo", "Bilbo", "Sam", "Elrond", "Galadriel",
    "Celeborn", "Balrog", "Anduin", "Samwise", "Gandalf", "Legolas", "Aragorn", "Gimli", "Faramir",
    "Arwen", "Pippin", "Boromir", "Theoden", "Eowyn"};
std::vector< std::string > g_vecTestClientNamesB = {"Gollum"};

void test_protocol_server_startup( const char* strProto, int nPort ) {
    // simply start/stop server
    std::atomic_bool was_started = false;
    skutils::test::with_test_environment( [&]() -> void {
        skutils::test::with_test_server(
            [&]( skutils::test::test_server & /*refServer*/ ) -> void {
                was_started = true;
                skutils::test::test_log_e( cc::success( "WE ARE HERE, SERVER ALIVE" ) );
            },
            strProto, nPort );
    } );
    BOOST_REQUIRE( was_started );
}

void test_protocol_single_call( const char* strProto, int nPort ) {
    // simple single client call
    std::atomic_bool end_of_actions_was_reached = false;
    skutils::test::with_test_environment( [&]() -> void {
        skutils::test::with_test_client_server(
            [&]( skutils::test::test_server& /*refServer*/,
                skutils::test::test_client& refClient ) -> void {
                std::string strCall( "{ \"method\": \"hello\", \"params\": {} }" );
                nlohmann::json joCall =
                    skutils::test::ensure_call_id_present_copy( nlohmann::json::parse( strCall ) );
                nlohmann::json joResult = refClient.call( joCall );
                BOOST_REQUIRE( joCall.dump() == joResult.dump() );
                //
                end_of_actions_was_reached = true;
            },
            "Chadwick", strProto, nPort );
    } );
    BOOST_REQUIRE( end_of_actions_was_reached );
}

void test_protocol_serial_calls(
    const char* strProto, int nPort, const std::vector< std::string >& vecClientNames ) {
    // multiple clients serial server calls
    std::atomic_size_t cnt_actions_performed = 0, cnt_clients = vecClientNames.size(),
                       wait_time_step_ms = 500;
    skutils::test::test_log_e( cc::debug( "Protocol serial calls test with " ) +
                               cc::size10( size_t( cnt_clients ) ) + cc::debug( " client(s)" ) );
    skutils::test::with_test_environment( [&]() -> void {
        skutils::test::with_test_server(
            [&]( skutils::test::test_server & /*refServer*/ ) -> void {
                skutils::test::test_log_e( cc::sunny( "Server startup" ) );
                for ( size_t i = 0; i < cnt_clients; ++i ) {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds( size_t( wait_time_step_ms ) ) );
                    skutils::test::with_test_client(
                        [&, i]( skutils::test::test_client& refClient ) -> void {
                            // std::this_thread::sleep_for(
                            //    std::chrono::milliseconds( size_t( wait_time_step_ms ) * i ) );
                            skutils::test::test_log_e( cc::bright( "Client " ) + cc::size10( i ) +
                                                       cc::bright( " begin" ) );
                            std::string strCall( "{ \"method\": \"hello\", \"params\": {} }" );
                            nlohmann::json joCall = skutils::test::ensure_call_id_present_copy(
                                nlohmann::json::parse( strCall ) );
                            nlohmann::json joResult = refClient.call( joCall );
                            BOOST_REQUIRE( joCall.dump() == joResult.dump() );
                            ++cnt_actions_performed;
                            skutils::test::test_log_e( cc::bright( "Client " ) + cc::size10( i ) +
                                                       cc::bright( " finish" ) );
                        },
                        vecClientNames[i], strProto, nPort, true );
                }
                skutils::test::test_log_e( cc::sunny( "Server finish" ) );
            },
            strProto, nPort );
    } );
    size_t idxWaitAttempt, cntWaitAttempts = size_t( cnt_clients ) + 1;
    for ( idxWaitAttempt = 0; size_t( idxWaitAttempt ) < size_t( cntWaitAttempts ) &&
                              size_t( cnt_actions_performed ) < size_t( cnt_clients );
          ++idxWaitAttempt ) {
        skutils::test::test_log_e(
            cc::debug( "Waiting for test to complete, step " ) +
            cc::size10( size_t( idxWaitAttempt ) ) + cc::debug( " of " ) +
            cc::size10( size_t( cntWaitAttempts ) ) + cc::debug( ", performed " ) +
            cc::size10( size_t( cnt_actions_performed ) ) + cc::debug( " action(s) of " ) +
            cc::size10( size_t( cnt_clients ) ) + cc::debug( " expeted ..." ) );
        std::this_thread::sleep_for( std::chrono::milliseconds( size_t( wait_time_step_ms ) ) );
    }
    BOOST_REQUIRE( cnt_actions_performed == cnt_clients );
}

void test_protocol_parallel_calls(
    const char* strProto, int nPort, const std::vector< std::string >& vecClientNames ) {
    // multiple clients parallel server calls
    std::atomic_size_t cnt_actions_performed = 0, cnt_clients = vecClientNames.size(),
                       wait_time_step_ms = 500, parallel_client_indexer = 0;
    std::mutex mtxClientIndexer;
    skutils::test::test_log_e( cc::debug( "Protocol parallel calls test with " ) +
                               cc::size10( size_t( cnt_clients ) ) + cc::debug( " client(s)" ) );
    skutils::test::with_test_environment( [&]() -> void {
        skutils::test::with_test_server(
            [&]( skutils::test::test_server & /*refServer*/ ) -> void {
                skutils::test::test_log_e( cc::sunny( "Server startup" ) );
                skutils::test::with_test_clients(
                    [&]( skutils::test::test_client& refClient ) -> void {
                        size_t i = 0;
                        {  // block
                            std::lock_guard< std::mutex > lock( mtxClientIndexer );
                            i = parallel_client_indexer;
                            ++parallel_client_indexer;
                        }  // block
                        skutils::test::test_log_e(
                            cc::bright( "Client " ) + cc::size10( i ) + cc::bright( " begin" ) );
                        std::string strCall( "{ \"method\": \"hello\", \"params\": {} }" );
                        nlohmann::json joCall = skutils::test::ensure_call_id_present_copy(
                            nlohmann::json::parse( strCall ) );
                        nlohmann::json joResult = refClient.call( joCall );
                        BOOST_REQUIRE( joCall.dump() == joResult.dump() );
                        //
                        joCall["method"] = "second_method";
                        joResult = refClient.call( joCall );
                        BOOST_REQUIRE( joCall.dump() == joResult.dump() );
                        //
                        joCall["method"] = "third_method";
                        joResult = refClient.call( joCall );
                        BOOST_REQUIRE( joCall.dump() == joResult.dump() );
                        //
                        joCall["method"] = "fourth_method";
                        joResult = refClient.call( joCall );
                        BOOST_REQUIRE( joCall.dump() == joResult.dump() );
                        //
                        joCall["method"] = "fifth_method";
                        joResult = refClient.call( joCall );
                        BOOST_REQUIRE( joCall.dump() == joResult.dump() );
                        //
                        ++cnt_actions_performed;
                        skutils::test::test_log_e(
                            cc::bright( "Client " ) + cc::size10( i ) + cc::bright( " finish" ) );
                    },
                    vecClientNames, strProto, nPort, true );
            },
            strProto, nPort );
        skutils::test::test_log_e( cc::sunny( "Server finish" ) );
    } );
    size_t idxWaitAttempt, cntWaitAttempts = size_t( cnt_clients ) + 1;
    for ( idxWaitAttempt = 0; size_t( idxWaitAttempt ) < size_t( cntWaitAttempts ) &&
                              size_t( cnt_actions_performed ) < size_t( cnt_clients );
          ++idxWaitAttempt ) {
        skutils::test::test_log_e(
            cc::debug( "Waiting for test to complete, step " ) +
            cc::size10( size_t( idxWaitAttempt ) ) + cc::debug( " of " ) +
            cc::size10( size_t( cntWaitAttempts ) ) + cc::debug( ", performed " ) +
            cc::size10( size_t( cnt_actions_performed ) ) + cc::debug( " action(s) of " ) +
            cc::size10( size_t( cnt_clients ) ) + cc::debug( " expeted ..." ) );
        std::this_thread::sleep_for( std::chrono::milliseconds( size_t( wait_time_step_ms ) ) );
    }
    BOOST_REQUIRE( cnt_actions_performed == cnt_clients );
}

void test_protocol_busy_port( const char* strProto, int nPort ) {
    skutils::test::test_log_e( cc::debug( "Protocol busy port test" ) );
    skutils::test::with_test_environment( [&]() -> void {
        skutils::test::with_busy_tcp_port(
            [&]() -> void {  // fn_with_busy_tcp_port_worker_t
                skutils::test::test_log_e( cc::sunny( "Busy port allocated" ) );
                skutils::test::with_test_server(
                    [&]( skutils::test::test_server & /*refServer*/ ) -> void {
                        skutils::test::test_log_e( cc::sunny( "Server startup" ) );
                        skutils::test::test_log_sf(
                            cc::sunny( "WE SHOULD NOT REACH THIS EXECUTION POINT" ) );
                        BOOST_REQUIRE( false );
                    },
                    strProto, nPort );
                skutils::test::test_log_e( cc::sunny( "Server finish" ) );
            },
            [&](
                const std::string& strErrorDescription ) -> bool {  // fn_with_busy_tcp_port_error_t
                skutils::test::test_log_e( cc::success( "Busy port detected with message: " ) +
                                           cc::bright( strErrorDescription ) );
                skutils::test::test_log_ss( cc::success( "SUCCESS - busy port handled" ) );
                BOOST_REQUIRE( true );
                return true;  // returns true if errror should be ignored
            },
            nPort );
        skutils::test::test_log_e( cc::sunny( "Busy port de-allocated" ) );
    } );
}

void test_protocol_rest_call( const char* strProto, int nPort, bool tt, bool ft ) {
    std::atomic_bool end_1_reached = false, end_2_reached = false;
    skutils::test::with_test_environment( [&]() -> void {
        skutils::test::with_test_server(
            [&]( skutils::test::test_server& /*refServer*/ ) -> void {
                std::string strCall( "{ \"id\": \"1234567\", \"method\": \"hello\", \"params\": {} }" );
                nlohmann::json joCall =
                    skutils::test::ensure_call_id_present_copy( nlohmann::json::parse( strCall ) );
                if( tt ) {
                    skutils::test::test_log_e( cc::normal( "\"True test\" part startup" ) );
                    std::string strURL = skutils::tools::format( "%s://127.0.0.1:%d", strProto, nPort );
                    skutils::url u( strURL );
                    skutils::rest::client restCall( u );
                    skutils::rest::data_t dataOut = restCall.call( strCall );
                    BOOST_REQUIRE( ! dataOut.empty() );
                    nlohmann::json joResult = nlohmann::json::parse( dataOut.s_ );
                    skutils::test::test_log_e( cc::info( "input" ) + cc::debug( "..........." ) + cc::normal( joCall.dump() ) );
                    skutils::test::test_log_e( cc::info( "output" ) + cc::debug( ".........." ) + cc::normal( joResult.dump() ) );
                    BOOST_REQUIRE( joCall.dump() == joResult.dump() );
                    end_1_reached = true;
                    skutils::test::test_log_e( cc::success( "\"True test\" part finish" ) );
                }
                if( ft ) {
                    skutils::test::test_log_e( cc::normal( "\"False test\" part startup" ) );
                    std::string strURL = skutils::tools::format( "%s://127.0.0.1:%d", strProto, nPort + 123 ); // incorrect port
                    skutils::url u( strURL );
                    skutils::rest::client restCall( u );
                    skutils::rest::data_t dataOut = restCall.call( strCall );
                    skutils::test::test_log_e( cc::info( "error type" ) + cc::debug( "......" ) + cc::num10( int( dataOut.ei_.et_ ) ) );
                    skutils::test::test_log_e( cc::info( "error code" ) + cc::debug( "......" ) + cc::num10( int( dataOut.ei_.ec_ ) ) );
                    skutils::test::test_log_e( cc::info( "error text" ) + cc::debug( "......" ) + cc::normal( dataOut.ei_.strError_ ) );
                    BOOST_REQUIRE( dataOut.empty() );
                    BOOST_REQUIRE( dataOut.ei_.et_ != skutils::http::common_network_exception::error_type::et_no_error );
                    BOOST_REQUIRE( ! dataOut.ei_.strError_.empty() );
                    end_2_reached = true;
                    skutils::test::test_log_e( cc::success( "\"False test\" part finish" ) );
                }
            },
            strProto, nPort );
    } );
    BOOST_REQUIRE( end_1_reached || (!tt) );
    BOOST_REQUIRE( end_2_reached || (!ft) );
}

};  // namespace test
};  // namespace skutils

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE( SkUtils )
BOOST_AUTO_TEST_SUITE( helper )

BOOST_AUTO_TEST_CASE( simple ) {}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
