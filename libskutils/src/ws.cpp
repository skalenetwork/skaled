#include <skutils/console_colors.h>
#include <skutils/network.h>
#include <skutils/url.h>
#include <skutils/utils.h>
#include <skutils/ws.h>
#include <chrono>
#include <sstream>

#include <inttypes.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>

#if ( defined _WIN32 )
#include "gettimeofday.h"
#include <io.h>
#else
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>
#endif

namespace skutils {
namespace ws {

namespace utils {

bool is_json( const char* strProbablyJSON, nlohmann::json* p_jo /*= nullptr*/ ) {
    if ( strProbablyJSON == nullptr || strProbablyJSON[0] == '\0' )
        return false;
    try {
        nlohmann::json jo = nlohmann::json::parse( strProbablyJSON );
        if ( p_jo )
            ( *p_jo ) = jo;
        return true;
    } catch ( ... ) {
    }
    if ( p_jo )
        ( *p_jo ) = nlohmann::json();
    return false;
}
bool is_json( const std::string& strProbablyJSON, nlohmann::json* p_jo /*= nullptr*/ ) {
    if ( strProbablyJSON.empty() )
        return false;
    return is_json( strProbablyJSON.c_str(), p_jo );
}
bool is_json( const char* strProbablyJSON, const char* strMustHavePropertyName,
    const char* strMustHavePropertyValue ) {
    if ( strProbablyJSON == nullptr || strProbablyJSON[0] == '\0' )
        return false;
    nlohmann::json jo;
    if ( !is_json( strProbablyJSON, &jo ) )
        return false;
    return is_json( jo, strMustHavePropertyName, strMustHavePropertyValue );
}
bool is_json( const std::string& strProbablyJSON, const char* strMustHavePropertyName,
    const char* strMustHavePropertyValue ) {
    if ( strProbablyJSON.empty() )
        return false;
    return is_json( strProbablyJSON.c_str(), strMustHavePropertyName, strMustHavePropertyValue );
}
bool is_json( const nlohmann::json& jo, const char* strMustHavePropertyName,
    const char* strMustHavePropertyValue ) {
    if ( strMustHavePropertyName == nullptr || strMustHavePropertyName[0] == 0 )
        return true;
    try {
        if ( jo.count( strMustHavePropertyName ) == 0 )
            return false;
        const nlohmann::json& jo_result = jo[std::string( strMustHavePropertyName )];
        if ( !jo_result.is_string() )
            return false;
        std::string strResult = jo_result.get< std::string >();
        if ( skutils::tools::to_lower( strResult ) ==
             skutils::tools::to_lower( strMustHavePropertyValue ) )
            return true;
    } catch ( ... ) {
    }
    return false;
}
bool is_ping( const char* strProbablyJSON ) {
    return is_json( strProbablyJSON, "method", "ping" ) ||
           is_json( strProbablyJSON, "result", "ping" );
}
bool is_ping( const std::string& strProbablyJSON ) {
    return is_json( strProbablyJSON, "method", "ping" ) ||
           is_json( strProbablyJSON, "result", "ping" );
}
bool is_ping( const nlohmann::json& jo ) {
    return is_json( jo, "method", "ping" ) || is_json( jo, "result", "ping" );
}
bool is_pong( const char* strProbablyJSON ) {
    return is_json( strProbablyJSON, "method", "pong" ) ||
           is_json( strProbablyJSON, "result", "pong" );
}
bool is_pong( const std::string& strProbablyJSON ) {
    return is_json( strProbablyJSON, "method", "pong" ) ||
           is_json( strProbablyJSON, "result", "pong" );
}
bool is_pong( const nlohmann::json& jo ) {
    return is_json( jo, "method", "pong" ) || is_json( jo, "result", "pong" );
}

bool is_ping_or_pong( const char* strProbablyJSON ) {
    if ( strProbablyJSON == nullptr || strProbablyJSON[0] == '\0' )
        return false;
    nlohmann::json jo;
    if ( !is_json( strProbablyJSON, &jo ) )
        return false;
    return is_ping_or_pong( jo );
}
bool is_ping_or_pong( const std::string& strProbablyJSON ) {
    if ( strProbablyJSON.empty() )
        return false;
    return is_ping_or_pong( strProbablyJSON.c_str() );
}
bool is_ping_or_pong( const nlohmann::json& jo ) {
    if ( is_json( jo, "method", "ping" ) || is_json( jo, "result", "ping" ) ||
         is_json( jo, "result", "pong" ) || is_json( jo, "method", "pong" ) )
        return true;
    return false;
}

};  // namespace utils

traffic_stats::traffic_stats() {
    init();
}
traffic_stats::traffic_stats( traffic_stats::myrct x ) : skutils::stats::named_event_stats( x ) {
    init();
    assign( x );
}
traffic_stats::traffic_stats( traffic_stats::myrrt x ) : skutils::stats::named_event_stats( x ) {
    init();
    move( x );
}
traffic_stats::~traffic_stats() {
    clear();
}

traffic_stats::bytes_count_t traffic_stats::text_tx() const {
    lock_type lock( const_cast< myrt >( *this ) );
    bytes_count_t n = text_tx_;
    return n;
}
traffic_stats::bytes_count_t traffic_stats::text_rx() const {
    lock_type lock( const_cast< myrt >( *this ) );
    bytes_count_t n = text_rx_;
    return n;
}
traffic_stats::bytes_count_t traffic_stats::bin_tx() const {
    lock_type lock( const_cast< myrt >( *this ) );
    bytes_count_t n = bin_tx_;
    return n;
}
traffic_stats::bytes_count_t traffic_stats::bin_rx() const {
    lock_type lock( const_cast< myrt >( *this ) );
    bytes_count_t n = bin_rx_;
    return n;
}
traffic_stats::bytes_count_t traffic_stats::tx() const {
    lock_type lock( const_cast< myrt >( *this ) );
    bytes_count_t n = text_tx_ + bin_tx_;
    return n;
}
traffic_stats::bytes_count_t traffic_stats::rx() const {
    lock_type lock( const_cast< myrt >( *this ) );
    bytes_count_t n = text_rx_ + bin_rx_;
    return n;
}

using namespace skutils::stats;
double traffic_stats::bps_text_tx( time_point tpNow ) const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps( traffic_queue_text_tx_, tpNow );
    return lf;
}
double traffic_stats::bps_text_tx_last_known() const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps_last_known( traffic_queue_text_tx_ );
    return lf;
}
double traffic_stats::bps_text_tx() const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps_til_now( traffic_queue_text_tx_ );
    return lf;
}

double traffic_stats::bps_text_rx( time_point tpNow ) const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps( traffic_queue_text_rx_, tpNow );
    return lf;
}
double traffic_stats::bps_text_rx_last_known() const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps_last_known( traffic_queue_text_rx_ );
    return lf;
}
double traffic_stats::bps_text_rx() const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps_til_now( traffic_queue_text_rx_ );
    return lf;
}

double traffic_stats::bps_bin_tx( time_point tpNow ) const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps( traffic_queue_bin_tx_, tpNow );
    return lf;
}
double traffic_stats::bps_bin_tx_last_known() const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps_last_known( traffic_queue_bin_tx_ );
    return lf;
}
double traffic_stats::bps_bin_tx() const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps_til_now( traffic_queue_bin_tx_ );
    return lf;
}

double traffic_stats::bps_bin_rx( time_point tpNow ) const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps( traffic_queue_bin_rx_, tpNow );
    return lf;
}
double traffic_stats::bps_bin_rx_last_known() const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps_last_known( traffic_queue_bin_rx_ );
    return lf;
}
double traffic_stats::bps_bin_rx() const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps_til_now( traffic_queue_bin_rx_ );
    return lf;
}

double traffic_stats::bps_tx( time_point tpNow ) const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps( traffic_queue_all_tx_, tpNow );
    return lf;
}
double traffic_stats::bps_tx_last_known() const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps_last_known( traffic_queue_all_tx_ );
    return lf;
}
double traffic_stats::bps_tx() const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps_til_now( traffic_queue_all_tx_ );
    return lf;
}

double traffic_stats::bps_rx( time_point tpNow ) const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps( traffic_queue_all_rx_, tpNow );
    return lf;
}
double traffic_stats::bps_rx_last_known() const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps_last_known( traffic_queue_all_rx_ );
    return lf;
}
double traffic_stats::bps_rx() const {
    lock_type lock( const_cast< myrt >( *this ) );
    double lf = named_traffic_stats::stat_compute_bps_til_now( traffic_queue_all_rx_ );
    return lf;
}

traffic_stats::myrt traffic_stats::log_text_tx( bytes_count_t n ) {
    lock_type lock( *this );
    text_tx_ += n;
    traffic_record_item_t recNow( n );
    traffic_queue_all_tx_.push_back( recNow );
    traffic_queue_text_tx_.push_back( recNow );
    return ( *this );
}
traffic_stats::myrt traffic_stats::log_text_rx( traffic_stats::bytes_count_t n ) {
    lock_type lock( *this );
    text_rx_ += n;
    traffic_record_item_t recNow( n );
    traffic_queue_all_rx_.push_back( recNow );
    traffic_queue_text_rx_.push_back( recNow );
    return ( *this );
}
traffic_stats::myrt traffic_stats::log_bin_tx( traffic_stats::bytes_count_t n ) {
    lock_type lock( *this );
    bin_tx_ += n;
    traffic_record_item_t recNow( n );
    traffic_queue_all_tx_.push_back( recNow );
    traffic_queue_bin_tx_.push_back( recNow );
    return ( *this );
}
traffic_stats::myrt traffic_stats::log_bin_rx( traffic_stats::bytes_count_t n ) {
    lock_type lock( *this );
    bin_rx_ += n;
    traffic_record_item_t recNow( n );
    traffic_queue_all_rx_.push_back( recNow );
    traffic_queue_bin_rx_.push_back( recNow );
    return ( *this );
}

traffic_stats::e_last_instance_state_changing_type_t
traffic_stats::last_instance_state_changing_type() const {
    lock_type lock( const_cast< myrt >( *this ) );
    e_last_instance_state_changing_type_t e = elisctt_;
    return e;
}
std::string traffic_stats::last_instance_state_changing_type_as_str() const {
    lock_type lock( const_cast< myrt >( *this ) );
    switch ( elisctt_ ) {
    case elisctt_instantiated:
        return "instantiated";
    case elisctt_opened:
        return "opened";
    case elisctt_closed:
        return "closed";
    default:
        return "N/A-state";
    }
}
traffic_stats::time_point traffic_stats::instantiated() const {
    lock_type lock( const_cast< myrt >( *this ) );
    return time_stamp_instantiated_;
}
traffic_stats::nanoseconds traffic_stats::instantiated_ago(
    traffic_stats::time_point tpNow ) const {
    // lock_type lock( const_cast < myrt > ( *this ) );
    return std::chrono::duration_cast< nanoseconds >( tpNow - instantiated() );
}
traffic_stats::nanoseconds traffic_stats::instantiated_ago() const {
    // lock_type lock( const_cast < myrt > ( *this ) );
    return instantiated_ago( clock::now() );
}
traffic_stats::time_point traffic_stats::changed() const {
    lock_type lock( const_cast< myrt >( *this ) );
    switch ( elisctt_ ) {
    case elisctt_instantiated:
        return time_stamp_instantiated_;
    case elisctt_opened:
        return time_stamp_opened_;
    case elisctt_closed:
        return time_stamp_closed_;
    default:
        return clock::now();
    }
}
traffic_stats::nanoseconds traffic_stats::changed_ago( traffic_stats::time_point tpNow ) const {
    // lock_type lock( const_cast < myrt > ( *this ) );
    return std::chrono::duration_cast< nanoseconds >( tpNow - changed() );
}
traffic_stats::nanoseconds traffic_stats::changed_ago() const {
    // lock_type lock( const_cast < myrt > ( *this ) );
    return changed_ago( clock::now() );
}

void traffic_stats::log_open() {
    lock_type lock( *this );
    elisctt_ = elisctt_opened;
    time_stamp_opened_ = clock::now();
}
void traffic_stats::log_close() {
    lock_type lock( *this );
    elisctt_ = elisctt_closed;
    time_stamp_closed_ = clock::now();
}

size_t traffic_stats::g_nSizeDefaultOnQueueAdd = 10;
const char traffic_stats::g_strEventNameWebSocketFail[] = "fail";
const char traffic_stats::g_strEventNameWebSocketMessagesRecvText[] = "rx-txt";
const char traffic_stats::g_strEventNameWebSocketMessagesRecvBinary[] = "rx-bin";
const char traffic_stats::g_strEventNameWebSocketMessagesRecv[] = "rx";
const char traffic_stats::g_strEventNameWebSocketMessagesSentText[] = "tx-txt";
const char traffic_stats::g_strEventNameWebSocketMessagesSentBinary[] = "tx-bin";
const char traffic_stats::g_strEventNameWebSocketMessagesSent[] = "tx";
void traffic_stats::register_default_event_queues_for_web_socket() {
    event_queue_add( g_strEventNameWebSocketFail, g_nSizeDefaultOnQueueAdd );
    event_queue_add( g_strEventNameWebSocketMessagesRecvText, g_nSizeDefaultOnQueueAdd );
    event_queue_add( g_strEventNameWebSocketMessagesRecvBinary, g_nSizeDefaultOnQueueAdd );
    event_queue_add( g_strEventNameWebSocketMessagesRecv, g_nSizeDefaultOnQueueAdd );
    event_queue_add( g_strEventNameWebSocketMessagesSentText, g_nSizeDefaultOnQueueAdd );
    event_queue_add( g_strEventNameWebSocketMessagesSentBinary, g_nSizeDefaultOnQueueAdd );
    event_queue_add( g_strEventNameWebSocketMessagesSent, g_nSizeDefaultOnQueueAdd );
}
const char traffic_stats::g_strEventNameWebSocketPeerConnect[] = "peer connect";
const char traffic_stats::g_strEventNameWebSocketPeerDisconnect[] = "peer disconnect";
const char traffic_stats::g_strEventNameWebSocketPeerDisconnectFail[] = "peer disconnect fail";
void traffic_stats::register_default_event_queues_for_web_socket_peer() {
    register_default_event_queues_for_web_socket();
    event_queue_add( g_strEventNameWebSocketPeerConnect, g_nSizeDefaultOnQueueAdd );
    event_queue_add( g_strEventNameWebSocketPeerDisconnect, g_nSizeDefaultOnQueueAdd );
    event_queue_add( g_strEventNameWebSocketPeerDisconnectFail, g_nSizeDefaultOnQueueAdd );
}
const char traffic_stats::g_strEventNameWebSocketServerStart[] = "server start";
const char traffic_stats::g_strEventNameWebSocketServerStartFail[] = "server start fail";
const char traffic_stats::g_strEventNameWebSocketServerStop[] = "server stop";
void traffic_stats::register_default_event_queues_for_web_socket_server() {
    register_default_event_queues_for_web_socket();
    event_queue_add( g_strEventNameWebSocketPeerConnect, g_nSizeDefaultOnQueueAdd );
    event_queue_add( g_strEventNameWebSocketPeerDisconnect, g_nSizeDefaultOnQueueAdd );
    event_queue_add( g_strEventNameWebSocketPeerDisconnectFail, g_nSizeDefaultOnQueueAdd );
    event_queue_add( g_strEventNameWebSocketServerStart, g_nSizeDefaultOnQueueAdd );
    event_queue_add( g_strEventNameWebSocketServerStartFail, g_nSizeDefaultOnQueueAdd );
    event_queue_add( g_strEventNameWebSocketServerStop, g_nSizeDefaultOnQueueAdd );
}
const char traffic_stats::g_strEventNameWebSocketClientConnect[] = "connect";
const char traffic_stats::g_strEventNameWebSocketClientConnectFail[] = "fail connect";
const char traffic_stats::g_strEventNameWebSocketClientDisconnect[] = "disconnect";
const char traffic_stats::g_strEventNameWebSocketClientReconnect[] = "reconnect attempt";
void traffic_stats::register_default_event_queues_for_web_socket_client() {
    register_default_event_queues_for_web_socket();
    event_queue_add( g_strEventNameWebSocketClientConnect, g_nSizeDefaultOnQueueAdd );
    event_queue_add( g_strEventNameWebSocketClientConnectFail, g_nSizeDefaultOnQueueAdd );
    event_queue_add( g_strEventNameWebSocketClientDisconnect, g_nSizeDefaultOnQueueAdd );
    event_queue_add( g_strEventNameWebSocketClientReconnect, g_nSizeDefaultOnQueueAdd );
}

bool traffic_stats::empty() const {
    lock_type lock( const_cast< myrt >( *this ) );
    if ( text_tx_ != 0 || text_rx_ != 0 || bin_tx_ != 0 || bin_rx_ != 0 )
        return false;
    return true;
}
void traffic_stats::clear() {
    lock_type lock( *this );
    text_tx_ = text_rx_ = bin_tx_ = bin_rx_ = 0;
    elisctt_ = elisctt_instantiated;
    time_stamp_instantiated_ = time_stamp_opened_ = time_stamp_closed_ = clock::now();
    traffic_queue_all_tx_.clear();
    traffic_queue_all_rx_.clear();
    traffic_queue_text_tx_.clear();
    traffic_queue_text_rx_.clear();
    traffic_queue_bin_tx_.clear();
    traffic_queue_bin_rx_.clear();
    named_event_stats::clear();
}
void traffic_stats::init() {
    lock_type lock( *this );
    clear();
    limit( 50 );
}
traffic_stats::myrt traffic_stats::limit( size_t lim ) {
    traffic_queue_all_tx_.limit( lim );
    traffic_queue_all_rx_.limit( lim );
    traffic_queue_text_tx_.limit( lim );
    traffic_queue_text_rx_.limit( lim );
    traffic_queue_bin_tx_.limit( lim );
    traffic_queue_bin_rx_.limit( lim );
    return ( *this );
}
int traffic_stats::compare( traffic_stats::myrct x ) const {
    lock_type lock1( const_cast< myrt >( *this ) );
    lock_type lock2( const_cast< myrt >( x ) );
    if ( int( elisctt_ ) < int( x.elisctt_ ) )
        return -1;
    if ( int( elisctt_ ) > int( x.elisctt_ ) )
        return 1;
    // time_stamp_instantiated_, time_stamp_opened_, time_stamp_closed_ ... not used
    if ( bytes_count_t( text_tx_ ) < bytes_count_t( x.text_tx_ ) )
        return -1;
    if ( bytes_count_t( text_tx_ ) > bytes_count_t( x.text_tx_ ) )
        return 1;
    if ( bytes_count_t( text_rx_ ) < bytes_count_t( x.text_rx_ ) )
        return -1;
    if ( bytes_count_t( text_rx_ ) > bytes_count_t( x.text_rx_ ) )
        return 1;
    if ( bytes_count_t( bin_tx_ ) < bytes_count_t( x.bin_tx_ ) )
        return -1;
    if ( bytes_count_t( bin_tx_ ) > bytes_count_t( x.bin_tx_ ) )
        return 1;
    if ( bytes_count_t( bin_rx_ ) < bytes_count_t( x.bin_rx_ ) )
        return -1;
    if ( bytes_count_t( bin_rx_ ) > bytes_count_t( x.bin_rx_ ) )
        return 1;
    int n;
    n = traffic_queue_all_tx_.compare( x.traffic_queue_all_tx_ );
    if ( n )
        return n;
    n = traffic_queue_all_rx_.compare( x.traffic_queue_all_rx_ );
    if ( n )
        return n;
    n = traffic_queue_text_tx_.compare( x.traffic_queue_text_tx_ );
    if ( n )
        return n;
    n = traffic_queue_text_rx_.compare( x.traffic_queue_text_rx_ );
    if ( n )
        return n;
    n = traffic_queue_bin_tx_.compare( x.traffic_queue_bin_tx_ );
    if ( n )
        return n;
    n = traffic_queue_bin_rx_.compare( x.traffic_queue_bin_rx_ );
    if ( n )
        return n;
    n = named_event_stats::compare( x );
    if ( n )
        return n;
    return 0;
}
traffic_stats::myrt traffic_stats::assign( traffic_stats::myrct x ) {
    lock_type lock1( *this );
    lock_type lock2( const_cast< myrt >( x ) );
    elisctt_ = x.elisctt_;
    time_stamp_instantiated_ = x.time_stamp_instantiated_;
    time_stamp_opened_ = x.time_stamp_opened_;
    time_stamp_closed_ = x.time_stamp_closed_;
    text_tx_ = bytes_count_t( x.text_tx_ );
    text_rx_ = bytes_count_t( x.text_rx_ );
    bin_tx_ = bytes_count_t( x.bin_tx_ );
    bin_rx_ = bytes_count_t( x.bin_rx_ );
    traffic_queue_all_tx_ = x.traffic_queue_all_tx_;
    traffic_queue_all_rx_ = x.traffic_queue_all_rx_;
    traffic_queue_text_tx_ = x.traffic_queue_text_tx_;
    traffic_queue_text_rx_ = x.traffic_queue_text_rx_;
    traffic_queue_bin_tx_ = x.traffic_queue_bin_tx_;
    traffic_queue_bin_rx_ = x.traffic_queue_bin_rx_;
    named_event_stats::assign( x );
    return ( *this );
}
traffic_stats::myrt traffic_stats::move( traffic_stats::myrt x ) {
    lock_type lock1( *this );
    lock_type lock2( x );
    assign( x );
    x.clear();
    return ( *this );
}

void traffic_stats::lock() {}
void traffic_stats::unlock() {}

std::string traffic_stats::getLifeTimeDescription(
    traffic_stats::time_point tpNow, bool isColored /*= false*/ ) const {
    // lock_type lock( const_cast < myrt > ( *this ) );
    std::string strChangeType( last_instance_state_changing_type_as_str() ), strAgoSuffix( " ago" ),
        strSpace( " " );
    if ( isColored ) {
        strChangeType = cc::debug( strChangeType );
        strAgoSuffix = cc::debug( strAgoSuffix );
        strSpace = cc::debug( strSpace );
    }
    nanoseconds nsx = changed_ago( tpNow );
    uint64_t ns = nsx.count();
    std::stringstream ss;
    ss << strChangeType << strSpace << skutils::tools::nanoseconds_2_lifetime_str( ns, isColored )
       << strAgoSuffix;
    return ss.str();
}
std::string traffic_stats::getLifeTimeDescription( bool isColored /*= false*/ ) const {
    return getLifeTimeDescription( clock::now(), isColored );
}
std::string traffic_stats::getTrafficStatsDescription(
    traffic_stats::time_point tpNow, bool isColored /*= false*/ ) const {
    // lock_type lock( const_cast < myrt > ( *this ) );
    traffic_stats copy_of_this( *this );  // lock-free copy of this
    std::stringstream ss;
    bytes_count_t nTx = bytes_count_t( copy_of_this.tx() ), nRx =
                                                                bytes_count_t( copy_of_this.rx() );
    double lfTxBPS = copy_of_this.bps_tx( tpNow ), lfRxBPS = copy_of_this.bps_rx( tpNow );
    std::string strBytesPrefix( "traffic " ), strBytesSuffix( " byte(s)" ), strTx( "Tx" ),
        strRx( "Rx" ), strTxValue( skutils::tools::to_string( nTx ) ),
        strRxValue( skutils::tools::to_string( nRx ) ), strBpsPrefix( " at " ),
        strBpsSuffix( " bps" ), strTxBPS, strRxBPS, strBpsComposed, strSlash( "/" ),
        strSpace( " " );
    bool bHaveBpsInfo = ( lfTxBPS > 0.0 || lfRxBPS > 0.0 ) ? true : false;
    if ( bHaveBpsInfo ) {
        strTxBPS = skutils::tools::format( "%.03lf", lfTxBPS );
        strRxBPS = skutils::tools::format( "%.03lf", lfRxBPS );
    } else {
        strBpsComposed = "N/A";
    }
    if ( isColored ) {
        strBytesPrefix = cc::debug( strBytesPrefix );
        strBytesSuffix = cc::debug( strBytesSuffix );
        strTx = cc::ws_tx( strTx );
        strRx = cc::ws_rx( strRx );
        strTxValue = cc::ws_tx( strTxValue );
        strRxValue = cc::ws_rx( strRxValue );
        if ( !strTxBPS.empty() )
            strTxBPS = cc::ws_tx( strTxBPS );
        if ( !strRxBPS.empty() )
            strRxBPS = cc::ws_rx( strRxBPS );
        if ( !strBpsComposed.empty() )
            strBpsComposed = cc::error( strBpsComposed );
        strSlash = cc::debug( strSlash );
        strSpace = cc::debug( strSpace );
        if ( !strBpsPrefix.empty() )
            strBpsPrefix = cc::debug( strBpsPrefix );
        if ( !strBpsSuffix.empty() )
            strBpsSuffix = cc::debug( strBpsSuffix );
    }
    if ( bHaveBpsInfo )
        strBpsComposed = strTxBPS + strSlash + strRxBPS;
    ss << strBytesPrefix << strTx << strSlash << strRx << strSpace << strTxValue << strSlash
       << strRxValue << strBytesSuffix << strBpsPrefix << strBpsComposed << strBpsSuffix;
    return ss.str();
}
std::string traffic_stats::getTrafficStatsDescription( bool isColored /*= false*/ ) const {
    return getTrafficStatsDescription( clock::now(), isColored );
}

nlohmann::json traffic_stats::toJSON( time_point tpNow, bool bSkipEmptyStats /*= true*/ ) const {
    // lock_type lock( const_cast < myrt > ( *this ) );
    nlohmann::json jo =
        skutils::stats::named_event_stats::toJSON( tpNow, bSkipEmptyStats, "events" );
    traffic_stats copy_of_this( *this );  // lock-free copy of this
    double lfTxBPS = copy_of_this.bps_tx( tpNow );
    double lfRxBPS = copy_of_this.bps_rx( tpNow );
    bytes_count_t nTx = bytes_count_t( copy_of_this.tx() );
    bytes_count_t nRx = bytes_count_t( copy_of_this.rx() );
    jo["life_time"] = skutils::tools::nanoseconds_2_lifetime_str(
        copy_of_this.instantiated_ago( tpNow ).count(), false );
    jo["state"]["name"] = copy_of_this.last_instance_state_changing_type_as_str();
    jo["state"]["time_stamp"] = skutils::tools::nanoseconds_2_lifetime_str(
        copy_of_this.changed_ago( tpNow ).count(), false );
    jo["traffic"]["tx"] = nTx;
    jo["traffic"]["rx"] = nRx;
    jo["bps"]["tx"] = lfTxBPS;
    jo["bps"]["rx"] = lfRxBPS;
    return jo;
}
nlohmann::json traffic_stats::toJSON( bool bSkipEmptyStats /*= false*/ ) const {
    return toJSON( clock::now(), bSkipEmptyStats );
}

guarded_traffic_stats::guarded_traffic_stats()
//: traffic_stats_mtx_( "RMTX-TRAFFIC-STATS" )
{}
guarded_traffic_stats::guarded_traffic_stats( traffic_stats::myrct x )
    : traffic_stats( x )
//, traffic_stats_mtx_( "RMTX-TRAFFIC-STATS" )
{}
guarded_traffic_stats::guarded_traffic_stats( traffic_stats::myrrt x )
    : traffic_stats( x )
//, traffic_stats_mtx_( "RMTX-TRAFFIC-STATS" )
{}
guarded_traffic_stats::~guarded_traffic_stats() {}
void guarded_traffic_stats::lock() {
    // traffic_stats_mtx_.lock();
    skutils::get_ref_mtx().lock();
}
void guarded_traffic_stats::unlock() {
    // traffic_stats_mtx_.unlock();
    skutils::get_ref_mtx().unlock();
}

basic_network_settings::basic_network_settings( basic_network_settings* pBNS )
    // interval_ping_( 20 )  // seconds, ping-pong interval, 0 means not use
    : timeout_pong_( /*300*/ 60 * 60 * 24 * 365 )  // seconds, default value in wspp is 5000, 0
                                                   // means not use
      ,
      timeout_handshake_open_( 60 )  // seconds, default value in wspp is 5000, 0 means not use
      ,
      timeout_handshake_close_( 60 )  // seconds, default value in wspp is 5000, 0 means not use
                                      //#if(defined __HAVE_skutils_WS_BACKEND_NLWS__)
                                      //			, max_message_size_( 0 ) // bytes, 0 is
                                      // unlimited 			, max_body_size_( 0 ) // bytes, 0 is
                                      // unlimited #else /// (defined
                                      //__HAVE_skutils_WS_BACKEND_NLWS__)
      ,
      max_message_size_( 32 * 1000 * 1000 )  // bytes, default value in wspp is 32000000
      ,
      max_body_size_( 32 * 1000 * 1000 )  // bytes, default value in wspp is 32000000
      //#endif /// else from (defined __HAVE_skutils_WS_BACKEND_NLWS__)
      ,
      timeout_restart_on_close_( 3 )  // seconds
      ,
      timeout_restart_on_fail_( 2 )  // seconds
      ,
      log_ws_rx_tx_( true )
#if ( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
      ,
      server_disable_ipv6_( false )  // use LWS_SERVER_OPTION_DISABLE_IPV6 option
      ,
      server_validate_utf8_( false )  // use LWS_SERVER_OPTION_VALIDATE_UTF8 option
      ,
      server_skip_canonical_name_( false )  // use LWS_SERVER_OPTION_SKIP_SERVER_CANONICAL_NAME
                                            // option
      ,
      ssl_perform_global_init_( false )  // both client and server, use
                                         // LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT option
                                         // (inclompatible with ssl_perform_local_init_)
      ,
      ssl_perform_local_init_( true )  // both client and server, use
                                       // explicit OpenSSL init locally
                                       // (inclompatible with ssl_perform_global_init_)
      ,
      ssl_server_require_valid_certificate_(
          false )  // use LWS_SERVER_OPTION_REQUIRE_VALID_OPENSSL_CLIENT_CERT option
      ,
      ssl_server_allow_non_ssl_on_ssl_port_(
          false )  // use LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT option
      ,
      ssl_server_accept_connections_without_valid_cert_(
          true )  // use LWS_SERVER_OPTION_PEER_CERT_NOT_REQUIRED option
      ,
      ssl_server_redirect_http_2_https_( true )  // use LWS_SERVER_OPTION_REDIRECT_HTTP_TO_HTTPS
                                                 // option
      ,
      ssl_client_allow_self_signed_( true )  // use LCCSCF_ALLOW_SELFSIGNED option
      ,
      ssl_client_skip_host_name_check_( true )  // use LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK option
      ,
      ssl_client_allow_expired_( true )  // use LCCSCF_ALLOW_EXPIRED option
      ,
      cntMaxPortionBytesToSend_( 0 )
#endif  /// if( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
{
    if ( pBNS != nullptr )
        ( *this ) = ( *pBNS );
}
basic_network_settings& basic_network_settings::default_instance() {
    static basic_network_settings g_inst;
    return g_inst;
}
void basic_network_settings::bns_assign_from_default_instance() {
    ( *this ) = default_instance();
}

generic_participant::generic_participant() {}
generic_participant::~generic_participant() {}

generic_sender::generic_sender() {}
generic_sender::~generic_sender() {}
bool generic_sender::sendMessage( const std::string& msg ) {  // send as text
    return sendMessageText( msg );
}
bool generic_sender::sendMessage( const nlohmann::json& msg ) {
    return sendMessageText( msg.dump() );
}

e_ws_logging_level_t g_eWSLL = e_ws_logging_level_t::eWSLL_none;

e_ws_logging_level_t str2wsll( const std::string& s ) {
    std::string v = skutils::tools::to_lower( s );
    if ( v.empty() )
        return e_ws_logging_level_t::eWSLL_none;
    char c = v[0];
    switch ( c ) {
    case 'b':
        return e_ws_logging_level_t::eWSLL_basic;
    case 'd':
        return e_ws_logging_level_t::eWSLL_detailed;
    default:
        return e_ws_logging_level_t::eWSLL_none;
    }  // switch( c )
}
std::string wsll2str( e_ws_logging_level_t eWSLL ) {
    switch ( eWSLL ) {
    case e_ws_logging_level_t::eWSLL_basic:
        return "basic";
    case e_ws_logging_level_t::eWSLL_detailed:
        return "detailed";
    default:
        return "none";
    };  // switch( eWSLL )
}

namespace nlws {

message_payload_data::message_payload_data()
    : pBuffer_( nullptr ), cnt_( 0 ), type_( LWS_WRITE_TEXT ), isContinuation_( false ) {}
message_payload_data::message_payload_data( const message_payload_data& other )
    : pBuffer_( nullptr ), cnt_( 0 ), type_( LWS_WRITE_TEXT ), isContinuation_( false ) {
    assign( other );
}
message_payload_data::message_payload_data( message_payload_data&& other )
    : pBuffer_( nullptr ), cnt_( 0 ), type_( LWS_WRITE_TEXT ), isContinuation_( false ) {
    move( other );
}
message_payload_data& message_payload_data::operator=( const message_payload_data& other ) {
    assign( other );
    return ( *this );
}
message_payload_data& message_payload_data::operator=( message_payload_data&& other ) {
    move( other );
    return ( *this );
}
message_payload_data::~message_payload_data() {
    clear();
}
bool message_payload_data::empty() const {
    return ( size() == 0 ) ? true : false;
}
void message_payload_data::clear() {
    free();
    type_ = LWS_WRITE_TEXT;
}
void message_payload_data::fetchHeadPartAndMarkAsContinuation( size_t cntToFetch ) {
    if ( cntToFetch == 0 )
        return;
    if ( cnt_ == 0 )
        return;  // ???
    if ( cntToFetch > cnt_ )
        cntToFetch = cnt_;
    isContinuation_ = true;
    if ( cntToFetch == cnt_ )  // we will not realloc here for speed issues, just decrement cnt_
    {
        cnt_ = 0;
        return;
    }
    uint8_t* p = data();
    if ( !p )
        return;
    cnt_ -= cntToFetch;
    if ( cnt_ > 0 )
        ::memmove( p, p + cntToFetch, cnt_ );
}
size_t message_payload_data::pre() {
    return LWS_SEND_BUFFER_PRE_PADDING;
}
size_t message_payload_data::post() {
    return LWS_SEND_BUFFER_POST_PADDING;
}
void message_payload_data::free() {
    if ( pBuffer_ ) {
        ::free( pBuffer_ );
        pBuffer_ = nullptr;
    }
    cnt_ = 0;
}
uint8_t* message_payload_data::alloc( size_t cnt ) {
    clear();
    uint8_t* p = ( uint8_t* ) ::malloc( pre() + cnt + post() );
    if ( p ) {
        pBuffer_ = p;
        cnt_ = cnt;
    }
    return data();
}
uint8_t* message_payload_data::realloc( size_t cnt ) {
    if ( !pBuffer_ )
        return alloc( cnt );
    uint8_t* p_new = ( uint8_t* ) ::realloc( pBuffer_, pre() + cnt + post() );
    if ( !p_new )
        return nullptr;
    pBuffer_ = p_new;
    cnt_ = cnt;
    return data();
}
void message_payload_data::assign( const message_payload_data& other ) {
    if ( this == ( &other ) )
        return;
    clear();
    size_t cnt = other.size();
    if ( cnt == 0 )
        return;
    uint8_t* p = alloc( cnt );
    if ( !p )
        return;
    ::memcpy( p, other.data(), cnt );
    type_ = other.type_;
    isContinuation_ = other.isContinuation_;
}
void message_payload_data::move( message_payload_data& other ) {
    if ( this == ( &other ) )
        return;
    clear();
    pBuffer_ = other.pBuffer_;
    cnt_ = other.cnt_;
    type_ = other.type_;
    isContinuation_ = other.isContinuation_;
    other.pBuffer_ = nullptr;
    other.cnt_ = 0;
    other.isContinuation_ = false;
}
lws_write_protocol message_payload_data::type() const {
    return type_;
}
size_t message_payload_data::size() const {
    return cnt_;
}
const uint8_t* message_payload_data::data() const {
    return pBuffer_ + pre();
}
uint8_t* message_payload_data::data() {
    return pBuffer_ + pre();
}
uint8_t* message_payload_data::set_text( const char* s, size_t cnt ) {
    clear();
    size_t cntEffective = ( s == nullptr || cnt == 0 ) ? 0 : cnt;
    uint8_t* p = alloc( cntEffective );
    type_ = LWS_WRITE_TEXT;
    if ( p && cntEffective > 0 )
        ::memcpy( p, s, cntEffective );
    return p;
}
uint8_t* message_payload_data::set_text( const char* s ) {
    size_t cnt = s ? ::strlen( s ) : 0;
    return set_text( s, cnt );
}
uint8_t* message_payload_data::set_text() {
    return set_text( nullptr, 0 );
}
uint8_t* message_payload_data::set_text( const std::string& s ) {
    return s.empty() ? set_text( nullptr, 0 ) : set_text( s.c_str(), s.length() );
}
uint8_t* message_payload_data::set_binary( const uint8_t* pBinary, size_t cnt ) {
    clear();
    size_t cntEffective = ( pBinary == nullptr || cnt == 0 ) ? 0 : cnt;
    uint8_t* p = alloc( cntEffective );
    type_ = LWS_WRITE_BINARY;
    if ( p && cntEffective > 0 )
        ::memcpy( p, pBinary, cntEffective );
    return p;
}
uint8_t* message_payload_data::set_binary( const std::string& s ) {
    return s.empty() ? set_binary( nullptr, 0 ) : set_binary( ( uint8_t* ) &s.front(), s.size() );
}
uint8_t* message_payload_data::set_binary() {
    return set_binary( nullptr, 0 );
}
void message_payload_data::store_to_string( std::string& s ) const {
    s.clear();
    if ( empty() )
        return;
    const uint8_t* p = data();
    size_t cntHave = size();
    if ( type_ == LWS_WRITE_TEXT ) {
        if ( p && cntHave > 0 )
            s.insert( 0, ( char* ) p, cntHave );
        return;
    }
    s = std::string( p, p + cnt_ );
}
bool message_payload_data::send_to_wsi( struct lws* wsi, size_t cntMaxPortionBytesToSend,
    size_t& cntLeft, std::string* pStrErrorDescription /*= nullptr*/ ) const {
    if ( pStrErrorDescription )
        pStrErrorDescription->clear();
    cntLeft = 0;
    if ( wsi == nullptr ) {
        if ( pStrErrorDescription )
            ( *pStrErrorDescription ) = "wsi is NULL";
        return false;
    }
    size_t cnt = size();
    if ( cnt <= 0 ) {
        if ( pStrErrorDescription )
            ( *pStrErrorDescription ) = "no data to send";
        return false;
    }
    lws_write_protocol t = LWS_WRITE_CONTINUATION, tt = type();
    uint8_t* p = ( uint8_t* ) data();
    size_t cntPortion = cnt;
    bool isCtnn = isContinuation();
    bool isOverPortion =
        ( cntMaxPortionBytesToSend > 0 && cntPortion > cntMaxPortionBytesToSend ) ? true : false;
    if ( isCtnn || isOverPortion ) {
        // for more details see
        // https://stackoverflow.com/questions/33916549/libwebsocket-send-big-messages-with-limited-payload
        if ( isOverPortion )
            cntPortion = cntMaxPortionBytesToSend;
        cntLeft = cnt - cntPortion;
        if ( !isCtnn )
            t = tt;
        if ( cntLeft > 0 )
            t = lws_write_protocol( int( t ) | int( LWS_WRITE_NO_FIN ) );
    } else
        t = tt;
    if ( skutils::ws::g_eWSLL == skutils::ws::e_ws_logging_level_t::eWSLL_detailed ) {
        std::cerr << "X>>>|cntPortion = " << cntPortion << "\n";
        std::string xxx;
        store_to_string( xxx );
        std::cerr << "B>>>|" << xxx << "|\n";
    }
    int cntSent = lws_write( wsi, p, cntPortion, t );
    if ( skutils::ws::g_eWSLL == skutils::ws::e_ws_logging_level_t::eWSLL_detailed ) {
        std::cerr << "X>>>|cntSent = " << cntSent << "\n";
    }
    if ( cntSent < 0 ) {
        if ( pStrErrorDescription )
            ( *pStrErrorDescription ) =
                skutils::tools::format( "data(%d byte%s) was not sent(result is %d=0x%X)",
                    cntPortion, ( cntPortion > 1 ) ? "s" : "", cntSent, cntSent );
        return false;
    }
    ( const_cast< message_payload_data* >( this ) )->fetchHeadPartAndMarkAsContinuation( cntSent );
    if ( skutils::ws::g_eWSLL == skutils::ws::e_ws_logging_level_t::eWSLL_detailed ) {
        std::string xxx;
        store_to_string( xxx );
        std::cerr << "A>>>|" << xxx << "|\n";
    }
    // if( cntSent >= cnt )
    //	return true;
    cntLeft = size();
    return true;
}
bool message_payload_data::append( const message_payload_data& other ) {
    size_t cntBefore = size();
    lws_write_protocol t = type(), t_other = other.type();
    if ( t != t_other && cntBefore > 0 )
        return false;
    type_ = t_other;  // auto change type when assigning to this empty data object
    size_t cntOther = other.size();
    if ( cntOther == 0 )
        return true;
    if ( cntBefore == 0 ) {
        assign( other );
        return true;
    }
    size_t cntAfter = cntBefore + cntOther;
    uint8_t* p = realloc( cntAfter );
    if ( !p )
        return false;
    ::memcpy( p + cntBefore, other.data(), cntOther );
    return true;
}


std::string basic_api::g_strDefaultProtocolName( "" );  // "default-protocol"
// some experimental notes about max working connection counts with different buffer sizes on
// machine with 4GB RAM: 1024*128 - about 400   connections 1024*64  - about 800   connections
// 1024*32  - about 1600  connections
// 1024*16  - about 3200  connections
// 1024*8   - about 6400  connections
// 1024*4   - about 12800 connections
// 1024*2   - about 25600 connections
// 1024     - about 51200 connections
volatile size_t basic_api::g_nDefaultBufferSizeRX = 1024 * 16;  // 0 - unlimited
volatile size_t basic_api::g_nDefaultBufferSizeTX =
    1024 * 16;  // 0 - same as g_nDefaultBufferSizeRX


basic_api::basic_api( basic_network_settings* pBNS )
    : basic_network_settings( pBNS )
//, mtx_api_( "RMTX-NLWS-BASIC-API" )
{
    clear_fields();
}
basic_api::~basic_api() {
    clear_fields();
}

basic_api::mutex_type& basic_api::mtx_api() const {
    // return mtx_api_;
    return skutils::get_ref_mtx();
}


void basic_api::locked_execute( fn_lock_callback_t fn ) {
    if ( !fn )
        return;
    lock_type lock( mtx_api() );
    fn();
}
//			bool basic_api::try_locked_execute( fn_lock_callback_t fn, size_t cntAttempts, uint64_t
// nMillisecondsWaitBetweenAttempts ) { 				if( cntAttempts < 1 ) return false; if( ! fn
// ) return false;
//				//++ cntTryLockExecutes_;
//				bool bWasLocked = false;
//				for( size_t i = 0; i < cntAttempts; ++ i ) {
//					if( ! initialized_ )
//						break;
//					bWasLocked = mtx_api().try_lock();
//					if( bWasLocked )
//						break;
//					if( ! initialized_ )
//						break;
//					if( nMillisecondsWaitBetweenAttempts > 0 && i < (cntAttempts-1) )
//						std::this_thread::sleep_for(
// std::chrono::milliseconds(nMillisecondsWaitBetweenAttempts) ); 				} // for( size_t i =
// 0; i < cntAttempts; ++ i ) 				if( bWasLocked ) { 					try { fn();
// } catch( ... ) {
//						//-- cntTryLockExecutes_;
//						mtx_api().unlock();
//						throw;
//					}
//					//-- cntTryLockExecutes_;
//					mtx_api().unlock();
//				} // if( bWasLocked )
//				else {
//					//-- cntTryLockExecutes_;
//				}
//				return bWasLocked;
//			}

void basic_api::clear_fields() {
    interface_name_.clear();
    bns_assign_from_default_instance();
    max_message_size_ = 0;  // unlimited
    max_body_size_ = 0;     // unlimited
    //
    switch ( g_eWSLL ) {
    case e_ws_logging_level_t::eWSLL_basic:
        ::lws_set_log_level(
            LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO, /*lwsl_emit_syslog*/ nullptr );
        break;
    case e_ws_logging_level_t::eWSLL_detailed:
        ::lws_set_log_level( 0xFF, /*lwsl_emit_syslog*/ nullptr );
        break;
    default:
        ::lws_set_log_level( 0, /*lwsl_emit_syslog*/ nullptr );
        break;
    }  // switch( g_eWSLL )
    //
    static const struct lws_extension g_exts[] = {
        {"permessage-deflate", lws_extension_callback_pm_deflate, "permessage-deflate"},
        {"deflate-frame", lws_extension_callback_pm_deflate, "deflate_frame"},
        {nullptr, nullptr, nullptr}  // terminator
    };
    //
    initialized_ = false;
    // while( cntTryLockExecutes_ > 0 )
    //	std::this_thread::sleep_for( std::chrono::milliseconds(10) );
    ctx_ = nullptr;
    ::memset( &ctx_info_, 0, sizeof( struct lws_context_creation_info ) );
    ctx_info_.port = CONTEXT_PORT_NO_LISTEN;
    ctx_info_.iface = nullptr;
    // ctx_info_.protocols = vec_lws_protocols_.data(); // this MUST be done later

    ctx_info_.ssl_cert_filepath = nullptr;
    ctx_info_.ssl_private_key_filepath = nullptr;
    ctx_info_.ssl_ca_filepath = nullptr;
    //				ctx_info_.extensions = ::lws_get_internal_extensions();
    ctx_info_.extensions = g_exts;
    ctx_info_.gid = -1;
    ctx_info_.uid = -1;
    ctx_info_.options = 0;
    // keep alive
    ctx_info_.ka_time = 60;      // 60 seconds until connection is suspicious
    ctx_info_.ka_probes = 10;    // 10 probes after ^ time
    ctx_info_.ka_interval = 10;  // 10s interval for sending probes
    //
    ctx_info_.timeout_secs = 5;
    //
    unsigned int walk_protocol_id = 0;
    vec_lws_protocols_.clear();
    //
    //				vec_lws_protocols_.push_back( { // the first protocol must always be the HTTP
    // handler 						"http-only",            // name
    // stat_callback_http,
    // // callback 0, // per_session_data_size, no per session data. g_nDefaultBufferSizeRX, //
    // rx_buffer_size, max frame size / rx buffer walk_protocol_id,       // id, ignored by lws
    // nullptr, // user
    //						g_nDefaultBufferSizeTX  // tx_packet_size
    //					} );
    //				++ walk_protocol_id;
    //
    default_protocol_index_ = vec_lws_protocols_.size();
    vec_lws_protocols_.push_back( {
        g_strDefaultProtocolName.c_str(), stat_callback_server,
        0,                       // per_session_data_size
        g_nDefaultBufferSizeRX,  // rx_buffer_size, lws allocates this much space for rx data and
                                 // informs callback when something came
        walk_protocol_id,  // id, ignored by lws, but useful to contain user information bound to
                           // the selected protocol
        nullptr,  // user, by lws, but user code can pass a pointer here it can later access from
                  // the protocol callback
        g_nDefaultBufferSizeTX  // tx_packet_size, 0 indicates restrict send() size to
                                // .rx_buffer_size for backwards-compatibility
    } );
    ++walk_protocol_id;
    //

    vec_lws_protocols_.push_back( {nullptr, nullptr, 0, 0, 0, nullptr, 0} );  // terminator
    ctx_info_.protocols = vec_lws_protocols_.data();  // this MUST be done here
                                                      //
}

void basic_api::do_writable_callbacks_all_protocol() {
    //::lws_callback_on_writable_all_protocol( ctx_, vec_lws_protocols_.data() );
    ::lws_callback_on_writable_all_protocol( ctx_, &vec_lws_protocols_[default_protocol_index_] );
}

static enum pending_timeout g_arrPingPongTimeoutTypes[]{
    PENDING_TIMEOUT_AWAITING_PING,
    PENDING_TIMEOUT_WS_PONG_CHECK_SEND_PING,
    PENDING_TIMEOUT_WS_PONG_CHECK_GET_PONG,
};
static enum pending_timeout g_arrOpenTimeoutTypes[]{
    PENDING_TIMEOUT_ESTABLISH_WITH_SERVER,
    PENDING_TIMEOUT_SENT_CLIENT_HANDSHAKE,
    PENDING_TIMEOUT_SSL_ACCEPT,
    PENDING_TIMEOUT_AWAITING_CLIENT_HS_SEND,
    PENDING_TIMEOUT_AWAITING_PROXY_RESPONSE,
    PENDING_TIMEOUT_AWAITING_CONNECT_RESPONSE,
    PENDING_TIMEOUT_AWAITING_SOCKS_GREETING_REPLY,
    PENDING_TIMEOUT_AWAITING_SOCKS_CONNECT_REPLY,
    PENDING_TIMEOUT_AWAITING_SOCKS_AUTH_REPLY,
};
static enum pending_timeout g_arrCloseTimeoutTypes[]{
    PENDING_TIMEOUT_SHUTDOWN_FLUSH,
    PENDING_FLUSH_STORED_SEND_BEFORE_CLOSE,
    PENDING_TIMEOUT_KILLED_BY_SSL_INFO,
    PENDING_TIMEOUT_KILLED_BY_PARENT,
    PENDING_TIMEOUT_CLOSE_SEND,
    PENDING_TIMEOUT_CLOSE_ACK,
};
static enum pending_timeout g_arrNetworkTimeoutTypes[]{
    PENDING_TIMEOUT_HTTP_CONTENT,
    PENDING_TIMEOUT_AWAITING_SERVER_RESPONSE,
    //					PENDING_TIMEOUT_AWAITING_EXTENSION_CONNECT_RESPONSE,
    PENDING_TIMEOUT_HTTP_CONTENT,
    // PENDING_FLUSH_STORED_SEND_BEFORE_CLOSE,
    PENDING_TIMEOUT_HTTP_KEEPALIVE_IDLE,
    PENDING_TIMEOUT_CLIENT_ISSUE_PAYLOAD,
};
// PENDING_TIMEOUT_HOLDING_AH
void basic_api::configure_wsi( struct lws* wsi ) {
    if ( wsi == nullptr )
        return;
    size_t i, cnt;
    if ( timeout_handshake_open_ > 0 ) {
        for ( i = 0, cnt = sizeof( g_arrOpenTimeoutTypes ) / sizeof( g_arrOpenTimeoutTypes[0] );
              i < cnt; ++i )
            ::lws_set_timeout( wsi, g_arrOpenTimeoutTypes[i], timeout_handshake_open_ );
        for ( i = 0,
              cnt = sizeof( g_arrNetworkTimeoutTypes ) / sizeof( g_arrNetworkTimeoutTypes[0] );
              i < cnt; ++i )
            ::lws_set_timeout( wsi, g_arrNetworkTimeoutTypes[i], timeout_handshake_open_ );
    }
    if ( timeout_handshake_close_ > 0 ) {
        for ( i = 0, cnt = sizeof( g_arrCloseTimeoutTypes ) / sizeof( g_arrCloseTimeoutTypes[0] );
              i < cnt; ++i )
            ::lws_set_timeout( wsi, g_arrCloseTimeoutTypes[i], timeout_handshake_close_ );
    }
    if ( timeout_pong_ > 0 ) {
        for ( i = 0,
              cnt = sizeof( g_arrPingPongTimeoutTypes ) / sizeof( g_arrPingPongTimeoutTypes[0] );
              i < cnt; ++i ) {
            uint64_t tto = timeout_pong_;
            if ( g_arrPingPongTimeoutTypes[i] == PENDING_TIMEOUT_AWAITING_PING )
                tto *= 10;  // l_sergiy: for safety
            ::lws_set_timeout( wsi, g_arrPingPongTimeoutTypes[i], tto );
        }
    }
}

bool impl_frame_is_binary( struct lws* wsi, const uint8_t* /*in*/, size_t /*len*/ ) {
    if (::lws_frame_is_binary( wsi ) )
        return true;
    return false;
}

int basic_api::stat_callback_http( struct lws* wsi, enum lws_callback_reasons reason,
    void* /*user*/, void* /*in*/, size_t /*len*/ ) {
    bool isClose = false;
    switch ( reason ) {
    case LWS_CALLBACK_ESTABLISHED:
        // ctx = ::lws_get_context( wsi );
        // self = server_api::stat_get( ctx );
        //::lws_callback_on_writable( wsi );
        isClose = true;
        break;
    case LWS_CALLBACK_HTTP:
        //::lws_serve_http_file( wsi, "example.html", "text/html", nullptr, 0 );
        isClose = true;
        break;
    default:
        break;
    }  // switch( reason )
    if ( isClose ) {
        std::string strCloseReason( "HTTP is disabled" );
        ::lws_close_reason( wsi, lws_close_status::LWS_CLOSE_STATUS_POLICY_VIOLATION,
            ( unsigned char* ) strCloseReason.c_str(), strCloseReason.length() );
        return -1;
    }
    return 0;
}

/// extern "C" void impl_lws_close_free_wsi( struct lws *wsi, enum lws_close_status reason, int
/// bIsFree );

int basic_api::stat_callback_client(
    struct lws* wsi, enum lws_callback_reasons reason, void* /*user*/, void* in, size_t len ) {
    client_api* self = nullptr;
    switch ( reason ) {
        // case LWS_CALLBACK_CONFIRM_EXTENSION_OKAY:
        //	return 1; // disallow compression
        //				case LWS_CALLBACK_PROTOCOL_INIT: // per vhost
        //					lws_protocol_vh_priv_zalloc( lws_get_vhost(wsi), lws_get_protocol(wsi),
        // sizeof(struct per_vhost_data__lws_mirror) ); 				break;
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        self = client_api::stat_get( wsi );
        if ( self ) {
            // self->onLogMessage( e_ws_log_message_type_t::eWSLMT_debug, "NLWS:
            // LWS_CALLBACK_CLIENT_ESTABLISHED: connect with server success" );
            int fd = ::lws_get_socket_fd( wsi );
            self->cid_ = fd;
            self->connection_flag_ = true;
            self->onConnect();
            ::lws_callback_on_writable( wsi );
        }
        break;
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        self = client_api::stat_get( wsi );
        if ( self ) {
            // self->onLogMessage( e_ws_log_message_type_t::eWSLMT_error, "NLWS:
            // LWS_CALLBACK_CLIENT_CONNECTION_ERROR: connect with server error" );
            self->destroy_flag_ = true;
            self->connection_flag_ = false;
            self->clientThreadStopFlag_ = true;
            self->onFail( "NLWS: client connection error (via callback)" );
        }
        break;
    case LWS_CALLBACK_CLOSED:
        self = client_api::stat_get( wsi );
        if ( self ) {
            // self->onLogMessage( e_ws_log_message_type_t::eWSLMT_debug, "NLWS:
            // LWS_CALLBACK_CLOSED" );
            self->destroy_flag_ = true;
            self->connection_flag_ = true;
            self->clientThreadStopFlag_ = true;
            self->onDisconnect( "NLWS: client connection closed" );
        }
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        self = client_api::stat_get( wsi );
        if ( self ) {
            //						if( self->writeable_flag_ ) {
            //							//self->onLogMessage( e_ws_log_message_type_t::eWSLMT_debug,
            //"NLWS: LWS_CALLBACK_CLIENT_RECEIVE: client received data" );
            // self->destroy_flag_ = true; 							self->onDisconnect( "read
            // attempt when writable"
            // ); 							return
            // 0;
            //						}
            message_payload_data data;
            if ( impl_frame_is_binary( wsi, ( uint8_t* ) in, len ) )
                data.set_binary( ( uint8_t* ) in, len );
            else
                data.set_text( ( char* ) in, len );
            ////size_t nRemain = ::lws_remaining_packet_payload( wsi ); bool isFinalFragment = (
            /// nRemain == 0 ) ? true : false;
            bool isFinalFragment = ::lws_is_final_fragment( wsi );
            self->onMessage( data, isFinalFragment );
        }
        break;
    case LWS_CALLBACK_CLIENT_WRITEABLE:
        self = client_api::stat_get( wsi );
        if ( self ) {
            volatile bool bCloseAction = false;
            // self->onLogMessage( e_ws_log_message_type_t::eWSLMT_debug, "NLWS:
            // LWS_CALLBACK_CLIENT_WRITEABLE" );
            {  // block
                client_api::lock_type lock( self->mtx_api() );
                std::string cached_delayed_close_reason = self->delayed_close_reason_;
                lws_close_status cached_delayed_close_status =
                    ( lws_close_status ) self->delayed_close_status_;
                self->delayed_close_reason_.clear();
                self->delayed_close_status_ = 0;
                //
                if ( !cached_delayed_close_reason.empty() ) {
                    self->onLogMessage( e_ws_log_message_type_t::eWSLMT_debug,
                        cc::debug( "Processing close action for client connection" ) );
                    ::lws_close_reason( wsi, cached_delayed_close_status,
                        ( unsigned char* ) cached_delayed_close_reason.c_str(),
                        cached_delayed_close_reason.length() );
                    /// impl_lws_close_free_wsi( wsi, cached_delayed_close_status, 0 );
                    bCloseAction = true;
                    // self->onDisconnect( msg );
                    self->delay_deinit();
                }  // if( ! cached_delayed_close_reason.empty() )
                else {
                    payload_queue_t& pq = self->buffer_;
                    while ( !pq.empty() ) {
                        const message_payload_data& data = pq.front();
                        size_t cntLeft = 0;
                        size_t cntMaxPortion =
#if ( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
                            self->cntMaxPortionBytesToSend_;
#else
                            4096;
#endif
                        if ( g_nDefaultBufferSizeTX > 0 && cntMaxPortion < g_nDefaultBufferSizeTX )
                            cntMaxPortion = g_nDefaultBufferSizeTX;
                        std::string strErrorDescription;
                        if ( !data.send_to_wsi(
                                 wsi, cntMaxPortion, cntLeft, &strErrorDescription ) ) {
                            std::string strFailMessage;
                            strFailMessage +=
                                "NLWS: error writing to socket(client writable callback)";
                            if ( !strErrorDescription.empty() ) {
                                strFailMessage += ", error description: ";
                                strFailMessage += strErrorDescription;
                            }
                            self->onFail( strFailMessage );
                            break;
                        }
                        if ( cntLeft > 0 )
                            break;
                        pq.pop_front();  // only pop the message if it was sent successfully
                    }                    // while( ! pq.empty() )
                    if ( self->delayed_adjustment_pong_timeout_ >=
                         0 ) {  // -1 for no adjustment, otherwise change pong timeout
                        int64_t timeout_pong = self->delayed_adjustment_pong_timeout_;
                        self->delayed_adjustment_pong_timeout_ = -1;
                        size_t i, cnt;
                        for ( i = 0, cnt = sizeof( g_arrPingPongTimeoutTypes ) /
                                           sizeof( g_arrPingPongTimeoutTypes[0] );
                              i < cnt; ++i ) {
                            uint64_t tto = uint64_t( timeout_pong );
                            if ( g_arrPingPongTimeoutTypes[i] == PENDING_TIMEOUT_AWAITING_PING )
                                tto *= 10;  // l_sergiy: for safety
                            ::lws_set_timeout( wsi, g_arrPingPongTimeoutTypes[i], tto );
                        }
                    }
                }  // else from if( ! cached_delayed_close_reason.empty() )
            }      // block
            if ( bCloseAction )
                return -1;  // this closes connection according to
                            // https://libwebsockets.org/lws-api-doc-master/html/md_README_8coding.html
        }  // if( self )
        break;
        /*
                        case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS:
                            lwsl_err( "LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS\n" );
                        break;
        */
    default:
        break;
    }  // switch( reason )
    return 0;
}

int basic_api::stat_callback_server(
    struct lws* wsi, enum lws_callback_reasons reason, void* /*user*/, void* in, size_t len ) {
    lws_context* ctx = nullptr;
    server_api* self = nullptr;

    //				const size_t nPre = LWS_SEND_BUFFER_PRE_PADDING, nPost =
    // LWS_SEND_BUFFER_POST_PADDING; 				unsigned char buf[ nPre + 512 + nPost ];
    // unsigned char
    // * p = &buf[nPre];

    switch ( reason ) {
        // case LWS_CALLBACK_CONFIRM_EXTENSION_OKAY:
        //	return 1; // disallow compression
        //				case LWS_CALLBACK_PROTOCOL_INIT: // per vhost
        //					lws_protocol_vh_priv_zalloc( lws_get_vhost(wsi), lws_get_protocol(wsi),
        // sizeof(struct per_vhost_data__lws_mirror) ); 				break;
    case LWS_CALLBACK_ESTABLISHED:
        ctx = ::lws_get_context( wsi );
        self = server_api::stat_get( ctx );
        if ( self ) {
            self->configure_wsi( wsi );
            int fd = ::lws_get_socket_fd( wsi );
            char strPeerClientAddressName[256], strPeerRemoteIP[256];
            ::memset( strPeerClientAddressName, 0, sizeof( strPeerClientAddressName ) );
            ::memset( strPeerRemoteIP, 0, sizeof( strPeerRemoteIP ) );
            ::lws_get_peer_addresses( wsi, fd, strPeerClientAddressName,
                sizeof( strPeerClientAddressName ) - 1, strPeerRemoteIP,
                sizeof( strPeerRemoteIP ) - 1 );
            self->onConnect( fd, wsi, strPeerClientAddressName, strPeerRemoteIP );
            ::lws_callback_on_writable( wsi );
        }
        break;
    case LWS_CALLBACK_CLOSED:
        ctx = ::lws_get_context( wsi );
        self = server_api::stat_get( ctx );
        if ( self ) {
            int fd = ::lws_get_socket_fd( wsi );
            self->onDisconnect( fd, "NLWS: peer connection closed" );
        }
        break;

    case LWS_CALLBACK_SERVER_WRITEABLE:
        ctx = ::lws_get_context( wsi );
        self = server_api::stat_get( ctx );
        if ( self ) {
            // bool bCallWriteable = false;
            int fd = ::lws_get_socket_fd( wsi );
            volatile bool bCloseAction = false;
            {  // block
                server_api::lock_type lock( self->mtx_api() );
                server_api::map_connections_t::iterator itCnFind = self->connections_.find( fd ),
                                                        itCnEnd = self->connections_.end();
                if ( itCnFind == itCnEnd )
                    return -1;  // this closes connection according to
                                // https://libwebsockets.org/lws-api-doc-master/html/md_README_8coding.html
                server_api::connection_data* pcd = itCnFind->second;
                if ( !pcd )
                    return -1;  // this closes connection according to
                                // https://libwebsockets.org/lws-api-doc-master/html/md_README_8coding.html
                std::string cached_delayed_close_reason = pcd->delayed_close_reason_;
                lws_close_status cached_delayed_close_status =
                    ( lws_close_status ) pcd->delayed_close_status_;
                pcd->delayed_close_reason_.clear();
                pcd->delayed_close_status_ = 0;
                //
                // self->onLogMessage( e_ws_log_message_type_t::eWSLMT_debug, cc::debug("Processing
                // writable state for ") + strDescC );
                if ( !cached_delayed_close_reason.empty() ) {
                    std::string strDescC = pcd->description( true );
                    self->onLogMessage( e_ws_log_message_type_t::eWSLMT_debug,
                        cc::debug( "Processing close action for " ) + strDescC );
                    ::lws_close_reason( wsi, cached_delayed_close_status,
                        ( unsigned char* ) cached_delayed_close_reason.c_str(),
                        cached_delayed_close_reason.length() );
                    /// impl_lws_close_free_wsi( wsi, cached_delayed_close_status, 0 );
                    bCloseAction = true;
                    // bCallWriteable = true;
                    // self->onDisconnect( cid, msg );
                }  // if( ! cached_delayed_close_reason.empty() )
                else {
                    payload_queue_t& pq = pcd->buffer_;
                    while ( !pq.empty() ) {
                        const message_payload_data& data = pq.front();
                        size_t cntLeft = 0;
                        size_t cntMaxPortion =
#if ( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
                            self->cntMaxPortionBytesToSend_;
#else
                            4096;
#endif
                        if ( g_nDefaultBufferSizeTX > 0 && cntMaxPortion < g_nDefaultBufferSizeTX )
                            cntMaxPortion = g_nDefaultBufferSizeTX;
                        std::string strErrorDescription;
                        if ( !data.send_to_wsi(
                                 wsi, cntMaxPortion, cntLeft, &strErrorDescription ) ) {
                            std::string strFailMessage;
                            strFailMessage +=
                                "NLWS: error writing to socket(server writable callback), pcd is ";
                            std::string strDesc = pcd->description( false );
                            strFailMessage += strDesc;
                            if ( !strErrorDescription.empty() ) {
                                strFailMessage += ", error description: ";
                                strFailMessage += strErrorDescription;
                            }
                            self->onFail( fd, strFailMessage );
                            break;
                        }
                        if ( cntLeft > 0 )
                            break;
                        pq.pop_front();  // only pop the message if it was sent successfully
                        // bCallWriteable = true;
                    }  // while( ! pq.empty() )
                    if ( pcd->delayed_adjustment_pong_timeout_ >=
                         0 ) {  // -1 for no adjustment, otherwise change pong timeout
                        int64_t timeout_pong = pcd->delayed_adjustment_pong_timeout_;
                        pcd->delayed_adjustment_pong_timeout_ = -1;
                        size_t i, cnt;
                        for ( i = 0, cnt = sizeof( g_arrPingPongTimeoutTypes ) /
                                           sizeof( g_arrPingPongTimeoutTypes[0] );
                              i < cnt; ++i ) {
                            uint64_t tto = uint64_t( timeout_pong );
                            if ( g_arrPingPongTimeoutTypes[i] == PENDING_TIMEOUT_AWAITING_PING )
                                tto *= 10;  // l_sergiy: for safety
                            ::lws_set_timeout( wsi, g_arrPingPongTimeoutTypes[i], tto );
                        }
                        // bCallWriteable = true;
                    }
                }  // else from if( ! cached_delayed_close_reason.empty() )
            }      // block
                   // if( bCallWriteable )
            ::lws_callback_on_writable( wsi );
            if ( bCloseAction )
                return -1;  // this closes connection according to
                            // https://libwebsockets.org/lws-api-doc-master/html/md_README_8coding.html
        }  // if( self )
        break;

    case LWS_CALLBACK_RECEIVE:
        ctx = ::lws_get_context( wsi );
        self = server_api::stat_get( ctx );
        if ( self ) {
            int fd = ::lws_get_socket_fd( wsi );
            message_payload_data data;
            if ( impl_frame_is_binary( wsi, ( uint8_t* ) in, len ) )
                data.set_binary( ( uint8_t* ) in, len );
            else
                data.set_text( ( char* ) in, len );
            ////size_t nRemain = ::lws_remaining_packet_payload( wsi ); bool isFinalFragment = (
            /// nRemain == 0 ) ? true : false;
            bool isFinalFragment = ::lws_is_final_fragment( wsi );
            self->onMessage( fd, data, isFinalFragment );
        }
        break;
    case LWS_CALLBACK_HTTP: {
        //::lwsl_debug( "LWS_CALLBACK_HTTP\n" );
        ctx = ::lws_get_context( wsi );
        self = server_api::stat_get( ctx );
        unsigned int nHttpStatusToReturn = HTTP_STATUS_NOT_FOUND;
        std::string strHttpBodyToReturn =
            "<html><body>HTTP is not enabled on this server.</body></html>";
        std::string s( ( char* ) in );
        std::cout << "LWS_CALLBACK_HTTP - " << s << "\n";
        if ( self ) {
            int fd = ::lws_get_socket_fd( wsi );
            nHttpStatusToReturn = self->nHttpStatusToReturn_;
            strHttpBodyToReturn = self->strHttpBodyToReturn_;
            self->onHttp( fd  //, in, nHttpStatusToReturn, strHttpBodyToReturn
            );
        }
        ::lws_return_http_status( wsi, nHttpStatusToReturn, strHttpBodyToReturn.c_str() );
    } break;
    case LWS_CALLBACK_HTTP_BODY: {
    } break;
    case LWS_CALLBACK_HTTP_BODY_COMPLETION: {
    } break;
    case LWS_CALLBACK_HTTP_DROP_PROTOCOL: {
    } break;
    default:
        break;
    }  // switch( reason )
    return 0;
}

client_api::client_api( basic_network_settings* pBNS ) : basic_api( pBNS ) {
    clear_fields();
    if ( ssl_perform_local_init_ ) {
        SSL_load_error_strings();
        SSL_library_init();
    }
}
client_api::~client_api() {
    deinit();
    if ( ssl_perform_local_init_ ) {
        ERR_free_strings();
    }
}

void client_api::clear_fields() {
    lock_type lock( mtx_api() );
    delayed_de_init_ = false;
    basic_api::clear_fields();
    cid_ = 0;
    vec_lws_protocols_[default_protocol_index_].callback = stat_callback_client;
    destroy_flag_ = false;
    connection_flag_ = false;
    // writeable_flag_ = false;
    //
    strURL_.clear();
    wsi_ = nullptr;
    ssl_flags_ = 0;
    cert_path_.clear();
    key_path_.clear();
    ca_path_.clear();
    buffer_.clear();
    //
    accumulator_.clear();
}
bool client_api::init(
    const std::string& strURL, security_args* pSA, const char* strInterfaceName ) {
    lock_type lock( mtx_api() );
    skutils::url an_url( strURL );
    const auto& scheme = an_url.scheme();
    bool isSSL = ( scheme == "ws" ) ? false : true;
    std::string strHost = an_url.host();
    std::string strPath = an_url.path();
    int nPort = ::atoi( an_url.port().c_str() );
    return init( isSSL, strHost, nPort, strPath, pSA, strInterfaceName );
}

extern "C" void lws_ssl_elaborate_error();
extern "C" void lws_ssl_bind_passphrase( SSL_CTX* ssl_ctx, struct lws_context_creation_info* info );

bool client_api::init( bool isSSL, const std::string& strHost, int nPort,
    const std::string& strPath, security_args* pSA, const char* strInterfaceName ) {
    lock_type lock( mtx_api() );
    if ( initialized_ )
        return false;
    // clear_fields();
    deinit();
    interface_name_ = ( strInterfaceName && strInterfaceName[0] ) ? strInterfaceName : "";
    bool bDoGlobalInitSSL = false, bDoLocalInitSSL = false;
    if ( isSSL && pSA != nullptr ) {
        // if ( ssl_perform_global_init_ ) // LS: always do global SSL init in WSS client
        bDoGlobalInitSSL = true;
        // if ( !bDoGlobalInitSSL )
        //    bDoLocalInitSSL = true;

        cert_path_ = pSA->strCertificateFile_;
        key_path_ = pSA->strPrivateKeyFile_.c_str();
        ca_path_ = pSA->strCertificationChainFile_.c_str();  // ???
        ssl_flags_ |= LCCSCF_USE_SSL;
#if ( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
        if ( ssl_client_allow_self_signed_ )
#endif  /// if( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
            ssl_flags_ |= LCCSCF_ALLOW_SELFSIGNED;
#if ( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
        if ( ssl_client_skip_host_name_check_ )
#endif  /// if( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
            ssl_flags_ |= LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
#if ( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
        if ( ssl_client_allow_expired_ )
#endif  /// if( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
            ssl_flags_ |= LCCSCF_ALLOW_EXPIRED;
        //
        //
        ctx_info_.client_ssl_cert_filepath = ( !cert_path_.empty() ) ? cert_path_.c_str() : nullptr;
        ctx_info_.client_ssl_private_key_filepath =
            ( !key_path_.empty() ) ? key_path_.c_str() : nullptr;
        ctx_info_.client_ssl_ca_filepath = ( !ca_path_.empty() ) ? ca_path_.c_str() : nullptr;
        //
        //
    }  // i( isSSL && pSA != nullptr )

#if ( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
    if ( bDoGlobalInitSSL )  // ssl_perform_global_init_
#endif  /// if( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
        ctx_info_.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    //				ctx_info_.ssl_options_set = // compatibility with old implementation based on
    // wspp 						SSL_OP_NO_SSLv2 //| SSL_OP_NO_SSLv3 						|
    // SSL_OP_SINGLE_DH_USE
    //						//| SSL_OP_ALL
    //						;

    vec_lws_protocols_[default_protocol_index_].per_session_data_size =
        0;  // sizeof(struct client_session_data);
    vec_lws_protocols_[default_protocol_index_].rx_buffer_size =
        1024 * 100;  // rx_buffer_size, lws allocates this much space for rx data and informs
                     // callback when something came
    vec_lws_protocols_[default_protocol_index_].user =
        this;  // user, by lws, but user code can pass a pointer here it can later access from the
               // protocol callback
    vec_lws_protocols_[default_protocol_index_].tx_packet_size =
        0;  // tx_packet_size, 0 indicates restrict send() size to .rx_buffer_size for
            // backwards-compatibility

    ctx_info_.client_ssl_cipher_list = nullptr
        //					"ECDHE-ECDSA-AES256-GCM-SHA384:"
        //					"ECDHE-RSA-AES256-GCM-SHA384:"
        //					"DHE-RSA-AES256-GCM-SHA384:"
        //					"ECDHE-RSA-AES256-SHA384:"
        //					"HIGH:!aNULL:!eNULL:!EXPORT:"
        //					"!DES:!MD5:!PSK:!RC4:!HMAC_SHA1:"
        //					"!SHA1:!DHE-RSA-AES128-GCM-SHA256:"
        //					"!DHE-RSA-AES128-SHA256:"
        //					"!AES128-GCM-SHA256:"
        //					"!AES128-SHA256:"
        //					"!DHE-RSA-AES256-SHA256:"
        //					"!AES256-GCM-SHA384:"
        //					"!AES256-SHA256"
        ;
    // ctx_info_.simultaneous_ssl_restriction = 4;
    // ctx_info_.client_ssl_ca_filepath = "/opt/nma/certs/server.ca";

    /*
                    //ctx_info_.provided_client_ssl_ctx = ::SSL_CTX_new(::TLSv1_client_method());
                    //ctx_info_.provided_client_ssl_ctx = ::SSL_CTX_new(::TLSv1_1_client_method());
                    //ctx_info_.provided_client_ssl_ctx = ::SSL_CTX_new(::TLSv1_2_client_method());
                    //ctx_info_.provided_client_ssl_ctx = ::SSL_CTX_new(::SSLv3_client_method());
                    //ctx_info_.provided_client_ssl_ctx = ::SSL_CTX_new(::TLSv1_method());
                    //ctx_info_.provided_client_ssl_ctx = ::SSL_CTX_new(::SSLv23_method());
                    //ctx_info_.provided_client_ssl_ctx = ::SSL_CTX_new(::SSLv23_client_method());
                    //ctx_info_.provided_client_ssl_ctx = ::SSL_CTX_new(0);
                    //
                    ctx_info_.provided_client_ssl_ctx = ::SSL_CTX_new(::TLSv1_method()); ///
       asio::ssl::context::tlsv1 lwsl_info( "NLWS: =========== 1) setting client compat opts %p
       ===========================\n", ctx_info_.provided_client_ssl_ctx ); if(
       ctx_info_.provided_client_ssl_ctx ) {
                        //
                        //SSL_CTX_set_verify_depth( ctx_info_.provided_client_ssl_ctx, 2 );
                        //SSL_CTX_set_verify ... ... ...
                        //
                        SSL_CTX_set_options( // compatibility with old implementation based on wspp
                            ctx_info_.provided_client_ssl_ctx,
                            SSL_OP_NO_SSLv2        /// asio::ssl::context::no_sslv2
                            //| SSL_OP_NO_SSLv3
                            | SSL_OP_SINGLE_DH_USE /// asio::ssl::context::single_dh_use
                            | SSL_OP_ALL           /// asio::ssl::context::default_workarounds
                            | SSL_OP_NO_COMPRESSION // other
                            //| SSL_OP_CIPHER_SERVER_PREFERENCE // other
                            //| SSL_OP_NO_TICKET // other
                            //|SSL_OP_NO_TLSv1_2|SSL_OP_NO_TLSv1_1|SSL_OP_NO_DTLSv1|SSL_OP_NO_DTLSv1_2
       // other
                            );
                        //////////X509_STORE_set_flags(
       SSL_CTX_get_cert_store(ctx_info_.provided_client_ssl_ctx), X509_V_FLAG_TRUSTED_FIRST );
                        //SSL_CTX_set_default_verify_paths( ctx_info_.provided_client_ssl_ctx );
                        //
                        // simplar to lws_context_init_client_ssl()
                        const char *ca_filepath          = ctx_info_.client_ssl_ca_filepath;
                        const char *cipher_list          = ctx_info_.client_ssl_cipher_list;
                        const char *private_key_filepath =
       ctx_info_.client_ssl_private_key_filepath; const char *cert_filepath        =
       ctx_info_.client_ssl_cert_filepath; SSL_CTX *ssl_client_ctx =
       ctx_info_.provided_client_ssl_ctx; // vhost->ssl_client_ctx if( ! ca_filepath ) { if( !
       SSL_CTX_load_verify_locations( ssl_client_ctx, nullptr, LWS_OPENSSL_CLIENT_CERTS ) )
                                lwsl_err( "Unable to load SSL Client certs from %s (set by
       LWS_OPENSSL_CLIENT_CERTS) -- client ssl isn't going to work\n", LWS_OPENSSL_CLIENT_CERTS );
                        } else if( ! SSL_CTX_load_verify_locations( ssl_client_ctx, ca_filepath,
       nullptr ) ) { lwsl_err( "Unable to load SSL Client certs file from %s -- client ssl isn't
       going to work\n", ctx_info_.client_ssl_ca_filepath ); lws_ssl_elaborate_error();
                        }
                        else
                            lwsl_info( "loaded ssl_ca_filepath\n" );
                        if( cert_filepath ) {
                            lwsl_notice( "%s: doing cert filepath\n", __func__ );
                            auto n = SSL_CTX_use_certificate_chain_file( ssl_client_ctx,
       cert_filepath ); if( n <  1) { lwsl_err( "problem %d getting cert '%s'\n", n, cert_filepath
       ); lws_ssl_elaborate_error(); return 1;
                            }
                            lwsl_notice( "Loaded client cert %s\n", cert_filepath );
                        }
                        if( private_key_filepath ) {
                            lwsl_notice( "%s: doing private key filepath\n", __func__ );
                            lws_ssl_bind_passphrase( ssl_client_ctx, &ctx_info_ );
                            // set the private key from KeyFile
                            if( SSL_CTX_use_PrivateKey_file( ssl_client_ctx, private_key_filepath,
       SSL_FILETYPE_PEM ) != 1 ) { lwsl_err( "use_PrivateKey_file '%s'\n", private_key_filepath );
                                lws_ssl_elaborate_error();
                                return 1;
                            }
                            lwsl_notice( "Loaded client cert private key %s\n", private_key_filepath
       );
                            // verify private key
                            if( ! SSL_CTX_check_private_key( ssl_client_ctx ) ) {
                                lwsl_err("Private SSL key doesn't match cert\n");
                                return 1;
                            }
                        }
                    } // if( ctx_info_.provided_client_ssl_ctx )
                    lwsl_info( "NLWS: =========== 2) setting client compat opts %p
       ===========================\n", ctx_info_.provided_client_ssl_ctx );
    */

    ctx_info_.iface =
        ( !interface_name_.empty() ) ? ( const_cast< char* >( interface_name_.c_str() ) ) : nullptr;
    //
    //
    //
    //
    //

    ctx_ = ::lws_create_context( &ctx_info_ );
    if ( ctx_ == nullptr ) {
        onLogMessage(
            e_ws_log_message_type_t::eWSLMT_error, "NLWS: Failed to create client context" );
        return false;
    }
    std::string sp = ( !strPath.empty() ) ? strPath.c_str() : "/";
    strURL_ = skutils::tools::format(
        "%s://%s:%d%s", ssl_flags_ ? "wss" : "ws", strHost.c_str(), nPort, sp.c_str() );
    std::string strOrigin = skutils::tools::format( "%s:%d", strHost.c_str(), nPort );

    clientThreadStopFlag_ = false;
    clientThreadWasStopped_ = true;
    std::atomic_bool threadInitDone = false, threadInitSuccess = false;
    clientThread_ = std::thread( [&]() {
        clientThreadWasStopped_ = false;
        if ( bDoLocalInitSSL ) {
            SSL_load_error_strings();
            SSL_library_init();
        }
        //				wsi_ =
        //					::lws_client_connect(
        //						ctx_,
        //						strHost.c_str(),
        //						nPort,
        //						ssl_flags_,
        //						sp.c_str(),
        //						strOrigin.c_str(), // "origin"
        //						nullptr,
        //						g_strDefaultProtocolName.c_str(), // protocols_[cpi].name
        //						-1
        //						);
        struct lws_client_connect_info cci;
        ::memset( &cci, 0, sizeof( struct lws_client_connect_info ) );
        cci.context = ctx_;
        cci.ssl_connection = ssl_flags_;
        cci.address = strHost.c_str();
        cci.port = nPort;
        cci.path = sp.c_str();
        cci.host = ::lws_canonical_hostname( ctx_ );
        cci.origin = strOrigin.c_str();  // "origin"
        cci.protocol = g_strDefaultProtocolName.c_str();
        cci.ietf_version_or_minus_one = -1;
        cci.pwsi = &wsi_;
        wsi_ = ::lws_client_connect_via_info( &cci );
        if ( wsi_ == nullptr ) {
            deinit();
            onLogMessage( e_ws_log_message_type_t::eWSLMT_error, "NLWS: client wsi creation fail" );
            threadInitSuccess = false;
            threadInitDone = true;
            clientThreadWasStopped_ = true;
            if ( bDoLocalInitSSL ) {
                ERR_free_strings();
            }
            return;
        }
        configure_wsi( wsi_ );
        stat_reg( this );
        threadInitSuccess = true;
        threadInitDone = true;
        onLogMessage( e_ws_log_message_type_t::eWSLMT_debug, "NLWS thread: start" );
        while ( !clientThreadStopFlag_ ) {
            ::lws_service( ctx_, g_lws_service_timeout_ms );
            do_writable_callbacks_all_protocol();
        }
        onLogMessage( e_ws_log_message_type_t::eWSLMT_debug, "NLWS thread: stop" );
        if ( bDoLocalInitSSL ) {
            ERR_free_strings();
        }
        clientThreadWasStopped_ = true;
    } );
    while ( !threadInitDone )
        std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
    if ( !threadInitSuccess )
        return false;
    // do_writable_callbacks_all_protocol();
    // while( ! ( connection_flag_ || destroy_flag_ ) )
    //	std::this_thread::sleep_for( std::chrono::milliseconds(20) );
    initialized_ = true;
    return true;
}
void client_api::deinit() {
    clientThreadStopFlag_ = true;
    try {
        if ( clientThread_.joinable() )
            clientThread_.join();
    } catch ( ... ) {
    }
    lock_type lock( mtx_api() );
    // wsi_ ???
    if ( ctx_ ) {
        ::lws_context_destroy( ctx_ );
        ctx_ = nullptr;
    }
    stat_unreg( this );
    clear_fields();
    delayed_close_reason_.clear();
    delayed_close_status_ = 0;
}
void client_api::close( int nCloseStatus, const std::string& msg ) {
    lock_type lock( mtx_api() );
    if ( !initialized_ )
        return;
    if ( wsi_ == nullptr )
        return;
    ///::lws_close_reason( wsi_, (lws_close_status)nCloseStatus, (unsigned char *)msg.c_str(),
    /// msg.length() ); deinit();
    delayed_close_reason_ = msg;
    delayed_close_status_ = nCloseStatus;
    //
    // clientThreadStopFlag_ = true;
    // while( ! clientThreadWasStopped_ )
    //    std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
}
void client_api::delay_deinit() {
    if ( delayed_de_init_ )
        return;
    delayed_de_init_ = true;
    if ( onDelayDeinit_ )
        onDelayDeinit_();
}

void client_api::onConnect() {
    if ( onConnect_ )
        onConnect_();
}
void client_api::onDisconnect( const std::string& strMessage ) {
    if ( onDisconnect_ )
        onDisconnect_( strMessage );
}
void client_api::onMessage( const message_payload_data& data, bool isFinalFragment ) {
    lock_type lock( mtx_api() );
    accumulator_.append( data );
    if ( !isFinalFragment )
        return;
    message_payload_data final_data = std::move( accumulator_ );
    accumulator_.clear();
    if ( onMessage_ )
        onMessage_( final_data );
}
void client_api::onFail( const std::string& strMessage ) {
    if ( onFail_ )
        onFail_( strMessage );
}
void client_api::onLogMessage( e_ws_log_message_type_t eWSLMT, const std::string& strMessage ) {
    if ( onLogMessage_ )
        onLogMessage_( eWSLMT, strMessage );
}

bool client_api::send( const message_payload_data& data ) {
    if ( !initialized_ )
        return false;
    lock_type lock( mtx_api() );
    // Push this onto the buffer. It will be written out when the socket is writable.
    buffer_.push_back( data );
    return true;
}

client_api::map_ctx_2_inst_t client_api::g_ctx_2_inst;
client_api::map_inst_2_ctx_t client_api::g_inst_2_ctx;
client_api::mutex_type client_api::g_ctx_reg_mtx;
void client_api::stat_reg( client_api* api ) {
    lock_type lock( g_ctx_reg_mtx );
    assert( api );
    assert( api->wsi_ );
    g_ctx_2_inst[api->wsi_] = api;
    g_inst_2_ctx[api] = api->wsi_;
}
void client_api::stat_unreg( client_api* api ) {
    lock_type lock( g_ctx_reg_mtx );
    assert( api );
    if ( !api->wsi_ )
        return;
    g_ctx_2_inst.erase( api->wsi_ );
    g_inst_2_ctx.erase( api );
}
client_api* client_api::stat_get( void* ctx ) {
    lock_type lock( g_ctx_reg_mtx );
    assert( ctx );
    map_ctx_2_inst_t::iterator itFind = g_ctx_2_inst.find( ctx ), itEnd = g_ctx_2_inst.end();
    if ( itFind == itEnd ) {
        //					assert( false );
        return nullptr;
    }
    client_api* api = itFind->second;
    //				assert( api );
    //				assert( api->wsi_ );
    //				assert( api->wsi_ == ctx );
    return api;
}


srvmode_t str2srvmode( const std::string& s ) {
    std::string v = skutils::tools::to_lower( s );
    if ( v.empty() )
        return srvmode_t::srvmode_simple;
    char c = v[0];
    if ( c == 's' )
        return srvmode_t::srvmode_simple;
    if ( c == 'p' )
        return srvmode_t::srvmode_external_poll;
#if ( defined LWS_WITH_LIBEV )
    if ( v == "ev" )
        return srvmode_t::srvmode_ev;
#endif  // (defined LWS_WITH_LIBEV)
#if ( defined LWS_WITH_LIBEVENT )
    if ( v == "event" )
        return srvmode_t::srvmode_event;
#endif  // (defined LWS_WITH_LIBEVENT)
#if ( defined LWS_WITH_LIBUV )
    if ( v == "uv" )
        return srvmode_t::srvmode_uv;
#endif  // (defined LWS_WITH_LIBUV)
    return srvmode_t::srvmode_simple;
}
std::string srvmode2str( srvmode_t m ) {
    switch ( m ) {
    case srvmode_t::srvmode_external_poll:
        return "poll";
#if ( defined LWS_WITH_LIBEV )
    case srvmode_t::srvmode_ev:
        return "ev";
#endif  // (defined LWS_WITH_LIBEV)
#if ( defined LWS_WITH_LIBEVENT )
    case srvmode_t::srvmode_event:
        return "event";
#endif  // (defined LWS_WITH_LIBEVENT)
#if ( defined LWS_WITH_LIBUV )
    case srvmode_t::srvmode_uv:
        return "uv";
#endif  // (defined LWS_WITH_LIBUV)
    case srvmode_t::srvmode_simple:
    default:
        return "simple";
    }  // switch( m )
}
std::string list_srvmodes_as_str() {
    std::string s;
    s += srvmode2str( srvmode_t::srvmode_simple );
    s += ", ";
    s += srvmode2str( srvmode_t::srvmode_external_poll );
#if ( defined LWS_WITH_LIBEV )
    s += ", ";
    s += srvmode2str( srvmode_t::srvmode_ev );
#endif  // (defined LWS_WITH_LIBEV)
#if ( defined LWS_WITH_LIBEVENT )
    s += ", ";
    s += srvmode2str( srvmode_t::srvmode_event );
#endif  // (defined LWS_WITH_LIBEVENT)
#if ( defined LWS_WITH_LIBUV )
    s += ", ";
    s += srvmode2str( srvmode_t::srvmode_uv );
#endif  // (defined LWS_WITH_LIBUV)
    return s;
}

int g_lws_service_timeout_ms = 1000;

srvmode_t g_default_srvmode = srvmode_t::
    //#if ( defined LWS_WITH_LIBUV )
    //    srvmode_uv;
    //#else
    //#if ( defined LWS_WITH_LIBEV )
    //    srvmode_ev;
    //#else
    //#if ( defined LWS_WITH_LIBEVENT )
    //    srvmode_event;
    //#else

    srvmode_simple;

//#endif
//#endif
//#endif

bool g_default_explicit_vhost_enable = true;  // srvmode_simple and srvmode_external_poll only
bool g_default_dynamic_vhost_enable = false;


server_api::connection_data::connection_data() {}
server_api::connection_data::~connection_data() {
    setPeer( nullptr );
}
peer_ptr_t server_api::connection_data::getPeer() {
    return pPeer_;
}
void server_api::connection_data::setPeer( peer_ptr_t pPeer ) {
    if ( pPeer_ == pPeer )
        return;
    if ( pPeer_ ) {
        pPeer_->onLogMessage( e_ws_log_message_type_t::eWSLMT_debug,
            cc::debug( "Server API entry destruction for " ) + description( true ) );
        pPeer_->ref_release();
        pPeer_ = nullptr;
        sn_ = 0;
    }
    if ( pPeer ) {
        pPeer_ = pPeer;
        pPeer_->ref_retain();
        sn_ = pPeer_->serial_number();
        pPeer_->onLogMessage( e_ws_log_message_type_t::eWSLMT_debug,
            cc::debug( "Server API entry construction for " ) + description( true ) );
    }
}
std::string server_api::connection_data::unique_string_identifier(
    bool isColored /*= false*/ ) const {
    std::string strCid( peer::stat_getCidString( cid_ ) ), strSlash( "/" );
    if ( isColored ) {
        strSlash = cc::debug( strSlash );
        if ( !strCid.empty() )
            strCid = cc::bright( strCid );
    }
    std::string strPeerSerialNumber = skutils::tools::format( "%" PRIu64, uint64_t( sn_ ) );
    if ( isColored )
        strPeerSerialNumber = cc::notice( strPeerSerialNumber );
    std::stringstream ss;
    ss << strCid << strSlash << strPeerSerialNumber;
    return ss.str();
}
std::string server_api::connection_data::description( bool isColored /*= false*/ ) const {
    std::string strEq( "=" ), strSeparator( ", " ), strNameUis( "uis" ), strNameIP( "ip" ),
        strValueIP( strPeerRemoteIP_ ), strNameCloseStatus( "close status" ),
        strValueCloseStatus( skutils::tools::format( "%d", delayed_close_status_ ) ),
        strNameCloseReason( "close reason" ),
        strValueCloseReason(
            isColored ? cc::warn( delayed_close_reason_ ) : delayed_close_reason_ );
    if ( isColored ) {
        strEq = cc::debug( strEq );
        strSeparator = cc::debug( strSeparator );
        strNameUis = cc::notice( strNameUis );
        strNameIP = cc::notice( strNameIP );
        strNameCloseStatus = cc::notice( strNameCloseStatus );
        strNameCloseReason = cc::notice( strNameCloseReason );
        strValueIP = cc::u( strValueIP );
        strValueCloseStatus = cc::warn( strValueCloseStatus );
    }
    std::stringstream ss;
    ss << strNameUis << strEq << unique_string_identifier( isColored );
    if ( !strPeerRemoteIP_.empty() )
        ss << strSeparator << strNameIP << strEq << strValueIP;
    if ( delayed_close_status_ != 0 )
        ss << strSeparator << strNameCloseStatus << strEq << strValueCloseStatus;
    if ( !delayed_close_reason_.empty() )
        ss << strSeparator << strNameCloseReason << strEq << strValueCloseReason;
    return ss.str();
}


server_api::server_api( basic_network_settings* pBNS ) : basic_api( pBNS ) {
    clear_fields();
    if ( ssl_perform_local_init_ ) {
        SSL_load_error_strings();
        SSL_library_init();
    }
}
server_api::~server_api() {
    deinit();
    if ( ssl_perform_local_init_ ) {
        ERR_free_strings();
    }
}

void server_api::clear_fields() {
    basic_api::clear_fields();
    ctx_info_.port = 443;
    vec_lws_protocols_[default_protocol_index_].callback = stat_callback_server;
    //
    srvmode_ = g_default_srvmode;
    explicit_vhost_enable_ = g_default_explicit_vhost_enable;
    //
    external_poll_max_elements_ = 0;
    external_poll_fds_ = nullptr;
    external_poll_fd_lookup_ = nullptr;
    external_poll_cnt_fds_ = 0;
    ::memset( &external_poll_tv_, 0, sizeof( struct timeval ) );
    external_poll_ms_1_second_ = 0;
    dynamic_vhost_enable_ = g_default_dynamic_vhost_enable;
    dynamic_vhost_ = nullptr;
    //
    vhost_ = nullptr;
    external_poll_ms_ = external_poll_oldms_ = 0;
    cert_path_.clear();
    key_path_.clear();
    ca_path_.clear();
    use_ssl_ = false;
    //
    serverInterruptFlag_ = false;
    fn_internal_interrupt_action_ = nullptr;
    //
    nHttpStatusToReturn_ = HTTP_STATUS_NOT_FOUND;
    strHttpBodyToReturn_ = "<html><body>HTTP is not enabled on this server.</body></html>";
    //
    nZeroLwsServiceAttemtIndex_ = 0;
    nZeroLwsServiceAttemtCount_ = 3;
    nZeroLwsServiceAttemtTimeoutMS_ = 10;
    //
#if ( defined LWS_WITH_LIBUV )
    memset( &foreign_loops_, 0, sizeof( foreign_loops_ ) );
#endif  // (defined LWS_WITH_LIBUV)
}

bool server_api::init( bool isSSL, int nPort, security_args* pSA, const char* strInterfaceName ) {
    if ( initialized_ )
        return false;
    clear_fields();
    lock_type lock( mtx_api() );
    interface_name_ = ( strInterfaceName && strInterfaceName[0] ) ? strInterfaceName : "";
#if ( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
    if ( server_disable_ipv6_ )
        ctx_info_.options |= LWS_SERVER_OPTION_DISABLE_IPV6;
    if ( server_validate_utf8_ )
        ctx_info_.options |= LWS_SERVER_OPTION_VALIDATE_UTF8;
    if ( server_skip_canonical_name_ )
        ctx_info_.options |= LWS_SERVER_OPTION_SKIP_SERVER_CANONICAL_NAME;
#endif  /// if( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
    use_ssl_ = isSSL;
    if ( use_ssl_ && pSA != nullptr ) {
        cert_path_ = pSA->strCertificateFile_;
        key_path_ = pSA->strPrivateKeyFile_;
        ca_path_ = pSA->strCertificationChainFile_;  // ???
        use_ssl_ = true;
        // ctx_info_.ssl_options_set = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_SINGLE_DH_USE; //
        // | SSL_OP_ALL
#if ( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
        if ( ssl_perform_global_init_ )
#endif  /// if( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
            ctx_info_.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
#if ( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
        if ( ssl_server_require_valid_certificate_ )
            ctx_info_.options |= LWS_SERVER_OPTION_REQUIRE_VALID_OPENSSL_CLIENT_CERT;
        if ( ssl_server_allow_non_ssl_on_ssl_port_ )
#endif  /// if( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
            ctx_info_.options |= LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT;
#if ( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
        if ( ssl_server_accept_connections_without_valid_cert_ )
            ctx_info_.options |= LWS_SERVER_OPTION_PEER_CERT_NOT_REQUIRED;
        if ( ssl_server_redirect_http_2_https_ )
#endif  /// if( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
            ctx_info_.options |= LWS_SERVER_OPTION_REDIRECT_HTTP_TO_HTTPS;
        //| LWS_SERVER_OPTION_CREATE_VHOST_SSL_CTX
        //| LWS_SERVER_OPTION_DISABLE_OS_CA_CERTS
        ctx_info_.ssl_cert_filepath = ( !cert_path_.empty() ) ? cert_path_.c_str() : nullptr;
        ctx_info_.ssl_private_key_filepath = ( !key_path_.empty() ) ? key_path_.c_str() : nullptr;
        ctx_info_.ssl_ca_filepath = ( !ca_path_.empty() ) ? ca_path_.c_str() : nullptr;
#if defined( LWS_HAVE_SSL_CTX_set1_param )
//					crl_path_ = ...
#endif
        //
        //
        onLogMessage( e_ws_log_message_type_t::eWSLMT_debug,
            skutils::tools::format(
                "ctx_info_.ssl_cert_filepath=\"%s\"    ctx_info_.ssl_private_key_filepath=\"%s\"   "
                "ctx_info_.ssl_ca_filepath=\"%s\"",
                ctx_info_.ssl_cert_filepath, ctx_info_.ssl_private_key_filepath,
                ctx_info_.ssl_ca_filepath ) );
    }  // if( use_ssl_ && pSA != nullptr )

    switch ( srvmode_ ) {
#if ( defined LWS_WITH_LIBEV )
    case srvmode_t::srvmode_ev:
        ctx_info_.options |= LWS_SERVER_OPTION_LIBEV;
        break;
#endif  // (defined LWS_WITH_LIBEV)
#if ( defined LWS_WITH_LIBEVENT )
    case srvmode_t::srvmode_event:
        ctx_info_.options |= LWS_SERVER_OPTION_LIBEVENT;
        break;
#endif  // (defined LWS_WITH_LIBEVENT)
#if ( defined LWS_WITH_LIBUV )
    case srvmode_t::srvmode_uv:
        ctx_info_.options |= LWS_SERVER_OPTION_LIBUV;
        if ( !pUvLoop_ )
            pUvLoop_.reset( new uv_loop_t );
        ::memset( pUvLoop_.get(), 0, sizeof( uv_loop_t ) );
        ::uv_loop_init( pUvLoop_.get() );  // create the foreign loop
        foreign_loops_[0] = pUvLoop_.get();
        ctx_info_.foreign_loops = foreign_loops_;
        // ctx_info_.pcontext = &context;
        break;
#endif  // (defined LWS_WITH_LIBUV)
    default:
        if ( explicit_vhost_enable_ )
            ctx_info_.options |= LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
        break;
    }  // switch( srvmode_ )

    ctx_info_.port = nPort;

    switch ( srvmode_ ) {
    case srvmode_t::srvmode_external_poll: {
        external_poll_max_elements_ = size_t(::getdtablesize() );
        size_t cntBytesToAlloc = external_poll_max_elements_ * sizeof( struct lws_pollfd );
        external_poll_fds_ = ( lws_pollfd* ) ::malloc( cntBytesToAlloc );
        if ( external_poll_fds_ == nullptr ) {
            ::lwsl_err(
                "Out of memory while allocating %d bytes of memory for %d elements in "
                "external_poll_fds_\n",
                int( cntBytesToAlloc ), int( external_poll_max_elements_ ) );
            return false;
        }
        ::memset( external_poll_fds_, 0, cntBytesToAlloc );
        cntBytesToAlloc = external_poll_max_elements_ * sizeof( int );
        external_poll_fd_lookup_ = ( int* ) ::malloc( cntBytesToAlloc );
        if ( external_poll_fd_lookup_ == nullptr ) {
            ::free( external_poll_fds_ );
            external_poll_fds_ = nullptr;
            ::lwsl_err(
                "Out of memory while allocating %d bytes of memory for %d elements in "
                "external_poll_fd_lookup_\n",
                int( cntBytesToAlloc ), int( external_poll_max_elements_ ) );
            return false;
        }
        ::memset( external_poll_fd_lookup_, 0, cntBytesToAlloc );
    } break;
    default:
        break;
    }  // switch( srvmode_ )

    ctx_info_.iface =
        ( !interface_name_.empty() ) ? ( const_cast< char* >( interface_name_.c_str() ) ) : nullptr;
    //    if ( interval_ping_ > 0 )
    //        ctx_info_.ws_ping_pong_interval = interval_ping_;
    ctx_info_.max_http_header_pool = 256;

    ctx_info_.user = this;

    ctx_info_.ssl_cipher_list = nullptr
        //					"ECDHE-ECDSA-AES256-GCM-SHA384:"
        //					"ECDHE-RSA-AES256-GCM-SHA384:"
        //					"DHE-RSA-AES256-GCM-SHA384:"
        //					"ECDHE-RSA-AES256-SHA384:"
        //					"HIGH:!aNULL:!eNULL:!EXPORT:"
        //					"!DES:!MD5:!PSK:!RC4:!HMAC_SHA1:"
        //					"!SHA1:!DHE-RSA-AES128-GCM-SHA256:"
        //					"!DHE-RSA-AES128-SHA256:"
        //					"!AES128-GCM-SHA256:"
        //					"!AES128-SHA256:"
        //					"!DHE-RSA-AES256-SHA256:"
        //					"!AES256-GCM-SHA384:"
        //					"!AES256-SHA256"
        ;


    //				info.ip_limit_ah = 24;   // for testing
    //				info.ip_limit_wsi = 105; // for testing

    ctx_ = ::lws_create_context( &ctx_info_ );
    if ( !ctx_ ) {
        lwsl_err( "libwebsocket init failed\n" );
        deinit();
        return false;
    }
    stat_reg( this );

    switch ( srvmode_ ) {
    case srvmode_t::srvmode_simple:
    case srvmode_t::srvmode_external_poll: {
        vhost_ = ::lws_create_vhost( ctx_, &ctx_info_ );
        if ( !vhost_ ) {
            lwsl_err( "vhost creation failed\n" );
            deinit();
            return false;
        }

        // For testing dynamic vhost create / destroy later, we use port + 1
        // Normally if you were creating more vhosts, you would set info.name
        // for each to be the hostname external clients use to reach it
        // // // //ctx_info_.port++;

        ::lws_init_vhost_client_ssl( &ctx_info_, vhost_ );
    } break;
    default:
        dynamic_vhost_enable_ = false;
        break;
    }  // switch( srvmode_ )

    ::memset( &external_poll_tv_, 0, sizeof( struct timeval ) );
    ::gettimeofday( &external_poll_tv_, nullptr );
    switch ( srvmode_ ) {
    case srvmode_t::srvmode_external_poll:
        external_poll_ms_1_second_ = 0;
        break;
    default:
        break;
    }  // switch( srvmode_ )
    // do_writable_callbacks_all_protocol();
    initialized_ = true;
    return true;
}
void server_api::deinit() {
    lock_type lock( mtx_api() );
    map_connections_t::iterator itCnWalk = connections_.begin(), itCnEnd = connections_.end();
    for ( ; itCnWalk != itCnEnd; ++itCnWalk ) {
        connection_data* pcd = itCnWalk->second;
        if ( pcd )
            delete pcd;
    }
    connections_.clear();

    // vhost_ ????

    if ( dynamic_vhost_ ) {
        ::lws_vhost_destroy( dynamic_vhost_ );
        dynamic_vhost_ = nullptr;
    }
    if ( ctx_ ) {
        auto ctx2destroy = ctx_;
        ctx_ = nullptr;
        ::lws_context_destroy( ctx2destroy );
    }
    stat_unreg( this );
    if ( external_poll_fds_ ) {
        ::free( external_poll_fds_ );
        external_poll_fds_ = nullptr;
    }
    if ( external_poll_fd_lookup_ ) {
        ::free( external_poll_fd_lookup_ );
        external_poll_fd_lookup_ = nullptr;
    }
    initialized_ = false;
    clear_fields();
}
void server_api::close( connection_identifier_t cid, int nCloseStatus, const std::string& msg ) {
    if ( !initialized_ )
        return;
    lock_type lock( mtx_api() );
    map_connections_t::iterator itCnFind = connections_.find( cid ), itCnEnd = connections_.end();
    if ( itCnFind == itCnEnd )
        return;
    server_api::connection_data* pcd = itCnFind->second;
    if ( !pcd )
        return;
    struct lws* wsi = pcd->wsi_;
    if ( wsi == nullptr )
        return;
    pcd->delayed_close_reason_ = msg;
    pcd->delayed_close_status_ = nCloseStatus;
}

void server_api::external_poll_service_loop(
    fn_continue_status_flag_t fnContinueStatusFlag, size_t nServiceLoopRunLimit /*= 0*/ ) {
    if ( fnContinueStatusFlag ) {
        if ( !fnContinueStatusFlag() )
            return;
    }
    int n = 0;
    switch ( srvmode_ ) {
    case srvmode_t::srvmode_external_poll:
        external_poll_ms_1_second_ = 0;
        break;
    default:
        lwsl_debug( "server_api::external_poll_service_loop() is unavailable!\n" );
        break;
    }  // switch( srvmode_ )
    for ( size_t idxLoop = 0; n >= 0; ++idxLoop ) {
        if ( nServiceLoopRunLimit > 0 && idxLoop >= nServiceLoopRunLimit )
            break;
        if ( fnContinueStatusFlag ) {
            if ( !fnContinueStatusFlag() )
                return;
        }
        ::memset( &external_poll_tv_, 0, sizeof( struct timeval ) );
        ::gettimeofday( &external_poll_tv_, nullptr );
        // this provokes the LWS_CALLBACK_SERVER_WRITEABLE for every live websocket connection using
        // the our default protocol, as soon as it can take more packets (usually immediately)
        external_poll_ms_ =
            ( external_poll_tv_.tv_sec * 1000 ) + ( external_poll_tv_.tv_usec / 1000 );
        if ( ( external_poll_ms_ - external_poll_oldms_ ) > 50 ) {
            do_writable_callbacks_all_protocol();
            external_poll_oldms_ = external_poll_ms_;
        }
        switch ( srvmode_ ) {
        case srvmode_t::srvmode_external_poll: {
            // this represents an existing server's single poll action which also includes
            // libwebsocket sockets
            if ( !service_poll( n ) )
                return;
            if ( n < 0 )
                continue;
        } break;
        default: {
            // if libwebsockets sockets are all we care about, you can use this api which takes care
            // of the poll() and looping through finding who needed service if no socket needs
            // service, it'll return anyway after the number of ms in the second argument
            n = ::lws_service( ctx_, g_lws_service_timeout_ms );
        } break;
        }  // switch( srvmode_ )
        //
        if ( dynamic_vhost_enable_ && ( !dynamic_vhost_ ) ) {
            ::lwsl_notice( "creating dynamic vhost...\n" );
            dynamic_vhost_ = ::lws_create_vhost( ctx_, &ctx_info_ );
        } else if ( ( !dynamic_vhost_enable_ ) && dynamic_vhost_ ) {
            ::lwsl_notice( "destroying dynamic vhost...\n" );
            ::lws_vhost_destroy( dynamic_vhost_ );
            dynamic_vhost_ = nullptr;
        }
    }  // for( size_t idxLoop = 0; ...
}
bool server_api::service_poll( int& n ) {  // if returns true - continue service loop
    if ( !initialized_ )
        return false;
    switch ( srvmode_ ) {
    case srvmode_t::srvmode_external_poll: {
        n = ::poll( external_poll_fds_, external_poll_cnt_fds_, 50 );
        if ( n < 0 )
            return true;
        if ( n ) {
            for ( n = 0; n < int( external_poll_cnt_fds_ ); ++n ) {
                if ( external_poll_fds_[n].revents )
                    // returns immediately if the fd does not match anything under libwebsockets
                    // control
                    if (::lws_service_fd( ctx_, &external_poll_fds_[n] ) < 0 )
                        return false;
            }
            // if needed, force-service wsis that may not have read all input
            while ( !::lws_service_adjust_timeout( ctx_, 1, 0 ) ) {
                lwsl_debug( "extpoll doing forced service!\n" );
                ::lws_service_tsi( ctx_, -1, 0 );
            }
        } else {
            // no revents, but before polling again, make lws check for any timeouts
            if ( external_poll_ms_ - external_poll_ms_1_second_ > 1000 ) {
                lwsl_debug( "1 per sec\n" );
                ::lws_service_fd( ctx_, nullptr );
                external_poll_ms_1_second_ = external_poll_ms_;
            }
        }
    } break;
    default: {
        auto n = ::lws_service( ctx_, g_lws_service_timeout_ms );
        if ( n == 0 ) {
            ++nZeroLwsServiceAttemtIndex_;
            if ( nZeroLwsServiceAttemtIndex_ >= nZeroLwsServiceAttemtCount_ ) {
                nZeroLwsServiceAttemtIndex_ = 0;
                std::this_thread::sleep_for(
                    std::chrono::milliseconds( nZeroLwsServiceAttemtTimeoutMS_ ) );
            }
        }
    } break;
    }  // switch( srvmode_ )
    return true;
}

void server_api::poll( fn_continue_status_flag_t fnContinueStatusFlag ) {
    switch ( srvmode_ ) {
    case srvmode_t::srvmode_simple: {
        int n = 0;
        service_poll( n );
        do_writable_callbacks_all_protocol();
    } break;
    case srvmode_t::srvmode_external_poll:
        if ( fnContinueStatusFlag && fnContinueStatusFlag() )
            external_poll_service_loop( fnContinueStatusFlag, 10 );
        break;
    default:
        lwsl_debug( "server_api::poll() is unavailable!\n" );
        break;
    }  // switch( srvmode_ )
}

#if ( defined LWS_WITH_LIBEV )
static void my_ev_timeout_cb( EV_P_ ev_timer* w, int revents ) {
    server_api* self = server_api::stat_ptr2api( w );
    if ( !self ) {
        ::lwsl_err( "my_ev_timeout_cb() failed to get server_api\n" );
        return;
    }
    if ( self->serverInterruptFlag_ ) {
        ::ev_break( EV_A_ EVBREAK_ONE );
        return;
    }
    //::lws_callback_on_writable_all_protocol( self->ctx_, &protocols[PROTOCOL_DUMB_INCREMENT] );
    self->do_writable_callbacks_all_protocol();
}
#endif  // (defined LWS_WITH_LIBEV)

#if ( defined LWS_WITH_LIBEVENT )
static void my_event_ev_timeout_cb( evutil_socket_t sock_fd, short events, void* ctx ) {
    server_api* self = ( server_api* ) ctx;
    if ( !self ) {
        ::lwsl_err( "my_event_ev_timeout_cb() failed to get server_api\n" );
        return;
    }
    if ( self->serverInterruptFlag_ ) {
        return;
    }
    //::lws_callback_on_writable_all_protocol( context, &protocols[PROTOCOL_DUMB_INCREMENT] );
    self->do_writable_callbacks_all_protocol();
}
#endif  // (defined LWS_WITH_LIBEVENT)

#if ( defined LWS_WITH_LIBUV )
struct my_uv_counter_struct {
    server_api& api_;
    volatile size_t cur_ = 0;
    volatile size_t lim_ = 65535;
    volatile bool stop_loop_ = false;
    my_uv_counter_struct( server_api& api ) : api_( api ) {}
};  /// struct my_uv_counter_struct
    //			static void my_uv_signal_cb( uv_signal_t * watcher, int signum ) {
    //				//struct my_uv_counter_struct * c = (struct my_uv_counter_struct *)t->data;
    //				//server_api & self = c->api_;
    //				//self.serverInterruptFlag_ = true;
    //				::lwsl_err( "Signal %d caught, exiting...\n", watcher->signum );
    //				switch( watcher->signum ) {
    //				case SIGTERM:
    //				case SIGINT:
    //					break;
    //				default:
    //					::signal( SIGABRT, SIG_DFL );
    //					::abort();
    //					break;
    //				}
    //				if( self.ctx_ ) {
    //					lwsl_debug( "my_uv_signal_cb() will cancel lws service\n" );
    //					::lws_cancel_service( self.ctx_ );
    //				}
    //			}
// static void my_uv_stopping_timer_cb( uv_timer_t* t ) {
//    struct my_uv_counter_struct* c = ( struct my_uv_counter_struct* ) t->data;
//    server_api& self = c->api_;
//    if ( self.serverInterruptFlag_ ) {
//        if ( self.ctx_ )
//            ::lws_libuv_stop( self.ctx_ );
//        ::uv_stop( t->loop );
//        return;
//    }
//}
static void my_uv_timer_cb( uv_timer_t* t ) {
    struct my_uv_counter_struct* c = ( struct my_uv_counter_struct* ) t->data;
    server_api& self = c->api_;
    if ( self.serverInterruptFlag_ ) {
        ::uv_stop( t->loop );
        return;
    }
    lwsl_debug( "  timer %p cb, count %d, loop has %d handles\n", t, int( c->cur_ ),
        t->loop->active_handles );
    if ( c->cur_++ >= c->lim_ ) {
        lwsl_debug( "stop loop from timer\n" );
        ::uv_timer_stop( t );
        if ( c->stop_loop_ )
            ::uv_stop( t->loop );
    }  // if( c->cur_++ >= c->lim_ )
}
static void my_uv_timer_test_cancel_cb( uv_timer_t* t ) {
    struct my_uv_counter_struct* c = ( struct my_uv_counter_struct* ) t->data;
    if ( c == nullptr ) {
        ::uv_stop( t->loop );
        return;
    }
    server_api& self = c->api_;
    if ( self.serverInterruptFlag_ ) {
        ::uv_stop( t->loop );
        return;
    }
    // if( self.ctx_ )
    //	::lws_cancel_service( self.ctx_ );
}
static void my_uv_timer_close_cb( uv_handle_t* h ) {
    ::lwsl_notice( "timer close cb %p, loop has %d handles\n", h, h->loop->active_handles );
}
//			static void my_uv_outer_signal_cb( uv_signal_t * s, int signum ) {
//				::lwsl_notice( "Foreign loop got signal %d\n", signum );
//				::uv_signal_stop( s );
//				::uv_stop( s->loop );
//			}
static void my_uv_lws_uv_close_cb( uv_handle_t* /*handle*/ ) {
    //::lwsl_err( "%s\n", __func__ );
}
static void my_uv_lws_uv_walk_cb( uv_handle_t* handle, void* /*arg*/ ) {
    ::uv_close( handle, my_uv_lws_uv_close_cb );
}
// static void my_uv_idle_cb( uv_idle_t* uvi ) {
//    server_api* self = ( server_api* ) uvi->data;
//    if ( self->serverInterruptFlag_ ) {
//        if ( self->ctx_ )
//            ::lws_libuv_stop( self->ctx_ );
//        else
//            return;
//    }
//}
#endif  // (defined LWS_WITH_LIBUV)


bool server_api::service_mode_supported() const {
    if ( !initialized_ )
        return false;
    switch ( srvmode_ ) {
    case srvmode_t::srvmode_external_poll:
#if ( defined LWS_WITH_LIBEV )
    case srvmode_t::srvmode_ev:
#endif  // (defined LWS_WITH_LIBEV)
#if ( defined LWS_WITH_LIBEVENT )
    case srvmode_t::srvmode_event:
#endif  // (defined LWS_WITH_LIBEVENT)
#if ( defined LWS_WITH_LIBUV )
    case srvmode_t::srvmode_uv:
#endif  // (defined LWS_WITH_LIBUV)
        return true;
    default:
        return false;
    }  // switch( srvmode_ )
}
void server_api::service_interrupt() {
    if ( !initialized_ )
        return;

    try {
        if ( fn_internal_interrupt_action_ )
            fn_internal_interrupt_action_();
    } catch ( ... ) {
    }
    switch ( srvmode_ ) {
    case srvmode_t::srvmode_simple:
    case srvmode_t::srvmode_external_poll: {
        // TO-CHECK: whether this works as server interrupt
        if ( ctx_ )
            ::lws_cancel_service( ctx_ );

        //::lws_context_destroy( ctx_ );

        // lock_type lock( mtx_api() );
        // deinit();
    }
        return;
    default:
        lwsl_debug( "server_api::service() is unavailable!\n" );
        return;
    }  // switch( srvmode_ )
}
void server_api::service( fn_continue_status_flag_t fnContinueStatusFlag ) {
    if ( !initialized_ )
        return;
    switch ( srvmode_ ) {
    case srvmode_t::srvmode_external_poll:
        external_poll_service_loop( fnContinueStatusFlag );
        return;
#if ( defined LWS_WITH_LIBEV )
    case srvmode_t::srvmode_ev: {
        // notice: fnContinueStatusFlag is not used here
        struct ev_loop* loop = ::ev_default_loop( 0 );
        fn_internal_interrupt_action_ = [&]() {
            serverInterruptFlag_ = true;
            if ( loop != nullptr )
                ::ev_break( loop, EVBREAK_ONE );
        };
        ev_timer timeout_watcher;

        //						for( n = 0; n < (int)ARRAY_SIZE(sigs); n++ ) {
        //							ev_init(&signals[n], signal_cb);
        //							ev_signal_set(&signals[n], sigs[n]);
        //							ev_signal_start(loop, &signals[n]);
        //						}

        server_api::stat_ptr_reg( &timeout_watcher, this );
        ::lws_ev_initloop( ctx_, loop, 0 );
        ev_timer_init( &timeout_watcher, my_ev_timeout_cb, 0.05, 0.05 );
        ::ev_timer_start( loop, &timeout_watcher );
        ev_run( loop, 0 );
        server_api::stat_ptr_unreg( &timeout_watcher );
    }
        return;
#endif  // (defined LWS_WITH_LIBEV)
#if ( defined LWS_WITH_LIBEVENT )
    case srvmode_t::srvmode_event: {
        // notice: fnContinueStatusFlag is not used here
        struct event_base* event_base_loop = ::event_base_new();
        fn_internal_interrupt_action_ = [&]() {
            serverInterruptFlag_ = true;
            if ( event_base_loop != nullptr )
                ::event_base_loopbreak( event_base_loop );
        };
        struct event* timeout_watcher = nullptr;
        // do not use the default Signal Event Watcher & Handler
        ::lws_event_sigint_cfg( ctx_, 0, nullptr );
        // initialize the LWS with libevent loop
        ::lws_event_initloop( ctx_, event_base_loop, 0 );
        //
        timeout_watcher =
            ::event_new( event_base_loop, -1, EV_PERSIST, my_event_ev_timeout_cb, this );
        struct timeval tv = {0, 50000};
        ::evtimer_add( timeout_watcher, &tv );
        ::event_base_dispatch( event_base_loop );
    }
        return;
#endif  // (defined LWS_WITH_LIBEVENT)
#if ( defined LWS_WITH_LIBUV )
    case srvmode_t::srvmode_uv: {
        // notice: fnContinueStatusFlag is not used here
        // uv_idle_t idler;
        //::memset( &idler, 0, sizeof(uv_idle_t) );

        // uv_signal_t signal_outer;
        //::memset( &signal_outer, 0, sizeof(uv_signal_t) );

        uv_timer_t timer_outer;
        ::memset( &timer_outer, 0, sizeof( uv_timer_t ) );

        uv_timer_t timer_test_cancel;
        ::memset( &timer_test_cancel, 0, sizeof( uv_timer_t ) );

        my_uv_counter_struct ctr( *this );

        fn_internal_interrupt_action_ = [&]() {
            serverInterruptFlag_ = true;
            ::uv_stop( pUvLoop_.get() );
        };

        //
        // next part is usually invoked before lws context creation
        //

        // run some timer on that loop just so loop is not 'clean'
        ::uv_timer_init( pUvLoop_.get(), &timer_test_cancel );
        timer_test_cancel.data = &ctr;
        ::uv_timer_start( &timer_test_cancel, my_uv_timer_test_cancel_cb, 200,
            200 );  // TO-CHECK: was 2000, 2000

        ::uv_timer_init( pUvLoop_.get(), &timer_outer );
        timer_outer.data = &ctr;
        //        ctr.cur_ = 0;
        //        ctr.lim_ = ctr.cur_ + 5;  // TO-CHECK: wth is this?
        //        ctr.stop_loop_ = true;
        //        ::uv_timer_start( &timer_outer, my_uv_timer_cb, 0, 1000 );
        //        ::lwsl_notice( "running loop without libwebsockets for %d s\n", int( ctr.lim_ ) );

        ::uv_run( pUvLoop_.get(), UV_RUN_DEFAULT );
        fn_internal_interrupt_action_ = nullptr;
        // timer will stop loop and we will get here


        //
        // next part is usually invoked after lws context creation
        //

        //::lws_uv_sigint_cfg( ctx_, 1, my_uv_signal_cb );
        // prepare inner timer on loop, to run along with lws. Will exit after 5s while lws
        // keeps running
        my_uv_counter_struct ctr_inner( *this );  //= { 0, 3, 0 };
        int e;
        uv_timer_t timer_inner;
        ::uv_timer_init( pUvLoop_.get(), &timer_inner );
        timer_inner.data = &ctr_inner;
        ::uv_timer_start( &timer_inner, my_uv_timer_cb, 200, 1000 );
        // make this timer long-lived, should keep firing after lws exits
        ctr.cur_ = 0;
        ctr.lim_ = ctr.cur_ + 1000;  // TO-CHECK: wth is this?
        ::uv_timer_start( &timer_outer, my_uv_timer_cb, 0, 1000 );
        //
        ::uv_run( pUvLoop_.get(), UV_RUN_DEFAULT );
        fn_internal_interrupt_action_ = nullptr;
        // we are here either because signal stopped us, or outer timer expired
        //
        // close short timer
        ::uv_timer_stop( &timer_inner );
        ::uv_close( ( uv_handle_t* ) &timer_inner, my_uv_timer_close_cb );

        ::lwsl_notice( "Destroying lws context\n" );
        ::lws_context_destroy( ctx_ );  // detach lws

        lwsl_notice( "Please wait while the outer libuv test continues for 10s\n" );
        ctr.lim_ = ctr.cur_ + 10;  // TO-CHECK: wth is this?

        // try and run outer timer for 10 more seconds, (or sigint outer handler) after lws has
        // left the loop
        ::uv_run( pUvLoop_.get(), UV_RUN_DEFAULT );
        fn_internal_interrupt_action_ = nullptr;

        // clean up the foreign loop now...
        // PHASE 1: stop and close things we created outside of lws
        ::uv_timer_stop( &timer_outer );
        ::uv_timer_stop( &timer_test_cancel );
        ::uv_close( ( uv_handle_t* ) &timer_outer, my_uv_timer_close_cb );
        //::uv_signal_stop( &signal_outer );
        e = 100;
        while ( e-- ) {
            ::uv_run( pUvLoop_.get(), UV_RUN_NOWAIT );
            fn_internal_interrupt_action_ = nullptr;
        }
        // PHASE 2: close anything remaining
        ::uv_walk( pUvLoop_.get(), my_uv_lws_uv_walk_cb, nullptr );
        e = 100;
        while ( e-- ) {
            ::uv_run( pUvLoop_.get(), UV_RUN_NOWAIT );
            fn_internal_interrupt_action_ = nullptr;
        }
        // PHASE 3: close the UV loop itself
        e = ::uv_loop_close( pUvLoop_.get() );
        ::lwsl_notice( "uv loop close rc %s\n", e ? ::uv_strerror( e ) : "ok" );
        // PHASE 4: finalize context destruction
        ::lws_context_destroy( ctx_ );
        ctx_ = nullptr;
    }
        return;
#endif  // (defined LWS_WITH_LIBUV)
    default:
        lwsl_debug( "server_api::service() is unavailable!\n" );
        return;
    }  // switch( srvmode_ )
}  // namespace nlws

void server_api::onConnect( connection_identifier_t cid, struct lws* wsi,
    const char* strPeerClientAddressName, const char* strPeerRemoteIP ) {
    lock_type lock( mtx_api() );
    connection_data* pcd = new connection_data;
    pcd->cid_ = cid;
    // pcd->sn_ = serial_numper();
    pcd->wsi_ = wsi;
    pcd->creationTime_ = time( nullptr );
    pcd->strPeerClientAddressName_ = strPeerClientAddressName;
    pcd->strPeerRemoteIP_ = skutils::tools::format( "%s://%s", ::lws_is_ssl( wsi ) ? "wss" : "ws",
        ( strPeerRemoteIP != nullptr && strPeerRemoteIP[0] != '\0' ) ? strPeerRemoteIP :
                                                                       "unknown" );
    impl_eraseConnection( cid );
    connections_[cid] = pcd;
    if ( onConnect_ )
        onConnect_( cid, wsi, strPeerClientAddressName, strPeerRemoteIP );
}
void server_api::onDisconnect( connection_identifier_t cid, const std::string& strMessage ) {
    lock_type lock( mtx_api() );
    if ( onDisconnect_ )
        onDisconnect_( cid, strMessage );
    impl_removeConnection( cid );
}
void server_api::onMessage(
    connection_identifier_t cid, const message_payload_data& data, bool isFinalFragment ) {
    lock_type lock( mtx_api() );
    map_connections_t::iterator itCnFind = connections_.find( cid ), itCnEnd = connections_.end();
    if ( itCnFind == itCnEnd )
        return;
    server_api::connection_data* pcd = itCnFind->second;
    if ( !pcd )
        return;
    message_payload_data& a = pcd->accumulator_;
    a.append( data );
    if ( !isFinalFragment )
        return;
    message_payload_data final_data = std::move( a );
    a.clear();
    if ( onMessage_ ) {
        onMessage_( cid, final_data );
    }
}
void server_api::onHttp( connection_identifier_t cid ) {
    lock_type lock( mtx_api() );
    if ( onHttp_ )
        onHttp_( cid );
}
void server_api::onFail( connection_identifier_t cid, const std::string& strMessage ) {
    onLogMessage( e_ws_log_message_type_t::eWSLMT_error,
        cc::error( "Error: " ) + cc::warn( strMessage ) + " on cid " + cc::num10( cid ) );
    lock_type lock( mtx_api() );
    if ( impl_removeConnection( cid ) ) {
        if ( onFail_ )
            onFail_( cid, strMessage );
    }
}
void server_api::onLogMessage( e_ws_log_message_type_t eWSLMT, const std::string& strMessage ) {
    if ( onLogMessage_ )
        onLogMessage_( eWSLMT, strMessage );
}

bool server_api::send( connection_identifier_t cid, const message_payload_data& data ) {
    if ( !initialized_ )
        return false;
    lock_type lock( mtx_api() );
    map_connections_t::iterator itCnFind = connections_.find( cid ), itCnEnd = connections_.end();
    if ( itCnFind == itCnEnd )
        return false;
    server_api::connection_data* pcd = itCnFind->second;
    if ( !pcd )
        return false;
    // Push this onto the buffer. It will be written out when the socket is writable.
    payload_queue_t& pq = pcd->buffer_;
    pq.push_back( data );
    return true;
}
//			size_t server_api::broadcast( const message_payload_data & data ) { // ugly simple, we
// do
// not need it) 				if( ! initialized_ ) 					return 0;
// lock_type lock( mtx_api()
// ); 				map_connections_t::iterator itCnWalk = connections_.begin(), itCnEnd =
// connections_.end(); size_t cntSent = 0; 				for( ; itCnWalk != itCnEnd; ++ itCnWalk ) {
// if( send( itCnWalk->first, data ) )
//						++ cntSent;
//				}
//				return cntSent;
//			}

std::string server_api::getPeerClientAddressName( connection_identifier_t cid ) {  // notice: used
                                                                                   // as "origin"
    if ( !initialized_ )
        return "";
    lock_type lock( mtx_api() );
    map_connections_t::iterator itCnFind = connections_.find( cid ), itCnEnd = connections_.end();
    if ( itCnFind == itCnEnd )
        return "";
    server_api::connection_data* pcd = itCnFind->second;
    if ( pcd )
        return pcd->strPeerClientAddressName_;
    return "";
}
std::string server_api::getPeerRemoteIP( connection_identifier_t cid ) {  // notice: no port is
                                                                          // returned
    if ( !initialized_ )
        return "";
    lock_type lock( mtx_api() );
    map_connections_t::iterator itCnFind = connections_.find( cid ), itCnEnd = connections_.end();
    if ( itCnFind == itCnEnd )
        return "";
    server_api::connection_data* pcd = itCnFind->second;
    if ( pcd )
        return pcd->strPeerRemoteIP_;
    return "";
}
peer_ptr_t server_api::getPeer( connection_identifier_t cid ) {
    if ( !initialized_ )
        return nullptr;
    lock_type lock( mtx_api() );
    map_connections_t::iterator itCnFind = connections_.find( cid ), itCnEnd = connections_.end();
    if ( itCnFind == itCnEnd )
        return nullptr;
    server_api::connection_data* pcd = itCnFind->second;
    peer_ptr_t pPeer = nullptr;
    if ( pcd )
        pPeer = pcd->getPeer();
    return pPeer;
}
peer_ptr_t server_api::detachPeer( connection_identifier_t cid ) {
    if ( !initialized_ )
        return nullptr;
    lock_type lock( mtx_api() );
    map_connections_t::iterator itCnFind = connections_.find( cid ), itCnEnd = connections_.end();
    if ( itCnFind == itCnEnd )
        return nullptr;
    server_api::connection_data* pcd = itCnFind->second;
    if ( !pcd )
        return nullptr;
    peer_ptr_t pPeer = pcd->getPeer();
    pcd->setPeer( nullptr );
    return pPeer;
}
bool server_api::setPeer( connection_identifier_t cid, peer_ptr_t pPeer ) {
    if ( !initialized_ )
        return false;
    lock_type lock( mtx_api() );
    map_connections_t::iterator itCnFind = connections_.find( cid ), itCnEnd = connections_.end();
    if ( itCnFind == itCnEnd )
        return false;
    server_api::connection_data* pcd = itCnFind->second;
    if ( pcd ) {
        pcd->setPeer( pPeer );
        return true;
    }
    return false;
}
int64_t server_api::delayed_adjustment_pong_timeout(
    connection_identifier_t cid ) const {  // NLWS-specific
    if ( !initialized_ )
        return -1;
    lock_type lock( mtx_api() );
    map_connections_t::const_iterator itCnFind = connections_.find( cid ), itCnEnd =
                                                                               connections_.end();
    if ( itCnFind == itCnEnd )
        return -1;
    server_api::connection_data* pcd = itCnFind->second;
    int64_t to = -1;
    if ( pcd )
        to = pcd->delayed_adjustment_pong_timeout_;
    return to;
}
bool server_api::delayed_adjustment_pong_timeout(
    connection_identifier_t cid, int64_t to ) {  // NLWS-specific
    if ( !initialized_ )
        return false;
    lock_type lock( mtx_api() );
    map_connections_t::iterator itCnFind = connections_.find( cid ), itCnEnd = connections_.end();
    if ( itCnFind == itCnEnd )
        return false;
    server_api::connection_data* pcd = itCnFind->second;
    if ( pcd ) {
        pcd->delayed_adjustment_pong_timeout_ = to;
        return true;
    }
    return false;
}

std::string server_api::getValue( connection_identifier_t cid, const std::string& strName ) {
    if ( !initialized_ )
        return "";
    lock_type lock( mtx_api() );
    map_connections_t::iterator itCnFind = connections_.find( cid ), itCnEnd = connections_.end();
    if ( itCnFind == itCnEnd )
        return "";
    server_api::connection_data* pcd = itCnFind->second;
    if ( pcd )
        return pcd->keyValueMap_[strName];
    return "";
}
bool server_api::setValue(
    connection_identifier_t cid, const std::string& strName, const std::string& strValue ) {
    if ( !initialized_ )
        return false;
    lock_type lock( mtx_api() );
    map_connections_t::iterator itCnFind = connections_.find( cid ), itCnEnd = connections_.end();
    if ( itCnFind == itCnEnd )
        return false;
    server_api::connection_data* pcd = itCnFind->second;
    if ( pcd ) {
        pcd->keyValueMap_[strName] = strValue;
        return true;
    }
    return false;
}

int server_api::getNumberOfConnections() {
    if ( !initialized_ )
        return 0;
    lock_type lock( mtx_api() );
    return connections_.size();
}

void server_api::run( uint64_t timeout ) {
    if ( !initialized_ )
        return;
    while ( true ) {
        wait( timeout );
    }
}
void server_api::wait( uint64_t timeout ) {
    if ( !initialized_ )
        return;
    if (::lws_service( ctx_, timeout ) < 0 )
        throw std::runtime_error( "failure while polling for socket activity" );
}

bool server_api::impl_eraseConnection( connection_identifier_t cid ) {
    map_connections_t::iterator itCnFind = connections_.find( cid ), itCnEnd = connections_.end();
    if ( itCnFind == itCnEnd )
        return false;
    server_api::connection_data* pcd = itCnFind->second;
    connections_.erase( itCnFind );
    if ( pcd )
        delete pcd;
    return true;
}
bool server_api::impl_removeConnection( connection_identifier_t cid ) {
    if ( !initialized_ )
        return false;
    lock_type lock( mtx_api() );
    return impl_eraseConnection( cid );
}

server_api::map_ctx_2_inst_t server_api::g_ctx_2_inst;
server_api::map_inst_2_ctx_t server_api::g_inst_2_ctx;
server_api::mutex_type server_api::g_ctx_reg_mtx;
void server_api::stat_reg( server_api* api ) {
    lock_type lock( g_ctx_reg_mtx );
    assert( api );
    assert( api->ctx_ );
    g_ctx_2_inst[api->ctx_] = api;
    g_inst_2_ctx[api] = api->ctx_;
}
void server_api::stat_unreg( server_api* api ) {
    lock_type lock( g_ctx_reg_mtx );
    assert( api );
    if ( !api->ctx_ )
        return;
    g_ctx_2_inst.erase( api->ctx_ );
    g_inst_2_ctx.erase( api );
}
server_api* server_api::stat_get( void* ctx ) {
    lock_type lock( g_ctx_reg_mtx );
    assert( ctx );
    map_ctx_2_inst_t::iterator itFind = g_ctx_2_inst.find( ctx ), itEnd = g_ctx_2_inst.end();
    if ( itFind == itEnd ) {
        //					assert( false );
        return nullptr;
    }
    server_api* api = itFind->second;
    //				assert( api );
    //				assert( api->ctx_ );
    //				assert( api->ctx_ == ctx );
    return api;
}

server_api::mutex_type server_api::g_ptr_reg_mtx;
server_api::map_ptr_2_inst_t server_api::g_ptr_2_inst;
void server_api::stat_ptr_reg( void* p, server_api* api ) {
    lock_type lock( g_ptr_reg_mtx );
    assert( p );
    assert( api );
    g_ptr_2_inst[p] = api;
}
void server_api::stat_ptr_unreg( void* p ) {
    lock_type lock( g_ptr_reg_mtx );
    g_ptr_2_inst.erase( p );
}
server_api* server_api::stat_ptr2api( void* p ) {
    lock_type lock( g_ptr_reg_mtx );
    assert( p );
    map_ptr_2_inst_t::iterator itFind = g_ptr_2_inst.find( p ), itEnd = g_ptr_2_inst.end();
    if ( itFind == itEnd ) {
        //					assert( false );
        return nullptr;
    }
    server_api* api = itFind->second;
    //				assert( api );
    return api;
}


basic_participant::basic_participant() {}
basic_participant::~basic_participant() {}
std::string basic_participant::stat_backend_name() {
    return "nlws";
}
nlohmann::json basic_participant::toJSON( bool /*bSkipEmptyStats = true*/ ) const {
    nlohmann::json jo = nlohmann::json::object();
    jo["type"] = "participant";
    return jo;
}

basic_sender::basic_sender() {}
basic_sender::~basic_sender() {}
nlohmann::json basic_sender::toJSON( bool /*bSkipEmptyStats = true*/ ) const {
    nlohmann::json jo = nlohmann::json::object();
    jo["type"] = "sender";
    return jo;
}

basic_socket::basic_socket() {
    bns_assign_from_default_instance();
}
basic_socket::~basic_socket() {}
nlohmann::json basic_socket::toJSON( bool /*bSkipEmptyStats = true*/ ) const {
    nlohmann::json jo = nlohmann::json::object();
    jo["type"] = "socket";
    return jo;
}
void basic_socket::close() {}

void basic_socket::cancel() {}
void basic_socket::pause_reading() {}

void basic_socket::onOpen( hdl_t hdl ) {
    if ( onOpen_ )
        onOpen_( *this, hdl );
}
void basic_socket::onClose( hdl_t hdl, const std::string& reason, int local_close_code,
    const std::string& local_close_code_as_str ) {
    if ( onClose_ )
        onClose_( *this, hdl, reason, local_close_code, local_close_code_as_str );
}
void basic_socket::onFail( hdl_t hdl ) {
    if ( onFail_ )
        onFail_( *this, hdl );
}
void basic_socket::onMessage( hdl_t hdl, opcv eOpCode, const std::string& msg ) {
    if ( onMessage_ )
        onMessage_( *this, hdl, eOpCode, msg );
}
// void basic_socket::onStreamSocketInit( int native_fd ) {
//	std::stringstream ss;
//	skutils::network::basic_stream_socket_init( native_fd, ss );
//	std::string msg = ss.str();
//	if( ! msg.empty() )
//		onLogMessage( e_ws_log_message_type_t::eWSLMT_error, msg );
//}
void basic_socket::onLogMessage( e_ws_log_message_type_t eWSLMT, const std::string& msg ) {
    if ( !msg.empty() ) {
        if ( bLogToStdCerr_ )
            std::cerr << msg;
        if ( onLogMessage_ )
            onLogMessage_( *this, eWSLMT, msg );
    }
}


peer::peer( server& srv, const hdl_t& hdl )
    : srv_( srv ),
      peer_serial_number_( srv.request_new_peer_serial_number() ),
      hdl_( hdl ),
      cid_( 0 ),
      was_disconnected_( false ) {
    traffic_stats::register_default_event_queues_for_web_socket_peer();
    cid_ = stat_getCid( hdl );
}
peer::~peer() {
    if ( !was_disconnected_ )
        close( "BYE!" );
}
nlohmann::json peer::toJSON( bool bSkipEmptyStats /*= true*/ ) const {
    nlohmann::json jo = nlohmann::json::object();
    jo["type"] = "peer";
    jo["uid"] = unique_string_identifier();
    jo["type_description"] = getShortTypeDescrition( false );
    jo["description"] = getShortPeerDescription( false );
    jo["remote_ip"] = getRemoteIp();
    jo["connection_id"] = getCidString();
    jo["serial_number"] = serial_number();
    jo["scheme"] = srv().last_scheme_cached_;
    jo["stats"] = traffic_stats::toJSON( bSkipEmptyStats );
    return jo;
}
std::string peer::getShortTypeDescrition( bool isColored /*= false*/ ) const {
    // lock_type lock( ref_mtx() );
    std::string s( "peer" );
    if ( isColored ) {
        if ( !s.empty() )
            s = cc::info( s );
    }
    return s;
}
std::string peer::getShortPeerDescription(
    bool isColored /*= false*/, bool isLifetime /*= true*/, bool isTrafficStats /*= true*/ ) const {
    // lock_type lock( ref_mtx() );
    std::stringstream ss;
    std::string strSpace( " " ), strCommaSpace( ", " ), strIP( getRemoteIp() );
    if ( isColored ) {
        strSpace = cc::debug( strSpace );
        strCommaSpace = cc::debug( strCommaSpace );
        if ( !strIP.empty() )
            strIP = cc::u( strIP );
    }
    traffic_stats::time_point tpNow = traffic_stats::clock::now();
    ss << unique_string_identifier( isColored ) << strSpace << getShortTypeDescrition( isColored )
       << strSpace << strIP;
    if ( isLifetime )
        ss << strCommaSpace << getLifeTimeDescription( tpNow, isColored );
    if ( isTrafficStats )
        ss << strCommaSpace << getTrafficStatsDescription( tpNow, isColored );
    return ss.str();
}
std::string peer::unique_string_identifier( bool isColored /*= false*/ ) const {
    std::string strCid( getCidString() ), strSlash( "/" );
    if ( isColored ) {
        strSlash = cc::debug( strSlash );
        if ( !strCid.empty() )
            strCid = cc::bright( strCid );
    }
    size_t s_no = serial_number();
    std::string strPeerSerialNumber = skutils::tools::format( "%" PRIu64, uint64_t( s_no ) );
    if ( isColored )
        strPeerSerialNumber = cc::notice( strPeerSerialNumber );
    std::stringstream ss;
    ss << strCid << strSlash << strPeerSerialNumber;
    return ss.str();
}
void peer::onPeerRegister() {
    traffic_stats::log_open();
}
void peer::onPeerUnregister() {  // peer will no longer receive onMessage after call to this
    opened_ = false;
    traffic_stats::log_close();
}
bool peer::isServerSide() const {
    return true;
}
connection_identifier_t peer::cid() const {
    if ( cid_ == 0 )
        cid_ = stat_getCid( hdl_ );
    return cid_;
}
connection_identifier_t peer::stat_getCid( const hdl_t& hdl ) {
    return hdl;
}

std::string peer::stat_getCidString( const connection_identifier_t& cid ) {
    std::string strCid;
    try {
        std::ostringstream ss;
        ss  //<< std::uppercase << std::setfill('0') << std::setw(8) << std::hex
            << cid;
        strCid = ss.str();
    } catch ( ... ) {
        strCid.clear();
    }
    return strCid;
}
connection_identifier_t peer::stat_parseCid( const std::string& strCid ) {
    connection_identifier_t cid = 0;
    if ( !strCid.empty() ) {
        try {
            std::istringstream ss( strCid );
            ss >> cid;
        } catch ( ... ) {
            cid = 0;
        }
    }
    return cid;
}
connection_identifier_t peer::stat_parseCid( const char* strCid ) {
    connection_identifier_t cid = 0;
    if ( strCid != 0 && strCid[0] != '\0' ) {
        std::string s = strCid;
        cid = stat_parseCid( s );
    }
    return cid;
}

bool peer::isConnected() const noexcept {
    return opened_;
}
void peer::async_close( const std::string& msg,
    int nCloseStatus,            // = int(close_status::going_away)
    size_t nTimeoutMilliseconds  // = 0
) {
    ref_retain();
    try {
        std::thread( [this, msg, nCloseStatus, nTimeoutMilliseconds]() -> void {
            try {
                if ( nTimeoutMilliseconds > 0 )
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds( nTimeoutMilliseconds ) );
                close( msg, nCloseStatus );
            } catch ( ... ) {
            }
            ref_release();
        } )
            .detach();
        return;
    } catch ( ... ) {
        ref_release();
    }
}
void peer::close( const std::string& msg,
    int nCloseStatus  // = int(close_status::going_away)
) {
    if ( was_disconnected_ )
        return;
    ref_retain();
    try {
        srv_.close( hdl_, nCloseStatus, msg );
        traffic_stats::event_add( g_strEventNameWebSocketPeerDisconnect );
    } catch ( const std::exception& ex ) {
        const char* strWhat = ex.what();
        if ( strWhat == nullptr || strWhat[0] == '\0' )
            strWhat = "unknown exception";
        std::stringstream ss;
        ss << cc::error( "Exception: " ) << cc::warn( strWhat );
        srv_.onLogMessage( e_ws_log_message_type_t::eWSLMT_error, ss.str() );
        // clean_up( cid_ );
        traffic_stats::event_add( g_strEventNameWebSocketPeerDisconnectFail );
    } catch ( ... ) {
        std::stringstream ss;
        ss << cc::error( "Unknown exception" );
        srv_.onLogMessage( e_ws_log_message_type_t::eWSLMT_error, ss.str() );
        // clean_up( cid_ );
        traffic_stats::event_add( g_strEventNameWebSocketPeerDisconnectFail );
    }
    traffic_stats::log_close();
    ref_release();
}
void peer::cancel() {
    try {
        srv_.cancel_hdl( hdl_ );
    } catch ( ... ) {
    }
}
void peer::pause_reading() {
    try {
        srv_.pause_reading_hdl( hdl_ );
    } catch ( ... ) {
    }
}
void peer::onMessage( const std::string& msg, opcv eOpCode ) {
    if ( onPeerMessage_ )
        onPeerMessage_( *this, msg, eOpCode );
    if ( eOpCode == opcv::text ) {
        traffic_stats::log_text_rx( msg.length() );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesRecvText );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesRecv );
    } else if ( eOpCode == opcv::binary ) {
        traffic_stats::log_bin_rx( msg.size() );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesRecvBinary );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesRecv );
    }
}
void peer::onClose(
    const std::string& reason, int local_close_code, const std::string& local_close_code_as_str ) {
    opened_ = false;
    if ( onPeerClose_ )
        onPeerClose_( *this, reason, local_close_code, local_close_code_as_str );
    traffic_stats::log_close();
}
void peer::onFail() {
    traffic_stats::event_add( traffic_stats::g_strEventNameWebSocketFail );
    srv().event_add( traffic_stats::g_strEventNameWebSocketFail );
    opened_ = false;
    if ( onPeerFail_ )
        onPeerFail_( *this );
    traffic_stats::log_close();
}
bool peer::sendMessage( const std::string& msg, opcv eOpCode ) {
    // ss << cc::debug(">>> ") << cc::warn(getSender()) << cc::debug(", ") <<
    // cc::warn(getCidString()) << cc::debug(", ") << cc::c(msg) << "/n";
    std::string strCid = getCidString();
    std::string strRemoteIp = getRemoteIp();
    try {
        if ( !srv_.sendMessage( hdl_, msg, eOpCode ) )
            return false;
        if ( eOpCode == opcv::text ) {
            traffic_stats::log_text_tx( msg.length() );
            traffic_stats::event_add( g_strEventNameWebSocketMessagesSentText );
            traffic_stats::event_add( g_strEventNameWebSocketMessagesSent );
        } else if ( eOpCode == opcv::binary ) {
            traffic_stats::log_bin_tx( msg.size() );
            traffic_stats::event_add( g_strEventNameWebSocketMessagesSentBinary );
            traffic_stats::event_add( g_strEventNameWebSocketMessagesSent );
        }
        return true;
    } catch ( const std::exception& ex ) {
        const char* strWhat = ex.what();
        if ( strWhat == nullptr || strWhat[0] == '\0' )
            strWhat = "unknown exception";
        std::stringstream ss;
        ss << cc::error( "nlws-peer sendMessage(" ) << cc::warn( strCid ) << cc::error( "," )
           << cc::u( strRemoteIp ) << cc::error( ") failed, exception: " ) << cc::warn( strWhat );
        srv_.onLogMessage( e_ws_log_message_type_t::eWSLMT_error, ss.str() );
    } catch ( ... ) {
        std::stringstream ss;
        ss << cc::error( "nlws-peer sendMessage(" ) << cc::warn( strCid ) << cc::error( "," )
           << cc::u( strRemoteIp ) << cc::error( ") failed, unknown exception" );
        srv_.onLogMessage( e_ws_log_message_type_t::eWSLMT_error, ss.str() );
    }
    return false;
}
void peer::onLogMessage( e_ws_log_message_type_t eWSLMT, const std::string& msg ) {
    srv_.onLogMessage( eWSLMT, msg );
}
const security_args& peer::onGetSecurityArgs() const {
    return srv_.onGetSecurityArgs();
}

std::string peer::getRemoteIp() const {
    return srv_.getRemoteIp( hdl_ );
}
std::string peer::getOrigin() const {
    return srv_.getOrigin( hdl_ );
}

std::string peer::getCidString() const {
    return stat_getCidString( cid_ );
}


server::server( basic_network_settings* pBNS )
    : api_( pBNS ), server_serial_number_( 0 ), listen_backlog_( 0 ) {
    traffic_stats::register_default_event_queues_for_web_socket_server();
    api_.onConnect_ = [this]( connection_identifier_t cid, struct lws* /*wsi*/,
                          const char* /*strPeerClientAddressName*/,
                          const char* /*strPeerRemoteIP*/ ) { onOpen( cid ); };
    api_.onDisconnect_ = [this]( connection_identifier_t cid, const std::string& strMessage ) {
        onClose( cid, strMessage.c_str(), 0, "" );
    };
    api_.onMessage_ = [this]( connection_identifier_t cid, const message_payload_data& data ) {
        opcv eOpCode = ( data.type() == LWS_WRITE_BINARY ) ? opcv::binary : opcv::text;
        std::string s;
        data.store_to_string( s );
        onMessage( cid, eOpCode, s );
    };
    api_.onHttp_ = [this]( connection_identifier_t cid ) {
        onHttp( cid );  // return value is ignored in this implementation
    };
    api_.onFail_ = [this]( connection_identifier_t cid, const std::string& strMessage ) {
        if ( !strMessage.empty() )
            onLogMessage( e_ws_log_message_type_t::eWSLMT_error, cc::error( strMessage ) );
        onFail( cid );
    };
    api_.onLogMessage_ = [this]( e_ws_log_message_type_t eWSLMT, const std::string& strMessage ) {
        onLogMessage( eWSLMT, strMessage );
    };
}
server::~server() {
    close();
}
nlohmann::json server::toJSON( bool bSkipEmptyStats /*= true*/ ) const {
    nlohmann::json jo = nlohmann::json::object();
    jo["type"] = "server";
    jo["server_type"] = cc::strip( type() );
    jo["uid"] = "sum";
    jo["remote_ip"] = "";
    jo["nma_peer_type"] = "NMA server";
    jo["nma_hub_type"] = "";
    jo["nma_ui_type"] = "";
    jo["serial"] = "";
    jo["email"] = "";
    jo["portal_monitoring"] = "";
    jo["port"] = port();
    jo["scheme"] = last_scheme_cached_;
    jo["stats"] = traffic_stats::toJSON( bSkipEmptyStats );
    return jo;
}
size_t server::request_new_peer_serial_number() {
    size_t n = server_serial_number_;
    ++server_serial_number_;
    return n;
}

bool server::isServerSide() const {
    return true;
}
std::string server::type() const {
    return api_.use_ssl_ ? cc::success( "SSL/TLS" ) : cc::fatal( "non-secure" );
}
int server::port() const {
    return api_.ctx_info_.port;
}
int server::defaultPort() const {
    return api_.use_ssl_ ? 443 : 9666;
}
bool server::open( const std::string& scheme, int nPort, const char* strInterfaceName ) {
    last_scheme_cached_ = scheme;
    bool isSSL = ( scheme == "ws" ) ? false : true;
    basic_network_settings &bns_api = api_, bns_this = ( *this );
    bns_api = bns_this;
    if ( !api_.init( isSSL, nPort, this, strInterfaceName ) ) {
        traffic_stats::event_add( g_strEventNameWebSocketServerStartFail );
        return false;
    }
    traffic_stats::log_open();
    traffic_stats::event_add( g_strEventNameWebSocketServerStart );
    return true;
}
void server::close() {
    server_api::lock_type lock( api_.mtx_api() );
    api_.deinit();
    traffic_stats::log_close();
    // server_serial_number_ = 0;
    traffic_stats::event_add( g_strEventNameWebSocketServerStop );
}
void server::close( hdl_t hdl, int nCloseStatus, const std::string& msg ) {
    api_.close( hdl, nCloseStatus, msg );
}
void server::cancel_hdl( hdl_t /*hdl*/ ) {
    // TO-FIX: cancel() not implemented yet
}
void server::pause_reading_hdl( hdl_t /*hdl*/ ) {
    // TO-FIX: pause_reading() not implemented yet
}
void server::poll( fn_continue_status_flag_t fnContinueStatusFlag ) {
    api_.poll( fnContinueStatusFlag );
}
void server::reset() {
    // TO-FIX: reset() not implemented yet
}
bool server::service_mode_supported() const {
    return api_.service_mode_supported();
}
void server::service_interrupt() {
    api_.service_interrupt();
}
void server::service( fn_continue_status_flag_t fnContinueStatusFlag ) {
    api_.service( fnContinueStatusFlag );
}
std::string server::getRemoteIp( hdl_t hdl ) {
    return api_.getPeerRemoteIP( hdl );  // notice: no port is returned
}
std::string server::getOrigin( hdl_t hdl ) {
    return api_.getPeerClientAddressName( hdl );  // notice: used as "origin"
}
bool server::sendMessage( hdl_t hdl, const std::string& msg, opcv eOpCode /*= opcv::text*/ ) {
    message_payload_data data;
    if ( eOpCode == opcv::binary )
        data.set_binary( msg );
    else
        data.set_text( msg );
    if ( !api_.send( hdl, data ) )
        return false;
    if ( eOpCode == opcv::text ) {
        traffic_stats::log_text_tx( msg.length() );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesSentText );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesSent );
    } else if ( eOpCode == opcv::binary ) {
        traffic_stats::log_bin_tx( msg.size() );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesSentBinary );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesSent );
    }
    return true;
}
//
peer_ptr_t server::onPeerInstantiate( hdl_t hdl ) {
    peer_ptr_t pPeer( nullptr );
    try {
        if ( onPeerInstantiate_ )
            pPeer = onPeerInstantiate_( *this, hdl );
        if ( !pPeer )
            pPeer = new peer( *this, hdl );
    } catch ( ... ) {
    }
    return pPeer;
}
peer_ptr_t server::getPeer( hdl_t hdl ) {
    return api_.getPeer( hdl );
}
peer_ptr_t server::detachPeer( hdl_t hdl ) {
    return api_.detachPeer( hdl );
}
bool server::onPeerRegister( peer_ptr_t pPeer ) {
    if ( !pPeer )
        return false;
    try {
        pPeer->opened_ = true;
        // hdl_t hdl = pPeer->hdl();
        pPeer->ref_retain();  // mormal, typically first/last ref
        if ( onPeerRegister_ )
            onPeerRegister_( pPeer );
        pPeer->onPeerRegister();
        traffic_stats::event_add( g_strEventNameWebSocketPeerConnect );
    } catch ( ... ) {
        return false;
    }
    return true;
}
bool server::onPeerUnregister( peer_ptr_t pPeer ) {
    if ( !pPeer )
        return false;
    pPeer->ref_retain();  // exrra ref, to protect onPeerUnregister_ specific implementatoion, such
                          // as un-ddos accept
    try {
        traffic_stats::event_add( g_strEventNameWebSocketPeerDisconnect );
        if ( onPeerUnregister_ )
            onPeerUnregister_( pPeer );
    } catch ( ... ) {
    }
    try {
        pPeer->onPeerUnregister();
    } catch ( ... ) {
    }
    pPeer->opened_ = false;
    if ( pPeer->ref_release() > 0 )  // exrra ref, to protect onPeerUnregister_ specific
                                     // implementatoion, such as un-ddos accept
        pPeer->ref_release();  // mormal, typically first/last ref, dependant on onPeerUnregister_
                               // functor implementation
    return true;
}
int64_t server::delayed_adjustment_pong_timeout(
    connection_identifier_t cid ) const {  // NLWS-specific
    return api_.delayed_adjustment_pong_timeout( cid );
}
void server::delayed_adjustment_pong_timeout(
    connection_identifier_t cid, int64_t to ) {  // NLWS-specific
    api_.delayed_adjustment_pong_timeout( cid, to );
}

//
void server::onOpen( hdl_t hdl ) {
    basic_socket::onOpen( hdl );
    peer_ptr_t pPeer( nullptr );
    try {
        pPeer = onPeerInstantiate( hdl );
    } catch ( ... ) {
        pPeer = nullptr;
    }
    if ( !pPeer ) {
        std::string strRemoteIp = getRemoteIp( hdl );
        std::stringstream ss;
        ss << cc::error( "Failed to instantiate peer(" ) << cc::u( strRemoteIp )
           << cc::error( ")" );
        onLogMessage( e_ws_log_message_type_t::eWSLMT_error, ss.str() );
        close( hdl, close_status::internal_endpoint_error, "internal peer initialization error" );
        return;
    }
    onPeerRegister( pPeer );
    api_.setPeer( hdl, pPeer );
}
void server::onClose( hdl_t hdl, const std::string& reason, int local_close_code,
    const std::string& local_close_code_as_str ) {
    basic_socket::onClose( hdl, reason, local_close_code, local_close_code_as_str );
    peer_ptr_t pPeer = detachPeer( hdl );
    if ( pPeer ) {
        pPeer->was_disconnected_ = true;
        onPeerUnregister( pPeer );
    }
}
void server::onFail( hdl_t hdl ) {
    traffic_stats::event_add( traffic_stats::g_strEventNameWebSocketFail );
    basic_socket::onFail( hdl );
}
void server::onMessage( hdl_t hdl, opcv eOpCode, const std::string& msg ) {
    peer_ptr_t pPeer = getPeer( hdl );
    if ( pPeer ) {
        pPeer->onMessage( msg, eOpCode );
        if ( eOpCode == opcv::text ) {
            traffic_stats::log_text_rx( msg.length() );
            traffic_stats::event_add( g_strEventNameWebSocketMessagesRecvText );
            traffic_stats::event_add( g_strEventNameWebSocketMessagesRecv );
        } else if ( eOpCode == opcv::binary ) {
            traffic_stats::log_bin_rx( msg.size() );
            traffic_stats::event_add( g_strEventNameWebSocketMessagesRecvBinary );
            traffic_stats::event_add( g_strEventNameWebSocketMessagesRecv );
        }
        return;
    }
    //
    std::string strRemoteIp = getRemoteIp( hdl );
    std::stringstream ss;
    ss << cc::warn( "No instantiated peer(" ) << cc::u( strRemoteIp )
       << cc::warn( ") for message (" ) << cc::str( msg ) << cc::warn( "<<" );
    onLogMessage( e_ws_log_message_type_t::eWSLMT_warning, ss.str() );
    //
    basic_socket::onMessage( hdl, eOpCode, msg );
    if ( eOpCode == opcv::text ) {
        traffic_stats::log_text_rx( msg.length() );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesRecvText );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesRecv );
    } else if ( eOpCode == opcv::binary ) {
        traffic_stats::log_bin_rx( msg.size() );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesRecvBinary );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesRecv );
    }
}
bool server::onHttp( hdl_t hdl ) {
    return onHttp_ ? onHttp_( *this, hdl ) : false;
}

// void server::onStreamSocketInit( int native_fd ) {
//	basic_socket::onStreamSocketInit( native_fd );
//}
void server::onLogMessage( e_ws_log_message_type_t eWSLMT, const std::string& msg ) {
    basic_socket::onLogMessage( eWSLMT, msg );
}
const security_args& server::onGetSecurityArgs() const {
    return ( *this );
}


client::client( basic_network_settings* pBNS ) : api_( pBNS ) {
    traffic_stats::register_default_event_queues_for_web_socket_client();
    api_.onConnect_ = [this]() {
        onOpen( api_.cid_ );
        setConnected( true );
    };
    api_.onDisconnect_ = [this]( const std::string& strMessage ) {
        setConnected( false );
        onClose( api_.cid_, strMessage.c_str(), 0, "" );
    };
    api_.onMessage_ = [this]( const message_payload_data& data ) {
        opcv eOpCode = ( data.type() == LWS_WRITE_BINARY ) ? opcv::binary : opcv::text;
        std::string s;
        data.store_to_string( s );
        onMessage( api_.cid_, eOpCode, s );
    };
    api_.onFail_ = [this]( const std::string& strMessage ) {
        if ( !strMessage.empty() )
            onLogMessage( e_ws_log_message_type_t::eWSLMT_error, cc::error( strMessage ) );
        setConnected( false );
        onFail( api_.cid_ );
    };
    api_.onLogMessage_ = [this]( e_ws_log_message_type_t eWSLMT, const std::string& strMessage ) {
        onLogMessage( eWSLMT, strMessage );
    };
    api_.onDelayDeinit_ = [this]() { onDelayDeinit(); };
}
client::~client() {
    enableRestartTimer( false );
    close();
}
nlohmann::json client::toJSON( bool bSkipEmptyStats /*= true*/ ) const {
    nlohmann::json jo = nlohmann::json::object();
    jo["type"] = "client";
    jo["url"] = uri();
    jo["stats"] = traffic_stats::toJSON( bSkipEmptyStats );
    return jo;
}
bool client::isServerSide() const {
    return false;
}

bool client::is_ssl() const {
    return api_.ssl_flags_ ? true : false;
}
std::string client::type() const {
    return api_.ssl_flags_ ? cc::success( "SSL/TLS" ) : cc::fatal( "non-secure" );
}
std::string client::uri() const {
    return api_.strURL_;
}

bool client::open( const std::string& uri, const char* strInterfaceName ) {
    close();
    try {
        strLastURI_ = uri;
        basic_network_settings &bns_api = api_, bns_this = ( *this );
        bns_api = bns_this;
        if ( !api_.init( uri, this, strInterfaceName ) ) {
            traffic_stats::event_add( g_strEventNameWebSocketClientConnectFail );
            return false;
        }
        traffic_stats::log_open();
        traffic_stats::event_add( g_strEventNameWebSocketClientConnect );
        return true;
    } catch ( const std::exception& ex ) {
        const char* strWhat = ex.what();
        if ( strWhat == nullptr || strWhat[0] == '\0' )
            strWhat = "unknown exception";
        std::stringstream ss;
        ss << cc::error( "open: " ) << cc::warn( strWhat );
        onLogMessage( e_ws_log_message_type_t::eWSLMT_error, ss.str() );
    } catch ( ... ) {
        std::stringstream ss;
        ss << cc::error( "open: " ) << cc::warn( "unknown exception" );
        onLogMessage( e_ws_log_message_type_t::eWSLMT_error, ss.str() );
    }
    traffic_stats::event_add( g_strEventNameWebSocketClientConnectFail );
    return false;
}
bool client::openLocalHost( int nPort ) {
    std::stringstream ss;
    ss << "ws://localhost:" << nPort;
    std::string uri = ss.str();
    return open( uri );
}
void client::close() {
    if ( !isRestartTimerEnabled() )
        restart_timer_.stop();
    api_.deinit();
    traffic_stats::log_close();
    traffic_stats::event_add( g_strEventNameWebSocketClientDisconnect );
}
void client::resetConnection() {
    enableRestartTimer( false );
    client::close();
    enableRestartTimer( true );
}
void client::async_close( const std::string& msg,
    int nCloseStatus,            // = int(close_status::going_away)
    size_t nTimeoutMilliseconds  // = 0
) {
    ref_retain();
    try {
        std::thread( [this, msg, nCloseStatus, nTimeoutMilliseconds]() -> void {
            try {
                if ( nTimeoutMilliseconds > 0 )
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds( nTimeoutMilliseconds ) );
                close( msg, nCloseStatus );
            } catch ( ... ) {
            }
            ref_release();
        } )
            .detach();
        return;
    } catch ( ... ) {
        ref_release();
    }
}
void client::close( const std::string& msg, int nCloseStatus /*= int(close_status::going_away)*/ ) {
    api_.close( nCloseStatus, msg );
    // api_.deinit();
    traffic_stats::log_close();
    traffic_stats::event_add( g_strEventNameWebSocketClientDisconnect );
}
void client::cancel() {
    // TO-FIX: cancel() not yet implemented
}
void client::pause_reading() {
    // TO-FIX: pause_reading() not yet implemented
}

bool client::isConnected() const noexcept {
    return api_.connection_flag_;
}
void client::setConnected( bool /*state*/ ) {
    // notice: "state" parameter must be in sync with api_.connection_flag_
}

bool client::sendMessage( const std::string& msg, opcv eOpCode ) {
    message_payload_data data;
    if ( eOpCode == opcv::binary )
        data.set_binary( msg );
    else
        data.set_text( msg );
    if ( !api_.send( data ) )
        return false;
    if ( eOpCode == opcv::text ) {
        traffic_stats::log_text_tx( msg.length() );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesSentText );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesSent );
    } else if ( eOpCode == opcv::binary ) {
        traffic_stats::log_bin_tx( msg.size() );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesSentBinary );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesSent );
    }
    return true;
}
void client::onMessage( hdl_t hdl, opcv eOpCode, const std::string& msg ) {
    basic_socket::onMessage( hdl, eOpCode, msg );
    if ( eOpCode == opcv::text ) {
        traffic_stats::log_text_rx( msg.length() );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesRecvText );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesRecv );
    } else if ( eOpCode == opcv::binary ) {
        traffic_stats::log_bin_rx( msg.size() );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesRecvBinary );
        traffic_stats::event_add( g_strEventNameWebSocketMessagesRecv );
    }
}
void client::onDisconnected() {
    if ( onDisconnected_ )
        onDisconnected_( *this );
    traffic_stats::log_close();
}
void client::onOpen( hdl_t hdl ) {
    basic_socket::onOpen( hdl );
    // sendMessage( Proto::getRegisterMethod() );
}
void client::impl_ensure_restart_timer_is_running() {
    if ( !isRestartTimerEnabled() )
        return;
    // if( isConnected() )
    //	return;
    if ( restart_timer_.running() )
        return;
    restart_timer_.run( std::chrono::seconds( timeout_restart_on_close_ ), [&]() {
        std::thread( [&]() {
            //								if( isConnected() )
            //									return;
            onLogMessage(
                e_ws_log_message_type_t::eWSLMT_info, "NLWS: will re-connect to " + strLastURI_ );
            close();
            open( strLastURI_ );
        } )
            .detach();
    } );
}
void client::onClose( hdl_t hdl, const std::string& reason, int local_close_code,
    const std::string& local_close_code_as_str ) {
    basic_socket::onClose( hdl, reason, local_close_code, local_close_code_as_str );
    impl_ensure_restart_timer_is_running();
    traffic_stats::log_close();
    traffic_stats::event_add( g_strEventNameWebSocketClientDisconnect );
}
void client::onFail( hdl_t hdl ) {
    traffic_stats::event_add( traffic_stats::g_strEventNameWebSocketFail );
    basic_socket::onFail( hdl );
    impl_ensure_restart_timer_is_running();
    traffic_stats::log_close();
}
void client::onDelayDeinit() {  // NLWS-specific
}
// void client::onStreamSocketInit( int native_fd ) {
//	basic_socket::onStreamSocketInit( native_fd );
//}
void client::onLogMessage( e_ws_log_message_type_t eWSLMT, const std::string& msg ) {
    basic_socket::onLogMessage( eWSLMT, msg );
}
const security_args& client::onGetSecurityArgs() const {
    return ( *this );
}
bool client::isRestartTimerEnabled() const {
    return isRestartTimerEnabled_;
}
void client::enableRestartTimer( bool isEnabled ) {
    isRestartTimerEnabled_ = isEnabled;
    if ( isRestartTimerEnabled_ )
        impl_ensure_restart_timer_is_running();
    else
        restart_timer_.stop();
}
int64_t client::delayed_adjustment_pong_timeout() const {  // NLWS-specific
    return api_.delayed_adjustment_pong_timeout_;
}
void client::delayed_adjustment_pong_timeout( int64_t to ) {  // NLWS-specific
    api_.delayed_adjustment_pong_timeout_ = to;
}

};  // namespace nlws

};  // namespace ws
};  // namespace skutils
