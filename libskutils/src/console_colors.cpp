#include <inttypes.h>
#include <memory.h>
#include <skutils/console_colors.h>
#include <skutils/utils.h>
#include <stdio.h>
#include <strings.h>
#include <algorithm>
#include <exception>
#include <map>
#include <stdexcept>

#define CC_CONSOLE_COLOR_DEFAULT "\033[0m"
#define CC_FORECOLOR( C ) "\033[" #C "m"
#define CC_BACKCOLOR( C ) "\033[" #C "m"
#define CC_ATTR( A ) "\033[" #A "m"

namespace cc {
e_reset_mode_t g_eResetMode = e_reset_mode_t::__ERM_FULL;
e_reset_mode_t str_2_reset_mode( const std::string& s ) {
    std::string x = skutils::tools::to_lower( skutils::tools::trim_copy( s ) );
    if ( x.empty() || x[0] == 'f' )
        return e_reset_mode_t::__ERM_FULL;
    if ( x[0] == 'c' )
        return e_reset_mode_t::__ERM_COLORS;
    if ( x[0] == 'z' )
        return e_reset_mode_t::__ERM_ZERO;
    if ( x[0] == 'w' )
        return e_reset_mode_t::__ERM_WIDE;
    if ( x[0] == 'e' )
        return e_reset_mode_t::__ERM_EMULATION;
    return e_reset_mode_t::__ERM_FULL;
}
e_reset_mode_t str_2_reset_mode( const char* s ) {
    std::string x( ( s != nullptr ) ? s : "" );
    return str_2_reset_mode( x );
}
std::string reset_mode_2_str( e_reset_mode_t eResetMode ) {
    switch ( eResetMode ) {
    case e_reset_mode_t::__ERM_COLORS:
        return "colors";
    case e_reset_mode_t::__ERM_ZERO:
        return "zero";
    case e_reset_mode_t::__ERM_WIDE:
        return "wide";
    case e_reset_mode_t::__ERM_EMULATION:
        return "emulation";
    case e_reset_mode_t::__ERM_FULL:
    default:
        return "full";
    }  // switch( eResetMode )
}

size_t g_nJsonStringValueOutputSizeLimit = 0;
namespace control {
namespace attribute {
const char _console_[] = CC_CONSOLE_COLOR_DEFAULT;
const char _underline_[] = CC_ATTR( 4 );
const char _bold_[] = CC_ATTR( 1 );
};  // namespace attribute
namespace foreground {
const char _black_[] = CC_FORECOLOR( 30 );
const char _blue_[] = CC_FORECOLOR( 34 );
const char _red_[] = CC_FORECOLOR( 31 );
const char _magenta_[] = CC_FORECOLOR( 35 );
const char _green_[] = CC_FORECOLOR( 92 );
const char _cyan_[] = CC_FORECOLOR( 36 );
const char _yellow_[] = CC_FORECOLOR( 33 );
const char _white_[] = CC_FORECOLOR( 37 );
const char _console_[] = CC_FORECOLOR( 39 );
const char _light_black_[] = CC_FORECOLOR( 90 );
const char _light_blue_[] = CC_FORECOLOR( 94 );
const char _light_red_[] = CC_FORECOLOR( 91 );
const char _light_magenta_[] = CC_FORECOLOR( 95 );
const char _light_green_[] = CC_FORECOLOR( 92 );
const char _light_cyan_[] = CC_FORECOLOR( 96 );
const char _light_yellow_[] = CC_FORECOLOR( 93 );
const char _light_white_[] = CC_FORECOLOR( 97 );
};  // namespace foreground
namespace background {
const char _black_[] = CC_BACKCOLOR( 40 );
const char _blue_[] = CC_BACKCOLOR( 44 );
const char _red_[] = CC_BACKCOLOR( 41 );
const char _magenta_[] = CC_BACKCOLOR( 45 );
const char _green_[] = CC_BACKCOLOR( 42 );
const char _cyan_[] = CC_BACKCOLOR( 46 );
const char _yellow_[] = CC_BACKCOLOR( 43 );
const char _white_[] = CC_BACKCOLOR( 47 );
const char _console_[] = CC_BACKCOLOR( 49 );
const char _light_black_[] = CC_BACKCOLOR( 100 );
const char _light_blue_[] = CC_BACKCOLOR( 104 );
const char _light_red_[] = CC_BACKCOLOR( 101 );
const char _light_magenta_[] = CC_BACKCOLOR( 105 );
const char _light_green_[] = CC_BACKCOLOR( 102 );
const char _light_cyan_[] = CC_BACKCOLOR( 106 );
const char _light_yellow_[] = CC_BACKCOLOR( 103 );
const char _light_white_[] = CC_BACKCOLOR( 107 );
};  // namespace background
};  // namespace control

bool volatile _on_ = false;
volatile int _default_json_indent_ = -1;

std::string tune( attribute a, color f, color b ) {  // l_sergiy: does not work well, especially
                                                     // with resetting colors
    if ( !_on_ )
        return std::string( "" );
    char s[32] = "";
    sprintf( s, "\033[%d;%d;%dm", int( a ), int( f ) + 30, int( b ) + 40 );
    return s;
}

// http://www.termsys.demon.co.uk/vtansi.htm
const char* reset() {
    if ( !_on_ )
        return "";
    static const std::string g_strResetModeColors = ( []() -> std::string {
        std::stringstream ss;
        ss << control::foreground::_white_ << control::background::_console_;
        return ss.str();
    }() );
    static const std::string g_strResetModeZero = ( []() -> std::string {
        std::stringstream ss;
        ss << control::foreground::_white_ << CC_BACKCOLOR( 0 );
        return ss.str();
    }() );
    static const std::string g_strResetModeEmulation =
        tune( cc::attribute::_reset_, cc::color::_default_, cc::color::_default_ );
    static const char g_strResetModeWide[] = "\033[00m";
    static const char g_strResetModeFull[] = "\033[0m";
    switch ( g_eResetMode ) {
    case e_reset_mode_t::__ERM_COLORS:
        return g_strResetModeColors.c_str();
    case e_reset_mode_t::__ERM_ZERO:
        return g_strResetModeZero.c_str();
    case e_reset_mode_t::__ERM_EMULATION:
        return g_strResetModeEmulation.c_str();
    case e_reset_mode_t::__ERM_WIDE:
        return g_strResetModeWide;
    case e_reset_mode_t::__ERM_FULL:
        return g_strResetModeFull;
    }  /// switch( g_eResetMode )
    return g_strResetModeFull;
}
const char* normal() {
    if ( !_on_ )
        return "";
    return reset();
}
const char* trace() {
    if ( !_on_ )
        return "";
    return control::foreground::_white_;
}
const char* debug() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_black_;
}
const char* info() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_blue_;
}
const char* note() {
    if ( !_on_ )
        return "";
    return control::foreground::_blue_;
}
const char* notice() {
    if ( !_on_ )
        return "";
    return control::foreground::_magenta_;
}
const char* attention() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_magenta_;
}
const char* bright() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_white_;
}
const char* sunny() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_yellow_;
}
const char* success() {
    if ( !_on_ )
        return "";
    return control::foreground::_green_;
}
const char* fail() {
    if ( !_on_ )
        return "";
    return control::foreground::_red_;
}
const char* warn() {
    if ( !_on_ )
        return "";
    return control::foreground::_yellow_;
}
const char* error() {
    if ( !_on_ )
        return "";
    return control::foreground::_red_;
}
const char* found() {
    if ( !_on_ )
        return "";
    static const std::string s =
        std::string( "" ) + control::foreground::_blue_ + control::background::_light_yellow_;
    return s.c_str();
}
const char* deep_debug() {
    if ( !_on_ )
        return "";
    static const std::string s =
        std::string( "" ) + control::foreground::_black_ + control::background::_light_black_;
    return s.c_str();
}
const char* deep_info() {
    if ( !_on_ )
        return "";
    static const std::string s =
        std::string( "" ) + control::foreground::_black_ + control::background::_light_blue_;
    return s.c_str();
}
const char* deep_note() {
    if ( !_on_ )
        return "";
    static const std::string s =
        std::string( "" ) + control::foreground::_black_ + control::background::_blue_;
    return s.c_str();
}
const char* deep_notice() {
    if ( !_on_ )
        return "";
    static const std::string s =
        std::string( "" ) + control::foreground::_black_ + control::background::_magenta_;
    return s.c_str();
}
const char* deep_warn() {
    if ( !_on_ )
        return "";
    static const std::string s =
        std::string( "" ) + control::foreground::_black_ + control::background::_light_yellow_;
    return s.c_str();
}
const char* deep_error() {
    if ( !_on_ )
        return "";
    static const std::string s =
        std::string( "" ) + control::foreground::_black_ + control::background::_light_red_;
    return s.c_str();
}
const char* fatal() {
    if ( !_on_ )
        return "";
    static const std::string s =
        std::string( "" ) + control::foreground::_light_yellow_ + control::background::_red_;
    return s.c_str();
}
const char* ws_rx_inv() {
    if ( !_on_ )
        return "";
    static const std::string s =
        std::string( "" ) + control::foreground::_white_ + control::background::_cyan_;
    return s.c_str();
}
const char* ws_tx_inv() {
    if ( !_on_ )
        return "";
    static const std::string s =
        std::string( "" ) + control::foreground::_white_ + control::background::_blue_;
    return s.c_str();
}
const char* ws_rx() {
    if ( !_on_ )
        return "";
    static const std::string s = std::string( "" ) + control::foreground::_light_cyan_;
    return s.c_str();
}
const char* ws_tx() {
    if ( !_on_ )
        return "";
    static const std::string s = std::string( "" ) + control::foreground::_light_blue_;
    return s.c_str();
}

const char* path_slash() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_white_;
}  // '/'
const char* path_part() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_cyan_;
}

const char* url_prefix_punct() {
    if ( !_on_ )
        return "";
    return control::foreground::_yellow_;
}  // ':', etc.
const char* url_prefix_slash() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_white_;
}  // '/'
const char* url_prefix_scheme() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_red_;
}
const char* url_prefix_user_name() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_cyan_;
}
const char* url_prefix_at() {
    if ( !_on_ )
        return "";
    return control::foreground::_yellow_;
}  // '@'
const char* url_prefix_user_pass() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_green_;
}
const char* url_prefix_host() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_blue_;
}
const char* url_prefix_number() {
    if ( !_on_ )
        return "";
    return control::foreground::_blue_;
}  // explict IP version as number
const char* url_prefix_port() {
    if ( !_on_ )
        return "";
    return control::foreground::_blue_;
}
const char* url_prefix_path() {
    if ( !_on_ )
        return "";
    return control::foreground::_magenta_;
}
const char* url_prefix_qq() {
    if ( !_on_ )
        return "";
    return control::foreground::_cyan_;
}  // query '?'
const char* url_prefix_qk() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_yellow_;
}  // query key
const char* url_prefix_qe() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_magenta_;
}  // query '='
const char* url_prefix_qv() {
    if ( !_on_ )
        return "";
    return control::foreground::_yellow_;
}  // query value
const char* url_prefix_qa() {
    if ( !_on_ )
        return "";
    return control::foreground::_cyan_;
}  // query '&'
const char* url_prefix_hash() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_white_;
}  // '#'
const char* url_prefix_fragment() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_red_;
}

const char* json_prefix( nlohmann::json::value_t vt ) {
    if ( !_on_ )
        return "";
    typedef std::map< nlohmann::json::value_t, const char* > map_json_prefixes_by_type_t;
    static const map_json_prefixes_by_type_t g_map{
        {nlohmann::json::value_t::null, control::foreground::_red_},  // null value
        {nlohmann::json::value_t::object,
            control::foreground::_light_blue_},  // object (unordered set of name/value pairs)
        {nlohmann::json::value_t::array, control::foreground::_green_},    // array (ordered
                                                                           // collection of values)
        {nlohmann::json::value_t::string, control::foreground::_yellow_},  // string value
        {nlohmann::json::value_t::boolean, control::foreground::_magenta_},      // boolean value
        {nlohmann::json::value_t::number_integer, control::foreground::_blue_},  // number value
                                                                                 // (integer)
        {nlohmann::json::value_t::number_float, control::foreground::_cyan_},    // number value
                                                                                 // (floating-point)
        {nlohmann::json::value_t::discarded,
            control::foreground::_red_},  // discarded by the the parser callback function
    };
    map_json_prefixes_by_type_t::const_iterator itFind = g_map.find( vt );
    const char* s = ( itFind != g_map.end() ) ? itFind->second : reset();
    return s;
}
const char* json_prefix_brace_square() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_white_;
}
const char* json_prefix_brace_curved() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_white_;
}
const char* json_prefix_double_colon() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_white_;
}
const char* json_prefix_comma() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_black_;
}
const char* json_prefix_member_name() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_yellow_;
}
std::size_t json_extra_space( const std::string& s ) noexcept {
    std::size_t result = 0;
    for ( const auto& c : s ) {
        switch ( c ) {
        case '"':
        case '\\':
        case '\b':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            result += 1;
            break;  // from c (1 byte) to \x (2 bytes)
        default:
            if ( c >= 0x00 and c <= 0x1f )
                result += 5;
            break;  // from c (1 byte) to \uxxxx (6 bytes)
        }           // switch( c )
    }               // for( const auto & c : s )
    return result;
}
std::string json_escape_string( const std::string& s ) noexcept {
    const auto space = json_extra_space( s );
    if ( space == 0 )
        return s;
    // create a result string of necessary size
    std::string result( s.size() + space, '\\' );
    std::size_t pos = 0;
    for ( const auto& c : s ) {
        switch ( c ) {
        case '"':
            result[pos + 1] = '"';
            pos += 2;
            break;  // quotation mark (0x22)
        case '\\':
            pos += 2;
            break;  // reverse solidus (0x5c), nothing to change
        case '\b':
            result[pos + 1] = 'b';
            pos += 2;
            break;  // backspace (0x08)
        case '\f':
            result[pos + 1] = 'f';
            pos += 2;
            break;  // formfeed (0x0c)
        case '\n':
            result[pos + 1] = 'n';
            pos += 2;
            break;  // newline (0x0a)
        case '\r':
            result[pos + 1] = 'r';
            pos += 2;
            break;  // carriage return (0x0d)
        case '\t':
            result[pos + 1] = 't';
            pos += 2;
            break;  // horizontal tab (0x09)
        default:
            if ( c >= 0x00 and c <= 0x1f ) {
                sprintf( &result[pos + 1], "u%04x", int( c ) );  // print character c as \uxxxx
                pos += 6;
                result[pos] = '\\';  // overwrite trailing null character
            } else
                result[pos++] = c;  // all other characters are added as-is
            break;
        }  // switch( c )
    }      // for( const auto & c : s )
    return result;
}
static void json_internal( const nlohmann::json& jo, std::ostream& os, const bool pretty_print,
    const unsigned int indent_step, const unsigned int current_indent = 0 ) {
    // variable to hold indentation for recursive calls
    unsigned int new_indent = current_indent;
    nlohmann::json::value_t vt = jo.type();
    std::string stc = json_prefix( vt );
    switch ( vt ) {
    case nlohmann::json::value_t::object: {
        if ( jo.empty() ) {
            os << json_prefix_brace_curved() << "{}" << reset();
            return;
        }
        os << json_prefix_brace_curved() << "{" << reset();
        if ( pretty_print ) {  // increase indentation
            new_indent += indent_step;
            os << "\n";
        }
        auto b = jo.cbegin(), e = jo.cend();
        for ( auto i = b; i != e; ++i ) {
            if ( i != b )
                os << json_prefix_comma() << ( pretty_print ? ",\n" : "," ) << reset();
            os << reset() << std::string( new_indent, ' ' ) << json_prefix_member_name() << "\""
               << json_escape_string( i.key().c_str() ) << "\"" << reset()
               << json_prefix_double_colon() << ":" << ( pretty_print ? " " : "" ) << reset();
            json_internal( i.value(), os, pretty_print, indent_step, new_indent );
        }
        if ( pretty_print ) {  // decrease indentation
            new_indent -= indent_step;
            os << reset() << "\n";
        }
        os << reset() << std::string( new_indent, ' ' ) << json_prefix_brace_curved() << "}"
           << reset();
        return;
    }
    case nlohmann::json::value_t::array: {
        if ( jo.empty() ) {
            os << json_prefix_brace_square() << "[]" << reset();
            return;
        }
        os << json_prefix_brace_square() << "[" << reset();
        if ( pretty_print ) {  // increase indentation
            new_indent += indent_step;
            os << reset() << "\n";
        }
        auto b = jo.cbegin(), e = jo.cend();
        for ( auto i = b; i != e; ++i ) {
            if ( i != b )
                os << json_prefix_comma() << ( pretty_print ? ",\n" : "," ) << reset();
            os << reset() << std::string( new_indent, ' ' );
            json_internal( *i, os, pretty_print, indent_step, new_indent );
        }
        if ( pretty_print ) {  // decrease indentation
            new_indent -= indent_step;
            os << reset() << "\n";
        }
        os << stc << std::string( new_indent, ' ' ) << json_prefix_brace_square() << "]" << reset();
        return;
    }
    case nlohmann::json::value_t::string: {
        std::string strValue = jo.get< std::string >();
        if ( g_nJsonStringValueOutputSizeLimit > 0 ) {
            if ( strValue.length() > g_nJsonStringValueOutputSizeLimit )
                strValue = strValue.substr( 0, g_nJsonStringValueOutputSizeLimit ) + "...";
        }
        strValue = json_escape_string( strValue );
        std::string s2;
        if ( u( s2, strValue, false, false ) )
            strValue = s2;  // yes, it's URL
        else if ( eml( s2, strValue, false ) )
            strValue = s2;  // yes, it's e-mail
        else if ( p( s2, strValue, false ) )
            strValue = s2;  // yes, it's path
        os << stc << std::string( "\"" ) << strValue << "\"" << reset();
        return;
    }
    case nlohmann::json::value_t::boolean: {
        // os << stc << ( jo.get<bool>() ? "true" : "false" ) << reset();
        os << flag( jo.get< bool >() );
        return;
    }
    case nlohmann::json::value_t::number_unsigned: {
        os << stc << jo.get< unsigned >() << reset();
        return;
    }
    case nlohmann::json::value_t::number_integer: {
        os << stc << jo.get< int >() << reset();
        return;
    }
    case nlohmann::json::value_t::number_float: {
        // 15 digits of precision allows round-trip IEEE 754
        // string->double->string; to be safe, we read this value from
        // std::numeric_limits<number_float_t>::digits10
        os << /*stc*/ control::foreground::_cyan_
           << std::setprecision( std::numeric_limits< nlohmann::json::number_float_t >::digits10 )
           << jo.get< nlohmann::json::number_float_t >() << reset();
        return;
    }
    case nlohmann::json::value_t::discarded:
        os << stc << "<discarded>" << reset();
        return;
    case nlohmann::json::value_t::null:
        os << stc << "null" << reset();
        return;
    default:
        break;
    }  // switch( vt )
}
std::string json( const nlohmann::json& jo, const int indent /*= -1*/ ) {
    std::stringstream ss;
    json_internal( jo, ss, ( indent >= 0 ) ? true : false,
        ( indent >= 0 ) ? static_cast< unsigned int >( indent ) : 0 );
    return ss.str();
}
std::string json( const std::string& s ) {
    nlohmann::json jo = nlohmann::json::parse( s );
    return json( jo );
}
std::string json( const char* s ) {
    if ( s == nullptr || s[0] == '\0' )
        return std::string( "" );
    return json( std::string( s ) );
}

std::string flag( bool b ) {  // true/false
    // return std::string("") + json_prefix( nlohmann::json::value_t::boolean ) + ( b ? "true" :
    // "false" ) + reset();
    return std::string( "" ) +
           ( _on_ ? ( b ? control::foreground::_light_green_ : control::foreground::_light_red_ ) :
                    "" ) +
           ( b ? "true" : "false" ) + reset();
}
std::string flag_ed( bool b ) {  // enabled/disabled
    return std::string( "" ) +
           ( _on_ ? ( b ? control::foreground::_light_green_ : control::foreground::_light_red_ ) :
                    "" ) +
           ( b ? "enabled" : "disabled" ) + reset();
}
std::string flag_yn( bool b ) {  // yes/no
    return std::string( "" ) +
           ( _on_ ? ( b ? control::foreground::_light_green_ : control::foreground::_light_red_ ) :
                    "" ) +
           ( b ? "yes" : "no" ) + reset();
}

std::string chr( char c ) {
    char s[128];
    ::sprintf( s, "%c", c );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::string ) + s + reset();
}
std::string chr( unsigned char c ) {
    char s[128];
    ::sprintf( s, "%c", c );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::string ) + s + reset();
}

std::string str( const std::string& s ) {
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::string ) + s + reset();
}
std::string str( const char* s ) {
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::string ) + s + reset();
}

std::string num10( int8_t n ) {
    char s[128];
    ::sprintf( s, "%d", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num10( uint8_t n ) {
    char s[128];
    ::sprintf( s, "%u", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num10( int16_t n ) {
    char s[128];
    ::sprintf( s, "%d", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num10( uint16_t n ) {
    char s[128];
    ::sprintf( s, "%u", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num10( int32_t n ) {
    char s[128];
    ::sprintf( s, "%d", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num10( uint32_t n ) {
    char s[128];
    ::sprintf( s, "%u", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num10( int64_t n ) {
    char s[128];
    ::sprintf( s, "%" PRId64, n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num10( uint64_t n ) {
    char s[128];
    ::sprintf( s, "%" PRIu64, n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string size10( size_t n ) {
    return num10( uint64_t( n ) );
}

std::string num16( int8_t n ) {
    char s[128];
    ::sprintf( s, "%x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num16( uint8_t n ) {
    char s[128];
    ::sprintf( s, "%x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num16( int16_t n ) {
    char s[128];
    ::sprintf( s, "%x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num16( uint16_t n ) {
    char s[128];
    ::sprintf( s, "%x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num16( int32_t n ) {
    char s[128];
    ::sprintf( s, "%x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num16( uint32_t n ) {
    char s[128];
    ::sprintf( s, "%x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num16( int64_t n ) {
    char s[128];
    ::sprintf( s, "%" PRIx64, n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num16( uint64_t n ) {
    char s[128];
    ::sprintf( s, "%" PRIx64, n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string size16( size_t n ) {
    return num16( uint64_t( n ) );
}

std::string num016( int8_t n ) {
    char s[128];
    ::sprintf( s, "%02x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num016( uint8_t n ) {
    char s[128];
    ::sprintf( s, "%02x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num016( int16_t n ) {
    char s[128];
    ::sprintf( s, "%04x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num016( uint16_t n ) {
    char s[128];
    ::sprintf( s, "%04x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num016( int32_t n ) {
    char s[128];
    ::sprintf( s, "%08x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num016( uint32_t n ) {
    char s[128];
    ::sprintf( s, "%08x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num016( int64_t n ) {
    char s[128];
    ::sprintf( s, "%016" PRIx64, n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num016( uint64_t n ) {
    char s[128];
    ::sprintf( s, "%016" PRIx64, n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string size016( size_t n ) {
    return num016( uint64_t( n ) );
}

std::string num0x( int8_t n ) {
    char s[128];
    ::sprintf( s, "0x%02x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num0x( uint8_t n ) {
    char s[128];
    ::sprintf( s, "0x%02x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num0x( int16_t n ) {
    char s[128];
    ::sprintf( s, "0x%04x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num0x( uint16_t n ) {
    char s[128];
    ::sprintf( s, "0x%04x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num0x( int32_t n ) {
    char s[128];
    ::sprintf( s, "0x%08x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num0x( uint32_t n ) {
    char s[128];
    ::sprintf( s, "0x%08x", n );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num0x( int64_t n ) {
    char s[128];
    ::sprintf( s, "0x%08x%08x", uint32_t( n >> 32 ), uint32_t( n & 0xFFFFFFFF ) );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string num0x( uint64_t n ) {
    char s[128];
    ::sprintf( s, "0x%08x%08x", uint32_t( n >> 32 ), uint32_t( n & 0xFFFFFFFF ) );
    return std::string( "" ) + json_prefix( nlohmann::json::value_t::number_integer ) + s + reset();
}
std::string size0x( size_t n ) {
    return num0x( uint64_t( n ) );
}

std::string num10eq0x( int8_t n ) {
    return num10( n ) + json_prefix_double_colon() + "=" + num0x( n ) + reset();
}
std::string num10eq0x( uint8_t n ) {
    return num10( n ) + json_prefix_double_colon() + "=" + num0x( n ) + reset();
}
std::string num10eq0x( int16_t n ) {
    return num10( n ) + json_prefix_double_colon() + "=" + num0x( n ) + reset();
}
std::string num10eq0x( uint16_t n ) {
    return num10( n ) + json_prefix_double_colon() + "=" + num0x( n ) + reset();
}
std::string num10eq0x( int32_t n ) {
    return num10( n ) + json_prefix_double_colon() + "=" + num0x( n ) + reset();
}
std::string num10eq0x( uint32_t n ) {
    return num10( n ) + json_prefix_double_colon() + "=" + num0x( n ) + reset();
}
std::string num10eq0x( int64_t n ) {
    return num10( n ) + json_prefix_double_colon() + "=" + num0x( n ) + reset();
}
std::string num10eq0x( uint64_t n ) {
    return num10( n ) + json_prefix_double_colon() + "=" + num0x( n ) + reset();
}
std::string size10eq0x( size_t n ) {
    return num10eq0x( uint64_t( n ) );
}

std::string numScientific( const float& n ) {
    char s[128];
    ::sprintf( s, "%e", n );
    return std::string( "" ) +
           /*json_prefix( nlohmann::json::value_t::number_float )*/ control::foreground::_cyan_ +
           s + reset();
}
std::string numScientific( const double& n ) {
    char s[128];
    ::sprintf( s, "%le", n );
    return std::string( "" ) +
           /*json_prefix( nlohmann::json::value_t::number_float )*/ control::foreground::_cyan_ +
           s + reset();
}
std::string numDefault( const float& n ) {
    char s[128];
    ::sprintf( s, "%f", n );
    return std::string( "" ) +
           /*json_prefix( nlohmann::json::value_t::number_float )*/ control::foreground::_cyan_ +
           s + reset();
}
std::string numDefault( const double& n ) {
    char s[128];
    ::sprintf( s, "%lf", n );
    return std::string( "" ) +
           /*json_prefix( nlohmann::json::value_t::number_float )*/ control::foreground::_cyan_ +
           s + reset();
}

std::string num( int8_t n ) {
    return num10eq0x( n );
}
std::string num( uint8_t n ) {
    return num10eq0x( n );
}
std::string num( int16_t n ) {
    return num10eq0x( n );
}
std::string num( uint16_t n ) {
    return num10eq0x( n );
}
std::string num( int32_t n ) {
    return num10eq0x( n );
}
std::string num( uint32_t n ) {
    return num10eq0x( n );
}
std::string num( int64_t n ) {
    return num10eq0x( n );
}
std::string num( uint64_t n ) {
    return num10eq0x( n );
}
std::string num( const float& n ) {
    return numDefault( n );
}
std::string num( const double& n ) {
    return numDefault( n );
}
std::string size( size_t n ) {
    return num( uint64_t( n ) );
}

const char* tsep() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_white_;
}
const char* day_count() {
    if ( !_on_ )
        return "";
    return control::foreground::_yellow_;
}
const char* dpart() {
    if ( !_on_ )
        return "";
    return control::foreground::_yellow_;
}
const char* tpart() {
    if ( !_on_ )
        return "";
    return control::foreground::_light_magenta_;
}
const char* tfrac() {
    if ( !_on_ )
        return "";
    return control::foreground::_magenta_;
}

static void stat_parse_skip_space( const char*& s ) {
    if ( s == nullptr || ( *s ) == '\0' )
        return;
    while (
        std::isspace( *s ) || ( *s ) == '\f' || ( *s ) == '\t' || ( *s ) == '\r' || ( *s ) == '\n' )
        ++s;
}
//	static bool stat_parse_expect_at_least_one_space( const char * & s ) {
//		if( s == nullptr || (*s) == '\0' )
//			return false;
//		bool bOK = ( (*s) == ' ' ) ? true : false;
//		if( bOK )
//			++ s;
//		stat_parse_skip_space( s );
//		return true;
//	}
static bool stat_parse_expect_one_character( const char*& s, char c ) {
    if ( s == nullptr || ( *s ) == '\0' )
        return false;
    stat_parse_skip_space( s );
    bool bOK = ( ( *s ) == c ) ? true : false;
    if ( bOK )
        ++s;
    stat_parse_skip_space( s );
    return true;
}
static bool stat_parse_expect_one_of_characters( const char*& s, const char* strChars ) {
    if ( s == nullptr || ( *s ) == '\0' || strChars == nullptr || ( *strChars ) == '\0' )
        return false;
    stat_parse_skip_space( s );
    bool bOK = false;
    for ( const char* p = strChars; ( *p ) != '\0'; ++p ) {
        bOK = ( ( *s ) == ( *p ) ) ? true : false;
        if ( !bOK )
            continue;
        ++s;
        break;
    }
    if ( !bOK )
        return false;
    stat_parse_skip_space( s );
    return true;
}
static std::string stat_parse_accumulate_digits( const char*& s, int nExpectedLength = -1 ) {
    std::string strResult;
    if ( s == nullptr || ( *s ) == '\0' )
        return strResult;
    stat_parse_skip_space( s );
    while ( std::isdigit( *s ) ) {
        strResult += ( *s );
        ++s;
    }
    stat_parse_skip_space( s );
    if ( nExpectedLength > 0 ) {
        for ( ; strResult.length() < size_t( nExpectedLength ); )
            strResult += '0';
    }
    return strResult;
}
static bool stat_parse_accumulate_unsigned_integer(
    const char*& s, uint64_t& nResult, int nExpectedLength = -1 ) {
    nResult = 0;
    std::string strInteger = stat_parse_accumulate_digits( s, nExpectedLength );
    if ( strInteger.empty() )
        return true;
    try {
        std::istringstream is( strInteger );
        is >> nResult;
        return true;
    } catch ( ... ) {
    }
    return false;
}

// notice: requires exact time format
// example: 2018-03-21 20:44:12.977332
bool string2time( const char* s, std::tm& aTm, uint64_t& nMicroSeconds ) {
    //::memcpy( &aTm, 0, sizeof(std::tm) );
    nMicroSeconds = 0;
    if ( s == nullptr || ( *s ) == '\0' )
        return false;
    uint64_t year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, micro = 0;
    if ( !stat_parse_accumulate_unsigned_integer( s, year ) )
        return false;
    if ( year < 1900 )
        return false;
    if ( !stat_parse_expect_one_of_characters( s, "-:" ) )  // '-' between year and month
        return false;
    if ( !stat_parse_accumulate_unsigned_integer( s, month ) )
        return false;
    if ( month < 1 || month > 12 )
        return false;
    if ( !stat_parse_expect_one_of_characters( s, "-:" ) )  // '-' month year and day
        return false;
    if ( !stat_parse_accumulate_unsigned_integer( s, day ) )
        return false;
    if ( day < 1 || day > 31 )
        return false;
    // stat_parse_expect_at_least_one_space( s ); // space(s) between day and hour
    if ( !stat_parse_expect_one_of_characters( s, " Tt" ) )  // '-' month year and day
        return false;
    if ( !stat_parse_accumulate_unsigned_integer( s, hour ) )
        return false;
    if ( hour > 23 )
        return false;
    if ( !stat_parse_expect_one_character( s, ':' ) )  // ':' month hour and minute
        return false;
    if ( !stat_parse_accumulate_unsigned_integer( s, minute ) )
        return false;
    if ( minute > 59 )
        return false;
    if ( !stat_parse_expect_one_character( s, ':' ) )  // ':' minute hour and second
        return false;
    if ( !stat_parse_accumulate_unsigned_integer( s, second ) )
        return false;
    if ( second > 60 )
        return false;
    if ( !stat_parse_expect_one_character( s, '.' ) )  // '.' month second and micro
        return false;
    if ( !stat_parse_accumulate_unsigned_integer( s, micro, 6 ) )
        return false;
    if ( micro > 999999 )
        return false;
    aTm.tm_year = year - 1900;  // years since [1900..)
    aTm.tm_mon = month - 1;     // zero based month [0..11]
    aTm.tm_mday = day;          // [1..31]
    aTm.tm_hour = hour;         // [0..23]
    aTm.tm_min = minute;        // [0..59]
    aTm.tm_sec = second;        // [0..60]
    nMicroSeconds = micro;      // [0..999999]
    return true;
}

// notice: requires exact time format
// example: 2018-03-21 20:44:12.977332
bool string2time(
    const std::string& timeStr, std::chrono::high_resolution_clock::time_point& ptTime ) {
    std::tm aTm;
    uint64_t ms = 0;

    cc::string2time( timeStr.c_str(), aTm, ms );
    std::time_t tt = timegm( &aTm );
    ptTime += std::chrono::duration< uint64_t >( tt ) - std::chrono::microseconds( ms );
    return true;
}


bool string2duration( const std::string& s, std::chrono::duration< uint64_t >& outDuration,
    const std::chrono::hours& hours, const std::chrono::minutes& minutes,
    const std::chrono::seconds& seconds ) {
    if ( s.empty() || ( s.length() != 8 && s.length() != 5 && s.length() != 2 ) )
        return false;

    uint64_t parsedHour = 61, parsedMinute = 61, parsedSecond = 61;
    const char* sPtr = s.c_str();
    if ( s.length() == 8 ) {
        if ( !stat_parse_accumulate_unsigned_integer( sPtr, parsedHour ) )
            return false;
        if ( parsedHour > 23 )
            return false;
        if ( !stat_parse_expect_one_character( sPtr, ':' ) )  // ':' month hour and minute
            return false;
    }

    if ( s.length() == 8 || s.length() == 5 ) {
        if ( !stat_parse_accumulate_unsigned_integer( sPtr, parsedMinute ) )
            return false;
        if ( parsedMinute > 59 )
            return false;
        if ( !stat_parse_expect_one_character( sPtr, ':' ) )  // ':' minute hour and second
            return false;
    }

    if ( s.length() == 8 || s.length() == 5 || s.length() == 2 ) {
        if ( !stat_parse_accumulate_unsigned_integer( sPtr, parsedSecond ) )
            return false;
        if ( parsedSecond > 60 )
            return false;
    }

    outDuration +=
        ( ( parsedHour == 61 ) ? hours : std::chrono::hours( parsedHour ) ) +          // [0..23]
        ( ( parsedMinute == 61 ) ? minutes : std::chrono::minutes( parsedMinute ) ) +  // [0..59]
        ( ( parsedSecond == 61 ) ? seconds : std::chrono::seconds( parsedSecond ) );   // [0..60]

    return true;
}

template < typename Container, typename Fun >
void tuple_for_each( const Container& c, Fun fun ) {
    for ( auto& e : c )
        fun( std::get< 0 >( e ), std::get< 1 >( e ), std::get< 2 >( e ) );
}

std::string duration2string( std::chrono::nanoseconds time ) {
    using namespace std::chrono;

    using T = std::tuple< std::chrono::nanoseconds, int, const char* >;

    constexpr T formats[] = {T{hours( 1 ), 2, ""}, T{minutes( 1 ), 2, ":"}, T{seconds( 1 ), 2, ":"},
        T{milliseconds( 1 ), 3, "."}};

    std::ostringstream o;
    tuple_for_each( formats, [&time, &o]( auto denominator, auto width, auto separator ) {
        o << separator << std::setw( width ) << std::setfill( '0' ) << ( time / denominator );
        time = time % denominator;
    } );
    return o.str();
}

std::string time2string(
    std::time_t tt, uint64_t nMicroSeconds, bool isUTC, bool isColored /*= true*/ ) {
    struct std::tm aTm = isUTC ? ( *std::gmtime( &tt ) ) : ( *std::localtime( &tt ) );
    return time2string( aTm, nMicroSeconds, isColored );
}
std::string time2string( const std::tm& aTm, uint64_t nMicroSeconds, bool isColored /*= true*/ ) {
    const std::tm& effective_tm = aTm;
    std::stringstream ss;
    ss << std::setfill( '0' );
    if ( isColored )
        ss << dpart();
    ss << std::setw( 4 ) << ( effective_tm.tm_year + 1900 );
    if ( isColored )
        ss << tsep();
    ss << '-';
    if ( isColored )
        ss << dpart();
    ss << std::setw( 2 ) << ( effective_tm.tm_mon + 1 );
    if ( isColored )
        ss << tsep();
    ss << '-';
    if ( isColored )
        ss << dpart();
    ss << std::setw( 2 ) << effective_tm.tm_mday;
    if ( isColored )
        ss << tsep();
    ss << ' ';
    if ( isColored )
        ss << tpart();
    ss << std::setw( 2 ) << effective_tm.tm_hour;
    if ( isColored )
        ss << tsep();
    ss << ':';
    if ( isColored )
        ss << tpart();
    ss << std::setw( 2 ) << effective_tm.tm_min;
    if ( isColored )
        ss << tsep();
    ss << ':';
    if ( isColored )
        ss << tpart();
    ss << std::setw( 2 );
    ss << effective_tm.tm_sec;
    if ( isColored )
        ss << tsep();
    ss << ".";
    if ( isColored )
        ss << tfrac();
    ss << std::setw( 6 ) << nMicroSeconds;
    if ( isColored )
        ss << reset();
    std::string s = ss.str();
    return s;
}

template < typename clock_type_t = std::chrono::high_resolution_clock >
inline time_t clock_2_time_t( const typename clock_type_t::time_point& ptTime ) {
#if ( defined __BUILDING_4_MAC_OS_X__ )
    time_t tt =
        std::chrono::time_point_cast< std::chrono::seconds >( ptTime ).time_since_epoch().count();
    return tt;
#else   // (defined __BUILDING_4_MAC_OS_X__)
    time_t tt = clock_type_t::to_time_t( ptTime );
    return tt;
#endif  // (defined __BUILDING_4_MAC_OS_X__)
}

std::string time2string( const std::chrono::high_resolution_clock::time_point& ptTime, bool isUTC,
    bool isDaysInsteadOfYMD, bool isColored /*= true*/ ) {
    std::stringstream ss;
    typedef std::chrono::duration< int,
        std::ratio_multiply< std::chrono::hours::period, std::ratio< 24 > >::type >
        days;
    std::chrono::high_resolution_clock::duration tp = ptTime.time_since_epoch();
    days d = std::chrono::duration_cast< days >( tp );
    tp -= d;
    std::chrono::hours h = std::chrono::duration_cast< std::chrono::hours >( tp );
    tp -= h;
    std::chrono::minutes m = std::chrono::duration_cast< std::chrono::minutes >( tp );
    tp -= m;
    std::chrono::seconds sec = std::chrono::duration_cast< std::chrono::seconds >( tp );
    tp -= sec;
    // std::chrono::milliseconds millis = std::chrono::duration_cast < std::chrono::milliseconds > (
    // tp );
    std::chrono::microseconds micros =
        std::chrono::duration_cast< std::chrono::microseconds >( tp );
    // std::chrono::nanoseconds nanos = std::chrono::duration_cast < std::chrono::nanoseconds > ( tp
    // ); typedef std::chrono::duration < int64_t, std::pico > picoseconds; picoseconds picos =
    // std::chrono::duration_cast < picoseconds > ( tp );
    ss << std::setfill( '0' );
    if ( isDaysInsteadOfYMD ) {
        if ( isColored )
            ss << day_count();
        ss << std::setw( 5 ) << d.count();
        if ( isColored )
            ss << tsep();
        ss << ' ';
        if ( isColored )
            ss << tpart();
        ss << std::setw( 2 ) << h.count();
        if ( isColored )
            ss << tsep();
        ss << ':';
        if ( isColored )
            ss << tpart();
        ss << std::setw( 2 ) << m.count();
        if ( isColored )
            ss << tsep();
        ss << ':';
        if ( isColored )
            ss << tpart();
        ss << std::setw( 2 ) << sec.count();
        if ( isColored )
            ss << std::setw( 0 );
        if ( isColored )
            ss << tsep();
        ss << '.';
        if ( isColored )
            ss << tfrac();
        ss << std::setw( 6 ) << micros.count();
        if ( isColored )
            ss << reset();
    } else {
        time_t tt = clock_2_time_t( ptTime );
        // tm utc_tm = *gmtime( &tt );
        // tm local_tm = *localtime( &tt );
        tm effective_tm = isUTC ? ( *gmtime( &tt ) ) : ( *localtime( &tt ) );
        if ( isColored )
            ss << dpart();
        ss << std::setw( 4 ) << ( effective_tm.tm_year + 1900 );
        if ( isColored )
            ss << tsep();
        ss << '-';
        if ( isColored )
            ss << dpart();
        ss << std::setw( 2 ) << ( effective_tm.tm_mon + 1 );
        if ( isColored )
            ss << tsep();
        ss << '-';
        if ( isColored )
            ss << dpart();
        ss << std::setw( 2 ) << effective_tm.tm_mday;
        if ( isColored )
            ss << tsep();
        ss << ' ';
        if ( isColored )
            ss << tpart();
        ss << std::setw( 2 ) << effective_tm.tm_hour;
        if ( isColored )
            ss << tsep();
        ss << ':';
        if ( isColored )
            ss << tpart();
        ss << std::setw( 2 ) << effective_tm.tm_min;
        if ( isColored )
            ss << tsep();
        ss << ':';
        if ( isColored )
            ss << tpart();
        ss << std::setw( 2 ) << effective_tm.tm_sec;
        if ( isColored )
            ss << tsep();
        ss << '.';
        if ( isColored )
            ss << tfrac();
        ss << std::setw( 6 ) << micros.count();
        if ( isColored )
            ss << reset();
    }
    std::string s = ss.str();
    return s;
}
std::string now2string( bool isUTC, bool isDaysInsteadOfYMD, bool isColored /*= true*/ ) {
    std::chrono::high_resolution_clock::time_point ptTimeNow =
        std::chrono::high_resolution_clock::now();
    std::string s = time2string( ptTimeNow, isUTC, isDaysInsteadOfYMD, isColored );
    return s;
}

std::string jsNow2string( bool isUTC /*= true*/ ) {
    return now2string( isUTC, false, false );  // a.kozachenko TODO: make it in jsTime format
}

volatile bool g_bEnableJsonColorization = true;
std::string j( const nlohmann::json& x ) {
    return g_bEnableJsonColorization ? c( x ) : x.dump();
}
std::string j( const std::string& strProbablyJson ) {
    if ( g_bEnableJsonColorization ) {
        try {
            nlohmann::json jo = nlohmann::json::parse( strProbablyJson );
            return c( jo );
        } catch ( ... ) {
        }
    }
    return c( strProbablyJson );
}
std::string j( const char* strProbablyJson ) {
    return j( std::string( strProbablyJson ) );
}

bool u( std::string& s, const skutils::url& url, bool bThrow, bool bInterrupt ) {
    s.clear();
    std::stringstream ss_url;
    std::int8_t nIPversion = url.ip_version();
    std::string strScheme = url.scheme();
    std::string strHost = url.host();
    std::string strUserName = url.user_name();
    std::string strPwd = url.user_password();
    std::string strPort = url.port();
    std::string strPath = url.path();
    std::string strFragment = url.fragment();
    if ( !strScheme.empty() )
        ss_url << url_prefix_scheme() << strScheme << url_prefix_punct() << ":";
    if ( !strHost.empty() ) {
        ss_url << url_prefix_slash() << "//";
        if ( !strUserName.empty() ) {
            ss_url << url_prefix_user_name() << skutils::url::stat_do_encode( strUserName, 0x05 );
            if ( !strPwd.empty() ) {
                ss_url << url_prefix_punct() << ':';
                ss_url << url_prefix_user_pass() << skutils::url::stat_do_encode( strPwd, 0x05 );
            }
            ss_url << url_prefix_at() << '@';
        }
        if ( nIPversion == 0 || nIPversion == 4 )
            ss_url << url_prefix_host() << strHost;
        else if ( nIPversion == 6 )
            ss_url << url_prefix_punct() << "[" << url_prefix_host() << strHost
                   << url_prefix_punct() << "]";
        else
            ss_url << url_prefix_punct() << "[v" << url_prefix_number() << std::hex
                   << ( int ) nIPversion << std::dec << url_prefix_punct() << '.'
                   << url_prefix_host() << strHost << url_prefix_punct() << "]";
        if ( !strPort.empty() )
            if ( !( ( strScheme == "http" && strPort == "80" ) ||
                     ( strScheme == "https" && strPort == "443" ) ) )
                ss_url << url_prefix_punct() << ":" << url_prefix_port() << strPort;
    } else {
        if ( !strUserName.empty() ) {
            ss_url << reset();
            if ( bThrow )
                throw skutils::url::compose_error( "user info defined, but host is empty" );
            if ( bInterrupt ) {
                s = ss_url.str();
                return false;
            }
        }
        if ( !strPort.empty() ) {
            ss_url << reset();
            if ( bThrow )
                throw skutils::url::compose_error( "port defined, but host is empty" );
            if ( bInterrupt ) {
                s = ss_url.str();
                return false;
            }
        }
        if ( !strPath.empty() ) {
            const char *b = strPath.data(), *e = b + strPath.length(),
                       *p = skutils::find_first_of( b, e, ":/" );
            if ( p != e && *p == ':' ) {
                ss_url << reset();
                if ( bThrow )
                    throw skutils::url::compose_error(
                        "the first segment of the relative path can't contain ':'" );
                if ( bInterrupt ) {
                    s = ss_url.str();
                    return false;
                }
            }
        }
    }
    if ( !strPath.empty() ) {
        if ( strPath[0] != '/' && ( !strHost.empty() ) ) {
            ss_url << reset();
            if ( bThrow )
                throw skutils::url::compose_error(
                    "path must start with '/' when host is not empty" );
            if ( bInterrupt ) {
                s = ss_url.str();
                return false;
            }
        }
        // ss_url << url_prefix_path() << skutils::url::stat_do_encode( strPath, 0x0F );
        std::string spe = skutils::url::stat_do_encode( strPath, 0x0F );
        auto itWalk = spe.cbegin(), itEnd = spe.cend();
        bool bLastIsSlash = false;
        for ( size_t i = 0; itWalk != itEnd; ++itWalk, ++i ) {
            char c = ( *itWalk );
            if ( i == 0 ) {
                if ( c == '/' ) {
                    ss_url << url_prefix_slash();
                    bLastIsSlash = true;
                } else {
                    ss_url << url_prefix_path();
                    bLastIsSlash = true;
                }
            } else {
                bool bNowIsSlash = ( c == '/' ) ? true : false;
                if ( bLastIsSlash != bNowIsSlash ) {
                    if ( c == '/' )
                        ss_url << url_prefix_slash();
                    else
                        ss_url << url_prefix_path();
                    bLastIsSlash = bNowIsSlash;
                }
            }
            ss_url << c;
        }
    }
    const skutils::url::Query& kvQuery = url.query();
    if ( !kvQuery.empty() ) {
        ss_url << url_prefix_qq() << "?";
        auto it = kvQuery.begin(), end = kvQuery.end();
        if ( it->key().empty() ) {
            ss_url << reset();
            if ( bThrow )
                throw skutils::url::compose_error( "first query entry has no key" );
            if ( bInterrupt ) {
                s = ss_url.str();
                return false;
            }
        }
        ss_url << url_prefix_qk() << skutils::url::stat_do_encode_query_key( it->key(), 0x1F );
        if ( !it->val().empty() )
            ss_url << url_prefix_qe() << "=" << url_prefix_qv()
                   << skutils::url::stat_do_encode_query_val( it->val(), 0x1F );
        while ( ++it != end ) {
            if ( it->key().empty() ) {
                ss_url << reset();
                if ( bThrow )
                    throw skutils::url::compose_error( "a query entry has no key" );
                if ( bInterrupt ) {
                    s = ss_url.str();
                    return false;
                }
            }
            ss_url << url_prefix_qa() << "&" << url_prefix_qk()
                   << skutils::url::stat_do_encode_query_key( it->key(), 0x1F );
            if ( !it->val().empty() )
                ss_url << url_prefix_qe() << "=" << url_prefix_qv()
                       << skutils::url::stat_do_encode_query_val( it->val(), 0x1F );
        }
    }
    if ( !strFragment.empty() )
        ss_url << url_prefix_hash() << "#" << url_prefix_fragment()
               << skutils::url::stat_do_encode( strFragment, 0x1F );
    ss_url << reset();
    s = ss_url.str();
    return true;
}
bool u( std::string& s, const std::string& strProbablyURL, bool bThrow, bool bInterrupt ) {
    s.clear();
    try {
        if ( !strProbablyURL.empty() ) {
            size_t nPos = strProbablyURL.find( "://" );
            if ( nPos != std::string::npos && nPos > 0 ) {
                skutils::url url( strProbablyURL );
                if ( !( url.scheme().empty() || url.host().empty() ) )
                    if ( u( s, url, bThrow, bInterrupt ) )
                        return true;
            }
        }
    } catch ( ... ) {
    }
    s = strProbablyURL;
    return false;
}
std::string u( const skutils::url& url ) {
    std::string s;
    try {
        u( s, url, false, false );
    } catch ( ... ) {
        s.clear();
    }
    // return cc::str( s );
    return s;
}
std::string u( const std::string& strProbablyURL ) {
    try {
        if ( !strProbablyURL.empty() ) {
            size_t nPos = strProbablyURL.find( "://" );
            if ( nPos != std::string::npos && nPos > 0 ) {
                skutils::url url( strProbablyURL );
                if ( !( url.scheme().empty() || url.host().empty() ) )
                    return u( url );
            }
        }
    } catch ( ... ) {
    }
    return strProbablyURL;
}
std::string u( const char* strProbablyURL ) {
    if ( strProbablyURL == 0 || strProbablyURL[0] == '\0' )
        return "";
    return u( std::string( strProbablyURL ) );
}

bool p( std::string& s, const std::string& strProbablyPath, bool bThrow ) {
    try {
        s.clear();
        if ( strProbablyPath.empty() ) {
            if ( bThrow )
                throw std::runtime_error( "not an absolute path" );
            return false;
        }
        size_t nPos = strProbablyPath.find( "/" );
        if ( nPos == std::string::npos ) {
            if ( bThrow )
                throw std::runtime_error( "not an absolute path" );
            return false;
        }
        std::string rest = strProbablyPath;
        std::stringstream ss;
        for ( ; !rest.empty(); ) {
            if ( nPos > 0 ) {
                ss << path_part() << rest.substr( 0, nPos );
                rest.erase( 0, nPos + 1 );
            } else
                rest.erase( 0, 1 );
            ss << path_slash() << '/';
            nPos = rest.find( "/" );
            if ( nPos == std::string::npos ) {
                if ( !rest.empty() )
                    ss << path_part() << rest.substr( 0, nPos );
                break;
            }
        }  // for( ; ! rest.empty(); )
        ss << reset();
        s = ss.str();
        return true;
    } catch ( ... ) {
        if ( bThrow )
            throw;
    }
    s.clear();
    return false;
}
std::string p( const std::string& strProbablyPath ) {
    std::string s;
    if ( !p( s, strProbablyPath, false ) )
        return strProbablyPath;
    return s;
}
std::string p( const char* strProbablyPath ) {
    if ( strProbablyPath == 0 || strProbablyPath[0] == '\0' )
        return "";
    return p( std::string( strProbablyPath ) );
}

std::string pe( const std::string& strPath ) {  // path with comment about file existance and size
    return pe( strPath.c_str() );
}
std::string pe( const char* strPath ) {  // path with comment about file existance and size
    if ( strPath == nullptr )
        return null_str();
    if ( strPath[0] == '\0' )
        return empty_str();
    std::string strColoredPath = cc::p( strPath );
    size_t nFileSize = 0;
    if ( !skutils::tools::file_exists( strPath, &nFileSize ) )
        return strColoredPath + cc::debug( " " ) + cc::fatal( "file does not exist" );
    std::stringstream ss;
    ss << strColoredPath << cc::success( " file exists, " ) << cc::size10( nFileSize )
       << cc::success( " bytes" );
    return ss.str();
}

static bool stat_is_email_enabled_char( char c ) {
    if (::isalnum( c ) || c == '.' || c == '_' )
        return true;
    return false;
}
static bool stat_is_email_enabled_chars_only( const std::string& s ) {
    std::string::const_iterator itWalk = s.cbegin(), itEnd = s.cend();
    for ( ; itWalk != itEnd; ++itWalk ) {
        if ( !stat_is_email_enabled_char( *itWalk ) )
            return false;
    }
    return true;
}
bool eml( std::string& s, const std::string& strProbablyeMail, bool /*bThrow*/ ) {
    s.clear();
    if ( strProbablyeMail.empty() )
        return false;
    skutils::string_list_t top_list = skutils::tools::split( strProbablyeMail, '@' );
    size_t cntTopParts = top_list.size();
    if ( cntTopParts != 2 )
        return false;
    const std::string &strEmailName = top_list.front(), &strEmailDomain = top_list.back();
    if ( strEmailName.empty() || strEmailDomain.empty() ||
         ( !stat_is_email_enabled_chars_only( strEmailDomain ) ) )
        return false;
    skutils::string_list_t domain_list = skutils::tools::split( strEmailDomain, '.' );
    size_t cntDomainParts = domain_list.size();
    if ( cntDomainParts < 1 )  // ??? may be < 2 ???
        return false;
    skutils::string_list_t::const_iterator itWalk = domain_list.cbegin(), itEnd =
                                                                              domain_list.cend();
    for ( ; itWalk != itEnd; ++itWalk ) {
        const std::string& strDomainPart = ( *itWalk );
        if ( strDomainPart.empty() || ( !stat_is_email_enabled_chars_only( strDomainPart ) ) )
            return false;
    }
    std::stringstream ss;
    ss << url_prefix_user_name() << strEmailName << url_prefix_at() << '@' << url_prefix_host()
       << strEmailDomain << reset();
    s = ss.str();
    return true;
}
std::string eml( const std::string& strProbablyeMail ) {
    std::string s;
    if ( !eml( s, strProbablyeMail, false ) )
        return strProbablyeMail;
    return s;
}
std::string eml( const char* strProbablyeMail ) {
    if ( strProbablyeMail == 0 || strProbablyeMail[0] == '\0' )
        return "";
    return eml( std::string( strProbablyeMail ) );
}

std::string not_computed_yet_str() {  // "not computed yet"
    return std::string( error() ) + "not computed yet" + std::string( reset() );
}
std::string empty_str() {  // "empty"
    return std::string( error() ) + "empty" + std::string( reset() );
}
std::string null_str() {  // "null"
    return std::string( error() ) + "null" + std::string( reset() );
}

std::string binary_singleline( const void* pBinary, size_t cnt,
    size_t cntAlign,          // = 16
    char chrEmptyPosition,    // = ' '
    const char* strSeparator  // = nullptr
) {
    if ( pBinary == nullptr )
        return null_str();
    std::stringstream ss;
    const uint8_t* p = ( const uint8_t* ) pBinary;
    size_t i;
    for ( i = 0; i < cnt; ++i, ++p ) {
        if ( i > 0 && strSeparator != nullptr && strSeparator[0] != '\0' )
            ss << strSeparator;
        const uint8_t a_byte = ( *p );
        ss << num016( a_byte );
    }
    for ( ; i < cntAlign; ++i, ++p ) {
        if ( i > 0 && strSeparator != nullptr && strSeparator[0] != '\0' )
            ss << strSeparator;
        ss << chrEmptyPosition << chrEmptyPosition;
    }
    return ss.str();
}
std::string binary_singleline( const std::string& strBinaryBuffer,
    size_t cntAlign,          // = 16
    char chrEmptyPosition,    // = ' '
    const char* strSeparator  // = nullptr
) {
    if ( strBinaryBuffer.empty() )
        return empty_str();
    std::vector< uint8_t > vec( std::begin( strBinaryBuffer ), std::end( strBinaryBuffer ) );
    const void* pBinary = vec.data();
    size_t cnt = vec.size();
    return binary_singleline( pBinary, cnt, cntAlign, chrEmptyPosition, strSeparator );
}
std::string binary_singleline( const void* pBinary, size_t cnt,
    const char* strSeparator  // = nullptr
) {
    return binary_singleline( pBinary, cnt, cnt, ' ', strSeparator );
}
std::string binary_singleline( const std::string& strBinaryBuffer,
    const char* strSeparator  // = nullptr
) {
    if ( strBinaryBuffer.empty() )
        return empty_str();
    std::vector< uint8_t > vec( std::begin( strBinaryBuffer ), std::end( strBinaryBuffer ) );
    const void* pBinary = vec.data();
    size_t cnt = vec.size();
    return binary_singleline( pBinary, cnt, strSeparator );
}

std::string binary_singleline_ascii( const void* pBinary, size_t cnt,
    size_t cntAlign,          // = 16
    char chrNonPrintable,     // = '?'
    char chrEmptyPosition,    // = ' '
    const char* strSeparator  // = nullptr
) {
    if ( pBinary == nullptr )
        return null_str();
    std::stringstream ss;
    const uint8_t* p = ( const uint8_t* ) pBinary;
    size_t i;
    for ( i = 0; i < cnt; ++i, ++p ) {
        if ( i > 0 && strSeparator != nullptr && strSeparator[0] != '\0' )
            ss << strSeparator;
        const uint8_t a_byte = ( *p );
        bool is_pribtable = ::isprint( a_byte ) ? true : false;
        char c = is_pribtable ? char( a_byte ) : chrNonPrintable;
        std::string part;
        part += c;
        ss << ( is_pribtable ? cc::note( part ) : cc::debug( part ) );
    }
    for ( ; i < cntAlign; ++i, ++p ) {
        if ( i > 0 && strSeparator != nullptr && strSeparator[0] != '\0' )
            ss << strSeparator;
        ss << chrEmptyPosition;
    }
    return ss.str();
}
std::string binary_singleline_ascii( const std::string& strBinaryBuffer,
    size_t cntAlign,          // = 16
    char chrNonPrintable,     // = '?'
    char chrEmptyPosition,    // = ' '
    const char* strSeparator  // = nullptr
) {
    if ( strBinaryBuffer.empty() )
        return empty_str();
    std::vector< uint8_t > vec( std::begin( strBinaryBuffer ), std::end( strBinaryBuffer ) );
    const void* pBinary = vec.data();
    size_t cnt = vec.size();
    return binary_singleline_ascii(
        pBinary, cnt, cntAlign, chrNonPrintable, chrEmptyPosition, strSeparator );
}

std::string binary_table( const void* pBinary, size_t cnt,
    size_t cntPerRow,                       // = 16
    bool isAlsoGenerateIndexNumbersPrefix,  // = true
    bool isAlsoGenerateAsciiSuffix,         // = true
    char chrNonPrintable,                   // = '?'
    char chrEmptyPosition,                  // = ' '
    const char* strSeparatorHex,            // = " "
    const char* strSeparatorAscii,          // = nullptr
    const char* strSeparatorBetweenTables,  // = "|"
    const char* strRowPrefix,               // = nullptr
    const char* strRowSuffix                // = "\n"
) {
    if ( pBinary == nullptr )
        return null_str();
    std::stringstream ss;
    const uint8_t* p = ( const uint8_t* ) pBinary;
    size_t i, stepRow = ( cntPerRow > 0 ) ? cntPerRow : 16;
    for ( i = 0; i < cnt; i += cntPerRow, ++p ) {
        if ( strRowPrefix != nullptr && strRowPrefix[0] != '\0' )
            ss << cc::debug( strRowPrefix );
        if ( isAlsoGenerateIndexNumbersPrefix ) {
            ss << size016( i );
            if ( strSeparatorBetweenTables != nullptr && strSeparatorBetweenTables[0] != '\0' )
                ss << cc::warn( strSeparatorBetweenTables );
        }
        size_t cntThisRow = cnt - i;
        if ( cntThisRow > stepRow )
            cntThisRow = stepRow;
        ss << binary_singleline( p, cntThisRow, stepRow, chrEmptyPosition, strSeparatorHex );
        if ( isAlsoGenerateAsciiSuffix ) {
            if ( strSeparatorBetweenTables != nullptr && strSeparatorBetweenTables[0] != '\0' )
                ss << cc::warn( strSeparatorBetweenTables );
            ss << binary_singleline_ascii(
                p, cntThisRow, stepRow, chrNonPrintable, chrEmptyPosition, strSeparatorAscii );
        }
        if ( strRowSuffix != nullptr && strRowSuffix[0] != '\0' )
            ss << strRowSuffix;
    }
    return ss.str();
}
std::string binary_table( const std::string& strBinaryBuffer,
    size_t cntPerRow,                       // = 16
    bool isAlsoGenerateIndexNumbersPrefix,  // = true
    bool isAlsoGenerateAsciiSuffix,         // = true
    char chrNonPrintable,                   // = '?'
    char chrEmptyPosition,                  // = ' '
    const char* strSeparatorHex,            // = " "
    const char* strSeparatorAscii,          // = nullptr
    const char* strSeparatorBetweenTables,  // = "|"
    const char* strRowPrefix,               // = nullptr
    const char* strRowSuffix                // = "\n"
) {
    if ( strBinaryBuffer.empty() )
        return empty_str();
    std::vector< uint8_t > vec( std::begin( strBinaryBuffer ), std::end( strBinaryBuffer ) );
    const void* pBinary = vec.data();
    size_t cnt = vec.size();
    return binary_table( pBinary, cnt, cntPerRow, isAlsoGenerateIndexNumbersPrefix,
        isAlsoGenerateAsciiSuffix, chrNonPrintable, chrEmptyPosition, strSeparatorHex,
        strSeparatorAscii, strSeparatorBetweenTables, strRowPrefix, strRowSuffix );
}

std::string rpc_message( const nlohmann::json& x ) {
    return cc::j( x );
}
std::string rpc_message( const std::string& msg, bool isBinaryMessage ) {
    if ( !isBinaryMessage )
        return cc::j( msg );
    std::string s;
    size_t cnt = msg.size();
    if ( cnt > 0 )
        s += cc::debug( "binary data of " ) + cc::size( cnt ) + cc::debug( " byte(s):\n" );
    std::string bt = cc::binary_table( msg );
    size_t bt_len = bt.length();
    if ( bt_len > 0 ) {
        char c = bt[bt_len - 1];
        if ( c == '\n' )
            bt.erase( bt_len - 1, 1 );
    }
    s += bt;
    return s;
}

std::string rpc_rpc_log_type_2_str( e_rpc_log_type_t erpcltt ) {
    std::string s( "WS" );
    switch ( erpcltt ) {
    case e_rpc_log_type_t::erpcltt_ws_tx:
        s += " Tx";
        break;
    case e_rpc_log_type_t::erpcltt_ws_rx:
        s += " Rx";
        break;
    }
    return s;
}
std::string rpc_rpc_log_type_2_direction_arrows_str( e_rpc_log_type_t erpcltt ) {
    std::string s;
    switch ( erpcltt ) {
    case e_rpc_log_type_t::erpcltt_ws_tx:
        s += " >>> ";
        break;
    case e_rpc_log_type_t::erpcltt_ws_rx:
        s += " <<< ";
        break;
    }
    return s;
}
std::string rpc_rpc_log_colorize_prefix( e_rpc_log_type_t erpcltt, const std::string& s ) {
    switch ( erpcltt ) {
    case e_rpc_log_type_t::erpcltt_ws_tx:
        return ws_tx_inv( s );
    case e_rpc_log_type_t::erpcltt_ws_rx:
        return ws_rx_inv( s );
    }
    return s;
}
std::string rpc_rpc_log_colorize_suffix( e_rpc_log_type_t erpcltt, const std::string& s ) {
    switch ( erpcltt ) {
    case e_rpc_log_type_t::erpcltt_ws_tx:
        return ws_tx( s );
    case e_rpc_log_type_t::erpcltt_ws_rx:
        return ws_rx( s );
    }
    return s;
}

std::string rpc_log( e_rpc_log_type_t erpcltt,
    const char* strConnectionType,         // can be nullptr
    const char* strConnectionDescription,  // can be nullptr
    const std::string& msg, bool isBinaryMessage ) {
    std::string strPrefix = rpc_rpc_log_type_2_str( erpcltt ), strSuffix;
    if ( strConnectionType != nullptr && strConnectionType[0] != '\0' ) {
        strPrefix += '(';
        strPrefix += strConnectionType;
        strPrefix += ')';
    }
    strPrefix = rpc_rpc_log_colorize_prefix( erpcltt, strPrefix );
    std::string strArrows = rpc_rpc_log_type_2_direction_arrows_str( erpcltt );
    strArrows = rpc_rpc_log_colorize_suffix( erpcltt, strArrows );
    if ( strConnectionDescription != nullptr && strConnectionDescription[0] != '\0' ) {
        strSuffix += strArrows;
        std::string s2;
        if ( u( s2, strConnectionDescription, false, false ) ) {
            // yes, it's URL
        } else if ( eml( s2, strConnectionDescription, false ) ) {
            // yes, it's e-mail
        } else
            s2 = rpc_rpc_log_colorize_suffix( erpcltt, strConnectionDescription );
        strSuffix += s2;
    }
    strSuffix += strArrows;
    return strPrefix + strSuffix + rpc_message( msg, isBinaryMessage );
}
std::string rpc_log( e_rpc_log_type_t erpcltt,
    const std::string& strConnectionType,         // can be empty
    const std::string& strConnectionDescription,  // can be empty
    const std::string& msg, bool isBinaryMessage ) {
    return rpc_log( erpcltt, strConnectionType.c_str(), strConnectionDescription.c_str(), msg,
        isBinaryMessage );
}
std::string rpc_log( e_rpc_log_type_t erpcltt, const std::string& msg, bool isBinaryMessage ) {
    return rpc_log( erpcltt, nullptr, nullptr, msg, isBinaryMessage );
}
std::string rpc_log( e_rpc_log_type_t erpcltt,
    const char* strConnectionType,         // can be nullptr
    const char* strConnectionDescription,  // can be nullptr
    const nlohmann::json& x ) {
    return rpc_log( erpcltt, strConnectionType, strConnectionDescription, x.dump(), false );
}
std::string rpc_log( e_rpc_log_type_t erpcltt,
    const std::string& strConnectionType,         // can be empty
    const std::string& strConnectionDescription,  // can be empty
    const nlohmann::json& x ) {
    return rpc_log(
        erpcltt, strConnectionType.c_str(), strConnectionDescription.c_str(), x.dump(), false );
}
std::string rpc_log( e_rpc_log_type_t erpcltt, const nlohmann::json& x ) {
    return rpc_log( erpcltt, nullptr, nullptr, x );
}

// l_sergiy: https://github.com/theZiz/aha/blob/master/aha.c
namespace ansi_conv {
// table for vt220 character set, see also
// https://whitefiles.org/b1_s/1_free_guides/fg2cd/pgs/c03b.htm
static const char ansi_vt220_character_set[256][16] = {
    "&#x2400;", "&#x2401;", "&#x2402;", "&#x2403;", "&#x2404;", "&#x2405;", "&#x2406;",
    "&#x2407;",  // 00..07
    "&#x2408;", "&#x2409;", "&#x240a;", "&#x240b;", "&#x240c;", "&#x240d;", "&#x240e;",
    "&#x240f;",  // 08..0f
    "&#x2410;", "&#x2411;", "&#x2412;", "&#x2413;", "&#x2414;", "&#x2415;", "&#x2416;",
    "&#x2417;",  // 10..17
    "&#x2418;", "&#x2419;", "&#x241a;", "&#x241b;", "&#x241c;", "&#x241d;", "&#x241e;",
    "&#x241f;",                                     // 18..1f
    " ", "!", "\"", "#", "$", "%", "&", "'",        // 20..27
    "(", ")", "*", "+", ",", "-", ".", "/",         // 28..2f
    "0", "1", "2", "3", "4", "5", "6", "7",         // 30..37
    "8", "9", ":", ";", "&lt;", "=", "&gt;", "?",   // 38..3f
    "@", "A", "B", "C", "D", "E", "F", "G",         // 40..47
    "H", "I", "J", "K", "L", "M", "N", "O",         // 48..4f
    "P", "Q", "R", "S", "T", "U", "V", "W",         // 50..57
    "X", "Y", "Z", "[", "\\", "]", "^", "_",        // 58..5f
    "`", "a", "b", "c", "d", "e", "f", "g",         // 60..67
    "h", "i", "j", "k", "l", "m", "n", "o",         // 68..6f
    "p", "q", "r", "s", "t", "u", "v", "w",         // 70..77
    "x", "y", "z", "{", "|", "}", "~", "&#x2421;",  // 78..7f
    "&#x25c6;", "&#x2592;", "&#x2409;", "&#x240c;", "&#x240d;", "&#x240a;", "&#x00b0;",
    "&#x00b1;",  // 80..87
    "&#x2400;", "&#x240b;", "&#x2518;", "&#x2510;", "&#x250c;", "&#x2514;", "&#x253c;",
    "&#x23ba;",  // 88..8f
    "&#x23bb;", "&#x2500;", "&#x23bc;", "&#x23bd;", "&#x251c;", "&#x2524;", "&#x2534;",
    "&#x252c;",  // 90..97
    "&#x2502;", "&#x2264;", "&#x2265;", "&pi;    ", "&#x2260;", "&pound;", "&#x0095;",
    "&#x2421;",                                                                     // 98..9f
    "&#x2588;", "&#x00a1;", "&#x00a2;", "&#x00a3;", " ", "&yen;", " ", "&#x00a7;",  // a0..a7
    "&#x00a4;", "&#x00a9;", "&#x00ba;", "&#x00qb;", " ", " ", " ", " ",             // a8..af
    "&#x23bc;", "&#x23bd;", "&#x00b2;", "&#x00b3;", "&#x00b4;", "&#x00b5;", "&#x00b6;",
    "&#x00b7;",  // b0..b7
    "&#x00b8;", "&#x00b9;", "&#x00ba;", "&#x00bb;", "&#x00bc;", "&#x00bd;", "&#x00be;",
    "&#x00bf;",  // b8..bf
    "&#x00c0;", "&#x00c1;", "&#x00c2;", "&#x00c3;", "&#x00c4;", "&#x00c5;", "&#x00c6;",
    "&#x00c7;",  // c0..c7
    "&#x00c8;", "&#x00c9;", "&#x00ca;", "&#x00cb;", "&#x00cc;", "&#x00cd;", "&#x00ce;",
    "&#x00cf;",  // c8..cf
    " ", "&#x00d1;", "&#x00d2;", "&#x00d3;", "&#x00d4;", "&#x00d5;", "&#x00d6;",
    "&#x0152;",  // d0..d7
    "&#x00d8;", "&#x00d9;", "&#x00da;", "&#x00db;", "&#x00dc;", "&#x0178;", " ",
    "&#x00df;",  // d8..df
    "&#x00e0;", "&#x00e1;", "&#x00e2;", "&#x00e3;", "&#x00e4;", "&#x00e5;", "&#x00e6;",
    "&#x00e7;",  // e0..e7
    "&#x00e8;", "&#x00e9;", "&#x00ea;", "&#x00eb;", "&#x00ec;", "&#x00ed;", "&#x00ee;",
    "&#x00ef;",  // e8..ef
    " ", "&#x00f1;", "&#x00f2;", "&#x00f3;", "&#x00f4;", "&#x00f5;", "&#x00f6;",
    "&#x0153;",  // f0..f7
    "&#x00f8;", "&#x00f9;", "&#x00fa;", "&#x00fb;", "&#x00fc;", "&#x00ff;", " ",
    "&#x2588;",  // f8..ff
};

//		static int getNextChar( register FILE * fp ) {
//			int c;
//			if( ( c = fgetc( fp ) ) != EOF )
//				return c;
//			fprintf( stderr, "Unknown Error in File Parsing!\n" );
//			exit(1);
//		}
static int getNextChar( std::istream& is ) {
    if ( is.eof() )
        return 0;
    char c;
    is.read( &c, 1 );
    return c;
}

typedef struct selem* pelem;
typedef struct selem {
    unsigned char digit[8];
    unsigned char digitcount;
    pelem next;
} telem;

static pelem parseInsert( char* s ) {
    pelem firstelem = nullptr;
    pelem momelem = nullptr;
    unsigned char digit[8];
    unsigned char digitcount = 0;
    int pos = 0;
    for ( pos = 0; pos < 1024; pos++ ) {
        if ( s[pos] == '[' )
            continue;
        if ( s[pos] == ';' || s[pos] == 0 ) {
            if ( digitcount == 0 ) {
                digit[0] = 0;
                digitcount = 1;
            }
            pelem newelem = ( pelem ) malloc( sizeof( telem ) );
            for ( unsigned char a = 0; a < 8; a++ )
                newelem->digit[a] = digit[a];
            newelem->digitcount = digitcount;
            newelem->next = nullptr;
            if ( momelem == nullptr )
                firstelem = newelem;
            else
                momelem->next = newelem;
            momelem = newelem;
            digitcount = 0;
            memset( digit, 0, 8 );
            if ( s[pos] == 0 )
                break;
        } else if ( digitcount < 8 ) {
            digit[digitcount] = s[pos] - '0';
            digitcount++;
        }
    }
    return firstelem;
}
static void deleteParse( pelem elem ) {
    while ( elem != nullptr ) {
        pelem temp = elem->next;
        free( elem );
        elem = temp;
    }
}
class producer {
public:
    enum class producer_mode_t { strip, normal, black, pink };

private:
    producer_mode_t pm_;
    std::ostream& os_;

public:
    int iso_;  // -1 is utf8
    char htop_fix_;
    bool black_background_is_transparent_;
    producer( producer_mode_t pm, std::ostream& os )
        : pm_( pm ),
          os_( os ),
          iso_( -1 )  // utf8
          ,
          htop_fix_( 0 ),
          black_background_is_transparent_( true ) {}
    void char_normal( char c ) { os_ << c; }
    void char_special( int c ) { os_ << ansi_vt220_character_set[( ( int ) c + 32 ) & 255]; }
    void char13() {
        // if( pm_ == producer_mode_t::strip )
        //	return;
        if ( pm_ == producer_mode_t::strip )
            os_ << "\n";
        else
            os_ << "<br>";
    }
    void char_double_quote() {
        if ( pm_ == producer_mode_t::strip )
            os_ << '\"';
        else
            os_ << "&quot;";
    }
    void char_less_than() {
        if ( pm_ == producer_mode_t::strip )
            os_ << '<';
        else
            os_ << "&lt;";
    }
    void char_greater_than() {
        if ( pm_ == producer_mode_t::strip )
            os_ << '>';
        else
            os_ << "&gt;";
    }
    void char_amp() {
        if ( pm_ == producer_mode_t::strip )
            os_ << '&';
        else
            os_ << "&amp;";
    }
    void space() {
        if ( pm_ == producer_mode_t::strip )
            return;
        os_ << " ";
    }
    void span_close() {
        if ( pm_ == producer_mode_t::strip )
            return;
        os_ << "</span>";
    }
    void span_open() {
        if ( pm_ == producer_mode_t::strip )
            return;
        os_ << "<span style=\"";
    }
    void tag_ending() {
        if ( pm_ == producer_mode_t::strip )
            return;
        os_ << "\">";
    }
    void foreground_color( int clr ) {
        if ( pm_ == producer_mode_t::strip )
            return;
        switch ( clr ) {
        case 0:
            os_ << "color:dimgray;";
            break;  // black
        case 1:
            os_ << "color:red;";
            break;  // red
        case 2:
            if ( pm_ != producer_mode_t::black )
                os_ << "color:green;";
            else
                os_ << "color:lime;";
            break;  // green
        case 3:
            if ( pm_ != producer_mode_t::black )
                os_ << "color:olive;";
            else
                os_ << "color:yellow;";
            break;  // yellow
        case 4:
            if ( pm_ != producer_mode_t::black )
                os_ << "color:blue;";
            else
                os_ << "color:#3333FF;";
            break;  // blue
        case 5:
            if ( pm_ != producer_mode_t::black )
                os_ << "color:purple;";
            else
                os_ << "color:fuchsia;";
            break;  // purple
        case 6:
            if ( pm_ != producer_mode_t::black )
                os_ << "color:teal;";
            else
                os_ << "color:aqua;";
            break;  // cyan
        case 7:
            if ( pm_ != producer_mode_t::black )
                os_ << "color:gray;";
            else
                os_ << "color:white;";
            break;  // white
        case 8:
            if ( pm_ == producer_mode_t::black )
                os_ << "color:black;";
            else if ( pm_ == producer_mode_t::pink )
                os_ << "color:pink;";
            else
                os_ << "color:white;";
            break;  // background color
        case 9:
            if ( pm_ != producer_mode_t::black )
                os_ << "color:black;";
            else
                os_ << "color:white;";
            break;  // foreground Color
        }
    }
    void background_color( int clr ) {
        if ( pm_ == producer_mode_t::strip )
            return;
        switch ( clr ) {
        case 0:
            if ( black_background_is_transparent_ )
                os_ << "background-color:transparent;";
            else
                os_ << "background-color:black;";
            break;  // black
        case 1:
            os_ << "background-color:red;";
            break;  // red
        case 2:
            if ( pm_ != producer_mode_t::black )
                os_ << "background-color:green;";
            else
                os_ << "background-color:lime;";
            break;  // green
        case 3:
            if ( pm_ != producer_mode_t::black )
                os_ << "background-color:olive;";
            else
                os_ << "background-color:yellow;";
            break;  // yellow
        case 4:
            if ( pm_ != producer_mode_t::black )
                os_ << "background-color:blue;";
            else
                os_ << "background-color:#3333FF;";
            break;  // blue
        case 5:
            if ( pm_ != producer_mode_t::black )
                os_ << "background-color:purple;";
            else
                os_ << "background-color:fuchsia;";
            break;  // purple
        case 6:
            if ( pm_ != producer_mode_t::black )
                os_ << "background-color:teal;";
            else
                os_ << "background-color:aqua;";
            break;  // cyan
        case 7:
            if ( pm_ != producer_mode_t::black )
                os_ << "background-color:gray;";
            else
                os_ << "background-color:white;";
            break;  // white
        case 8:
            if ( pm_ == producer_mode_t::black ) {
                if ( black_background_is_transparent_ )
                    os_ << "background-color:transparent;";
                else
                    os_ << "background-color:black;";
            } else if ( pm_ == producer_mode_t::pink )
                os_ << "background-color:pink;";
            else
                os_ << "background-color:white;";
            break;  // background color
        case 9:
            if ( pm_ != producer_mode_t::black ) {
                if ( black_background_is_transparent_ )
                    os_ << "background-color:transparent;";
                else
                    os_ << "background-color:black;";
            } else
                os_ << "background-color:white;";
            break;  // foreground color
        }
    }
    void underline() {
        if ( pm_ == producer_mode_t::strip )
            return;
        os_ << "text-decoration:underline;";
    }
    void bold() {
        if ( pm_ == producer_mode_t::strip )
            return;
        os_ << "font-weight:bold;";
    }
    void blink() {
        if ( pm_ == producer_mode_t::strip )
            return;
        os_ << "text-decoration:blink;";
    }
};  /// class producer
void ansi_transform( producer& p, std::istream& is ) {
    char line_break = 0;
    // char word_wrap = 0;
    //
    //
    //
    int fc = -1;                  // standard Foreground Color //IRC-Color+8
    int bc = -1;                  // standard Background Color //IRC-Color+8
    int ul = 0;                   // not underlined
    int bo = 0;                   // not bold
    int bl = 0;                   // bo blinking
    int negative = 0;             // no negative image
    int special_char = 0;         // no special characters
    int ofc, obc, oul, obo, obl;  // old values
    int line = 0;
    int momline = 0;
    int newline = -1;
    int temp;
    while ( !is.eof() ) {
        char c = 0;
        is.read( &c, 1 );
        if ( c == '\033' ) {
            // saving old values
            ofc = fc;
            obc = bc;
            oul = ul;
            obo = bo;
            obl = bl;
            // searching the end (a letter) and safe the insert:
            c = getNextChar( is );
            if ( c == '[' ) {  // CSI code, see
                               // https://en.wikipedia.org/wiki/ANSI_escape_code#Colors
                char buf[1024];
                buf[0] = '[';
                int counter = 1;
                while ( ( c < 'A' ) || ( ( c > 'Z' ) && ( c < 'a' ) ) || ( c > 'z' ) ) {
                    c = getNextChar( is );
                    buf[counter] = c;
                    if ( c == '>' )  // end of htop
                        break;
                    counter++;
                    if ( counter > 1022 )
                        break;
                }
                buf[counter - 1] = 0;
                switch ( c ) {
                case 'm': {
                    // fprintf( stderr, "\n%s\n", buf ); // DEBUG
                    pelem elem = parseInsert( buf );
                    pelem momelem = elem;
                    while ( momelem != nullptr ) {
                        // jump over zeros
                        int mompos = 0;
                        while ( mompos < momelem->digitcount && momelem->digit[mompos] == 0 )
                            mompos++;
                        if ( mompos == momelem->digitcount ) {  // only zeros => delete all
                            bo = 0;
                            ul = 0;
                            bl = 0;
                            fc = -1;
                            bc = -1;
                            negative = 0;
                            special_char = 0;
                            c = 0;  // l_sergiy: added special case to cancel char output on reset
                        } else {
                            int a_digit = momelem->digit[mompos];
                            switch ( a_digit ) {
                            case 1:
                                if ( mompos + 1 == momelem->digitcount )  // 1, 1X not supported
                                    bo = 1;
                                break;
                            case 2:
                                if ( mompos + 1 < momelem->digitcount )  // 2X, 2 not supported
                                    switch ( momelem->digit[mompos + 1] ) {
                                    case 1:  // reset and double underline (which aha doesn't
                                             // support)
                                    case 2:
                                        bo = 0;
                                        break;  // reset bold
                                    case 4:
                                        ul = 0;
                                        break;  // reset underline
                                    case 5:
                                        bl = 0;
                                        break;  // reset blink
                                    case 7:     // reset Inverted
                                        if ( bc == -1 )
                                            bc = 8;
                                        if ( fc == -1 )
                                            fc = 9;
                                        temp = bc;
                                        bc = fc;
                                        fc = temp;
                                        negative = 0;
                                        break;
                                    }
                                break;
                            case 3:
                                if ( mompos + 1 < momelem->digitcount ) {  // 3X, 3 not supported
                                    if ( negative == 0 )
                                        fc = momelem->digit[mompos + 1];
                                    else
                                        bc = momelem->digit[mompos + 1];
                                }
                                break;
                            case 4:
                                if ( mompos + 1 == momelem->digitcount )  // 4
                                    ul = 1;
                                else {  // 4X
                                    if ( negative == 0 )
                                        bc = momelem->digit[mompos + 1];
                                    else
                                        fc = momelem->digit[mompos + 1];
                                }
                                break;
                            case 5:
                                if ( mompos + 1 == momelem->digitcount )  // 5, 5X not supported
                                    bl = 1;
                                break;
                            // 6 and 6X not supported at all
                            case 7:
                                if ( bc == -1 )  // 7, 7X is mot defined (and supported)
                                    bc = 8;
                                if ( fc == -1 )
                                    fc = 9;
                                temp = bc;
                                bc = fc;
                                fc = temp;
                                negative = 1 - negative;
                                break;
                                // 8 and 9 not supported
                                //									case 8:
                                //									case 9:
                                //										// experimental
                                //										if( negative == 0 )
                                //											bc = momelem->digit[
                                // mompos
                                //+
                                // 1
                                //]; 										else
                                // fc = momelem->digit[ mompos + 1 ]; break;
                            }  // switch( a_digit )
                        }
                        momelem = momelem->next;
                    }
                    deleteParse( elem );
                } break;
                case 'H':
                    if ( p.htop_fix_ ) {  // a little dirty ...
                        pelem elem = parseInsert( buf );
                        pelem second = elem->next;
                        if ( second == nullptr )
                            second = elem;
                        newline = second->digit[0] - 1;
                        if ( second->digitcount > 1 )
                            newline = ( newline + 1 ) * 10 + second->digit[1] - 1;
                        if ( second->digitcount > 2 )
                            newline = ( newline + 1 ) * 10 + second->digit[2] - 1;
                        deleteParse( elem );
                        if ( newline < line )
                            line_break = 1;
                    }
                    break;
                }
                if ( p.htop_fix_ )
                    if ( line_break ) {
                        for ( ; line < 80; line++ )
                            p.space();
                    }
                // checking the differences
                if ( ( fc != ofc ) || ( bc != obc ) || ( ul != oul ) || ( bo != obo ) ||
                     ( bl != obl ) ) {  // ANY Change
                    if ( ( ofc != -1 ) || ( obc != -1 ) || ( oul != 0 ) || ( obo != 0 ) ||
                         ( obl != 0 ) )
                        p.span_close();
                    if ( ( fc != -1 ) || ( bc != -1 ) || ( ul != 0 ) || ( bo != 0 ) ||
                         ( bl != 0 ) ) {
                        p.span_open();
                        p.foreground_color( fc );
                        p.background_color( bc );
                        if ( ul )
                            p.underline();
                        if ( bo )
                            p.bold();
                        if ( bl )
                            p.blink();
                        p.tag_ending();
                    }
                }
            } else if ( c == ']' ) {        // operating System Command (OSC), ignoring for now
                while ( c != 2 && c != 7 )  // STX and BEL end an OSC.
                    c = getNextChar( is );
            } else if ( c == '(' ) {  // Some VT100 ESC sequences, which should be ignored
                // reading (and ignoring!) one character should work for "(B" (US ASCII character
                // set), "(A" (UK ASCII character set) and "(0" (Graphic) this whole "standard" is
                // f...cked up. Really...
                c = getNextChar( is );
                if ( c == '0' )  // we do not ignore ESC(0 ;)
                    special_char = 1;
                else
                    special_char = 0;
            }
        } else if ( c == 13 && p.htop_fix_ ) {
            for ( ; line < 80; line++ )
                p.space();
            line = 0;
            momline++;
            p.char13();
        } else if ( c != 8 ) {
            line++;
            if ( line_break ) {
                p.char13();
                line = 0;
                line_break = 0;
                momline++;
            }
            if ( newline >= 0 ) {
                while ( newline > line ) {
                    p.space();
                    line++;
                }
                newline = -1;
            }
            switch ( c ) {
            case 0:
                break;  // l_sergiy: added special case to cancel char output on reset
            case '&':
                p.char_amp();
                break;
            case '\"':
                p.char_double_quote();
                break;
            case '<':
                p.char_less_than();
                break;
            case '>':
                p.char_greater_than();
                break;
            case '\n':
            case 13:
                momline++;
                line = 0;
                [[fallthrough]];
            default:
                if ( special_char )
                    p.char_special( c );
                else
                    p.char_normal( c );
            }
            if ( p.iso_ > 0 )                // only at ISOS
                if ( ( c & 128 ) == 128 ) {  // first bit set => there must be followbytes
                    int bits = 2;
                    if ( ( c & 32 ) == 32 )
                        bits++;
                    if ( ( c & 16 ) == 16 )
                        bits++;
                    for ( int meow = 1; meow < bits; meow++ )
                        p.char_normal( getNextChar( is ) );
                }
        }
    }  // while( ! is.eof() )
    if ( ( fc != -1 ) || ( bc != -1 ) || ( ul != 0 ) || ( bo != 0 ) || ( bl != 0 ) )
        p.span_close();
}
};  // namespace ansi_conv

std::string generate_html( const char* s ) {
    if ( s == nullptr || s[0] == '\0' )
        return "";
    std::string s2( s );
    return generate_html( s2 );
}
std::string generate_html( const std::string& s ) {
    std::ostringstream os;
    // std::istringstream is( s );
    std::stringstream is( s, std::ios::in | std::ios::binary );
    ansi_conv::producer p( ansi_conv::producer::producer_mode_t::normal,  // normal, black, pink
        os );
    ansi_conv::ansi_transform( p, is );
    return os.str();
}

//	// https://rosettacode.org/wiki/Strip_control_codes_and_extended_characters_from_a_string#C
//	#define IS_CTRL  (1 << 0)
//	#define IS_EXT	 (1 << 1)
//	#define IS_ALPHA (1 << 2)
//	#define IS_DIGIT (1 << 3) // not used, just give you an idea
//	static unsigned int g_stat_strip_char_table[256] = {0};
//	static void stat_init_strip_table() { // could use ctypes, but then they pretty much do the same
// thing 		static bool g_bInitialized = false; 		if( g_bInitialized ) 			return;
// g_bInitialized
// = true; 		int i; 		for( i = 0; i < 32; i++ ) 			g_stat_strip_char_table[i] |=
// IS_CTRL; 		g_stat_strip_char_table[127]
//|= IS_CTRL; 		for( i = 'A'; i <= 'Z'; i++ ) { 			g_stat_strip_char_table[i] |=
// IS_ALPHA; 			g_stat_strip_char_table[i + 0x20] |= IS_ALPHA; /* lower case */
//		}
//		for( i = 128; i < 256; i++ )
//			g_stat_strip_char_table[i] |= IS_EXT;
//	}
//	static void stat_impl_strip( char * str, int what ) { // depends on what "stripped" means; we do
// it in place. "what" is a combination of the IS_* macros, meaning strip if a char IS_ any of them
//		stat_init_strip_table();
//		unsigned char * ptr, * s = (unsigned char*)str;
//		ptr = s;
//		while( *s != '\0' ) {
//			if( ( g_stat_strip_char_table[(int)*s] & what ) == 0 )
//				*(ptr++) = *s;
//			s++;
//		}
//		*ptr = '\0';
//	}
std::string strip( const char* s ) {
    if ( s == nullptr || s[0] == '\0' )
        return "";
    //
    //
    //		std::string tmp = s;
    //		stat_impl_strip( (char*)tmp.c_str(), IS_CTRL | IS_EXT );
    //		std::string ret = tmp.c_str();
    //
    //
    //		std::string ret;
    //		char cp = '\0';
    //		char cpp = '\0';
    //		char cppp = '\0';
    //		char cpppp = '\0';
    //		char cppppp = '\0';
    //		char cpppppp = '\0';
    //		char cppppppp = '\0';
    //		char cpppppppp = '\0';
    //		char cppppppppp = '\0';
    //		for( ; *s; ++ s ) {
    //			char c = (*s);
    //			if( c == '\033' ) {
    //			} else if( c == '[' && cp == '\033' ) {
    //			} else if( isdigit( c ) && cp == '[' && cpp == '\033' ) {
    //			} else if( isdigit( c ) && isdigit( cp ) && cpp == '[' && cppp == '\033' ) {
    //			} else if( c == 'm' && isdigit( cp ) && isdigit( cpp ) && cppp == '[' && cpppp ==
    //'\033'
    //) { 			} else if( c == 0x1B ) { 			} else if( c == '[' && cp == 0x1B ) {
    //} else if( cp == '[' && cpp
    //== 0x1B ) { 			} else if( c=='m' && cpp == '[' && cppp == 0x1B ) { 			} else
    // if(
    // c
    //==
    //';' && cpp ==
    //'[' && cppp == 0x1B ) { 			} else if( cp == ';' && cppp == '[' && cpppp == 0x1B ) {
    //} else if(
    // cpp == ';' && cpppp == '[' && cppppp == 0x1B ) { 			} else if( cppp == ';' && cppppp
    // ==
    // '['
    // &&
    // cpppppp == 0x1B ) { 			} else if( cpppp == ';' && cpppppp == '[' && cppppppp == 0x1B )
    // { } else if( cppppp == ';' && cppppppp == '[' && cpppppppp == 0x1B ) { 			} else if(
    // c =='m'
    // && cpppppp ==
    //';' && cpppppppp == '[' && cppppppppp == 0x1B ) { 			} else //if( isprint( c ) )
    // ret += c; 			cppppppppp = cpppppppp; 			cpppppppp = cppppppp;
    // cppppppp = cpppppp; cpppppp = cppppp; 			cppppp = cpppp; 			cpppp = cppp;
    // cppp = cpp; 			cpp = cp; 			cp = c;
    //		}
    //		return ret;
    //
    //
    std::ostringstream os;
    std::istringstream is( s );
    ansi_conv::producer p( ansi_conv::producer::producer_mode_t::strip, os );
    ansi_conv::ansi_transform( p, is );
    return os.str();
}
///	::sprintf( strBuffer, "%c[%d;%d;%dm", 0x1B, int(a), int(f) + 30, int(b) + 40 );
std::string strip( const std::string& s ) {
    return strip( s.c_str() );
}
};  // namespace cc
