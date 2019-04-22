// note: based on
// https://github.com/chmike/CxxUrl
// git@github.com:chmike/CxxUrl.git

#if ( !defined SKUTILS_URL_H )
#define SKUTILS_URL_H 1

#include <cstdint>
#include <list>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace skutils {
std::string cutUriPort( const std::string& uri );
extern std::list< std::string > split2list(
    const std::string& s, char delim, size_t nMaxSplits = std::string::npos );
extern std::vector< std::string > split2vector(
    const std::string& s, char delim, size_t nMaxSplits = std::string::npos );
extern bool is_char( char c, std::uint8_t mask );
extern bool is_chars( const char* s, const char* e, std::uint8_t mask );
extern bool is_alpha( char c );
extern bool is_number( char c );
extern bool is_alpha_or_number( char c );
extern bool is_heximal_digit( char c );
extern bool is_uint( const char*& s, const char* e, uint32_t max );
extern bool is_uint( const std::string& s, uint32_t max );
extern bool is_pchars( const char* s, const char* e );
extern void to_lower( std::string& s );
extern const char* find_first_of( const char* s, const char* e, const char* q );
extern const char* find_char( const char* s, const char* e, const char c );
extern inline bool is_scheme( const std::string& s );
extern std::string normalize_scheme( const char* b, const char* e );

extern bool is_ipv4( const char* s, const char* e );
extern bool is_ipv4( const std::string& s );
extern bool is_valid_ipv4( const char* s, const char* e );
extern bool is_valid_ipv4( const std::string& s );
extern bool is_ipv6( const char* s, const char* e );
extern bool is_ipv6( const std::string& s );
extern bool is_valid_ipv6( const char* s, const char* e );
extern bool is_valid_ipv6( const std::string& s );
extern std::string normalize_ipv6( const char* s, const char* e );
extern std::string normalize_ipv6( const std::string& s );
extern bool is_good_url_port( const char* s, const char* e );
extern bool is_good_url_port( const char* s );
extern bool is_good_url_port( const std::string& s );
extern std::string normalize_url_path( const std::string& s );

class url {
public:
    static std::string stat_do_encode( const std::string& s, std::uint8_t mask );
    static std::string stat_do_encode_query_key( const std::string& s, std::uint8_t mask );
    static std::string stat_do_encode_query_val( const std::string& s, std::uint8_t mask );
    class parse_error : public std::invalid_argument {  // may be thrown when decoding an URL or an
                                                        // assigning value
    public:
        parse_error( const std::string& reason ) : std::invalid_argument( reason ) {}
    };  /// class parse_error

    class compose_error : public std::runtime_error {  // may be thrown when composing an URL
    public:
        compose_error( const std::string& reason ) : std::runtime_error( reason ) {}
    };  /// class compose_error

    url() : wasParsed_( true ), wasBuilt_( true ), nIPversion_( -1 ) {}
    url( const url& url ) : nIPversion_( -1 ) { assign( url ); }
    url( url&& an_url ) : nIPversion_( -1 ) { assign( std::move( an_url ) ); }
    url( const std::string& url_str )
        : strUrl_( url_str ), wasParsed_( false ), wasBuilt_( false ), nIPversion_( -1 ) {}
    url& operator=( const std::string& url_str ) { return str( url_str ); }
    url& operator=( const url& an_url ) {
        assign( an_url );
        return ( *this );
    }
    url& operator=( url&& an_url ) {
        assign( std::move( an_url ) );
        return ( *this );
    }
    int compare( const url& other ) const {
        std::string l = str(), r = other.str();
        return l.compare( r );
    }
    bool operator==( const url& other ) const { return ( compare( other ) == 0 ) ? true : false; }
    bool operator!=( const url& other ) const { return ( compare( other ) != 0 ) ? true : false; }
    bool operator<( const url& other ) const { return ( compare( other ) < 0 ) ? true : false; }
    bool operator<=( const url& other ) const { return ( compare( other ) <= 0 ) ? true : false; }
    bool operator>( const url& other ) const { return ( compare( other ) > 0 ) ? true : false; }
    bool operator>=( const url& other ) const { return ( compare( other ) >= 0 ) ? true : false; }

    url& clear();

    std::string str() const {
        if ( !wasBuilt_ )
            compose_url();
        return strUrl_;
    }  // compose if needed and return it as string
    url& str( const std::string& url_str ) {
        strUrl_ = url_str;
        wasBuilt_ = wasParsed_ = false;
        return ( *this );
    }  // all fields are overwritten

    const std::string& scheme() const {
        lazy_parse();
        return strScheme_;
    }
    url& scheme( const std::string& s );

    const std::string& user_info() const {
        lazy_parse();
        return strUserName_;
    }
    url& user_info( const std::string& s );

    std::pair< std::string, std::string > user_name_and_password() const;  // returns strings pair
                                                                           // object, not referemnce
    url& user_name_and_password(
        const std::pair< std::string, std::string >& n_p, bool bRemoveIfEmpty );
    url& user_name_and_password(
        const std::string& strName, const std::string& strPassword, bool bRemoveIfEmpty );
    std::string user_name() const;  // returns string object, not referemnce
    url& user_name( const std::string& s );
    std::string user_password() const;  // returns string object, not referemnce
    url& user_password( const std::string& s );

    const std::string& host() const {
        lazy_parse();
        return strHost_;
    }
    url& host( const std::string& h, uint8_t ip_v = 0 );

    std::int8_t ip_version() const {
        lazy_parse();
        return nIPversion_;
    }  // get host IP version: 0=name, 4=IPv4, 6=IPv6, -1=undefined

    const std::string& port() const {
        lazy_parse();
        return strPort_;
    }
    url& port( const std::string& str );
    url& port( std::uint16_t num );

    const std::string& path() const {
        lazy_parse();
        return strPath_;
    }
    url& path( const std::string& str );

    class KeyVal {
    public:
        KeyVal() {}
        KeyVal( const std::string& key, const std::string& val ) : m_key( key ), m_val( val ) {}
        KeyVal( const std::string& key ) : m_key( key ) {}  // val will be empty
        bool operator==( const KeyVal& q ) const { return m_key == q.m_key && m_val == q.m_val; }

        void swap( KeyVal& q ) {
            std::swap( m_key, q.m_key );
            std::swap( m_val, q.m_val );
        }

        const std::string& key() const { return m_key; }
        void key( const std::string& k ) { m_key = k; }

        const std::string& val() const { return m_val; }
        void val( const std::string& v ) { m_val = v; }

        friend std::ostream& operator<<( std::ostream& o, const KeyVal& kv ) {
            o << "<key(" << kv.m_key << ") val(" << kv.m_val << ")> ";
            return o;
        }

    private:
        std::string m_key;
        std::string m_val;
    };  /// class KeyVal

    typedef std::vector< KeyVal > Query;

    // Get a reference to the query vector for read only access
    const Query& query() const {
        lazy_parse();
        return kvQuery_;
    }

    const KeyVal& query( size_t i ) const {
        lazy_parse();
        if ( i >= kvQuery_.size() ) {
            std::stringstream ss;
            ss << "invalid Url query index (" << i << ")";
            throw std::out_of_range( ss.str() );
        }
        return kvQuery_[i];
    }
    Query& set_query() {
        lazy_parse();
        wasBuilt_ = false;
        return kvQuery_;
    }
    KeyVal& set_query( size_t i ) {
        lazy_parse();
        if ( i >= kvQuery_.size() ) {
            std::stringstream ss;
            ss << "invalid Url query index (" << i << ")";
            throw std::out_of_range( ss.str() );
        }
        wasBuilt_ = false;
        return kvQuery_[i];
    }
    url& set_query( const Query& q ) {
        lazy_parse();
        if ( q != kvQuery_ ) {
            kvQuery_ = q;
            wasBuilt_ = false;
        }
        return ( *this );
    }
    url& add_query( const KeyVal& kv ) {
        lazy_parse();
        wasBuilt_ = false;
        kvQuery_.push_back( kv );
        return ( *this );
    }
    url& add_query( const std::string& key, const std::string& val ) {
        lazy_parse();
        wasBuilt_ = false;
        kvQuery_.emplace_back( key, val );
        return ( *this );
    }
    url& add_query( const std::string& key ) {
        lazy_parse();
        wasBuilt_ = false;
        kvQuery_.emplace_back( key );
        return ( *this );
    }

    const std::string& fragment() const {
        lazy_parse();
        return strFragment_;
    }
    url& fragment( const std::string& f );

    std::ostream& output( std::ostream& o ) const;
    friend std::ostream& operator<<( std::ostream& o, const url& an_url ) {
        return an_url.output( o );
    }

private:
    void assign( const url& an_url );
    void assign( url&& an_url );
    void compose_url() const;
    void lazy_parse() const {
        if ( !wasParsed_ )
            parse_url();
    }
    void parse_url() const;
    mutable std::string strScheme_;
    mutable std::string strUserName_;
    mutable std::string strHost_;
    mutable std::string strPort_;
    mutable std::string strPath_;
    mutable Query kvQuery_;
    mutable std::string strFragment_;
    mutable std::string strUrl_;
    mutable bool wasParsed_;
    mutable bool wasBuilt_;
    mutable std::int8_t nIPversion_;

public:
    bool is_local_private_network_address() const;
    bool update_user_name_and_password( const char* strOptionalUser,
        const char* strOptionalPassword, bool bRemoveIfEmpty );  // returns true if was updated
    bool update_user_name_and_password( const std::string& strOptionalUser,
        const std::string& strOptionalPassword,
        bool bRemoveIfEmpty );            // returns true if was updated
    bool strip_user_name_and_password();  // returns true if was updated
};                                        /// class url

extern bool update_url_user_name_and_password( url& an_url, const char* strOptionalUser,
    const char* strOptionalPassword, bool bRemoveIfEmpty );
extern bool update_url_user_name_and_password( url& an_url, const std::string& strOptionalUser,
    const std::string& strOptionalPassword, bool bRemoveIfEmpty );
extern bool update_url_user_name_and_password( std::string& str_url, const char* strOptionalUser,
    const char* strOptionalPassword, bool bRemoveIfEmpty );
extern bool update_url_user_name_and_password( std::string& str_url,
    const std::string& strOptionalUser, const std::string& strOptionalPassword,
    bool bRemoveIfEmpty );
extern std::string update_url_user_name_and_password_copy( const std::string& str_url,
    const char* strOptionalUser, const char* strOptionalPassword, bool bRemoveIfEmpty );
extern std::string update_url_user_name_and_password_copy( const std::string& str_url,
    const std::string& strOptionalUser, const std::string& strOptionalPassword,
    bool bRemoveIfEmpty );
extern bool strip_url_user_name_and_password( url& an_url );
extern bool strip_url_user_name_and_password( std::string& str_url );
extern std::string strip_url_user_name_and_password_copy( const std::string& str_url );

// the following group of methods are accepting IP address as parameter, not URL
extern bool is_local_private_network_address_ipv4( const std::string& s );
extern bool is_local_private_network_address_ipv4( const char* s, const char* e );
extern bool is_local_private_network_address_ipv6( const std::string& s );
extern bool is_local_private_network_address_ipv6( const char* s, const char* e );
extern bool is_local_private_network_address( const std::string& s );
extern bool is_local_private_network_address( const char* s, const char* e );

// the following group of methods are accepting URL as parameter, not IP address
extern bool is_local_private_network_url( const url& an_url );
extern bool is_local_private_network_url( const std::string& str_url );
extern bool is_local_private_network_url( const char* s, const char* e );

};  // namespace skutils

#endif  /// SKUTILS_URL_H
