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
    std::string s_;
    std::string content_type_;
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

class client {
private:
    skutils::url u_;
    std::unique_ptr< skutils::http::client > ch_;
    std::unique_ptr< skutils::ws::client > cw_;

private:
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;
    mutable mutex_type mtxData_;
    typedef std::list< data_t > data_list_t;
    data_list_t lstData_;

public:
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
        size_t cntSteps = 1000 );
    data_t call( const std::string& strJsonIn, bool isAutoGenJsonID = true,
        e_data_fetch_strategy edfs = e_data_fetch_strategy::edfs_default,
        std::chrono::milliseconds wait_step = std::chrono::milliseconds( 20 ),
        size_t cntSteps = 1000 );

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
