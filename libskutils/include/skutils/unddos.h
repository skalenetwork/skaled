#if ( !defined __SKUTILS_UN_DDOS_H )
#define __SKUTILS_UN_DDOS_H 1

#include <stdint.h>
#include <stdlib.h>
#include <algorithm>
#include <string>
#include <vector>

#include <skutils/multithreading.h>
#include <skutils/utils.h>

namespace skutils {
namespace unddos {

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
    void clear();
    origin_entry_setting& assign( const origin_entry_setting& other );
    origin_entry_setting& merge( const origin_entry_setting& other );
    void fromJSON( const nlohmann::json& jo );
    void toJSON( nlohmann::json& jo ) const;
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
    void clear();
    settings& assign( const settings& other );
    settings& merge( const origin_entry_setting& oe );
    settings& merge( const settings& other );
    size_t indexOfOrigin( const origin_entry_setting& oe, size_t idxStart = std::string::npos );
    size_t indexOfOrigin( const char* origin_wildcard, size_t idxStart = std::string::npos );
    size_t indexOfOrigin( const std::string& origin_wildcard, size_t idxStart = std::string::npos );
    void fromJSON( const nlohmann::json& jo );
    void toJSON( nlohmann::json& jo ) const;
};  /// class settings

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class algorithm {
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;
    mutex_type mtx_;
    settings settings_;

public:
    algorithm();
    algorithm( const settings& st );
    algorithm( const algorithm& ) = delete;
    algorithm( algorithm&& ) = delete;
    virtual ~algorithm();
    algorithm& operator=( const algorithm& ) = delete;
    algorithm& operator=( const settings& st );
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace unddos
};  // namespace skutils

#endif  // (!defined __SKUTILS_UN_DDOS_H)
