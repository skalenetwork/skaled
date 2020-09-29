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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace unddos
};  // namespace skutils
