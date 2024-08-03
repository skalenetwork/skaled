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

inline void setCallTimeToNowIfZero(time_tick_mark& ttm ) {
    ttm = make_tick_mark( ttm );
}

class custom_method_setting {
public:
    size_t max_calls_per_second_ = 0;
    size_t max_calls_per_minute_ = 0;
    custom_method_setting& merge( const custom_method_setting& other ) {
        max_calls_per_second_ = std::min( max_calls_per_second_, other.max_calls_per_second_ );
        max_calls_per_minute_ = std::min( max_calls_per_minute_, other.max_calls_per_minute_ );
        return ( *this );
    }
};

typedef std::map< std::string, custom_method_setting > map_custom_method_settings_t;

typedef std::vector< std::string > origin_wildcards_t;

class origin_entry_setting {
public:
    origin_wildcards_t m_originWildcards;
    size_t m_defaultMaxCallsPerSec = 0;
    size_t m_defaultMaxCallsPerMin = 0;
    duration m_banPerSecDuration = duration(0 );
    duration m_banPerMinDuration = duration(0 );
    size_t m_maxWSConn = 0;
    map_custom_method_settings_t m_mapCustomMethodSettings;
    origin_entry_setting();
    origin_entry_setting( const origin_entry_setting& other );
    origin_entry_setting( origin_entry_setting&& other );
    virtual ~origin_entry_setting();
    origin_entry_setting& operator=( const origin_entry_setting& other );
    void load_defaults_for_any_origin();
    void load_friendly_for_any_origin();
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
    size_t max_calls_per_second( const char* strMethod ) const;
    size_t max_calls_per_minute( const char* strMethod ) const;
};

typedef std::vector< origin_entry_setting > origin_entry_settings_t;

class settings {
public:
    bool m_enabled = true;
    origin_entry_settings_t m_originSettings;
    origin_entry_setting m_globalLimitSetting;
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
    origin_entry_setting& find_origin_entry_setting( const char* origin );
    origin_entry_setting& auto_append_any_origin_rule();
};


class tracked_origin {
public:
    std::mutex x_mutex;
    std::string m_origin;
    uint64_t m_currentSec;
    uint64_t m_currentMin;
    std::map<std::string, uint64_t> m_currentMinUseCounterPerMethod;
    std::map<std::string, uint64_t>  m_currentSecUseCounterPerMethod;
    uint64_t m_banUntilSec = 0;
    tracked_origin( const char* origin = nullptr, time_tick_mark ttm = time_tick_mark( 0 ) );
    tracked_origin( const std::string& origin, time_tick_mark ttm = time_tick_mark( 0 ) );

    tracked_origin(const tracked_origin& other)
            : m_origin(other.m_origin),
              m_currentSec(other.m_currentSec),
              m_currentMin(other.m_currentMin),
              m_currentMinUseCounterPerMethod(other.m_currentMinUseCounterPerMethod),
              m_currentSecUseCounterPerMethod(other.m_currentSecUseCounterPerMethod),
              m_banUntilSec(other.m_banUntilSec) {
    }


    tracked_origin(tracked_origin&& _other) noexcept
            : m_origin(std::move(_other.m_origin)),
              m_currentSec(_other.m_currentSec),
              m_currentMin(_other.m_currentMin),
              m_currentMinUseCounterPerMethod(std::move(_other.m_currentMinUseCounterPerMethod)),
              m_currentSecUseCounterPerMethod(std::move(_other.m_currentSecUseCounterPerMethod)),
              m_banUntilSec(_other.m_banUntilSec) {
        // x_mutex is intentionally not moved
        _other.m_currentSec = 0;
        _other.m_currentMin = 0;
        _other.m_banUntilSec = 0;
    }


    virtual ~tracked_origin();
    operator std::string() const { return m_origin; }
    void clearBan();
    bool isBanned(uint64_t _timeSec);

    void recordUse(uint64_t _useTimeSec, const char* _strMethod);

};


enum class e_high_load_detection_result_t {
    ehldr_no_error,
    ehldr_detected_ban_per_sec,     // ban by too high load per sec
    ehldr_detected_ban_per_min,  // ban by too high load per min
    ehldr_bad_origin,
    ehldr_already_banned  // still banned
};



class algorithm {

    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;
    typedef std::map< std::string, size_t > map_ws_conn_counts_t;

    mutable mutex_type x_mtx;
    mutable settings m_settings;

    tracked_origin m_globalOrigin;
    std::map< std::string, tracked_origin > m_trackedOriginsMap;

    map_ws_conn_counts_t m_mapWsConnCounts;
    size_t m_WsConnCountGlobal = 0;

public:
    algorithm();
    algorithm( const settings& st );
    algorithm( const algorithm& ) = delete;
    algorithm( algorithm&& ) = delete;
    virtual ~algorithm();
    algorithm& operator=( const algorithm& ) = delete;
    algorithm& operator=( const settings& st );
    e_high_load_detection_result_t register_call_from_origin(const char* _origin,
                                                             const char* _strMethod, time_tick_mark _callTime = time_tick_mark(0 ),
                                                             duration _durationToPast = duration(60 ) );

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
    bool is_ban_ws_conn_for_origin( const char* origin ) const;

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
};

};
};

#endif  // (!defined __SKUTILS_UN_DDOS_H)
