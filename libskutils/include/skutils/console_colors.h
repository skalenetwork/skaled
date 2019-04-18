#ifndef CONSOLECOLORS_H
#define CONSOLECOLORS_H

#include <chrono>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
//#include <type_traits>
#include <stdint.h>
#include <time.h>

//#include <nlohmann/json.hpp>
#include <json.hpp>

#include <skutils/url.h>

namespace cc {
enum class e_reset_mode_t {
    __ERM_COLORS,
    __ERM_ZERO,
    __ERM_EMULATION,
    __ERM_WIDE,
    __ERM_FULL
};  /// enum class e_reset_mode_t
extern e_reset_mode_t g_eResetMode;
e_reset_mode_t str_2_reset_mode( const std::string& s );
e_reset_mode_t str_2_reset_mode( const char* s );
std::string reset_mode_2_str( e_reset_mode_t eResetMode );
extern size_t g_nJsonStringValueOutputSizeLimit;
namespace control {
namespace attribute {
extern const char _console_[];
extern const char _underline_[];
extern const char _bold_[];
};  // namespace attribute
namespace foreground {
extern const char _black_[];
extern const char _blue_[];
extern const char _red_[];
extern const char _magenta_[];
extern const char _green_[];
extern const char _cyan_[];
extern const char _yellow_[];
extern const char _white_[];
extern const char _console_[];
extern const char _light_black_[];
extern const char _light_blue_[];
extern const char _light_red_[];
extern const char _light_magenta_[];
extern const char _light_green_[];
extern const char _light_cyan_[];
extern const char _light_yellow_[];
extern const char _light_white_[];
};  // namespace foreground
namespace background {
extern const char _black_[];
extern const char _blue_[];
extern const char _red_[];
extern const char _magenta_[];
extern const char _green_[];
extern const char _cyan_[];
extern const char _yellow_[];
extern const char _white_[];
extern const char _console_[];
extern const char _light_black_[];
extern const char _light_blue_[];
extern const char _light_red_[];
extern const char _light_magenta_[];
extern const char _light_green_[];
extern const char _light_cyan_[];
extern const char _light_yellow_[];
extern const char _light_white_[];
};  // namespace background
};  // namespace control

enum class color {
    _black_ = 0,
    _red_,
    _green_,
    _yellow_,
    _blue_,
    _magenta_,
    _cyan_,
    _white_,
    _default_ = 9
};  /// enum color
enum class attribute {
    _reset_ = 0,
    _bright_,
    _dim_,
    _underline_,
    _blink_,
    _reverse_,
    _hidden_
};  /// enum attribute

extern volatile bool _on_;
extern volatile int _default_json_indent_;

extern std::string tune( attribute a, color f, color b );
static inline std::ostream& tune( std::ostream& os, attribute a, color f, color b ) {
    return os << tune( a, f, b );
}

extern const char* reset();
static inline std::string reset( const std::string& s ) {
    return reset() + s + reset();
}
static inline std::ostream& reset( std::ostream& os ) {
    return os << reset();
}

extern const char* normal();
static inline std::string normal( const std::string& s ) {
    return normal() + s + reset();
}
static inline std::ostream& normal( std::ostream& os ) {
    return os << normal();
}

extern const char* trace();
static inline std::string trace( const std::string& s ) {
    return trace() + s + reset();
}
static inline std::ostream& trace( std::ostream& os ) {
    return os << trace();
}

extern const char* debug();
static inline std::string debug( const std::string& s ) {
    return debug() + s + reset();
}
static inline std::ostream& debug( std::ostream& os ) {
    return os << debug();
}

extern const char* deep_debug();
static inline std::string deep_debug( const std::string& s ) {
    return deep_debug() + s + reset();
}
static inline std::ostream& deep_debug( std::ostream& os ) {
    return os << deep_debug();
}

extern const char* info();
static inline std::string info( const std::string& s ) {
    return info() + s + reset();
}
static inline std::ostream& info( std::ostream& os ) {
    return os << info();
}

extern const char* deep_info();
static inline std::string deep_info( const std::string& s ) {
    return deep_info() + s + reset();
}
static inline std::ostream& deep_info( std::ostream& os ) {
    return os << deep_info();
}

extern const char* note();
static inline std::string note( const std::string& s ) {
    return note() + s + reset();
}
static inline std::ostream& note( std::ostream& os ) {
    return os << note();
}

extern const char* deep_note();
static inline std::string deep_note( const std::string& s ) {
    return deep_note() + s + reset();
}
static inline std::ostream& deep_note( std::ostream& os ) {
    return os << deep_note();
}

extern const char* notice();
static inline std::string notice( const std::string& s ) {
    return notice() + s + reset();
}
static inline std::ostream& notice( std::ostream& os ) {
    return os << notice();
}

extern const char* deep_notice();
static inline std::string deep_notice( const std::string& s ) {
    return deep_notice() + s + reset();
}
static inline std::ostream& deep_notice( std::ostream& os ) {
    return os << deep_notice();
}

extern const char* attention();
static inline std::string attention( const std::string& s ) {
    return attention() + s + reset();
}
static inline std::ostream& attention( std::ostream& os ) {
    return os << attention();
}

extern const char* bright();
static inline std::string bright( const std::string& s ) {
    return bright() + s + reset();
}
static inline std::ostream& bright( std::ostream& os ) {
    return os << bright();
}

extern const char* sunny();
static inline std::string sunny( const std::string& s ) {
    return sunny() + s + reset();
}
static inline std::ostream& sunny( std::ostream& os ) {
    return os << sunny();
}

extern const char* success();
static inline std::string success( const std::string& s ) {
    return success() + s + reset();
}
static inline std::ostream& success( std::ostream& os ) {
    return os << success();
}

extern const char* found();
static inline std::string found( const std::string& s ) {
    return found() + s + reset();
}
static inline std::ostream& found( std::ostream& os ) {
    return os << found();
}

extern const char* fail();
static inline std::string fail( const std::string& s ) {
    return fail() + s + reset();
}
static inline std::ostream& fail( std::ostream& os ) {
    return os << fail();
}

extern const char* warn();
static inline std::string warn( const std::string& s ) {
    return warn() + s + reset();
}
static inline std::ostream& warn( std::ostream& os ) {
    return os << warn();
}

extern const char* deep_warn();
static inline std::string deep_warn( const std::string& s ) {
    return deep_warn() + s + reset();
}
static inline std::ostream& deep_warn( std::ostream& os ) {
    return os << deep_warn();
}

extern const char* error();
static inline std::string error( const std::string& s ) {
    return error() + s + reset();
}
static inline std::ostream& error( std::ostream& os ) {
    return os << error();
}

extern const char* deep_error();
static inline std::string deep_error( const std::string& s ) {
    return deep_error() + s + reset();
}
static inline std::ostream& deep_error( std::ostream& os ) {
    return os << deep_error();
}

extern const char* fatal();
static inline std::string fatal( const std::string& s ) {
    return fatal() + s + reset();
}
static inline std::ostream& fatal( std::ostream& os ) {
    return os << fatal();
}

extern const char* ws_rx_inv();
static inline std::string ws_rx_inv( const std::string& s ) {
    return ws_rx_inv() + s + reset();
}
static inline std::ostream& ws_rx_inv( std::ostream& os ) {
    return os << ws_rx_inv();
}

extern const char* ws_tx_inv();
static inline std::string ws_tx_inv( const std::string& s ) {
    return ws_tx_inv() + s + reset();
}
static inline std::ostream& ws_tx_inv( std::ostream& os ) {
    return os << ws_tx_inv();
}

extern const char* ws_rx();
static inline std::string ws_rx( const std::string& s ) {
    return ws_rx() + s + reset();
}
static inline std::ostream& ws_rx( std::ostream& os ) {
    return os << ws_rx();
}

extern const char* ws_tx();
static inline std::string ws_tx( const std::string& s ) {
    return ws_tx() + s + reset();
}
static inline std::ostream& ws_tx( std::ostream& os ) {
    return os << ws_tx();
}

extern const char* path_slash();  // '/'
extern const char* path_part();

extern const char* url_prefix_punct();  // ':', etc.
extern const char* url_prefix_slash();  // '/'
extern const char* url_prefix_scheme();
extern const char* url_prefix_user_name();
extern const char* url_prefix_at();  // '@'
extern const char* url_prefix_user_pass();
extern const char* url_prefix_host();
extern const char* url_prefix_number();  // explict IP version as number
extern const char* url_prefix_port();
extern const char* url_prefix_path();
extern const char* url_prefix_qq();    // query '?'
extern const char* url_prefix_qk();    // query key
extern const char* url_prefix_qe();    // query '='
extern const char* url_prefix_qv();    // query value
extern const char* url_prefix_qa();    // query '&'
extern const char* url_prefix_hash();  // '#'
extern const char* url_prefix_fragment();

extern const char* json_prefix( nlohmann::json::value_t vt );
extern const char* json_prefix_brace_square();
extern const char* json_prefix_brace_curved();
extern const char* json_prefix_double_colon();
extern const char* json_prefix_comma();
extern const char* json_prefix_member_name();
extern std::size_t json_extra_space( const std::string& s ) noexcept;
extern std::string json_escape_string( const std::string& s ) noexcept;

extern std::string json( const nlohmann::json& jo, const int indent = -1 );
extern std::string json( const std::string& s );
extern std::string json( const char* s );

extern std::string flag( bool b );     // true/false
extern std::string flag_ed( bool b );  // enabled/disabled
extern std::string flag_yn( bool b );  // yes/no

extern std::string chr( char c );
extern std::string chr( unsigned char c );

extern std::string str( const std::string& s );
extern std::string str( const char* s );

extern std::string num10( int8_t n );
extern std::string num10( uint8_t n );
extern std::string num10( int16_t n );
extern std::string num10( uint16_t n );
extern std::string num10( int32_t n );
extern std::string num10( uint32_t n );
extern std::string num10( int64_t n );
extern std::string num10( uint64_t n );
extern std::string size10( size_t n );

extern std::string num16( int8_t n );
extern std::string num16( uint8_t n );
extern std::string num16( int16_t n );
extern std::string num16( uint16_t n );
extern std::string num16( int32_t n );
extern std::string num16( uint32_t n );
extern std::string num16( int64_t n );
extern std::string num16( uint64_t n );
extern std::string size16( size_t n );

extern std::string num016( int8_t n );
extern std::string num016( uint8_t n );
extern std::string num016( int16_t n );
extern std::string num016( uint16_t n );
extern std::string num016( int32_t n );
extern std::string num016( uint32_t n );
extern std::string num016( int64_t n );
extern std::string num016( uint64_t n );
extern std::string size016( size_t n );

extern std::string num0x( int8_t n );
extern std::string num0x( uint8_t n );
extern std::string num0x( int16_t n );
extern std::string num0x( uint16_t n );
extern std::string num0x( int32_t n );
extern std::string num0x( uint32_t n );
extern std::string num0x( int64_t n );
extern std::string num0x( uint64_t n );
extern std::string size0x( size_t n );

extern std::string num10eq0x( int8_t n );
extern std::string num10eq0x( uint8_t n );
extern std::string num10eq0x( int16_t n );
extern std::string num10eq0x( uint16_t n );
extern std::string num10eq0x( int32_t n );
extern std::string num10eq0x( uint32_t n );
extern std::string num10eq0x( int64_t n );
extern std::string num10eq0x( uint64_t n );
extern std::string size10eq0x( size_t n );

extern std::string numScientific( const float& n );
extern std::string numScientific( const double& n );
extern std::string numDefault( const float& n );
extern std::string numDefault( const double& n );

extern std::string num( int8_t n );
extern std::string num( uint8_t n );
extern std::string num( int16_t n );
extern std::string num( uint16_t n );
extern std::string num( int32_t n );
extern std::string num( uint32_t n );
extern std::string num( int64_t n );
extern std::string num( uint64_t n );
extern std::string num( const float& n );
extern std::string num( const double& n );
extern std::string size( size_t n );

extern const char* tsep();
extern const char* day_count();
extern const char* dpart();
extern const char* tpart();
extern const char* tfrac();
extern bool string2time( const char* s, std::tm& aTm, uint64_t& nMicroSeconds );
extern bool string2time(
    const std::string& timeStr, std::chrono::high_resolution_clock::time_point& ptTime );
extern bool string2duration( const std::string& s, std::chrono::duration< uint64_t >& outDuration,
    const std::chrono::hours& hours = std::chrono::hours::zero(),
    const std::chrono::minutes& minutes = std::chrono::minutes::zero(),
    const std::chrono::seconds& seconds = std::chrono::seconds::zero() );
extern std::string duration2string( std::chrono::nanoseconds time );
extern std::string time2string(
    std::time_t tt, uint64_t nMicroSeconds, bool isUTC = false, bool isColored = true );
extern std::string time2string( const std::tm& aTm, uint64_t nMicroSeconds, bool isColored = true );
extern std::string time2string( const std::chrono::high_resolution_clock::time_point& ptTime,
    bool isUTC = false, bool isDaysInsteadOfYMD = false, bool isColored = true );
extern std::string now2string(
    bool isUTC = false, bool isDaysInsteadOfYMD = false, bool isColored = true );
extern std::string jsNow2string( bool isUTC = true );

inline std::string c() {
    return reset();
}
inline std::string c( bool x ) {
    return flag( x );
}  // true/false
inline std::string c( const std::string& x ) {
    return x;
}  // { return str( x ); }
inline std::string c( const char* x ) {
    return x;
}  // { return str( x ); }
//  inline std::string c( unsigned char          x ) { char s[2] = { char(x), '\0' }; return s; } //
//  { return chr( x ); }
inline std::string c( char x ) {
    char s[2] = {char( x ), '\0'};
    return s;
}  // { return chr( x ); }
   //  inline std::string c( int8_t                 x ) { return num( x ); }
inline std::string c( uint8_t x ) {
    return num( x );
}
inline std::string c( int16_t x ) {
    return num( x );
}
inline std::string c( uint16_t x ) {
    return num( x );
}
inline std::string c( int32_t x ) {
    return num( x );
}
inline std::string c( uint32_t x ) {
    return num( x );
}
inline std::string c( int64_t x ) {
    return num( x );
}
inline std::string c( uint64_t x ) {
    return num( x );
}
inline std::string c( const float& x ) {
    return num( x );
}
inline std::string c( const double& x ) {
    return num( x );
}
inline std::string c( const nlohmann::json& x, const int indent = -1 ) {
    return json( x, ( indent >= 0 ) ? indent : _default_json_indent_ );
}
inline std::string c( const std::chrono::high_resolution_clock::time_point& x, bool isUTC = false,
    bool isDaysInsteadOfYMD = false ) {
    return time2string( x, isUTC, isDaysInsteadOfYMD );
}
extern volatile bool g_bEnableJsonColorization;
extern std::string j( const nlohmann::json& x );
extern std::string j( const std::string& strProbablyJson );
extern std::string j( const char* strProbablyJson );
extern bool u( std::string& s, const skutils::url& url, bool bThrow, bool bInterrupt );
extern bool u( std::string& s, const std::string& strProbablyURL, bool bThrow, bool bInterrupt );
extern std::string u( const skutils::url& url );
extern std::string u( const std::string& strProbablyURL );
extern std::string u( const char* strProbablyURL );
extern bool p( std::string& s, const std::string& strProbablyPath, bool bThrow );
extern std::string p( const std::string& strProbablyPath );
extern std::string p( const char* strProbablyPath );
extern std::string pe( const std::string& strPath );  // path with comment about file existance and
                                                      // size
extern std::string pe( const char* strPath );  // path with comment about file existance and size
extern bool eml( std::string& s, const std::string& strProbablyeMail, bool bThrow );
extern std::string eml( const std::string& strProbablyeMail );
extern std::string eml( const char* strProbablyeMail );
inline std::string ed( bool x ) {
    return flag_ed( x );
}  // enabled/disabled
inline std::string yn( bool x ) {
    return flag_yn( x );
}  // yes/no

extern std::string not_computed_yet_str();  // "not computed yet"
extern std::string empty_str();             // "empty"
extern std::string null_str();              // "null"

extern std::string binary_singleline( const void* pBinary, size_t cnt, size_t cntAlign = 16,
    char chrEmptyPosition = ' ', const char* strSeparator = nullptr );
extern std::string binary_singleline( const std::string& strBinaryBuffer, size_t cntAlign = 16,
    char chrEmptyPosition = ' ', const char* strSeparator = nullptr );
extern std::string binary_singleline(
    const void* pBinary, size_t cnt, const char* strSeparator = nullptr );
extern std::string binary_singleline(
    const std::string& strBinaryBuffer, const char* strSeparator = nullptr );

extern std::string binary_singleline_ascii( const void* pBinary, size_t cnt, size_t cntAlign = 16,
    char chrNonPrintable = '?', char chrEmptyPosition = ' ', const char* strSeparator = nullptr );
extern std::string binary_singleline_ascii( const std::string& strBinaryBuffer,
    size_t cntAlign = 16, char chrNonPrintable = '?', char chrEmptyPosition = ' ',
    const char* strSeparator = nullptr );

extern std::string binary_table( const void* pBinary, size_t cnt, size_t cntPerRow = 16,
    bool isAlsoGenerateIndexNumbersPrefix = true, bool isAlsoGenerateAsciiSuffix = true,
    char chrNonPrintable = '?', char chrEmptyPosition = ' ', const char* strSeparatorHex = " ",
    const char* strSeparatorAscii = nullptr, const char* strSeparatorBetweenTables = "|",
    const char* strRowPrefix = nullptr, const char* strRowSuffix = "\n" );
extern std::string binary_table( const std::string& strBinaryBuffer, size_t cntPerRow = 16,
    bool isAlsoGenerateIndexNumbersPrefix = true, bool isAlsoGenerateAsciiSuffix = true,
    char chrNonPrintable = '?', char chrEmptyPosition = ' ', const char* strSeparatorHex = " ",
    const char* strSeparatorAscii = nullptr, const char* strSeparatorBetweenTables = "|",
    const char* strRowPrefix = nullptr, const char* strRowSuffix = "\n" );

extern std::string rpc_message( const nlohmann::json& x );
extern std::string rpc_message( const std::string& msg, bool isBinaryMessage = false );

enum class e_rpc_log_type_t { erpcltt_ws_tx, erpcltt_ws_rx };  /// enum class e_rpc_log_type_t
extern std::string rpc_rpc_log_type_2_str( e_rpc_log_type_t erpcltt );
extern std::string rpc_rpc_log_type_2_direction_arrows_str( e_rpc_log_type_t erpcltt );
extern std::string rpc_rpc_log_colorize_prefix( e_rpc_log_type_t erpcltt, const std::string s );
extern std::string rpc_rpc_log_colorize_suffix( e_rpc_log_type_t erpcltt, const std::string s );

extern std::string rpc_log( e_rpc_log_type_t erpcltt,
    const char* strConnectionType,         // can be nullptr
    const char* strConnectionDescription,  // can be nullptr
    const std::string& msg, bool isBinaryMessage );
extern std::string rpc_log( e_rpc_log_type_t erpcltt,
    const std::string& strConnectionType,         // can be empty
    const std::string& strConnectionDescription,  // can be empty
    const std::string& msg, bool isBinaryMessage );
extern std::string rpc_log(
    e_rpc_log_type_t erpcltt, const std::string& msg, bool isBinaryMessage );
extern std::string rpc_log( e_rpc_log_type_t erpcltt,
    const char* strConnectionType,         // can be nullptr
    const char* strConnectionDescription,  // can be nullptr
    const nlohmann::json& x );
extern std::string rpc_log( e_rpc_log_type_t erpcltt,
    const std::string& strConnectionType,         // can be empty
    const std::string& strConnectionDescription,  // can be empty
    const nlohmann::json& x );
extern std::string rpc_log( e_rpc_log_type_t erpcltt, const nlohmann::json& x );

extern std::string generate_html( const char* s );
extern std::string generate_html( const std::string& s );
extern std::string strip( const char* s );
extern std::string strip( const std::string& s );

template < typename stream_t >
class streamer {
public:
    stream_t& os_;
    bool isAutoFlush_ : 1;
    streamer( stream_t& os, bool isAutoFlush = true ) : os_( os ), isAutoFlush_( isAutoFlush ) {}
    template < typename to_be_written_t >
    streamer& operator<<( const to_be_written_t& x ) {
        os_ << c( x );
        if ( isAutoFlush_ )
            os_.flush();
        return ( *this );
    }
};  /// class out

class writer {
public:
    typedef std::function< void( const std::string& ) > fn_write_t;
    typedef std::function< void() > fn_flush_t;
    fn_write_t fnWrite_;
    fn_flush_t fnFlush_;
    bool isAutoFlush_ : 1;
    writer( bool isAutoFlush = true ) : isAutoFlush_( isAutoFlush ) {}
    writer( fn_write_t fnWrite, fn_flush_t fnFlush, bool isAutoFlush = true )
        : fnWrite_( fnWrite ), fnFlush_( fnFlush ), isAutoFlush_( isAutoFlush ) {}
    template < typename to_be_written_t >
    writer& operator<<( const to_be_written_t& x ) {
        if ( !fnWrite_ )
            return ( *this );
        std::stringstream ss;
        ss << c( x );
        std::string s = ss.str();
        fnWrite_( s );
        if ( isAutoFlush_ && fnFlush_ )
            fnFlush_();
        return ( *this );
    }
};  /// class writer

class accumulator : public writer {
    std::string s_;

public:
    accumulator() : writer( false ) {
        fnWrite_ = [&]( const std::string& s ) { s_ += s; };
    }
    const std::string& str() const { return s_; }
};  /// class accumulator

};  // namespace cc

#endif  // CONSOLECOLORS_H
