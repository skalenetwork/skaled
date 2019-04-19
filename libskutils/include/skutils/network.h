#if ( !defined __SKUTILS_NETWOKR_H )
#define __SKUTILS_NETWOKR_H 1

#include <inttypes.h>
#include <algorithm>
#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

//#include <nlohmann/json.hpp>
#include <json.hpp>

//#define SKUTILS_WITH_SSL 1

extern "C" {
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#if ( defined SKUTILS_WITH_SSL )
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#endif  // (defined SKUTILS_WITH_SSL)
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
};

namespace skutils {
namespace network {

extern void basic_stream_socket_init( int native_fd, std::ostream& osLog );

extern std::string get_canonical_host_name();

enum class e_address_family_t {
    eaft_unknown_or_error,
    eaft_ip_v4,
    eaft_ip_v6,
};  /// enum class e_address_family_t
extern e_address_family_t get_fd_name_info(
    int fd, int& nPort, std::string& strAddress, bool isPeer );
extern std::string get_fd_name_as_url( int fd, const char* strUrlScheme, bool isPeer );

#if ( defined SKUTILS_WITH_SSL )
extern bool generate_self_signed_certificate_and_key_files( const char* strPathToSaveKey,
    const char* strPathToSaveCertificate, size_t nKeyBits, uint64_t ttExpiration,
    const std::map< std::string, std::string >& mapKeyProperties, bool bSkipIfFilesExist,
    bool* p_bWasSkipped = nullptr );
extern bool generate_self_signed_certificate_and_key_files( const char* strPathToSaveKey,
    const char* strPathToSaveCertificate, bool bSkipIfFilesExist = true,
    bool* p_bWasSkipped = nullptr );
#endif  // (defined SKUTILS_WITH_SSL)

typedef union {
    struct sockaddr_in6 sa6;
    struct sockaddr_in sa4;
} sockaddr46;
extern int get_address_info46( int ipVersion, const char* ads, sockaddr46** result );
extern std::string resolve_address_for_client_connection(
    int ipVersion, const char* ads, sockaddr46& sa46 );

extern bool fd_configure_pipe( int fd );

#if ( defined SKUTILS_WITH_SSL )
extern const std::set< std::string >& get_all_ssl_method_names();
extern const SSL_METHOD* SSL_METHOD_ptr_from_name( const char* strSslMethodName );

extern std::string ssl_elaborate_error();
extern std::string x509_2_str( X509_NAME* x509_obj );
#endif  // (defined SKUTILS_WITH_SSL)

extern std::list< std::pair< std::string, std::string > > get_machine_ip_addresses(
    bool is4, bool is6 );  // first-interface name, second-address
extern std::pair< std::string, std::string > get_machine_ip_address_v4();  // first-interface name,
                                                                           // second-address
extern std::pair< std::string, std::string > get_machine_ip_address_v6();  // first-interface name,
                                                                           // second-address

};  // namespace network
};  // namespace skutils

#endif  /// (!defined __SKUTILS_NETWOKR_H)
