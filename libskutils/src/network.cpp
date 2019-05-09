#include <skutils/console_colors.h>
#include <skutils/network.h>
#include <skutils/url.h>
#include <skutils/utils.h>

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
namespace network {

void basic_stream_socket_init( int native_fd, std::ostream& osLog ) {
#if ( defined TCP_KEEPIDLE )
    int timeout = 3;
#endif
    int intvl = 3;
    int probes = 3;
    int on = 1;
    int ret_keepalive =
        ::setsockopt( native_fd, SOL_SOCKET, SO_KEEPALIVE, ( void* ) &on, sizeof( int ) );
#if ( defined TCP_KEEPIDLE )
    int ret_keepidle =
        ::setsockopt( native_fd, IPPROTO_TCP, TCP_KEEPIDLE, ( void* ) &timeout, sizeof( int ) );
#endif
    int ret_keepintvl =
        ::setsockopt( native_fd, IPPROTO_TCP, TCP_KEEPINTVL, ( void* ) &intvl, sizeof( int ) );
    int ret_keepinit =
        ::setsockopt( native_fd, IPPROTO_TCP, TCP_KEEPCNT, ( void* ) &probes, sizeof( int ) );
#if ( defined SO_RCVTIMEO ) && ( defined SO_RCVTIMEO )
    const unsigned int timeout_milli = 20 * 1000;
    struct timeval tv_timeout;
    tv_timeout.tv_sec = timeout_milli / 1000;
    tv_timeout.tv_usec = timeout_milli % 1000;
    const int ret_rcv_timeoout =
        setsockopt( native_fd, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof( tv_timeout ) );
    const int ret_snd_timeoout =
        setsockopt( native_fd, SOL_SOCKET, SO_SNDTIMEO, &tv_timeout, sizeof( tv_timeout ) );
#endif
#if ( defined SO_NOSIGPIPE )
    int no_pipe = 1;
    ::setsockopt( native_fd, SOL_SOCKET, SO_NOSIGPIPE, &no_pipe, sizeof( no_pipe ) );
#endif
#if ( defined TCP_NODELAY )
    int no_delay = 1;
    ::setsockopt( native_fd, IPPROTO_TCP, TCP_NODELAY, ( void* ) &no_delay, sizeof( no_delay ) );
#endif
    if ( ret_keepalive
#if ( defined TCP_KEEPIDLE )
         || ret_keepidle
#endif
         || ret_keepintvl || ret_keepinit
#if ( defined SO_RCVTIMEO ) && ( defined SO_RCVTIMEO )
         || ret_rcv_timeoout || ret_snd_timeoout
#endif
    )
        osLog << cc::error( "Failed to enable keep alive on TCP client socket" ) << native_fd
              << cc::error( "." );
}

std::string get_canonical_host_name() {
    char s[512];
    ::memset( s, 0, sizeof( s ) );
    ::gethostname( s, sizeof( s ) - 1 );
    return s;
}

e_address_family_t get_fd_name_info( int fd, int& nPort, std::string& strAddress, bool isPeer ) {
    nPort = 0;
    strAddress.clear();
    sockaddr_storage addr;
    socklen_t addrLength = sizeof( addr );
    if ( isPeer ) {
        if ( getpeername( fd, ( sockaddr* ) &addr, &addrLength ) == -1 )
            return e_address_family_t::eaft_unknown_or_error;
    } else {
        if ( getsockname( fd, ( sockaddr* ) &addr, &addrLength ) == -1 )
            return e_address_family_t::eaft_unknown_or_error;
    }
    static __thread char buf[INET6_ADDRSTRLEN];
    if ( addr.ss_family == AF_INET ) {
        sockaddr_in* ipv4 = ( sockaddr_in* ) &addr;
        inet_ntop( AF_INET, &ipv4->sin_addr, buf, sizeof( buf ) );
        nPort = ntohs( ipv4->sin_port );
        strAddress = buf;
        return e_address_family_t::eaft_ip_v4;
    } else {
        sockaddr_in6* ipv6 = ( sockaddr_in6* ) &addr;
        inet_ntop( AF_INET6, &ipv6->sin6_addr, buf, sizeof( buf ) );
        nPort = ntohs( ipv6->sin6_port );
        strAddress = buf;
        return e_address_family_t::eaft_ip_v6;
    }
}
std::string get_fd_name_as_url( int fd, const char* strUrlScheme, bool isPeer ) {
    int nPort = 0;
    std::string strAddress;
    e_address_family_t eaft = get_fd_name_info( fd, nPort, strAddress, isPeer );
    switch ( eaft ) {
    case e_address_family_t::eaft_ip_v4:
    case e_address_family_t::eaft_ip_v6:
        return skutils::tools::format( "%s://%s:%d",
            ( strUrlScheme != nullptr && strUrlScheme[0] != '\0' ) ? strUrlScheme : "ws",
            strAddress.c_str(), nPort );
    default:
        return skutils::tools::format( "bad(%s,%d)", isPeer ? "peer" : "sockert", fd );
    }  // switch( eaft )
}

#if ( defined SKUTILS_WITH_SSL )
EVP_PKEY* generate_key( size_t nKeyBits ) {  // generate a nKeyBits-bit RSA key
    EVP_PKEY* pkey = EVP_PKEY_new();         // allocate memory for the EVP_PKEY structure
    if ( !pkey )
        return nullptr;
    RSA* rsa = RSA_generate_key( nKeyBits, RSA_F4, nullptr, nullptr );  // generate the RSA key and
                                                                        // assign it to pkey
    if ( !EVP_PKEY_assign_RSA( pkey, rsa ) ) {
        EVP_PKEY_free( pkey );
        return nullptr;
    }
    return pkey;  // the key has been generated, return it
}
X509* generate_x509( EVP_PKEY* pkey, uint64_t ttExpiration,
    const std::map< std::string, std::string >& mapKeyProperties ) {  // generate a self-signed x509
                                                                      // certificate
    X509* x509 = X509_new();  // allocate memory for the X509 structure
    if ( !x509 )
        return nullptr;
    ASN1_INTEGER_set( X509_get_serialNumber( x509 ), 1 );  // set the serial number
    X509_gmtime_adj( X509_get_notBefore( x509 ), 0 );  // this certificate is valid from now until
                                                       // exactly one year from now
    X509_gmtime_adj( X509_get_notAfter( x509 ), ttExpiration );
    X509_set_pubkey( x509, pkey );  // set the public key for our certificate
    X509_NAME* name =
        X509_get_subject_name( x509 );  // we want to copy the subject name to the issuer name
    std::map< std::string, std::string >::const_iterator itWalk = mapKeyProperties.cbegin(),
                                                         itEnd = mapKeyProperties.cend();
    for ( ; itWalk != itEnd; ++itWalk )
        X509_NAME_add_entry_by_txt( name, itWalk->first.c_str(), MBSTRING_ASC,
            ( unsigned char* ) itWalk->second.c_str(), -1, -1,
            0 );                                   // set the country code and common name
    X509_set_issuer_name( x509, name );            // now set the issuer name
    if ( !X509_sign( x509, pkey, EVP_sha1() ) ) {  // actually sign the certificate with our key
        X509_free( x509 );
        return nullptr;
    }
    return x509;
}
bool write_key_to_disk( EVP_PKEY* pkey, const char* strPathToSaveKey ) {
    FILE* f =
        fopen( ( strPathToSaveKey != nullptr && strPathToSaveKey[0] != '\0' ) ? strPathToSaveKey :
                                                                                "key.pem",
            "wb" );  // open the PEM file for writing the key to disk
    if ( !f )
        return false;
    bool bSaved = PEM_write_PrivateKey(
        f, pkey, nullptr, nullptr, 0, nullptr, nullptr );  // write the key to disk.
    fclose( f );
    return bSaved;
}
bool write_certifcate_to_disk( X509* x509, const char* strPathToSaveCertificate ) {
    FILE* f =
        fopen( ( strPathToSaveCertificate != nullptr && strPathToSaveCertificate[0] != '\0' ) ?
                   strPathToSaveCertificate :
                   "cert.pem",
            "wb" );  // open the PEM file for writing the certificate to disk
    if ( !f )
        return false;
    bool bSaved = PEM_write_X509( f, x509 );  // write the certificate to disk
    fclose( f );
    return bSaved;
}
bool generate_self_signed_certificate_and_key_files( const char* strPathToSaveKey,
    const char* strPathToSaveCertificate, size_t nKeyBits, uint64_t ttExpiration,
    const std::map< std::string, std::string >& mapKeyProperties, bool bSkipIfFilesExist,
    bool* p_bWasSkipped  // = nullptr
) {
    if ( bSkipIfFilesExist ) {
        if ( skutils::tools::file_exists( strPathToSaveKey ) &&
             skutils::tools::file_exists( strPathToSaveCertificate ) ) {
            if ( p_bWasSkipped )
                ( *p_bWasSkipped ) = true;
            return true;
        }
    }
    if ( p_bWasSkipped )
        ( *p_bWasSkipped ) = false;
    // for more details see these links:
    //     https://gist.github.com/nathan-osman/5041136
    //     https://dev.to/ecnepsnai/pragmatically-generating-a-self-signed-certificate-and-private-key-usingopenssl
    // similar command line is:
    //     openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365
    EVP_PKEY* pkey = generate_key( nKeyBits );  // generate the key
    if ( !pkey )
        return false;
    X509* x509 =
        generate_x509( pkey, ttExpiration, mapKeyProperties );  // generate the x509 certificate
    if ( !x509 ) {
        EVP_PKEY_free( pkey );
        return false;
    }
    bool bSaved = ( write_key_to_disk( pkey, strPathToSaveKey ) &&
                      write_certifcate_to_disk( x509, strPathToSaveCertificate ) ) ?
                      true :
                      false;  // write the private key and certificate out to disk
    EVP_PKEY_free( pkey );
    X509_free( x509 );
    return bSaved;
}
bool generate_self_signed_certificate_and_key_files(
    const char* strPathToSaveKey, const char* strPathToSaveCertificate,
    bool bSkipIfFilesExist,  // = true
    bool* p_bWasSkipped      // = nullptr
) {
    if ( bSkipIfFilesExist ) {
        if ( skutils::tools::file_exists( strPathToSaveKey ) &&
             skutils::tools::file_exists( strPathToSaveCertificate ) )
            return true;
    }
    size_t nKeyBits = 1024 * 4;
    static const uint64_t g_one_year_expiration = 60 * 60 * 24 * 365;
    uint64_t ttExpiration = g_one_year_expiration * 1;
    std::map< std::string, std::string > mapKeyProperties;
    mapKeyProperties["C"] = "CA";
    mapKeyProperties["O"] = "MyCompany";
    mapKeyProperties["CN"] = get_canonical_host_name();  // "localhost";
    return generate_self_signed_certificate_and_key_files( strPathToSaveKey,
        strPathToSaveCertificate, nKeyBits, ttExpiration, mapKeyProperties, bSkipIfFilesExist,
        p_bWasSkipped );
}
#endif  // (defined SKUTILS_WITH_SSL)

int get_address_info46( int ipVersion, const char* ads, sockaddr46** result ) {
    struct addrinfo hints;
    ::memset( &hints, 0, sizeof( hints ) );
    ( *result ) = nullptr;
    if ( ipVersion == 6 ) {
        //#if !defined(__ANDROID__)
        hints.ai_family = AF_INET6;
        hints.ai_flags = AI_V4MAPPED;
        //#endif
    } else {
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_CANONNAME;
    }
    return getaddrinfo( ads, nullptr, &hints, ( addrinfo** ) result );
}
std::string resolve_address_for_client_connection(
    int ipVersion, const char* ads, sockaddr46& sa46 ) {
    std::string cce;
    addrinfo* result = nullptr;
    int n = get_address_info46( ipVersion, ads, ( sockaddr46** ) &result );
    if ( ipVersion == 6 ) {
        if ( n ) {
            // get_address_info46 failed, there is no usable result
            cce = "ipv6 get_address_info46 failed";
            goto oom4;
        }
        ::memset( &sa46, 0, sizeof( sockaddr46 ) );
        sa46.sa6.sin6_family = AF_INET6;
        switch ( result->ai_family ) {
        case AF_INET:
            // if( ipv6only )
            break;
            // map IPv4 to IPv6
            ::memset( ( char* ) &sa46.sa6.sin6_addr, 0, sizeof( sa46.sa6.sin6_addr ) );
            sa46.sa6.sin6_addr.s6_addr[10] = 0xff;
            sa46.sa6.sin6_addr.s6_addr[11] = 0xff;
            ::memcpy( &sa46.sa6.sin6_addr.s6_addr[12],
                &( ( struct sockaddr_in* ) result->ai_addr )->sin_addr, sizeof( struct in_addr ) );
            // ...log... ( "uplevelling AF_INET to AF_INET6" );
            break;
        case AF_INET6:
            ::memcpy( &sa46.sa6.sin6_addr, &( ( struct sockaddr_in6* ) result->ai_addr )->sin6_addr,
                sizeof( struct in6_addr ) );
            sa46.sa6.sin6_scope_id = ( ( struct sockaddr_in6* ) result->ai_addr )->sin6_scope_id;
            sa46.sa6.sin6_flowinfo = ( ( struct sockaddr_in6* ) result->ai_addr )->sin6_flowinfo;
            break;
        default:
            // ...log... ( "Unknown address family" );
            freeaddrinfo( result );
            cce = "unknown address family";
            goto oom4;
        }
    } else {
        void* p = nullptr;
        if ( !n ) {
            struct addrinfo* res = result;
            // pick the first AF_INET (IPv4) result
            while ( !p && res ) {
                switch ( res->ai_family ) {
                case AF_INET:
                    p = &( ( struct sockaddr_in* ) res->ai_addr )->sin_addr;
                    break;
                }
                res = res->ai_next;
            }
            //#if defined(LWS_FALLBACK_GETHOSTBYNAME)
        } else if ( n == EAI_SYSTEM ) {
            // ...log... ( "getaddrinfo (ipv4) failed, trying gethostbyname" );
            struct hostent* host = gethostbyname( ads );
            if ( host ) {
                p = host->h_addr;
            } else {
                cce = "gethostbyname (ipv4) failed";
                goto oom4;
            }
            //#endif
        } else {
            cce = "getaddrinfo failed";
            goto oom4;
        }
        if ( !p ) {
            if ( result )
                freeaddrinfo( result );
            cce = "unable to lookup address";
            goto oom4;
        }
        sa46.sa4.sin_family = AF_INET;
        sa46.sa4.sin_addr = *( ( struct in_addr* ) p );
        ::memset( &sa46.sa4.sin_zero, 0, 8 );
    }
    if ( result )
        freeaddrinfo( result );
oom4:
    // we're closing, losing some rx is OK
    return cce;
    // failed:
    // failed1:
    return cce;
}

bool fd_configure_pipe( int fd ) {
    if ( fd == -1 )
        return false;
        // struct timeval tv;
        // tv.tv_sec = 3;  // timeuout
        // tv.tv_usec = 0;
#if ( defined SO_RCVTIMEO ) && ( defined SO_RCVTIMEO )
    const unsigned int timeout_milli = 20 * 1000;
    struct timeval tv_timeout;
    tv_timeout.tv_sec = timeout_milli / 1000;
    tv_timeout.tv_usec = timeout_milli % 1000;
    const int ret_rcv_timeoout =
        ::setsockopt( fd, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof( tv_timeout ) );
    const int ret_snd_timeoout =
        ::setsockopt( fd, SOL_SOCKET, SO_SNDTIMEO, &tv_timeout, sizeof( tv_timeout ) );
    if ( ret_rcv_timeoout || ret_snd_timeoout )
        return false;
#endif
#if ( defined SO_NOSIGPIPE )
    int no_pipe = 1;
    ::setsockopt( fd, SOL_SOCKET, SO_NOSIGPIPE, &no_pipe, sizeof( no_pipe ) );
#endif
#if ( defined TCP_NODELAY )
    int no_delay = 1;
    ::setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, ( void* ) &no_delay, sizeof( no_delay ) );
#endif
    return true;
}

#if ( defined SKUTILS_WITH_SSL )
const std::set< std::string >& get_all_ssl_method_names() {
    static std::set< std::string > g_set( {
    // SSL v2
#if ( OPENSSL_VERSION_NUMBER >= 0x10100000L )
        "sslv2", "sslv2_client", "sslv2_server",
#endif
            // SSL v3
            "sslv3", "sslv3_client", "sslv3_server",
            // TLS v1.0
            "tlsv1", "tlsv1_client", "tlsv1_server",
        // TLS v1.1
#if ( OPENSSL_VERSION_NUMBER >= 0x10100000L ) && ( !defined LIBRESSL_VERSION_NUMBER )
            "tlsv11", "tlsv11_client", "tlsv11_server",
#elif ( defined SSL_TXT_TLSV1_1 )
            "tlsv11", "tlsv11_client", "tlsv11_server",
#else   // defined(SSL_TXT_TLSV1_1)
        // no support for this
#endif  // defined(SSL_TXT_TLSV1_1)
        // TLS v1.2
#if ( OPENSSL_VERSION_NUMBER >= 0x10100000L ) && ( !defined LIBRESSL_VERSION_NUMBER )
            "tlsv12"
            "tlsv12_client",
            "tlsv12_server",
#elif defined( SSL_TXT_TLSV1_1 )
            "tlsv12", "tlsv12_client", "tlsv12_server",
#else   // defined(SSL_TXT_TLSV1_1)
        // no support for this
#endif  // defined(SSL_TXT_TLSV1_1)
        // any supported SSL/TLS version
            "sslv23", "sslv23_client", "sslv23_server",
            // any supported TLS version
            "tls", "tls_client",
            "tls_server"  //,
    } );
    return g_set;
}

const SSL_METHOD* SSL_METHOD_ptr_from_name( const char* strSslMethodName ) {
    if ( strSslMethodName == nullptr || strSslMethodName[0] == '\0' )
        return nullptr;
        // SSL v2
#if ( OPENSSL_VERSION_NUMBER >= 0x10100000L )
    if ( strcasecmp( strSslMethodName, "sslv2" ) == 0 )
        return ::SSLv2_method();
    if ( strcasecmp( strSslMethodName, "sslv2_client" ) == 0 )
        return ::SSLv2_client_method();
    if ( strcasecmp( strSslMethodName, "sslv2_server" ) == 0 )
        return ::SSLv2_server_method();
#endif
        // SSL v3
#if ( OPENSSL_VERSION_NUMBER >= 0x10100000L ) && ( !defined LIBRESSL_VERSION_NUMBER )
    if ( strcasecmp( strSslMethodName, "sslv3" ) == 0 )
        return ::TLS_method();
    if ( strcasecmp( strSslMethodName, "sslv3_client" ) == 0 )
        return ::TLS_client_method();
    if ( strcasecmp( strSslMethodName, "sslv3_server" ) == 0 )
        return ::TLS_server_method();
#else
    if ( strcasecmp( strSslMethodName, "sslv3" ) == 0 )
        return ::SSLv3_method();
    if ( strcasecmp( strSslMethodName, "sslv3_client" ) == 0 )
        return ::SSLv3_client_method();
    if ( strcasecmp( strSslMethodName, "sslv3_server" ) == 0 )
        return ::SSLv3_server_method();
#endif  // defined(OPENSSL_NO_SSL3)
        // TLS v1.0
#if ( OPENSSL_VERSION_NUMBER >= 0x10100000L ) && ( !defined LIBRESSL_VERSION_NUMBER )
    if ( strcasecmp( strSslMethodName, "tlsv1" ) == 0 )
        return ::TLS_method();
    if ( strcasecmp( strSslMethodName, "tlsv1_client" ) == 0 )
        return ::TLS_client_method();
    if ( strcasecmp( strSslMethodName, "tlsv1_server" ) == 0 )
        return ::TLS_server_method();
#else   // (OPENSSL_VERSION_NUMBER >= 0x10100000L)
    if ( strcasecmp( strSslMethodName, "tlsv1" ) == 0 )
        return ::TLSv1_method();
    if ( strcasecmp( strSslMethodName, "tlsv1_client" ) == 0 )
        return ::TLSv1_client_method();
    if ( strcasecmp( strSslMethodName, "tlsv1_server" ) == 0 )
        return ::TLSv1_server_method();
#endif  // (OPENSSL_VERSION_NUMBER >= 0x10100000L)
        // TLS v1.1
#if ( OPENSSL_VERSION_NUMBER >= 0x10100000L ) && ( !defined LIBRESSL_VERSION_NUMBER )
    if ( strcasecmp( strSslMethodName, "tlsv11" ) == 0 )
        return ::TLS_method();
    if ( strcasecmp( strSslMethodName, "tlsv11_client" ) == 0 )
        return ::TLS_client_method();
    if ( strcasecmp( strSslMethodName, "tlsv11_server" ) == 0 )
        return ::TLS_server_method();
#elif ( defined SSL_TXT_TLSV1_1 )
    if ( strcasecmp( strSslMethodName, "tlsv11" ) == 0 )
        return ::TLSv1_1_method();
    if ( strcasecmp( strSslMethodName, "tlsv11_client" ) == 0 )
        return ::TLSv1_1_client_method();
    if ( strcasecmp( strSslMethodName, "tlsv11_server" ) == 0 )
        return ::TLSv1_1_server_method();
#else   // defined(SSL_TXT_TLSV1_1)
        // no support for this
#endif  // defined(SSL_TXT_TLSV1_1)
        // TLS v1.2
#if ( OPENSSL_VERSION_NUMBER >= 0x10100000L ) && ( !defined LIBRESSL_VERSION_NUMBER )
    if ( strcasecmp( strSslMethodName, "tlsv12" ) == 0 )
        return ::TLS_method();
    if ( strcasecmp( strSslMethodName, "tlsv12_client" ) == 0 )
        return ::TLS_client_method();
    if ( strcasecmp( strSslMethodName, "tlsv12_server" ) == 0 )
        return ::TLS_server_method();
#elif defined( SSL_TXT_TLSV1_1 )
    if ( strcasecmp( strSslMethodName, "tlsv12" ) == 0 )
        return ::TLSv1_2_method();
    if ( strcasecmp( strSslMethodName, "tlsv12_client" ) == 0 )
        return ::TLSv1_2_client_method();
    if ( strcasecmp( strSslMethodName, "tlsv12_server" ) == 0 )
        return ::TLSv1_2_server_method();
#else   // defined(SSL_TXT_TLSV1_1)
        // no support for this
#endif  // defined(SSL_TXT_TLSV1_1)
        // any supported SSL/TLS version
    if ( strcasecmp( strSslMethodName, "sslv23" ) == 0 )
        return ::SSLv23_method();
    if ( strcasecmp( strSslMethodName, "sslv23_client" ) == 0 )
        return ::SSLv23_client_method();
    if ( strcasecmp( strSslMethodName, "sslv23_server" ) == 0 )
        return ::SSLv23_server_method();
        // any supported TLS version
#if ( OPENSSL_VERSION_NUMBER >= 0x10100000L ) && ( !defined LIBRESSL_VERSION_NUMBER )
    if ( strcasecmp( strSslMethodName, "tls" ) == 0 )
        return ::TLS_method();
    if ( strcasecmp( strSslMethodName, "tls_client" ) == 0 )
        return ::TLS_client_method();
    if ( strcasecmp( strSslMethodName, "tls_server" ) == 0 )
        return ::TLS_server_method();
#else
    if ( strcasecmp( strSslMethodName, "tls" ) == 0 )
        return ::SSLv23_method();
    if ( strcasecmp( strSslMethodName, "tls_client" ) == 0 )
        return ::SSLv23_client_method();
    if ( strcasecmp( strSslMethodName, "tls_server" ) == 0 )
        return ::SSLv23_server_method();
#endif
    return nullptr;
}

std::string ssl_elaborate_error() {
    std::stringstream ss;
    char buf[1024 * 4];
    u_long err;
    for ( size_t i = 0; ( err = ERR_get_error() ) != 0; ++i ) {
        ERR_error_string_n( err, buf, sizeof( buf ) );
        if ( i > 0 )
            ss << "; ";
        ss << buf;
    }
    return ss.str();
}

std::string x509_2_str( X509_NAME* x509_obj ) {
    char buf[1024 * 4];
    ::memset( buf, 0, sizeof( buf ) );
    return X509_NAME_oneline( x509_obj, buf, sizeof( buf ) );
}
#endif  // (defined SKUTILS_WITH_SSL)

std::list< std::pair< std::string, std::string > > get_machine_ip_addresses(
    bool is4, bool is6 ) {  // first-interface name, second-address
    std::list< std::pair< std::string, std::string > > lst;
    if ( !( is4 || is6 ) )
        return lst;
    struct ifaddrs* ifAddrStruct = nullptr;
    struct ifaddrs* ifa = nullptr;
    void* tmpAddrPtr = nullptr;
    ::getifaddrs( &ifAddrStruct );
    for ( ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next ) {
        if ( !ifa->ifa_addr ) {
            // ifa->ifa_addr can be NULL under VPN
        } else if ( ifa->ifa_addr->sa_family == AF_INET ) {  // check it is IP4
            if ( is4 ) {
                // is a valid IP4 Address
                tmpAddrPtr = &( ( struct sockaddr_in* ) ifa->ifa_addr )->sin_addr;
                char addressBuffer[INET_ADDRSTRLEN];
                inet_ntop( AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN );
                // printf( "'%s': %s\n", ifa->ifa_name, addressBuffer );
                std::pair< std::string, std::string > x( ifa->ifa_name, addressBuffer );
                lst.push_back( x );
            }
        } else if ( ifa->ifa_addr->sa_family == AF_INET6 ) {  // check it is IP6
            if ( is6 ) {
                // is a valid IP6 Address
                tmpAddrPtr = &( ( struct sockaddr_in6* ) ifa->ifa_addr )->sin6_addr;
                char addressBuffer[INET6_ADDRSTRLEN];
                inet_ntop( AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN );
                // printf( "'%s': %s\n", ifa->ifa_name, addressBuffer );
                std::pair< std::string, std::string > x( ifa->ifa_name, addressBuffer );
                lst.push_back( x );
            }
        }
    }  // for( ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next )
    if ( ifAddrStruct != nullptr )
        freeifaddrs( ifAddrStruct );
    return lst;
}
static std::pair< std::string, std::string > stat_find_ip_addr_non_local(
    const std::list< std::pair< std::string, std::string > >& lst ) {
    std::pair< std::string, std::string > x;
    std::list< std::pair< std::string, std::string > >::const_iterator itWalk = lst.cbegin(),
                                                                       itEnd = lst.cend();
    for ( ; itWalk != itEnd; ++itWalk ) {
        const std::string& strIF = itWalk->first;
        if ( strIF == "lo" || strncmp( strIF.c_str(), "virbr", 5 ) == 0 ||
             strncmp( strIF.c_str(), "docker", 6 ) == 0 ||
             strncmp( strIF.c_str(), "veth", 4 ) == 0 )
            continue;
        const std::string& strAdd = itWalk->second;
        if ( strAdd == "127.0.0.1" || strAdd == "::1" ||
             strncmp( strAdd.c_str(), "172.", 4 ) == 0 ||
             strncmp( strAdd.c_str(), "192.", 4 ) == 0 ||
             strncmp( strAdd.c_str(), "10.", 3 ) == 0 ||
             strncmp( strAdd.c_str(), "fe80::", 6 ) == 0 )
            continue;
        x = ( *itWalk );
        return x;
    }
    itWalk = lst.cbegin();
    itEnd = lst.cend();
    for ( ; itWalk != itEnd; ++itWalk ) {
        const std::string& strIF = itWalk->first;
        if ( strIF == "lo" || strncmp( strIF.c_str(), "virbr", 5 ) == 0 ||
             strncmp( strIF.c_str(), "docker", 6 ) == 0 ||
             strncmp( strIF.c_str(), "veth", 4 ) == 0 )
            continue;
        const std::string& strAdd = itWalk->second;
        if ( strAdd == "127.0.0.1" || strAdd == "::1" )
            continue;
        x = ( *itWalk );
        return x;
    }
    return x;
}
std::pair< std::string, std::string > get_machine_ip_address_v4() {  // first-interface name,
                                                                     // second-address
    std::list< std::pair< std::string, std::string > > lst =
        get_machine_ip_addresses( true, false );  // first-interface name, second-address
    return stat_find_ip_addr_non_local( lst );
}
std::pair< std::string, std::string > get_machine_ip_address_v6() {  // first-interface name,
                                                                     // second-address
    std::list< std::pair< std::string, std::string > > lst =
        get_machine_ip_addresses( false, true );  // first-interface name, second-address
    return stat_find_ip_addr_non_local( lst );
}

};  // namespace network
};  // namespace skutils
