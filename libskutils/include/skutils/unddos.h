#if ( !defined __SKUTILS_UN_DDOS_H )
#define __SKUTILS_UN_DDOS_H 1

#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <algorithm>
#include <chrono>
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

class origin_entry_setting {
public:
    std::string origin_wildcard_;
    size_t maxCallsPerSecond_ = 0;
    size_t maxCallsPerMinute_ = 0;
    origin_entry_setting();
    origin_entry_setting( const origin_entry_setting& other );
    origin_entry_setting( origin_entry_setting&& other );
    virtual ~origin_entry_setting();
    origin_entry_setting& operator=( const origin_entry_setting& other );
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
};  /// class origin_entry_setting

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class settings {
public:
    std::vector< origin_entry_setting > origins_;
    size_t maxCallsPerSecond_ = 0;
    size_t maxCallsPerMinute_ = 0;
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
        const std::string& origin, size_t idxStart = std::string::npos ) const;
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

typedef std::list< time_entry > time_entries_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tracked_origin {
public:
    std::string origin_;
    time_entries_t time_entries_;
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
        duration durationToPast = duration( 60 ) ) const;
};  /// class tracked_origin

typedef std::set< tracked_origin > tracked_origins_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class algorithm {
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;
    mutex_type mtx_;
    settings settings_;
    tracked_origins_t tracked_origins_;
    size_t cntCalls_ = 0;  // all registered calls, all origins

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
    bool register_access_from_origin( const char* origin,
        time_tick_mark ttmNow = time_tick_mark( 0 ), duration durationToPast = duration( 60 ) );
};  /// class algorithm

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace unddos
};  // namespace skutils

#endif  // (!defined __SKUTILS_UN_DDOS_H)
