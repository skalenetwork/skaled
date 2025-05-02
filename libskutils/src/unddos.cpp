#include <libdevcore/Log.h>
#include <skutils/unddos.h>
#include <shared_mutex>

namespace skutils::unddos {

origin_dos_limits::origin_dos_limits() {
    clear();
}

origin_dos_limits::origin_dos_limits( const origin_dos_limits& other ) {
    assign( other );
}

origin_dos_limits::origin_dos_limits( origin_dos_limits&& other ) {
    assign( other );
    other.clear();
}

origin_dos_limits::~origin_dos_limits() {
    clear();
}

origin_dos_limits& origin_dos_limits::operator=( const origin_dos_limits& other ) {
    assign( other );
    return ( *this );
}


void origin_dos_limits::load_unlim_for_any_origin() {
    clear();
    m_originWildcards.push_back( "*" );
    m_defaultMaxCallsPerSec = std::numeric_limits< size_t >::max();
    m_defaultMaxCallsPerMin = std::numeric_limits< size_t >::max();
    m_banPerSecDuration = duration( 0 );
    m_banPerMinDuration = duration( 0 );
}


bool origin_dos_limits::empty() const {
    if ( !m_originWildcards.empty() )
        return false;
    return true;
}

void origin_dos_limits::clear() {
    m_originWildcards.clear();
    m_defaultMaxCallsPerSec = 0;
    m_defaultMaxCallsPerMin = 0;
    m_banPerSecDuration = duration( 0 );
    m_banPerMinDuration = duration( 0 );
    m_mapCustomMethodLimits.clear();
}

origin_dos_limits& origin_dos_limits::assign( const origin_dos_limits& other ) {
    if ( ( ( void* ) ( this ) ) == ( ( void* ) ( &other ) ) )
        return ( *this );
    clear();
    m_originWildcards = other.m_originWildcards;
    m_defaultMaxCallsPerSec = other.m_defaultMaxCallsPerSec;
    m_defaultMaxCallsPerMin = other.m_defaultMaxCallsPerMin;
    m_banPerSecDuration = other.m_banPerSecDuration;
    m_banPerMinDuration = other.m_banPerMinDuration;
    m_mapCustomMethodLimits = other.m_mapCustomMethodLimits;
    return ( *this );
}

origin_dos_limits& origin_dos_limits::merge( const origin_dos_limits& other ) {
    if ( ( ( void* ) ( this ) ) == ( ( void* ) ( &other ) ) )
        return ( *this );
    if ( m_originWildcards != other.m_originWildcards )
        return ( *this );
    m_defaultMaxCallsPerSec = std::min( m_defaultMaxCallsPerSec, other.m_defaultMaxCallsPerSec );
    m_defaultMaxCallsPerMin = std::min( m_defaultMaxCallsPerMin, other.m_defaultMaxCallsPerMin );
    m_banPerSecDuration = std::max( m_banPerSecDuration, other.m_banPerSecDuration );
    m_banPerMinDuration = std::max( m_banPerMinDuration, other.m_banPerMinDuration );
    if ( !other.m_mapCustomMethodLimits.empty() ) {
        nlohmann::json joCMS = nlohmann::json::object();
        map_custom_method_limits_t::const_iterator itWalk = other.m_mapCustomMethodLimits.cbegin(),
                                                   itEnd = other.m_mapCustomMethodLimits.cend();
        for ( ; itWalk != itEnd; ++itWalk ) {
            const custom_method_limits& cme = itWalk->second;
            map_custom_method_limits_t::iterator itFind =
                m_mapCustomMethodLimits.find( itWalk->first );
            if ( itFind != m_mapCustomMethodLimits.end() ) {
                itFind->second.merge( cme );  // merge with existing
                continue;
            }
            m_mapCustomMethodLimits[itWalk->first] = cme;  // add mew
        }
    }
    return ( *this );
}

void origin_dos_limits::fromJSON( const nlohmann::json& jo ) {
    clear();
    if ( jo.find( "origin" ) != jo.end() ) {
        nlohmann::json jarrWildcards = jo["origin"];
        if ( jarrWildcards.is_string() )
            m_originWildcards.push_back( jarrWildcards.get< std::string >() );
        else if ( jarrWildcards.is_array() ) {
            for ( const nlohmann::json& joWildcard : jarrWildcards ) {
                if ( joWildcard.is_string() )
                    m_originWildcards.push_back( joWildcard.get< std::string >() );
            }
        }
    }
    if ( jo.find( "max_calls_per_second" ) != jo.end() )
        m_defaultMaxCallsPerSec = jo["max_calls_per_second"].get< size_t >();
    if ( jo.find( "max_calls_per_minute" ) != jo.end() )
        m_defaultMaxCallsPerMin = jo["max_calls_per_minute"].get< size_t >();
    if ( jo.find( "ban_peak" ) != jo.end() )
        m_banPerSecDuration = jo["ban_peak"].get< size_t >();
    if ( jo.find( "ban_lengthy" ) != jo.end() )
        m_banPerMinDuration = jo["ban_lengthy"].get< size_t >();
    if ( jo.find( "custom_method_settings" ) != jo.end() ) {
        const nlohmann::json& joCMS = jo["custom_method_settings"];
        for ( auto it = joCMS.cbegin(); it != joCMS.cend(); ++it ) {
            const nlohmann::json& joMethod = it.value();
            custom_method_limits cme;
            if ( joMethod.find( "max_calls_per_second" ) != jo.end() )
                cme.m_maxCallsPerSecond = joMethod["max_calls_per_second"].get< size_t >();
            if ( joMethod.find( "max_calls_per_minute" ) != jo.end() )
                cme.m_maxCallsPerMinute = joMethod["max_calls_per_minute"].get< size_t >();
            m_mapCustomMethodLimits[it.key()] = cme;
        }
    }
}

void origin_dos_limits::toJSON( nlohmann::json& jo ) const {
    jo = nlohmann::json::object();
    nlohmann::json jarrWildcards = nlohmann::json::array();
    for ( const std::string& wildcard : m_originWildcards )
        jarrWildcards.push_back( wildcard );
    jo["origin"] = jarrWildcards;
    jo["max_calls_per_second"] = m_defaultMaxCallsPerSec;
    jo["max_calls_per_minute"] = m_defaultMaxCallsPerMin;
    jo["ban_peak"] = m_banPerSecDuration;
    jo["ban_lengthy"] = m_banPerMinDuration;
    if ( !m_mapCustomMethodLimits.empty() ) {
        nlohmann::json joCMS = nlohmann::json::object();
        map_custom_method_limits_t::const_iterator itWalk = m_mapCustomMethodLimits.cbegin(),
                                                   itEnd = m_mapCustomMethodLimits.cend();
        for ( ; itWalk != itEnd; ++itWalk ) {
            const custom_method_limits& cme = itWalk->second;
            nlohmann::json joMethod = nlohmann::json::object();
            joMethod["max_calls_per_second"] = cme.m_maxCallsPerSecond;
            joMethod["max_calls_per_minute"] = cme.m_maxCallsPerMinute;
            joCMS[itWalk->first] = joMethod;
        }
        jo["custom_method_settings"] = joCMS;
    }
}

bool origin_dos_limits::match_origin( const std::string& origin ) const {
    if ( origin.empty() )
        return false;
    for ( const std::string& wildcard : m_originWildcards ) {
        if ( skutils::tools::wildcmp( wildcard.c_str(), origin.c_str() ) )
            return true;
    }
    return false;
}

size_t origin_dos_limits::max_calls_per_second( const std::string& strMethod ) const {
    if ( strMethod.empty() )
        return m_defaultMaxCallsPerSec;
    map_custom_method_limits_t::const_iterator itFind = m_mapCustomMethodLimits.find( strMethod ),
                                               itEnd = m_mapCustomMethodLimits.cend();
    if ( itFind == itEnd )
        return m_defaultMaxCallsPerSec;
    const custom_method_limits& cme = itFind->second;
    const size_t cnt = std::max( m_defaultMaxCallsPerSec, cme.m_maxCallsPerSecond );
    return cnt;
}

size_t origin_dos_limits::max_calls_per_minute( const std::string& strMethod ) const {
    if ( strMethod.empty() )
        return m_defaultMaxCallsPerMin;
    map_custom_method_limits_t::const_iterator itFind = m_mapCustomMethodLimits.find( strMethod ),
                                               itEnd = m_mapCustomMethodLimits.cend();
    if ( itFind == itEnd )
        return m_defaultMaxCallsPerMin;
    const custom_method_limits& cme = itFind->second;
    const size_t cnt = std::max( m_defaultMaxCallsPerMin, cme.m_maxCallsPerMinute );
    return cnt;
}


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
    if ( !m_enabled )
        return true;
    if ( !m_originDosLimits.empty() )
        return false;
    if ( !m_globalLimitSetting.empty() )
        return false;
    return true;
}

void settings::clear() {
    m_enabled = true;
    m_originDosLimits.clear();
    m_globalLimitSetting.clear();
}

settings& settings::assign( const settings& other ) {
    if ( ( ( void* ) ( this ) ) == ( ( void* ) ( &other ) ) )
        return ( *this );
    clear();
    m_enabled = other.m_enabled;
    m_originDosLimits = other.m_originDosLimits;
    m_globalLimitSetting = other.m_globalLimitSetting;
    return ( *this );
}


void settings::fromJSON( const nlohmann::json& jo ) {
    clear();

    bool isEnabled = true;
    if ( jo.find( "enabled" ) != jo.end() ) {
        const nlohmann::json& joEnabled = jo["enabled"];
        if ( joEnabled.is_boolean() )
            isEnabled = joEnabled.get< bool >();
    }
    m_enabled = isEnabled;

    if ( !m_enabled ) {
        return;
    }

    if ( jo.find( "origins" ) != jo.end() ) {
        const nlohmann::json& joOrigins = jo["origins"];
        if ( joOrigins.is_array() ) {
            for ( const nlohmann::json& joOrigin : joOrigins ) {
                origin_dos_limits oe;
                oe.fromJSON( joOrigin );
                m_originDosLimits.push_back( oe );
            }
        }
    }
    if ( jo.find( "global" ) != jo.end() ) {
        const nlohmann::json& joGlobalLimit = jo["global"];
        origin_dos_limits oe;
        oe.fromJSON( joGlobalLimit );
        m_globalLimitSetting = oe;
    } else
        m_globalLimitSetting.load_unlim_for_any_origin();
}


size_t settings::findOriginLimitsMatch( const std::string& _origin ) const {
    if ( _origin.empty() )
        return std::string::npos;

    for ( size_t i = 0; i < m_originDosLimits.size(); ++i ) {
        if ( m_originDosLimits[i].match_origin( _origin ) )
            return i;
    }
    return std::string::npos;
}

origin_dos_limits& settings::findOriginDosLimits( const std::string& _origin ) {
    size_t i = findOriginLimitsMatch( _origin );
    if ( i != std::string::npos ) {
        return m_originDosLimits[i];
    } else {
        return m_globalLimitSetting;
    }
}


tracked_origin::tracked_origin( const std::string& _origin ) : m_origin( _origin ){};

void tracked_origin::setDosLimits( const origin_dos_limits& _dosLimits ) {
    m_dosLimits = _dosLimits;
}

e_high_load_detection_result_t tracked_origin::recordMethodUseAndDetectBan(
    uint64_t _callTimeSec, const std::string& _strMethod ) {
    recordUse( _callTimeSec, _strMethod );

    if ( isBanned( _callTimeSec ) ) {
        return e_high_load_detection_result_t::ehldr_already_banned;  // still banned
    }

    return detectBan( _callTimeSec, _strMethod );
}

e_high_load_detection_result_t tracked_origin::detectBan(
    uint64_t _callTimeSec, const std::string& _strMethod ) {
    std::shared_lock< std::shared_mutex > lock( x_mutex );

    auto maxCallsPerMinute = m_dosLimits.max_calls_per_minute( _strMethod );

    if ( maxCallsPerMinute > 0 ) {
        if ( m_currentMinUseCounterPerMethod[_strMethod] > maxCallsPerMinute ) {
            m_banUntilSec = _callTimeSec + m_dosLimits.m_banPerMinDuration;
            return e_high_load_detection_result_t::ehldr_detected_ban_per_min;  // ban by too high
                                                                                // load per min
        }
    }

    auto maxCallsPerSecond = m_dosLimits.max_calls_per_second( _strMethod );
    if ( maxCallsPerSecond > 0 ) {
        if ( m_currentSecUseCounterPerMethod[_strMethod] > maxCallsPerSecond ) {
            m_banUntilSec = _callTimeSec + m_dosLimits.m_banPerSecDuration;
            return e_high_load_detection_result_t::ehldr_detected_ban_per_sec;
        }
    }

    return e_high_load_detection_result_t::ehldr_no_error;
}


tracked_origin::~tracked_origin() {}


bool tracked_origin::isBanned( uint64_t _timeSec ) {
    return ( _timeSec <= m_banUntilSec );
}

void tracked_origin::recordUse( uint64_t _useTimeSec, const std::string& _method ) {
    static constexpr uint64_t SECONDS_IN_MINUTE = 60;
    auto minute = _useTimeSec / SECONDS_IN_MINUTE;

    std::unique_lock< std::shared_mutex > lock( x_mutex );

    if ( ( uint64_t ) _useTimeSec > m_currentSec ) {
        // next hour arrived. Reset use counter
        m_currentSecUseCounterPerMethod.clear();
        m_currentSec = ( uint64_t ) _useTimeSec;
    }

    if ( minute > m_currentMin ) {
        // next minute arrived. Reset use counters
        m_currentMinUseCounterPerMethod.clear();
        m_currentMin = minute;
    }


    // increment counters

    if ( m_currentSecUseCounterPerMethod.count( _method ) > 0 ) {
        m_currentSecUseCounterPerMethod[_method]++;
    } else {
        m_currentSecUseCounterPerMethod[_method] = 1;
    }

    if ( m_currentMinUseCounterPerMethod.count( _method ) > 0 ) {
        m_currentMinUseCounterPerMethod[_method]++;
    } else {
        m_currentMinUseCounterPerMethod[_method] = 1;
    }
}

algorithm::algorithm() : m_globalOrigin( "" ) {}

algorithm::algorithm( const settings& st ) : m_globalOrigin( "" ) {
    m_settings = st;
    m_globalOrigin.setDosLimits( m_settings.m_globalLimitSetting );
}

algorithm::~algorithm() {}

algorithm& algorithm::operator=( const settings& st ) {
    m_settings = st;
    return ( *this );
}

constexpr uint64_t MAX_UNDDOS_MAP_ENTRIES = 256 * 1024;

e_high_load_detection_result_t algorithm::register_call_from_origin(
    const std::string& _origin, const std::string& _strMethod, time_tick_mark _callTime ) {
    if ( !m_settings.m_enabled ) {
        // DOS protection disabled
        return e_high_load_detection_result_t::ehldr_no_error;
    }

    if ( _origin.empty() )
        return e_high_load_detection_result_t::ehldr_bad_origin;

    // set the call time to current time if it was not provided
    setCallTimeToNowIfZero( _callTime );

    // first check for global ban since it does not need to access the map

    auto result = m_globalOrigin.recordMethodUseAndDetectBan( _callTime, _strMethod );

    if ( result != e_high_load_detection_result_t::ehldr_no_error ) {
        if ( result == e_high_load_detection_result_t::ehldr_detected_ban_per_sec ) {
            cwarn << "Global ban per second for:" << _origin;
        } else if ( result == e_high_load_detection_result_t::ehldr_detected_ban_per_min ) {
            cwarn << "Global ban per min for:" << _origin;
        }
        return result;
    }

    // now we checked for global ban, check for a ban based on origin
    // we need to read lock to do it
    std::shared_ptr< tracked_origin > trackedOrigin = nullptr;
    {
        std::shared_lock< std::shared_mutex > lock( x_mtx );
        auto iterator = m_trackedOriginsMap.find( _origin );
        if ( iterator != m_trackedOriginsMap.end() ) {
            trackedOrigin = iterator->second;
        }
    }

    // if we did not find the tracked origin, it is not in the map yet. We need to init it under
    // write lock

    if ( !trackedOrigin ) {
        addNewOriginToMap( _origin, _callTime );
        return e_high_load_detection_result_t::ehldr_no_error;
    } else {
        // since we now have trackedOrigin the rest can be done without holding any lock on the map
        result = trackedOrigin->recordMethodUseAndDetectBan( _callTime, _strMethod );
        if ( result != e_high_load_detection_result_t::ehldr_no_error ) {
            if ( result == e_high_load_detection_result_t::ehldr_detected_ban_per_sec ) {
                cwarn << "Ban per second for:" << _origin;
            } else if ( result == e_high_load_detection_result_t::ehldr_detected_ban_per_min ) {
                cwarn << "Ban per min for:" << _origin;
            }
        }
        return result;
    }
}

void algorithm::addNewOriginToMap( const std::string& _origin, time_tick_mark _callTime ) {
    const origin_dos_limits& oe = m_settings.findOriginDosLimits( _origin );
    {
        std::unique_lock< std::shared_mutex > writeLock( x_mtx );
        if ( m_trackedOriginsMap.size() > MAX_UNDDOS_MAP_ENTRIES ) {
            // the map grows in size, we clear it from time to time
            // so that it does not grow indefinitely because of old accesses
            // that will happen very infrequently
            // to fill the map
            m_trackedOriginsMap.clear();
        }
        if ( m_trackedOriginsMap.count( _origin ) == 0 ) {
            m_trackedOriginsMap.emplace( _origin, std::make_shared< tracked_origin >( _origin ) );
            m_trackedOriginsMap.at( _origin )->setDosLimits( oe );
            m_trackedOriginsMap.at( _origin )->m_currentSec = _callTime;
            m_trackedOriginsMap.at( _origin )->m_currentMin = _callTime / 60;
        }
    }
}


void algorithm::load_settings_from_json( const nlohmann::json& joUnDdosSettings ) {
    std::unique_lock< std::shared_mutex > lock( x_mtx );
    settings new_settings;
    new_settings.fromJSON( joUnDdosSettings );
    m_settings = new_settings;
}

void algorithm::disable_ddos() const {
    std::shared_lock< std::shared_mutex > lock( x_mtx );
    m_settings.m_enabled = false;
}


};  // namespace skutils::unddos
