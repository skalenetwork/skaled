#if ( !defined __SKUTILS_UN_DDOS_H )
#define __SKUTILS_UN_DDOS_H 1

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <algorithm>
#include <chrono>
#include <limits>
#include <list>
#include <set>
#include <string>
#include <vector>

#include <skutils/multithreading.h>
#include <skutils/utils.h>

namespace skutils {
namespace unddos {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef time_t time_tick_mark;
typedef time_t duration;

inline time_tick_mark now_tick_mark() {
    return time_tick_mark( time( nullptr ) );
}

inline time_tick_mark make_tick_mark( time_tick_mark ttm ) {
    if ( ttm == time_tick_mark( 0 ) )
        ttm = now_tick_mark();
    return ttm;
}

inline void adjust_now_tick_mark( time_tick_mark& ttm ) {
    ttm = make_tick_mark( ttm );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class custom_method_setting {
public:
    size_t max_calls_per_second_ = 0;
    size_t max_calls_per_minute_ = 0;
    custom_method_setting& merge( const custom_method_setting& other ) {
        max_calls_per_second_ = std::min( max_calls_per_second_, other.max_calls_per_second_ );
        max_calls_per_minute_ = std::min( max_calls_per_minute_, other.max_calls_per_minute_ );
        return ( *this );
    }
};  // class custom_method_setting

typedef std::map< std::string, custom_method_setting > map_custom_method_settings_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::vector< std::string > origin_wildcards_t;

class origin_entry_setting {
public:
    origin_wildcards_t origin_wildcards_;
    size_t max_calls_per_second_ = 0;
    size_t max_calls_per_minute_ = 0;
    duration ban_peak_ = duration( 0 );
    duration ban_lengthy_ = duration( 0 );
    size_t max_ws_conn_ = 0;
    map_custom_method_settings_t map_custom_method_settings_;
    origin_entry_setting();
    origin_entry_setting( const origin_entry_setting& other );
    origin_entry_setting( origin_entry_setting&& other );
    virtual ~origin_entry_setting();
    origin_entry_setting& operator=( const origin_entry_setting& other );
    void load_defaults_for_any_origin();
    void load_friendly_for_any_origin();
    void load_reasonable_for_any_origin();
    void load_unlim_for_any_origin();
    void load_unlim_for_localhost_only();
    void load_custom_method_as_multiplier_of_default(
        const char* strMethod, double lfMultiplier = 10.0 );
    void load_recommended_custom_methods_as_multiplier_of_default( double lfMultiplier = 10.0 );
    bool empty() const;
    operator bool() const { return ( !empty() ); }
    bool operator!() const { return empty(); }
    void clear();
    origin_entry_setting& assign( const origin_entry_setting& other );
    origin_entry_setting& merge( const origin_entry_setting& other );
    void fromJSON( const nlohmann::json& jo );
    void toJSON( nlohmann::json& jo ) const;
    bool match_origin( const char* origin ) const;
    bool match_origin( const std::string& origin ) const;
    size_t max_calls_per_second( const char* strMethod ) const;
    size_t max_calls_per_minute( const char* strMethod ) const;
};  /// class origin_entry_setting

typedef std::vector< origin_entry_setting > origin_entry_settings_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class settings {
public:
    bool enabled_ = true;
    origin_entry_settings_t origins_;
    origin_entry_setting global_limit_;
    settings();
    settings( const settings& other );
    settings( settings&& other );
    virtual ~settings();
    settings& operator=( const settings& other );
    bool empty() const;
    operator bool() const { return ( !empty() ); }
    bool operator!() const { return empty(); }
    void clear();
    settings& assign( const settings& other );
    settings& merge( const origin_entry_setting& oe );
    settings& merge( const settings& other );
    size_t indexOfOrigin( const origin_entry_setting& oe, size_t idxStart = std::string::npos );
    size_t indexOfOrigin( const char* origin_wildcard, size_t idxStart = std::string::npos );
    size_t indexOfOrigin( const std::string& origin_wildcard, size_t idxStart = std::string::npos );
    void fromJSON( const nlohmann::json& jo );
    void toJSON( nlohmann::json& jo ) const;
    size_t find_origin_entry_setting_match(
        const char* origin, size_t idxStart = std::string::npos ) const;
    size_t find_origin_entry_setting_match(
        const std::string& origin, size_t idxStart = std::string::npos ) const {
        return find_origin_entry_setting_match( origin.c_str(), idxStart );
    }
    origin_entry_setting& find_origin_entry_setting( const char* origin );
    origin_entry_setting& find_origin_entry_setting( const std::string& origin ) {
        return find_origin_entry_setting( origin.c_str() );
    }
    const origin_entry_setting& find_origin_entry_setting( const char* origin ) const {
        return ( const_cast< settings* >( this ) )->find_origin_entry_setting( origin );
    }
    const origin_entry_setting& find_origin_entry_setting( const std::string& origin ) const {
        return ( const_cast< settings* >( this ) )->find_origin_entry_setting( origin );
    }
    origin_entry_setting& auto_append_any_origin_rule();
};  /// class settings

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class time_entry {
public:
    time_tick_mark ttm_ = time_tick_mark( 0 );
    time_entry( time_tick_mark ttm = time_tick_mark( 0 ) );
    time_entry( const time_entry& other );
    time_entry( time_entry&& other );
    virtual ~time_entry();
    time_entry& operator=( const time_entry& other );
    bool empty() const;
    operator bool() const { return ( !empty() ); }
    bool operator!() const { return empty(); }
    void clear();
    time_entry& assign( const time_entry& other );
    int compare( const time_entry& other ) const;
    bool operator==( const time_entry& other ) const {
        return ( compare( other ) == 0 ) ? true : false;
    }
    bool operator!=( const time_entry& other ) const {
        return ( compare( other ) != 0 ) ? true : false;
    }
    bool operator<( const time_entry& other ) const {
        return ( compare( other ) < 0 ) ? true : false;
    }
    bool operator<=( const time_entry& other ) const {
        return ( compare( other ) <= 0 ) ? true : false;
    }
    bool operator>( const time_entry& other ) const {
        return ( compare( other ) > 0 ) ? true : false;
    }
    bool operator>=( const time_entry& other ) const {
        return ( compare( other ) >= 0 ) ? true : false;
    }
};  /// class time_entry

typedef std::vector< time_entry > time_entries_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tracked_origin {
public:
    std::string origin_;
    time_entries_t time_entries_;
    time_tick_mark ban_until_ = time_tick_mark( 0 );
    tracked_origin( const char* origin = nullptr, time_tick_mark ttm = time_tick_mark( 0 ) );
    tracked_origin( const std::string& origin, time_tick_mark ttm = time_tick_mark( 0 ) );
    tracked_origin( const tracked_origin& other );
    tracked_origin( tracked_origin&& other );
    virtual ~tracked_origin();
    operator std::string() const { return origin_; }
    tracked_origin& operator=( const tracked_origin& other );
    bool empty() const;
    operator bool() const { return ( !empty() ); }
    bool operator!() const { return empty(); }
    void clear();
    tracked_origin& assign( const tracked_origin& other );
    int compare( const tracked_origin& other ) const;
    int compare( const char* origin ) const;
    int compare( const std::string& origin ) const;
    //
    bool operator==( const tracked_origin& other ) const {
        return ( compare( other ) == 0 ) ? true : false;
    }
    bool operator!=( const tracked_origin& other ) const {
        return ( compare( other ) != 0 ) ? true : false;
    }
    bool operator<( const tracked_origin& other ) const {
        return ( compare( other ) < 0 ) ? true : false;
    }
    bool operator<=( const tracked_origin& other ) const {
        return ( compare( other ) <= 0 ) ? true : false;
    }
    bool operator>( const tracked_origin& other ) const {
        return ( compare( other ) > 0 ) ? true : false;
    }
    bool operator>=( const tracked_origin& other ) const {
        return ( compare( other ) >= 0 ) ? true : false;
    }
    //
    bool operator==( const char* origin ) const {
        return ( compare( origin ) == 0 ) ? true : false;
    }
    bool operator!=( const char* origin ) const {
        return ( compare( origin ) != 0 ) ? true : false;
    }
    bool operator<( const char* origin ) const { return ( compare( origin ) < 0 ) ? true : false; }
    bool operator<=( const char* origin ) const {
        return ( compare( origin ) <= 0 ) ? true : false;
    }
    bool operator>( const char* origin ) const { return ( compare( origin ) > 0 ) ? true : false; }
    bool operator>=( const char* origin ) const {
        return ( compare( origin ) >= 0 ) ? true : false;
    }
    //
    bool operator==( const std::string& origin ) const {
        return ( compare( origin ) == 0 ) ? true : false;
    }
    bool operator!=( const std::string& origin ) const {
        return ( compare( origin ) != 0 ) ? true : false;
    }
    bool operator<( const std::string& origin ) const {
        return ( compare( origin ) < 0 ) ? true : false;
    }
    bool operator<=( const std::string& origin ) const {
        return ( compare( origin ) <= 0 ) ? true : false;
    }
    bool operator>( const std::string& origin ) const {
        return ( compare( origin ) > 0 ) ? true : false;
    }
    bool operator>=( const std::string& origin ) const {
        return ( compare( origin ) >= 0 ) ? true : false;
    }
    //
    size_t unload_old_data_by_time_to_past(
        time_tick_mark ttmNow = time_tick_mark( 0 ), duration durationToPast = duration( 60 ) );
    size_t unload_old_data_by_count( size_t cntEntriesMax );
    size_t count_to_past( time_tick_mark ttmNow = time_tick_mark( 0 ),
        duration durationToPast = duration( 60 ), size_t cntOptimizedMaxSteps = size_t( -1 ),
        size_t cntTargetUnDDoS = size_t( -1 ) ) const;
    bool clear_ban();
    bool check_ban( time_tick_mark ttmNow = time_tick_mark( 0 ), bool isAutoClear = true );
};  /// class tracked_origin

typedef std::set< tracked_origin > tracked_origins_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum class e_high_load_detection_result_t {
    ehldr_no_error,
    ehldr_peak,     // ban by too high load per minute
    ehldr_lengthy,  // ban by too high load per second
    ehldr_bad_origin,
    ehldr_ban  // still banned
};

class algorithm {
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;
    mutable mutex_type mtx_;
    mutable settings settings_;
    tracked_origins_t tracked_origins_;
    tracked_origin tracked_global_;
    typedef std::map< std::string, size_t > map_ws_conn_counts_t;
    map_ws_conn_counts_t map_ws_conn_counts_;
    size_t ws_conn_count_global_ = 0;
    size_t cntOptimizedMaxSteps4cm_ =
        15;  // local per one caller, per minute (optimize approximation for calls per time unit)
    size_t cntOptimizedMaxSteps4cs_ =
        15;  // local per one caller, per second (optimize approximation for calls per time unit)
    size_t cntOptimizedMaxSteps4gm_ =
        10;  // global for all callers, per minute (optimize approximation for calls per time unit)
    size_t cntOptimizedMaxSteps4gs_ =
        10;  // global for all callers, per second (optimize approximation for calls per time unit)

public:
    algorithm();
    algorithm( const settings& st );
    algorithm( const algorithm& ) = delete;
    algorithm( algorithm&& ) = delete;
    virtual ~algorithm();
    algorithm& operator=( const algorithm& ) = delete;
    algorithm& operator=( const settings& st );
    size_t unload_old_data_by_time_to_past(
        time_tick_mark ttmNow = time_tick_mark( 0 ), duration durationToPast = duration( 60 ) );
    e_high_load_detection_result_t register_call_from_origin( const char* origin,
        const char* strMethod, time_tick_mark ttmNow = time_tick_mark( 0 ),
        duration durationToPast = duration( 60 ) );
    e_high_load_detection_result_t register_call_from_origin( const std::string& origin,
        const std::string& strMethod, time_tick_mark ttmNow = time_tick_mark( 0 ),
        duration durationToPast = duration( 60 ) ) {
        return register_call_from_origin(
            origin.c_str(), strMethod.c_str(), ttmNow, durationToPast );
    }
    e_high_load_detection_result_t register_call_from_origin( const char* origin,
        time_tick_mark ttmNow = time_tick_mark( 0 ), duration durationToPast = duration( 60 ) ) {
        return register_call_from_origin( origin, nullptr, ttmNow, durationToPast );
    }
    e_high_load_detection_result_t register_call_from_origin( const std::string& origin,
        time_tick_mark ttmNow = time_tick_mark( 0 ), duration durationToPast = duration( 60 ) ) {
        return register_call_from_origin( origin.c_str(), nullptr, ttmNow, durationToPast );
    }
    bool is_ban_ws_conn_for_origin( const char* origin ) const;
    bool is_ban_ws_conn_for_origin( const std::string& origin ) const {
        return is_ban_ws_conn_for_origin( origin.c_str() );
    }
    e_high_load_detection_result_t register_ws_conn_for_origin( const char* origin );
    e_high_load_detection_result_t register_ws_conn_for_origin( const std::string& origin ) {
        return register_ws_conn_for_origin( origin.c_str() );
    }
    bool unregister_ws_conn_for_origin( const char* origin );
    bool unregister_ws_conn_for_origin( const std::string& origin ) {
        return unregister_ws_conn_for_origin( origin.c_str() );
    }
    bool load_settings_from_json( const nlohmann::json& joUnDdosSettings );
    settings get_settings() const;
    void set_settings( const settings& new_settings ) const;
    nlohmann::json get_settings_json() const;
    nlohmann::json stats( time_tick_mark ttmNow = time_tick_mark( 0 ),
        duration durationToPast = duration( 60 ) ) const;
};  /// class algorithm

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace unddos
};  // namespace skutils

#endif  // (!defined __SKUTILS_UN_DDOS_H)
