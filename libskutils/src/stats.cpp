#include <skutils/console_colors.h>
#include <skutils/stats.h>
#include <skutils/utils.h>

namespace skutils {
namespace stats {

event_record_item_t::event_record_item_t() : time_stamp_( clock::now() ) {}
event_record_item_t::event_record_item_t( time_point a_time_stamp_ )
    : time_stamp_( a_time_stamp_ ) {}
event_record_item_t::event_record_item_t( event_record_item_t::myrct x ) {
    assign( x );
}
event_record_item_t::~event_record_item_t() {}
event_record_item_t::myrt event_record_item_t::assign( event_record_item_t::myrct x ) {
    time_stamp_ = x.time_stamp_;
    return ( *this );
}
int event_record_item_t::compare( event_record_item_t::myrct x ) const {
    if ( time_stamp_ < x.time_stamp_ )
        return -1;
    if ( time_stamp_ > x.time_stamp_ )
        return 1;
    return 0;
}

named_event_stats::named_event_stats() {}
named_event_stats::named_event_stats( named_event_stats::myrct x ) {
    init();
    assign( x );
}
named_event_stats::named_event_stats( named_event_stats::myrrt x ) {
    init();
    move( x );
}
named_event_stats::~named_event_stats() {
    clear();
}

void named_event_stats::init() {
    clear();
}

bool named_event_stats::empty() const {
    return m_Events.empty();
}

void named_event_stats::clear() {
    m_Events.clear();
}

int named_event_stats::compare( named_event_stats::myrct x ) const {
    size_t cnt1 = m_Events.size();
    size_t cnt2 = x.m_Events.size();
    if ( cnt1 < cnt2 )
        return -1;
    if ( cnt1 > cnt2 )
        return 1;

    auto itWalk1 = m_Events.cbegin();
    auto itEnd1 = m_Events.cend();
    auto itWalk2 = x.m_Events.cbegin();
    auto itEnd2 = x.m_Events.cend();
    for ( ; itWalk1 != itEnd1 && itWalk2 != itEnd2; ++itWalk1, ++itWalk2 ) {
        std::string strQueueName1 = itWalk1->first;
        std::string strQueueName2 = itWalk2->first;
        int n = ::strcmp( strQueueName1.c_str(), strQueueName2.c_str() );
        if ( n != 0 )
            return n;
        const auto event1 = itWalk1->second;
        const auto event2 = itWalk2->second;

        if ( event1 < event2 )
            return -1;
        else if ( event1 > event2 )
            return 1;
        else
            return 0;
    }

    return 0;
}

named_event_stats::myrt named_event_stats::assign( named_event_stats::myrct x ) {
    clear();
    m_Events.insert( x.m_Events.cbegin(), x.m_Events.cend() );

    return ( *this );
}

named_event_stats::myrt named_event_stats::move( named_event_stats::myrt x ) {
    assign( x );
    x.clear();
    return ( *this );
}
void named_event_stats::lock() {}
void named_event_stats::unlock() {}


void named_event_stats::event_queue_add( const std::string& strQueueName, size_t nSize ) {
    lock_type lock( *this );
    auto itEvent = m_Events.find( strQueueName );
    if ( itEvent == std::end( m_Events ) ) {
        m_Events.emplace( std::piecewise_construct, std::forward_as_tuple( strQueueName ),
            std::forward_as_tuple( clock::now() ) );
    } else {
        if ( nSize == 0 ) {
            ++itEvent->second.m_prevPerSec;
            ++itEvent->second.m_total;
        } else {
            itEvent->second.m_prevPerSec += nSize;
            itEvent->second.m_total += itEvent->second.m_prevPerSec;
        }

        itEvent->second.m_lastTime = clock::now();
    }
}

bool named_event_stats::event_queue_remove( const std::string& strQueueName ) {
    lock_type lock( *this );
    auto itFind = m_Events.find( strQueueName ), itEnd = m_Events.end();
    if ( itFind == itEnd )
        return false;
    m_Events.erase( itFind );
    return true;
}

size_t named_event_stats::event_queue_remove_all() {
    lock_type lock( *this );
    size_t n = m_Events.size();
    clear();
    return n;
}

bool named_event_stats::event_add( const std::string& strQueueName ) {
    lock_type lock( *this );
    event_queue_add( strQueueName, 0 );
    return true;
}

bool named_event_stats::event_add( const std::string& strQueueName, size_t size ) {
    lock_type lock( *this );

    event_queue_add( strQueueName, size );
    return true;
}

double named_event_stats::compute_eps_from_start(
    const std::string& eventName, const time_point& tpNow ) const {
    auto itEvent = m_Events.find( eventName );
    if ( itEvent == std::end( m_Events ) )
        return 0.0;

    const auto duration_milliseconds = std::chrono::duration_cast< std::chrono::milliseconds >(
        tpNow - itEvent->second.m_startTime );
    const double seconds = static_cast< double >( duration_milliseconds.count() ) / 1000.0;
    const double epsResult = static_cast< double >( itEvent->second.m_total ) / seconds;

    return epsResult;
}

double named_event_stats::compute_eps(
    const std::string& eventName, const time_point& tpNow, size_t* p_nSummary ) const {
    auto itEvent = m_Events.find( eventName );
    if ( itEvent == std::end( m_Events ) )
        return 0.0;
    return compute_eps( itEvent, tpNow, p_nSummary );
}

double named_event_stats::compute_eps(
    t_NamedEventsIt& itEvent, const time_point& tpNow, size_t* p_nSummary ) const {
    const auto duration_milliseconds = std::chrono::duration_cast< std::chrono::milliseconds >(
        tpNow - itEvent->second.m_prevTime );
    const double seconds = static_cast< double >( duration_milliseconds.count() ) / 1000.0;
    const double epsResult = static_cast< double >( itEvent->second.m_prevPerSec ) / seconds;

    itEvent->second.m_prevTime = clock::now();
    itEvent->second.m_lastTime = clock::now();
    itEvent->second.m_prevPerSec = 0;
    itEvent->second.m_prevUnitsPerSecond = epsResult;
    if ( p_nSummary )
        *p_nSummary = itEvent->second.m_total;
    return epsResult;
}

std::string named_event_stats::getEventStatsDescription( time_point tpNow,
    bool isColored /*= false*/, bool bSkipEmptyStats /*= true*/,
    bool bWithSummaryAsSuffix /*= false*/ ) const {
    named_event_stats copy_of_this( *this );  // lock-free copy of this
    if ( copy_of_this.m_Events.empty() )
        return "";
    std::string strSpace( " " ), strCommaSpace( ", " ), strSuffixPerSecond( " p/s" );
    if ( isColored ) {
        strSpace = cc::debug( strSpace );
        strCommaSpace = cc::debug( strCommaSpace );
        strSuffixPerSecond = cc::debug( strSuffixPerSecond );
    }
    std::stringstream ss;
    bool isNotFirst = false;
    for ( auto itCurrent = std::begin( copy_of_this.m_Events );
          itCurrent != std::end( copy_of_this.m_Events ); ++itCurrent ) {
        size_t nSummary = 0;
        const double lfEPS = compute_eps( itCurrent, tpNow, &nSummary );
        if ( bSkipEmptyStats && tools::equal( lfEPS, 0.0 ) && nSummary == 0 )
            continue;

        std::string strEPS;
        std::string eventName = itCurrent->first;
        if ( bWithSummaryAsSuffix )
            strEPS = skutils::tools::format( "%.3lf(%" PRIu64 ")", lfEPS, uint64_t( nSummary ) );
        else
            strEPS = skutils::tools::format( "%.3lf", lfEPS );
        if ( isColored ) {
            eventName = cc::info( eventName );
            strEPS = cc::note( strEPS );
        }
        if ( isNotFirst )
            ss << strCommaSpace;
        ss << eventName << strSpace << strEPS << strSuffixPerSecond;
        isNotFirst = true;
    }

    return ss.str();
}

std::string named_event_stats::getEventStatsDescription( bool isColored /*= false*/,
    bool bSkipEmptyStats /*= true*/, bool bWithSummaryAsSuffix /*= false*/ ) const {
    return getEventStatsDescription(
        clock::now(), isColored, bSkipEmptyStats, bWithSummaryAsSuffix );
}

nlohmann::json named_event_stats::toJSON(
    time_point tpNow, bool bSkipEmptyStats /*= true*/, const std::string& prefix ) const {
    lock_type lock( const_cast< myrt >( *this ) );
    nlohmann::json jo = nlohmann::json::object();
    if ( !m_Events.empty() ) {
        nlohmann::json joEPSs = nlohmann::json::object();
        nlohmann::json joSummaries = nlohmann::json::object();
        nlohmann::json joEPSFromStart = nlohmann::json::object();
        size_t i = 0;
        for ( auto itCurrent = std::begin( m_Events ); itCurrent != std::end( m_Events );
              ++itCurrent ) {
            const std::string strQueueName = itCurrent->first;
            const double lfEPS = compute_eps( itCurrent, tpNow );
            if ( bSkipEmptyStats && tools::equal( lfEPS, 0.0 ) )
                continue;
            joEPSs[strQueueName] = lfEPS;
            joSummaries[strQueueName] = itCurrent->second.m_total;
            joEPSFromStart[strQueueName] = compute_eps_from_start( strQueueName, tpNow );
            ++i;
        }

        if ( i > 0 ) {
            jo[prefix] = joEPSs;
            jo[prefix + "_summaries"] = joSummaries;
            jo[prefix + "_per_sec_from_start"] = joEPSs;
        }
    }
    return jo;
}
nlohmann::json named_event_stats::toJSON( bool bSkipEmptyStats /*= false*/ ) const {
    return toJSON( clock::now(), bSkipEmptyStats );
}

std::set< std::string > named_event_stats::all_queue_names() const {
    std::set< std::string > setNames;
    named_event_stats copy_of_this{*this};  // lock-free copy of this
    for ( const auto& itName : copy_of_this.m_Events ) {
        setNames.insert( itName.first );
    }
    return setNames;
}

traffic_record_item_t::traffic_record_item_t() : time_stamp_( clock::now() ), bytes_( 0 ) {}
traffic_record_item_t::traffic_record_item_t( time_point a_time_stamp_, bytes_count_t a_bytes_ )
    : time_stamp_( a_time_stamp_ ), bytes_( a_bytes_ ) {}
traffic_record_item_t::traffic_record_item_t( bytes_count_t a_bytes_ )
    : time_stamp_( clock::now() ), bytes_( a_bytes_ ) {}
traffic_record_item_t::traffic_record_item_t( traffic_record_item_t::myrct x ) {
    assign( x );
}
traffic_record_item_t::~traffic_record_item_t() {}
traffic_record_item_t::myrt traffic_record_item_t::assign( traffic_record_item_t::myrct x ) {
    time_stamp_ = x.time_stamp_;
    bytes_ = x.bytes_;
    return ( *this );
}
int traffic_record_item_t::compare( traffic_record_item_t::myrct x ) const {
    if ( time_stamp_ < x.time_stamp_ )
        return -1;
    if ( time_stamp_ > x.time_stamp_ )
        return 1;
    if ( bytes_ < x.bytes_ )
        return -1;
    if ( bytes_ > x.bytes_ )
        return 1;
    return 0;
}

namespace named_traffic_stats {
double stat_compute_bps(
    const traffic_queue_t& qtr, time_point tpNow, bytes_count_t* p_nSummary /*= nullptr*/ ) {
    if ( p_nSummary )
        ( *p_nSummary ) = qtr.summary();
    if ( qtr.empty() )
        return 0.0;
    const traffic_record_item_t& from = qtr.front();  // , & to = qtr.back();
    const auto duration_milliseconds = std::chrono::duration_cast< std::chrono::milliseconds >(
        /*to.time_stamp_*/ /*clock::now()*/ tpNow - from.time_stamp_ );
    size_t cntMilliseconds = duration_milliseconds.count();
    if ( cntMilliseconds < 1 )
        return 0.0;  // cntMilliseconds = 1;
    bytes_count_t cntBytes = 0;
    size_t cnt = qtr.size();
    if ( cnt <= 1 ) {
        if ( cntMilliseconds < 100 )
            cntMilliseconds = 100;
    }
    for ( size_t i = 1; i < cnt; ++i ) {
        const traffic_record_item_t& rec = qtr[i];
        cntBytes += rec.bytes_;
    }
    if ( cntBytes < 1 )
        cntBytes = 1;
    double lfPPS = 1000.0 * double( cntBytes - 1 ) / double( cntMilliseconds );
    return lfPPS;
}

double stat_compute_bps_last_known(
    const traffic_queue_t& qtr, bytes_count_t* p_nSummary /*= nullptr*/ ) {
    if ( p_nSummary )
        ( *p_nSummary ) = 0;
    if ( qtr.empty() )
        return 0.0;
    const traffic_record_item_t& to = qtr.back();
    return stat_compute_bps( qtr, to.time_stamp_, p_nSummary );
}

double stat_compute_bps_til_now(
    const traffic_queue_t& qtr, bytes_count_t* p_nSummary /*= nullptr*/ ) {
    return stat_compute_bps( qtr, clock::now(), p_nSummary );
}

}  // namespace named_traffic_stats


};  // namespace stats
};  // namespace skutils
