// note: based on
// https://github.com/chmike/CxxUrl
// git@github.com:chmike/CxxUrl.git

#include <skutils/url.h>
#include <string.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <list>
#include <sstream>
#include <string>
#include <vector>

namespace skutils {
std::string cutUriPort( const std::string& uri ) {
    const std::string::size_type dotsPosition = uri.rfind( ':' );
    const std::string::size_type slashPosition = uri.rfind( '/' );
    return ( dotsPosition > slashPosition ) ? uri.substr( 0, dotsPosition ) : uri;
}


std::list< std::string > split2list(
    const std::string& s, char delim, size_t nMaxSplits /*= std::string::npos*/ ) {
    std::list< std::string > lst;
    if ( nMaxSplits == std::string::npos ) {
        std::stringstream ss( s );
        std::string item;
        while ( std::getline( ss, item, delim ) ) {
            lst.push_back( item );
        }
        return lst;
    }
    lst = split2list( s, delim );
    size_t cnt = lst.size();
    if ( cnt <= nMaxSplits )
        return lst;
    std::string strAccum;
    std::list< std::string > lst2;
    std::list< std::string >::const_iterator itWalk = lst.begin(), itEnd = lst.end();
    for ( size_t i = 0; itWalk != itEnd; ++itWalk, ++i ) {
        const std::string& strWalk = ( *itWalk );
        if ( i < cnt ) {
            lst2.push_back( strWalk );
            continue;
        }
        if ( i > cnt )
            strAccum += delim;
        strAccum += strWalk;
    }
    lst2.push_back( strAccum );
    return lst2;
}
std::vector< std::string > split2vector(
    const std::string& s, char delim, size_t nMaxSplits /*= std::string::npos*/ ) {
    std::list< std::string > lst = split2list( s, delim, nMaxSplits );
    std::vector< std::string > vec;
    vec.insert( vec.end(), lst.begin(), lst.end() );
    return vec;
}

static const uint8_t g_tbl[256] = {0, 0, 0, 0, 0, 0, 0, 0,  // NUL SOH STX ETX  EOT ENQ ACK BEL
    0, 0, 0, 0, 0, 0, 0, 0,                                 // BS  HT  LF  VT   FF  CR  SO  SI
    0, 0, 0, 0, 0, 0, 0, 0,                                 // DLE DC1 DC2 DC3  DC4 NAK SYN ETB
    0, 0, 0, 0, 0, 0, 0, 0,                                 // CAN EM  SUB ESC  FS  GS  RS  US
    0x00, 0x01, 0x00, 0x00, 0x01, 0x20, 0x01, 0x01,         // SP ! " #  $ % & '
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x08,         //  ( ) * +  , - . /
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,         //  0 1 2 3  4 5 6 7
    0x01, 0x01, 0x04, 0x01, 0x00, 0x01, 0x00, 0x10,         //  8 9 : ;  < = > ?
    0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,         //  @ A B C  D E F G
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,         //  H I J K  L M N O
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,         //  P Q R S  T U V W
    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,         //  X Y Z [  \ ] ^ _
    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,         //  ` a b c  d e f g
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,         //  h i j k  l m n o
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,         //  p q r s  t u v w
    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,         //  x y z {  | } ~ DEL
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
bool is_char( char c, std::uint8_t mask ) {
    return ( g_tbl[static_cast< unsigned char >( c )] & mask ) != 0;
}
bool is_chars( const char* s, const char* e, std::uint8_t mask ) {
    while ( s != e )
        if ( !is_char( *s++, mask ) )
            return false;
    return true;
}
bool is_alpha( char c ) {
    return ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' );
}
bool is_number( char c ) {
    return c >= '0' && c <= '9';
}
bool is_alpha_or_number( char c ) {
    return is_alpha( c ) || is_number( c );
}
bool is_heximal_digit( char c ) {
    return is_number( c ) || ( c >= 'A' && c <= 'F' ) || ( c >= 'a' && c <= 'f' );
}
bool is_uint( const char*& s, const char* e, uint32_t max ) {
    if ( s == e || ( !is_number( *s ) ) )
        return false;
    const char* t = s;
    uint32_t val = uint32_t( *t ) - uint32_t( '0' );
    ++t;
    if ( val )
        while ( t != e && is_number( *t ) ) {
            val = val * 10 + ( uint32_t( *t ) - uint32_t( '0' ) );
            ++t;
        }
    if ( val > max )
        return false;
    s = t;
    return true;
}
bool is_uint( const std::string& s, uint32_t max ) {
    const char* p = s.c_str();
    return is_uint( p, p + s.length(), max );
}
bool is_pchars( const char* s, const char* e ) {
    return is_chars( s, e, 0x07 );
}
static inline char stat_get_hex_digit( char c ) {
    if ( c >= '0' && c <= '9' )
        return static_cast< char >( c - '0' );
    if ( c >= 'A' && c <= 'F' )
        return static_cast< char >( c - 'A' + static_cast< char >( 10 ) );
    if ( c >= 'a' && c <= 'f' )
        return static_cast< char >( c - 'a' + static_cast< char >( 10 ) );
    return static_cast< char >( -1 );
}
void to_lower( std::string& s ) {
    for ( auto& c : s )
        if ( c >= 'A' && c <= 'Z' )
            c |= 0x20;
}
const char* find_first_of( const char* s, const char* e, const char* q ) {
    for ( ; s != e; ++s )
        for ( const char* t = q; *t; ++t )
            if ( *s == *t )
                return s;
    return e;
}
const char* find_char( const char* s, const char* e, const char c ) {
    while ( s != e && *s != c )
        ++s;
    return s;
}
static inline bool is_scheme( const char* s, const char* e ) {
    if ( ( !s ) || ( !e ) || s == e || ( !is_alpha( *s ) ) )
        return false;
    char c;
    while ( ++s != e )
        if ( !is_alpha_or_number( c = *s ) && c != '+' && c != '-' && c != '.' )
            return false;
    return true;
}
inline bool is_scheme( const std::string& s ) {
    return is_scheme( s.c_str(), s.c_str() + s.length() );
}
std::string normalize_scheme( const char* b, const char* e ) {
    std::string o( b, size_t( e - b ) );
    to_lower( o );
    return o;
}
bool is_ipv4( const char* s, const char* e ) {
    size_t l = size_t( e - s );
    if ( l < 7 || l > 254 )
        return false;
    for ( const char* p = s; p != e; ++p )
        if ( *p != '.' && ( !is_number( *p ) ) )
            return false;
    return true;
}
bool is_ipv4( const std::string& s ) {
    return is_ipv4( s.c_str(), s.c_str() + s.length() );
}
bool is_valid_ipv4( const char* s, const char* e ) {
    return is_uint( s, e, 255 ) && s != e && *s++ == '.' && is_uint( s, e, 255 ) && s != e &&
           *s++ == '.' && is_uint( s, e, 255 ) && s != e && *s++ == '.' && is_uint( s, e, 255 ) &&
           s == e;
}
bool is_valid_ipv4( const std::string& s ) {
    return is_valid_ipv4( s.c_str(), s.c_str() + s.length() );
}
static inline bool stat_is_reg_name( const char* s, const char* e ) {
    return is_chars( s, e, 0x01 );
}
static inline bool stat_is_reg_name( const std::string& s ) {
    return stat_is_reg_name( s.c_str(), s.c_str() + s.length() );
}
static inline std::string stat_normalize_reg_name( const std::string& s ) {
    std::string o( s );
    to_lower( o );  // see rfc 4343
    return o;
}
bool is_ipv6( const char* s, const char* e ) {
    size_t l = size_t( e - s );
    if ( l < 2 || l > 254 )
        return false;
    for ( const char* p = s; p != e; ++p )
        if ( *p != ':' && *p != '.' && ( !is_heximal_digit( *p ) ) )
            return false;
    return true;
}
bool is_ipv6( const std::string& s ) {
    return is_ipv6( s.c_str(), s.c_str() + s.length() );
}
bool is_valid_ipv6( const char* s, const char* e ) {
    if ( ( e - s ) > 39 || ( e - s ) < 2 )
        return false;
    bool null_field = false;
    const char *b = s, *p = s;
    int nfields = 0, ndigits = 0;
    if ( p[0] == ':' ) {
        if ( p[1] != ':' )
            return false;
        null_field = true;
        b = ( p += 2 );
        if ( p == e )
            return true;
    }
    while ( p != e ) {
        if ( *p == '.' ) {
            return ( ( !null_field && nfields == 6 ) || ( null_field && nfields < 7 ) ) &&
                   is_valid_ipv4( b, e );
        } else if ( *p == ':' ) {
            if ( ndigits == 0 ) {
                if ( null_field )
                    return false;
                null_field = true;
            } else {
                ++nfields;
                ndigits = 0;
            }
            b = ++p;
        } else {
            if ( ( ++ndigits > 4 ) || !is_heximal_digit( *p++ ) )
                return false;
        }
    }
    if ( ndigits > 0 )
        ++nfields;
    else {
        if ( e[-1] == ':' ) {
            if ( e[-2] == ':' && nfields < 8 )
                return true;
            return false;
        }
    }
    return ( !null_field && nfields == 8 ) || ( null_field && nfields < 8 );
}
bool is_valid_ipv6( const std::string& s ) {
    return is_valid_ipv6( s.c_str(), s.c_str() + s.length() );
}
std::string normalize_ipv6( const char* s, const char* e ) {
    if ( !is_ipv6( s, e ) )
        throw url::parse_error( "IPv6 [" + std::string( s, e - s ) + "] is invalid" );
    if ( ( e - s ) == 2 && s[0] == ':' && s[1] == ':' )
        return std::string( s, e - s );
    // split IPv6 at colons
    const char *p = s, *tokens[10];
    if ( *p == ':' )
        ++p;
    if ( e[-1] == ':' )
        --e;
    const char* b = p;
    size_t i = 0;
    while ( p != e ) {
        if ( *p++ == ':' ) {
            tokens[i++] = b;
            b = p;
        }
    }
    if ( i < 8 )
        tokens[i++] = b;
    tokens[i] = p;
    size_t ntokens = i;
    // get IPv4 address which is normalized by default
    const char *ipv4_b = nullptr, *ipv4_e = nullptr;
    if ( ( tokens[ntokens] - tokens[ntokens - 1] ) > 5 ) {
        ipv4_b = tokens[ntokens - 1];
        ipv4_e = tokens[ntokens];
        --ntokens;
    }
    // decode the fields
    std::uint16_t fields[8];
    size_t null_pos = 8, null_len = 0, nfields = 0;
    for ( size_t i = 0; i < ntokens; ++i ) {
        const char* p = tokens[i];
        if ( p == tokens[i + 1] || *p == ':' )
            null_pos = i;
        else {
            std::uint16_t field = std::uint16_t( stat_get_hex_digit( *p++ ) );
            while ( p != tokens[i + 1] && *p != ':' )
                field = std::uint16_t( field << 4 ) | std::uint16_t( stat_get_hex_digit( *p++ ) );
            fields[nfields++] = field;
        }
    }
    i = nfields;
    nfields = ( ipv4_b ) ? 6 : 8;
    if ( i < nfields ) {
        size_t last = nfields;
        if ( i != null_pos )
            do
                fields[--last] = fields[--i];
            while ( i != null_pos );
        do
            fields[--last] = 0;
        while ( last != null_pos );
    }
    // locate first longer sequence of zero
    i = null_len = 0;
    null_pos = nfields;
    size_t first = 0;
    for ( ;; ) {
        while ( i < nfields && fields[i] != 0 )
            ++i;
        if ( i == nfields )
            break;
        first = i;
        while ( i < nfields && fields[i] == 0 )
            ++i;
        if ( ( i - first ) > null_len ) {
            null_pos = first;
            null_len = i - first;
        }
        if ( i == nfields )
            break;
    }
    if ( null_len == 1 ) {
        null_pos = nfields;
        null_len = 1;
    }
    // encode normalized IPv6
    std::stringstream str;
    if ( null_pos == 0 ) {
        str << std::hex << ':';
        i = null_len;
    } else {
        str << std::hex << fields[0];
        for ( i = 1; i < null_pos; ++i )
            str << ':' << fields[i];
        if ( i < nfields )
            str << ':';
        i += null_len;
        if ( i == 8 && null_len != 0 )
            str << ':';
    }
    for ( ; i < nfields; ++i )
        str << ':' << fields[i];
    if ( ipv4_b )
        str << ':' << std::string( ipv4_b, ipv4_e - ipv4_b );
    return str.str();
}
std::string normalize_ipv6( const std::string& s ) {
    return normalize_ipv6( s.c_str(), s.c_str() + s.length() );
}
bool is_good_url_port( const char* s, const char* e ) {
    if ( !is_uint( s, e, 65535 ) )
        return false;
    if ( s != e )
        return false;
    return true;
}
bool is_good_url_port( const char* s ) {
    return ( s == nullptr ) ? false : is_good_url_port( s, s + ::strlen( s ) );
}
bool is_good_url_port( const std::string& s ) {
    return is_good_url_port( s.c_str(), s.c_str() + s.length() );
}
std::string normalize_url_path( const std::string& s ) {
    if ( s.empty() )
        return s;
    std::string elem;
    std::vector< std::string > elems;
    std::stringstream si( s );
    while ( !std::getline( si, elem, '/' ).eof() ) {
        if ( elem == "" || elem == "." )
            continue;
        if ( elem == ".." ) {
            if ( !elems.empty() )
                elems.pop_back();
            continue;
        }
        elems.push_back( elem );
    }
    if ( elem == "." )
        elems.push_back( "" );
    else if ( elem == ".." ) {
        if ( !elems.empty() )
            elems.pop_back();
    } else
        elems.push_back( elem );
    std::stringstream so;
    if ( s[0] == '/' )
        so << '/';
    if ( !elems.empty() ) {
        auto it = elems.begin(), end = elems.end();
        so << *it;
        while ( ++it != end )
            so << '/' << *it;
    }
    return so.str();
}
std::string decode( const char* s, const char* e ) {
    std::string o;
    o.reserve( e - s );
    while ( s != e ) {
        char c = *s++, a, b;
        if ( c == '%' ) {
            if ( s == e || ( a = stat_get_hex_digit( *s++ ) ) < 0 || s == e ||
                 ( b = stat_get_hex_digit( *s++ ) ) < 0 )
                throw url::parse_error( "invalid percent encoding" );
            c = static_cast< char >( ( a << 4 ) | b );
        }
        o.push_back( c );
    }
    return o;
}
std::string decode_plus( const char* s, const char* e ) {
    std::string o;
    o.reserve( e - s );
    while ( s != e ) {
        char c = *s++, a, b;
        if ( c == '+' )
            c = ' ';
        else if ( c == '%' ) {
            if ( s == e || ( a = stat_get_hex_digit( *s++ ) ) < 0 || s == e ||
                 ( b = stat_get_hex_digit( *s++ ) ) < 0 )
                throw url::parse_error( "invalid percent encoding" );
            c = static_cast< char >( ( a << 4 ) | b );
        }
        o.push_back( c );
    }
    return o;
}

class encode {
public:
    encode( const std::string& s, std::uint8_t mask ) : m_s( s ), m_mask( mask ) {}

private:
    const std::string& m_s;
    std::uint8_t m_mask;
    friend std::ostream& operator<<( std::ostream& o, const encode& e ) {
        for ( const char c : e.m_s )
            if ( is_char( c, e.m_mask ) )
                o << c;
            else
                o << '%' << "0123456789ABCDEF"[c >> 4] << "0123456789ABCDEF"[c & 0xF];
        return o;
    }
};  /// class encode

std::string url::stat_do_encode( const std::string& s, std::uint8_t mask ) {
    std::stringstream ss;
    ss << encode( s, mask );
    return ss.str();
}

class encode_query_key {
public:
    encode_query_key( const std::string& s, std::uint8_t mask ) : m_s( s ), m_mask( mask ) {}

private:
    const std::string& m_s;
    std::uint8_t m_mask;
    friend std::ostream& operator<<( std::ostream& o, const encode_query_key& e ) {
        for ( const char c : e.m_s )
            if ( c == ' ' )
                o << '+';
            else if ( c == '+' )
                o << "%2B";
            else if ( c == '=' )
                o << "%3D";
            else if ( c == '&' )
                o << "%26";
            else if ( c == ';' )
                o << "%3B";
            else if ( is_char( c, e.m_mask ) )
                o << c;
            else
                o << '%' << "0123456789ABCDEF"[c >> 4] << "0123456789ABCDEF"[c & 0xF];
        return o;
    }
};  /// class encode_query_key

std::string url::stat_do_encode_query_key( const std::string& s, std::uint8_t mask ) {
    std::stringstream ss;
    ss << encode_query_key( s, mask );
    return ss.str();
}

class encode_query_val {
public:
    encode_query_val( const std::string& s, std::uint8_t mask ) : m_s( s ), m_mask( mask ) {}

private:
    const std::string& m_s;
    std::uint8_t m_mask;
    friend std::ostream& operator<<( std::ostream& o, const encode_query_val& e ) {
        for ( const char c : e.m_s )
            if ( c == ' ' )
                o << '+';
            else if ( c == '+' )
                o << "%2B";
            else if ( c == '&' )
                o << "%26";
            else if ( c == ';' )
                o << "%3B";
            else if ( is_char( c, e.m_mask ) )
                o << c;
            else
                o << '%' << "0123456789ABCDEF"[c >> 4] << "0123456789ABCDEF"[c & 0xF];
        return o;
    }
};  /// class encode_query_val

std::string url::stat_do_encode_query_val( const std::string& s, std::uint8_t mask ) {
    std::stringstream ss;
    ss << encode_query_val( s, mask );
    return ss.str();
}

void url::assign( const url& an_url ) {
    wasParsed_ = an_url.wasParsed_;
    wasBuilt_ = an_url.wasBuilt_;
    if ( wasParsed_ ) {
        strScheme_ = an_url.strScheme_;
        strUserName_ = an_url.strUserName_;
        strHost_ = an_url.strHost_;
        nIPversion_ = an_url.nIPversion_;
        strPort_ = an_url.strPort_;
        strPath_ = an_url.strPath_;
        kvQuery_ = an_url.kvQuery_;
        strFragment_ = an_url.strFragment_;
    }
    if ( ( !wasParsed_ ) || wasBuilt_ )
        strUrl_ = an_url.strUrl_;
}
void url::assign( url&& an_url ) {
    wasParsed_ = an_url.wasParsed_;
    wasBuilt_ = an_url.wasBuilt_;
    if ( wasParsed_ ) {
        strScheme_ = std::move( an_url.strScheme_ );
        strUserName_ = std::move( an_url.strUserName_ );
        strHost_ = std::move( an_url.strHost_ );
        nIPversion_ = std::move( an_url.nIPversion_ );
        strPort_ = std::move( an_url.strPort_ );
        strPath_ = std::move( an_url.strPath_ );
        kvQuery_ = std::move( an_url.kvQuery_ );
        strFragment_ = std::move( an_url.strFragment_ );
    }
    if ( ( !wasParsed_ ) || wasBuilt_ )
        strUrl_ = std::move( an_url.strUrl_ );
}

url& url::scheme( const std::string& s ) {
    if ( !is_scheme( s ) )
        throw url::parse_error( "invalid scheme \"" + s + "\"" );
    lazy_parse();
    std::string o( s );
    to_lower( o );
    if ( o != strScheme_ ) {
        strScheme_ = o;
        wasBuilt_ = false;
        if ( ( strScheme_ == "http" && strPort_ == "80" ) ||
             ( strScheme_ == "https" && strPort_ == "443" ) )
            strPort_ = "";
    }
    return ( *this );
}
url& url::user_info( const std::string& s ) {
    if ( s.length() > 256 )
        throw url::parse_error( "user info is longer than 256 characters \"" + s + "\"" );
    lazy_parse();
    if ( strUserName_ != s ) {
        strUserName_ = s;
        wasBuilt_ = false;
    }
    return ( *this );
}
std::pair< std::string, std::string > url::user_name_and_password() const {  // returns strings pair
                                                                             // object, not
                                                                             // referemnce
    std::pair< std::string, std::string > n_p;
    std::list< std::string > lst = split2list( user_info(), ':', 1 );
    std::list< std::string >::const_iterator itWalk = lst.begin(), itEnd = lst.end();
    if ( itWalk == itEnd )
        return n_p;
    n_p.first = ( *itWalk );
    ++itWalk;
    if ( itWalk != itEnd )
        n_p.second = ( *itWalk );
    return n_p;
}
url& url::user_name_and_password(
    const std::pair< std::string, std::string >& n_p, bool bRemoveIfEmpty ) {
    std::string s;
    if ( !n_p.first.empty() ) {
        s += n_p.first;
        s += ':';
        if ( !n_p.first.empty() )
            s += n_p.second;
        user_info( s );
    } else if ( bRemoveIfEmpty )
        user_info( s );
    return ( *this );
}
url& url::user_name_and_password(
    const std::string& strName, const std::string& strPassword, bool bRemoveIfEmpty ) {
    return user_name_and_password(
        std::pair< std::string, std::string >( strName, strPassword ), bRemoveIfEmpty );
}
std::string url::user_name() const {  // returns string object, not referemnce
    return user_name_and_password().first;
}
url& url::user_name( const std::string& s ) {
    std::pair< std::string, std::string > n_p = user_name_and_password();
    n_p.first = s;
    return user_name_and_password( n_p, true );
}
std::string url::user_password() const {  // returns string object, not referemnce
    return user_name_and_password().second;
}
url& url::user_password( const std::string& s ) {
    std::pair< std::string, std::string > n_p = user_name_and_password();
    n_p.second = s;
    return user_name_and_password( n_p, true );
}
url& url::host( const std::string& h, std::uint8_t ip_v ) {
    if ( h.length() > 253 )
        throw url::parse_error( "host is longer than 253 characters \"" + h + "\"" );
    lazy_parse();
    std::string o;
    if ( h.empty() )
        ip_v = -1;
    else if ( is_ipv4( h ) ) {
        if ( !is_valid_ipv4( h ) )
            throw url::parse_error( "invalid IPv4 address \"" + h + "\"" );
        ip_v = 4;
        o = h;
    } else if ( ip_v != 0 && ip_v != 4 && ip_v != 6 ) {
        if ( !is_ipv6( h ) ) {
            throw url::parse_error( "invalid IPvFuture address \"" + h + "\"" );
        }
        o = h;
    } else if ( is_ipv6( h ) ) {
        if ( !is_valid_ipv6( h ) )
            throw url::parse_error( "invalid IPv6 address \"" + h + "\"" );
        ip_v = 6;
        o = normalize_ipv6( h );
    } else if ( stat_is_reg_name( h ) ) {
        ip_v = 0;
        o = stat_normalize_reg_name( h );
    } else
        throw url::parse_error( "invalid host \"" + h + "\"" );
    if ( strHost_ != o || nIPversion_ != ip_v ) {
        strHost_ = o;
        nIPversion_ = ip_v;
        wasBuilt_ = false;
    }
    return ( *this );
}
url& url::port( const std::string& p ) {
    if ( !is_good_url_port( p ) )
        throw url::parse_error( "invalid port \"" + p + "\"" );
    lazy_parse();
    std::string o( p );
    if ( ( strScheme_ == "http" && o == "80" ) || ( strScheme_ == "https" && o == "443" ) )
        o = "";
    if ( strPort_ != o ) {
        strPort_ = o;
        wasBuilt_ = false;
    }
    return ( *this );
}
url& url::port( std::uint16_t num ) {
    std::stringstream ss;
    ss << num;
    return port( ss.str() );
}
url& url::path( const std::string& p ) {
    if ( p.length() > 8000 )
        throw url::parse_error( "path is longer than 8000 characters \"" + p + "\"" );
    lazy_parse();
    std::string o( normalize_url_path( p ) );
    if ( strPath_ != o ) {
        strPath_ = o;
        wasBuilt_ = false;
    }
    return ( *this );
}
url& url::fragment( const std::string& f ) {
    if ( f.length() > 256 )
        throw url::parse_error( "fragment is longer than 256 characters \"" + f + "\"" );
    lazy_parse();
    if ( strFragment_ != f ) {
        strFragment_ = f;
        wasBuilt_ = false;
    }
    return ( *this );
}

url& url::clear() {
    strUrl_.clear();
    strScheme_.clear();
    strUserName_.clear();
    strHost_.clear();
    strPort_.clear();
    strPath_.clear();
    kvQuery_.clear();
    strFragment_.clear();
    nIPversion_ = -1;
    wasBuilt_ = true;
    wasParsed_ = true;
    return ( *this );
}

void url::parse_url() const {
    if ( strUrl_.empty() ) {
        const_cast< url* >( this )->clear();
        wasParsed_ = wasBuilt_ = true;
        return;
    }
    if ( strUrl_.length() > 8000 )
        throw url::parse_error( "URI is longer than 8000 characters" );
    const char *s = strUrl_.c_str(), *e = s + strUrl_.length();
    std::int8_t ip_v = -1;
    const char *scheme_b, *scheme_e, *user_b, *user_e, *host_b, *host_e, *port_b, *port_e, *path_b,
        *path_e, *query_b, *query_e, *fragment_b, *fragment_e;
    scheme_b = scheme_e = user_b = user_e = host_b = host_e = port_b = port_e = path_b = path_e =
        query_b = query_e = fragment_b = fragment_e = nullptr;
    const char *b = s, *p = find_first_of( b, e, ":/?#" );
    if ( p == e ) {
        if ( !is_chars( b, p, 0x2F ) )
            throw url::parse_error( "Path '" + std::string( b, p ) + "' in '" +
                                    std::string( s, e - s ) + "' is invalid" );
        path_b = b;
        path_e = e;
    } else {
        // get schema if any
        if ( *p == ':' ) {
            if ( !is_scheme( b, p ) )
                throw url::parse_error( "Scheme in '" + std::string( s, e - s ) + "' is invalid" );
            scheme_b = b;
            scheme_e = p;
            p = find_first_of( b = p + 1, e, "/?#" );
        }
        // get authority if any
        if ( p != e && *p == '/' && ( e - b ) > 1 && b[0] == '/' && b[1] == '/' ) {
            const char* ea = find_first_of( b += 2, e, "/?#" );  // locate end of authority
            p = find_char( b, ea, '@' );
            // get user info if any
            if ( p != ea ) {
                if ( !is_chars( b, p, 0x05 ) )
                    throw url::parse_error(
                        "User info in '" + std::string( s, e - s ) + "' is invalid" );
                user_b = b;
                user_e = p;
                b = p + 1;
            }
            // Get IP literal if any
            if ( *b == '[' ) {
                // locate end of IP literal
                p = find_char( ++b, ea, ']' );
                if ( *p != ']' )
                    throw url::parse_error( "Missing ] in '" + std::string( s, e - s ) + "'" );
                // decode IPvFuture protocol version
                if ( *b == 'v' ) {
                    if ( is_heximal_digit( *++b ) ) {
                        ip_v = stat_get_hex_digit( *b );
                        if ( is_heximal_digit( *++b ) ) {
                            ip_v =
                                static_cast< int8_t >( ( ip_v << 8 ) | stat_get_hex_digit( *b ) );
                        }
                    }
                    if ( ip_v == -1 || *b++ != '.' || ( !is_chars( b, p, 0x05 ) ) )
                        throw url::parse_error(
                            "Host address in '" + std::string( s, e - s ) + "' is invalid" );
                } else if ( is_ipv6( b, p ) ) {
                    ip_v = 6;
                } else
                    throw url::parse_error(
                        "Host address in '" + std::string( s, e - s ) + "' is invalid" );
                host_b = b;
                host_e = p;
                b = p + 1;
            } else {
                p = find_char( b, ea, ':' );
                if ( is_ipv4( b, p ) )
                    ip_v = 4;
                else if ( stat_is_reg_name( b, p ) )
                    ip_v = 0;
                else
                    throw url::parse_error(
                        "Host address in '" + std::string( s, e - s ) + "' is invalid" );
                host_b = b;
                host_e = p;
                b = p;
            }
            // get port if any
            if ( b != ea && *b == ':' ) {
                if ( !is_good_url_port( ++b, ea ) )
                    throw url::parse_error( "Port '" + std::string( b, ea - b ) + "' in '" +
                                            std::string( s, e - s ) + "' is invalid" );
                port_b = b;
                port_e = ea;
            }
            b = ea;
        }
        p = find_first_of( b, e, "?#" );
        if ( !is_chars( b, p, 0x2F ) )
            throw url::parse_error( "Path '" + std::string( b, p ) + "' in '" +
                                    std::string( s, e - s ) + "' is invalid" );
        path_b = b;
        path_e = p;
        if ( p != e && *p == '?' ) {
            p = find_char( b = p + 1, e, '#' );
            query_b = b;
            query_e = p;
        }
        if ( p != e && *p == '#' ) {
            if ( !is_chars( p + 1, e, 0x3F ) )
                throw url::parse_error( "Fragment '" + std::string( p + 1, e ) + "' in '" +
                                        std::string( s, e - s ) + "' is invalid" );
            fragment_b = p + 1;
            fragment_e = e;
        }
    }
    std::string _scheme, _user, _host, _port, _path, _query, _fragment;
    Query query_v;
    if ( scheme_b )
        _scheme = normalize_scheme( scheme_b, scheme_e );
    if ( user_b )
        _user = decode( user_b, user_e );
    if ( host_b ) {
        _host = decode( host_b, host_e );
        if ( ip_v == 0 )
            _host = stat_normalize_reg_name( _host );
        else if ( ip_v == 6 )
            _host = normalize_ipv6( _host );
    }
    if ( port_b )
        _port = std::string( port_b, port_e - port_b );
    if ( path_b )
        _path = normalize_url_path( decode( path_b, path_e ) );
    if ( query_b ) {
        _query = std::string( query_b, query_e );
        p = b = query_b;
        while ( p != query_e ) {
            p = find_first_of( b, query_e, "=;&" );
            if ( !is_chars( b, p, 0x3F ) )
                throw url::parse_error( "Query key '" + std::string( b, p ) + "' in '" +
                                        std::string( s, e - s ) + "' is invalid" );
            std::string key( decode_plus( b, p ) ), val;
            if ( p != query_e ) {
                if ( *p == '=' ) {
                    p = find_first_of( b = p + 1, query_e, ";&" );
                    if ( !is_chars( b, p, 0x3F ) )
                        throw url::parse_error( "Query value '" + std::string( b, p ) + "' in '" +
                                                std::string( s, e - s ) + "' is invalid" );
                    val = decode_plus( b, p );
                }
                b = p + 1;
            }
            query_v.emplace_back( key, val );
        }
    }
    if ( fragment_b )
        _fragment = decode( fragment_b, fragment_e );
    strScheme_ = _scheme;
    strUserName_ = _user;
    strHost_ = _host;
    nIPversion_ = ip_v;
    strPort_ = _port;
    strPath_ = _path;
    kvQuery_ = query_v;
    strFragment_ = _fragment;
    wasParsed_ = true;
    wasBuilt_ = false;
}

void url::compose_url() const {
    lazy_parse();
    std::stringstream ss_url;
    if ( !strScheme_.empty() )
        ss_url << strScheme_ << ":";
    if ( !strHost_.empty() ) {
        ss_url << "//";
        if ( !strUserName_.empty() )
            ss_url << encode( strUserName_, 0x05 ) << '@';
        if ( nIPversion_ == 0 || nIPversion_ == 4 )
            ss_url << strHost_;
        else if ( nIPversion_ == 6 )
            ss_url << "[" << strHost_ << "]";
        else
            ss_url << "[v" << std::hex << ( int ) nIPversion_ << std::dec << '.' << strHost_ << "]";
        if ( !strPort_.empty() )
            if ( !( ( strScheme_ == "http" && strPort_ == "80" ) ||
                     ( strScheme_ == "https" && strPort_ == "443" ) ) )
                ss_url << ":" << strPort_;
    } else {
        if ( !strUserName_.empty() )
            throw url::compose_error( "user info defined, but host is empty" );
        if ( !strPort_.empty() )
            throw url::compose_error( "port defined, but host is empty" );
        if ( !strPath_.empty() ) {
            const char *b = strPath_.data(), *e = b + strPath_.length(),
                       *p = find_first_of( b, e, ":/" );
            if ( p != e && *p == ':' )
                throw url::compose_error(
                    "the first segment of the relative path can't contain ':'" );
        }
    }
    if ( !strPath_.empty() ) {
        if ( strPath_[0] != '/' && ( !strHost_.empty() ) )
            throw url::compose_error( "path must start with '/' when host is not empty" );
        ss_url << encode( strPath_, 0x0F );
    }
    if ( !kvQuery_.empty() ) {
        ss_url << "?";
        auto it = kvQuery_.begin(), end = kvQuery_.end();
        if ( it->key().empty() )
            throw url::compose_error( "first query entry has no key" );
        ss_url << encode_query_key( it->key(), 0x1F );
        if ( !it->val().empty() )
            ss_url << "=" << encode_query_val( it->val(), 0x1F );
        while ( ++it != end ) {
            if ( it->key().empty() )
                throw url::compose_error( "a query entry has no key" );
            ss_url << "&" << encode_query_key( it->key(), 0x1F );
            if ( !it->val().empty() )
                ss_url << "=" << encode_query_val( it->val(), 0x1F );
        }
    }
    if ( !strFragment_.empty() )
        ss_url << "#" << encode( strFragment_, 0x1F );
    wasBuilt_ = false;
    strUrl_ = ss_url.str();
}

std::ostream& url::output( std::ostream& o ) const {
    lazy_parse();
    if ( !wasBuilt_ )
        compose_url();
    o << "Url:{url(" << strUrl_ << ")";
    if ( !strScheme_.empty() )
        o << " scheme(" << strScheme_ << ")";
    if ( !strUserName_.empty() )
        o << " user_info(" << strUserName_ << ")";
    if ( nIPversion_ != -1 )
        o << " host(" << strHost_ << ") IPv(" << ( int ) nIPversion_ << ")";
    if ( !strPort_.empty() )
        o << " port(" << strPort_ << ")";
    if ( !strPath_.empty() )
        o << " path(" << strPath_ << ")";
    if ( !kvQuery_.empty() ) {
        std::stringstream ss;
        ss << " query(";
        for ( const auto& q : kvQuery_ )
            ss << q;
        std::string s( ss.str() );
        o << s.substr( 0, s.length() - 1 ) << ")";
    }
    if ( !strFragment_.empty() )
        o << "fragment(" << strFragment_ << ") ";
    o << "}";
    return o;
}

bool url::is_local_private_network_address() const {
    std::int8_t ip_v = ip_version();
    switch ( ip_v ) {
    case 4:
        return is_local_private_network_address_ipv4( host() );
    case 6:
        return is_local_private_network_address_ipv6( host() );
    default:
        return false;
    }  // switch( ip_v )
}

bool url::update_user_name_and_password( const char* strOptionalUser,
    const char* strOptionalPassword, bool bRemoveIfEmpty ) {  // returns true if was updated
    if ( ( !bRemoveIfEmpty ) && ( strOptionalUser == nullptr || strOptionalUser[0] == '\0' ) )
        return false;
    std::pair< std::string, std::string > n_p1 = user_name_and_password();
    std::pair< std::string, std::string > n_p2 = n_p1;
    if ( strOptionalUser != nullptr && strOptionalUser[0] != '\0' ) {
        n_p2.first = strOptionalUser;
        if ( strOptionalPassword != nullptr && strOptionalPassword[0] != '\0' )
            n_p2.second = strOptionalPassword;
        if ( n_p1 == n_p2 )
            return false;
    }
    user_name_and_password( n_p2, bRemoveIfEmpty );
    return true;
}
bool url::update_user_name_and_password( const std::string& strOptionalUser,
    const std::string& strOptionalPassword, bool bRemoveIfEmpty ) {  // returns true if was updated
    if ( ( !bRemoveIfEmpty ) && strOptionalUser.empty() )
        return false;
    return update_user_name_and_password( strOptionalUser.c_str(),
        ( !strOptionalPassword.empty() ) ? strOptionalPassword.c_str() : nullptr, bRemoveIfEmpty );
}
bool url::strip_user_name_and_password() {  // returns true if was updated
    return update_user_name_and_password( std::string(), std::string(), true );
}

bool update_url_user_name_and_password( url& an_url, const char* strOptionalUser,
    const char* strOptionalPassword, bool bRemoveIfEmpty ) {
    return an_url.update_user_name_and_password(
        strOptionalUser, strOptionalPassword, bRemoveIfEmpty );
}
bool update_url_user_name_and_password( url& an_url, const std::string& strOptionalUser,
    const std::string& strOptionalPassword, bool bRemoveIfEmpty ) {
    try {
        if ( ( !bRemoveIfEmpty ) && strOptionalUser.empty() )
            return false;
        return update_url_user_name_and_password( an_url, strOptionalUser.c_str(),
            ( !strOptionalPassword.empty() ) ? strOptionalPassword.c_str() : nullptr,
            bRemoveIfEmpty );
    } catch ( ... ) {
        return false;
    }
}
bool update_url_user_name_and_password( std::string& str_url, const char* strOptionalUser,
    const char* strOptionalPassword, bool bRemoveIfEmpty ) {
    try {
        url an_url( str_url );
        if ( !an_url.update_user_name_and_password(
                 strOptionalUser, strOptionalPassword, bRemoveIfEmpty ) )
            return false;
        std::string strResult = an_url.str();
        if ( strResult.empty() )
            return false;  // ???
        str_url = strResult;
        return true;
    } catch ( ... ) {
        return false;
    }
}
bool update_url_user_name_and_password( std::string& str_url, const std::string& strOptionalUser,
    const std::string& strOptionalPassword, bool bRemoveIfEmpty ) {
    if ( ( !bRemoveIfEmpty ) && strOptionalUser.empty() )
        return false;
    return update_url_user_name_and_password( str_url, strOptionalUser.c_str(),
        ( !strOptionalPassword.empty() ) ? strOptionalPassword.c_str() : nullptr, bRemoveIfEmpty );
}
bool strip_url_user_name_and_password( url& an_url ) {
    return update_url_user_name_and_password( an_url, std::string( "" ), std::string( "" ), true );
}
std::string update_url_user_name_and_password_copy( const std::string& str_url,
    const char* strOptionalUser, const char* strOptionalPassword, bool bRemoveIfEmpty ) {
    std::string s = str_url;
    return update_url_user_name_and_password(
               s, strOptionalUser, strOptionalPassword, bRemoveIfEmpty ) ?
               s :
               str_url;
}
std::string update_url_user_name_and_password_copy( const std::string& str_url,
    const std::string& strOptionalUser, const std::string& strOptionalPassword,
    bool bRemoveIfEmpty ) {
    std::string s = str_url;
    return update_url_user_name_and_password(
               s, strOptionalUser, strOptionalPassword, bRemoveIfEmpty ) ?
               s :
               str_url;
}
bool strip_url_user_name_and_password( std::string& str_url ) {
    if ( str_url.empty() )
        return false;
    return update_url_user_name_and_password( str_url, std::string( "" ), std::string( "" ), true );
}
std::string strip_url_user_name_and_password_copy( const std::string& str_url ) {
    std::string s = str_url;
    return strip_url_user_name_and_password( s ) ? s : str_url;
}

// the following group of methods are accepting IP address as parameter, not URL
extern bool is_local_private_network_address_ipv4( const std::string& s ) {
    if ( s.empty() )
        return false;
    if ( !( is_ipv4( s ) && is_valid_ipv4( s ) ) )
        return false;
    std::vector< std::string > vec = split2vector( s, '.' );
    if ( vec.size() != 4 )
        return false;  // ???
    // note: see "Reserved address blocks" table here https://en.wikipedia.org/wiki/IPv4
    if ( !is_uint( vec[0], 255 ) )
        return false;  // ???
    char* ep = nullptr;
    unsigned long n = ::strtoul( vec[0].c_str(), &ep, 10 );
    switch ( n ) {
    case 10:
    case 127:
    case 172:
    case 192:
        return true;
    }  // switch( n )
    return false;
}
extern bool is_local_private_network_address_ipv4( const char* s, const char* e ) {
    if ( s == nullptr )
        return false;
    std::string tmp( s, size_t( e - s ) );
    return is_local_private_network_address_ipv4( tmp );
}
extern bool is_local_private_network_address_ipv6( const std::string& s ) {
    if ( s.empty() )
        return false;
    if ( !( is_ipv6( s ) && is_valid_ipv6( s ) ) )
        return false;
    if ( s == "::1" )
        return true;
    if ( s[0] == ':' )
        return false;
    // note: read about adress scopes
    // https://en.wikipedia.org/wiki/IPv6_address
    // http://www.gestioip.net/docu/ipv6_address_examples.html
    if ( s[0] == 'f' || s[0] == 'F' )
        return true;  // note: this method should be additionally re-checked
    //
    if ( strncasecmp( s.c_str(), "fc00::", 6 ) != 0 )
        return true;
    return false;
}
extern bool is_local_private_network_address_ipv6( const char* s, const char* e ) {
    if ( s == nullptr )
        return false;
    std::string tmp( s, size_t( e - s ) );
    return is_local_private_network_address_ipv6( tmp );
}
bool is_local_private_network_address( const char* s, const char* e ) {
    if ( is_local_private_network_address_ipv4( s, e ) )
        return true;
    if ( is_local_private_network_address_ipv6( s, e ) )
        return true;
    return false;
}
bool is_local_private_network_address( const std::string& s ) {
    if ( is_local_private_network_address_ipv4( s ) )
        return true;
    if ( is_local_private_network_address_ipv6( s ) )
        return true;
    return false;
}

// the following group of methods are accepting URL as parameter, not IP address
bool is_local_private_network_url( const url& an_url ) {
    return an_url.is_local_private_network_address();
}
bool is_local_private_network_url( const std::string& str_url ) {
    try {
        url an_url( str_url );
        return is_local_private_network_url( an_url );
    } catch ( ... ) {
        return false;
    }
}
extern bool is_local_private_network_url( const char* s, const char* e ) {
    if ( s == nullptr )
        return false;
    std::string tmp( s, size_t( e - s ) );
    return is_local_private_network_url( tmp );
}
};  // namespace skutils
