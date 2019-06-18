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

#include <skutils/dispatch.h>
#include <skutils/network.h>

/// configuration

#define __SKUTILS_HTTP_ACCEPT_WAIT_MILLISECONDS__ ( 3 * 1000 )
#define __SKUTILS_HTTP_KEEPALIVE_TIMEOUT_MILLISECONDS__ ( 20 * 1000 )

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

// enum class e_http_version { v1_0 = 0, v1_1 };

typedef std::multimap< std::string, std::string, detail::ci > map_headers;

template < typename uint64_t, typename... Args >
std::pair< std::string, std::string > make_range_header( uint64_t value, Args... args );

typedef std::multimap< std::string, std::string > map_params;
typedef std::smatch match;
typedef std::function< bool( uint64_t current, uint64_t total ) > fn_progress;

struct multipart_file {
    std::string filename_;
    std::string content_type_;
    size_t offset_ = 0;
    size_t length_ = 0;
};  /// struct multipart_file
typedef std::multimap< std::string, multipart_file > multipart_files;

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

struct response {
    std::string version_;
    int status_;
    map_headers headers_;
    std::string body_;
    std::function< std::string( uint64_t offset ) > streamcb_;

    bool has_header( const char* key ) const;
    std::string get_header_value( const char* key, size_t id = 0 ) const;
    size_t get_header_value_count( const char* key ) const;
    void set_header( const char* key, const char* val );

    void set_redirect( const char* uri );
    void set_content( const char* s, size_t n, const char* content_type );
    void set_content( const std::string& s, const char* content_type );

    response() : status_( -1 ) {}
};  /// struct response

class stream {
public:
    virtual ~stream() {}
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

class buffer_stream : public stream {
public:
    buffer_stream() {}
    virtual ~buffer_stream() {}
    virtual int read( char* ptr, size_t size );
    virtual int write( const char* ptr, size_t size );
    virtual int write( const char* ptr );
    virtual std::string get_remote_addr() const;
    const std::string& get_buffer() const;

private:
    std::string buffer_;
};  /// class buffer_stream

class server {
public:
    typedef std::function< void( const request&, response& ) > Handler;
    typedef std::function< void( const request&, const response& ) > Logger;

    server();

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

    void set_keep_alive_max_count( size_t count );

    int bind_to_any_port( const char* host, int socket_flags = 0 );
    bool listen_after_bind();

    bool listen( const char* host, int port, int socket_flags = 0 );

    bool is_running() const;
    void stop();
    virtual bool is_ssl() const { return false; }

protected:
    bool process_request(
        const std::string& origin, stream& strm, bool last_connection, bool& connection_close );

    size_t keep_alive_max_count_;

private:
    typedef std::vector< std::pair< std::regex, Handler > > Handlers;

    socket_t create_server_socket( const char* host, int port, int socket_flags ) const;
    int bind_internal( const char* host, int port, int socket_flags );
    bool listen_internal();

    bool routing( request& req, response& res );
    bool handle_file_request( request& req, response& res );
    bool dispatch_request( request& req, response& res, Handlers& handlers );

    bool parse_request_line( const char* s, request& req );
    void write_response( stream& strm, bool last_connection, const request& req, response& res );

    virtual bool read_and_close_socket( socket_t sock );

    volatile bool is_in_loop_ = false;
    bool is_running_;
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

    std::mutex running_connectoin_handlers_mutex_;
    volatile int running_connectoin_handlers_;
    int running_connectoin_handlers_get() {
        int n = 0;
        std::lock_guard< std::mutex > guard( running_connectoin_handlers_mutex_ );
        n = running_connectoin_handlers_;
        return n;
    }
    void running_connectoin_handlers_increment() {
        std::lock_guard< std::mutex > guard( running_connectoin_handlers_mutex_ );
        ++running_connectoin_handlers_;
    }
    void running_connectoin_handlers_decrement() {
        std::lock_guard< std::mutex > guard( running_connectoin_handlers_mutex_ );
        --running_connectoin_handlers_;
    }

public:
    bool is_in_loop() const { return is_in_loop_; }
    void wait_while_in_loop() const {
        while ( is_in_loop() )
            std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
    }
};  /// class server

class client {
public:
    client( const char* host, int port = 80, int timeout_milliseconds = 60 * 1000 );

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
    socket_t create_client_socket() const;
    bool read_response_line( stream& strm, response& res );
    void write_request( stream& strm, request& req );
    virtual bool read_and_close_socket( socket_t sock, request& req, response& res );
};  /// class client

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
    SSL_server( const char* cert_path, const char* private_key_path );
    ~SSL_server() override;
    bool is_valid() const override;
    bool is_ssl() const override { return true; }

private:
    bool read_and_close_socket( socket_t sock ) override;
    SSL_CTX* ctx_;
    std::mutex ctx_mutex_;
};  /// class SSL_server

class SSL_client : public client {
public:
    SSL_client( const char* host, int port = 443, int timeout_milliseconds = 60 * 1000 );
    ~SSL_client() override;
    bool is_valid() const override;
    bool is_ssl() const override { return true; }

private:
    bool read_and_close_socket( socket_t sock, request& req, response& res ) override;
    SSL_CTX* ctx_;
    std::mutex ctx_mutex_;
};  /// class SSL_server

/// Implementation

namespace detail {

template < class Fn >
void split( const char* b, const char* e, char d, Fn fn ) {
    int i = 0;
    int beg = 0;
    while ( e ? ( b + i != e ) : ( b[i] != '\0' ) ) {
        if ( b[i] == d ) {
            fn( &b[beg], &b[i] );
            beg = i + 1;
        }
        i++;
    }
    if ( i )
        fn( &b[beg], &b[i] );
}

// NOTE: until the read size reaches "fixed_buffer_size", use "fixed_buffer" to store data. The call
// can set memory on stack for performance.
class stream_line_reader {
public:
    stream_line_reader( stream& strm, char* fixed_buffer, size_t fixed_buffer_size )
        : strm_( strm ), fixed_buffer_( fixed_buffer ), fixed_buffer_size_( fixed_buffer_size ) {}
    const char* ptr() const {
        if ( glowable_buffer_.empty() ) {
            return fixed_buffer_;
        } else {
            return glowable_buffer_.data();
        }
    }
    bool getline() {
        fixed_buffer_used_size_ = 0;
        glowable_buffer_.clear();
        for ( size_t i = 0;; i++ ) {
            char byte;
            auto n = strm_.read( &byte, 1 );
            if ( n < 0 )
                return false;
            if ( n == 0 ) {
                if ( i == 0 )
                    return false;
                break;
            }
            append( byte );
            if ( byte == '\n' )
                break;
        }
        return true;
    }

private:
    void append( char c ) {
        if ( fixed_buffer_used_size_ < fixed_buffer_size_ - 1 ) {
            fixed_buffer_[fixed_buffer_used_size_++] = c;
            fixed_buffer_[fixed_buffer_used_size_] = '\0';
        } else {
            if ( glowable_buffer_.empty() ) {
                assert( fixed_buffer_[fixed_buffer_used_size_] == '\0' );
                glowable_buffer_.assign( fixed_buffer_, fixed_buffer_used_size_ );
            }
            glowable_buffer_ += c;
        }
    }
    stream& strm_;
    char* fixed_buffer_;
    const size_t fixed_buffer_size_;
    size_t fixed_buffer_used_size_;
    std::string glowable_buffer_;
};

inline int close_socket( socket_t sock ) {
#ifdef _WIN32
    return closesocket( sock );
#else
    return close( sock );
#endif
}

inline int poll_impl( socket_t sock, short which_poll, int timeout_milliseconds ) {
    struct pollfd fds[1];
    int nfds = 1;
    fds[0].fd = sock;
    fds[0].events = which_poll;
    int rc = poll( fds, nfds, timeout_milliseconds );
    return rc;
}
inline bool poll_read( socket_t sock, int timeout_milliseconds ) {
    return ( poll_impl( sock, POLLIN, timeout_milliseconds ) > 0 ) ? true : false;
}
inline bool poll_write( socket_t sock, int timeout_milliseconds ) {
    return ( poll_impl( sock, POLLOUT, timeout_milliseconds ) > 0 ) ? true : false;
}

inline bool wait_until_socket_is_ready_client( socket_t sock, int timeout_milliseconds ) {
    //
    // TO-DO: l_sergiy: switch HTTP/client to poll() later
    //
    //    if ( poll_read( sock, timeout_milliseconds ) || poll_write( sock, timeout_milliseconds ) )
    //    {
    //        int error = 0;
    //        socklen_t len = sizeof( error );
    //        if ( getsockopt( sock, SOL_SOCKET, SO_ERROR, ( char* ) &error, &len ) < 0 || error )
    //            return false;
    //    } else
    //        return false;
    //    return true;

    fd_set fdsr;
    FD_ZERO( &fdsr );
    FD_SET( sock, &fdsr );
    auto fdsw = fdsr;
    auto fdse = fdsr;
    timeval tv;
    tv.tv_sec = static_cast< long >( timeout_milliseconds / 1000 );
    tv.tv_usec = static_cast< long >( ( timeout_milliseconds % 1000 ) * 1000 );
    if ( select( static_cast< int >( sock + 1 ), &fdsr, &fdsw, &fdse, &tv ) < 0 )
        return false;
    if ( FD_ISSET( sock, &fdsr ) || FD_ISSET( sock, &fdsw ) ) {
        int error = 0;
        socklen_t len = sizeof( error );
        if ( getsockopt( sock, SOL_SOCKET, SO_ERROR, ( char* ) &error, &len ) < 0 || error )
            return false;
    } else
        return false;
    return true;
}

template < typename T >
inline bool read_and_close_socket( socket_t sock, size_t keep_alive_max_count, T callback ) {
    bool ret = false;
    if ( keep_alive_max_count > 0 ) {
        auto count = keep_alive_max_count;
        while ( count > 0 &&
                detail::poll_read( sock, __SKUTILS_HTTP_KEEPALIVE_TIMEOUT_MILLISECONDS__ ) ) {
            socket_stream strm( sock );
            auto last_connection = count == 1;
            auto connection_close = false;
            ret = callback( strm, last_connection, connection_close );
            if ( !ret || connection_close )
                break;
            count--;
        }
    } else {
        socket_stream strm( sock );
        auto dummy_connection_close = false;
        ret = callback( strm, true, dummy_connection_close );
    }
    close_socket( sock );
    return ret;
}

inline int shutdown_socket( socket_t sock ) {
#ifdef _WIN32
    return shutdown( sock, SD_BOTH );
#else
    return shutdown( sock, SHUT_RDWR );
#endif
}

template < typename Fn >
socket_t create_socket( const char* host, int port, Fn fn, int socket_flags = 0 ) {
#ifdef _WIN32
#define SO_SYNCHRONOUS_NONALERT 0x20
#define SO_OPENTYPE 0x7008
    int opt = SO_SYNCHRONOUS_NONALERT;
    setsockopt( INVALID_SOCKET, SOL_SOCKET, SO_OPENTYPE, ( char* ) &opt, sizeof( opt ) );
#endif
    struct addrinfo hints;
    struct addrinfo* result;
    memset( &hints, 0, sizeof( struct addrinfo ) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = socket_flags;
    hints.ai_protocol = 0;
    auto service = std::to_string( port );
    if ( getaddrinfo( host, service.c_str(), &hints, &result ) )
        return INVALID_SOCKET;
    for ( auto rp = result; rp; rp = rp->ai_next ) {
        auto sock = socket( rp->ai_family, rp->ai_socktype, rp->ai_protocol );
        if ( sock == INVALID_SOCKET ) {
            continue;
        }
        // make "reuse address" option available
        int yes = 1;
        setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, ( char* ) &yes, sizeof( yes ) );
        // bind or connect
        if ( fn( sock, *rp ) ) {
            freeaddrinfo( result );
            return sock;
        }
        close_socket( sock );
    }
    freeaddrinfo( result );
    return INVALID_SOCKET;
}

inline void set_nonblocking( socket_t sock, bool nonblocking ) {
#ifdef _WIN32
    auto flags = nonblocking ? 1UL : 0UL;
    ioctlsocket( sock, FIONBIO, &flags );
#else
    auto flags = fcntl( sock, F_GETFL, 0 );
    fcntl( sock, F_SETFL, nonblocking ? ( flags | O_NONBLOCK ) : ( flags & ( ~O_NONBLOCK ) ) );
#endif
}

inline bool is_connection_error() {
#ifdef _WIN32
    return WSAGetLastError() != WSAEWOULDBLOCK;
#else
    return errno != EINPROGRESS;
#endif
}

inline std::string get_remote_addr( socket_t sock ) {
    struct sockaddr_storage addr;
    socklen_t len = sizeof( addr );
    if ( !getpeername( sock, ( struct sockaddr* ) &addr, &len ) ) {
        char ipstr[NI_MAXHOST];
        if ( !getnameinfo( ( struct sockaddr* ) &addr, len, ipstr, sizeof( ipstr ), nullptr, 0,
                 NI_NUMERICHOST ) )
            return ipstr;
    }
    return std::string();
}

inline bool is_file( const std::string& path ) {
    struct stat st;
    return stat( path.c_str(), &st ) >= 0 && S_ISREG( st.st_mode );
}

inline bool is_dir( const std::string& path ) {
    struct stat st;
    return stat( path.c_str(), &st ) >= 0 && S_ISDIR( st.st_mode );
}

inline bool is_valid_path( const std::string& path ) {
    size_t level = 0;
    size_t i = 0;
    // skip slash
    while ( i < path.size() && path[i] == '/' )
        i++;
    while ( i < path.size() ) {
        // read component
        auto beg = i;
        while ( i < path.size() && path[i] != '/' )
            i++;
        auto len = i - beg;
        assert( len > 0 );
        if ( !path.compare( beg, len, "." ) ) {
            ;
        } else if ( !path.compare( beg, len, ".." ) ) {
            if ( level == 0 )
                return false;
            level--;
        } else
            level++;
        // skip slash
        while ( i < path.size() && path[i] == '/' )
            i++;
    }
    return true;
}

inline void read_file( const std::string& path, std::string& out ) {
    std::ifstream fs( path, std::ios_base::binary );
    fs.seekg( 0, std::ios_base::end );
    auto size = fs.tellg();
    fs.seekg( 0 );
    out.resize( static_cast< size_t >( size ) );
    fs.read( &out[0], size );
}

inline std::string file_extension( const std::string& path ) {
    std::smatch m;
    auto pat = std::regex( "\\.([a-zA-Z0-9]+)$" );
    if ( std::regex_search( path, m, pat ) )
        return m[1].str();
    return std::string();
}

inline const char* find_content_type( const std::string& path ) {
    auto ext = file_extension( path );
    if ( ext == "txt" )
        return "text/plain";
    if ( ext == "html" )
        return "text/html";
    if ( ext == "css" )
        return "text/css";
    if ( ext == "jpeg" || ext == "jpg" )
        return "image/jpg";
    if ( ext == "png" )
        return "image/png";
    if ( ext == "gif" )
        return "image/gif";
    if ( ext == "svg" )
        return "image/svg+xml";
    if ( ext == "ico" )
        return "image/x-icon";
    if ( ext == "json" )
        return "application/json";
    if ( ext == "pdf" )
        return "application/pdf";
    if ( ext == "js" )
        return "application/javascript";
    if ( ext == "xml" )
        return "application/xml";
    if ( ext == "xhtml" )
        return "application/xhtml+xml";
    return nullptr;
}

inline const char* status_message( int status ) {
    switch ( status ) {
    case 200:
        return "OK";
    case 301:
        return "Moved Permanently";
    case 302:
        return "Found";
    case 303:
        return "See Other";
    case 304:
        return "Not Modified";
    case 400:
        return "Bad request";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 415:
        return "Unsupported Media Type";
    default:
    case 500:
        return "Internal server Error";
    }
}

inline bool has_header( const map_headers& headers, const char* key ) {
    return headers.find( key ) != headers.end();
}

inline const char* get_header_value(
    const map_headers& headers, const char* key, size_t id = 0, const char* def = nullptr ) {
    auto it = headers.find( key );
    std::advance( it, id );
    if ( it != headers.end() )
        return it->second.c_str();
    return def;
}

inline int get_header_value_int( const map_headers& headers, const char* key, int def = 0 ) {
    auto it = headers.find( key );
    if ( it != headers.end() )
        return std::stoi( it->second );
    return def;
}

inline bool read_headers( stream& strm, map_headers& headers ) {
    static std::regex re( R"((.+?):\s*(.+?)\s*\r\n)" );
    const auto bufsiz = 2048;
    char buf[bufsiz];
    stream_line_reader reader( strm, buf, bufsiz );
    for ( ;; ) {
        if ( !reader.getline() )
            return false;
        if ( !strcmp( reader.ptr(), "\r\n" ) )
            break;
        std::cmatch m;
        if ( std::regex_match( reader.ptr(), m, re ) ) {
            auto key = std::string( m[1] );
            auto val = std::string( m[2] );
            headers.emplace( key, val );
        }
    }
    return true;
}

inline bool read_content_with_length(
    stream& strm, std::string& out, size_t len, fn_progress progress ) {
    out.assign( len, 0 );
    size_t r = 0;
    while ( r < len ) {
        auto n = strm.read( &out[r], len - r );
        if ( n <= 0 )
            return false;
        r += n;
        if ( progress ) {
            if ( !progress( r, len ) )
                return false;
        }
    }
    return true;
}

inline bool read_content_without_length( stream& strm, std::string& out ) {
    for ( ;; ) {
        char byte;
        auto n = strm.read( &byte, 1 );
        if ( n < 0 )
            return false;
        else if ( n == 0 )
            return true;
        out += byte;
    }
    return true;
}

inline bool read_content_chunked( stream& strm, std::string& out ) {
    const auto bufsiz = 16;
    char buf[bufsiz];
    stream_line_reader reader( strm, buf, bufsiz );
    if ( !reader.getline() )
        return false;
    auto chunk_len = std::stoi( reader.ptr(), 0, 16 );
    while ( chunk_len > 0 ) {
        std::string chunk;
        if ( !read_content_with_length( strm, chunk, chunk_len, nullptr ) )
            return false;
        if ( !reader.getline() )
            return false;
        if ( strcmp( reader.ptr(), "\r\n" ) )
            break;
        out += chunk;
        if ( !reader.getline() )
            return false;
        chunk_len = std::stoi( reader.ptr(), 0, 16 );
    }
    if ( chunk_len == 0 ) {
        // reader terminator after chunks
        if ( !reader.getline() || strcmp( reader.ptr(), "\r\n" ) )
            return false;
    }
    return true;
}

template < typename T >
bool read_content( stream& strm, T& x, fn_progress progress = fn_progress() ) {
    if ( has_header( x.headers_, "Content-Length" ) ) {
        auto len = get_header_value_int( x.headers_, "Content-Length", 0 );
        if ( len == 0 ) {
            const auto& encoding = get_header_value( x.headers_, "Transfer-Encoding", 0, "" );
            if ( !strcasecmp( encoding, "chunked" ) ) {
                return read_content_chunked( strm, x.body_ );
            }
        }
        return read_content_with_length( strm, x.body_, len, progress );
    } else {
        const auto& encoding = get_header_value( x.headers_, "Transfer-Encoding", 0, "" );
        if ( !strcasecmp( encoding, "chunked" ) ) {
            return read_content_chunked( strm, x.body_ );
        }
        return read_content_without_length( strm, x.body_ );
    }
    return true;
}

template < typename T >
inline void write_headers( stream& strm, const T& info ) {
    for ( const auto& x : info.headers_ ) {
        strm.write_format( "%s: %s\r\n", x.first.c_str(), x.second.c_str() );
    }
    strm.write( "\r\n" );
}

inline std::string encode_url( const std::string& s ) {
    std::string result;

    for ( auto i = 0; s[i]; i++ ) {
        switch ( s[i] ) {
        case ' ':
            result += "%20";
            break;
        case '+':
            result += "%2B";
            break;
        case '\r':
            result += "%0D";
            break;
        case '\n':
            result += "%0A";
            break;
        case '\'':
            result += "%27";
            break;
        case ',':
            result += "%2C";
            break;
        case ':':
            result += "%3A";
            break;
        case ';':
            result += "%3B";
            break;
        default:
            auto c = static_cast< uint8_t >( s[i] );
            if ( c >= 0x80 ) {
                result += '%';
                char hex[4];
                size_t len = snprintf( hex, sizeof( hex ) - 1, "%02X", c );
                assert( len == 2 );
                result.append( hex, len );
            } else {
                result += s[i];
            }
            break;
        }
    }

    return result;
}

inline bool is_hex( char c, int& v ) {
    if ( 0x20 <= c && isdigit( c ) ) {
        v = c - '0';
        return true;
    } else if ( 'A' <= c && c <= 'F' ) {
        v = c - 'A' + 10;
        return true;
    } else if ( 'a' <= c && c <= 'f' ) {
        v = c - 'a' + 10;
        return true;
    }
    return false;
}

inline bool from_hex_to_i( const std::string& s, size_t i, size_t cnt, int& val ) {
    if ( i >= s.size() ) {
        return false;
    }

    val = 0;
    for ( ; cnt; i++, cnt-- ) {
        if ( !s[i] ) {
            return false;
        }
        int v = 0;
        if ( is_hex( s[i], v ) ) {
            val = val * 16 + v;
        } else {
            return false;
        }
    }
    return true;
}

inline std::string from_i_to_hex( uint64_t n ) {
    const char* charset = "0123456789abcdef";
    std::string ret;
    do {
        ret = charset[n & 15] + ret;
        n >>= 4;
    } while ( n > 0 );
    return ret;
}

inline size_t to_utf8( int code, char* buff ) {
    if ( code < 0x0080 ) {
        buff[0] = ( code & 0x7F );
        return 1;
    } else if ( code < 0x0800 ) {
        buff[0] = ( 0xC0 | ( ( code >> 6 ) & 0x1F ) );
        buff[1] = ( 0x80 | ( code & 0x3F ) );
        return 2;
    } else if ( code < 0xD800 ) {
        buff[0] = ( 0xE0 | ( ( code >> 12 ) & 0xF ) );
        buff[1] = ( 0x80 | ( ( code >> 6 ) & 0x3F ) );
        buff[2] = ( 0x80 | ( code & 0x3F ) );
        return 3;
    } else if ( code < 0xE000 ) {  // D800 - DFFF is invalid...
        return 0;
    } else if ( code < 0x10000 ) {
        buff[0] = ( 0xE0 | ( ( code >> 12 ) & 0xF ) );
        buff[1] = ( 0x80 | ( ( code >> 6 ) & 0x3F ) );
        buff[2] = ( 0x80 | ( code & 0x3F ) );
        return 3;
    } else if ( code < 0x110000 ) {
        buff[0] = ( 0xF0 | ( ( code >> 18 ) & 0x7 ) );
        buff[1] = ( 0x80 | ( ( code >> 12 ) & 0x3F ) );
        buff[2] = ( 0x80 | ( ( code >> 6 ) & 0x3F ) );
        buff[3] = ( 0x80 | ( code & 0x3F ) );
        return 4;
    }

    // NOTREACHED
    return 0;
}

inline std::string decode_url( const std::string& s ) {
    std::string result;

    for ( size_t i = 0; i < s.size(); i++ ) {
        if ( s[i] == '%' && i + 1 < s.size() ) {
            if ( s[i + 1] == 'u' ) {
                int val = 0;
                if ( from_hex_to_i( s, i + 2, 4, val ) ) {
                    // 4 digits Unicode codes
                    char buff[4];
                    size_t len = to_utf8( val, buff );
                    if ( len > 0 ) {
                        result.append( buff, len );
                    }
                    i += 5;  // 'u0000'
                } else {
                    result += s[i];
                }
            } else {
                int val = 0;
                if ( from_hex_to_i( s, i + 1, 2, val ) ) {
                    // 2 digits hex codes
                    result += val;
                    i += 2;  // '00'
                } else {
                    result += s[i];
                }
            }
        } else if ( s[i] == '+' ) {
            result += ' ';
        } else {
            result += s[i];
        }
    }

    return result;
}

inline void parse_query_text( const std::string& s, map_params& params ) {
    split( &s[0], &s[s.size()], '&', [&]( const char* b, const char* e ) {
        std::string key;
        std::string val;
        split( b, e, '=', [&]( const char* b, const char* e ) {
            if ( key.empty() ) {
                key.assign( b, e );
            } else {
                val.assign( b, e );
            }
        } );
        params.emplace( key, decode_url( val ) );
    } );
}

inline bool parse_multipart_boundary( const std::string& content_type, std::string& boundary ) {
    auto pos = content_type.find( "boundary=" );
    if ( pos == std::string::npos ) {
        return false;
    }

    boundary = content_type.substr( pos + 9 );
    return true;
}

inline bool parse_multipart_formdata(
    const std::string& boundary, const std::string& body, multipart_files& files ) {
    static std::string dash = "--";
    static std::string crlf = "\r\n";

    static std::regex re_content_type( "Content-Type: (.*?)", std::regex_constants::icase );

    static std::regex re_content_disposition(
        "Content-Disposition: form-data; name=\"(.*?)\"(?:; filename=\"(.*?)\")?",
        std::regex_constants::icase );

    auto dash_boundary = dash + boundary;

    auto pos = body.find( dash_boundary );
    if ( pos != 0 ) {
        return false;
    }

    pos += dash_boundary.size();

    auto next_pos = body.find( crlf, pos );
    if ( next_pos == std::string::npos ) {
        return false;
    }

    pos = next_pos + crlf.size();

    while ( pos < body.size() ) {
        next_pos = body.find( crlf, pos );
        if ( next_pos == std::string::npos ) {
            return false;
        }

        std::string name;
        multipart_file file;

        auto header = body.substr( pos, ( next_pos - pos ) );

        while ( pos != next_pos ) {
            std::smatch m;
            if ( std::regex_match( header, m, re_content_type ) ) {
                file.content_type_ = m[1];
            } else if ( std::regex_match( header, m, re_content_disposition ) ) {
                name = m[1];
                file.filename_ = m[2];
            }

            pos = next_pos + crlf.size();

            next_pos = body.find( crlf, pos );
            if ( next_pos == std::string::npos ) {
                return false;
            }

            header = body.substr( pos, ( next_pos - pos ) );
        }

        pos = next_pos + crlf.size();

        next_pos = body.find( crlf + dash_boundary, pos );

        if ( next_pos == std::string::npos ) {
            return false;
        }

        file.offset_ = pos;
        file.length_ = next_pos - pos;

        pos = next_pos + crlf.size() + dash_boundary.size();

        next_pos = body.find( crlf, pos );
        if ( next_pos == std::string::npos ) {
            return false;
        }

        files.emplace( name, file );

        pos = next_pos + crlf.size();
    }

    return true;
}

inline std::string to_lower( const char* beg, const char* end ) {
    std::string out;
    auto it = beg;
    while ( it != end ) {
        out += ::tolower( *it );
        it++;
    }
    return out;
}

inline void make_range_header_core( std::string& ) {}

template < typename uint64_t >
inline void make_range_header_core( std::string& field, uint64_t value ) {
    if ( !field.empty() ) {
        field += ", ";
    }
    field += std::to_string( value ) + "-";
}

template < typename uint64_t, typename... Args >
inline void make_range_header_core(
    std::string& field, uint64_t value1, uint64_t value2, Args... args ) {
    if ( !field.empty() ) {
        field += ", ";
    }
    field += std::to_string( value1 ) + "-" + std::to_string( value2 );
    make_range_header_core( field, args... );
}

#ifdef __SKUTILS_HTTP_WITH_ZLIB_SUPPORT__
inline bool can_compress( const std::string& content_type ) {
    return !content_type.find( "text/" ) || content_type == "image/svg+xml" ||
           content_type == "application/javascript" || content_type == "application/json" ||
           content_type == "application/xml" || content_type == "application/xhtml+xml";
}

inline void compress( std::string& content ) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    auto ret = deflateInit2( &strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY );
    if ( ret != Z_OK ) {
        return;
    }

    strm.avail_in = content.size();
    strm.next_in = ( Bytef* ) content.data();

    std::string compressed;

    const auto bufsiz = 16384;
    char buff[bufsiz];
    do {
        strm.avail_out = bufsiz;
        strm.next_out = ( Bytef* ) buff;
        deflate( &strm, Z_FINISH );
        compressed.append( buff, bufsiz - strm.avail_out );
    } while ( strm.avail_out == 0 );

    content.swap( compressed );

    deflateEnd( &strm );
}

inline void decompress( std::string& content ) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    // 15 is the value of wbits, which should be at the maximum possible value to ensure
    // that any gzip stream can be decoded. The offset of 16 specifies that the stream
    // to decompress will be formatted with a gzip wrapper.
    auto ret = inflateInit2( &strm, 16 + 15 );
    if ( ret != Z_OK ) {
        return;
    }

    strm.avail_in = content.size();
    strm.next_in = ( Bytef* ) content.data();

    std::string decompressed;

    const auto bufsiz = 16384;
    char buff[bufsiz];
    do {
        strm.avail_out = bufsiz;
        strm.next_out = ( Bytef* ) buff;
        inflate( &strm, Z_NO_FLUSH );
        decompressed.append( buff, bufsiz - strm.avail_out );
    } while ( strm.avail_out == 0 );

    content.swap( decompressed );

    inflateEnd( &strm );
}
#endif

#ifdef _WIN32
class WSInit {
public:
    WSInit() {
        WSADATA wsaData;
        WSAStartup( 0x0002, &wsaData );
    }

    ~WSInit() { WSACleanup(); }
};

static WSInit wsinit_;
#endif

}  // namespace detail

/// Header utilities

template < typename uint64_t, typename... Args >
inline std::pair< std::string, std::string > make_range_header( uint64_t value, Args... args ) {
    std::string field;
    detail::make_range_header_core( field, value, args... );
    field.insert( 0, "bytes=" );
    return std::make_pair( "Range", field );
}

/// Request implementation

inline bool request::has_header( const char* key ) const {
    return detail::has_header( headers_, key );
}

inline std::string request::get_header_value( const char* key, size_t id ) const {
    return detail::get_header_value( headers_, key, id, "" );
}

inline size_t request::get_header_value_count( const char* key ) const {
    auto r = headers_.equal_range( key );
    return std::distance( r.first, r.second );
}

inline void request::set_header( const char* key, const char* val ) {
    headers_.emplace( key, val );
}

inline bool request::has_param( const char* key ) const {
    return params_.find( key ) != params_.end();
}

inline std::string request::get_param_value( const char* key, size_t id ) const {
    auto it = params_.find( key );
    std::advance( it, id );
    if ( it != params_.end() ) {
        return it->second;
    }
    return std::string();
}

inline size_t request::get_param_value_count( const char* key ) const {
    auto r = params_.equal_range( key );
    return std::distance( r.first, r.second );
}

inline bool request::has_file( const char* key ) const {
    return files_.find( key ) != files_.end();
}

inline multipart_file request::get_file_value( const char* key ) const {
    auto it = files_.find( key );
    if ( it != files_.end() ) {
        return it->second;
    }
    return multipart_file();
}

/// Response implementation

inline bool response::has_header( const char* key ) const {
    return headers_.find( key ) != headers_.end();
}

inline std::string response::get_header_value( const char* key, size_t id ) const {
    return detail::get_header_value( headers_, key, id, "" );
}

inline size_t response::get_header_value_count( const char* key ) const {
    auto r = headers_.equal_range( key );
    return std::distance( r.first, r.second );
}

inline void response::set_header( const char* key, const char* val ) {
    headers_.emplace( key, val );
}

inline void response::set_redirect( const char* url ) {
    set_header( "Location", url );
    status_ = 302;
}

inline void response::set_content( const char* s, size_t n, const char* content_type ) {
    body_.assign( s, n );
    set_header( "Content-Type", content_type );
}

inline void response::set_content( const std::string& s, const char* content_type ) {
    body_ = s;
    set_header( "Content-Type", content_type );
}

/// Stream implementation

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

/// Socket stream implementation

inline socket_stream::socket_stream( socket_t sock ) : sock_( sock ) {}

inline socket_stream::~socket_stream() {}

inline int socket_stream::read( char* ptr, size_t size ) {
    return recv( sock_, ptr, static_cast< int >( size ), 0 );
}

inline int socket_stream::write( const char* ptr, size_t size ) {
    return send( sock_, ptr, static_cast< int >( size ), 0 );
}

inline int socket_stream::write( const char* ptr ) {
    return write( ptr, strlen( ptr ) );
}

inline std::string socket_stream::get_remote_addr() const {
    return detail::get_remote_addr( sock_ );
}

/// Buffer stream implementation

inline int buffer_stream::read( char* ptr, size_t size ) {
#if defined( _MSC_VER ) && _MSC_VER < 1900
    return static_cast< int >( buffer_._Copy_s( ptr, size, size ) );
#else
    return static_cast< int >( buffer_.copy( ptr, size ) );
#endif
}

inline int buffer_stream::write( const char* ptr, size_t size ) {
    buffer_.append( ptr, size );
    return static_cast< int >( size );
}

inline int buffer_stream::write( const char* ptr ) {
    size_t size = strlen( ptr );
    buffer_.append( ptr, size );
    return static_cast< int >( size );
}

inline std::string buffer_stream::get_remote_addr() const {
    return "";
}

inline const std::string& buffer_stream::get_buffer() const {
    return buffer_;
}

/// HTTP server implementation

inline server::server()
    : keep_alive_max_count_( 5 ),
      is_running_( false ),
      svr_sock_( INVALID_SOCKET ),
      running_connectoin_handlers_( 0 ) {
#ifndef _WIN32
    ::signal( SIGPIPE, SIG_IGN );
#endif
}

inline server::~server() {}

inline server& server::Get( const char* pattern, Handler handler ) {
    get_handlers_.push_back( std::make_pair( std::regex( pattern ), handler ) );
    return *this;
}

inline server& server::Post( const char* pattern, Handler handler ) {
    post_handlers_.push_back( std::make_pair( std::regex( pattern ), handler ) );
    return *this;
}

inline server& server::Put( const char* pattern, Handler handler ) {
    put_handlers_.push_back( std::make_pair( std::regex( pattern ), handler ) );
    return *this;
}

inline server& server::Patch( const char* pattern, Handler handler ) {
    patch_handlers_.push_back( std::make_pair( std::regex( pattern ), handler ) );
    return *this;
}

inline server& server::Delete( const char* pattern, Handler handler ) {
    delete_handlers_.push_back( std::make_pair( std::regex( pattern ), handler ) );
    return *this;
}

inline server& server::Options( const char* pattern, Handler handler ) {
    options_handlers_.push_back( std::make_pair( std::regex( pattern ), handler ) );
    return *this;
}

inline bool server::set_base_dir( const char* path ) {
    if ( detail::is_dir( path ) ) {
        base_dir_ = path;
        return true;
    }
    return false;
}

inline void server::set_error_handler( Handler handler ) {
    error_handler_ = handler;
}

inline void server::set_logger( Logger logger ) {
    logger_ = logger;
}

inline void server::set_keep_alive_max_count( size_t count ) {
    keep_alive_max_count_ = count;
}

inline int server::bind_to_any_port( const char* host, int socket_flags ) {
    return bind_internal( host, 0, socket_flags );
}

inline bool server::listen_after_bind() {
    return listen_internal();
}

inline bool server::listen( const char* host, int port, int socket_flags ) {
    if ( bind_internal( host, port, socket_flags ) < 0 )
        return false;
    return listen_internal();
}

inline bool server::is_running() const {
    return is_running_;
}

inline void server::stop() {
    if ( is_running_ ) {
        is_running_ = false;
        wait_while_in_loop();
        assert( svr_sock_ != INVALID_SOCKET );
        auto sock = svr_sock_;
        svr_sock_ = INVALID_SOCKET;
        detail::shutdown_socket( sock );
        detail::close_socket( sock );
    }
}

inline bool server::parse_request_line( const char* s, request& req ) {
    static std::regex re(
        "(GET|HEAD|POST|PUT|PATCH|DELETE|OPTIONS) (([^?]+)(?:\\?(.+?))?) (HTTP/1\\.[01])\r\n" );
    std::cmatch m;
    if ( std::regex_match( s, m, re ) ) {
        req.version_ = std::string( m[5] );
        req.method_ = std::string( m[1] );
        req.target_ = std::string( m[2] );
        req.path_ = detail::decode_url( m[3] );
        // parse query text
        auto len = std::distance( m[4].first, m[4].second );
        if ( len > 0 ) {
            detail::parse_query_text( m[4], req.params_ );
        }
        return true;
    }
    return false;
}

inline void server::write_response(
    stream& strm, bool last_connection, const request& req, response& res ) {
    assert( res.status_ != -1 );
    if ( 400 <= res.status_ && error_handler_ ) {
        error_handler_( req, res );
    }
    // response line
    strm.write_format( "HTTP/1.1 %d %s\r\n", res.status_, detail::status_message( res.status_ ) );
    // neaders
    if ( last_connection || req.get_header_value( "Connection" ) == "close" ) {
        res.set_header( "Connection", "close" );
    }
    if ( !last_connection && req.get_header_value( "Connection" ) == "Keep-Alive" ) {
        res.set_header( "Connection", "Keep-Alive" );
    }
    if ( res.body_.empty() ) {
        if ( !res.has_header( "Content-Length" ) ) {
            if ( res.streamcb_ ) {
                // streamed response
                res.set_header( "Transfer-Encoding", "chunked" );
            } else {
                res.set_header( "Content-Length", "0" );
            }
        }
    } else {
#ifdef __SKUTILS_HTTP_WITH_ZLIB_SUPPORT__
        // TO-DO: 'Accpet-Encoding' has gzip, not gzip;q=0
        const auto& encodings = req.get_header_value( "Accept-Encoding" );
        if ( encodings.find( "gzip" ) != std::string::npos &&
             detail::can_compress( res.get_header_value( "Content-Type" ) ) ) {
            detail::compress( res.body );
            res.set_header( "Content-Encoding", "gzip" );
        }
#endif
        if ( !res.has_header( "Content-Type" ) ) {
            res.set_header( "Content-Type", "text/plain" );
        }
        auto length = std::to_string( res.body_.size() );
        res.set_header( "Content-Length", length.c_str() );
    }
    detail::write_headers( strm, res );
    // body
    if ( req.method_ != "HEAD" ) {
        if ( !res.body_.empty() ) {
            strm.write( res.body_.c_str(), res.body_.size() );
        } else if ( res.streamcb_ ) {
            bool chunked_response = !res.has_header( "Content-Length" );
            uint64_t offset = 0;
            bool data_available = true;
            while ( data_available ) {
                std::string chunk = res.streamcb_( offset );
                offset += chunk.size();
                data_available = !chunk.empty();
                // Emit chunked response header and footer for each chunk
                if ( chunked_response )
                    chunk = detail::from_i_to_hex( chunk.size() ) + "\r\n" + chunk + "\r\n";
                if ( strm.write( chunk.c_str(), chunk.size() ) < 0 )
                    break;  // Stop on error
            }
        }
    }
    // log
    if ( logger_ ) {
        logger_( req, res );
    }
}

inline bool server::handle_file_request( request& req, response& res ) {
    if ( !base_dir_.empty() && detail::is_valid_path( req.path_ ) ) {
        std::string path = base_dir_ + req.path_;

        if ( !path.empty() && path.back() == '/' ) {
            path += "index.html";
        }
        if ( detail::is_file( path ) ) {
            detail::read_file( path, res.body_ );
            auto type = detail::find_content_type( path );
            if ( type ) {
                res.set_header( "Content-Type", type );
            }
            res.status_ = 200;
            return true;
        }
    }
    return false;
}

inline socket_t server::create_server_socket( const char* host, int port, int socket_flags ) const {
    return detail::create_socket( host, port,
        []( socket_t sock, struct addrinfo& ai ) -> bool {
            if (::bind( sock, ai.ai_addr, static_cast< int >( ai.ai_addrlen ) ) ) {
                return false;
            }
            if (::listen( sock, 5 ) ) {  // Listen through 5 channels
                return false;
            }
            return true;
        },
        socket_flags );
}

inline int server::bind_internal( const char* host, int port, int socket_flags ) {
    if ( !is_valid() ) {
        return -1;
    }
    svr_sock_ = create_server_socket( host, port, socket_flags );
    if ( svr_sock_ == INVALID_SOCKET ) {
        return -1;
    }
    if ( port == 0 ) {
        struct sockaddr_storage address;
        socklen_t len = sizeof( address );
        if ( getsockname( svr_sock_, reinterpret_cast< struct sockaddr* >( &address ), &len ) ==
             -1 ) {
            return -1;
        }
        if ( address.ss_family == AF_INET ) {
            return ntohs( reinterpret_cast< struct sockaddr_in* >( &address )->sin_port );
        } else if ( address.ss_family == AF_INET6 ) {
            return ntohs( reinterpret_cast< struct sockaddr_in6* >( &address )->sin6_port );
        } else {
            return -1;
        }
    } else {
        return port;
    }
}

inline bool server::listen_internal() {
    is_in_loop_ = true;
    auto ret = true;
    try {
        is_running_ = true;
        for ( ; is_running_; ) {
            bool isOK = detail::poll_read( svr_sock_, __SKUTILS_HTTP_ACCEPT_WAIT_MILLISECONDS__ );
            if ( !isOK ) {  // Timeout
                if ( svr_sock_ == INVALID_SOCKET ) {
                    // The server socket was closed by 'stop' method.
                    break;
                }
                continue;
            }

            MICROPROFILE_SCOPEI( "skutils", "http::server::listen_internal", MP_PALEGREEN );

            socket_t sock = accept( svr_sock_, NULL, NULL );
            if ( sock == INVALID_SOCKET ) {
                continue;
            }
            skutils::dispatch::job_t fn = [=]() {
                running_connectoin_handlers_increment();
                read_and_close_socket( sock );
                running_connectoin_handlers_decrement();
            };
            skutils::dispatch::async( fn );
        }
        for ( ; is_running_; ) {
            std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
            if ( !running_connectoin_handlers_get() ) {
                break;
            }
        }
    } catch ( const std::exception& ex ) {
        std::cerr << ex.what() << std::endl;
    }
    is_running_ = false;
    is_in_loop_ = false;
    return ret;
}

inline bool server::routing( request& req, response& res ) {
    if ( req.method_ == "GET" && handle_file_request( req, res ) ) {
        return true;
    }
    if ( req.method_ == "GET" || req.method_ == "HEAD" ) {
        return dispatch_request( req, res, get_handlers_ );
    } else if ( req.method_ == "POST" ) {
        return dispatch_request( req, res, post_handlers_ );
    } else if ( req.method_ == "PUT" ) {
        return dispatch_request( req, res, put_handlers_ );
    } else if ( req.method_ == "PATCH" ) {
        return dispatch_request( req, res, patch_handlers_ );
    } else if ( req.method_ == "DELETE" ) {
        return dispatch_request( req, res, delete_handlers_ );
    } else if ( req.method_ == "OPTIONS" ) {
        return dispatch_request( req, res, options_handlers_ );
    }
    return false;
}

inline bool server::dispatch_request( request& req, response& res, Handlers& handlers ) {
    for ( const auto& x : handlers ) {
        const auto& pattern = x.first;
        const auto& handler = x.second;
        if ( std::regex_match( req.path_, req.matches_, pattern ) ) {
            handler( req, res );
            return true;
        }
    }
    return false;
}

inline bool server::process_request(
    const std::string& origin, stream& strm, bool last_connection, bool& connection_close ) {
    MICROPROFILE_SCOPEI( "skutils", "http::server::process_request", MP_PAPAYAWHIP );

    const auto bufsiz = 2048;
    char buf[bufsiz];
    detail::stream_line_reader reader( strm, buf, bufsiz );
    // connection has been closed on client
    if ( !reader.getline() ) {
        return false;
    }
    request req;
    req.origin_ = origin;
    response res;
    res.version_ = "HTTP/1.1";
    // request line and headers
    if ( !parse_request_line( reader.ptr(), req ) || !detail::read_headers( strm, req.headers_ ) ) {
        res.status_ = 400;
        write_response( strm, last_connection, req, res );
        return true;
    }
    if ( req.get_header_value( "Connection" ) == "close" ) {
        connection_close = true;
    }
    req.set_header( "REMOTE_ADDR", strm.get_remote_addr().c_str() );
    // body
    if ( req.method_ == "POST" || req.method_ == "PUT" || req.method_ == "PATCH" ) {
        if ( !detail::read_content( strm, req ) ) {
            res.status_ = 400;
            write_response( strm, last_connection, req, res );
            return true;
        }
        const auto& content_type = req.get_header_value( "Content-Type" );
        if ( req.get_header_value( "Content-Encoding" ) == "gzip" ) {
#ifdef __SKUTILS_HTTP_WITH_ZLIB_SUPPORT__
            detail::decompress( req.body );
#else
            res.status_ = 415;
            write_response( strm, last_connection, req, res );
            return true;
#endif
        }
        if ( !content_type.find( "application/x-www-form-urlencoded" ) ) {
            detail::parse_query_text( req.body_, req.params_ );
        } else if ( !content_type.find( "multipart/form-data" ) ) {
            std::string boundary;
            if ( !detail::parse_multipart_boundary( content_type, boundary ) ||
                 !detail::parse_multipart_formdata( boundary, req.body_, req.files_ ) ) {
                res.status_ = 400;
                write_response( strm, last_connection, req, res );
                return true;
            }
        }
    }
    if ( routing( req, res ) ) {
        if ( res.status_ == -1 )
            res.status_ = 200;
    } else
        res.status_ = 404;
    write_response( strm, last_connection, req, res );
    return true;
}

inline bool server::is_valid() const {
    return true;
}

inline bool server::read_and_close_socket( socket_t sock ) {
    return detail::read_and_close_socket( sock, keep_alive_max_count_,
        [this, sock]( stream& strm, bool last_connection, bool& connection_close ) {
            std::string origin =
                skutils::network::get_fd_name_as_url( sock, is_ssl() ? "HTTPS" : "HTTP", true );
            return process_request( origin, strm, last_connection, connection_close );
        } );
}

// HTTP client implementation
inline client::client( const char* host, int port, int timeout_milliseconds )
    : host_( host ),
      port_( port ),
      timeout_milliseconds_( timeout_milliseconds ),
      host_and_port_( host_ + ":" + std::to_string( port_ ) ) {}

inline client::~client() {}

inline bool client::is_valid() const {
    return true;
}

inline socket_t client::create_client_socket() const {
    return detail::create_socket(
        host_.c_str(), port_, [=]( socket_t sock, struct addrinfo& ai ) -> bool {
            detail::set_nonblocking( sock, true );
            auto ret = connect( sock, ai.ai_addr, static_cast< int >( ai.ai_addrlen ) );
            if ( ret < 0 ) {
                if ( detail::is_connection_error() || ( !detail::wait_until_socket_is_ready_client(
                                                          sock, timeout_milliseconds_ ) ) ) {
                    detail::close_socket( sock );
                    return false;
                }
            }

            detail::set_nonblocking( sock, false );
            return true;
        } );
}

inline bool client::read_response_line( stream& strm, response& res ) {
    const auto bufsiz = 2048;
    char buf[bufsiz];
    detail::stream_line_reader reader( strm, buf, bufsiz );
    if ( !reader.getline() ) {
        return false;
    }
    const static std::regex re( "(HTTP/1\\.[01]) (\\d+?) .*\r\n" );
    std::cmatch m;
    if ( std::regex_match( reader.ptr(), m, re ) ) {
        res.version_ = std::string( m[1] );
        res.status_ = std::stoi( std::string( m[2] ) );
    }
    return true;
}

inline bool client::send( request& req, response& res ) {
    if ( req.path_.empty() ) {
        return false;
    }
    auto sock = create_client_socket();
    if ( sock == INVALID_SOCKET ) {
        return false;
    }
    return read_and_close_socket( sock, req, res );
}

inline void client::write_request( stream& strm, request& req ) {
    buffer_stream bstrm;
    // request line
    auto path = detail::encode_url( req.path_ );
    bstrm.write_format( "%s %s HTTP/1.1\r\n", req.method_.c_str(), path.c_str() );
    // headers
    if ( !req.has_header( "Host" ) ) {
        if ( is_ssl() ) {
            if ( port_ == 443 ) {
                req.set_header( "Host", host_.c_str() );
            } else {
                req.set_header( "Host", host_and_port_.c_str() );
            }
        } else {
            if ( port_ == 80 ) {
                req.set_header( "Host", host_.c_str() );
            } else {
                req.set_header( "Host", host_and_port_.c_str() );
            }
        }
    }
    if ( !req.has_header( "Accept" ) ) {
        req.set_header( "Accept", "*/*" );
    }
    if ( !req.has_header( "User-Agent" ) ) {
        req.set_header( "User-Agent", "skutols-http/0.1" );
    }
    // TO-DO: Support KeepAlive connection
    // if (!req.has_header("Connection")) {
    req.set_header( "Connection", "close" );
    // }
    if ( req.body_.empty() ) {
        if ( req.method_ == "POST" || req.method_ == "PUT" || req.method_ == "PATCH" ) {
            req.set_header( "Content-Length", "0" );
        }
    } else {
        if ( !req.has_header( "Content-Type" ) ) {
            req.set_header( "Content-Type", "text/plain" );
        }
        if ( !req.has_header( "Content-Length" ) ) {
            auto length = std::to_string( req.body_.size() );
            req.set_header( "Content-Length", length.c_str() );
        }
    }
    detail::write_headers( bstrm, req );
    // body
    if ( !req.body_.empty() ) {
        bstrm.write( req.body_.c_str(), req.body_.size() );
    }
    // flush buffer
    auto& data = bstrm.get_buffer();
    strm.write( data.data(), data.size() );
}

inline bool client::process_request( const std::string& /*origin*/, stream& strm, request& req,
    response& res, bool& connection_close ) {
    // send request
    write_request( strm, req );
    // receive response and headers
    if ( !read_response_line( strm, res ) || !detail::read_headers( strm, res.headers_ ) ) {
        return false;
    }

    if ( res.get_header_value( "Connection" ) == "close" || res.version_ == "HTTP/1.0" ) {
        connection_close = true;
    }
    // body
    if ( req.method_ != "HEAD" ) {
        if ( !detail::read_content( strm, res, req.progress_ ) ) {
            return false;
        }
        if ( res.get_header_value( "Content-Encoding" ) == "gzip" ) {
#ifdef __SKUTILS_HTTP_WITH_ZLIB_SUPPORT__
            detail::decompress( res.body );
#else
            return false;
#endif
        }
    }
    return true;
}

inline bool client::read_and_close_socket( socket_t sock, request& req, response& res ) {
    std::string origin =
        skutils::network::get_fd_name_as_url( sock, is_ssl() ? "HTTPS" : "HTTP", false );
    return detail::read_and_close_socket( sock, 0,
        [&, origin]( stream& strm, bool,  // last_connection
            bool& connection_close ) {
            return process_request( origin, strm, req, res, connection_close );
        } );
}

inline std::shared_ptr< response > client::Get( const char* path, fn_progress progress ) {
    return Get( path, map_headers(), progress );
}

inline std::shared_ptr< response > client::Get(
    const char* path, const map_headers& headers, fn_progress progress ) {
    request req;
    req.method_ = "GET";
    req.path_ = path;
    req.headers_ = headers;
    req.progress_ = progress;
    auto res = std::make_shared< response >();
    return send( req, *res ) ? res : nullptr;
}

inline std::shared_ptr< response > client::Head( const char* path ) {
    return Head( path, map_headers() );
}

inline std::shared_ptr< response > client::Head( const char* path, const map_headers& headers ) {
    request req;
    req.method_ = "HEAD";
    req.headers_ = headers;
    req.path_ = path;
    auto res = std::make_shared< response >();
    return send( req, *res ) ? res : nullptr;
}

inline std::shared_ptr< response > client::Post(
    const char* path, const std::string& body, const char* content_type ) {
    return Post( path, map_headers(), body, content_type );
}

inline std::shared_ptr< response > client::Post( const char* path, const map_headers& headers,
    const std::string& body, const char* content_type ) {
    request req;
    req.method_ = "POST";
    req.headers_ = headers;
    req.path_ = path;
    req.headers_.emplace( "Content-Type", content_type );
    req.body_ = body;
    auto res = std::make_shared< response >();
    return send( req, *res ) ? res : nullptr;
}

inline std::shared_ptr< response > client::Post( const char* path, const map_params& params ) {
    return Post( path, map_headers(), params );
}

inline std::shared_ptr< response > client::Post(
    const char* path, const map_headers& headers, const map_params& params ) {
    std::string query;
    for ( auto it = params.begin(); it != params.end(); ++it ) {
        if ( it != params.begin() ) {
            query += "&";
        }
        query += it->first;
        query += "=";
        query += it->second;
    }
    return Post( path, headers, query, "application/x-www-form-urlencoded" );
}

inline std::shared_ptr< response > client::Put(
    const char* path, const std::string& body, const char* content_type ) {
    return Put( path, map_headers(), body, content_type );
}

inline std::shared_ptr< response > client::Put( const char* path, const map_headers& headers,
    const std::string& body, const char* content_type ) {
    request req;
    req.method_ = "PUT";
    req.headers_ = headers;
    req.path_ = path;
    req.headers_.emplace( "Content-Type", content_type );
    req.body_ = body;
    auto res = std::make_shared< response >();
    return send( req, *res ) ? res : nullptr;
}

inline std::shared_ptr< response > client::Patch(
    const char* path, const std::string& body, const char* content_type ) {
    return Patch( path, map_headers(), body, content_type );
}

inline std::shared_ptr< response > client::Patch( const char* path, const map_headers& headers,
    const std::string& body, const char* content_type ) {
    request req;
    req.method_ = "PATCH";
    req.headers_ = headers;
    req.path_ = path;
    req.headers_.emplace( "Content-Type", content_type );
    req.body_ = body;
    auto res = std::make_shared< response >();
    return send( req, *res ) ? res : nullptr;
}

inline std::shared_ptr< response > client::Delete( const char* path ) {
    return Delete( path, map_headers() );
}

inline std::shared_ptr< response > client::Delete( const char* path, const map_headers& headers ) {
    request req;
    req.method_ = "DELETE";
    req.path_ = path;
    req.headers_ = headers;
    auto res = std::make_shared< response >();
    return send( req, *res ) ? res : nullptr;
}

inline std::shared_ptr< response > client::Options( const char* path ) {
    return Options( path, map_headers() );
}

inline std::shared_ptr< response > client::Options( const char* path, const map_headers& headers ) {
    request req;
    req.method_ = "OPTIONS";
    req.path_ = path;
    req.headers_ = headers;
    auto res = std::make_shared< response >();
    return send( req, *res ) ? res : nullptr;
}

/// SSL Implementation

namespace detail {

template < typename U, typename V, typename T >
inline bool read_and_close_socket_ssl( socket_t sock, size_t keep_alive_max_count,
    // TO-DO: OpenSSL 1.0.2 occasionally crashes...
    // The upcoming 1.1.0 is going to be thread safe.
    SSL_CTX* ctx, std::mutex& ctx_mutex, U SSL_connect_or_accept, V setup, T callback ) {
    SSL* ssl = nullptr;
    {
        std::lock_guard< std::mutex > guard( ctx_mutex );
        ssl = SSL_new( ctx );
        if ( !ssl ) {
            return false;
        }
    }
    auto bio = BIO_new_socket( sock, BIO_NOCLOSE );
    SSL_set_bio( ssl, bio, bio );
    setup( ssl );
    SSL_connect_or_accept( ssl );
    bool ret = false;
    if ( keep_alive_max_count > 0 ) {
        auto count = keep_alive_max_count;
        while ( count > 0 &&
                detail::poll_read( sock, __SKUTILS_HTTP_KEEPALIVE_TIMEOUT_MILLISECONDS__ ) ) {
            SSL_socket_stream strm( sock, ssl );
            auto last_connection = count == 1;
            auto connection_close = false;
            ret = callback( strm, last_connection, connection_close );
            if ( !ret || connection_close ) {
                break;
            }
            count--;
        }
    } else {
        SSL_socket_stream strm( sock, ssl );
        auto dummy_connection_close = false;
        ret = callback( strm, true, dummy_connection_close );
    }
    SSL_shutdown( ssl );
    {
        std::lock_guard< std::mutex > guard( ctx_mutex );
        SSL_free( ssl );
    }
    close_socket( sock );
    return ret;
}

class SSLInit {
public:
    SSLInit() {
        SSL_load_error_strings();
        SSL_library_init();
    }
    ~SSLInit() { ERR_free_strings(); }
};  /// class SSLInit

static SSLInit sslinit_;

};  // namespace detail

/// SSL socket stream implementation

inline SSL_socket_stream::SSL_socket_stream( socket_t sock, SSL* ssl )
    : sock_( sock ), ssl_( ssl ) {}

inline SSL_socket_stream::~SSL_socket_stream() {}

inline int SSL_socket_stream::read( char* ptr, size_t size ) {
    return SSL_read( ssl_, ptr, size );
}

inline int SSL_socket_stream::write( const char* ptr, size_t size ) {
    return SSL_write( ssl_, ptr, size );
}

inline int SSL_socket_stream::write( const char* ptr ) {
    return write( ptr, strlen( ptr ) );
}

inline std::string SSL_socket_stream::get_remote_addr() const {
    return detail::get_remote_addr( sock_ );
}

/// SSL HTTP server implementation

inline SSL_server::SSL_server( const char* cert_path, const char* private_key_path ) {
    ctx_ = SSL_CTX_new( SSLv23_server_method() );

    if ( ctx_ ) {
        SSL_CTX_set_options( ctx_, SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
                                       SSL_OP_NO_COMPRESSION |
                                       SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION );

        // auto ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
        // SSL_CTX_set_tmp_ecdh(ctx_, ecdh);
        // EC_KEY_free(ecdh);

        if ( SSL_CTX_use_certificate_chain_file( ctx_, cert_path ) != 1 ||
             SSL_CTX_use_PrivateKey_file( ctx_, private_key_path, SSL_FILETYPE_PEM ) != 1 ) {
            SSL_CTX_free( ctx_ );
            ctx_ = nullptr;
        }
    }
}

inline SSL_server::~SSL_server() {
    if ( ctx_ ) {
        SSL_CTX_free( ctx_ );
    }
}

inline bool SSL_server::is_valid() const {
    return ctx_;
}

inline bool SSL_server::read_and_close_socket( socket_t sock ) {
    std::string origin =
        skutils::network::get_fd_name_as_url( sock, is_ssl() ? "HTTPS" : "HTTP", true );
    return detail::read_and_close_socket_ssl( sock, keep_alive_max_count_, ctx_, ctx_mutex_,
        SSL_accept,
        [origin]( SSL*  // ssl
        ) {},
        [this, origin]( stream& strm, bool last_connection, bool& connection_close ) {
            return process_request( origin, strm, last_connection, connection_close );
        } );
}

/// SSL HTTP client implementation
///
inline SSL_client::SSL_client( const char* host, int port, int timeout_milliseconds )
    : client( host, port, timeout_milliseconds ) {
    ctx_ = SSL_CTX_new( SSLv23_client_method() );
}

inline SSL_client::~SSL_client() {
    if ( ctx_ ) {
        SSL_CTX_free( ctx_ );
    }
}

inline bool SSL_client::is_valid() const {
    return ctx_;
}

inline bool SSL_client::read_and_close_socket( socket_t sock, request& req, response& res ) {
    std::string origin =
        skutils::network::get_fd_name_as_url( sock, is_ssl() ? "HTTPS" : "HTTP", false );
    return is_valid() && detail::read_and_close_socket_ssl( sock, 0, ctx_, ctx_mutex_, SSL_connect,
                             [&]( SSL* ssl ) { SSL_set_tlsext_host_name( ssl, host_.c_str() ); },
                             [&, origin]( stream& strm, bool,  // last_connection
                                 bool& connection_close ) {
                                 return process_request( origin, strm, req, res, connection_close );
                             } );
}

};  // namespace http
};  // namespace skutils

#endif  /// SKUTILS_HTTP_H
