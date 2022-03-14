// based on https://github.com/yhirose/cpp-httplib

#if ( !defined SKUTILS_HTTP_H )
#define SKUTILS_HTTP_H 1

#if ( defined _WIN32 )

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif  //_CRT_SECURE_NO_WARNINGS

#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif  //_CRT_NONSTDC_NO_DEPRECATE

#if defined( _MSC_VER ) && _MSC_VER < 1900
#define snprintf _snprintf_s
#endif  // _MSC_VER

#ifndef S_ISREG
#define S_ISREG( m ) ( ( ( m ) &S_IFREG ) == S_IFREG )
#endif  // S_ISREG

#ifndef S_ISDIR
#define S_ISDIR( m ) ( ( ( m ) &S_IFDIR ) == S_IFDIR )
#endif  // S_ISDIR

#ifndef NOMINMAX
#define NOMINMAX
#endif  // NOMINMAX

#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment( lib, "ws2_32.lib" )

#ifndef strcasecmp
#define strcasecmp _stricmp
#endif  // strcasecmp

typedef SOCKET socket_t;

#else  // (defined _WIN32)

#include <cstring>

typedef int socket_t;
#define INVALID_SOCKET ( -1 )

#endif  // else from (defined _WIN32)

#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include <openssl/err.h>
#include <openssl/ssl.h>

#ifdef __SKUTILS_HTTP_WITH_ZLIB_SUPPORT__
#include <zlib.h>
#endif

#include <libdevcore/microprofile.h>

#include <skutils/atomic_shared_ptr.h>
#include <skutils/dispatch.h>
#include <skutils/multithreading.h>
#include <skutils/network.h>
#include <skutils/url.h>
#include <skutils/utils.h>

//#include <nlohmann/json.hpp>
#include <json.hpp>

/// configuration

#define __SKUTILS_HTTP_ACCEPT_WAIT_MILLISECONDS__ ( 5 * 1000 )
#define __SKUTILS_HTTP_KEEPALIVE_TIMEOUT_MILLISECONDS__ ( 30 * 1000 )
#define __SKUTILS_HTTP_KEEPALIVE_MAX_COUNT__ ( 5 )

#define __SKUTILS_ASYNC_HTTP_POLL_TIMEOUT_MILLISECONDS__ ( 10 )
#define __SKUTILS_ASYNC_HTTP_FIRST_TIMEOUT_MILLISECONDS__ ( 20 )
#define __SKUTILS_ASYNC_HTTP_NEXT_TIMEOUT_MILLISECONDS__ ( 100 )
#define __SKUTILS_ASYNC_HTTP_RETRY_COUNT__ ( 10 * 30 )
// above: multiplier 10 makes about 1 second(for 100 milliseconds of 2nd timeout), 30 is 30 seconds
// appripriately

#define __SKUTILS_HTTP_DEFAULT_MAX_PARALLEL_QUEUES_COUNT__ ( 16 )

#define __SKUTILS_HTTP_CLIENT_CONNECT_TIMEOUT_MILLISECONDS__ ( 60 * 1000 )

//#define #define __SKUTILS_HTTP_ENABLE_FILE_REQUEST_HANDLING 1

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace skutils {
namespace http {

namespace detail {

struct ci {
    bool operator()( const std::string& s1, const std::string& s2 ) const {
        return std::lexicographical_compare( s1.begin(), s1.end(), s2.begin(), s2.end(),
            []( char c1, char c2 ) { return ::tolower( c1 ) < ::tolower( c2 ); } );
    }
};  /// struct ci

};  // namespace detail

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::multimap< std::string, std::string, detail::ci > map_headers;

template < typename uint64_t, typename... Args >
std::pair< std::string, std::string > make_range_header( uint64_t value, Args... args );

typedef std::multimap< std::string, std::string > map_params;
typedef std::smatch match;
typedef std::function< bool( uint64_t current, uint64_t total ) > fn_progress;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct multipart_file {
    std::string filename_;
    std::string content_type_;
    size_t offset_ = 0;
    size_t length_ = 0;
};  /// struct multipart_file
typedef std::multimap< std::string, multipart_file > multipart_files;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct request {
    std::string origin_;
    std::string version_;
    std::string method_;
    std::string target_;
    std::string path_;
    map_headers headers_;
    std::string body_;
    map_params params_;
    multipart_files files_;
    match matches_;

    fn_progress progress_;

    request();
    ~request();

    bool has_header( const char* key ) const;
    std::string get_header_value( const char* key, size_t id = 0 ) const;
    size_t get_header_value_count( const char* key ) const;
    void set_header( const char* key, const char* val );

    bool has_param( const char* key ) const;
    std::string get_param_value( const char* key, size_t id = 0 ) const;
    size_t get_param_value_count( const char* key ) const;

    bool has_file( const char* key ) const;
    multipart_file get_file_value( const char* key ) const;
};  /// struct request

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct response {
    std::string version_;
    int status_ = -1;
    map_headers headers_;
    std::string body_;
    std::function< std::string( uint64_t offset ) > streamcb_;
    bool send_status_ = false;

    response();
    ~response();

    bool has_header( const char* key ) const;
    std::string get_header_value( const char* key, size_t id = 0 ) const;
    size_t get_header_value_count( const char* key ) const;
    void set_header( const char* key, const char* val );

    void set_redirect( const char* uri );
    void set_content( const char* s, size_t n, const char* content_type );
    void set_content( const std::string& s, const char* content_type );

};  /// struct response

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class stream {
public:
    stream();
    virtual ~stream();
    virtual int read( char* ptr, size_t size ) = 0;
    virtual int write( const char* ptr, size_t size1 ) = 0;
    virtual std::string get_remote_addr() const = 0;

    template < typename... Args >
    void write_format( const char* fmt, const Args&... args );
};  /// class stream

class socket_stream : public stream {
public:
    socket_stream( socket_t sock );
    virtual ~socket_stream();
    virtual int read( char* ptr, size_t size );
    virtual int write( const char* ptr, size_t size );
    virtual std::string get_remote_addr() const;

private:
    socket_t sock_;
};  /// class socket_stream

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class buffer_stream : public stream {
public:
    buffer_stream();
    virtual ~buffer_stream();
    virtual int read( char* ptr, size_t size );
    virtual int write( const char* ptr, size_t size );
    virtual std::string get_remote_addr() const;
    const std::string& get_buffer() const;

private:
    std::string buffer_;
};  /// class buffer_stream

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class server;

class async_query_handler : public skutils::ref_retain_release {
public:
    server& srv_;
    async_query_handler( server& srv );
    virtual ~async_query_handler();
    virtual void run() = 0;
    virtual bool step() = 0;  // returns true only if next step scheduled
    virtual void was_added();
    virtual void will_remove();
    bool remove_this_task();
};  /// class async_query_handler

typedef void* task_id_t;
typedef skutils::retain_release_ptr< async_query_handler > task_ptr_t;
typedef std::map< task_id_t, task_ptr_t > map_tasks_t;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class async_read_and_close_socket_base : public async_query_handler {
public:
    socket_t socket_;
    std::atomic_bool active_, have_socket_;
    skutils::dispatch::queue_id_t qid_;
    size_t poll_ms_, retry_index_, retry_count_, retry_after_ms_, retry_first_ms_;
    clock_t tpStep_ = ( ( clock_t ) 0 );

    typedef std::function< void( stream& strm, bool last_connection, bool& connection_close ) >
        callback_success_t;
    typedef std::function< void( const char* strErrorDescription ) > callback_fail_t;

    callback_success_t callback_success_;
    callback_fail_t callback_fail_;

    async_read_and_close_socket_base( server& srv, socket_t socket,
        size_t poll_ms = __SKUTILS_ASYNC_HTTP_POLL_TIMEOUT_MILLISECONDS__,
        size_t retry_count = __SKUTILS_ASYNC_HTTP_RETRY_COUNT__,
        size_t retry_after_ms = __SKUTILS_ASYNC_HTTP_FIRST_TIMEOUT_MILLISECONDS__,
        size_t retry_first_ms = __SKUTILS_ASYNC_HTTP_NEXT_TIMEOUT_MILLISECONDS__ );
    ~async_read_and_close_socket_base();
    void run() override;
    virtual bool schedule_check_clock();
    virtual void schedule_first_step();
    virtual void schedule_next_step();
    virtual void close_socket();
    void call_fail_handler( const char* strErrorDescription = nullptr, bool is_close_socket = true,
        bool is_remove_this_task = true );
};  /// class async_read_and_close_socket_base


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class async_read_and_close_socket : public async_read_and_close_socket_base {
public:
    async_read_and_close_socket( server& srv, socket_t socket,
        size_t poll_ms = __SKUTILS_ASYNC_HTTP_POLL_TIMEOUT_MILLISECONDS__,
        size_t retry_count = __SKUTILS_ASYNC_HTTP_RETRY_COUNT__,
        size_t retry_after_ms = __SKUTILS_ASYNC_HTTP_FIRST_TIMEOUT_MILLISECONDS__,
        size_t retry_first_ms = __SKUTILS_ASYNC_HTTP_NEXT_TIMEOUT_MILLISECONDS__ );
    ~async_read_and_close_socket() override;
    void run() override;
    bool step() override;  // returns true only if next step scheduled
    void was_added() override;
    void will_remove() override;
    void close_socket() override;
};  /// class async_read_and_close_socket

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class async_read_and_close_socket_SSL : public async_read_and_close_socket_base {
public:
    typedef std::function< int( SSL* ) > SSL_connect_or_accept_t;
    SSL_connect_or_accept_t SSL_connect_or_accept_;  // if not set then SSL_accept() is called
                                                     // instead

    typedef std::function< void() > setup_ssl_t;
    setup_ssl_t setup_ssl_;

    SSL_CTX* ctx_;
    SSL* ssl_;
    BIO* bio_;

    async_read_and_close_socket_SSL( server& srv, SSL_CTX* ctx, socket_t socket,
        size_t poll_ms = __SKUTILS_ASYNC_HTTP_POLL_TIMEOUT_MILLISECONDS__,
        size_t retry_count = __SKUTILS_ASYNC_HTTP_RETRY_COUNT__,
        size_t retry_after_ms = __SKUTILS_ASYNC_HTTP_FIRST_TIMEOUT_MILLISECONDS__,
        size_t retry_first_ms = __SKUTILS_ASYNC_HTTP_NEXT_TIMEOUT_MILLISECONDS__ );
    ~async_read_and_close_socket_SSL() override;
    void run() override;
    bool step() override;
    void was_added() override;
    void will_remove() override;
    void close_socket() override;
};  /// class async_read_and_close_socket_SSL

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class common_network_exception : public std::exception {
public:
    enum error_type {
        et_no_error = 0,
        et_unknown = 1,
        et_fatal = 2,
        et_ssl_fatal = 3,
        et_ssl_error = 4,
    };
    struct error_info {
        error_type et_ = et_no_error;
        std::string strError_;
        int ec_ = 0;
        void clear() {
            et_ = et_no_error;
            strError_.clear();
            ec_ = 0;
        }
    };
    error_info ei_;
    explicit common_network_exception( const char* strError ) {
        ei_.strError_ = strError;
        ei_.et_ = common_network_exception::error_type::et_unknown;
    }
    explicit common_network_exception( const std::string& strError ) {
        ei_.strError_ = strError;
        ei_.et_ = common_network_exception::error_type::et_unknown;
    }
    explicit common_network_exception( const error_info& ei ) { ei_ = ei; }
    virtual ~common_network_exception() throw() {}
    virtual const char* what() const throw() { return ei_.strError_.c_str(); }
};

class common {
public:
    mutable int ipVer_ = -1;  // not known before connect or listen
    common_network_exception::error_info eiLast_;
    common( int ipVer );
    virtual ~common();
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class server : public common {
public:
    mutable bool is_async_http_transfer_mode_;
    mutable int boundToPort_ = -1;
    typedef std::function< void( const request&, response& ) > Handler;
    typedef std::function< void( const request&, const response& ) > Logger;

    server( size_t a_max_handler_queues = __SKUTILS_HTTP_DEFAULT_MAX_PARALLEL_QUEUES_COUNT__,
        bool is_async_http_transfer_mode = true );

    virtual ~server();

    virtual bool is_valid() const;

    server& Get( const char* pattern, Handler handler );
    server& Post( const char* pattern, Handler handler );

    server& Put( const char* pattern, Handler handler );
    server& Patch( const char* pattern, Handler handler );
    server& Delete( const char* pattern, Handler handler );
    server& Options( const char* pattern, Handler handler );

#if ( defined __SKUTILS_HTTP_ENABLE_FILE_REQUEST_HANDLING )
    std::string base_dir_get() const;
    bool base_dir_set( const char* path );
#endif  // (defined __SKUTILS_HTTP_ENABLE_FILE_REQUEST_HANDLING)

    void set_error_handler( Handler handler );
    void set_logger( Logger logger );

    size_t get_keep_alive_max_count() const;
    void set_keep_alive_max_count( size_t cnt );

    int bind_to_any_port( int ipVer, const char* host, int socket_flags = 0,
        bool is_reuse_address = true, bool is_reuse_port = false );
    bool listen_after_bind();

    bool listen( int ipVer, const char* host, int port, int socket_flags = 0,
        bool is_reuse_address = true, bool is_reuse_port = false );

    bool is_running() const;
    void stop();
    virtual bool is_ssl() const { return false; }

protected:
    bool process_request(
        const std::string& origin, stream& strm, bool last_connection, bool& connection_close );

    std::atomic< size_t > keep_alive_max_count_;

    virtual socket_t create_server_socket( int ipVer, const char* host, int port, int socket_flags,
        bool is_reuse_address = true, bool is_reuse_port = false ) const;
    virtual int bind_internal( int ipVer, const char* host, int port, int socket_flags,
        bool is_reuse_address = true, bool is_reuse_port = false );

private:
    typedef std::vector< std::pair< std::regex, Handler > > Handlers;

    bool listen_internal();

    bool routing( request& req, response& res );
#if ( defined __SKUTILS_HTTP_ENABLE_FILE_REQUEST_HANDLING )
    bool handle_file_request( request& req, response& res );
#endif  // (defined __SKUTILS_HTTP_ENABLE_FILE_REQUEST_HANDLING)
    bool dispatch_request( request& req, response& res, Handlers& handlers );

    bool parse_request_line( const char* s, request& req );
    void write_response( stream& strm, bool last_connection, const request& req, response& res );

    virtual bool read_and_close_socket_sync( socket_t sock );
    virtual void read_and_close_socket_async( socket_t sock );

    std::atomic_bool is_in_loop_ = false;
    std::atomic_bool is_running_ = false;
    socket_t svr_sock_;
#if ( defined __SKUTILS_HTTP_ENABLE_FILE_REQUEST_HANDLING )
    std::string base_dir_;
#endif  // (defined __SKUTILS_HTTP_ENABLE_FILE_REQUEST_HANDLING)
    Handlers get_handlers_;
    Handlers post_handlers_;
    Handlers put_handlers_;
    Handlers patch_handlers_;
    Handlers delete_handlers_;
    Handlers options_handlers_;
    Handler error_handler_;
    Logger logger_;

protected:
    typedef skutils::multithreading::recursive_mutex_type tasks_mutex_type;
    typedef std::lock_guard< tasks_mutex_type > tasks_lock_type;
    tasks_mutex_type& tasks_mtx();
    map_tasks_t map_tasks_;
    bool have_running_tasks();
    std::atomic_bool suspend_adding_tasks_ = false;
    bool add_task( task_ptr_t pTask );
    bool remove_task( task_ptr_t pTask );
    void remove_all_tasks();

public:
    bool is_in_loop() const { return is_in_loop_; }
    void wait_while_in_loop() const {
        while ( is_in_loop() )
            std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
    }

protected:
    std::mutex queue_handlers_mutex_;
    size_t max_handler_queues_, current_handler_queue_;
    skutils::dispatch::queue_id_t handler_queue_id_at_index( size_t i ) const {
        skutils::dispatch::queue_id_t id;
        id = skutils::tools::format(
            "%s/%d/%d-%p-%zu", is_ssl() ? "https" : "http", ipVer_, boundToPort_, this, i );
        return id;
    }
    skutils::dispatch::queue_id_t next_handler_queue_id() {
        std::lock_guard< std::mutex > guard( queue_handlers_mutex_ );
        if ( current_handler_queue_ >= max_handler_queues_ )
            current_handler_queue_ = 0;
        skutils::dispatch::queue_id_t id = handler_queue_id_at_index( current_handler_queue_ );
        ++current_handler_queue_;
        return id;
    }

public:
    void close_all_handler_queues() {
        for ( size_t i = 0; i < max_handler_queues_; ++i ) {
            skutils::dispatch::remove( handler_queue_id_at_index( i ) );
        }
    }

    friend class async_query_handler;
    friend class async_read_and_close_socket_base;
    friend class async_read_and_close_socket;
    friend class async_read_and_close_socket_SSL;
};  /// class server

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class client : public common {
public:
    client( int ipVer,  // is not known before connect
        const char* host, int port = 80,
        int timeout_milliseconds = __SKUTILS_HTTP_CLIENT_CONNECT_TIMEOUT_MILLISECONDS__ );

    virtual ~client();

    virtual bool is_valid() const;
    virtual bool is_ssl() const { return false; }

    std::shared_ptr< response > Get(
        const char* path, fn_progress progress = nullptr, bool isReturnErrorResponse = false );
    std::shared_ptr< response > Get( const char* path, const map_headers& headers,
        fn_progress progress = nullptr, bool isReturnErrorResponse = false );

    std::shared_ptr< response > Head( const char* path, bool isReturnErrorResponse = false );
    std::shared_ptr< response > Head(
        const char* path, const map_headers& headers, bool isReturnErrorResponse = false );

    std::shared_ptr< response > Post( const char* path, const std::string& body,
        const char* content_type, bool isReturnErrorResponse = false );
    std::shared_ptr< response > Post( const char* path, const map_headers& headers,
        const std::string& body, const char* content_type, bool isReturnErrorResponse = false );

    std::shared_ptr< response > Post(
        const char* path, const map_params& params, bool isReturnErrorResponse = false );
    std::shared_ptr< response > Post( const char* path, const map_headers& headers,
        const map_params& params, bool isReturnErrorResponse = false );

    std::shared_ptr< response > Put( const char* path, const std::string& body,
        const char* content_type, bool isReturnErrorResponse = false );
    std::shared_ptr< response > Put( const char* path, const map_headers& headers,
        const std::string& body, const char* content_type, bool isReturnErrorResponse = false );

    std::shared_ptr< response > Patch( const char* path, const std::string& body,
        const char* content_type, bool isReturnErrorResponse = false );
    std::shared_ptr< response > Patch( const char* path, const map_headers& headers,
        const std::string& body, const char* content_type, bool isReturnErrorResponse = false );

    std::shared_ptr< response > Delete( const char* path, bool isReturnErrorResponse = false );
    std::shared_ptr< response > Delete(
        const char* path, const map_headers& headers, bool isReturnErrorResponse = false );

    std::shared_ptr< response > Options( const char* path, bool isReturnErrorResponse = false );
    std::shared_ptr< response > Options(
        const char* path, const map_headers& headers, bool isReturnErrorResponse = false );

    bool send( request& req, response& res );

protected:
    bool process_request( const std::string& origin, stream& strm, request& req, response& res,
        bool& connection_close );
    const std::string host_;
    const int port_;
    int timeout_milliseconds_;
    const std::string host_and_port_;

protected:
    socket_t create_client_socket( int ipVer, int socket_flags = 0, bool is_reuse_address = true,
        bool is_reuse_port = false ) const;

private:
    bool read_response_line( stream& strm, response& res );
    void write_request( stream& strm, request& req );
    virtual bool read_and_close_socket( socket_t sock, request& req, response& res );
};  /// class client

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SSL_socket_stream : public stream {
public:
    SSL_socket_stream( socket_t sock, SSL* ssl );
    ~SSL_socket_stream() override;
    int read( char* ptr, size_t size ) override;
    int write( const char* ptr, size_t size ) override;
    std::string get_remote_addr() const override;

private:
    socket_t sock_;
    SSL* ssl_;
};  /// class SSL_socket_stream

class SSL_server : public server {
public:
    SSL_server( const char* cert_path, const char* private_key_path,
        size_t a_max_handler_queues = __SKUTILS_HTTP_DEFAULT_MAX_PARALLEL_QUEUES_COUNT__,
        bool is_async_http_transfer_mode = true );
    ~SSL_server() override;
    bool is_valid() const override;
    bool is_ssl() const override { return true; }

private:
    bool read_and_close_socket_sync( socket_t sock ) override;
    void read_and_close_socket_async( socket_t sock ) override;
    SSL_CTX* ctx_;
    std::mutex ctx_mutex_;
};  /// class SSL_server

class SSL_client_options {
public:
    std::string ca_file, ca_path, client_cert, client_key;
    int client_key_type = SSL_FILETYPE_PEM;
    long ctx_mode = SSL_MODE_AUTO_RETRY;
    long ctx_cache_mode = SSL_SESS_CACHE_CLIENT;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SSL_client : public client {
public:
    SSL_client_options optsSSL;
    SSL_client( int ipVer, const char* host, int port = 443,
        int timeout_milliseconds = __SKUTILS_HTTP_CLIENT_CONNECT_TIMEOUT_MILLISECONDS__,
        SSL_client_options* pOptsSSL = nullptr );
    ~SSL_client() override;
    bool is_valid() const override;
    bool is_ssl() const override { return true; }

private:
    bool read_and_close_socket( socket_t sock, request& req, response& res ) override;
    SSL_CTX* ctx_;
    std::mutex ctx_mutex_;
};  /// class SSL_client

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace detail {

// NOTE: until the read size reaches "fixed_buffer_size", use "fixed_buffer" to store data. The call
// can set memory on stack for performance.
class stream_line_reader {
public:
    stream_line_reader( stream& strm, char* fixed_buffer, size_t fixed_buffer_size );
    ~stream_line_reader();
    const char* ptr() const;
    bool getline();

private:
    void append( char c );
    stream& strm_;
    char* fixed_buffer_;
    const size_t fixed_buffer_size_;
    size_t fixed_buffer_used_size_;
    std::string glowable_buffer_;
};

class SSLInit {
public:
    SSLInit();
    ~SSLInit();
};  /// class SSLInit

// static SSLInit sslinit_;

};  // namespace detail

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template < typename... Args >
inline void stream::write_format( const char* fmt, const Args&... args ) {
    const auto bufsiz = 2048;
    char buf[bufsiz];
#if defined( _MSC_VER ) && _MSC_VER < 1900
    auto n = _snprintf_s( buf, bufsiz, bufsiz - 1, fmt, args... );
#else
    auto n = snprintf( buf, bufsiz - 1, fmt, args... );
#endif
    if ( n > 0 ) {
        if ( n >= bufsiz - 1 ) {
            std::vector< char > glowable_buf( bufsiz );

            while ( n >= static_cast< int >( glowable_buf.size() - 1 ) ) {
                glowable_buf.resize( glowable_buf.size() * 2 );
#if defined( _MSC_VER ) && _MSC_VER < 1900
                n = _snprintf_s(
                    &glowable_buf[0], glowable_buf.size(), glowable_buf.size() - 1, fmt, args... );
#else
                n = snprintf( &glowable_buf[0], glowable_buf.size() - 1, fmt, args... );
#endif
            }
            write( &glowable_buf[0], n );
        } else {
            write( buf, n );
        }
    }
}

};  // namespace http

struct result_of_http_request {
    bool isBinary_ = false;
    nlohmann::json joOut_;
    std::vector< uint8_t > vecBytes_;
};  /// struct result_of_http_request

namespace http_pg {
typedef std::function< skutils::result_of_http_request( const nlohmann::json&,
    const std::string& strOrigin, int ipVer, const std::string& strDstAddress, int nDstPort ) >
    pg_on_request_handler_t;

typedef void* wrapped_proxygen_server_handle;

struct pg_accumulate_entry {
    int ipVer_ = -1;
    std::string strBindAddr_;
    int nPort_ = -1;
    std::string cert_path_;
    std::string private_key_path_;
    std::string ca_path_;
};  // struct pg_accumulate_entry

typedef std::vector< pg_accumulate_entry > pg_accumulate_entries;

bool pg_logging_get();
void pg_logging_set( bool bIsLoggingMode );
wrapped_proxygen_server_handle pg_start( pg_on_request_handler_t h, const pg_accumulate_entry& pge,
    int32_t threads = 0, int32_t threads_limit = 0 );
wrapped_proxygen_server_handle pg_start( pg_on_request_handler_t h,
    const pg_accumulate_entries& entries, int32_t threads = 0, int32_t threads_limit = 0 );
void pg_stop( wrapped_proxygen_server_handle hServer );

void pg_accumulate_clear();
size_t pg_accumulate_size();
void pg_accumulate_add( int ipVer, std::string strBindAddr, int nPort, const char* cert_path,
    const char* private_key_path, const char* ca_path );
void pg_accumulate_add( const pg_accumulate_entry& pge );
wrapped_proxygen_server_handle pg_accumulate_start(
    pg_on_request_handler_t h, int32_t threads = 0, int32_t threads_limit = 0 );

typedef void ( *logging_fail_func_t )();

void install_logging_fail_func( logging_fail_func_t fn );

void init_logging( const char* strProgramName );

};  // namespace http_pg

namespace http_curl {

class client {
    const skutils::url u_;
    int timeout_milliseconds_;
    skutils::http::SSL_client_options optsSSL;
    const char * pCurlCryptoEngine_ = nullptr;
    const char *pCryptoEnginePassphrase_ = nullptr;
    //
    struct MemoryStruct {
        char * memory;
        size_t size;
    };
    static size_t stat_WriteMemoryCallback( void * contents, size_t size, size_t nmemb, void * userp );
    //
public:
    bool isVerboseInsideCURL_ = false;
    bool isSslVerifyPeer_ = false;
    bool isSslVerifyHost_ = false;
    std::string strUserAgent_ = "libcurl-agent/1.0";
    std::string strDnsServers_ = "8.8.8.8,4.4.4.4,8.8.4.4";
    std::string strKeyType_ = "PEM";
    //
    client( const skutils::url& u,
        int timeout_milliseconds = __SKUTILS_HTTP_CLIENT_CONNECT_TIMEOUT_MILLISECONDS__,
        skutils::http::SSL_client_options* pOptsSSL = nullptr );
    virtual ~client();
    virtual bool is_valid() const;
    virtual bool is_ssl() const;
    virtual bool is_ssl_with_explicit_cert_key() const;
    virtual bool query(
            const char * strInData,
            const char * strInContentType, // i.e. "application/json"
            std::string & strOutData,
            std::string & strOutContentType,
            skutils::http::common_network_exception::error_info& ei
            );
}; // class client

}; // namespace http_curl

};  // namespace skutils

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif  /// SKUTILS_HTTP_H
