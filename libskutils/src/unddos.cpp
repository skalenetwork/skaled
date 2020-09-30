#include <skutils/unddos.h>

namespace skutils {
namespace unddos {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

origin_entry_setting::origin_entry_setting() {
    clear();
}

origin_entry_setting::origin_entry_setting( const origin_entry_setting& other ) {
    assign( other );
}

origin_entry_setting::origin_entry_setting( origin_entry_setting&& other ) {
    assign( other );
    other.clear();
}

origin_entry_setting::~origin_entry_setting() {
    clear();
}

origin_entry_setting& origin_entry_setting::operator=( const origin_entry_setting& other ) {
    assign( other );
    return ( *this );
}

bool origin_entry_setting::empty() const {
    if ( !origin_wildcard_.empty() )
        return false;
    return true;
}

void origin_entry_setting::clear() {
    origin_wildcard_.clear();
    maxCallsPerSecond_ = 0;
    maxCallsPerMinute_ = 0;
}

origin_entry_setting& origin_entry_setting::assign( const origin_entry_setting& other ) {
    if ( ( ( void* ) ( this ) ) == ( ( void* ) ( &other ) ) )
        return ( *this );
    clear();
    origin_wildcard_ = other.origin_wildcard_;
    maxCallsPerSecond_ = other.maxCallsPerSecond_;
    maxCallsPerMinute_ = other.maxCallsPerMinute_;
    return ( *this );
}

origin_entry_setting& origin_entry_setting::merge( const origin_entry_setting& other ) {
    if ( ( ( void* ) ( this ) ) == ( ( void* ) ( &other ) ) )
        return ( *this );
    if ( origin_wildcard_ != other.origin_wildcard_ )
        return ( *this );
    maxCallsPerSecond_ = std::min( maxCallsPerSecond_, other.maxCallsPerSecond_ );
    maxCallsPerMinute_ = std::min( maxCallsPerMinute_, other.maxCallsPerMinute_ );
    return ( *this );
}

void origin_entry_setting::fromJSON( const nlohmann::json& jo ) {
    clear();
    if ( jo.find( "origin" ) != jo.end() )
        origin_wildcard_ = jo["origin"].get< std::string >();
    if ( jo.find( "maxCallsPerSecond" ) != jo.end() )
        maxCallsPerSecond_ = jo["maxCallsPerSecond"].get< size_t >();
    if ( jo.find( "maxCallsPerMinute" ) != jo.end() )
        maxCallsPerMinute_ = jo["maxCallsPerMinute"].get< size_t >();
}

void origin_entry_setting::toJSON( nlohmann::json& jo ) const {
    jo = nlohmann::json::object();
    jo["origin"] = origin_wildcard_;
    jo["maxCallsPerSecond"] = maxCallsPerSecond_;
    jo["maxCallsPerMinute"] = maxCallsPerMinute_;
}

bool origin_entry_setting::match_origin( const char* origin ) const {
    if ( origin == nullptr || ( *origin ) == '\0' )
        return false;
    if ( !skutils::tools::wildcmp( origin_wildcard_.c_str(), origin ) )
        return false;
    return true;
}
bool origin_entry_setting::match_origin( const std::string& origin ) const {
    return match_origin( origin.c_str() );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

settings::settings() {
    clear();
}

settings::settings( const settings& other ) {
    assign( other );
}

settings::settings( settings&& other ) {
    assign( other );
    other.clear();
}

settings::~settings() {
    clear();
}

settings& settings::operator=( const settings& other ) {
    assign( other );
    return ( *this );
}

bool settings::empty() const {
    if ( !origins_.empty() )
        return false;
    return true;
}

void settings::clear() {
    origins_.clear();
    maxCallsPerSecond_ = 0;
    maxCallsPerMinute_ = 0;
}

settings& settings::assign( const settings& other ) {
    if ( ( ( void* ) ( this ) ) == ( ( void* ) ( &other ) ) )
        return ( *this );
    clear();
    origins_ = other.origins_;
    maxCallsPerSecond_ = other.maxCallsPerSecond_;
    maxCallsPerMinute_ = other.maxCallsPerMinute_;
    return ( *this );
}

settings& settings::merge( const settings& other ) {
    if ( ( ( void* ) ( this ) ) == ( ( void* ) ( &other ) ) )
        return ( *this );
    maxCallsPerSecond_ = std::min( maxCallsPerSecond_, other.maxCallsPerSecond_ );
    maxCallsPerMinute_ = std::min( maxCallsPerMinute_, other.maxCallsPerMinute_ );
    for ( const origin_entry_setting& oe : other.origins_ )
        merge( oe );
    return ( *this );
}
settings& settings::merge( const origin_entry_setting& oe ) {
    size_t i = indexOfOrigin( oe );
    if ( i == std::string::npos )
        origins_.push_back( oe );
    else
        origins_[i].merge( oe );
    return ( *this );
}

size_t settings::indexOfOrigin( const origin_entry_setting& oe, size_t idxStart ) {
    return indexOfOrigin( oe.origin_wildcard_, idxStart );
}
size_t settings::indexOfOrigin( const char* origin_wildcard, size_t idxStart ) {
    if ( origin_wildcard == nullptr || ( *origin_wildcard ) == '\0' )
        return std::string::npos;
    size_t cnt = origins_.size();
    size_t i = ( idxStart == std::string::npos ) ? 0 : ( idxStart + 1 );
    for ( ; i < cnt; ++i ) {
        const origin_entry_setting& oe = origins_[i];
        if ( oe.origin_wildcard_ == origin_wildcard )
            return i;
    }
    return std::string::npos;
}
size_t settings::indexOfOrigin( const std::string& origin_wildcard, size_t idxStart ) {
    if ( origin_wildcard.empty() )
        return std::string::npos;
    return indexOfOrigin( origin_wildcard.c_str(), idxStart );
}

void settings::fromJSON( const nlohmann::json& jo ) {
    clear();
    if ( jo.find( "origins" ) != jo.end() ) {
        const nlohmann::json& joOrigins = jo["origins"];
        if ( joOrigins.is_array() ) {
            for ( const nlohmann::json& joOrigin : joOrigins ) {
                origin_entry_setting oe;
                oe.fromJSON( joOrigin );
                origins_.push_back( oe );
            }
        }
    }
    if ( jo.find( "maxCallsPerSecond" ) != jo.end() )
        maxCallsPerSecond_ = jo["maxCallsPerSecond"].get< size_t >();
    if ( jo.find( "maxCallsPerMinute" ) != jo.end() )
        maxCallsPerMinute_ = jo["maxCallsPerMinute"].get< size_t >();
}

void settings::toJSON( nlohmann::json& jo ) const {
    jo = nlohmann::json::object();
    nlohmann::json joOrigins = nlohmann::json::array();
    for ( const origin_entry_setting& oe : origins_ ) {
        nlohmann::json joOrigin = nlohmann::json::object();
        oe.toJSON( joOrigin );
        joOrigin.push_back( joOrigin );
    }
    jo["origins"] = joOrigins;
    jo["maxCallsPerSecond"] = maxCallsPerSecond_;
    jo["maxCallsPerMinute"] = maxCallsPerMinute_;
}

size_t settings::find_origin_entry_setting_match( const char* origin, size_t idxStart ) const {
    if ( origin == nullptr || ( *origin ) == '\0' )
        return std::string::npos;
    size_t cnt = origins_.size();
    size_t i = ( idxStart == std::string::npos ) ? 0 : ( idxStart + 1 );
    for ( ; i < cnt; ++i ) {
        const origin_entry_setting& oe = origins_[i];
        if ( oe.match_origin( origin ) )
            return i;
    }
    return std::string::npos;
}
size_t settings::find_origin_entry_setting_match(
    const std::string& origin, size_t idxStart ) const {
    return find_origin_entry_setting_match( origin.c_str(), idxStart );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

time_entry::time_entry( time_tick_mark ttm ) : ttm_( ttm ) {}

time_entry::time_entry( const time_entry& other ) {
    assign( other );
}

time_entry::time_entry( time_entry&& other ) {
    assign( other );
    other.clear();
}

time_entry::~time_entry() {
    clear();
}

time_entry& time_entry::operator=( const time_entry& other ) {
    return assign( other );
}

bool time_entry::empty() const {
    if ( ttm_ != time_tick_mark( 0 ) )
        return false;
    return true;
}

void time_entry::clear() {
    ttm_ = time_tick_mark( 0 );
}

time_entry& time_entry::assign( const time_entry& other ) {
    clear();
    ttm_ = other.ttm_;
    return ( *this );
}

int time_entry::compare( const time_entry& other ) const {
    if ( ttm_ < other.ttm_ )
        return -1;
    if ( ttm_ > other.ttm_ )
        return 1;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

tracked_origin::tracked_origin( const char* origin, time_tick_mark ttm )
    : origin_( ( origin != nullptr && origin[0] != '\0' ) ? origin : "" ) {
    if ( ttm != time_tick_mark( 0 ) )
        time_entries_.push_back( time_entry( ttm ) );
}

tracked_origin::tracked_origin( const std::string& origin, time_tick_mark ttm )
    : origin_( origin ) {
    if ( ttm != time_tick_mark( 0 ) )
        time_entries_.push_back( time_entry( ttm ) );
}

tracked_origin::tracked_origin( const tracked_origin& other ) {
    assign( other );
}

tracked_origin::tracked_origin( tracked_origin&& other ) {
    assign( other );
    other.clear();
}

tracked_origin::~tracked_origin() {
    clear();
}

tracked_origin& tracked_origin::operator=( const tracked_origin& other ) {
    return assign( other );
}

bool tracked_origin::empty() const {
    if ( !origin_.empty() )
        return false;
    if ( !time_entries_.empty() )
        return false;
    return true;
}

void tracked_origin::clear() {
    origin_.clear();
    time_entries_.clear();
}

tracked_origin& tracked_origin::assign( const tracked_origin& other ) {
    clear();
    origin_ = other.origin_;
    time_entries_ = other.time_entries_;
    return ( *this );
}

int tracked_origin::compare( const tracked_origin& other ) const {
    int n = origin_.compare( other.origin_ );
    return n;
}
int tracked_origin::compare( const char* origin ) const {
    int n = origin_.compare( origin ? origin : "" );
    return n;
}
int tracked_origin::compare( const std::string& origin ) const {
    int n = origin_.compare( origin );
    return n;
}

size_t tracked_origin::unload_old_data_by_time_to_past(
    time_tick_mark ttmNow, duration durationToPast ) {
    if ( durationToPast == duration( 0 ) )
        return 0;
    adjust_now_tick_mark( ttmNow );
    time_tick_mark ttmUntil = ttmNow - durationToPast;
    size_t cntRemoved = 0;
    adjust_now_tick_mark( ttmNow );
    while ( !time_entries_.empty() ) {
        const time_entry& te = time_entries_.front();
        if ( te.ttm_ < ttmUntil ) {
            time_entries_.pop_front();
            ++cntRemoved;
        }
    }
    return cntRemoved;
}

size_t tracked_origin::unload_old_data_by_count( size_t cntEntriesMax ) {
    size_t cntRemoved = 0;
    while ( time_entries_.size() > cntEntriesMax ) {
        time_entries_.pop_front();
        ++cntRemoved;
    }
    return cntRemoved;
}

size_t tracked_origin::count_to_past( time_tick_mark ttmNow, duration durationToPast ) const {
    // if ( durationToPast == duration( 0 ) )
    //    return 0;
    adjust_now_tick_mark( ttmNow );
    time_tick_mark ttmUntil = ttmNow - durationToPast;
    size_t cnt = 0;
    time_entries_t::const_reverse_iterator itWalk = time_entries_.crbegin(),
                                           itEnd = time_entries_.crend();
    for ( ; itWalk != itEnd; ++itWalk ) {
        const time_entry& te = ( *itWalk );
        if ( ttmUntil <= te.ttm_ && te.ttm_ <= ttmNow )
            ++cnt;
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

algorithm::algorithm() {}

algorithm::algorithm( const settings& st ) {
    settings_ = st;
}

algorithm::~algorithm() {}

algorithm& algorithm::operator=( const settings& st ) {
    lock_type lock( mtx_ );
    settings_ = st;
    return ( *this );
}

size_t algorithm::unload_old_data_by_time_to_past(
    time_tick_mark ttmNow, duration durationToPast ) {
    if ( durationToPast == duration( 0 ) )
        return 0;
    adjust_now_tick_mark( ttmNow );
    lock_type lock( mtx_ );
    size_t cnt = 0;
    std::set< std::string > setOriginsTorRemove;
    tracked_origins_t::iterator itWalk = tracked_origins_.begin(), itEnd = tracked_origins_.end();
    for ( ; itWalk != itEnd; ++itWalk ) {
        tracked_origin& to = const_cast< tracked_origin& >( *itWalk );
        size_t cntWalk = to.unload_old_data_by_time_to_past( ttmNow, durationToPast );
        cnt += cntWalk;
        if ( to.time_entries_.empty() ) {
            setOriginsTorRemove.insert( to.origin_ );  // TO-FIX: do not unload banned
        }
    }
    for ( const std::string& origin : setOriginsTorRemove )
        tracked_origins_.erase( origin );
    return cnt;
}

e_high_load_detection_result_t algorithm::register_call_from_origin(
    const char* origin, time_tick_mark ttmNow, duration durationToPast ) {
    if ( origin == nullptr || origin[0] == '\0' )
        return e_high_load_detection_result_t::ehldr_bad_origin;
    adjust_now_tick_mark( ttmNow );
    lock_type lock( mtx_ );
    unload_old_data_by_time_to_past( ttmNow, durationToPast );  // unload first
    tracked_origins_t::iterator itFind = tracked_origins_.find( origin ),
                                itEnd = tracked_origins_.end();
    if ( itFind == itEnd ) {
        tracked_origin to( origin, ttmNow );
        return e_high_load_detection_result_t::ehldr_no_error;
    }
    // return detect_high_load( origin.ttmNow, durationToPast )
    tracked_origin& to = const_cast< tracked_origin& >( *itFind );
    to.time_entries_.push_back( time_entry( ttmNow ) );
    if ( to.unload_old_data_by_count( settings_.maxCallsPerMinute_ ) > 0 )
        return e_high_load_detection_result_t::ehldr_peak;  // ban by too high load per minute
    if ( to.count_to_past( ttmNow, 1 ) > settings_.maxCallsPerSecond_ )
        return e_high_load_detection_result_t::ehldr_lengthy;  // ban by too high load per second
    return e_high_load_detection_result_t::ehldr_no_error;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace unddos
};  // namespace skutils
