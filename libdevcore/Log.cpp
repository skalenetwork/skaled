/*
    Modifications Copyright (C) 2018-2019 SKALE Labs

    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file Log.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "Log.h"

#ifdef __APPLE__
#include <pthread.h>
#endif

#include <libdevcore/microprofile.h>

#pragma warning( push )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"

#include <boost/core/null_deleter.hpp>
#include <boost/log/attributes/clock.hpp>
#include <boost/log/attributes/function.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/exception_handler.hpp>

#pragma warning(pop)
#pragma GCC diagnostic pop

#include <skutils/console_colors.h>

#include <chrono>
#include <memory>
#include <thread>

#if defined( NDEBUG )
#include <boost/log/sinks/async_frontend.hpp>
template < class T >
using log_sink = boost::log::sinks::asynchronous_sink< T >;
#else
#include <boost/log/sinks/sync_frontend.hpp>
template < class T >
using log_sink = boost::log::sinks::synchronous_sink< T >;
#endif

namespace dev {

#if defined( __GLIBC__ ) || defined( __APPLE__ )
// nothing)
#else
static thread_local std::string g_logThreadName( "main" );
#endif

std::string getThreadName() {
#if defined( __GLIBC__ ) || defined( __APPLE__ )
    char buffer[128];
    pthread_getname_np( pthread_self(), buffer, 127 );
    buffer[127] = 0;
    return buffer;
#else
    return g_logThreadName.empty() ? std::string( "<unknown>" ) : g_logThreadName;
#endif
}

void setThreadName( std::string const& _n ) {
#if defined( __GLIBC__ )
    pthread_setname_np( pthread_self(), _n.c_str() );
#elif defined( __APPLE__ )
    pthread_setname_np( _n.c_str() );
#else
    g_logThreadName = _n;
#endif
    MicroProfileOnThreadCreate( _n.c_str() );
}

BOOST_LOG_ATTRIBUTE_KEYWORD( channel, "Channel", std::string )
BOOST_LOG_ATTRIBUTE_KEYWORD( context, "Context", std::string )
BOOST_LOG_ATTRIBUTE_KEYWORD( threadName, "ThreadName", std::string )
BOOST_LOG_ATTRIBUTE_KEYWORD( timestamp, "TimeStamp", std::string )

void setupLogging( LoggingOptions const& _options ) {
    auto sink = boost::make_shared< log_sink< boost::log::sinks::text_ostream_backend > >();

    boost::shared_ptr< std::ostream > stream{&std::cout, boost::null_deleter{}};
    sink->locked_backend()->add_stream( stream );
    sink->set_filter( [_options]( boost::log::attribute_value_set const& _set ) {
        if ( _set["Severity"].extract< int >() > _options.verbosity )
            return false;

        auto const messageChannel = _set[channel];
        return ( _options.includeChannels.empty() ||
                   contains( _options.includeChannels, messageChannel ) ) &&
               !contains( _options.excludeChannels, messageChannel );
    } );

    namespace expr = boost::log::expressions;
    std::string strThreadName;
    {  // block
        std::stringstream ss;
        ss << threadName;
        strThreadName = ss.str();
    }  // block
    std::string strChannel;
    {  // block
        std::stringstream ss;
        ss << channel;
        strChannel = ss.str();
    }  // block
    sink->set_formatter( expr::stream
                         << timestamp << " " << cc::info( strThreadName ) << " "
                         << cc::warn( strChannel )
                         << expr::if_( expr::has_attr(
                                context ) )[expr::stream << " " << cc::warn( strChannel )]
                         << " " << expr::smessage );

    boost::log::core::get()->add_sink( sink );
    boost::log::core::get()->add_global_attribute(
        "ThreadName", boost::log::attributes::make_function( &getThreadName ) );
    boost::log::core::get()->add_global_attribute(
        "TimeStamp", boost::log::attributes::make_function(
                         []() -> std::string { return cc::now2string( true ); } ) );

    boost::log::core::get()->set_exception_handler(
        boost::log::make_exception_handler< std::exception >( []( std::exception const& _ex ) {
            std::cerr << "Exception from the logging library: " << _ex.what() << '\n';
        } ) );
}

}  // namespace dev
