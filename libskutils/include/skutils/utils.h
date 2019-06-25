#if ( !defined __SKUTILS_UTILS_H )
#define __SKUTILS_UTILS_H 1

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

//#include <nlohmann/json.hpp>
#include <json.hpp>

namespace skutils {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::string string_type_t;
typedef std::set< string_type_t > string_set_t;
typedef std::list< string_type_t > string_list_t;
typedef std::vector< string_type_t > string_vector_t;

namespace tools {

//        std::string format         ( const char * format, ... );
//        std::string format_no_throw( const char * format, ... );
//        std::string format         ( const char * format, va_list args );
//        std::string format_no_throw( const char * format, va_list args );

template < class T >
bool equal( T d1, T d2, typename std::enable_if< std::is_floating_point< T >::value >::type* = 0 ) {
    static constexpr double eps = std::numeric_limits< double >::epsilon();
    return ( std::abs( d1 - d2 ) <= eps );
}
template < typename... Args >
inline std::string format( const char* format, Args... args ) {
    char* null_str = nullptr;
    size_t size = std::snprintf( null_str, 0, format, args... ) + 1;  // extra space for '\0'
    std::unique_ptr< char[] > buf( new char[size] );
    std::snprintf( buf.get(), size, format, args... );
    return std::string( buf.get(), buf.get() + size - 1 );  // we don't want the '\0' inside
}
template < typename... Args >
inline std::string format_no_throw( const char* format, Args... args ) {
    try {
        char* null_str = nullptr;
        size_t size = std::snprintf( null_str, 0, format, args... ) + 1;  // extra space for '\0'
        std::unique_ptr< char[] > buf( new char[size] );
        std::snprintf( buf.get(), size, format, args... );
        return std::string( buf.get(), buf.get() + size - 1 );  // we don't want the '\0' inside
    } catch ( ... ) {
        return "";
    }
}

inline bool default_trim_what( unsigned char ch ) {
    return std::isspace( ch ) || ch == '\r' || ch == '\n' || ch == '\t';
}
inline void ltrim( std::string& s,
    std::function< bool( unsigned char ) > f = default_trim_what ) {  // trim from start (in place)
    s.erase( s.begin(),
        std::find_if( s.begin(), s.end(), [&]( unsigned char ch ) { return !f( ch ); } ) );
}
inline void rtrim( std::string& s,
    std::function< bool( unsigned char ) > f = default_trim_what ) {  // trim from end (in place)
    s.erase(
        std::find_if( s.rbegin(), s.rend(), [&]( unsigned char ch ) { return !f( ch ); } ).base(),
        s.end() );
}
inline void trim( std::string& s, std::function< bool( unsigned char ) > f = default_trim_what ) {
    ltrim( s, f );
    rtrim( s, f );
}  // trim from both ends (in place)
inline std::string ltrim_copy(
    std::string s, std::function< bool( unsigned char ) > f = default_trim_what ) {
    ltrim( s, f );
    return s;
}  // trim from start (copying)
inline std::string rtrim_copy(
    std::string s, std::function< bool( unsigned char ) > f = default_trim_what ) {
    rtrim( s, f );
    return s;
}  // trim from end (copying)
inline std::string trim_copy(
    std::string s, std::function< bool( unsigned char ) > f = default_trim_what ) {
    trim( s, f );
    return s;
}  // trim from both ends (copying)
extern string_list_t split( const std::string& s, char delim );
extern string_list_t split( const std::string& s, const char* delimiters );
inline string_list_t split( const std::string& s, const std::string& delimiters ) {
    return split( s, delimiters.c_str() );
}
inline string_vector_t split2vec( const std::string& s, char delim ) {
    string_list_t lst = split( s, delim );
    string_vector_t vec;
    vec.insert( vec.end(), lst.begin(), lst.end() );
    return vec;
}
inline string_vector_t split2vec( const std::string& s, const char* delimiters ) {
    string_list_t lst = split( s, delimiters );
    string_vector_t vec;
    vec.insert( vec.end(), lst.begin(), lst.end() );
    return vec;
}
inline string_vector_t split2vec( const std::string& s, const std::string& delimiters ) {
    return split2vec( s, delimiters.c_str() );
}
extern void replace_all(
    std::string& s, const std::string& f, const std::string& r );  // replace (in place)
extern std::string replace_all_copy(
    const std::string& s, const std::string& f, const std::string& r );  // replace (copying)

template < typename T >
inline std::string to_string( const T& val ) {
    std::ostringstream ss;
    ss << val;
    return ss.str();
}

extern std::string to_upper( const std::string& strSrc );
extern std::string to_lower( const std::string& strSrc );

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern char get_path_slash_char();
extern void ensure_slash_at_end( std::string& s );
extern void ensure_no_slash_at_end( std::string& s );
extern std::string ensure_slash_at_end_copy( const char* s );
extern std::string ensure_slash_at_end_copy( const std::string& s );
extern std::string ensure_no_slash_at_end_copy( const char* s );
extern std::string ensure_no_slash_at_end_copy( const std::string& s );

extern std::string get_tmp_folder_path();  // without slash at end
extern std::string get_tmp_file_path(
    size_t cntCharactersInFileName = 16 );  // uses get_tmp_folder_path() and generates tmp file
                                            // name using
                                            // generate_random_text_string(cntCharactersInFileName)

extern time_t getFileModificationTime( const char* strPath );
extern time_t getFileModificationTime( const std::string& strPath );

extern string_list_t enum_dir( const char* strDirPath, bool bDirs, bool bFiles );
inline string_list_t enum_dir( const std::string& strDirPath, bool bDirs, bool bFiles ) {
    return enum_dir( strDirPath.c_str(), bDirs, bFiles );
}
inline string_vector_t enum_dir_vec( const char* strDirPath, bool bDirs, bool bFiles ) {
    string_list_t lst = enum_dir( strDirPath, bDirs, bFiles );
    string_vector_t vec;
    vec.insert( vec.end(), lst.begin(), lst.end() );
    return vec;
}
inline string_vector_t enum_dir_vec( const std::string& strDirPath, bool bDirs, bool bFiles ) {
    return enum_dir_vec( strDirPath.c_str(), bDirs, bFiles );
}

extern bool is_file( const char* s, bool isCheckReadable = false, bool isCheckWritable = false,
    bool isCheckExecutable = false );
extern bool is_directory( const char* s );
extern bool stat_remove_all_content_in_directory( const char* s );

extern bool is_running_as_root();

extern bool file_exists( const std::string& filename, size_t* p_size = nullptr );

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// json string escapement is based on
// https://github.com/ATNoG/JSONObject/blob/nativeCpp/StringUtils.cpp
extern std::string escapeJSON( const std::string& input );
extern std::string unescapeJSON( const std::string& input );

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern bool to_bool( const std::string& strValue );

extern std::string nanoseconds_2_lifetime_str( uint64_t ns, bool isColored = false );

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern double mem_usage();  // 0.0...1.0
extern size_t cpu_count();

constexpr static int EXT_PART_NUM = 14;
constexpr static int PART_NUM = 7;
constexpr static char DISKSTATS[] = "/proc/diskstats";
constexpr static int MAX_NAME_LEN = 128;
constexpr static int MAX_LINE_LEN = 256;

#if ( defined __BUILDING_4_MAC_OS_X__ )
extern float osx_GetSystemMemoryUsagePercentage();
extern float osx_GetCPULoad();
#else
struct DiskInfo {
    size_t readIOS;
    size_t writeIOS;
    size_t readSectors;
    size_t writeSectors;
};
extern std::atomic_size_t g_nSleepMillisecondsBetweenCpuLoadMeasurements;
extern std::vector< long double > cpu_load();
extern std::map< std::string, DiskInfo > disk_load();
extern nlohmann::json calculate_load_interval( const std::map< std::string, DiskInfo >& prevLoad,
    const std::map< std::string, DiskInfo >& currentLoad, size_t nSleepMs );
#endif

class load_monitor {
    std::atomic_bool stop_flag_;
    std::atomic< double > cpu_load_;
    size_t nSleepMillisecondsBetweenCpuLoadMeasurements_;
    std::thread thread_;

#if ( !defined __BUILDING_4_MAC_OS_X__ )
    nlohmann::json diskLoad_;
    mutable std::mutex diskLoadMutex_;
#endif
public:
    load_monitor( size_t nSleepMillisecondsBetweenCpuLoadMeasurements =
                      0 );  // 0 means use g_nSleepMillisecondsBetweenCpuLoadMeasurements
    virtual ~load_monitor();
    double last_cpu_load() const;
#if ( !defined __BUILDING_4_MAC_OS_X__ )
    nlohmann::json last_disk_load() const;
#endif
private:
    void thread_proc();
#if ( !defined __BUILDING_4_MAC_OS_X__ )
    void calculate_load( size_t nSleep );
#endif
};  /// class cpu_load_monitor

extern std::string create_random_uuid( const char* strSeparator );
extern std::string create_random_uuid();
extern std::string create_uuid();
extern std::string generate_random_text_string( size_t cntCharacters = 10 );

namespace base64 {
// based on https://www.codeproject.com/Articles/98355/SMTP-Client-with-SSL-TLS
extern std::string encode( unsigned char const*, size_t len );
inline std::string encode( const std::string& s ) {
    return encode( ( unsigned char* ) s.c_str(), s.length() );
}
extern std::string decode( std::string const& s );
};  // namespace base64

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class md5 {  // based on https://www.codeproject.com/Articles/98355/SMTP-Client-with-SSL-TLS
public:
    md5();
    void update( unsigned char* input, size_t input_length );
    void update( std::istream& stream );
    void update( FILE* f );
    void update( std::ifstream& stream );
    void finalize();
    md5( unsigned char* string );  // digest string, finalize
    md5( std::istream& stream );   // digest stream, finalize
    md5( FILE* file );             // digest file, close, finalize
    md5( std::ifstream& stream );  // digest stream, close, finalize
    // methods to acquire finalized result
    unsigned char* raw_digest();  // digest as a 16-byte binary array
    char* hex_digest();           // digest as a 33-byte ascii-hex string
    friend std::ostream& operator<<( std::ostream&, md5& context );

private:
    typedef uint32_t uint4;
    typedef uint16_t uint2;
    typedef uint8_t uint1;
    uint4 state[4];
    uint4 count[2];  // number of *bits*, mod 2^64
    uint1 buf[64];   // input buffer
    uint1 digest[16];
    uint1 finalized;
    void init();
    void transform( uint1* buf );  // does the real update work (note, length is implied to be 64)
    static void encode( uint1* dest, uint4* src, uint4 length );
    static void decode( uint4* dest, uint1* src, uint4 length );
    static void memcpy( uint1* dest, uint1* src, uint4 length );
    static void memset( uint1* start, uint1 val, uint4 length );
    static inline uint4 rotate_left( uint4 x, uint4 n );
    static inline uint4 F( uint4 x, uint4 y, uint4 z );
    static inline uint4 G( uint4 x, uint4 y, uint4 z );
    static inline uint4 H( uint4 x, uint4 y, uint4 z );
    static inline uint4 I( uint4 x, uint4 y, uint4 z );
    static inline void FF( uint4& a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac );
    static inline void GG( uint4& a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac );
    static inline void HH( uint4& a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac );
    static inline void II( uint4& a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac );
};  /// class md5

};  // namespace tools

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template < typename T >
class round_queue {
    typedef std::vector< T > vec_t;
    vec_t vec_;
    size_t limit_, pos_;  // pos_ is at[0] element position
    round_queue& reconstruct() {
        if ( pos_ == 0 )
            return ( *this );
        vec_t v;
        size_t i, cnt = size();
        for ( i = 0; i < cnt; ++i )
            v.push_back( at( i ) );
        vec_ = v;
        pos_ = 0;
        return ( *this );
    }

public:
    size_t size() const { return vec_.size(); }
    size_t limit() const { return limit_; }
    round_queue& limit( size_t lim ) {
        if ( limit_ == lim )
            return ( *this );
        reconstruct();
        limit_ = lim;
        while ( size() > limit_ )
            vec_.pop_back();
        vec_.reserve( limit_ );
        return ( *this );
    }
    bool push_back( const T& val ) {
        if ( limit_ == 0 )
            return false;
        if ( size() < limit_ ) {
            vec_.push_back( val );
            return true;
        }
        vec_[pos_] = val;
        ++pos_;
        if ( pos_ >= limit_ )
            pos_ = 0;
        return true;
    }
    T& at( size_t i ) {
        size_t cnt = size();
        if ( i >= cnt )
            throw std::runtime_error( "round queue index out of range" );
        i += pos_;
        if ( i >= cnt )
            i -= cnt;
        T& val = vec_[i];
        return val;
    }
    const T& at( size_t i ) const { return ( const_cast< round_queue* >( this ) )->at( i ); }
    T& front() { return at( 0 ); }
    const T& front() const { return at( 0 ); }
    T& back() { return at( size() - 1 ); }
    const T& back() const { return at( size() - 1 ); }
    round_queue& clear() {
        vec_.clear();
        pos_ = 0;
        return ( *this );
    }
    bool empty() const {
        size_t cnt = vec_.size();
        return ( cnt == 0 ) ? true : false;
    }
    int compare( const round_queue& other ) const {
        size_t cntThis = size(), cntOther = other.size();
        size_t i, cntWalk = ( cntThis < cntOther ) ? cntThis : cntOther;
        for ( i = 0; i < cntWalk; ++i ) {
            const T& valThis = at( i );
            const T& valOther = other.at( i );
            if ( valThis < valOther )
                return -1;
            if ( valThis > valOther )
                return 1;
        }
        if ( cntThis < cntOther )
            return -1;
        if ( cntThis > cntOther )
            return 1;
        return 0;
    }
    round_queue& assign( const round_queue& other ) {
        if ( ( ( void* ) ( this ) ) == ( ( void* ) ( &other ) ) )
            return ( *this );
        clear();
        vec_ = other.vec_;
        limit_ = other.limit_;
        pos_ = other.pos_;
        return ( *this );
    }
    round_queue& move( round_queue& other ) {
        if ( ( ( void* ) ( this ) ) == ( ( void* ) ( &other ) ) )
            return ( *this );
        assign( other );
        other.clear();
        return ( *this );
    }
    round_queue( size_t lim = 0 ) : limit_( 0 ), pos_( 0 ) { limit( lim ); }
    round_queue( const round_queue& other ) { assign( other ); }
    round_queue( round_queue&& other ) { move( other ); }
    ~round_queue() { clear(); }
    round_queue& operator=( const round_queue& other ) { return assign( other ); }
    round_queue& operator=( round_queue&& other ) { return move( other ); }
    bool operator==( const round_queue& other ) const {
        return ( compare( other ) == 0 ) ? true : false;
    }
    bool operator!=( const round_queue& other ) const {
        return ( compare( other ) != 0 ) ? true : false;
    }
    bool operator<( const round_queue& other ) const {
        return ( compare( other ) < 0 ) ? true : false;
    }
    bool operator<=( const round_queue& other ) const {
        return ( compare( other ) <= 0 ) ? true : false;
    }
    bool operator>( const round_queue& other ) const {
        return ( compare( other ) > 0 ) ? true : false;
    }
    bool operator>=( const round_queue& other ) const {
        return ( compare( other ) >= 0 ) ? true : false;
    }
    T& operator[]( size_t i ) { return at( i ); }
    const T& operator[]( size_t i ) const { return at( i ); }
};  /// template class round_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template < typename summary_t, typename base_t >
class with_summary : public base_t {
    summary_t summary_;

public:
    summary_t summary() const { return summary_; }
    with_summary& summary( summary_t n ) {
        summary_ = n;
        return ( *this );
    }
    with_summary& summary_increment( summary_t n = 1 ) {
        summary_ += n;
        return ( *this );
    }
    with_summary& summary_decrement( summary_t n = 1 ) {
        summary_ -= n;
        return ( *this );
    }
    with_summary& clear() {
        base_t::clear();
        summary_ = 0;
        return ( *this );
    }
    bool empty() const {
        if ( summary_ )
            return false;
        return base_t::empty();
    }
    int compare( const with_summary& other ) const {
        if ( summary_ < other.summary_ )
            return -1;
        if ( summary_ > other.summary_ )
            return -1;
        return base_t::compare( other );
    }
    with_summary& assign( const with_summary& other ) {
        if ( ( ( void* ) ( this ) ) == ( ( void* ) ( &other ) ) )
            return ( *this );
        base_t::assign( other );
        summary_ = other.summary_;
        return ( *this );
    }
    with_summary& move( with_summary& other ) {
        if ( ( ( void* ) ( this ) ) == ( ( void* ) ( &other ) ) )
            return ( *this );
        assign( other );
        other.clear();
        return ( *this );
    }
    with_summary() : summary_( 0 ) { clear(); }
    with_summary( const with_summary& other ) : summary_( 0 ) { assign( other ); }
    with_summary( with_summary&& other ) : summary_( 0 ) { move( other ); }
    ~with_summary() { clear(); }
    with_summary& operator=( const with_summary& other ) { return assign( other ); }
    with_summary& operator=( with_summary&& other ) { return move( other ); }
    bool operator==( const with_summary& other ) const {
        return ( compare( other ) == 0 ) ? true : false;
    }
    bool operator!=( const with_summary& other ) const {
        return ( compare( other ) != 0 ) ? true : false;
    }
    bool operator<( const with_summary& other ) const {
        return ( compare( other ) < 0 ) ? true : false;
    }
    bool operator<=( const with_summary& other ) const {
        return ( compare( other ) <= 0 ) ? true : false;
    }
    bool operator>( const with_summary& other ) const {
        return ( compare( other ) > 0 ) ? true : false;
    }
    bool operator>=( const with_summary& other ) const {
        return ( compare( other ) >= 0 ) ? true : false;
    }
};  /// template class with_summary

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace terminal {

extern char getch_no_wait();

};  // namespace terminal

namespace signal {

extern std::atomic_bool g_bStop;

extern bool get_signal_description( int nSignalNo, std::string& strSignalName,
    std::string& strSignalDescription );  // returns true if signal name is known
extern std::string signal2str( int nSignalNo, const char* strPrefix = nullptr,
    const char* strSuffix = nullptr, bool isAddDesc = true );
typedef void ( *fn_signal_handler_t )( int nSignalNo );
extern bool init_common_signal_handling( fn_signal_handler_t fnSignalHander );
extern void print_backtrace();

};  // namespace signal


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace skutils

#endif  /// (!defined __SKUTILS_UTILS_H)
