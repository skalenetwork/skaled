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
    typedef std::function< std::string( const std::string& arg ) > handler;

    SkaleDebugInterface();

    int add_handler( handler h );
    void remove_handler( int pos );

    std::string call( const std::string& arg );


private:
    std::vector< handler > handlers;
};

class SkaleDebugTracer {
public:
    void tracepoint( const std::string& name );

    void break_on_tracepoint( const std::string& name, int count );
    void wait_for_tracepoint( const std::string& name );
    void continue_on_tracepoint( const std::string& name );
    int get_tracepoint_count( const std::string& name ) {
        int rv = find_by_name( name ).pass_count;
        return rv;
    }

    std::set< std::string > get_tracepoints() const {
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
    std::mutex local_mutex;
    tracepoint_struct& find_by_name( const std::string& name ) {
        std::lock_guard< std::mutex > lock( local_mutex );
        return tracepoints[name];
    }
};

std::string DebugTracer_handler( const std::string& arg, SkaleDebugTracer& tracer );

#endif  // SKALEDEBUG_H
