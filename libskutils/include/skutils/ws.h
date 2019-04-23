#if ( !defined __SKUTILS_WS_H )
#define __SKUTILS_WS_H 1

#include <stdint.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <skutils/async_work.h>
#include <skutils/atomic_shared_ptr.h>
#include <skutils/multithreading.h>
#include <skutils/network.h>
#include <skutils/stats.h>
#include <skutils/utils.h>

#include <libwebsockets.h>
#include <lws_config.h>
#if ( !defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
#define __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ 1
#endif

#if ( defined LWS_WITH_LIBUV )
#include <uv.h>
#endif  // (defined LWS_WITH_LIBUV)

namespace skutils {
namespace ws {

enum class e_ws_logging_level_t {
    eWSLL_none,
    eWSLL_basic,
    eWSLL_detailed
};  /// enum class e_ws_logging_level_t
extern e_ws_logging_level_t g_eWSLL;
extern e_ws_logging_level_t str2wsll( const std::string& s );
extern std::string wsll2str( e_ws_logging_level_t eWSLL );

enum class e_ws_log_message_type_t {
    eWSLMT_debug,
    eWSLMT_info,
    eWSLMT_warning,
    eWSLMT_error
};  /// enum class e_ws_log_message_type_t

class traffic_stats : public skutils::stats::named_event_stats {
public:
    typedef traffic_stats myt;
    typedef myt& myrt;
    typedef myt&& myrrt;
    typedef const myt& myrct;
    typedef std::lock_guard< myt > lock_type;
    typedef skutils::stats::clock clock;
    typedef skutils::stats::time_point time_point;
    typedef skutils::stats::nanoseconds nanoseconds;
    typedef skutils::stats::duration_base_t duration_base_t;
    typedef skutils::stats::duration duration;
    typedef skutils::stats::event_record_item_t event_record_item_t;
    typedef skutils::stats::event_queue_t event_queue_t;
    typedef skutils::stats::named_event_stats named_event_stats;
    typedef skutils::stats::bytes_count_t bytes_count_t;
    typedef skutils::stats::traffic_record_item_t traffic_record_item_t;
    typedef skutils::stats::traffic_queue_t traffic_queue_t;
    enum e_last_instance_state_changing_type_t {
        elisctt_instantiated,
        elisctt_opened,
        elisctt_closed,
    };  // enum e_last_instance_state_changing_type_t
protected:
    volatile bytes_count_t text_tx_, text_rx_, bin_tx_, bin_rx_;
    e_last_instance_state_changing_type_t elisctt_;
    time_point time_stamp_instantiated_, time_stamp_opened_, time_stamp_closed_;
    traffic_queue_t traffic_queue_all_tx_, traffic_queue_all_rx_, traffic_queue_text_tx_,
        traffic_queue_text_rx_, traffic_queue_bin_tx_, traffic_queue_bin_rx_;
    void init();
    myrt limit( size_t lim );

public:
    traffic_stats();
    traffic_stats( myrct x );
    traffic_stats( myrrt x );
    virtual ~traffic_stats();
    myrt operator=( myrct x ) { return assign( x ); }
    myrt operator=( myrrt x ) { return move( x ); }
    bool operator==( myrct x ) const { return ( compare( x ) == 0 ) ? true : false; }
    bool operator!=( myrct x ) const { return ( compare( x ) != 0 ) ? true : false; }
    bool operator<( myrct x ) const { return ( compare( x ) < 0 ) ? true : false; }
    bool operator<=( myrct x ) const { return ( compare( x ) <= 0 ) ? true : false; }
    bool operator>( myrct x ) const { return ( compare( x ) > 0 ) ? true : false; }
    bool operator>=( myrct x ) const { return ( compare( x ) >= 0 ) ? true : false; }
    operator bool() const { return ( !empty() ); }
    bool operator!() const { return empty(); }
    virtual bytes_count_t text_tx() const;
    virtual bytes_count_t text_rx() const;
    virtual bytes_count_t bin_tx() const;
    virtual bytes_count_t bin_rx() const;
    virtual bytes_count_t tx() const;
    virtual bytes_count_t rx() const;
    double bps_text_tx( time_point tpNow ) const;
    double bps_text_tx_last_known() const;
    double bps_text_tx() const;
    double bps_text_rx( time_point tpNow ) const;
    double bps_text_rx_last_known() const;
    double bps_text_rx() const;
    double bps_bin_tx( time_point tpNow ) const;
    double bps_bin_tx_last_known() const;
    double bps_bin_tx() const;
    double bps_bin_rx( time_point tpNow ) const;
    double bps_bin_rx_last_known() const;
    double bps_bin_rx() const;
    double bps_tx( time_point tpNow ) const;
    double bps_tx_last_known() const;
    double bps_tx() const;
    double bps_rx( time_point tpNow ) const;
    double bps_rx_last_known() const;
    double bps_rx() const;
    virtual myrt log_text_tx( bytes_count_t n );
    virtual myrt log_text_rx( bytes_count_t n );
    virtual myrt log_bin_tx( bytes_count_t n );
    virtual myrt log_bin_rx( bytes_count_t n );
    e_last_instance_state_changing_type_t last_instance_state_changing_type() const;
    std::string last_instance_state_changing_type_as_str() const;
    time_point instantiated() const;
    nanoseconds instantiated_ago( time_point tpNow ) const;
    nanoseconds instantiated_ago() const;
    time_point changed() const;
    nanoseconds changed_ago( time_point tpNow ) const;
    nanoseconds changed_ago() const;
    virtual void log_open();
    virtual void log_close();
    bool empty() const;
    void clear();
    int compare( myrct x ) const;
    myrt assign( myrct x );
    myrt move( myrt x );
    virtual void lock();
    virtual void unlock();
    virtual std::string getLifeTimeDescription( time_point tpNow, bool isColored = false ) const;
    std::string getLifeTimeDescription( bool isColored = false ) const;
    virtual std::string getTrafficStatsDescription(
        time_point tpNow, bool isColored = false ) const;
    std::string getTrafficStatsDescription( bool isColored = false ) const;
    nlohmann::json toJSON( time_point tpNow, bool bSkipEmptyStats = true ) const;
    nlohmann::json toJSON( bool bSkipEmptyStats = true ) const;
    static size_t g_nDefaultEventQueueSizeForWebSocket;
    static const char g_strEventNameWebSocketFail[];
    static const char g_strEventNameWebSocketMessagesRecvText[];
    static const char g_strEventNameWebSocketMessagesRecvBinary[];
    static const char g_strEventNameWebSocketMessagesRecv[];
    static const char g_strEventNameWebSocketMessagesSentText[];
    static const char g_strEventNameWebSocketMessagesSentBinary[];
    static const char g_strEventNameWebSocketMessagesSent[];
    void registrer_default_event_queues_for_web_socket();
    static const char g_strEventNameWebSocketPeerConnect[];
    static const char g_strEventNameWebSocketPeerDisconnect[];
    static const char g_strEventNameWebSocketPeerDisconnectFail[];
    void registrer_default_event_queues_for_web_socket_peer();
    static const char g_strEventNameWebSocketServerStart[];
    static const char g_strEventNameWebSocketServerStartFail[];
    static const char g_strEventNameWebSocketServerStop[];
    void registrer_default_event_queues_for_web_socket_server();
    static const char g_strEventNameWebSocketClientConnect[];
    static const char g_strEventNameWebSocketClientConnectFail[];
    static const char g_strEventNameWebSocketClientDisconnect[];
    static const char g_strEventNameWebSocketClientReconnect[];
    void registrer_default_event_queues_for_web_socket_client();
};  /// class traffic_stats

class guarded_traffic_stats : public traffic_stats {
    typedef skutils::multithreading::recursive_mutex_type mutex_type;

protected:
    // mutex_type traffic_stats_mtx_;
public:
    guarded_traffic_stats();
    guarded_traffic_stats( myrct x );
    guarded_traffic_stats( myrrt x );
    ~guarded_traffic_stats() override;
    void lock() override;
    void unlock() override;
};  // class guarded_traffic_stats

class security_args {
public:
    std::string strCertificateFile_;
    std::string strCertificationChainFile_;
    std::string strPrivateKeyFile_;
    security_args() = default;
    security_args( const security_args& ) = default;
    security_args( security_args&& ) = default;
    security_args& operator=( const security_args& ) = default;
    security_args& operator=( security_args&& ) = default;
    // 			void init_default_for_nma() {
    // 				strCertificateFile_ = "/opt/nma/certs/server.crt";
    // 				strCertificationChainFile_ = "/opt/nma/certs/server.ca";
    // 				strPrivateKeyFile_ = "/opt/nma/certs/server.key";
    // 			}
};  /// class security_args

class basic_network_settings {
public:
    uint64_t interval_ping_;                                       // seconds
    uint64_t timeout_pong_;                                        // seconds
    uint64_t timeout_handshake_open_, timeout_handshake_close_;    // seconds
    size_t max_message_size_, max_body_size_;                      // bytes
    uint64_t timeout_restart_on_close_, timeout_restart_on_fail_;  // seconds
    bool log_ws_rx_tx_;
#if ( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
    bool server_disable_ipv6_;         // use LWS_SERVER_OPTION_DISABLE_IPV6 option
    bool server_validate_utf8_;        // use LWS_SERVER_OPTION_VALIDATE_UTF8 option
    bool server_skip_canonical_name_;  // use LWS_SERVER_OPTION_SKIP_SERVER_CANONICAL_NAME option
    bool ssl_perform_global_init_;     // both client and server, use
                                       // LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT option
    bool
        ssl_server_require_valid_certificate_;  // use
                                                // LWS_SERVER_OPTION_REQUIRE_VALID_OPENSSL_CLIENT_CERT
                                                // option
    bool ssl_server_allow_non_ssl_on_ssl_port_;  // use LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT
                                                 // option
    bool
        ssl_server_accept_connections_without_valid_cert_;  // use
                                                            // LWS_SERVER_OPTION_PEER_CERT_NOT_REQUIRED
                                                            // option
    bool ssl_server_redirect_http_2_https_;  // use LWS_SERVER_OPTION_REDIRECT_HTTP_TO_HTTPS option
    bool ssl_client_allow_self_signed_;      // use LCCSCF_ALLOW_SELFSIGNED option
    bool ssl_client_skip_host_name_check_;   // use LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK option
    bool ssl_client_allow_expired_;          // use LCCSCF_ALLOW_EXPIRED option
    size_t cntMaxPortionBytesToSend_;
#endif  /// if( defined __skutils_WS_OFFER_DETAILED_NLWS_CONFIGURATION_OPTIONS__ )
    basic_network_settings();
    basic_network_settings( const basic_network_settings& ) = default;
    basic_network_settings( basic_network_settings&& ) = default;
    basic_network_settings& operator=( const basic_network_settings& ) = default;
    basic_network_settings& operator=( basic_network_settings&& ) = default;
    static basic_network_settings& default_instance();
    void bns_assign_from_default_instance();
};  /// class basic_network_settings

class generic_participant {
public:
    generic_participant();
    virtual ~generic_participant();
    virtual bool isServerSide() const = 0;
    virtual const security_args& onGetSecurityArgs() const = 0;
};  /// class generic_participant

class generic_sender : public skutils::ref_retain_release {
public:
    generic_sender();
    virtual ~generic_sender();
    virtual bool sendMessage( const std::string& msg, int nOpCode ) = 0;
    virtual bool sendMessageText( const std::string& msg ) = 0;
    virtual bool sendMessageBinary( const std::string& msg ) = 0;
    bool sendMessage( const std::string& msg );  // send as text
    bool sendMessage( const nlohmann::json& msg );
};  /// class generic_sender

typedef std::function< bool() > fn_continue_status_flag_t;

namespace utils {

extern bool is_json( const char* strProbablyJSON, nlohmann::json* p_jo = nullptr );
extern bool is_json( const std::string& strProbablyJSON, nlohmann::json* p_jo = nullptr );
extern bool is_json( const char* strProbablyJSON, const char* strMustHavePropertyName,
    const char* strMustHavePropertyValue );
extern bool is_json( const std::string& strProbablyJSON, const char* strMustHavePropertyName,
    const char* strMustHavePropertyValue );
extern bool is_json( const nlohmann::json& jo, const char* strMustHavePropertyName,
    const char* strMustHavePropertyValue );
extern bool is_ping( const char* strProbablyJSON );
extern bool is_ping( const std::string& strProbablyJSON );
extern bool is_ping( const nlohmann::json& jo );
extern bool is_pong( const char* strProbablyJSON );
extern bool is_pong( const std::string& strProbablyJSON );
extern bool is_pong( const nlohmann::json& jo );
extern bool is_ping_or_pong( const char* strProbablyJSON );
extern bool is_ping_or_pong( const std::string& strProbablyJSON );
extern bool is_ping_or_pong( const nlohmann::json& jo );

};  // namespace utils

namespace nlws {

typedef int connection_identifier_t;    // socket handle
typedef connection_identifier_t hdl_t;  // same as connection_identifier_t
enum opcv {
    continuation = 0x0,
    text = 0x1,
    binary = 0x2,
    rsv3 = 0x3,
    rsv4 = 0x4,
    rsv5 = 0x5,
    rsv6 = 0x6,
    rsv7 = 0x7,
    close = 0x8,
    ping = 0x9,
    pong = 0xA,
    control_rsvb = 0xB,
    control_rsvc = 0xC,
    control_rsvd = 0xD,
    control_rsve = 0xE,
    control_rsvf = 0xF,

    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    RSV3 = 0x3,
    RSV4 = 0x4,
    RSV5 = 0x5,
    RSV6 = 0x6,
    RSV7 = 0x7,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA,
    CONTROL_RSVB = 0xB,
    CONTROL_RSVC = 0xC,
    CONTROL_RSVD = 0xD,
    CONTROL_RSVE = 0xE,
    CONTROL_RSVF = 0xF
};  /// enum opcv

enum close_status {
    blank = 0,
    omit_handshake = 1,
    force_tcp_drop = 2,
    normal = 1000,
    going_away = 1001,
    protocol_error = 1002,
    unsupported_data = 1003,
    no_status = 1005,
    abnormal_close = 1006,
    invalid_payload = 1007,
    policy_violation = 1008,
    message_too_big = 1009,
    extension_required = 1010,
    internal_endpoint_error = 1011,
    service_restart = 1012,
    try_again_later = 1013,
    tls_handshake = 1015,
    subprotocol_error = 3000,
    invalid_subprotocol_data = 3001,
    rsv_start = 1016,
    rsv_end = 2999
};  // enum close_status

class basic_participant;
class basic_sender;
class basic_socket;
class peer;
class server;
class client;
typedef peer* peer_ptr_t;

typedef std::function< void( basic_socket& ) > onVoid_t;
typedef std::function< void( basic_socket&, const std::string& ) > onText_t;
typedef std::function< void( basic_socket&, e_ws_log_message_type_t eWSLMT, const std::string& ) >
    onLogMessage_t;
typedef std::function< void( basic_participant&, hdl_t, opcv eOpCode, const std::string& ) >
    onTextFrom_t;
typedef std::function< void( basic_socket&, hdl_t ) > onStateChanged_t;
typedef std::function< void( basic_socket&, hdl_t, const std::string& reason, int local_close_code,
    const std::string& local_close_code_as_str ) >
    onClose_t;
typedef std::function< bool( basic_socket&, hdl_t ) > onQueryHandling_t;

class message_payload_data {
private:
    uint8_t* pBuffer_;
    size_t cnt_;
    lws_write_protocol type_;  // LWS_WRITE_TEXT or LWS_WRITE_BINARY
    bool isContinuation_;

public:
    message_payload_data();
    message_payload_data( const message_payload_data& );
    message_payload_data( message_payload_data&& );
    message_payload_data& operator=( const message_payload_data& );
    message_payload_data& operator=( message_payload_data&& );
    ~message_payload_data();
    bool empty() const;
    void clear();
    bool isContinuation() const { return isContinuation_; }
    void fetchHeadPartAndMarkAsContinuation( size_t cntToFetch );
    static size_t pre();
    static size_t post();
    void free();
    uint8_t* alloc( size_t cnt );
    uint8_t* realloc( size_t cnt );
    void assign( const message_payload_data& );
    void move( message_payload_data& );
    lws_write_protocol type() const;
    size_t size() const;
    const uint8_t* data() const;
    uint8_t* data();
    uint8_t* set_text( const char* s, size_t cnt );
    uint8_t* set_text( const char* s );
    uint8_t* set_text();
    uint8_t* set_text( const std::string& s );
    uint8_t* set_binary( const uint8_t* pBinary, size_t cnt );
    uint8_t* set_binary( const std::string& s );
    uint8_t* set_binary();
    void store_to_string( std::string& s ) const;
    bool send_to_wsi( struct lws* wsi, size_t cntMaxPortionBytesToSend, size_t& cntLeft,
        std::string* pStrErrorDescription = nullptr ) const;
    bool append( const message_payload_data& other );
};  /// class message_payload_data

typedef std::list< message_payload_data > payload_queue_t;

class basic_api : public basic_network_settings {
public:
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;
    typedef std::function< void() > fn_lock_callback_t;
    // mutable mutex_type mtx_api_;
    mutex_type& mtx_api() const;
    std::atomic_bool initialized_;
    // std::atomic_size_t cntTryLockExecutes_;
    struct lws_context* ctx_;
    struct lws_context_creation_info ctx_info_;
    typedef std::vector< lws_protocols > vec_lws_protocols_t;
    vec_lws_protocols_t vec_lws_protocols_;
    size_t default_protocol_index_;
    static std::string g_strDefaultProtocolName;
    static volatile size_t g_nDefaultBufferSizeRX;
    static volatile size_t g_nDefaultBufferSizeTX;
    basic_api();
    basic_api( const basic_api& ) = delete;
    basic_api( basic_api&& ) = delete;
    basic_api& operator=( const basic_api& ) = delete;
    basic_api& operator=( basic_api&& ) = delete;
    virtual ~basic_api();
    void locked_execute( fn_lock_callback_t fn );
    //				bool try_locked_execute( fn_lock_callback_t fn, size_t cntAttemts = 10, uint64_t
    // nMillisecondsWaitBetweenAttempts = 20 );
    void clear_fields();
    void do_writable_callbacks_all_protocol();
    void configure_wsi( struct lws* wsi );
    static int stat_callback_http(
        struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len );
    static int stat_callback_server(
        struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len );
    static int stat_callback_client(
        struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len );
};  /// class basic_api

class client_api : public basic_api {
public:
    connection_identifier_t cid_;
    bool destroy_flag_;
    bool connection_flag_;
    bool writeable_flag_;
    payload_queue_t buffer_;  // ordered list of pending messages to flush out when socket is
                              // writable
    std::string delayed_close_reason_;
    int delayed_close_status_ = 0;
    int64_t delayed_adjustment_pong_timeout_ =
        -1;  // -1 for no adjustment, otherwise change pong timeout
    volatile bool delayed_de_init_;
    //
    std::string strURL_;
    std::thread clientThread_;
    std::atomic_bool clientThreadStopFlag_ = false, clientThreadWasStopped_ = true;
    struct lws* wsi_;
    int ssl_flags_;
    std::string cert_path_, key_path_, ca_path_;
    //
    message_payload_data accumulator_;
    //
    client_api();
    client_api( const client_api& ) = delete;
    client_api( client_api&& ) = delete;
    client_api& operator=( const client_api& ) = delete;
    client_api& operator=( client_api&& ) = delete;
    virtual ~client_api();
    void clear_fields();
    bool init( const std::string& strURL, security_args* pSA );
    bool init( bool isSSL, const std::string& strHost, int nPort, const std::string& strPath,
        security_args* pSA );
    void deinit();
    void close( int nCloseStatus, const std::string& msg );

    typedef std::function< void() > onConnect_t;
    typedef std::function< void( const std::string& strMessage ) > onDisconnect_t;
    typedef std::function< void( const message_payload_data& data ) > onMessage_t;
    typedef std::function< void( const std::string& strMessage ) > onFail_t;
    typedef std::function< void( e_ws_log_message_type_t eWSLMT, const std::string& strMessage ) >
        onLogMessage_t;
    typedef std::function< void() > onDelayDeinit_t;
    onConnect_t onConnect_;
    onDisconnect_t onDisconnect_;
    onMessage_t onMessage_;
    onFail_t onFail_;
    onLogMessage_t onLogMessage_;
    onDelayDeinit_t onDelayDeinit_;
    virtual void onConnect();
    virtual void onDisconnect( const std::string& strMessage );
    virtual void onMessage( const message_payload_data& data, bool isFinalFragment );
    virtual void onFail( const std::string& strMessage );
    virtual void onLogMessage( e_ws_log_message_type_t eWSLMT, const std::string& strMessage );

    bool send( const message_payload_data& data );

private:
    typedef std::map< void*, client_api* > map_ctx_2_inst_t;
    typedef std::map< client_api*, void* > map_inst_2_ctx_t;
    static map_ctx_2_inst_t g_ctx_2_inst;
    static map_inst_2_ctx_t g_inst_2_ctx;
    static mutex_type g_ctx_reg_mtx;
    static void stat_reg( client_api* api );
    static void stat_unreg( client_api* api );

public:
    static client_api* stat_get( void* ctx );
    void delay_deinit();
};  /// class client_api

enum class srvmode_t {
    srvmode_simple,
    srvmode_external_poll,
#if ( defined LWS_WITH_LIBEV )
    srvmode_ev,
#endif  // (defined LWS_WITH_LIBEV)
#if ( defined LWS_WITH_LIBEVENT )
    srvmode_event,
#endif  // (defined LWS_WITH_LIBEVENT)
#if ( defined LWS_WITH_LIBUV )
    srvmode_uv
#endif  // (defined LWS_WITH_LIBUV)
};      // enum class srvmode_t
extern srvmode_t g_default_srvmode;
extern bool g_default_explicit_vhost_enable;  // srvmode_simple and srvmode_external_poll only
extern bool g_default_dynamic_vhost_enable;   // srvmode_external_poll only
extern srvmode_t str2srvmode( const std::string& s );
extern std::string srvmode2str( srvmode_t m );
extern std::string list_srvmodes_as_str();

class server_api : public basic_api {
public:
    srvmode_t srvmode_;
    bool explicit_vhost_enable_;
    //
    size_t external_poll_max_elements_;
    struct lws_pollfd* external_poll_fds_;
    int* external_poll_fd_lookup_;
    /*nfds_t*/ size_t external_poll_cnt_fds_;
    struct timeval external_poll_tv_;
    int external_poll_ms_1_second_;
    unsigned int external_poll_ms_, external_poll_oldms_;
    bool dynamic_vhost_enable_;
    struct lws_vhost* dynamic_vhost_;
    //
    //
    // struct lws_plat_file_ops fops_plat_;
    //
    struct lws_vhost* vhost_;
    //				std::string interface_name_;
    //				const char * iface_;
    std::string cert_path_, key_path_, ca_path_;
    bool use_ssl_;
    //
    std::atomic_bool serverInterruptFlag_;
    typedef std::function< void() > fn_internal_interrupt_action_t;
    fn_internal_interrupt_action_t fn_internal_interrupt_action_;
    //
    unsigned int nHttpStatusToReturn_;
    std::string strHttpBodyToReturn_;

#if ( defined LWS_WITH_LIBUV )
    std::unique_ptr< uv_loop_t > pUvLoop_ = nullptr;
    void* foreign_loops_[1];
#endif  // (defined LWS_WITH_LIBUV)

    // represents a client connection
    class connection_data {
    public:
        connection_identifier_t cid_ = 0;
        size_t sn_ = 0;
        struct lws* wsi_ = nullptr;
        payload_queue_t buffer_;  // ordered list of pending messages to flush out when socket is
                                  // writable
        std::string delayed_close_reason_;
        int delayed_close_status_ = 0;
        int64_t delayed_adjustment_pong_timeout_ =
            -1;  // -1 for no adjustment, otherwise change pong timeout
        std::map< std::string, std::string > keyValueMap_;
        time_t creationTime_ = 0;

    protected:
        peer_ptr_t pPeer_ = nullptr;

    public:
        std::string strPeerClientAddressName_, strPeerRemoteIP_;
        message_payload_data accumulator_;
        connection_data();
        virtual ~connection_data();
        peer_ptr_t getPeer();
        void setPeer( peer_ptr_t pPeer );
        std::string unique_string_identifier( bool isColored = false ) const;
        std::string description( bool isColored = false ) const;
    };  /// struct connection_data

    // manages connections. Unfortunately this is public because static callback for
    // libwebsockets is defined outside the instance and needs access to it.
    typedef std::map< int, connection_data* > map_connections_t;
    map_connections_t connections_;

    server_api();
    server_api( const server_api& ) = delete;
    server_api( server_api&& ) = delete;
    server_api& operator=( const server_api& ) = delete;
    server_api& operator=( server_api&& ) = delete;
    virtual ~server_api();
    void clear_fields();
    bool init( bool isSSL, int nPort, security_args* pSA );
    void deinit();
    void close( connection_identifier_t cid, int nCloseStatus, const std::string& msg );

    bool service_mode_supported() const;
    void service_interrupt();
    void service( fn_continue_status_flag_t fnContinueStatusFlag );

    void external_poll_service_loop(
        fn_continue_status_flag_t fnContinueStatusFlag, size_t nServiceLoopRunLimit = 0 );
    bool service_poll( int& n );  // if returns true - continue service loop
    void poll( fn_continue_status_flag_t fnContinueStatusFlag );

    void run( uint64_t timeout = 50 );
    void wait( uint64_t timeout = 50 );
    bool send( connection_identifier_t cid, const message_payload_data& data );
    // size_t broadcast( const message_payload_data & data ); // ugly simple, we do not need it)

    std::string getPeerClientAddressName( connection_identifier_t cid );  // notice: used as
                                                                          // "origin"
    std::string getPeerRemoteIP( connection_identifier_t cid );  // notice: no port is returned
    peer_ptr_t getPeer( connection_identifier_t cid );
    peer_ptr_t detachPeer( connection_identifier_t cid );
    bool setPeer( connection_identifier_t cid, peer_ptr_t pPeer );

    int64_t delayed_adjustment_pong_timeout( connection_identifier_t cid ) const;  // NLWS-specific
    bool delayed_adjustment_pong_timeout(
        connection_identifier_t cid, int64_t to );  // NLWS-specific

    // key->value storage for each connection
    std::string getValue( connection_identifier_t cid, const std::string& strName );
    bool setValue(
        connection_identifier_t cid, const std::string& strName, const std::string& strValue );

    int getNumberOfConnections();

    typedef std::function< void( connection_identifier_t cid, struct lws* wsi,
        const char* strPeerClientAddressName, const char* strPeerRemoteIP ) >
        onConnect_t;
    typedef std::function< void( connection_identifier_t cid, const std::string& strMessage ) >
        onDisconnect_t;
    typedef std::function< void( connection_identifier_t cid, const message_payload_data& data ) >
        onMessage_t;
    typedef std::function< void( connection_identifier_t cid ) > onHttp_t;
    typedef std::function< void( connection_identifier_t cid, const std::string& strMessage ) >
        onFail_t;
    typedef std::function< void( e_ws_log_message_type_t eWSLMT, const std::string& strMessage ) >
        onLogMessage_t;
    onConnect_t onConnect_;
    onDisconnect_t onDisconnect_;
    onMessage_t onMessage_;
    onHttp_t onHttp_;
    onFail_t onFail_;
    onLogMessage_t onLogMessage_;
    virtual void onConnect( connection_identifier_t cid, struct lws* wsi,
        const char* strPeerClientAddressName, const char* strPeerRemoteIP );
    virtual void onDisconnect( connection_identifier_t cid, const std::string& strMessage );
    virtual void onMessage(
        connection_identifier_t cid, const message_payload_data& data, bool isFinalFragment );
    virtual void onHttp( connection_identifier_t cid );
    virtual void onFail( connection_identifier_t cid, const std::string& strMessage );
    virtual void onLogMessage( e_ws_log_message_type_t eWSLMT, const std::string& strMessage );

private:
    bool impl_eraseConnection( connection_identifier_t cid );
    bool impl_removeConnection( connection_identifier_t cid );

private:
    typedef std::map< void*, server_api* > map_ctx_2_inst_t;
    typedef std::map< server_api*, void* > map_inst_2_ctx_t;
    static map_ctx_2_inst_t g_ctx_2_inst;
    static map_inst_2_ctx_t g_inst_2_ctx;
    static mutex_type g_ctx_reg_mtx;
    static void stat_reg( server_api* api );
    static void stat_unreg( server_api* api );
    //
    typedef std::map< void*, server_api* > map_ptr_2_inst_t;
    static mutex_type g_ptr_reg_mtx;
    static map_ptr_2_inst_t g_ptr_2_inst;

public:
    //
    static server_api* stat_get( void* ctx );
    //
    static void stat_ptr_reg( void* p, server_api* api );
    static void stat_ptr_unreg( void* p );
    static server_api* stat_ptr2api( void* p );
};  /// class server_api


class basic_participant : public generic_participant {
public:
    basic_participant();
    virtual ~basic_participant();
    static std::string stat_backend_name();
    virtual nlohmann::json toJSON( bool bSkipEmptyStats = true ) const;
};  /// class basic_participant

class basic_sender : public basic_participant, public generic_sender {
public:
    basic_sender();
    virtual ~basic_sender();
    virtual bool sendMessage( const std::string& msg, opcv eOpCode = opcv::text ) = 0;
    bool sendMessage( const std::string& msg, int nOpCode ) override {
        return sendMessage( msg, opcv( nOpCode ) );
    }
    bool sendMessageText( const std::string& msg ) override {
        return sendMessage( msg, opcv::text );
    }
    bool sendMessageBinary( const std::string& msg ) override {
        return sendMessage( msg, opcv::binary );
    }
    nlohmann::json toJSON( bool bSkipEmptyStats = true ) const override;
};  /// class basic_socket

class basic_socket : public basic_participant, public basic_network_settings {
public:
    onStateChanged_t onOpen_;
    onClose_t onClose_;
    onStateChanged_t onFail_;
    onTextFrom_t onMessage_;
    bool bLogToStdCerr_ = false;
    onLogMessage_t onLogMessage_;
    basic_socket();
    basic_socket( const basic_socket& ) = delete;
    basic_socket( basic_socket&& ) = delete;
    basic_socket& operator=( const basic_socket& ) = delete;
    basic_socket& operator=( basic_socket&& ) = delete;
    virtual ~basic_socket();
    virtual void close();
    virtual void cancel();
    virtual void pause_reading();
    virtual void onOpen( hdl_t hdl );
    virtual void onClose( hdl_t hdl, const std::string& reason, int local_close_code,
        const std::string& local_close_code_as_str );
    virtual void onFail( hdl_t hdl );
    virtual void onMessage( hdl_t hdl, opcv eOpCode, const std::string& msg );
    // virtual void onStreamSocketInit( int native_fd );
    virtual void onLogMessage( e_ws_log_message_type_t eWSLMT, const std::string& msg );
    nlohmann::json toJSON( bool bSkipEmptyStats = true ) const override;
};  /// class basic_socket

typedef std::function< void( peer& aPeer, const std::string& msg, opcv eOpCode ) > onPeerMessage_t;
typedef std::function< void( peer& aPeer, const std::string& reason, int local_close_code,
    const std::string& local_close_code_as_str ) >
    onPeerClose_t;
typedef std::function< void( peer& aPeer ) > onPeerFail_t;

class peer : public basic_sender, public guarded_traffic_stats {
public:
    onPeerMessage_t onPeerMessage_;
    onPeerClose_t onPeerClose_;
    onPeerFail_t onPeerFail_;
    peer() = delete;
    peer( const peer& ) = delete;
    peer( peer&& ) = delete;
    peer& operator=( const peer& ) = delete;
    peer& operator=( peer&& ) = delete;
    peer( server& srv, const hdl_t& hdl );
    virtual ~peer();
    virtual std::string getShortTypeDescrition( bool isColored = false ) const;
    virtual std::string getShortPeerDescription(
        bool isColored = false, bool isLifetime = true, bool isTrafficStats = true ) const;
    virtual void onPeerRegister();
    virtual void onPeerUnregister();  // peer will no longer receive onMessage after call to this
    bool isServerSide() const override;
    connection_identifier_t cid() const;
    virtual void async_close( const std::string& msg,
        int nCloseStatus = int( close_status::going_away ), size_t nTimeoutMilliseconds = 0 );
    virtual void close(
        const std::string& msg, int nCloseStatus = int( close_status::going_away ) );
    virtual void cancel();
    virtual void pause_reading();
    virtual void onMessage( const std::string& msg, opcv eOpCode );
    virtual void onClose( const std::string& reason, int local_close_code,
        const std::string& local_close_code_as_str );
    virtual void onFail();
    bool sendMessage( const std::string& msg, opcv eOpCode = opcv::text ) override;
    virtual void onLogMessage( e_ws_log_message_type_t eWSLMT, const std::string& msg );
    const security_args& onGetSecurityArgs() const override;
    virtual std::string getRemoteIp() const;
    virtual std::string getOrigin() const;
    virtual std::string getCidString() const;
    static connection_identifier_t stat_getCid( const hdl_t& hdl );
    // static std::string stat_getCidString( const hdl_t & hdl ); // hdl_t is same as
    // connection_identifier_t
    static std::string stat_getCidString( const connection_identifier_t& cid );
    static connection_identifier_t stat_parseCid( const std::string& strCid );
    static connection_identifier_t stat_parseCid( const char* strCid );
    virtual bool isConnected() const noexcept;
    size_t serial_number() const { return peer_serial_number_; }
    std::string unique_string_identifier( bool isColored = false ) const;
    int64_t delayed_adjustment_pong_timeout() const;     // NLWS-specific
    void delayed_adjustment_pong_timeout( int64_t to );  // NLWS-specific
protected:
    mutable volatile bool opened_ = false;
    server& srv_;
    size_t peer_serial_number_;
    hdl_t hdl_;
    mutable connection_identifier_t cid_;
    mutable volatile bool was_disconnected_;

public:
    server& srv() { return srv_; }
    const server& srv() const { return srv_; }
    hdl_t hdl() { return hdl_; }
    //
    nlohmann::json toJSON( bool bSkipEmptyStats = true ) const override;
    //
    friend class server;
};  /// class peer

typedef std::function< peer_ptr_t( server& srv, hdl_t hdl ) > onPeerInstantiate_t;
typedef std::function< void( peer_ptr_t& pPeer ) > onPeerEvent_t;

class server : public basic_socket, public security_args, public guarded_traffic_stats {
    server_api api_;
    size_t server_serial_number_;

public:
    bool reuse_addr_;
    int listen_backlog_;
    std::string last_scheme_cached_;
    onPeerInstantiate_t onPeerInstantiate_;
    onPeerEvent_t onPeerRegister_;
    onPeerEvent_t onPeerUnregister_;
    server();
    server( const server& ) = delete;
    server( server&& ) = delete;
    server& operator=( const server& ) = delete;
    server& operator=( server&& ) = delete;
    virtual ~server();
    size_t request_new_peer_serial_number();
    bool isServerSide() const override;
    std::string type() const;
    int port() const;
    int defaultPort() const;
    bool open( const std::string& scheme, int nPort );
    void close() override;
    void close( hdl_t hdl, int nCloseStatus, const std::string& msg );
    void cancel_hdl( hdl_t hdl );
    void pause_reading_hdl( hdl_t hdl );
    void poll( fn_continue_status_flag_t fnContinueStatusFlag );
    void reset();

    bool service_mode_supported() const;
    void service_interrupt();
    void service( fn_continue_status_flag_t fnContinueStatusFlag );

    std::string getRemoteIp( hdl_t hdl );
    std::string getOrigin( hdl_t hdl );
    bool sendMessage( hdl_t hdl, const std::string& msg, opcv eOpCode = opcv::text );
    //
    virtual peer_ptr_t onPeerInstantiate( hdl_t hdl );
    peer_ptr_t getPeer( hdl_t hdl );
    peer_ptr_t detachPeer( hdl_t hdl );
    virtual bool onPeerRegister( peer_ptr_t pPeer );
    virtual bool onPeerUnregister( peer_ptr_t pPeer );
    int64_t delayed_adjustment_pong_timeout( connection_identifier_t cid ) const;  // NLWS-specific
    void delayed_adjustment_pong_timeout(
        connection_identifier_t cid, int64_t to );  // NLWS-specific
    //
    // void onStreamSocketInit( int native_fd ) override;
    void onOpen( hdl_t hdl ) override;
    void onClose( hdl_t hdl, const std::string& reason, int local_close_code,
        const std::string& local_close_code_as_str ) override;
    void onFail( hdl_t hdl ) override;
    virtual bool onHttp( hdl_t hdl );
    void onMessage( hdl_t hdl, opcv eOpCode, const std::string& msg ) override;
    void onLogMessage( e_ws_log_message_type_t eWSLMT, const std::string& msg ) override;
    const security_args& onGetSecurityArgs() const override;

public:
    onQueryHandling_t onHttp_;
    nlohmann::json toJSON( bool bSkipEmptyStats = true ) const override;
};  /// class server

class client : public basic_socket,
               public basic_sender,
               public security_args,
               public guarded_traffic_stats {
    client_api api_;
    skutils::async::timer<> restart_timer_;
    bool isRestartTimerEnabled_ = true;
    std::string strLastURI_;

public:
    client();
    client( const client& ) = delete;
    client( client&& ) = delete;
    client& operator=( const client& ) = delete;
    client& operator=( client&& ) = delete;
    virtual ~client();
    bool isServerSide() const override;
    std::string type() const;
    std::string uri() const;
    virtual bool open( const std::string& uri );
    virtual bool openLocalHost( int nPort );
    void close() override;
    void resetConnection();
    void cancel() override;
    void pause_reading() override;
    virtual void async_close( const std::string& msg,
        int nCloseStatus = int( close_status::going_away ), size_t nTimeoutMilliseconds = 0 );
    virtual void close(
        const std::string& msg, int nCloseStatus = int( close_status::going_away ) );
    virtual bool isConnected() const noexcept;
    virtual void setConnected( bool state );
    bool sendMessage( const std::string& msg, opcv eOpCode = opcv::text ) override;
    void onMessage( hdl_t hdl, opcv eOpCode, const std::string& msg ) override;
    virtual void onDisconnected();
    void onOpen( hdl_t hdl ) override;
    void onClose( hdl_t hdl, const std::string& reason, int local_close_code,
        const std::string& local_close_code_as_str ) override;
    void onFail( hdl_t hdl ) override;
    virtual void onDelayDeinit();  // NLWS-specific
    // void onStreamSocketInit( int native_fd ) override;
    void onLogMessage( e_ws_log_message_type_t eWSLMT, const std::string& msg ) override;
    const security_args& onGetSecurityArgs() const override;
    virtual bool isRestartTimerEnabled() const;
    virtual void enableRestartTimer( bool isEnabled );
    onVoid_t onDisconnected_;
    int64_t delayed_adjustment_pong_timeout() const;     // NLWS-specific
    void delayed_adjustment_pong_timeout( int64_t to );  // NLWS-specific
protected:
    void impl_ensure_restart_timer_is_running();

public:
    nlohmann::json toJSON( bool bSkipEmptyStats = true ) const override;
};  /// class client

};  // namespace nlws

#define __DO_DECLARE_skutils_TYPES_NLWS__                          \
    typedef nlws::connection_identifier_t connection_identifier_t; \
    typedef nlws::hdl_t hdl_t;                                     \
    typedef nlws::opcv opcv;                                       \
    typedef nlws::close_status close_status;                       \
    typedef nlws::basic_participant basic_participant;             \
    typedef nlws::basic_sender basic_sender;                       \
    typedef nlws::basic_socket basic_socket;                       \
    typedef nlws::server server;                                   \
    typedef nlws::peer peer;                                       \
    typedef nlws::peer_ptr_t peer_ptr_t;                           \
    typedef nlws::client client;

//		namespace client {
//			__DO_DECLARE_skutils_TYPES_NLWS__
//		}; /// namespace client

//		namespace server {
//			__DO_DECLARE_skutils_TYPES_NLWS__
//		}; /// namespace server

__DO_DECLARE_skutils_TYPES_NLWS__

};  // namespace ws
};  // namespace skutils

#endif  // (!defined __SKUTILS_WS_H)
