#include <skutils/rest_call.h>
#include <skutils/utils.h>

#include <stdlib.h>

#include <cstdlib>
#include <thread>

#include <skutils/utils.h>

namespace skutils {
namespace rest {

data_t::data_t() {}
data_t::data_t( const data_t& d ) {
    assign( d );
}
data_t::~data_t() {
    clear();
}
data_t& data_t::operator=( const data_t& d ) {
    assign( d );
    return ( *this );
}

bool data_t::is_json() const {
    return ( strcasecmp( content_type_.c_str(), "application/json" ) == 0 ||
               strcasecmp( content_type_.c_str(), "text/json" ) == 0 ) ?
               true :
               false;
}
bool data_t::is_binary() const {
    return ( strcasecmp( content_type_.c_str(), "application/octet-stream" ) == 0 ) ? true : false;
}

bool data_t::empty() const {
    return content_type_.empty();
}
void data_t::clear() {
    content_type_.clear();
    s_.clear();
}
void data_t::assign( const data_t& d ) {
    s_ = d.s_;
    content_type_ = d.content_type_;
    ei_ = d.ei_;
}

nlohmann::json data_t::extract_json() const {
    nlohmann::json jo = nlohmann::json::object();
    try {
        if ( !is_json() )
            return jo;
        jo = nlohmann::json::parse( s_ );
    } catch ( ... ) {
    }
    return jo;
}

std::string data_t::extract_json_id() const {
    std::string id;
    try {
        nlohmann::json jo = extract_json();
        if ( jo.count( "id" ) > 0 ) {
            nlohmann::json wid = jo[id];
            if ( wid.is_string() ) {
                id = wid.get< std::string >();
            } else if ( wid.is_number_unsigned() ) {
                unsigned n = wid.get< unsigned >();
                id = std::to_string( n );
            } else if ( wid.is_number() ) {
                int n = wid.get< int >();
                id = std::to_string( n );
            }
        }
    } catch ( ... ) {
    }
    return id;
}

client::client() {
    async_remove_all();
}

client::client( const skutils::url& u ) {
    open( u );
}

client::client( const std::string& url_str ) {
    open( url_str );
}

client::client( const char* url_str ) {
    open( std::string( url_str ? url_str : "" ) );
}

client::~client() {
    close();
}

bool client::open( const skutils::url& u, std::chrono::milliseconds wait_step, size_t cntSteps ) {
    try {
        std::string strScheme = skutils::tools::to_lower( skutils::tools::trim_copy( u.scheme() ) );
        if ( strScheme.empty() )
            return false;
        //
        std::string strHost = skutils::tools::to_lower( skutils::tools::trim_copy( u.host() ) );
        if ( strHost.empty() )
            return false;
        //
        std::string strPort = skutils::tools::to_lower( skutils::tools::trim_copy( u.port() ) );
        if ( strPort.empty() )
            strPort = "80";
        int nPort = std::atoi( strPort.c_str() );
        //
        if ( strScheme == "http" ) {
            close();
            ch_.reset( new skutils::http::client( -1, strHost.c_str(), nPort ) );
        } else if ( strScheme == "https" ) {
            close();
            ch_.reset( new skutils::http::SSL_client( -1, strHost.c_str(), nPort,
                __SKUTILS_HTTP_CLIENT_CONNECT_TIMEOUT_MILLISECONDS__, &optsSSL ) );
        } else if ( strScheme == "ws" || strScheme == "wss" ) {
            cw_.reset( new skutils::ws::client );
            cw_->onMessage_ = [&]( skutils::ws::basic_participant&, skutils::ws::hdl_t,
                                  skutils::ws::opcv eOpCode, const std::string& s ) -> void {
                data_t d;
                d.s_ = s;
                d.content_type_ = ( eOpCode == skutils::ws::opcv::binary ) ?
                                      "application/octet-stream" :
                                      "application/json" /*g_str_default_content_type*/;
                handle_data_arrived( d );
            };
            if ( !cw_->open( u.str() ) ) {
                close();
                return false;
            }
            for ( size_t i = 0; ( !cw_->isConnected() ) && i < cntSteps; ++i ) {
                std::this_thread::sleep_for( wait_step );
            }
            if ( !cw_->isConnected() ) {
                close();
                return false;
            }
        } else
            return false;
        return true;
    } catch ( ... ) {
    }
    close();
    return false;
}

bool client::open(
    const std::string& url_str, std::chrono::milliseconds wait_step, size_t cntSteps ) {
    skutils::url u( url_str );
    return open( u, wait_step, cntSteps );
}

bool client::open( const char* url_str, std::chrono::milliseconds wait_step, size_t cntSteps ) {
    return open( std::string( url_str ? url_str : "" ), wait_step, cntSteps );
}

void client::close() {
    try {
        if ( ch_ ) {
            ch_.reset();
        }
        if ( cw_ ) {
            cw_->close();
            cw_.reset();
        }
    } catch ( ... ) {
    }
}

std::string client::get_connected_url_scheme() const {
    if ( ch_ ) {
        if ( ch_->is_valid() ) {
            if ( ch_->is_ssl() )
                return "https";
            return "http";
        }
    } else if ( cw_ ) {
        if ( cw_->isConnected() ) {
            if ( cw_->is_ssl() )
                return "wss";
            return "ws";
        }
    }
    return "";  // not connected
}
bool client::is_open() const {
    std::string s = get_connected_url_scheme();
    if ( s.empty() )
        return false;
    return true;
}

bool client::handle_data_arrived( const data_t& d ) {
    if ( d.empty() )
        return false;
    await_t a;
    {  // block
        lock_type lock( mtxData_ );
        // try to find async handler
        std::string strCallID = d.extract_json_id();
        if ( !strCallID.empty() )
            a = async_get( strCallID );
        // push it, if not found
        if ( a.strCallID.empty() )
            lstData_.push_back( d );
        else
            async_remove_by_call_id( strCallID );
    }  // block
    if ( !a.strCallID.empty() ) {
        if ( a.onData )
            a.onData( a.joIn, d );
    }
    return true;
}

data_t client::fetch_data_with_strategy( e_data_fetch_strategy edfs, const std::string id ) {
    data_t d;
    lock_type lock( mtxData_ );
    if ( lstData_.empty() )
        return d;
    switch ( edfs ) {
    case e_data_fetch_strategy::edfs_by_equal_json_id: {
        data_list_t::iterator itWalk = lstData_.begin(), itEnd = lstData_.end();
        for ( ; itWalk != itEnd; ++itWalk ) {
            const data_t& walk = ( *itWalk );
            if ( walk.is_json() ) {
                try {
                    std::string wid = walk.extract_json_id();
                    if ( wid == id ) {
                        d = walk;
                        lstData_.erase( itWalk );
                        break;
                    }
                } catch ( ... ) {
                }
            }
        }  // for ( ; itWalk != itEnd; ++itWalk )
    } break;
    case e_data_fetch_strategy::edfs_nearest_arrived:
        d = lstData_.front();
        lstData_.pop_front();
        break;
    case e_data_fetch_strategy::edfs_nearest_binary: {
        data_list_t::iterator itWalk = lstData_.begin(), itEnd = lstData_.end();
        for ( ; itWalk != itEnd; ++itWalk ) {
            const data_t& walk = ( *itWalk );
            if ( walk.is_binary() ) {
                d = walk;
                lstData_.erase( itWalk );
                break;
            }
        }  // for ( ; itWalk != itEnd; ++itWalk )
    } break;
    case e_data_fetch_strategy::edfs_nearest_json: {
        data_list_t::iterator itWalk = lstData_.begin(), itEnd = lstData_.end();
        for ( ; itWalk != itEnd; ++itWalk ) {
            const data_t& walk = ( *itWalk );
            if ( walk.is_json() ) {
                d = walk;
                lstData_.erase( itWalk );
                break;
            }
        }  // for ( ; itWalk != itEnd; ++itWalk )
    } break;
    case e_data_fetch_strategy::edfs_nearest_text: {
        data_list_t::iterator itWalk = lstData_.begin(), itEnd = lstData_.end();
        for ( ; itWalk != itEnd; ++itWalk ) {
            const data_t& walk = ( *itWalk );
            if ( !walk.is_binary() ) {
                d = walk;
                lstData_.erase( itWalk );
                break;
            }
        }  // for ( ; itWalk != itEnd; ++itWalk )
    } break;
    }  // switch( edfs )
    return d;
}

const char client::g_str_default_content_type[] = "application/json";

std::string client::stat_extract_short_content_type_string( const std::string& s ) {
    std::string h = skutils::tools::to_lower( skutils::tools::trim_copy( s ) );
    size_t pos = h.find( ';' );
    if ( pos != std::string::npos && pos > 0 )
        h = skutils::tools::trim_copy( h.substr( 0, pos ) );
    return h;
}

uint64_t client::stat_get_random_number( uint64_t const& min, uint64_t const& max ) {
    return ( ( ( uint64_t )( unsigned int ) rand() << 32 ) + ( uint64_t )( unsigned int ) rand() ) %
               ( max - min ) +
           min;
}
uint64_t client::stat_get_random_number() {
    return stat_get_random_number( 1, RAND_MAX );
}

bool client::stat_auto_gen_json_id( nlohmann::json& jo ) {
    if ( !jo.is_object() )
        return false;
    if ( jo.count( "id" ) > 0 )
        return false;
    static mutex_type g_mtx;
    lock_type lock( g_mtx );
    static volatile uint64_t g_id = stat_get_random_number();
    ++g_id;
    if ( g_id == 0 )
        ++g_id;
    jo["id"] = g_id;
    return true;  // "id" was generated and set
}

data_t client::call( const nlohmann::json& joIn, bool isAutoGenJsonID, e_data_fetch_strategy edfs,
    std::chrono::milliseconds wait_step, size_t cntSteps ) {
    nlohmann::json jo = joIn;
    if ( isAutoGenJsonID )
        stat_auto_gen_json_id( jo );
    std::string strJsonIn = jo.dump();
    if ( ch_ ) {
        if ( ch_->is_valid() ) {
            data_t d;
            std::shared_ptr< skutils::http::response > resp =
                ch_->Post( "/", strJsonIn, "application/json" );
            d.ei_ = ch_->eiLast_;
            if ( !resp )
                return d;  // data_t();
            if ( resp->status_ != 200 )
                return d;  // data_t();
            d.s_ = resp->body_;
            std::string h;
            if ( resp->has_header( "Content-Type" ) )
                h = stat_extract_short_content_type_string(
                    resp->get_header_value( "Content-Type" ) );
            d.content_type_ = ( !h.empty() ) ? h : g_str_default_content_type;
            handle_data_arrived( d );
        }
    } else if ( cw_ ) {
        if ( cw_->isConnected() ) {
            if ( !cw_->sendMessage( strJsonIn ) )
                return data_t();
            for ( size_t i = 0; ( cw_->isConnected() ) && i < cntSteps; ++i ) {
                data_t d = fetch_data_with_strategy( edfs );
                if ( !d.empty() )
                    return d;
                std::this_thread::sleep_for( wait_step );
            }
        }
    }
    data_t d = fetch_data_with_strategy( edfs );
    if ( d.empty() ) {
        d.ei_.et_ = skutils::http::common_network_exception::error_type::et_unknown;
        d.ei_.ec_ = errno;
        d.ei_.strError_ = "WS(S) data transfer error";
    }
    return d;
}
data_t client::call( const std::string& strJsonIn, bool isAutoGenJsonID, e_data_fetch_strategy edfs,
    std::chrono::milliseconds wait_step, size_t cntSteps ) {
    try {
        nlohmann::json jo = nlohmann::json::parse( strJsonIn );
        return call( jo, isAutoGenJsonID, edfs, wait_step, cntSteps );
    } catch ( ... ) {
    }
    return data_t();
}

skutils::dispatch::queue_id_t client::async_get_dispatch_queue_id(
    const std::string& strCallID ) const {
    return skutils::dispatch::generate_id(
        ( void* ) this, ( !strCallID.empty() ) ? strCallID.c_str() : "noID" );
}
await_t client::async_get( const std::string& strCallID ) const {
    if ( strCallID.empty() )
        return await_t();
    lock_type lock( mtxData_ );
    map_await_t::const_iterator itFind = map_await_.find( strCallID ), itEnd = map_await_.cend();
    if ( itFind == itEnd )
        return await_t();
    return itFind->second;
}
void client::async_add( await_t& a ) {
    auto strCallID = a.strCallID;
    if ( strCallID.empty() )
        throw std::runtime_error( "rest async call must have non-empty \"id\"" );
    lock_type lock( mtxData_ );
    map_await_t::const_iterator itFind = map_await_.find( a.strCallID ), itEnd = map_await_.cend();
    if ( itFind != itEnd )
        throw std::runtime_error( "rest async call must have unique \"id\"" );
    skutils::dispatch::job_t fnTimeout = [=]() -> void {
        await_t a2 = async_get( strCallID );
        async_remove_by_call_id( strCallID );
        if ( a2.onError )
            a2.onError( a2.joIn, "timeout" );
    };
    skutils::dispatch::once( async_get_dispatch_queue_id( strCallID ), fnTimeout,
        skutils::dispatch::duration_from_milliseconds( a.wait_timeout.count() ),
        &a.idTimeoutNotificationJob );
    map_await_[strCallID] = a;
}
void client::async_remove_impl( await_t& a ) {
    // no lock needed here
    if ( a.strCallID.empty() )
        return;
    if ( !a.idTimeoutNotificationJob.empty() ) {
        skutils::dispatch::stop( a.idTimeoutNotificationJob );
        a.idTimeoutNotificationJob.clear();
    }
    skutils::dispatch::remove( async_get_dispatch_queue_id( a.strCallID ) );
}
bool client::async_remove_by_call_id( const std::string& strCallID ) {
    lock_type lock( mtxData_ );
    map_await_t::iterator itFind = map_await_.find( strCallID ), itEnd = map_await_.end();
    if ( itFind == itEnd )
        return false;
    async_remove_impl( itFind->second );
    map_await_.erase( itFind );
    return true;
}
void client::async_remove_all() {
    lock_type lock( mtxData_ );
    map_await_t::iterator itWalk = map_await_.begin(), itEnd = map_await_.end();
    for ( ; itWalk != itEnd; ++itWalk )
        async_remove_impl( itWalk->second );
    map_await_.clear();
}

void client::async_call( const nlohmann::json& joIn, fn_async_call_data_handler_t onData,
    fn_async_call_error_handler_t onError, bool isAutoGenJsonID, e_data_fetch_strategy edfs,
    std::chrono::milliseconds wait_timeout ) {
    if ( !onData )
        throw std::runtime_error(
            "skutils::reset::client::async_call() cannot be used without onData() handler" );
    if ( !onError )
        throw std::runtime_error(
            "skutils::reset::client::async_call() cannot be used without onError() handler" );
    nlohmann::json jo = joIn;
    if ( isAutoGenJsonID )
        stat_auto_gen_json_id( jo );
    std::string strCallID = jo["id"].dump();
    std::string strJsonIn = jo.dump();
    if ( ch_ ) {
        if ( ch_->is_valid() ) {
            data_t d;
            std::shared_ptr< skutils::http::response > resp =
                ch_->Post( "/", strJsonIn, "application/json" );
            if ( !resp ) {
                onError( jo, "empty responce" );
                return;
            }
            if ( resp->status_ != 200 ) {
                onError( jo, skutils::tools::format( "returned status %d from call to %s",
                                 int( resp->status_ ), u_.str().c_str() ) );
                return;
            }
            d.s_ = resp->body_;
            std::string h;
            if ( resp->has_header( "Content-Type" ) )
                h = stat_extract_short_content_type_string(
                    resp->get_header_value( "Content-Type" ) );
            d.content_type_ = ( !h.empty() ) ? h : g_str_default_content_type;
            handle_data_arrived( d );
            data_t dataOut = fetch_data_with_strategy( edfs );
            onData( jo, dataOut );
            return;
        };
        onError( jo, skutils::tools::format( "not connected to %s", u_.str().c_str() ) );
        return;
    }
    if ( cw_ ) {
        if ( cw_->isConnected() ) {
            await_t a;
            a.strCallID = strCallID;
            a.joIn = jo;
            a.wait_timeout = wait_timeout;
            // a.idTimeoutNotificationJob = ....
            a.onData = onData;
            a.onError = onError;
            async_add( a );
            if ( !cw_->sendMessage( strJsonIn ) ) {
                onError(
                    jo, skutils::tools::format( "data transfer error to %s", u_.str().c_str() ) );
                return;
            }
            //            for ( size_t i = 0; ( cw_->isConnected() ) && i < cntSteps; ++i ) {
            //                data_t d = fetch_data_with_strategy( edfs );
            //                if ( !d.empty() )
            //                    return d;
            //                std::this_thread::sleep_for( wait_step );
            //            }
            //            return;
            return;
        }
        onError( jo, skutils::tools::format( "not connected to %s", u_.str().c_str() ) );
        return;
    }
    onError( jo, skutils::tools::format( "connection not initialized for %s", u_.str().c_str() ) );
}
void client::async_call( const std::string& strJsonIn, fn_async_call_data_handler_t onData,
    fn_async_call_error_handler_t onError, bool isAutoGenJsonID, e_data_fetch_strategy edfs,
    std::chrono::milliseconds wait_timeout ) {
    try {
        nlohmann::json jo = nlohmann::json::parse( strJsonIn );
        return async_call( jo, onData, onError, isAutoGenJsonID, edfs, wait_timeout );
    } catch ( ... ) {
    }
}

};  // namespace rest
};  // namespace skutils
