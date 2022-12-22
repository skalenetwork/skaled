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

void origin_entry_setting::load_defaults_for_any_origin() {
    load_friendly_for_any_origin();
    // load_reasonable_for_any_origin();
}

void origin_entry_setting::load_friendly_for_any_origin() {
    clear();
    origin_wildcards_.push_back( "*" );
    max_calls_per_second_ = 500;
    max_calls_per_minute_ = 15000;
    ban_peak_ = duration( 15 );
    ban_lengthy_ = duration( 120 );
    max_ws_conn_ = 50;
    load_recommended_custom_methods_as_multiplier_of_default();
}

void origin_entry_setting::load_reasonable_for_any_origin() {
    clear();
    origin_wildcards_.push_back( "*" );
    max_calls_per_second_ = 100;
    max_calls_per_minute_ = 5000;
    ban_peak_ = duration( 15 );
    ban_lengthy_ = duration( 120 );
    max_ws_conn_ = 10;
    load_recommended_custom_methods_as_multiplier_of_default();
}

void origin_entry_setting::load_unlim_for_any_origin() {
    clear();
    origin_wildcards_.push_back( "*" );
    max_calls_per_second_ = std::numeric_limits< size_t >::max();
    max_calls_per_minute_ = std::numeric_limits< size_t >::max();
    ban_peak_ = duration( 0 );
    ban_lengthy_ = duration( 0 );
    max_ws_conn_ = std::numeric_limits< size_t >::max();
    load_recommended_custom_methods_as_multiplier_of_default();
}

void origin_entry_setting::load_unlim_for_localhost_only() {
    clear();
    origin_wildcards_.push_back( "127.0.0.*" );
    origin_wildcards_.push_back( "::1" );
    max_calls_per_second_ = std::numeric_limits< size_t >::max();
    max_calls_per_minute_ = std::numeric_limits< size_t >::max();
    ban_peak_ = duration( 0 );
    ban_lengthy_ = duration( 0 );
    max_ws_conn_ = std::numeric_limits< size_t >::max();
    load_recommended_custom_methods_as_multiplier_of_default();
}

void origin_entry_setting::load_custom_method_as_multiplier_of_default(
    const char* strMethod, double lfMultiplier ) {
    if ( strMethod == nullptr || strMethod[0] == '\0' || lfMultiplier <= 0.0 )
        return;
    custom_method_setting cme;
    cme.max_calls_per_second_ = size_t( max_calls_per_second_ * lfMultiplier );
    cme.max_calls_per_minute_ = size_t( max_calls_per_minute_ * lfMultiplier );
    map_custom_method_settings_[strMethod] = cme;
}

void origin_entry_setting::load_recommended_custom_methods_as_multiplier_of_default(
    double lfMultiplier ) {
    static const char* g_arr[] = { "web3_clientVersion", "web3_sha3", "net_version", "eth_syncing",
        "eth_protocolVersion", "eth_gasPrice", "eth_blockNumber", "eth_getBalance",
        "eth_getBlockByHash", "eth_getBlockByNumber", "eth_getTransactionCount",
        "eth_getTransactionReceipt", "eth_getTransactionByHash",
        "eth_getTransactionByBlockHashAndIndex", "eth_getTransactionByBlockNumberAndIndex" };
    for ( size_t i = 0; i < sizeof( g_arr ) / sizeof( g_arr[0] ); ++i )
        load_custom_method_as_multiplier_of_default( g_arr[i], lfMultiplier );
}


bool origin_entry_setting::empty() const {
    if ( !origin_wildcards_.empty() )
        return false;
    return true;
}

void origin_entry_setting::clear() {
    origin_wildcards_.clear();
    max_calls_per_second_ = 0;
    max_calls_per_minute_ = 0;
    ban_peak_ = duration( 0 );
    ban_lengthy_ = duration( 0 );
    max_ws_conn_ = 0;
    map_custom_method_settings_.clear();
}

origin_entry_setting& origin_entry_setting::assign( const origin_entry_setting& other ) {
    if ( ( ( void* ) ( this ) ) == ( ( void* ) ( &other ) ) )
        return ( *this );
    clear();
    origin_wildcards_ = other.origin_wildcards_;
    max_calls_per_second_ = other.max_calls_per_second_;
    max_calls_per_minute_ = other.max_calls_per_minute_;
    ban_peak_ = other.ban_peak_;
    ban_lengthy_ = other.ban_lengthy_;
    max_ws_conn_ = other.max_ws_conn_;
    map_custom_method_settings_ = other.map_custom_method_settings_;
    return ( *this );
}

origin_entry_setting& origin_entry_setting::merge( const origin_entry_setting& other ) {
    if ( ( ( void* ) ( this ) ) == ( ( void* ) ( &other ) ) )
        return ( *this );
    if ( origin_wildcards_ != other.origin_wildcards_ )
        return ( *this );
    max_calls_per_second_ = std::min( max_calls_per_second_, other.max_calls_per_second_ );
    max_calls_per_minute_ = std::min( max_calls_per_minute_, other.max_calls_per_minute_ );
    ban_peak_ = std::max( ban_peak_, other.ban_peak_ );
    ban_lengthy_ = std::max( ban_lengthy_, other.ban_lengthy_ );
    max_ws_conn_ = std::min( max_ws_conn_, other.max_ws_conn_ );
    if ( !other.map_custom_method_settings_.empty() ) {
        nlohmann::json joCMS = nlohmann::json::object();
        map_custom_method_settings_t::const_iterator itWalk =
                                                         other.map_custom_method_settings_.cbegin(),
                                                     itEnd =
                                                         other.map_custom_method_settings_.cend();
        for ( ; itWalk != itEnd; ++itWalk ) {
            const custom_method_setting& cme = itWalk->second;
            map_custom_method_settings_t::iterator itFind =
                map_custom_method_settings_.find( itWalk->first );
            if ( itFind != map_custom_method_settings_.end() ) {
                itFind->second.merge( cme );  // merge with existing
                continue;
            }
            map_custom_method_settings_[itWalk->first] = cme;  // add mew
        }
    }
    return ( *this );
}

void origin_entry_setting::fromJSON( const nlohmann::json& jo ) {
    clear();
    if ( jo.find( "origin" ) != jo.end() ) {
        nlohmann::json jarrWildcards = jo["origin"];
        if ( jarrWildcards.is_string() )
            origin_wildcards_.push_back( jarrWildcards.get< std::string >() );
        else if ( jarrWildcards.is_array() ) {
            for ( const nlohmann::json& joWildcard : jarrWildcards ) {
                if ( joWildcard.is_string() )
                    origin_wildcards_.push_back( joWildcard.get< std::string >() );
            }
        }
    }
    if ( jo.find( "max_calls_per_second" ) != jo.end() )
        max_calls_per_second_ = jo["max_calls_per_second"].get< size_t >();
    if ( jo.find( "max_calls_per_minute" ) != jo.end() )
        max_calls_per_minute_ = jo["max_calls_per_minute"].get< size_t >();
    if ( jo.find( "ban_peak" ) != jo.end() )
        ban_peak_ = jo["ban_peak"].get< size_t >();
    if ( jo.find( "ban_lengthy" ) != jo.end() )
        ban_lengthy_ = jo["ban_lengthy"].get< size_t >();
    if ( jo.find( "max_ws_conn" ) != jo.end() )
        max_ws_conn_ = jo["max_ws_conn"].get< size_t >();
    if ( jo.find( "custom_method_settings" ) != jo.end() ) {
        const nlohmann::json& joCMS = jo["custom_method_settings"];
        for ( auto it = joCMS.cbegin(); it != joCMS.cend(); ++it ) {
            const nlohmann::json& joMethod = it.value();
            custom_method_setting cme;
            if ( joMethod.find( "max_calls_per_second" ) != jo.end() )
                cme.max_calls_per_second_ = joMethod["max_calls_per_second"].get< size_t >();
            if ( joMethod.find( "max_calls_per_minute" ) != jo.end() )
                cme.max_calls_per_minute_ = joMethod["max_calls_per_minute"].get< size_t >();
            map_custom_method_settings_[it.key()] = cme;
        }
    }
}

void origin_entry_setting::toJSON( nlohmann::json& jo ) const {
    jo = nlohmann::json::object();
    nlohmann::json jarrWildcards = nlohmann::json::array();
    for ( const std::string& wildcard : origin_wildcards_ )
        jarrWildcards.push_back( wildcard );
    jo["origin"] = jarrWildcards;
    jo["max_calls_per_second"] = max_calls_per_second_;
    jo["max_calls_per_minute"] = max_calls_per_minute_;
    jo["ban_peak"] = ban_peak_;
    jo["ban_lengthy"] = ban_lengthy_;
    jo["max_ws_conn"] = max_ws_conn_;
    if ( !map_custom_method_settings_.empty() ) {
        nlohmann::json joCMS = nlohmann::json::object();
        map_custom_method_settings_t::const_iterator itWalk = map_custom_method_settings_.cbegin(),
                                                     itEnd = map_custom_method_settings_.cend();
        for ( ; itWalk != itEnd; ++itWalk ) {
            const custom_method_setting& cme = itWalk->second;
            nlohmann::json joMethod = nlohmann::json::object();
            joMethod["max_calls_per_second"] = cme.max_calls_per_second_;
            joMethod["max_calls_per_minute"] = cme.max_calls_per_minute_;
            joCMS[itWalk->first] = joMethod;
        }
        jo["custom_method_settings"] = joCMS;
    }
}

bool origin_entry_setting::match_origin( const char* origin ) const {
    if ( origin == nullptr || ( *origin ) == '\0' )
        return false;
    for ( const std::string& wildcard : origin_wildcards_ ) {
        if ( skutils::tools::wildcmp( wildcard.c_str(), origin ) )
            return true;
    }
    return false;
}
bool origin_entry_setting::match_origin( const std::string& origin ) const {
    return match_origin( origin.c_str() );
}

size_t origin_entry_setting::max_calls_per_second( const char* strMethod ) const {
    if ( strMethod == nullptr || strMethod[0] == '\0' )
        return max_calls_per_second_;
    map_custom_method_settings_t::const_iterator itFind =
                                                     map_custom_method_settings_.find( strMethod ),
                                                 itEnd = map_custom_method_settings_.cend();
    if ( itFind == itEnd )
        return max_calls_per_second_;
    const custom_method_setting& cme = itFind->second;
    const size_t cnt = std::max( max_calls_per_second_, cme.max_calls_per_second_ );
    return cnt;
}

size_t origin_entry_setting::max_calls_per_minute( const char* strMethod ) const {
    if ( strMethod == nullptr || strMethod[0] == '\0' )
        return max_calls_per_minute_;
    map_custom_method_settings_t::const_iterator itFind =
                                                     map_custom_method_settings_.find( strMethod ),
                                                 itEnd = map_custom_method_settings_.cend();
    if ( itFind == itEnd )
        return max_calls_per_minute_;
    const custom_method_setting& cme = itFind->second;
    const size_t cnt = std::max( max_calls_per_minute_, cme.max_calls_per_minute_ );
    return cnt;
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
    if ( !enabled_ )
        return true;
    if ( !origins_.empty() )
        return false;
#if ( defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__ )
    if ( !global_limit_.empty() )
        return false;
#endif  // (defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__)
    return true;
}

void settings::clear() {
    enabled_ = true;
    origins_.clear();
#if ( defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__ )
    global_limit_.clear();
#endif  // (defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__)
}

settings& settings::assign( const settings& other ) {
    if ( ( ( void* ) ( this ) ) == ( ( void* ) ( &other ) ) )
        return ( *this );
    clear();
    enabled_ = other.enabled_;
    origins_ = other.origins_;
#if ( defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__ )
    global_limit_ = other.global_limit_;
#endif  // (defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__)
    return ( *this );
}

settings& settings::merge( const settings& other ) {
    if ( ( ( void* ) ( this ) ) == ( ( void* ) ( &other ) ) )
        return ( *this );
    for ( const origin_entry_setting& oe : other.origins_ )
        merge( oe );
#if ( defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__ )
    global_limit_.merge( other.global_limit_ );
#endif  // (defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__)
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
    for ( const std::string& wildcard : oe.origin_wildcards_ ) {
        size_t i = indexOfOrigin( wildcard, idxStart );
        if ( i != std::string::npos )
            return i;
    }
    return std::string::npos;
}
size_t settings::indexOfOrigin( const char* origin_wildcard, size_t idxStart ) {
    if ( origin_wildcard == nullptr || ( *origin_wildcard ) == '\0' )
        return std::string::npos;
    size_t cnt = origins_.size();
    size_t i = ( idxStart == std::string::npos ) ? 0 : ( idxStart + 1 );
    for ( ; i < cnt; ++i ) {
        const origin_entry_setting& oe = origins_[i];
        for ( const std::string& wildcard : oe.origin_wildcards_ ) {
            if ( wildcard == origin_wildcard )
                return i;
        }
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
#if ( defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__ )
    if ( jo.find( "global" ) != jo.end() ) {
        const nlohmann::json& joGlobalLimit = jo["global"];
        origin_entry_setting oe;
        oe.fromJSON( joGlobalLimit );
        global_limit_ = oe;
    }
#endif  // (defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__)
    bool isEnabled = true;
    if ( jo.find( "enabled" ) != jo.end() ) {
        const nlohmann::json& joEnabled = jo["enabled"];
        if ( joEnabled.is_boolean() )
            isEnabled = joEnabled.get< bool >();
    }
    enabled_ = isEnabled;
}

void settings::toJSON( nlohmann::json& jo ) const {
    jo = nlohmann::json::object();
    nlohmann::json joOrigins = nlohmann::json::array();
    for ( const origin_entry_setting& oe : origins_ ) {
        nlohmann::json joOrigin = nlohmann::json::object();
        oe.toJSON( joOrigin );
        joOrigins.push_back( joOrigin );
    }
#if ( defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__ )
    nlohmann::json joGlobalLimit = nlohmann::json::object();
    global_limit_.toJSON( joGlobalLimit );
#endif  // (defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__)
    jo["enabled"] = enabled_;
    jo["origins"] = joOrigins;
#if ( defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__ )
    jo["global"] = joGlobalLimit;
#endif  // (defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__)
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

origin_entry_setting& settings::find_origin_entry_setting( const char* origin ) {
    size_t i = find_origin_entry_setting_match( origin );
    if ( i != std::string::npos )
        return origins_[i];
    return auto_append_any_origin_rule();
}

origin_entry_setting& settings::auto_append_any_origin_rule() {
    if ( !origins_.empty() ) {
        size_t i = find_origin_entry_setting_match( "*" );
        if ( i != std::string::npos )
            return origins_[i];
    }
    origin_entry_setting oe;
    oe.load_defaults_for_any_origin();
    origins_.push_back( oe );
    return origins_[origins_.size() - 1];
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
    clear_ban();
}

tracked_origin& tracked_origin::assign( const tracked_origin& other ) {
    clear();
    origin_ = other.origin_;
    time_entries_ = other.time_entries_;
    ban_until_ = other.ban_until_;
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
        } else
            break;
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

size_t tracked_origin::count_to_past(
    time_tick_mark ttmNow, duration durationToPast, size_t cndOptimizedMaxSteps ) const {
    // if ( durationToPast == duration( 0 ) )
    //    return 0;
    if ( cndOptimizedMaxSteps == 0 )
        return 0;
    adjust_now_tick_mark( ttmNow );
    time_tick_mark ttmUntil = ttmNow - durationToPast;
    size_t cnt = 0;
    bool bNeedReScaling = false;
    time_tick_mark ttRescaling = ttmUntil;
    time_entries_t::const_reverse_iterator itWalk = time_entries_.crbegin(),
                                           itEnd = time_entries_.crend();
    for ( size_t idxStep = 0; itWalk != itEnd; ++itWalk, ++idxStep ) {
        const time_entry& te = ( *itWalk );
        if ( ttmUntil <= te.ttm_ && te.ttm_ <= ttmNow ) {
            ++cnt;
            ttRescaling = te.ttm_ - ttmUntil;
        }
        if ( idxStep >= cndOptimizedMaxSteps ) {
            bNeedReScaling = true;
            break;
        }
    }
    if ( bNeedReScaling && cnt > 0 && ttRescaling > 0 ) {
        cnt *= durationToPast;
        cnt /= ttRescaling;
    }
    return cnt;
}

bool tracked_origin::clear_ban() {
    if ( ban_until_ == time_tick_mark( 0 ) )
        return false;
    ban_until_ = time_tick_mark( 0 );
    return true;  // was cleared
}

bool tracked_origin::check_ban( time_tick_mark ttmNow, bool isAutoClear ) {
    if ( ban_until_ == time_tick_mark( 0 ) )
        return false;
    adjust_now_tick_mark( ttmNow );
    if ( ttmNow <= ban_until_ )
        return true;
    if ( isAutoClear )
        clear_ban();
    return false;
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
    if ( !settings_.enabled_ )
        return 0;
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
#if ( defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__ )
    tracked_global_.unload_old_data_by_time_to_past( ttmNow, durationToPast );
#endif  // (defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__)
    return cnt;
}

e_high_load_detection_result_t algorithm::register_call_from_origin(
    const char* origin, const char* strMethod, time_tick_mark ttmNow, duration durationToPast ) {
    if ( !settings_.enabled_ )
        return e_high_load_detection_result_t::ehldr_no_error;
    if ( origin == nullptr || origin[0] == '\0' )
        return e_high_load_detection_result_t::ehldr_bad_origin;
    adjust_now_tick_mark( ttmNow );
    lock_type lock( mtx_ );
    //
#if ( defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__ )
    tracked_global_.time_entries_.push_back( time_entry( ttmNow ) );
    if ( tracked_global_.check_ban( ttmNow ) )
        return e_high_load_detection_result_t::ehldr_ban;  // still banned
#endif  // (defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__)
    //
    unload_old_data_by_time_to_past( ttmNow, durationToPast );  // unload first
    tracked_origins_t::iterator itFind = tracked_origins_.find( origin ),
                                itEnd = tracked_origins_.end();
    if ( itFind == itEnd ) {
        tracked_origin to( origin, ttmNow );
        tracked_origins_.insert( to );
        return e_high_load_detection_result_t::ehldr_no_error;
    }
    // return detect_high_load( origin.ttmNow, durationToPast )
    tracked_origin& to = const_cast< tracked_origin& >( *itFind );
    to.time_entries_.push_back( time_entry( ttmNow ) );
    if ( to.check_ban( ttmNow ) )
        return e_high_load_detection_result_t::ehldr_ban;  // still banned
    const origin_entry_setting& oe = settings_.find_origin_entry_setting( origin );
    // const size_t cntUnloaded = to.unload_old_data_by_count( oe.max_calls_per_minute( strMethod )
    // ); if ( cntUnloaded > oe.max_calls_per_minute_ ) {
    //    to.ban_until_ = ttmNow + oe.ban_lengthy_;
    //    return e_high_load_detection_result_t::ehldr_peak;  // ban by too high load per minute
    //}
    size_t nMaxCallsPerTimeUnit = oe.max_calls_per_minute( strMethod );
    if ( nMaxCallsPerTimeUnit > 0 ) {
        size_t cntPast = to.count_to_past( ttmNow, durationToPast, cntOptimizedMaxSteps4cm_ );
        if ( cntPast > nMaxCallsPerTimeUnit ) {
            to.ban_until_ = ttmNow + oe.ban_lengthy_;
            return e_high_load_detection_result_t::ehldr_lengthy;  // ban by too high load per
                                                                   // second
        }
    }
    nMaxCallsPerTimeUnit = oe.max_calls_per_second( strMethod );
    if ( nMaxCallsPerTimeUnit > 0 ) {
        size_t cntPast = to.count_to_past( ttmNow, 1, cntOptimizedMaxSteps4cs_ );
        if ( cntPast > nMaxCallsPerTimeUnit ) {
            to.ban_until_ = ttmNow + oe.ban_peak_;
            return e_high_load_detection_result_t::ehldr_peak;  // ban by too high load per second
        }
    }
    //
    //
#if ( defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__ )
    nMaxCallsPerTimeUnit = settings_.global_limit_.max_calls_per_minute( strMethod );
    if ( nMaxCallsPerTimeUnit > 0 ) {
        size_t cntPast =
            tracked_global_.count_to_past( ttmNow, durationToPast, cntOptimizedMaxSteps4gm_ );
        if ( cntPast > nMaxCallsPerTimeUnit ) {
            tracked_global_.ban_until_ = ttmNow + settings_.global_limit_.ban_lengthy_;
            return e_high_load_detection_result_t::ehldr_lengthy;  // ban by too high load per
                                                                   // second
        }
    }
    nMaxCallsPerTimeUnit = settings_.global_limit_.max_calls_per_second( strMethod );
    if ( nMaxCallsPerTimeUnit > 0 ) {
        size_t cntPast = tracked_global_.count_to_past( ttmNow, 1, cntOptimizedMaxSteps4gs_ );
        if ( cntPast > nMaxCallsPerTimeUnit ) {
            tracked_global_.ban_until_ = ttmNow + settings_.global_limit_.ban_peak_;
            return e_high_load_detection_result_t::ehldr_peak;  // ban by too high load per second
        }
    }
#endif  // (defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__)
    //
    //
    return e_high_load_detection_result_t::ehldr_no_error;
}

bool algorithm::is_ban_ws_conn_for_origin( const char* origin ) const {
    if ( !settings_.enabled_ )
        return false;
    if ( origin == nullptr || origin[0] == '\0' )
        return true;
    lock_type lock( mtx_ );
#if ( defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__ )
    if ( ws_conn_count_global_ > settings_.global_limit_.max_ws_conn_ )
        return true;
#endif  // (defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__)
    map_ws_conn_counts_t::const_iterator itFind = map_ws_conn_counts_.find( origin ),
                                         itEnd = map_ws_conn_counts_.end();
    if ( itFind == itEnd )
        return false;
    const origin_entry_setting& oe = settings_.find_origin_entry_setting( origin );
    if ( itFind->second > oe.max_ws_conn_ )
        return true;
    return false;
}

e_high_load_detection_result_t algorithm::register_ws_conn_for_origin( const char* origin ) {
    if ( !settings_.enabled_ )
        return e_high_load_detection_result_t::ehldr_no_error;
    if ( origin == nullptr || origin[0] == '\0' )
        return e_high_load_detection_result_t::ehldr_bad_origin;
    lock_type lock( mtx_ );
#if ( defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__ )
    ++ws_conn_count_global_;
    if ( ws_conn_count_global_ > settings_.global_limit_.max_ws_conn_ )
        return e_high_load_detection_result_t::ehldr_peak;
#endif  // (defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__)
    map_ws_conn_counts_t::iterator itFind = map_ws_conn_counts_.find( origin ),
                                   itEnd = map_ws_conn_counts_.end();
    if ( itFind == itEnd ) {
        map_ws_conn_counts_[origin] = 1;
        itFind = map_ws_conn_counts_.find( origin );
    } else
        ++itFind->second;
    const origin_entry_setting& oe = settings_.find_origin_entry_setting( origin );
    if ( itFind->second > oe.max_ws_conn_ )
        return e_high_load_detection_result_t::ehldr_peak;
    return e_high_load_detection_result_t::ehldr_no_error;
}

bool algorithm::unregister_ws_conn_for_origin( const char* origin ) {
    if ( origin == nullptr || origin[0] == '\0' ) {
        if ( !settings_.enabled_ )
            return true;
        return false;
    }
    lock_type lock( mtx_ );
#if ( defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__ )
    if ( ws_conn_count_global_ > 0 )
        --ws_conn_count_global_;
#endif  // (defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__)
    map_ws_conn_counts_t::iterator itFind = map_ws_conn_counts_.find( origin ),
                                   itEnd = map_ws_conn_counts_.end();
    if ( itFind == itEnd ) {
        if ( !settings_.enabled_ )
            return true;
        return false;
    }
    if ( itFind->second >= 1 )
        --itFind->second;
    if ( itFind->second == 0 )
        map_ws_conn_counts_.erase( itFind );
    return true;
}

bool algorithm::load_settings_from_json( const nlohmann::json& joUnDdosSettings ) {
    lock_type lock( mtx_ );
    try {
        settings new_settings;
        new_settings.fromJSON( joUnDdosSettings );
        settings_ = new_settings;
        settings_.auto_append_any_origin_rule();
        return true;
    } catch ( ... ) {
        return false;
    }
}

settings algorithm::get_settings() const {
    lock_type lock( mtx_ );
    settings_.auto_append_any_origin_rule();
    settings copied = settings_;
    return copied;
}

void algorithm::set_settings( const settings& new_settings ) const {
    lock_type lock( mtx_ );
    settings_ = new_settings;
    settings_.auto_append_any_origin_rule();
}

nlohmann::json algorithm::get_settings_json() const {
    lock_type lock( mtx_ );
    settings_.auto_append_any_origin_rule();
    nlohmann::json joUnDdosSettings = nlohmann::json::object();
    settings_.toJSON( joUnDdosSettings );
    return joUnDdosSettings;
}

nlohmann::json algorithm::stats( time_tick_mark ttmNow, duration durationToPast ) const {
    lock_type lock( mtx_ );
    ( const_cast< algorithm* >( this ) )
        ->unload_old_data_by_time_to_past( ttmNow, durationToPast );  // unload first
    nlohmann::json joStats = nlohmann::json::object();
    nlohmann::json joCounts = nlohmann::json::object();
    nlohmann::json joCalls = nlohmann::json::object();
    nlohmann::json joWsConns = nlohmann::json::object();
    size_t cntRpcBan = 0, cntRpcNormal = 0, cntWsBan = 0, cntWsNormal = 0;
    for ( const tracked_origin& to : tracked_origins_ ) {
        nlohmann::json joOriginCallInfo = nlohmann::json::object();
        bool isBan = ( to.ban_until_ != time_tick_mark( 0 ) ) ? true : false;
        joOriginCallInfo["cps"] = to.count_to_past( ttmNow, 1, cntOptimizedMaxSteps4cs_ );
        joOriginCallInfo["cpm"] =
            to.count_to_past( ttmNow, durationToPast, cntOptimizedMaxSteps4cm_ );
        joOriginCallInfo["ban"] = isBan;
        joCalls[to.origin_] = joOriginCallInfo;
        if ( isBan )
            ++cntRpcBan;
        else
            ++cntRpcNormal;
    }
    for ( const map_ws_conn_counts_t::value_type& pr : map_ws_conn_counts_ ) {
        nlohmann::json joWsConnInfo = nlohmann::json::object();
        bool isBan = is_ban_ws_conn_for_origin( pr.first );
        joWsConnInfo["cnt"] = pr.second;
        joWsConnInfo["ban"] = isBan;
        joWsConns[pr.first] = joWsConnInfo;
        if ( isBan )
            ++cntWsBan;
        else
            ++cntWsNormal;
    }
    joCounts["rpc_ban"] = cntRpcBan;
    joCounts["rpc_normal"] = cntRpcNormal;
    joCounts["ws_ban"] = cntWsBan;
    joCounts["ws_normal"] = cntWsNormal;
    joStats["counts"] = joCounts;
    joStats["calls"] = joCalls;
    joStats["ws_conns"] = joWsConns;
#if ( defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__ )
    joStats["global_ws_conns_count"] = ws_conn_count_global_;
    joStats["global_cps"] = tracked_global_.count_to_past( ttmNow, 1, cntOptimizedMaxSteps4gs_ );
    joStats["global_cpm"] =
        tracked_global_.count_to_past( ttmNow, durationToPast, cntOptimizedMaxSteps4gm_ );
    bool isGlobalBan = ( tracked_global_.ban_until_ != time_tick_mark( 0 ) ) ? true : false;
    joStats["global_ban"] = isGlobalBan;
#endif  // (defined __UNDDOS_SUPPORT_4_GLOBAL_SUMMARY__)
    return joStats;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace unddos
};  // namespace skutils
