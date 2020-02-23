// based on https://github.com/yhirose/cpp-httplib

#if ( !defined SKUTILS_HTTP_H )
#define SKUTILS_HTTP_H 1

#ifdef _WIN32
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
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>

typedef int socket_t;
#define INVALID_SOCKET ( -1 )
#endif  //_WIN32

#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <atomic>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <thread>

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

/// configuration

#define __SKUTILS_HTTP_ACCEPT_WAIT_MILLISECONDS__ ( 5 * 1000 )
#define __SKUTILS_HTTP_KEEPALIVE_TIMEOUT_MILLISECONDS__ ( 30 * 1000 )
#define __SKUTILS_HTTP_KEEPALIVE_MAX_COUNT__ ( 5 )

#define __SKUTILS_ASYNC_HTTP_POLL_TIMEOUT_MILLISECONDS__ ( 10 )
#define __SKUTILS_ASYNC_HTTP_FIRST_TIMEOUT_MILLISECONDS__ ( 0 )
#define __SKUTILS_ASYNC_HTTP_NEXT_TIMEOUT_MILLISECONDS__ ( 10 )
#define __SKUTILS_ASYNC_HTTP_RETRY_COUNT__ ( 100 * 30 )

#define __SKUTILS_HTTP_DEFAULT_MAX_PARALLEL_QUEUES_COUNT__ ( 16 )

#define __SKUTILS_HTTP_CLIENT_CONNECT_TIMEOUT_MILLISECONDS__ ( 60 * 1000 )

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
    virtual int write( const char* ptr ) = 0;
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
    virtual int write( const char* ptr );
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
    virtual int write( const char* ptr );
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
    virtual void step() = 0;
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

    typedef std::function< void( stream& strm, bool last_connection, bool& connection_close ) >
        callback_success_t;
    typedef std::function< void() > callback_fail_t;

    callback_success_t callback_success_;
    callback_fail_t callback_fail_;

    async_read_and_close_socket_base( server& srv, socket_t socket,
        size_t poll_ms = __SKUTILS_ASYNC_HTTP_POLL_TIMEOUT_MILLISECONDS__,
        size_t retry_count = __SKUTILS_ASYNC_HTTP_RETRY_COUNT__,
        size_t retry_after_ms = __SKUTILS_ASYNC_HTTP_FIRST_TIMEOUT_MILLISECONDS__,
        size_t retry_first_ms = __SKUTILS_ASYNC_HTTP_NEXT_TIMEOUT_MILLISECONDS__ );
    virtual ~async_read_and_close_socket_base();
    virtual void run();
    virtual void schedule_first_step();
    virtual void schedule_next_step();
    virtual void close_socket();
    void call_fail_handler( bool is_close_socket = true, bool is_remove_this_task = true );
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
    virtual ~async_read_and_close_socket();
    virtual void run();
    virtual void step();
    virtual void was_added();
    virtual void will_remove();
    virtual void close_socket();
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
    virtual ~async_read_and_close_socket_SSL();
    virtual void run();
    virtual void step();
    virtual void was_added();
    virtual void will_remove();
    virtual void close_socket();
};  /// class async_read_and_close_socket_SSL

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class server {
public:
    mutable bool is_async_http_transfer_mode_;
    mutable int ipVer_ = -1;  // not known before listen
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

    bool set_base_dir( const char* path );

    void set_error_handler( Handler handler );
    void set_logger( Logger logger );

    size_t get_keep_alive_max_count() const;
    void set_keep_alive_max_count( size_t cnt );

    int bind_to_any_port( int ipVer, const char* host, int socket_flags = 0 );
    bool listen_after_bind();

    bool listen( int ipVer, const char* host, int port, int socket_flags = 0 );

    bool is_running() const;
    void stop();
    virtual bool is_ssl() const { return false; }

protected:
    bool process_request(
        const std::string& origin, stream& strm, bool last_connection, bool& connection_close );

    std::atomic< size_t > keep_alive_max_count_;

private:
    typedef std::vector< std::pair< std::regex, Handler > > Handlers;

    socket_t create_server_socket( int ipVer, const char* host, int port, int socket_flags ) const;
    int bind_internal( int ipVer, const char* host, int port, int socket_flags );
    bool listen_internal();

    bool routing( request& req, response& res );
    bool handle_file_request( request& req, response& res );
    bool dispatch_request( request& req, response& res, Handlers& handlers );

    bool parse_request_line( const char* s, request& req );
    void write_response( stream& strm, bool last_connection, const request& req, response& res );

    virtual bool read_and_close_socket_sync( socket_t sock );
    virtual void read_and_close_socket_async( socket_t sock );

    std::atomic_bool is_in_loop_ = false;
    std::atomic_bool is_running_ = false;
    socket_t svr_sock_;
    std::string base_dir_;
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

class client {
public:
    mutable int ipVer_ = -1;  // not known before connect
    client( int ipVer, const char* host, int port = 80,
        int timeout_milliseconds = __SKUTILS_HTTP_CLIENT_CONNECT_TIMEOUT_MILLISECONDS__ );

    virtual ~client();

    virtual bool is_valid() const;
    virtual bool is_ssl() const { return false; }

    std::shared_ptr< response > Get( const char* path, fn_progress progress = nullptr );
    std::shared_ptr< response > Get(
        const char* path, const map_headers& headers, fn_progress progress = nullptr );

    std::shared_ptr< response > Head( const char* path );
    std::shared_ptr< response > Head( const char* path, const map_headers& headers );

    std::shared_ptr< response > Post(
        const char* path, const std::string& body, const char* content_type );
    std::shared_ptr< response > Post( const char* path, const map_headers& headers,
        const std::string& body, const char* content_type );

    std::shared_ptr< response > Post( const char* path, const map_params& params );
    std::shared_ptr< response > Post(
        const char* path, const map_headers& headers, const map_params& params );

    std::shared_ptr< response > Put(
        const char* path, const std::string& body, const char* content_type );
    std::shared_ptr< response > Put( const char* path, const map_headers& headers,
        const std::string& body, const char* content_type );

    std::shared_ptr< response > Patch(
        const char* path, const std::string& body, const char* content_type );
    std::shared_ptr< response > Patch( const char* path, const map_headers& headers,
        const std::string& body, const char* content_type );

    std::shared_ptr< response > Delete( const char* path );
    std::shared_ptr< response > Delete( const char* path, const map_headers& headers );

    std::shared_ptr< response > Options( const char* path );
    std::shared_ptr< response > Options( const char* path, const map_headers& headers );

    bool send( request& req, response& res );

protected:
    bool process_request( const std::string& origin, stream& strm, request& req, response& res,
        bool& connection_close );
    const std::string host_;
    const int port_;
    int timeout_milliseconds_;
    const std::string host_and_port_;

private:
    socket_t create_client_socket( int ipVer ) const;
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
    int write( const char* ptr ) override;
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
};  // namespace skutils

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif  /// SKUTILS_HTTP_H
