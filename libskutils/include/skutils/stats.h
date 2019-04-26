#if ( !defined __SKUTILS_STATS_H )
#define __SKUTILS_STATS_H 1

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <skutils/atomic_shared_ptr.h>
#include <skutils/multithreading.h>
#include <skutils/utils.h>

#include <inttypes.h>

#include <json.hpp>

namespace skutils {
namespace stats {

typedef std::chrono::steady_clock clock;
typedef std::chrono::time_point< clock > time_point;

typedef std::chrono::nanoseconds nanoseconds;
typedef int64_t duration_base_t;
typedef std::chrono::duration< duration_base_t > duration;

struct event_record_item_t {
    typedef event_record_item_t myt;
    typedef myt& myrt;
    typedef const myt& myrct;
    time_point time_stamp_;
    myrt assign( myrct x );
    int compare( myrct x ) const;
    event_record_item_t();
    event_record_item_t( time_point a_time_stamp_ );
    event_record_item_t( myrct x );
    ~event_record_item_t();
    myrt operator=( myrct x ) { return assign( x ); }
    bool operator==( myrct x ) const { return ( compare( x ) == 0 ) ? true : false; }
    bool operator!=( myrct x ) const { return ( compare( x ) != 0 ) ? true : false; }
    bool operator<( myrct x ) const { return ( compare( x ) < 0 ) ? true : false; }
    bool operator<=( myrct x ) const { return ( compare( x ) <= 0 ) ? true : false; }
    bool operator>( myrct x ) const { return ( compare( x ) > 0 ) ? true : false; }
    bool operator>=( myrct x ) const { return ( compare( x ) >= 0 ) ? true : false; }
};  /// struct event_record_item_t
typedef skutils::with_summary< size_t, skutils::round_queue< event_record_item_t > > event_queue_t;

struct UnitsPerSecond {
    UnitsPerSecond( time_point time )
        : m_startTime{clock::now()},
          m_prevTime{time},
          m_lastTime{std::move( time )},
          m_prevPerSec{1},
          m_total{1},
          m_prevUnitsPerSecond{0.0} {}
    UnitsPerSecond( time_point time, size_t prevCount, size_t totalCount )
        : m_startTime{clock::now()},
          m_prevTime{time},
          m_lastTime{std::move( time )},
          m_prevPerSec{prevCount},
          m_total{totalCount},
          m_prevUnitsPerSecond{0.0} {}

    bool operator<( const UnitsPerSecond& unit1 ) const {
        return std::tie( m_startTime, m_prevTime, m_lastTime, m_prevPerSec, m_total ) <
               std::tie( unit1.m_startTime, unit1.m_prevTime, unit1.m_lastTime, unit1.m_prevPerSec,
                   unit1.m_total );
    }
    bool operator>( const UnitsPerSecond& unit1 ) const { return ( *this < unit1 ) ? false : true; }
    time_point m_startTime;
    time_point m_prevTime;
    time_point m_lastTime;
    size_t m_prevPerSec;
    size_t m_total;
    double m_prevUnitsPerSecond;
};

using t_NamedEvents = std::map< std::string, UnitsPerSecond >;
using t_NamedEventsIt = t_NamedEvents::iterator;

class named_event_stats {
public:
    typedef named_event_stats myt;
    typedef myt& myrt;
    typedef myt&& myrrt;
    typedef const myt& myrct;
    typedef std::lock_guard< myt > lock_type;
    named_event_stats();
    named_event_stats( myrct x );
    named_event_stats( myrrt x );
    virtual ~named_event_stats();
    myrt limit( size_t lim );
    bool empty() const;
    void clear();
    int compare( myrct x ) const;
    myrt assign( myrct x );
    myrt move( myrt x );
    virtual void lock();
    virtual void unlock();
    myrt operator=( myrct x ) { return assign( x ); }
    myrt operator=( myrrt x ) { return move( x ); }
    bool operator==( myrct x ) const { return ( compare( x ) == 0 ) ? true : false; }
    bool operator!=( myrct x ) const { return ( compare( x ) != 0 ) ? true : false; }
    bool operator<( myrct x ) const { return ( compare( x ) < 0 ) ? true : false; }
    bool operator<=( myrct x ) const { return ( compare( x ) <= 0 ) ? true : false; }
    bool operator>( myrct x ) const { return ( compare( x ) > 0 ) ? true : false; }
    bool operator>=( myrct x ) const { return ( compare( x ) >= 0 ) ? true : false; }
    operator bool() const { return ( !empty() ); }
    bool operator!() const { return empty(); }
    void event_queue_add( const std::string& strQueueName, size_t nQueueSize );  //+
    bool event_queue_remove( const std::string& strQueueName );                  //+
    size_t event_queue_remove_all();                                             //+
    bool event_add( const std::string& strQueueName );
    bool event_add( const std::string& strQueueName, size_t size );
    double compute_eps_from_start( const std::string& name, const time_point& tpNow ) const;
    double compute_eps(
        const std::string& name, const time_point& tpNow, size_t* p_nSummary = nullptr ) const;
    double compute_eps(
        t_NamedEventsIt& currentIt, const time_point& tpNow, size_t* p_nSummary = nullptr ) const;
    virtual std::string getEventStatsDescription( time_point tpNow, bool isColored = false,
        bool bSkipEmptyStats = true, bool bWithSummaryAsSuffix = false ) const;
    std::string getEventStatsDescription( bool isColored = false, bool bSkipEmptyStats = true,
        bool bWithSummaryAsSuffix = false ) const;
    virtual nlohmann::json toJSON(
        time_point tpNow, bool bSkipEmptyStats = true, const std::string& prefix = "items" ) const;
    virtual nlohmann::json toJSON( bool bSkipEmptyStats = true ) const;
    virtual std::set< std::string > all_queue_names() const;

protected:
    void init();

    mutable t_NamedEvents m_Events;
};  /// class named_event_stats

typedef size_t bytes_count_t;

struct traffic_record_item_t {
    typedef traffic_record_item_t myt;
    typedef myt& myrt;
    typedef const myt& myrct;
    time_point time_stamp_;
    bytes_count_t bytes_;
    myrt assign( myrct x );
    int compare( myrct x ) const;
    traffic_record_item_t();
    traffic_record_item_t( time_point a_time_stamp_, bytes_count_t a_bytes_ );
    traffic_record_item_t( bytes_count_t a_bytes_ );
    traffic_record_item_t( myrct x );
    ~traffic_record_item_t();
    myrt operator=( myrct x ) { return assign( x ); }
    bool operator==( myrct x ) const { return ( compare( x ) == 0 ) ? true : false; }
    bool operator!=( myrct x ) const { return ( compare( x ) != 0 ) ? true : false; }
    bool operator<( myrct x ) const { return ( compare( x ) < 0 ) ? true : false; }
    bool operator<=( myrct x ) const { return ( compare( x ) <= 0 ) ? true : false; }
    bool operator>( myrct x ) const { return ( compare( x ) > 0 ) ? true : false; }
    bool operator>=( myrct x ) const { return ( compare( x ) >= 0 ) ? true : false; }
};  /// struct traffic_record_item_t
typedef skutils::with_summary< bytes_count_t, skutils::round_queue< traffic_record_item_t > >
    traffic_queue_t;

// namespace helper functions
namespace named_traffic_stats {
double stat_compute_bps(
    const traffic_queue_t& qtr, time_point tpNow, bytes_count_t* p_nSummary = nullptr );
double stat_compute_bps_last_known(
    const traffic_queue_t& qtr, bytes_count_t* p_nSummary = nullptr );
double stat_compute_bps_til_now( const traffic_queue_t& qtr, bytes_count_t* p_nSummary = nullptr );
}  // namespace named_traffic_stats

};  // namespace stats
};  // namespace skutils

#endif  /// (!defined __SKUTILS_STATS_H)
