#include "SkaleDebug.h"

#include <skutils/task_performance.h>

#include <cassert>
#include <sstream>


std::atomic<bool> SkaleDebugInterface::g_isEnabled = true;

#define CHECK_ENABLED       \
        if ( !SkaleDebugInterface::g_isEnabled ) { \
            return;         \
        };

#define CHECK_ENABLED_STR       \
if ( !SkaleDebugInterface::g_isEnabled ) { \
return "";         \
};

#define CHECK_ENABLED_INT       \
if ( !SkaleDebugInterface::g_isEnabled ) { \
return 0;         \
};

SkaleDebugInterface::SkaleDebugInterface() {}

int SkaleDebugInterface::add_handler( handler h ) {
    CHECK_ENABLED_INT;
    handlers.push_back( h );
    return handlers.size() - 1;
}

void SkaleDebugInterface::remove_handler( int pos ) {
    CHECK_ENABLED
    handlers.erase( handlers.begin() + pos );
}

std::string SkaleDebugInterface::call( const std::string& arg ) {
    CHECK_ENABLED_STR;
    for ( auto handler : handlers ) {
        std::string res = handler( arg );
        if ( !res.empty() )
            return res;
    }
    return "";
}

void SkaleDebugTracer::call_on_tracepoint(
    const std::function< void( const std::string& ) >& callee ) {
    CHECK_ENABLED;
    global_callbacks.push_back( callee );
}

void SkaleDebugTracer::call_on_tracepoint(
    const std::string& name, const std::function< void( const std::string& ) >& callee ) {
    CHECK_ENABLED;
    tracepoint_struct& tp_obj = find_by_name( name );

    std::lock_guard< std::mutex > thread_lock( tp_obj.thread_mutex );

    tp_obj.callbacks.push_back( callee );
}

void SkaleDebugTracer::break_on_tracepoint( const std::string& name, int count ) {
    CHECK_ENABLED;
    tracepoint_struct& tp_obj = find_by_name( name );

    std::lock_guard< std::mutex > thread_lock( tp_obj.thread_mutex );

    assert( !tp_obj.need_break );
    tp_obj.need_break = true;
    tp_obj.waiting_count = 0;
    tp_obj.needed_waiting_count = count;
}

void SkaleDebugTracer::wait_for_tracepoint( const std::string& name ) {
    CHECK_ENABLED;
    tracepoint_struct& tp_obj = find_by_name( name );

    std::unique_lock< std::mutex > lock2( tp_obj.caller_mutex );
    tp_obj.caller_cond.wait( lock2 );
}

void SkaleDebugTracer::continue_on_tracepoint( const std::string& name ) {
    CHECK_ENABLED;
    tracepoint_struct& tp_obj = find_by_name( name );

    std::lock_guard< std::mutex > thread_lock( tp_obj.thread_mutex );
    assert( !tp_obj.need_break );
    tp_obj.thread_cond.notify_all();
}

void SkaleDebugTracer::tracepoint( const std::string& name ) {
    CHECK_ENABLED;
    tracepoint_struct& tp_obj = find_by_name( name );

    std::unique_lock< std::mutex > lock2( tp_obj.thread_mutex );
    ++tp_obj.pass_count;

    for ( auto f : global_callbacks )
        f( name );
    for ( auto f : tp_obj.callbacks )
        f( name );

    if ( tp_obj.need_break ) {
        ++tp_obj.waiting_count;

        // WARNING! wait_for_tracepoint will be triggered exclusively if it was called BEFORE
        // tracepoint was reached!!
        if ( tp_obj.waiting_count == tp_obj.needed_waiting_count ) {
            std::unique_lock< std::mutex > lock2( tp_obj.caller_mutex );
            tp_obj.need_break = false;
            tp_obj.caller_cond.notify_all();
        }

        tp_obj.thread_cond.wait( lock2 );
    }
}

std::string DebugTracer_handler( const std::string& arg, SkaleDebugTracer& tracer ) {
    CHECK_ENABLED_STR;
    using namespace std;

    if ( arg.find( "trace " ) == 0 ) {
        istringstream stream( arg );

        string trace;
        stream >> trace;
        string command;
        stream >> command;

        if ( arg.find( "trace break" ) == 0 ) {
            string name;
            stream >> name;
            int count = 1;
            try {
                stream >> count;
            } catch ( ... ) {
            }

            tracer.break_on_tracepoint( name, count );
        } else if ( arg.find( "trace wait" ) == 0 ) {
            string name;
            stream >> name;

            tracer.wait_for_tracepoint( name );
        } else if ( arg.find( "trace continue" ) == 0 ) {
            string name;
            stream >> name;

            tracer.continue_on_tracepoint( name );
        } else if ( arg.find( "trace count" ) == 0 ) {
            string name;
            stream >> name;

            return to_string( tracer.get_tracepoint_count( name ) );
        } else if ( arg.find( "trace list" ) == 0 ) {
            set< string > keys = tracer.get_tracepoints();
            ostringstream out;
            for ( const string& key : keys )
                out << key << " ";
            return out.str();
        } else
            assert( false );

        return "ok";
    }  // "trace"

    return "";
};
