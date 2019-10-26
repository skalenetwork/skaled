#ifndef SKALEDEBUG_H
#define SKALEDEBUG_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

class SkaleDebugInterface {
public:
    typedef std::recursive_mutex mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;

    typedef std::function< std::string( const std::string& arg ) > handler;

    SkaleDebugInterface();

    int add_handler( handler h );
    void remove_handler( int pos );

    std::string call( const std::string& arg );


private:
    mutable mutex_type mtx_;
    typedef std::vector< handler > vec_handlers_t;
    vec_handlers_t handlers;
};

class SkaleDebugTracer {
public:
    typedef std::recursive_mutex mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;

    void tracepoint( const std::string& name );

    void break_on_tracepoint( const std::string& name, int count );
    void wait_for_tracepoint( const std::string& name );
    void continue_on_tracepoint( const std::string& name );
    int get_tracepoint_count( const std::string& name ) {
        lock_type lock( mtx_ );
        int rv = find_by_name( name ).pass_count;
        return rv;
    }

    std::set< std::string > get_tracepoints() const {
        lock_type lock( mtx_ );
        std::set< std::string > res;
        for ( const auto& p : tracepoints ) {
            res.insert( p.first );
        }  // for
        return res;
    }

private:
    struct tracepoint_struct {
        int pass_count = 0;
        std::mutex thread_mutex, caller_mutex;
        std::condition_variable thread_cond, caller_cond;
        bool need_break = false;
        int waiting_count = 0;
        int needed_waiting_count = 0;
    };

    std::map< std::string, tracepoint_struct > tracepoints;
    tracepoint_struct& find_by_name( const std::string& name ) {
        lock_type lock( mtx_ );
        return tracepoints[name];
    }

    mutable mutex_type mtx_;
};

std::string DebugTracer_handler( const std::string& arg, SkaleDebugTracer& tracer );

#endif  // SKALEDEBUG_H
