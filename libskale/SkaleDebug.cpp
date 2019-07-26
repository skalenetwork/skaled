#include "SkaleDebug.h"

#include <cassert>
#include <sstream>

SkaleDebugInterface::SkaleDebugInterface() {}

int SkaleDebugInterface::add_handler( handler h ) {
    handlers.push_back( h );
    return handlers.size() - 1;
}

void SkaleDebugInterface::remove_handler( int pos ) {
    handlers.erase( handlers.begin() + pos );
}

std::string SkaleDebugInterface::call( const std::string& arg ) {
    for ( auto h : handlers ) {
        std::string res = h( arg );
        if ( !res.empty() )
            return res;
    }
    return "";
}

void SkaleDebugTracer::break_on_tracepoint( const std::string& name, int count ) {
    tracepoint_struct& obj = find_by_name( name );

    std::lock_guard< std::mutex > thread_lock( obj.thread_mutex );

    assert( !obj.need_break );
    obj.need_break = true;
    obj.needed_waiting_count = count;
}

void SkaleDebugTracer::wait_for_tracepoint( const std::string& name ) {
    tracepoint_struct& obj = find_by_name( name );

    std::unique_lock< std::mutex > lock( obj.caller_mutex );
    obj.caller_cond.wait( lock );
}

void SkaleDebugTracer::continue_on_tracepoint( const std::string& name ) {
    tracepoint_struct& obj = find_by_name( name );

    std::lock_guard< std::mutex > lock( obj.thread_mutex );
    assert( !obj.need_break );
    obj.thread_cond.notify_all();
}

void SkaleDebugTracer::tracepoint( const std::string& name ) {
    tracepoint_struct& obj = find_by_name( name );

    std::unique_lock< std::mutex > lock( obj.thread_mutex );
    ++obj.pass_count;
    if ( obj.need_break ) {
        ++obj.waiting_count;

        if ( obj.waiting_count == obj.needed_waiting_count ) {
            std::unique_lock< std::mutex > lock2( obj.caller_mutex );
            obj.need_break = false;
            obj.caller_cond.notify_all();
        }

        obj.thread_cond.wait( lock );
    }
}

std::string DebugTracer_handler( const std::string& arg, SkaleDebugTracer& tracer ) {
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
        } else
            assert( false );

        return "ok";
    }  // "trace"

    return "";
};
