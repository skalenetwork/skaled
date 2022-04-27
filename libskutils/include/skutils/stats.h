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
        : m_startTime{ clock::now() },
          m_prevTime{ time },
          m_lastTime{ std::move( time ) },
          m_prevPerSec{ 1 },
          m_total{ 1 },
          m_total1{ 1 },
          m_prevUnitsPerSecond{ 0.0 } {}
    UnitsPerSecond( time_point time, size_t prevCount, size_t totalCount )
        : m_startTime{ clock::now() },
          m_prevTime{ time },
          m_lastTime{ std::move( time ) },
          m_prevPerSec{ prevCount },
          m_total{ totalCount },
          m_total1{ 1 },
          m_prevUnitsPerSecond{ 0.0 } {}

    bool operator<( const UnitsPerSecond& unit1 ) const {
        return std::tie( m_startTime, m_prevTime, m_lastTime, m_prevPerSec, m_total, m_total1 ) <
               std::tie( unit1.m_startTime, unit1.m_prevTime, unit1.m_lastTime, unit1.m_prevPerSec,
                   unit1.m_total, unit1.m_total1 );
    }
    bool operator>( const UnitsPerSecond& unit1 ) const { return ( *this < unit1 ) ? false : true; }
    time_point m_startTime;
    time_point m_prevTime;
    time_point m_lastTime;
    size_t m_prevPerSec;
    size_t m_total;
    size_t m_total1;
    double m_prevUnitsPerSecond;
    typedef std::pair< time_point, double > history_item_t;
    typedef std::list< history_item_t > history_item_list_t;
    history_item_list_t m_history_per_second;
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
    //
    double compute_eps_from_start( const std::string& name, const time_point& tpNow ) const;
    double compute_eps( const std::string& name, const time_point& tpNow,
        size_t* p_nSummary = nullptr, size_t* p_nSummary1 = nullptr ) const;
    double compute_eps( t_NamedEventsIt& currentIt, const time_point& tpNow,
        size_t* p_nSummary = nullptr, size_t* p_nSummary1 = nullptr ) const;
    static size_t g_nUnitsPerSecondHistoryMaxSize;
    double compute_eps_smooth( const std::string& name, const time_point& tpNow,
        size_t* p_nSummary = nullptr, size_t* p_nSummary1 = nullptr ) const;
    double compute_eps_smooth( t_NamedEventsIt& currentIt, const time_point& tpNow,
        size_t* p_nSummary = nullptr, size_t* p_nSummary1 = nullptr ) const;
    //
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
};  // namespace named_traffic_stats

namespace time_tracker {

class element : public skutils::ref_retain_release {
public:
    typedef std::pair< skutils::stats::time_point, void* > id_t;

    typedef std::chrono::steady_clock clock;
    typedef std::chrono::time_point< clock > time_point;

    typedef std::chrono::duration< double, std::milli > duration;

private:
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;
    static mutex_type& mtx() { return skutils::get_ref_mtx(); }

    mutable volatile bool isError_ = false, isStopped_ = false;
    mutable std::string strSubSystem_, strProtocol_, strMethod_;

    mutable time_point tpStart_, tpEnd_;

    void do_register();
    void do_unregister();

public:
    static const char g_strMethodNameUnknown[];

    element( const char* strSubSystem, const char* strProtocol, const char* strMethod,
        int /*nServerIndex*/, int /*ipVer*/ );
    virtual ~element();
    void stop() const;
    void setMethod( const char* strMethod ) const;
    void setError() const;
    double getDurationInSeconds() const;
    id_t getID() { return id_t( tpStart_, this ); }
    const std::string& getProtocol() const { return strProtocol_; }
    const std::string& getMethod() const { return strMethod_; }
};  /// class element

typedef skutils::retain_release_ptr< element > element_ptr_t;

class queue : public skutils::ref_retain_release {
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;
    static mutex_type& mtx() { return skutils::get_ref_mtx(); }

    typedef std::map< element::id_t, element_ptr_t > map_rtte_t;  // rttID -> rttElement
    typedef std::map< std::string, map_rtte_t > map_pq_t;         // protocol name -> map_rtte_t
    map_pq_t map_pq_;

    size_t nMaxItemsInQueue = 100;

public:
    typedef std::map< std::string, skutils::retain_release_ptr< queue > >
        map_subsystem_time_trackers_t;

    queue();
    virtual ~queue();
    static queue& getQueueForSubsystem( const char* strSubSystem );
    void do_register( element_ptr_t& rttElement );
    void do_unregister( element_ptr_t& rttElement );
    std::list< std::string > getProtocols();
    nlohmann::json getProtocolStats( const char* strProtocol );
    nlohmann::json getAllStats();
};  /// class queue

};  // namespace time_tracker

};  // namespace stats
};  // namespace skutils

#endif  /// (!defined __SKUTILS_STATS_H)
