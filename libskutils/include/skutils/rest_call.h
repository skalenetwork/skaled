#if ( !defined __SKUTILS_REST_CALLS_H )
#define __SKUTILS_REST_CALLS_H 1

#include <stdint.h>
#include <strings.h>
#include <functional>
#include <memory>
#include <string>

#include <chrono>
#include <mutex>

#include <skutils/url.h>

#include <skutils/http.h>
#include <skutils/ws.h>

#include <skutils/dispatch.h>
#include <skutils/multithreading.h>

//#include <nlohmann/json.hpp>
#include <json.hpp>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <zmq.hpp>

#include <jsonrpccpp/client.h>
#include <thirdparty/zguide/zhelpers.hpp>

#define __SKUTIS_REST_USE_CURL_FOR_HTTP 1

namespace skutils {
namespace rest {

enum class e_data_fetch_strategy {
    edfs_by_equal_json_id,
    edfs_nearest_arrived,
    edfs_nearest_binary,
    edfs_nearest_json,
    edfs_nearest_text,
    edfs_default = e_data_fetch_strategy::edfs_nearest_arrived
};  /// enum class e_data_fetch_strategy

struct data_t {
    std::string s_, err_s_;
    std::string content_type_;
    skutils::http::common_network_exception::error_info ei_;
    data_t();
    data_t( const data_t& d );
    ~data_t();
    data_t& operator=( const data_t& d );
    bool is_json() const;
    bool is_binary() const;
    bool empty() const;
    void clear();
    void assign( const data_t& d );
    nlohmann::json extract_json() const;
    std::string extract_json_id() const;
};  /// struct data_t

typedef std::function< void( nlohmann::json& joIn, const data_t& dataOut ) >
    fn_async_call_data_handler_t;
typedef std::function< void( nlohmann::json& joIn, const std::string& strError ) >
    fn_async_call_error_handler_t;

struct await_t {
    std::string strCallID;
    nlohmann::json joIn;
    std::chrono::milliseconds wait_timeout;
    skutils::dispatch::job_id_t idTimeoutNotificationJob;
    fn_async_call_data_handler_t onData;
    fn_async_call_error_handler_t onError;
};  // struct await_t

class sz_cli {
public:
    skutils::url u_;
    skutils::http::SSL_client_options optsSSL_;
    size_t cntAttemptsToSendMessage_ = 10;
    size_t timeoutMilliseconds_ = 10 * 1000;

private:
    std::string cert_;
    std::string key_;
    EVP_PKEY* pKeyPrivate_ = nullptr;
    EVP_PKEY* pKeyPublic_ = nullptr;
    X509* x509Cert_ = nullptr;
    zmq::context_t zmq_ctx_;
    skutils::url u2_;
    bool isConnected_ = false;
    std::shared_ptr< zmq::socket_t > pClientSocket_;
    std::recursive_mutex mtx_;
    static std::string stat_f2s( const std::string& strFileName );
    static std::pair< EVP_PKEY*, X509* > stat_cert_2_public_key(
        const std::string& strCertificate );
    nlohmann::json stat_sendMessage( nlohmann::json& joRequest, bool bExceptionOnTimeout,
        size_t cntAttempts, size_t timeoutMilliseconds );
    std::string stat_sendMessageZMQ( std::string& _req, bool bExceptionOnTimeout,
        size_t cntAttempts, size_t timeoutMilliseconds );
    static std::string stat_a2h( const uint8_t* ptr, size_t cnt );
    static std::string stat_sign( EVP_PKEY* pKey, const std::string& s );

public:
    sz_cli();
    sz_cli( const skutils::url& u, const skutils::http::SSL_client_options& optsSSL );
    virtual ~sz_cli();
    void reconnect();
    bool is_sign() const;
    bool is_ssl() const;
    bool isConnected() const;
    void close();
    bool sendMessage( const std::string& strMessage, std::string& strAnswer );
};  /// class sz_cli

class client {
private:
    skutils::url u_;
#if (defined __SKUTIS_REST_USE_CURL_FOR_HTTP)
    std::unique_ptr< skutils::http_curl::client > ch_;
#else  // (defined __SKUTIS_REST_USE_CURL_FOR_HTTP)
    std::unique_ptr< skutils::http::client > ch_;
#endif // else from (defined __SKUTIS_REST_USE_CURL_FOR_HTTP)
    std::unique_ptr< skutils::ws::client > cw_;
    std::unique_ptr< sz_cli > cz_;

private:
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;
    mutable mutex_type mtxData_;
    typedef std::list< data_t > data_list_t;
    data_list_t lstData_;

public:
    skutils::http::SSL_client_options optsSSL_;
    bool isVerboseInsideNetworkLayer_ = false;
    client();
    client( const skutils::url& u );
    client( const std::string& url_str );
    client( const char* url_str );
    virtual ~client();

    const skutils::url& url() const { return u_; }

    bool open( const skutils::url& u,
        std::chrono::milliseconds wait_step = std::chrono::milliseconds( 20 ),
        size_t cntSteps = 1000 );
    bool open( const std::string& url_str,
        std::chrono::milliseconds wait_step = std::chrono::milliseconds( 20 ),
        size_t cntSteps = 1000 );
    bool open( const char* url_str,
        std::chrono::milliseconds wait_step = std::chrono::milliseconds( 20 ),
        size_t cntSteps = 1000 );
    void close();
    std::string get_connected_url_scheme() const;
    bool is_open() const;

private:
    std::string u_path() const;
    std::string u_path_and_args() const;
    bool handle_data_arrived( const data_t& d );
    data_t fetch_data_with_strategy(
        e_data_fetch_strategy edfs = e_data_fetch_strategy::edfs_default,
        const std::string id = "" );
    static const char g_str_default_content_type[];
    static std::string stat_extract_short_content_type_string( const std::string& s );
    static bool stat_auto_gen_json_id( nlohmann::json& jo );
    static uint64_t stat_get_random_number( uint64_t const& min, uint64_t const& max );
    static uint64_t stat_get_random_number();

public:
    data_t call( const nlohmann::json& joIn, bool isAutoGenJsonID = true,
        e_data_fetch_strategy edfs = e_data_fetch_strategy::edfs_default,
        std::chrono::milliseconds wait_step = std::chrono::milliseconds( 20 ),
        size_t cntSteps = 1000, bool isReturnErrorResponse = false );
    data_t call( const std::string& strJsonIn, bool isAutoGenJsonID = true,
        e_data_fetch_strategy edfs = e_data_fetch_strategy::edfs_default,
        std::chrono::milliseconds wait_step = std::chrono::milliseconds( 20 ),
        size_t cntSteps = 1000, bool isReturnErrorResponse = false );

private:
    typedef std::map< std::string, await_t > map_await_t;
    mutable map_await_t map_await_;
    skutils::dispatch::queue_id_t async_get_dispatch_queue_id( const std::string& strCallID ) const;
    await_t async_get( const std::string& strCallID ) const;
    void async_add( await_t& a );
    void async_remove_impl( await_t& a );
    bool async_remove_by_call_id( const std::string& strCallID );
    void async_remove_all();

public:
    void async_call( const nlohmann::json& joIn, fn_async_call_data_handler_t onData,
        fn_async_call_error_handler_t onError, bool isAutoGenJsonID = true,
        e_data_fetch_strategy edfs = e_data_fetch_strategy::edfs_default,
        std::chrono::milliseconds wait_timeout = std::chrono::milliseconds( 20000 ) );
    void async_call( const std::string& strJsonIn, fn_async_call_data_handler_t onData,
        fn_async_call_error_handler_t onError, bool isAutoGenJsonID = true,
        e_data_fetch_strategy edfs = e_data_fetch_strategy::edfs_default,
        std::chrono::milliseconds wait_timeout = std::chrono::milliseconds( 20000 ) );

};  /// class client

};  // namespace rest
};  // namespace skutils

#endif  /// (!defined __SKUTILS_REST_CALLS_H)
