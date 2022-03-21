#include <skutils/http.h>

#if ( !defined _WIN32 )

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <curl/curl.h>

#endif  // (!defined _WIN32)

//#define __SKUTILS_HTTP_DEBUG_CONSOLE_TRACE_HTTP_TASK_STATES__ 1

namespace skutils {
namespace http {

static const char g_strCrLf[] = "\r\n";
static const size_t g_nSizeOfCrLf = 2;
static const char g_strDash[] = "--";
static const size_t g_nSizeOfDash = 2;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace detail {

int shutdown_socket( socket_t sock ) {
#ifdef _WIN32
    return shutdown( sock, SD_BOTH );
#else
    return shutdown( sock, SHUT_RDWR );
#endif
}

void auto_detect_ipVer( int& ipVer, const char* host ) {
    if ( ipVer == 4 || ipVer == 6 )
        return;
    if ( host == nullptr || host[0] == '\0' ) {
        ipVer = 4;
        return;
    }
    std::string s;
    s = host;
    if ( skutils::is_valid_ipv6( s ) ) {
        ipVer = 6;
        return;
    }
    ipVer = 4;
}

int close_socket( socket_t sock ) {
#ifdef _WIN32
    return closesocket( sock );
#else
    return close( sock );
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

stream_line_reader::stream_line_reader( stream& strm, char* fixed_buffer, size_t fixed_buffer_size )
    : strm_( strm ), fixed_buffer_( fixed_buffer ), fixed_buffer_size_( fixed_buffer_size ) {}
stream_line_reader::~stream_line_reader() {}

const char* stream_line_reader::ptr() const {
    if ( glowable_buffer_.empty() ) {
        return fixed_buffer_;
    } else {
        return glowable_buffer_.data();
    }
}

bool stream_line_reader::getline() {
    fixed_buffer_used_size_ = 0;
    glowable_buffer_.clear();
    for ( size_t i = 0;; i++ ) {
        char byte = 0;
        int n = strm_.read( &byte, sizeof( byte ) );
        if ( n < 1 ) {
            if ( n < 0 )
                return false;
            if ( n == 0 ) {
                if ( i == 0 )
                    return false;
                break;
            }
        }
        append( byte );
        if ( byte == '\n' )
            break;
    }
    return true;
}

void stream_line_reader::append( char c ) {
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int poll_impl( socket_t sock, short which_poll, int timeout_milliseconds ) {
    struct pollfd fds[1];
    int nfds = 1;
    fds[0].fd = sock;
    fds[0].events = which_poll;
    int rc = poll( fds, nfds, timeout_milliseconds );
    return rc;
}
bool poll_read( socket_t sock, int timeout_milliseconds ) {
    return ( poll_impl( sock, POLLIN, timeout_milliseconds ) > 0 ) ? true : false;
}
bool poll_write( socket_t sock, int timeout_milliseconds ) {
    return ( poll_impl( sock, POLLOUT, timeout_milliseconds ) > 0 ) ? true : false;
}

bool is_handle_a_socket( socket_t fd ) {
    struct stat statbuf;
    fstat(fd, &statbuf);
    bool bIsSocket = ( S_ISSOCK(statbuf.st_mode) ) ? true : false;
    return bIsSocket;
}

bool wait_until_socket_is_ready_client( socket_t sock, int timeout_milliseconds ) {
    if( ! is_handle_a_socket( sock ) )
        return false;

    //
    // TO-DO: l_sergiy: switch HTTP/client to poll() later
    //
    //    if ( poll_read( sock, timeout_milliseconds ) || poll_write( sock,
    //    timeout_milliseconds ) )
    //    {
    //        int error = 0;
    //        socklen_t len = sizeof( error );
    //        if ( getsockopt( sock, SOL_SOCKET, SO_ERROR, ( char* ) &error, &len
    //        ) < 0 || error )
    //            return false;
    //    } else
    //        return false;
    //    return true;

    //    fd_set fdsr;
    //    FD_ZERO( &fdsr );
    //    FD_SET( sock, &fdsr );
    //    auto fdsw = fdsr;
    //    auto fdse = fdsr;
    //    timeval tv;
    //    tv.tv_sec = static_cast< long >( timeout_milliseconds / 1000 );
    //    tv.tv_usec = static_cast< long >( ( timeout_milliseconds % 1000 ) * 1000 );
    //    if ( select( static_cast< int >( sock + 1 ), &fdsr, &fdsw, &fdse, &tv ) < 0 )
    //        return false;
    //    if ( FD_ISSET( sock, &fdsr ) || FD_ISSET( sock, &fdsw ) ) {
    //        int error = 0;
    //        socklen_t len = sizeof( error );
    //        if ( getsockopt( sock, SOL_SOCKET, SO_ERROR, ( char* ) &error, &len ) < 0 || error )
    //            return false;
    //    } else
    //        return false;
    //    return true;

    struct pollfd fds;
    fds.fd = sock;
    fds.events = POLLIN | POLLOUT;
    for( ; true; ) {
        int r = poll( &fds, 1, timeout_milliseconds );
        if( r == -1 ) {
            if( errno == EINTR )
                continue;
            // call to poll() failed
            break;
        } else if( r == 0 ) {
            // timeout expired
            break;
        } else if( fds.revents & ( POLLIN | POLLOUT ) ) {
            return true;
        } else if( fds.revents & ( POLLERR | POLLNVAL ) ) {
            // socket error
            break;
        }
    } // for( ; true; )
    return false;
}

template < typename T >
bool read_and_close_socket( socket_t sock, size_t keep_alive_max_count, T callback,
    common_network_exception::error_info& ei ) {
    ei.clear();
    bool ret = false;
    try {
        if( ! detail::is_handle_a_socket( sock ) )
            throw std::runtime_error( "cannot read(and close) broken socket handle(1)" );
        if ( keep_alive_max_count > 0 ) {
            size_t cnt = keep_alive_max_count;
            for ( ; cnt > 0; --cnt ) {
                if( ! detail::is_handle_a_socket( sock ) )
                    throw std::runtime_error( "cannot read(and close) broken socket handle(2)" );
                if ( !detail::poll_read( sock, __SKUTILS_HTTP_KEEPALIVE_TIMEOUT_MILLISECONDS__ ) )
                    continue;
                socket_stream strm( sock );
                auto last_connection = ( cnt == 1 ) ? true : false;
                auto connection_close = false;
                ret = callback( strm, last_connection, connection_close );
                if ( !ret ) {
                    ei.et_ = common_network_exception::error_type::et_unknown;
                    ei.ec_ = errno;
                    ei.strError_ = "data transfer error";
                }
                if ( ( !ret ) || connection_close )
                    break;
            }
        } else {
            socket_stream strm( sock );
            auto dummy_connection_close = false;
            ret = callback( strm, true, dummy_connection_close );
            if ( !ret ) {
                ei.et_ = common_network_exception::error_type::et_unknown;
                ei.ec_ = errno;
                ei.strError_ = "data transfer error";
            }
        }
        if ( ret )
            ei.clear();
    } catch ( common_network_exception& ex ) {
        ei = ex.ei_;
        if ( ei.strError_.empty() )
            ei.strError_ = "exception without description";
        if ( ei.et_ == common_network_exception::error_type::et_no_error )
            ei.et_ = common_network_exception::error_type::et_unknown;
    } catch ( std::exception& ex ) {
        ei.strError_ = ex.what();
        if ( ei.strError_.empty() )
            ei.strError_ = "exception without description";
        if ( ei.et_ == common_network_exception::error_type::et_no_error )
            ei.et_ = common_network_exception::error_type::et_unknown;
    } catch ( ... ) {
        ei.strError_ = "unknown exception";
        if ( ei.et_ == common_network_exception::error_type::et_no_error )
            ei.et_ = common_network_exception::error_type::et_unknown;
    }
    close_socket( sock );
    return ret;
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

template < typename Fn >
socket_t create_socket6( const char* host, int port, Fn fn, int socket_flags = 0,
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

void set_nonblocking( socket_t sock, bool nonblocking ) {
    if( ! is_handle_a_socket( sock ) )
        return;
#ifdef _WIN32
    auto flags = nonblocking ? 1UL : 0UL;
    ioctlsocket( sock, FIONBIO, &flags );
#else
    auto flags = fcntl( sock, F_GETFL, 0 );
    fcntl( sock, F_SETFL, nonblocking ? ( flags | O_NONBLOCK ) : ( flags & ( ~O_NONBLOCK ) ) );
#endif
}

bool is_connection_error() {
#ifdef _WIN32
    return WSAGetLastError() != WSAEWOULDBLOCK;
#else
    return errno != EINPROGRESS;
#endif
}

std::string get_remote_addr( socket_t sock ) {
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

bool is_file( const std::string& path ) {
    struct stat st;
    return stat( path.c_str(), &st ) >= 0 && S_ISREG( st.st_mode );
}

bool is_dir( const std::string& path ) {
    struct stat st;
    return stat( path.c_str(), &st ) >= 0 && S_ISDIR( st.st_mode );
}

bool is_valid_path( const std::string& path ) {
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

void read_file( const std::string& path, std::string& out ) {
    std::ifstream fs( path, std::ios_base::binary );
    fs.seekg( 0, std::ios_base::end );
    auto size = fs.tellg();
    fs.seekg( 0 );
    out.resize( static_cast< size_t >( size ) );
    fs.read( &out[0], size );
}

std::string file_extension( const std::string& path ) {
    std::smatch m;
    auto pat = std::regex( "\\.([a-zA-Z0-9]+)$" );
    if ( std::regex_search( path, m, pat ) )
        return m[1].str();
    return std::string();
}

const char* find_content_type( const std::string& path ) {
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

const char* status_message( int status ) {
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

bool has_header( const map_headers& headers, const char* key ) {
    return headers.find( key ) != headers.end();
}

const char* get_header_value(
    const map_headers& headers, const char* key, size_t id = 0, const char* def = nullptr ) {
    auto it = headers.find( key );
    std::advance( it, id );
    if ( it != headers.end() )
        return it->second.c_str();
    return def;
}

int get_header_value_int( const map_headers& headers, const char* key, int def = 0 ) {
    auto it = headers.find( key );
    if ( it != headers.end() )
        return std::stoi( it->second );
    return def;
}

bool read_headers( stream& strm, map_headers& headers ) {
    static std::regex re( R"((.+?):\s*(.+?)\s*\r\n)" );
    const auto bufsiz = 2048;
    char buf[bufsiz];
    stream_line_reader reader( strm, buf, bufsiz );
    for ( ;; ) {
        if ( !reader.getline() )
            return false;
        if ( !strcmp( reader.ptr(), g_strCrLf ) )
            break;
        try {
            std::cmatch m;
            if ( std::regex_match( reader.ptr(), m, re ) ) {
                auto key = std::string( m[1] );
                auto val = std::string( m[2] );
                headers.emplace( key, val );
            }
        } catch ( const std::exception& ex ) {
            std::string sw = ex.what();
            if ( sw.empty() )
                sw = "unknown error";
            std::cerr.flush();
            std::cerr << "HTTP server got \"" << sw
                      << "\" exception and failed to parse headers line: " << reader.ptr() << "\n";
            std::cerr.flush();
            return false;
        } catch ( ... ) {
            std::cerr.flush();
            std::cerr << "HTTP server failed to parse headers line: " << reader.ptr() << "\n";
            std::cerr.flush();
            return false;
        }
    }
    return true;
}

bool read_content_with_length( stream& strm, std::string& out, size_t len, fn_progress progress ) {
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

bool read_content_without_length( stream& strm, std::string& out ) {
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

bool read_content_chunked( stream& strm, std::string& out ) {
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
        if ( strcmp( reader.ptr(), g_strCrLf ) )
            break;
        out += chunk;
        if ( !reader.getline() )
            return false;
        chunk_len = std::stoi( reader.ptr(), 0, 16 );
    }
    if ( chunk_len == 0 ) {
        // reader terminator after chunks
        if ( !reader.getline() || strcmp( reader.ptr(), g_strCrLf ) )
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
void write_headers( stream& strm, const T& info ) {
    for ( const auto& x : info.headers_ ) {
        strm.write_format( "%s: %s\r\n", x.first.c_str(), x.second.c_str() );
    }
    strm.write( g_strCrLf, g_nSizeOfCrLf );
}

std::string encode_url( const std::string& s ) {
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

bool is_hex( char c, int& v ) {
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

bool from_hex_to_i( const std::string& s, size_t i, size_t cnt, int& val ) {
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

std::string from_i_to_hex( uint64_t n ) {
    const char* charset = "0123456789abcdef";
    std::string ret;
    do {
        ret = charset[n & 15] + ret;
        n >>= 4;
    } while ( n > 0 );
    return ret;
}

size_t to_utf8( int code, char* buff ) {
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

std::string decode_url( const std::string& s ) {
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

void parse_query_text( const std::string& s, map_params& params ) {
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

bool parse_multipart_boundary( const std::string& content_type, std::string& boundary ) {
    auto pos = content_type.find( "boundary=" );
    if ( pos == std::string::npos ) {
        return false;
    }

    boundary = content_type.substr( pos + 9 );
    return true;
}

bool parse_multipart_formdata(
    const std::string& boundary, const std::string& body, multipart_files& files ) {
    static std::regex re_content_type( "Content-Type: (.*?)", std::regex_constants::icase );

    static std::regex re_content_disposition(
        "Content-Disposition: form-data; name=\"(.*?)\"(?:; filename=\"(.*?)\")?",
        std::regex_constants::icase );

    auto dash_boundary = std::string( g_strDash ) + boundary;

    auto pos = body.find( dash_boundary );
    if ( pos != 0 ) {
        return false;
    }

    pos += dash_boundary.size();

    auto next_pos = body.find( g_strCrLf, pos );
    if ( next_pos == std::string::npos ) {
        return false;
    }

    pos = next_pos + g_nSizeOfCrLf;

    while ( pos < body.size() ) {
        next_pos = body.find( g_strCrLf, pos );
        if ( next_pos == std::string::npos ) {
            return false;
        }

        std::string name;
        multipart_file file;

        auto header = body.substr( pos, ( next_pos - pos ) );

        while ( pos != next_pos ) {
            try {
                std::smatch m;
                if ( std::regex_match( header, m, re_content_type ) ) {
                    file.content_type_ = m[1];
                } else if ( std::regex_match( header, m, re_content_disposition ) ) {
                    name = m[1];
                    file.filename_ = m[2];
                }
            } catch ( const std::exception& ex ) {
                std::string sw = ex.what();
                if ( sw.empty() )
                    sw = "unknown error";
                std::cerr.flush();
                std::cerr << "HTTP server got \"" << sw
                          << "\" exception and failed to parse multipart line: " << header << "\n";
                std::cerr.flush();
                return false;
            } catch ( ... ) {
                std::cerr.flush();
                std::cerr << "HTTP server failed to parse multipart line: " << header << "\n";
                std::cerr.flush();
                return false;
            }

            pos = next_pos + g_nSizeOfCrLf;

            next_pos = body.find( g_strCrLf, pos );
            if ( next_pos == std::string::npos ) {
                return false;
            }

            header = body.substr( pos, ( next_pos - pos ) );
        }

        pos = next_pos + g_nSizeOfCrLf;

        next_pos = body.find( std::string( g_strCrLf ) + dash_boundary, pos );

        if ( next_pos == std::string::npos ) {
            return false;
        }

        file.offset_ = pos;
        file.length_ = next_pos - pos;

        pos = next_pos + g_nSizeOfCrLf + dash_boundary.size();

        next_pos = body.find( g_strCrLf, pos );
        if ( next_pos == std::string::npos ) {
            return false;
        }

        files.emplace( name, file );

        pos = next_pos + g_nSizeOfCrLf;
    }

    return true;
}

std::string to_lower( const char* beg, const char* end ) {
    std::string out;
    auto it = beg;
    while ( it != end ) {
        out += ::tolower( *it );
        it++;
    }
    return out;
}

void make_range_header_core( std::string& ) {}

template < typename uint64_t >
void make_range_header_core( std::string& field, uint64_t value ) {
    if ( !field.empty() ) {
        field += ", ";
    }
    field += std::to_string( value ) + "-";
}

template < typename uint64_t, typename... Args >
void make_range_header_core( std::string& field, uint64_t value1, uint64_t value2, Args... args ) {
    if ( !field.empty() ) {
        field += ", ";
    }
    field += std::to_string( value1 ) + "-" + std::to_string( value2 );
    make_range_header_core( field, args... );
}

#ifdef __SKUTILS_HTTP_WITH_ZLIB_SUPPORT__
bool can_compress( const std::string& content_type ) {
    return !content_type.find( "text/" ) || content_type == "image/svg+xml" ||
           content_type == "application/javascript" || content_type == "application/json" ||
           content_type == "application/xml" || content_type == "application/xhtml+xml";
}

void compress( std::string& content ) {
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

void decompress( std::string& content ) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    // 15 is the value of wbits, which should be at the maximum possible value to
    // ensure that any gzip stream can be decoded. The offset of 16 specifies that
    // the stream to decompress will be formatted with a gzip wrapper.
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

};  // namespace detail

template < typename uint64_t, typename... Args >
std::pair< std::string, std::string > make_range_header( uint64_t value, Args... args ) {
    std::string field;
    detail::make_range_header_core( field, value, args... );
    field.insert( 0, "bytes=" );
    return std::make_pair( "Range", field );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

request::request() {}
request::~request() {}

bool request::has_header( const char* key ) const {
    return detail::has_header( headers_, key );
}

std::string request::get_header_value( const char* key, size_t id ) const {
    return detail::get_header_value( headers_, key, id, "" );
}

size_t request::get_header_value_count( const char* key ) const {
    auto r = headers_.equal_range( key );
    return std::distance( r.first, r.second );
}

void request::set_header( const char* key, const char* val ) {
    headers_.emplace( key, val );
}

bool request::has_param( const char* key ) const {
    return params_.find( key ) != params_.end();
}

std::string request::get_param_value( const char* key, size_t id ) const {
    auto it = params_.find( key );
    std::advance( it, id );
    if ( it != params_.end() ) {
        return it->second;
    }
    return std::string();
}

size_t request::get_param_value_count( const char* key ) const {
    auto r = params_.equal_range( key );
    return std::distance( r.first, r.second );
}

bool request::has_file( const char* key ) const {
    return files_.find( key ) != files_.end();
}

multipart_file request::get_file_value( const char* key ) const {
    auto it = files_.find( key );
    if ( it != files_.end() ) {
        return it->second;
    }
    return multipart_file();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

response::response() {}
response::~response() {}

bool response::has_header( const char* key ) const {
    return headers_.find( key ) != headers_.end();
}

std::string response::get_header_value( const char* key, size_t id ) const {
    return detail::get_header_value( headers_, key, id, "" );
}

size_t response::get_header_value_count( const char* key ) const {
    auto r = headers_.equal_range( key );
    return std::distance( r.first, r.second );
}

void response::set_header( const char* key, const char* val ) {
    headers_.emplace( key, val );
}

void response::set_redirect( const char* url ) {
    set_header( "Location", url );
    status_ = 302;
}

void response::set_content( const char* s, size_t n, const char* content_type ) {
    body_.assign( s, n );
    set_header( "Content-Type", content_type );
}

void response::set_content( const std::string& s, const char* content_type ) {
    body_ = s;
    set_header( "Content-Type", content_type );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

stream::stream() {}
stream::~stream() {}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

socket_stream::socket_stream( socket_t sock ) : sock_( sock ) {}

socket_stream::~socket_stream() {}

int socket_stream::read( char* ptr, size_t size ) {
    if ( ptr == nullptr || size == 0 )
        return 0;
    return recv( sock_, ptr, static_cast< int >( size ), 0 );
}

int socket_stream::write( const char* ptr, size_t size ) {
    return send( sock_, ptr, static_cast< int >( size ), 0 );
}

std::string socket_stream::get_remote_addr() const {
    return detail::get_remote_addr( sock_ );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

buffer_stream::buffer_stream() {}
buffer_stream::~buffer_stream() {}

int buffer_stream::read( char* ptr, size_t size ) {
    if ( ptr == nullptr || size == 0 )
        return 0;
#if defined( _MSC_VER ) && _MSC_VER < 1900
    return static_cast< int >( buffer_._Copy_s( ptr, size, size ) );
#else
    return static_cast< int >( buffer_.copy( ptr, size ) );
#endif
}

int buffer_stream::write( const char* ptr, size_t size ) {
    if ( ptr == nullptr || size == 0 )
        return 0;
    buffer_.append( ptr, size );
    return static_cast< int >( size );
}

std::string buffer_stream::get_remote_addr() const {
    return "";
}

const std::string& buffer_stream::get_buffer() const {
    return buffer_;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SSL_socket_stream::SSL_socket_stream( socket_t sock, SSL* ssl ) : sock_( sock ), ssl_( ssl ) {}

SSL_socket_stream::~SSL_socket_stream() {}

int SSL_socket_stream::read( char* ptr, size_t size ) {
    if ( ptr == nullptr || size == 0 )
        return 0;
    return SSL_read( ssl_, ptr, size );
}

int SSL_socket_stream::write( const char* ptr, size_t size ) {
    if ( ptr == nullptr || size == 0 )
        return 0;
    return SSL_write( ssl_, ptr, size );
}

std::string SSL_socket_stream::get_remote_addr() const {
    return detail::get_remote_addr( sock_ );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace detail {

std::string SSL_extract_current_error_as_text() {
    BIO* bio = BIO_new( BIO_s_mem() );
    if ( !bio )
        return "";
    ERR_print_errors( bio );
    char* buf;
    size_t len = BIO_get_mem_data( bio, &buf );
    std::string strErrorDescription( buf, len );
    BIO_free( bio );
    return strErrorDescription;
}

void SSL_accept_wrapper( SSL* ssl ) {
    auto rv = SSL_accept( ssl );
    if ( rv == 1 )
        return;
    common_network_exception::error_info ei;
    ei.et_ = common_network_exception::error_type::et_ssl_fatal;
    ei.ec_ = SSL_get_error( ssl, rv );
    ei.strError_ =
        skutils::tools::format( "SSL_accept() error, returned %d=0x%X", int( rv ), int( rv ) );
    std::string strErrorDescription = SSL_extract_current_error_as_text();
    if ( !strErrorDescription.empty() )
        ei.strError_ += ", error details: " + strErrorDescription;
    throw common_network_exception( ei );
}

void SSL_connect_wrapper( SSL* ssl ) {
    auto rv = SSL_connect( ssl );
    if ( rv == 1 )
        return;
    common_network_exception::error_info ei;
    ei.et_ = common_network_exception::error_type::et_ssl_fatal;
    ei.ec_ = SSL_get_error( ssl, rv );
    ei.strError_ =
        skutils::tools::format( "SSL_connect() error, returned %d=0x%X", int( rv ), int( rv ) );
    std::string strErrorDescription = SSL_extract_current_error_as_text();
    if ( !strErrorDescription.empty() )
        ei.strError_ += ", error details: " + strErrorDescription;
    throw common_network_exception( ei );
}

template < typename U, typename V, typename T >
bool read_and_close_socket_ssl( socket_t sock, size_t keep_alive_max_count,
    // TO-DO: OpenSSL 1.0.2 occasionally crashes...
    // The upcoming 1.1.0 is going to be thread safe.
    SSL_CTX* ctx, std::mutex& ctx_mutex, U SSL_connect_or_accept, V setup, T callback,
    common_network_exception::error_info& ei ) {
    ei.clear();
    SSL* ssl = nullptr;
    bool ret = false;
    try {
        {
            std::lock_guard< std::mutex > guard( ctx_mutex );
            ssl = SSL_new( ctx );
            if ( !ssl ) {
                std::string strError = "Failed to allocate SSL context";
                std::string strErrorDescription = SSL_extract_current_error_as_text();
                if ( !strErrorDescription.empty() )
                    strError += ", error details: " + strErrorDescription;
                ei.et_ = common_network_exception::error_type::et_ssl_fatal;
                ei.strError_ = strError;
                throw common_network_exception( ei );
            }
        }
        auto bio = BIO_new_socket( sock, BIO_NOCLOSE );
        SSL_set_bio( ssl, bio, bio );
        setup( ssl );
        ei.et_ = common_network_exception::error_type::et_ssl_error;  // assume it's et_ssl_error
                                                                      // for a while
        SSL_connect_or_accept( ssl );
        ei.et_ = common_network_exception::error_type::et_no_error;
        if ( keep_alive_max_count > 0 ) {
            size_t cnt = keep_alive_max_count;
            for ( ; cnt > 0; --cnt ) {
                if ( !detail::poll_read( sock, __SKUTILS_HTTP_KEEPALIVE_TIMEOUT_MILLISECONDS__ ) )
                    continue;
                SSL_socket_stream strm( sock, ssl );
                auto last_connection = ( cnt == 1 ) ? true : false;
                auto connection_close = false;
                ret = callback( strm, last_connection, connection_close );
                if ( !ret ) {
                    ei.et_ = common_network_exception::error_type::et_unknown;
                    ei.ec_ = errno;
                    ei.strError_ = "data transfer error";
                }
                if ( ( !ret ) || connection_close ) {
                    break;
                }
            }
        } else {
            SSL_socket_stream strm( sock, ssl );
            auto dummy_connection_close = false;
            ret = callback( strm, true, dummy_connection_close );
            if ( !ret ) {
                ei.et_ = common_network_exception::error_type::et_unknown;
                ei.ec_ = errno;
                ei.strError_ = "data transfer error";
            }
        }
        if ( ret )
            ei.clear();
    } catch ( common_network_exception& ex ) {
        ei = ex.ei_;
        if ( ei.strError_.empty() )
            ei.strError_ = "exception without description";
        if ( ei.et_ == common_network_exception::error_type::et_no_error )
            ei.et_ = common_network_exception::error_type::et_unknown;
    } catch ( std::exception& ex ) {
        ei.strError_ = ex.what();
        if ( ei.strError_.empty() )
            ei.strError_ = "exception without description";
        if ( ei.et_ == common_network_exception::error_type::et_no_error )
            ei.et_ = common_network_exception::error_type::et_unknown;
    } catch ( ... ) {
        ei.strError_ = "unknown exception";
        if ( ei.et_ == common_network_exception::error_type::et_no_error )
            ei.et_ = common_network_exception::error_type::et_unknown;
    }
    if ( ssl ) {
        SSL_shutdown( ssl );
        std::lock_guard< std::mutex > guard( ctx_mutex );
        SSL_free( ssl );
    }
    close_socket( sock );
    if ( !ei.strError_.empty() ) {
        ret = false;
        // throw std::runtime_error( strErrorDescription );
    }
    return ret;
}

SSLInit::SSLInit() {
    SSL_load_error_strings();
    SSL_library_init();
}

SSLInit::~SSLInit() {
    ERR_free_strings();
}

};  // namespace detail

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

async_query_handler::async_query_handler( server& srv ) : srv_( srv ) {
#if ( defined __SKUTILS_HTTP_DEBUG_CONSOLE_TRACE_HTTP_TASK_STATES__ )
    std::cout << skutils::tools::format( "http task ctor %p\n", this );
    std::cout.flush();
#endif
}
async_query_handler::~async_query_handler() {
#if ( defined __SKUTILS_HTTP_DEBUG_CONSOLE_TRACE_HTTP_TASK_STATES__ )
    std::cout << skutils::tools::format( "http task dtor %p\n", this );
    std::cout.flush();
#endif
    remove_this_task();
}

void async_query_handler::was_added() {
#if ( defined __SKUTILS_HTTP_DEBUG_CONSOLE_TRACE_HTTP_TASK_STATES__ )
    std::cout << skutils::tools::format( "http task add %p\n", this );
    std::cout.flush();
#endif
}
void async_query_handler::will_remove() {
#if ( defined __SKUTILS_HTTP_DEBUG_CONSOLE_TRACE_HTTP_TASK_STATES__ )
    std::cout << skutils::tools::format( "http task remove %p\n", this );
    std::cout.flush();
#endif
}

bool async_query_handler::remove_this_task() {
    return srv_.remove_task( task_ptr_t( this ) );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
async_read_and_close_socket_base::async_read_and_close_socket_base( server& srv, socket_t socket,
    size_t poll_ms, size_t retry_count, size_t retry_after_ms, size_t retry_first_ms )
    : async_query_handler( srv ),
      socket_( socket ),
      active_( true ),
      have_socket_( true ),
      qid_( srv.next_handler_queue_id() ),
      poll_ms_( poll_ms ),
      retry_index_( 0 ),
      retry_count_( retry_count ),
      retry_after_ms_( retry_after_ms ),
      retry_first_ms_( retry_first_ms ) {}
async_read_and_close_socket_base::~async_read_and_close_socket_base() {}

void async_read_and_close_socket_base::close_socket() {
    if ( have_socket_ ) {
        have_socket_ = false;
        detail::close_socket( socket_ );
    }
}

void async_read_and_close_socket_base::run() {
    if ( !active_ )
        return;
    schedule_first_step();
}

bool async_read_and_close_socket_base::schedule_check_clock() {
    if ( retry_index_ > retry_count_ )
        return false;
    clock_t tpNow = clock();
    clock_t tpMin = ( clock_t )( ( retry_index_ == 0 ) ? retry_first_ms_ : retry_after_ms_ );
    clock_t tpDist = tpNow - tpStep_;
    if ( tpDist < tpMin )
        return false;  // too early
    tpStep_ = tpNow;
    return true;
}

void async_read_and_close_socket_base::schedule_first_step() {
#if ( defined __SKUTILS_HTTP_DEBUG_CONSOLE_TRACE_HTTP_TASK_STATES__ )
    std::cout << skutils::tools::format( "http task schedule 1st step %p\n", this );
    std::cout.flush();
#endif
    skutils::retain_release_ptr< async_read_and_close_socket_base > pThis = this;
    skutils::dispatch::job_t job = [pThis]() -> void {
#if ( defined __SKUTILS_HTTP_DEBUG_CONSOLE_TRACE_HTTP_TASK_STATES__ )
        std::cout << skutils::tools::format(
            "http task will do 1st step %p\n", pThis.get_unconst() );
        std::cout.flush();
#endif
        pThis.get_unconst()->step();
#if ( defined __SKUTILS_HTTP_DEBUG_CONSOLE_TRACE_HTTP_TASK_STATES__ )
        std::cout << skutils::tools::format( "http task done 1st step %p\n", pThis.get_unconst() );
        std::cout.flush();
#endif
    };
    tpStep_ = clock();
    skutils::dispatch::async(
        qid_, job /*, skutils::dispatch::duration_from_milliseconds( retry_first_ms_ )*/ );
}

void async_read_and_close_socket_base::schedule_next_step() {
#if ( defined __SKUTILS_HTTP_DEBUG_CONSOLE_TRACE_HTTP_TASK_STATES__ )
    std::cout << skutils::tools::format( "http task shedule next step %p\n", this );
    std::cout.flush();
#endif
    skutils::retain_release_ptr< async_read_and_close_socket_base > pThis = this;
    skutils::dispatch::job_t job = [pThis]() -> void {
#if ( defined __SKUTILS_HTTP_DEBUG_CONSOLE_TRACE_HTTP_TASK_STATES__ )
        std::cout << skutils::tools::format(
            "http task will do next step %p\n", pThis.get_unconst() );
        std::cout.flush();
#endif
        pThis.get_unconst()->step();
#if ( defined __SKUTILS_HTTP_DEBUG_CONSOLE_TRACE_HTTP_TASK_STATES__ )
        std::cout << skutils::tools::format( "http task done next step %p\n", pThis.get_unconst() );
        std::cout.flush();
#endif
    };
    skutils::dispatch::async(
        qid_, job /*, skutils::dispatch::duration_from_milliseconds( retry_after_ms_ )*/ );
}

void async_read_and_close_socket_base::call_fail_handler(
    const char* strErrorDescription, bool is_close_socket, bool is_remove_this_task ) {
    try {
        if ( callback_fail_ )
            callback_fail_( strErrorDescription );
    } catch ( ... ) {
    }
    if ( is_close_socket )
        close_socket();
    if ( is_remove_this_task )
        remove_this_task();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

async_read_and_close_socket::async_read_and_close_socket( server& srv, socket_t socket,
    size_t poll_ms, size_t retry_count, size_t retry_after_ms, size_t retry_first_ms )
    : async_read_and_close_socket_base(
          srv, socket, poll_ms, retry_count, retry_after_ms, retry_first_ms ) {}

async_read_and_close_socket::~async_read_and_close_socket() {
    will_remove();
}

void async_read_and_close_socket::run() {
    async_read_and_close_socket_base::run();
}

bool async_read_and_close_socket::step() {
    std::string strErrorDescription;
    try {
        if ( !schedule_check_clock() ) {
            schedule_next_step();
            return true;
        }
        if ( retry_index_ > retry_count_ )
            throw std::runtime_error( "max attempt count done" );
        ++retry_index_;
        if ( retry_index_ > retry_count_ ) {
            call_fail_handler( "transfer timeout", false, false );
            close_socket();
            remove_this_task();
            return false;
        } else if ( detail::poll_read( socket_, poll_ms_ ) ) {
            socket_stream strm( socket_ );
            bool connection_close = false;
            if ( callback_success_ ) {
                bool is_fail = false;
                try {
                    bool last_connection = true;
                    callback_success_( strm, last_connection, connection_close );
                } catch ( ... ) {
                    is_fail = true;
                    connection_close = true;
                }
                if ( is_fail )
                    call_fail_handler( "transfer fail", false, false );
            }
            if ( connection_close )
                close_socket();
            remove_this_task();
            return false;
        }
        schedule_next_step();
        return true;
    } catch ( const common_network_exception& nx ) {
        if ( nx.what() && nx.what()[0] )
            strErrorDescription =
                std::string( nx.what() ) + " (errcode=" + std::to_string( nx.ei_.ec_ ) + ")";
    } catch ( std::exception& ex ) {
        strErrorDescription = ex.what();
    } catch ( ... ) {
    }
    if ( strErrorDescription.empty() )
        strErrorDescription = "unknown exception";
    call_fail_handler( strErrorDescription.c_str() );
    return false;
}

void async_read_and_close_socket::was_added() {
    async_read_and_close_socket_base::was_added();
}

void async_read_and_close_socket::will_remove() {
    if ( !active_ )
        return;
    active_ = false;
    close_socket();
    async_read_and_close_socket_base::will_remove();
}

void async_read_and_close_socket::close_socket() {
    async_read_and_close_socket_base::close_socket();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

async_read_and_close_socket_SSL::async_read_and_close_socket_SSL( server& srv, SSL_CTX* ctx,
    socket_t socket, size_t poll_ms, size_t retry_count, size_t retry_after_ms,
    size_t retry_first_ms )
    : async_read_and_close_socket_base(
          srv, socket, poll_ms, retry_count, retry_after_ms, retry_first_ms ),
      ctx_( ctx ),
      ssl_( nullptr ),
      bio_( nullptr ) {}

async_read_and_close_socket_SSL::~async_read_and_close_socket_SSL() {
    will_remove();
}

void async_read_and_close_socket_SSL::run() {
    async_read_and_close_socket_base::run();
}

bool async_read_and_close_socket_SSL::step() {
    std::string strErrorDescription;
    try {
        if ( !schedule_check_clock() ) {
            schedule_next_step();
            return true;
        }
        if ( retry_index_ > retry_count_ )
            throw std::runtime_error( "max attempt count done" );
        if ( retry_index_ == 0 ) {
            ssl_ = SSL_new( ctx_ );
            if ( !ssl_ )
                throw std::runtime_error( "SSL initialization failed" );
            bio_ = BIO_new_socket( socket_, BIO_NOCLOSE );
            SSL_set_bio( ssl_, bio_, bio_ );
            if ( setup_ssl_ )
                setup_ssl_();
            if ( SSL_connect_or_accept_ )
                SSL_connect_or_accept_( ssl_ );
            else
                detail::SSL_accept_wrapper( ssl_ );
        }
        ++retry_index_;
        if ( retry_index_ >= retry_count_ ) {
            call_fail_handler( "transfer timeout", false, false );
            close_socket();
            remove_this_task();
            return false;
        } else if ( detail::poll_read( socket_, poll_ms_ ) ) {
            SSL_socket_stream strm( socket_, ssl_ );
            bool connection_close = false;
            if ( callback_success_ ) {
                bool is_fail = false;
                try {
                    bool last_connection = true;
                    callback_success_( strm, last_connection, connection_close );
                } catch ( ... ) {
                    is_fail = true;
                    connection_close = true;
                }
                if ( is_fail )
                    call_fail_handler( "transfer fail", false, false );
            }
            if ( connection_close )
                close_socket();
            remove_this_task();
            return false;
        }
        schedule_next_step();
        return true;
    } catch ( std::exception& ex ) {
        strErrorDescription = ex.what();
    } catch ( ... ) {
    }
    if ( strErrorDescription.empty() )
        strErrorDescription = "unknown exception";
    call_fail_handler( strErrorDescription.c_str() );
    return false;
}

void async_read_and_close_socket_SSL::was_added() {
    async_read_and_close_socket_base::was_added();
}

void async_read_and_close_socket_SSL::will_remove() {
    if ( !active_ )
        return;
    active_ = false;
    close_socket();
    async_read_and_close_socket_base::will_remove();
}

void async_read_and_close_socket_SSL::close_socket() {
    if ( have_socket_ ) {
        if ( ssl_ ) {
            SSL_free( ssl_ );
            ssl_ = nullptr;
        }
    }
    async_read_and_close_socket_base::close_socket();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

common::common( int ipVer ) : ipVer_( ipVer ) {}

common::~common() {}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

server::server( size_t a_max_handler_queues, bool is_async_http_transfer_mode )
    : common( -1 ),
      is_async_http_transfer_mode_( is_async_http_transfer_mode ),
      keep_alive_max_count_( __SKUTILS_HTTP_KEEPALIVE_MAX_COUNT__ ),
      is_running_( false ),
      svr_sock_( INVALID_SOCKET ),
      max_handler_queues_( a_max_handler_queues ),
      current_handler_queue_( 0 ) {
    if ( max_handler_queues_ < 1 )
        max_handler_queues_ = 1;
#ifndef _WIN32
    ::signal( SIGPIPE, SIG_IGN );
#endif
}

server::~server() {
    remove_all_tasks();
    close_all_handler_queues();
}

server& server::Get( const char* pattern, Handler handler ) {
    get_handlers_.push_back( std::make_pair( std::regex( pattern ), handler ) );
    return *this;
}

server& server::Post( const char* pattern, Handler handler ) {
    post_handlers_.push_back( std::make_pair( std::regex( pattern ), handler ) );
    return *this;
}

server& server::Put( const char* pattern, Handler handler ) {
    put_handlers_.push_back( std::make_pair( std::regex( pattern ), handler ) );
    return *this;
}

server& server::Patch( const char* pattern, Handler handler ) {
    patch_handlers_.push_back( std::make_pair( std::regex( pattern ), handler ) );
    return *this;
}

server& server::Delete( const char* pattern, Handler handler ) {
    delete_handlers_.push_back( std::make_pair( std::regex( pattern ), handler ) );
    return *this;
}

server& server::Options( const char* pattern, Handler handler ) {
    options_handlers_.push_back( std::make_pair( std::regex( pattern ), handler ) );
    return *this;
}

#if ( defined __SKUTILS_HTTP_ENABLE_FILE_REQUEST_HANDLING )

std::string server::base_dir_get() const {
    return base_dir_;
}

bool server::set_base_dir( const char* path ) {
    if ( detail::is_dir( path ) ) {
        base_dir_ = path;
        return true;
    }
    return false;
}

#endif  // (defined __SKUTILS_HTTP_ENABLE_FILE_REQUEST_HANDLING)

void server::set_error_handler( Handler handler ) {
    error_handler_ = handler;
}

void server::set_logger( Logger logger ) {
    logger_ = logger;
}

size_t server::get_keep_alive_max_count() const {
    size_t cnt = keep_alive_max_count_;
    return cnt;
}

void server::set_keep_alive_max_count( size_t cnt ) {
    keep_alive_max_count_ = cnt;
}

int server::bind_to_any_port(
    int ipVer, const char* host, int socket_flags, bool is_reuse_address, bool is_reuse_port ) {
    return bind_internal( ipVer, host, 0, socket_flags, is_reuse_address, is_reuse_port );
}

bool server::listen_after_bind() {
    return listen_internal();
}

bool server::listen( int ipVer, const char* host, int port, int socket_flags, bool is_reuse_address,
    bool is_reuse_port ) {
    if ( bind_internal( ipVer, host, port, socket_flags, is_reuse_address, is_reuse_port ) < 0 )
        return false;
    return listen_internal();
}

bool server::is_running() const {
    return is_running_;
}

void server::stop() {
    // close_all_handler_queues();
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

bool server::parse_request_line( const char* s, request& req ) {
    try {
        static std::regex re(
            "(GET|HEAD|POST|PUT|PATCH|DELETE|OPTIONS) "
            "(([^?]+)(?:\\?(.+?))?) (HTTP/1\\.[01])\r\n" );
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
    } catch ( const std::exception& ex ) {
        std::string sw = ex.what();
        if ( sw.empty() )
            sw = "unknown error";
        std::cerr.flush();
        std::cerr << "HTTP server got \"" << sw << "\" exception and failed to parse line: " << s
                  << "\n";
        std::cerr.flush();
    } catch ( ... ) {
        std::cerr.flush();
        std::cerr << "HTTP server failed to parse line: " << s << "\n";
        std::cerr.flush();
    }
    return false;
}

void server::write_response(
    stream& strm, bool last_connection, const request& req, response& res ) {
    assert( res.status_ != -1 );
    if ( res.status_ != 200 )
        std::cout << "Failed to handle HTTP request, returning status " << res.status_
                  << ", request body is: " << req.body_ << "\n";
    if ( 400 <= res.status_ && error_handler_ ) {
        error_handler_( req, res );
    }
    // response line
    strm.write_format( "HTTP/1.1 %d %s\r\n", res.status_, detail::status_message( res.status_ ) );
    // headers
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
                    chunk = detail::from_i_to_hex( chunk.size() ) + g_strCrLf + chunk + g_strCrLf;
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

#if ( defined __SKUTILS_HTTP_ENABLE_FILE_REQUEST_HANDLING )

bool server::handle_file_request( request& req, response& res ) {
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

#endif  // (defined __SKUTILS_HTTP_ENABLE_FILE_REQUEST_HANDLING)

socket_t server::create_server_socket( int ipVer, const char* host, int port, int socket_flags,
    bool is_reuse_address, bool is_reuse_port ) const {
    detail::auto_detect_ipVer( ipVer, host );
    ipVer_ = ipVer;
    return detail::create_socket( ipVer, host, port,
        [this, port]( socket_t sock, struct addrinfo& ai ) -> bool {
            if (::bind( sock, ai.ai_addr, static_cast< int >( ai.ai_addrlen ) ) ) {
                return false;
            }
            boundToPort_ = port;
            if (::listen( sock, 5 ) ) {  // listen through 5 channels
                return false;
            }
            return true;
        },
        socket_flags, is_reuse_address, is_reuse_port );
}

int server::bind_internal( int ipVer, const char* host, int port, int socket_flags,
    bool is_reuse_address, bool is_reuse_port ) {
    if ( !is_valid() ) {
        return -1;
    }
    svr_sock_ =
        create_server_socket( ipVer, host, port, socket_flags, is_reuse_address, is_reuse_port );
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

bool server::listen_internal() {
    suspend_adding_tasks_ = false;
    is_in_loop_ = true;
    auto ret = true;
    try {
        is_running_ = true;
        for ( ; is_running_; ) {
            bool isOK = detail::poll_read( svr_sock_, __SKUTILS_HTTP_ACCEPT_WAIT_MILLISECONDS__ );
            if ( !isOK ) {  // timeout
                if ( svr_sock_ == INVALID_SOCKET ) {
                    // server socket was closed by "stop" method
                    break;
                }
                continue;
            }
            MICROPROFILE_SCOPEI( "skutils", "http::server::listen_internal", MP_PALEGREEN );
            socket_t sock = accept( svr_sock_, nullptr, nullptr );
            if ( sock == INVALID_SOCKET )
                continue;
            if ( is_async_http_transfer_mode_ )
                read_and_close_socket_async( sock );
            else
                read_and_close_socket_sync( sock );
        }
        suspend_adding_tasks_ = true;
        remove_all_tasks();
        for ( ; is_running_; ) {
            std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
            if ( !have_running_tasks() ) {
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

server::tasks_mutex_type& server::tasks_mtx() {
    return skutils::get_ref_mtx();
}

bool server::have_running_tasks() {
    size_t n = 0;
    {  // block
        tasks_lock_type lock( tasks_mtx() );
        n = map_tasks_.size();
    }  // block
    return ( n != 0 ) ? true : false;
}

bool server::add_task( task_ptr_t pTask ) {
    if ( !pTask )
        return false;
    tasks_lock_type lock( tasks_mtx() );
    task_id_t tid = task_id_t( pTask.get() );
    map_tasks_[tid] = pTask;
    pTask->was_added();
    return true;
}
bool server::remove_task( task_ptr_t pTask ) {
    if ( !pTask )
        return false;
    tasks_lock_type lock( tasks_mtx() );
    task_id_t tid = task_id_t( pTask.get() );
    map_tasks_t::iterator itFind = map_tasks_.find( tid ), itEnd = map_tasks_.end();
    if ( itFind == itEnd )
        return false;
    pTask->will_remove();
    map_tasks_.erase( itFind );
    return true;
}

void server::remove_all_tasks() {
    tasks_lock_type lock( tasks_mtx() );
    map_tasks_t::iterator itWalk = map_tasks_.begin(), itEnd = map_tasks_.end();
    for ( ; itWalk != itEnd; ++itWalk ) {
        task_ptr_t pTask = itWalk->second;
        pTask->will_remove();
    }
    map_tasks_.clear();
}

bool server::routing( request& req, response& res ) {
#if ( defined __SKUTILS_HTTP_ENABLE_FILE_REQUEST_HANDLING )
    if ( req.method_ == "GET" && handle_file_request( req, res ) )
        return true;
#endif  // (defined __SKUTILS_HTTP_ENABLE_FILE_REQUEST_HANDLING)
    if ( req.method_ == "GET" || req.method_ == "HEAD" )
        return dispatch_request( req, res, get_handlers_ );
    else if ( req.method_ == "POST" )
        return dispatch_request( req, res, post_handlers_ );
    else if ( req.method_ == "PUT" )
        return dispatch_request( req, res, put_handlers_ );
    else if ( req.method_ == "PATCH" )
        return dispatch_request( req, res, patch_handlers_ );
    else if ( req.method_ == "DELETE" )
        return dispatch_request( req, res, delete_handlers_ );
    else if ( req.method_ == "OPTIONS" )
        return dispatch_request( req, res, options_handlers_ );
    return false;
}

bool server::dispatch_request( request& req, response& res, Handlers& handlers ) {
    for ( const auto& x : handlers ) {
        const auto& pattern = x.first;
        const auto& handler = x.second;
        try {
            if ( std::regex_match( req.path_, req.matches_, pattern ) ) {
                handler( req, res );
                return true;
            }
        } catch ( const std::exception& ex ) {
            std::string sw = ex.what();
            if ( sw.empty() )
                sw = "unknown error";
            std::cerr.flush();
            std::cerr << "HTTP server got \"" << sw
                      << "\" exception and failed to parse request: " << req.path_ << "\n";
            std::cerr.flush();
        } catch ( ... ) {
            std::cerr.flush();
            std::cerr << "HTTP server failed to parse request: " << req.path_ << "\n";
            std::cerr.flush();
        }
    }
    return false;
}

bool server::process_request(
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

    // temporary disable persistent connections
    // if ( req.get_header_value( "Connection" ) == "close" ) {
    connection_close = true;
    //}

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

bool server::is_valid() const {
    return true;
}

bool server::read_and_close_socket_sync( socket_t sock ) {
    eiLast_.clear();
    common_network_exception::error_info ei;
    if ( !detail::read_and_close_socket( sock, get_keep_alive_max_count(),
             [this, sock]( stream& strm, bool last_connection, bool& connection_close ) {
                 std::string origin = skutils::network::get_fd_name_as_url(
                     sock, is_ssl() ? "HTTPS" : "HTTP", true );
                 return process_request( origin, strm, last_connection, connection_close );
             },
             ei ) ) {
        eiLast_ = ei;
        return false;
    }
    return true;
}

void server::read_and_close_socket_async( socket_t sock ) {
    auto pRT = new async_read_and_close_socket( *this, sock );
    task_ptr_t pTask = pRT;
    pRT->callback_success_ = [this, sock]( stream& strm, bool last_connection,
                                 bool& connection_close ) -> void {
        std::string origin =
            skutils::network::get_fd_name_as_url( sock, is_ssl() ? "HTTPS" : "HTTP", true );
        if ( !process_request( origin, strm, last_connection, connection_close ) )
            throw std::runtime_error( "failed to process request" );
    };
    pRT->callback_fail_ = [this, sock]( const char* strErrorDescription ) {
        std::cout << "failed to process http request from socket " << sock
                  << ", error description: "
                  << ( ( strErrorDescription && strErrorDescription[0] ) ? strErrorDescription :
                                                                           "unkknown error" )
                  << "\n";
    };
    add_task( pTask );
    pTask->run();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SSL_server::SSL_server( const char* cert_path, const char* private_key_path,
    size_t a_max_handler_queues, bool is_async_http_transfer_mode )
    : server( a_max_handler_queues, is_async_http_transfer_mode ) {
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

SSL_server::~SSL_server() {
    if ( ctx_ ) {
        SSL_CTX_free( ctx_ );
    }
}

bool SSL_server::is_valid() const {
    return ctx_;
}

bool SSL_server::read_and_close_socket_sync( socket_t sock ) {
    std::string origin =
        skutils::network::get_fd_name_as_url( sock, is_ssl() ? "HTTPS" : "HTTP", true );
    eiLast_.clear();
    common_network_exception::error_info ei;
    if ( !detail::read_and_close_socket_ssl( sock, get_keep_alive_max_count(), ctx_, ctx_mutex_,
             detail::SSL_accept_wrapper,
             [origin]( SSL*  // ssl
             ) {},
             [this, origin]( stream& strm, bool last_connection, bool& connection_close ) {
                 return process_request( origin, strm, last_connection, connection_close );
             },
             ei ) ) {
        eiLast_ = ei;
        return false;
    }
    return true;
}

void SSL_server::read_and_close_socket_async( socket_t sock ) {
    std::string origin =
        skutils::network::get_fd_name_as_url( sock, is_ssl() ? "HTTPS" : "HTTP", true );
    auto pRT = new async_read_and_close_socket_SSL( *this, ctx_, sock );
    task_ptr_t pTask = pRT;
    pRT->callback_success_ = [this, origin](
                                 stream& strm, bool last_connection, bool& connection_close ) {
        return process_request( origin, strm, last_connection, connection_close );
    };
    pRT->callback_fail_ = [this, sock]( const char* strErrorDescription ) {
        std::cout << "failed to process http request from socket(SSL) " << sock
                  << ", error description: "
                  << ( ( strErrorDescription && strErrorDescription[0] ) ? strErrorDescription :
                                                                           "unkknown error" )
                  << "\n";
    };
    add_task( pTask );
    pTask->run();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

client::client( int ipVer, const char* host, int port, int timeout_milliseconds )
    : common( ipVer ),
      host_( host ),
      port_( port ),
      timeout_milliseconds_( timeout_milliseconds ),
      host_and_port_( host_ + ":" + std::to_string( port_ ) ) {}

client::~client() {}

bool client::is_valid() const {
    return true;
}

socket_t client::create_client_socket(
    int ipVer, int socket_flags, bool is_reuse_address, bool is_reuse_port ) const {
    detail::auto_detect_ipVer( ipVer, host_.c_str() );
    ipVer_ = ipVer;
    return detail::create_socket( ipVer, host_.c_str(), port_,
        [=]( socket_t sock, struct addrinfo& ai ) -> bool {
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
        },
        socket_flags, is_reuse_address, is_reuse_port );
}

bool client::read_response_line( stream& strm, response& res ) {
    const auto bufsiz = 2048;
    char buf[bufsiz];
    detail::stream_line_reader reader( strm, buf, bufsiz );
    if ( !reader.getline() ) {
        return false;
    }
    try {
        const static std::regex re( "(HTTP/1\\.[01]) (\\d+?) .*\r\n" );
        std::cmatch m;
        if ( std::regex_match( reader.ptr(), m, re ) ) {
            res.version_ = std::string( m[1] );
            res.status_ = std::stoi( std::string( m[2] ) );
        }
    } catch ( const std::exception& ex ) {
        std::string sw = ex.what();
        if ( sw.empty() )
            sw = "unknown error";
        std::cerr.flush();
        std::cerr << "HTTP server got \"" << sw
                  << "\" exception and failed to parse response line: " << reader.ptr() << "\n";
        std::cerr.flush();
        return false;
    } catch ( ... ) {
        std::cerr.flush();
        std::cerr << "HTTP server failed to parse response line: " << reader.ptr() << "\n";
        std::cerr.flush();
        return false;
    }
    return true;
}

bool client::send( request& req, response& res ) {
    if ( req.path_.empty() ) {
        return false;
    }
    auto sock = create_client_socket( ipVer_ );
    if ( sock == INVALID_SOCKET ) {
        eiLast_.et_ = common_network_exception::error_type::et_fatal;
        eiLast_.strError_ = "Failed to create socket";
        eiLast_.ec_ = errno;
        return false;
    }
    return read_and_close_socket( sock, req, res );
}

void client::write_request( stream& strm, request& req ) {
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

bool client::process_request( const std::string& /*origin*/, stream& strm, request& req,
    response& res, bool& connection_close ) {
    // send request
    write_request( strm, req );
    // receive response and headers
    if ( !read_response_line( strm, res ) || !detail::read_headers( strm, res.headers_ ) ) {
        return false;
    }

    // temporary disable persistent connections
    // if ( res.get_header_value( "Connection" ) == "close" || res.version_ == "HTTP/1.0" ) {
    connection_close = true;
    //}
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

bool client::read_and_close_socket( socket_t sock, request& req, response& res ) {
    std::string origin =
        skutils::network::get_fd_name_as_url( sock, is_ssl() ? "HTTPS" : "HTTP", false );
    eiLast_.clear();
    common_network_exception::error_info ei;
    if ( !detail::read_and_close_socket( sock, 0,
             [&, origin]( stream& strm, bool,  // last_connection
                 bool& connection_close ) {
                 return process_request( origin, strm, req, res, connection_close );
             },
             ei ) ) {
        eiLast_ = ei;
        return false;
    }
    return true;
}

std::shared_ptr< response > client::Get(
    const char* path, fn_progress progress, bool isReturnErrorResponse ) {
    return Get( path, map_headers(), progress, isReturnErrorResponse );
}

std::shared_ptr< response > client::Get( const char* path, const map_headers& headers,
    fn_progress progress, bool isReturnErrorResponse ) {
    request req;
    req.method_ = "GET";
    req.path_ = path;
    req.headers_ = headers;
    req.progress_ = progress;
    auto res = std::make_shared< response >();
    res->send_status_ = send( req, *res );
    return ( res->send_status_ || isReturnErrorResponse ) ? res : nullptr;
}

std::shared_ptr< response > client::Head( const char* path, bool isReturnErrorResponse ) {
    return Head( path, map_headers(), isReturnErrorResponse );
}

std::shared_ptr< response > client::Head(
    const char* path, const map_headers& headers, bool isReturnErrorResponse ) {
    request req;
    req.method_ = "HEAD";
    req.headers_ = headers;
    req.path_ = path;
    auto res = std::make_shared< response >();
    res->send_status_ = send( req, *res );
    return ( res->send_status_ || isReturnErrorResponse ) ? res : nullptr;
}

std::shared_ptr< response > client::Post( const char* path, const std::string& body,
    const char* content_type, bool isReturnErrorResponse ) {
    return Post( path, map_headers(), body, content_type, isReturnErrorResponse );
}

std::shared_ptr< response > client::Post( const char* path, const map_headers& headers,
    const std::string& body, const char* content_type, bool isReturnErrorResponse ) {
    request req;
    req.method_ = "POST";
    req.headers_ = headers;
    req.path_ = path;
    req.headers_.emplace( "Content-Type", content_type );
    req.body_ = body;
    auto res = std::make_shared< response >();
    res->send_status_ = send( req, *res );
    return ( res->send_status_ || isReturnErrorResponse ) ? res : nullptr;
}

std::shared_ptr< response > client::Post(
    const char* path, const map_params& params, bool isReturnErrorResponse ) {
    return Post( path, map_headers(), params, isReturnErrorResponse );
}

std::shared_ptr< response > client::Post( const char* path, const map_headers& headers,
    const map_params& params, bool isReturnErrorResponse ) {
    std::string query;
    for ( auto it = params.begin(); it != params.end(); ++it ) {
        if ( it != params.begin() ) {
            query += "&";
        }
        query += it->first;
        query += "=";
        query += it->second;
    }
    return Post( path, headers, query, "application/x-www-form-urlencoded", isReturnErrorResponse );
}

std::shared_ptr< response > client::Put( const char* path, const std::string& body,
    const char* content_type, bool isReturnErrorResponse ) {
    return Put( path, map_headers(), body, content_type, isReturnErrorResponse );
}

std::shared_ptr< response > client::Put( const char* path, const map_headers& headers,
    const std::string& body, const char* content_type, bool isReturnErrorResponse ) {
    request req;
    req.method_ = "PUT";
    req.headers_ = headers;
    req.path_ = path;
    req.headers_.emplace( "Content-Type", content_type );
    req.body_ = body;
    auto res = std::make_shared< response >();
    res->send_status_ = send( req, *res );
    return ( res->send_status_ || isReturnErrorResponse ) ? res : nullptr;
}

std::shared_ptr< response > client::Patch( const char* path, const std::string& body,
    const char* content_type, bool isReturnErrorResponse ) {
    return Patch( path, map_headers(), body, content_type, isReturnErrorResponse );
}

std::shared_ptr< response > client::Patch( const char* path, const map_headers& headers,
    const std::string& body, const char* content_type, bool isReturnErrorResponse ) {
    request req;
    req.method_ = "PATCH";
    req.headers_ = headers;
    req.path_ = path;
    req.headers_.emplace( "Content-Type", content_type );
    req.body_ = body;
    auto res = std::make_shared< response >();
    res->send_status_ = send( req, *res );
    return ( res->send_status_ || isReturnErrorResponse ) ? res : nullptr;
}

std::shared_ptr< response > client::Delete( const char* path, bool isReturnErrorResponse ) {
    return Delete( path, map_headers(), isReturnErrorResponse );
}

std::shared_ptr< response > client::Delete(
    const char* path, const map_headers& headers, bool isReturnErrorResponse ) {
    request req;
    req.method_ = "DELETE";
    req.path_ = path;
    req.headers_ = headers;
    auto res = std::make_shared< response >();
    res->send_status_ = send( req, *res );
    return ( res->send_status_ || isReturnErrorResponse ) ? res : nullptr;
}

std::shared_ptr< response > client::Options( const char* path, bool isReturnErrorResponse ) {
    return Options( path, map_headers(), isReturnErrorResponse );
}

std::shared_ptr< response > client::Options(
    const char* path, const map_headers& headers, bool isReturnErrorResponse ) {
    request req;
    req.method_ = "OPTIONS";
    req.path_ = path;
    req.headers_ = headers;
    auto res = std::make_shared< response >();
    res->send_status_ = send( req, *res );
    return ( res->send_status_ || isReturnErrorResponse ) ? res : nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SSL_client::SSL_client(
    int ipVer, const char* host, int port, int timeout_milliseconds, SSL_client_options* pOptsSSL )
    : client( ipVer, host, port, timeout_milliseconds ) {
    if ( pOptsSSL )
        optsSSL = ( *pOptsSSL );
    ctx_ = SSL_CTX_new( SSLv23_client_method() );
    if ( optsSSL.ca_file.empty() ) {
        SSL_CTX_set_verify( ctx_, SSL_VERIFY_NONE, nullptr );
        SSL_CTX_set_verify_depth( ctx_, 0 );
    } else
        SSL_CTX_load_verify_locations( ctx_, optsSSL.ca_file.c_str(),
            ( !optsSSL.ca_path.empty() ) ? optsSSL.ca_path.c_str() : nullptr );
    if ( optsSSL.ctx_mode )
        SSL_CTX_set_mode( ctx_, optsSSL.ctx_mode );
    if ( optsSSL.ctx_cache_mode )
        SSL_CTX_set_session_cache_mode( ctx_, optsSSL.ctx_cache_mode );
    if ( !optsSSL.client_cert.empty() ) {
        if ( 1 != SSL_CTX_use_certificate_chain_file( ctx_, optsSSL.client_cert.c_str() ) )
            throw std::runtime_error( "unable to load client certificate chain" );
    }
    if ( !optsSSL.client_key.empty() ) {
        if ( 1 != SSL_CTX_use_PrivateKey_file(
                      ctx_, optsSSL.client_key.c_str(), optsSSL.client_key_type ) )
            throw std::runtime_error( "unable to load client key" );
    }
}

SSL_client::~SSL_client() {
    if ( ctx_ ) {
        SSL_CTX_free( ctx_ );
    }
}

bool SSL_client::is_valid() const {
    return ctx_;
}

bool SSL_client::read_and_close_socket( socket_t sock, request& req, response& res ) {
    std::string origin =
        skutils::network::get_fd_name_as_url( sock, is_ssl() ? "HTTPS" : "HTTP", false );
    if ( !is_valid() )
        return false;
    eiLast_.clear();
    common_network_exception::error_info ei;
    if ( !detail::read_and_close_socket_ssl( sock, 0, ctx_, ctx_mutex_, detail::SSL_connect_wrapper,
             [&]( SSL* ssl ) { SSL_set_tlsext_host_name( ssl, host_.c_str() ); },
             [&, origin]( stream& strm, bool,  // last_connection
                 bool& connection_close ) {
                 return process_request( origin, strm, req, res, connection_close );
             },
             ei ) ) {
        eiLast_ = ei;
        return false;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace http

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace http_curl {

client::client( const skutils::url& u,
        int timeout_milliseconds, // = __SKUTILS_HTTP_CLIENT_CONNECT_TIMEOUT_MILLISECONDS__
        skutils::http::SSL_client_options* pOptsSSL // = nullptr
        )
        : u_( u )
        , timeout_milliseconds_( timeout_milliseconds )
{
    if( pOptsSSL )
        optsSSL = (*pOptsSSL);
}

client::~client() {

}

bool client::is_valid() const {
    if( u_.host().empty() )
        return false;
const std::string strScheme = skutils::tools::to_lower( skutils::tools::trim_copy( u_.scheme() ) );
    if( !( strScheme == "http" || strScheme == "https" ) )
        return false;
    return true;
}

bool client::is_ssl() const {
const std::string strScheme = skutils::tools::to_lower( skutils::tools::trim_copy( u_.scheme() ) );
    if( strScheme != "https" )
        return false;
    return true;
}

bool client::is_ssl_with_explicit_cert_key() const {
    if( ! is_ssl() )
        return false;
    //if ( optsSSL.ca_file.empty() )
    //    return false;
    //if ( optsSSL.ca_path.empty() )
    //    return false;
    if ( optsSSL.client_cert.empty() )
        return false;
    if ( optsSSL.client_key.empty() )
        return false;
    return true;
}

size_t client::stat_WriteMemoryCallback( void * contents, size_t size, size_t nmemb, void * userp ) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *) userp;
    char *ptr = (char *) realloc(mem->memory, mem->size + realsize + 1);
    if( ! ptr )
        return 0;
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

bool client::query(
        const char * strInData,
        const char * strInContentType, // i.e. "application/json"
        std::string & strOutData,
        std::string & strOutContentType,
        skutils::http::common_network_exception::error_info& ei
        ) {
    ei.clear();
    strOutData.clear();
    strOutContentType.clear();
    bool ret = false;
    // curl_global_init( CURL_GLOBAL_DEFAULT );
    CURL * curl = nullptr;
    struct curl_slist * headers = nullptr;
    // char errbuf[CURL_ERROR_SIZE] = { 0, };
    //
    struct MemoryStruct chunk;
    chunk.memory = (char *) malloc( 1 );
    chunk.size = 0;
    try {
        if( ! chunk.memory )
            throw std::runtime_error( "CURL failed to alloc initial memory chunk" );
        curl = curl_easy_init();
        if( ! curl )
            throw std::runtime_error( "CURL easy init failed" );
        //
        std::string strHeaderContentType = skutils::tools::format(
                    "Content-Type: %s",
                    strInContentType ? strInContentType : "application/json"
                    );
        headers = curl_slist_append( headers, "Expect:" );
        headers = curl_slist_append( headers, strHeaderContentType.c_str() );
        //
        std::string strURL = u_.str();
        curl_easy_setopt( curl, CURLOPT_URL, strURL.c_str() );
        // FILE * headerfile = stdout; // fopen( "dumpit.txt", "wb");
        // curl_easy_setopt( curl, CURLOPT_HEADERDATA, headerfile );
        if( ! strUserAgent_.empty() )
            curl_easy_setopt( curl, CURLOPT_USERAGENT, strUserAgent_.c_str() );
        if( ! strDnsServers_.empty() )
            curl_easy_setopt( curl, CURLOPT_DNS_SERVERS, strDnsServers_.c_str() );
        curl_easy_setopt( curl, CURLOPT_COOKIEFILE, "" );
        curl_easy_setopt( curl, CURLOPT_TIMEOUT_MS, timeout_milliseconds_ );
        curl_easy_setopt( curl, CURLOPT_POST, 1L );
        curl_easy_setopt( curl, CURLOPT_HTTPHEADER, headers );
        curl_easy_setopt( curl, CURLOPT_POSTFIELDS, strInData );
        curl_easy_setopt( curl, CURLOPT_POSTFIELDSIZE, -1L );
        curl_easy_setopt( curl, CURLOPT_VERBOSE, isVerboseInsideCURL_ ? 1L : 0L );
        // curl_easy_setopt( curl, CURLOPT_ERRORBUFFER, errbuf );
        curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, stat_WriteMemoryCallback );
        curl_easy_setopt( curl, CURLOPT_WRITEDATA, (void *) &chunk );
        //
        if( is_ssl_with_explicit_cert_key() ) {
            if( pCurlCryptoEngine_ ) {
                // use crypto engine
                if( curl_easy_setopt( curl, CURLOPT_SSLENGINE, pCurlCryptoEngine_ ) != CURLE_OK )
                    throw std::runtime_error( "CURL cannot set crypto engine" );
                // set the crypto engine as default, only needed for the first time you load a engine in a curl object
                if( curl_easy_setopt( curl, CURLOPT_SSLENGINE_DEFAULT, 1L ) != CURLE_OK )
                    throw std::runtime_error( "CURL cannot set crypto engine as default" );
            } // if( pCurlCryptoEngine_ )
            // cert is stored PEM coded in file... since PEM is default, we needn't set it for PEM
            curl_easy_setopt( curl, CURLOPT_SSLCERTTYPE, "PEM" );
            // set the cert for client authentication
            curl_easy_setopt( curl, CURLOPT_SSLCERT, optsSSL.client_cert.c_str() ); // like "cert.pem"
            // for engine we must set the passphrase (if the key has one...)
            if( pCryptoEnginePassphrase_ )
                curl_easy_setopt( curl, CURLOPT_KEYPASSWD, pCryptoEnginePassphrase_ );
            // if we use a key stored in a crypto engine, we must set the key type to "ENG"
            if( ! strKeyType_.empty() )
                curl_easy_setopt( curl, CURLOPT_SSLKEYTYPE, strKeyType_.c_str() );
            // set the private key (file or ID in engine)
            curl_easy_setopt( curl, CURLOPT_SSLKEY, optsSSL.client_key.c_str() ); // like "key.pem"
            // set the file with the certs vaildating the server
            if ( ! optsSSL.ca_path.empty() )
                curl_easy_setopt( curl, CURLOPT_CAINFO, optsSSL.ca_path.c_str() );
        } // if( is_ssl_with_explicit_cert_key() )
        if( is_ssl() ) {
            // disconnect if we cannot validate server's cert?
            curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, isSslVerifyPeer_ ? 1L : 0L );
            curl_easy_setopt( curl, CURLOPT_SSL_VERIFYHOST, isSslVerifyHost_ ? 1L : 0L );
        } // if( is_ssl() )
        CURLcode curl_code = curl_easy_perform( curl );
        if( curl_code != CURLE_OK )
            throw std::runtime_error(
                    std::string( "CURL easy perform failed: " ) +
                    curl_easy_strerror( curl_code )
                    );
        long http_code;
        curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, &http_code );
        if( !( http_code == 200 && curl_code != CURLE_ABORTED_BY_CALLBACK ) ) {
            throw std::runtime_error(
                    skutils::tools::format( "CURL failed with code %d=0x%X, HTTP status is  %d=0x%X",
                                            int(curl_code), int(curl_code),
                                            int(http_code), int(http_code)
                                            )
                    );
        }
        char * ct = nullptr;
        curl_easy_getinfo( curl, CURLINFO_CONTENT_TYPE, &ct );
        strOutContentType = ct ? ct : ( strInContentType ? strInContentType : "application/json" );
        strOutData = std::string( chunk.memory, chunk.size );
        //
        ret = true;
    } catch ( skutils::http::common_network_exception& ex ) {
        ei = ex.ei_;
        if ( ei.strError_.empty() )
            ei.strError_ = "exception without description";
        if ( ei.et_ == skutils::http::common_network_exception::error_type::et_no_error )
            ei.et_ = skutils::http::common_network_exception::error_type::et_unknown;
std::cout << "HTTP/CURL got exception(1): " << ei.strError_ << "\n";
std::cout.flush();
    } catch ( std::exception& ex ) {
        ei.strError_ = ex.what();
        if ( ei.strError_.empty() )
            ei.strError_ = "exception without description";
        if ( ei.et_ == skutils::http::common_network_exception::error_type::et_no_error )
            ei.et_ = skutils::http::common_network_exception::error_type::et_unknown;
std::cout << "HTTP/CURL got exception(2): " << ei.strError_ << "\n";
std::cout.flush();
    } catch ( ... ) {
        ei.strError_ = "unknown exception";
        if ( ei.et_ == skutils::http::common_network_exception::error_type::et_no_error )
            ei.et_ = skutils::http::common_network_exception::error_type::et_unknown;
std::cout << "HTTP/CURL got exception(3): " << ei.strError_ << "\n";
std::cout.flush();
    }
    if( headers ) {
        curl_slist_free_all( headers );
        headers = nullptr;
    }
    if( curl ) {
        curl_easy_cleanup( curl );
        curl = nullptr;
    }
    if( chunk.memory ) {
        free( chunk.memory );
        chunk.memory = nullptr;
    }
    // curl_global_cleanup();
    return ret;
}

}; // namespace http_curl

};  // namespace skutils
