#if ( !defined __SKUTILS_UN_DDOS_H )
#define __SKUTILS_UN_DDOS_H 1

#include <skutils/multithreading.h>
#include <skutils/utils.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <algorithm>
#include <chrono>
#include <limits>
#include <list>
#include <set>
#include <shared_mutex>
#include <string>
#include <vector>

namespace skutils ::unddos {


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

inline void setCallTimeToNowIfZero( time_tick_mark& ttm ) {
    ttm = make_tick_mark( ttm );
}

class custom_method_limits {
public:
    size_t m_maxCallsPerSecond = 0;
    size_t m_maxCallsPerMinute = 0;

    custom_method_limits& merge( const custom_method_limits& other ) {
        m_maxCallsPerSecond = std::min( m_maxCallsPerSecond, other.m_maxCallsPerSecond );
        m_maxCallsPerMinute = std::min( m_maxCallsPerMinute, other.m_maxCallsPerMinute );
        return ( *this );
    }
};

typedef std::map< std::string, custom_method_limits > map_custom_method_limits_t;

typedef std::vector< std::string > origin_wildcards_t;

class origin_dos_limits {
public:
    origin_wildcards_t m_originWildcards;
    size_t m_defaultMaxCallsPerSec = 0;
    size_t m_defaultMaxCallsPerMin = 0;
    duration m_banPerSecDuration = duration( 0 );
    duration m_banPerMinDuration = duration( 0 );
    map_custom_method_limits_t m_mapCustomMethodLimits;

    origin_dos_limits();

    origin_dos_limits( const origin_dos_limits& other );

    origin_dos_limits( origin_dos_limits&& other );

    virtual ~origin_dos_limits();

    origin_dos_limits& operator=( const origin_dos_limits& other );

    void load_unlim_for_any_origin();

    bool empty() const;

    operator bool() const { return ( !empty() ); }

    bool operator!() const { return empty(); }

    void clear();

    origin_dos_limits& assign( const origin_dos_limits& other );

    origin_dos_limits& merge( const origin_dos_limits& other );

    void fromJSON( const nlohmann::json& jo );

    void toJSON( nlohmann::json& jo ) const;

    bool match_origin( const std::string& origin ) const;

    size_t max_calls_per_second( const std::string& strMethod ) const;

    size_t max_calls_per_minute( const std::string& strMethod ) const;
};

typedef std::vector< origin_dos_limits > origin_entry_settings_t;

class settings {
public:
    bool m_enabled = true;
    origin_entry_settings_t m_originDosLimits;
    origin_dos_limits m_globalLimitSetting;

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


    void fromJSON( const nlohmann::json& jo );

    size_t findOriginLimitsMatch( const std::string& origin ) const;

    origin_dos_limits& findOriginDosLimits( const std::string& _origin );
};


enum class e_high_load_detection_result_t {
    ehldr_no_error,
    ehldr_detected_ban_per_sec,  // ban by too high load per sec
    ehldr_detected_ban_per_min,  // ban by too high load per min
    ehldr_bad_origin,
    ehldr_already_banned  // still banned
};


class tracked_origin {
public:
    std::shared_mutex x_mutex;
    std::string m_origin;
    uint64_t m_currentSec = 0;
    uint64_t m_currentMin = 0;
    std::map< std::string, uint64_t > m_currentMinUseCounterPerMethod;
    std::map< std::string, uint64_t > m_currentSecUseCounterPerMethod;
    std::atomic< uint64_t > m_banUntilSec = 0;
    origin_dos_limits m_dosLimits;

    tracked_origin( const std::string& origin );


    tracked_origin( const tracked_origin& other )
        : m_origin( other.m_origin ),
          m_currentSec( other.m_currentSec ),
          m_currentMin( other.m_currentMin ),
          m_currentMinUseCounterPerMethod( other.m_currentMinUseCounterPerMethod ),
          m_currentSecUseCounterPerMethod( other.m_currentSecUseCounterPerMethod ),
          m_banUntilSec( other.m_banUntilSec.load() ) {}


    tracked_origin( tracked_origin&& _other ) noexcept
        : m_origin( std::move( _other.m_origin ) ),
          m_currentSec( _other.m_currentSec ),
          m_currentMin( _other.m_currentMin ),
          m_currentMinUseCounterPerMethod( std::move( _other.m_currentMinUseCounterPerMethod ) ),
          m_currentSecUseCounterPerMethod( std::move( _other.m_currentSecUseCounterPerMethod ) ),
          m_banUntilSec( _other.m_banUntilSec.load() ) {
        // x_mutex is intentionally not moved
        _other.m_currentSec = 0;
        _other.m_currentMin = 0;
        _other.m_banUntilSec = 0;
    }


    virtual ~tracked_origin();

    operator std::string() const { return m_origin; }


    bool isBanned( uint64_t _timeSec );

    void recordUse( uint64_t _useTimeSec, const std::string& _method );

    void setDosLimits( const origin_dos_limits& _dosLimits );

    e_high_load_detection_result_t detectBan(
        uint64_t _callTimeSec, const std::string& _strMethod );

    e_high_load_detection_result_t recordMethodUseAndDetectBan(
        uint64_t _callTimeSec, const std::string& _strMethod );
};


class algorithm {
    typedef std::map< std::string, size_t > map_ws_conn_counts_t;

    mutable std::shared_mutex x_mtx;
    mutable settings m_settings;

    tracked_origin m_globalOrigin;

    std::map< std::string, std::shared_ptr< tracked_origin > > m_trackedOriginsMap;

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

    e_high_load_detection_result_t register_call_from_origin( const std::string& _origin,
        const std::string& _strMethod, time_tick_mark _callTime = time_tick_mark( 0 ) );


    e_high_load_detection_result_t register_call_from_origin(
        const std::string& origin, time_tick_mark ttmNow = time_tick_mark( 0 ) ) {
        return register_call_from_origin( origin, "", ttmNow );
    }

    void load_settings_from_json( const nlohmann::json& joUnDdosSettings );

    void disable_ddos() const;


    void addNewOriginToMap( const std::string& _origin, time_tick_mark _callTime );
};

};  // namespace skutils::unddos

#endif  // (!defined __SKUTILS_UN_DDOS_H)
