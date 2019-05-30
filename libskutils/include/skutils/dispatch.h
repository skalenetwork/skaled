#if ( !defined SKUTILS_DISPATCH_H )
#define SKUTILS_DISPATCH_H 1

#include <skutils/atomic_shared_ptr.h>
#include <skutils/multithreading.h>
#include <skutils/thread_pool.h>
#include <stdint.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>

extern "C" {
struct uv_loop_s;
};  /// extern "C"

namespace skutils {
namespace dispatch {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::function< void() > fn_invoke_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// if uint64_t max value 18446744073709551615 nanoseconds, then...
// 18446744073709551615÷1000÷1000÷1000÷60÷60÷24÷360 = 593.06... years, i.e. approximately 500 years,
// i.e. acceptable but not too high
// typedef std::chrono::nanoseconds duration_t; // classic std::chrono::nanoseconds is based on
// int64_t - not uint64_t
typedef uint64_t duration_base_t;
typedef std::ratio< 1, 1000000000 > nano_t;
typedef std::chrono::duration< duration_base_t, nano_t > duration_t;
template < typename T >
duration_t duration_from_nanoseconds( const T& x ) {
    return duration_t( duration_base_t( x ) );
}
template < typename T >
duration_t duration_from_microiseconds( const T& x ) {
    return duration_t( duration_base_t( x ) * duration_base_t( 1000 ) );
}
template < typename T >
duration_t duration_from_milliseconds( const T& x ) {
    return duration_t( duration_base_t( x ) * duration_base_t( 1000 ) * duration_base_t( 1000 ) );
}
template < typename T >
duration_t duration_from_seconds( const T& x ) {
    return duration_t( duration_base_t( x ) * duration_base_t( 1000 ) * duration_base_t( 1000 ) *
                       duration_base_t( 1000 ) );
}
template < typename T >
duration_t duration_from_minutes( const T& x ) {
    return duration_t( duration_base_t( x ) * duration_base_t( 1000 ) * duration_base_t( 1000 ) *
                       duration_base_t( 1000 ) * duration_base_t( 60 ) );
}
template < typename T >
duration_t duration_from_hours( const T& x ) {
    return duration_t( duration_base_t( x ) * duration_base_t( 1000 ) * duration_base_t( 1000 ) *
                       duration_base_t( 1000 ) * duration_base_t( 60 ) * duration_base_t( 60 ) );
}
template < typename T >
duration_t duration_from_days( const T& x ) {
    return duration_t( duration_base_t( x ) * duration_base_t( 1000 ) * duration_base_t( 1000 ) *
                       duration_base_t( 1000 ) * duration_base_t( 60 ) * duration_base_t( 60 ) *
                       duration_base_t( 24 ) );
}
template < typename T >
duration_t duration_from_months( const T& x ) {
    return duration_t( duration_base_t( x ) * duration_base_t( 1000 ) * duration_base_t( 1000 ) *
                       duration_base_t( 1000 ) * duration_base_t( 60 ) * duration_base_t( 60 ) *
                       duration_base_t( 24 ) * duration_base_t( 31 ) );
}
template < typename T >
duration_t duration_from_years( const T& x ) {
    return duration_t( duration_base_t( x ) * duration_base_t( 1000 ) * duration_base_t( 1000 ) *
                       duration_base_t( 1000 ) * duration_base_t( 60 ) * duration_base_t( 60 ) *
                       duration_base_t( 24 ) * duration_base_t( 356 ) );
}

typedef std::string job_id_t;
typedef std::set< job_id_t > set_job_ids_t;

typedef std::string queue_id_t;
typedef std::set< queue_id_t > set_queue_ids_t;

typedef std::function< void() > job_t;
typedef std::deque< job_t > job_queue_t;

typedef std::function< bool( const job_id_t& id ) > job_state_event_pre_handler_t;
typedef std::function< void( const job_id_t& id ) > job_state_event_post_handler_t;
typedef std::function< void( const job_id_t& id, std::exception* pe ) > job_exception_handler_t;

typedef std::function< bool() > while_true_t;

typedef uint64_t priority_t;
typedef std::atomic_uint64_t atomic_priority_t;
#define SKUTILS_DISPATCH_PRIORITY_GOD skutils::dispatch::priority_t( 0 )
#define SKUTILS_DISPATCH_PRIORITY_BELOW_GOD skutils::dispatch::priority_t( 1 )
#define SKUTILS_DISPATCH_PRIORITY_ABSOLUTE skutils::dispatch::priority_t( 10 )
#define SKUTILS_DISPATCH_PRIORITY_HIGHEST skutils::dispatch::priority_t( 100 )
#define SKUTILS_DISPATCH_PRIORITY_HIGH skutils::dispatch::priority_t( 1000 )
#define SKUTILS_DISPATCH_PRIORITY_NORMAL skutils::dispatch::priority_t( 10000 )
#define SKUTILS_DISPATCH_PRIORITY_LOW skutils::dispatch::priority_t( 100000 )
#define SKUTILS_DISPATCH_PRIORITY_LOWEST skutils::dispatch::priority_t( 1000000 )

class loop;
class queue;
class domain;

extern skutils::multithreading::recursive_mutex_type& get_dispatch_mtx();

template < typename T >
class dispatch_shared_traits {
public:
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;
    typedef std::shared_ptr< T > stored_t;
    static mutex_type& mtx() { return skutils::dispatch::get_dispatch_mtx(); }
};  /// template class dispatch_shared_traits

typedef skutils::retain_release_ptr< loop, dispatch_shared_traits< loop > > loop_ptr_t;
typedef skutils::retain_release_ptr< queue, dispatch_shared_traits< queue > > queue_ptr_t;
typedef skutils::retain_release_ptr< domain, dispatch_shared_traits< domain > > domain_ptr_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// set of handy APIs for working with defaults
extern bool sleep_while_true(  // returns false if timeout is reached and fn() never returned false
    while_true_t fn,
    duration_t timeout = duration_t( 0 ),  // zero means no timeout
    duration_t step = duration_t( 10 * 1000 * 1000 ), size_t* p_nSleepStepsDone = nullptr );
extern domain_ptr_t default_domain( const size_t nNumberOfThreads = 0,  // 0 means use CPU count
    const size_t nQueueLimit = 0 );
extern const queue_id_t& get_default_queue_id();
extern queue_ptr_t get_default();
extern set_queue_ids_t get_all_names();  // get all queue names in default domain
extern queue_ptr_t get( const queue_id_t& id,
    bool isAutoAdd = true );  // add queue into default domain
extern std::string generate_id( void* ptr = nullptr, const char* strSys = nullptr );
extern queue_ptr_t get_auto_removeable();
extern queue_ptr_t add( const queue_id_t& id,
    priority_t pri = SKUTILS_DISPATCH_PRIORITY_NORMAL );       // add queue into default domain
extern priority_t priority( const queue_id_t& id );            // get
extern void priority( const queue_id_t& id, priority_t pri );  // set
extern void shutdown();                                        // shutdown default domain
extern size_t remove_all();                  // remove all queues from default domain
extern bool remove( const queue_id_t& id );  // remove queue from default domain

extern void async( const queue_id_t& id, job_t /*&*/ fn, duration_t timeout = duration_t( 0 ),
    duration_t interval = duration_t( 0 ), job_id_t* pJobID = nullptr );
extern void async( job_t /*&*/ fn, duration_t timeout = duration_t( 0 ),
    duration_t interval = duration_t( 0 ), job_id_t* pJobID = nullptr );
template < typename F, typename... Args >
inline void async( const queue_id_t& id, F&& f, duration_t timeout, duration_t interval,
    job_id_t* pJobID, Args&&... args ) {
    std::function< decltype( f( args... ) )() > fn =
        std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
    async( id, fn, timeout, interval, pJobID );
}
template < typename F, typename... Args >
inline void async(
    F&& f, duration_t timeout, duration_t interval, job_id_t* pJobID, Args&&... args ) {
    std::function< decltype( f( args... ) )() > fn =
        std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
    async( fn, timeout, interval, pJobID );
}

extern void sync( const queue_id_t& id, job_t /*&*/ fn );
template < typename F, typename... Args >
inline void sync( const queue_id_t& id, F&& f, duration_t timeout, duration_t interval,
    job_id_t* pJobID, Args&&... args ) {
    std::function< decltype( f( args... ) )() > fn =
        std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
    sync( id, fn, timeout, interval, pJobID );
}

extern void once(
    const queue_id_t& id, job_t /*&*/ fn, duration_t timeout, job_id_t* pJobID = nullptr );
extern void once( job_t /*&*/ fn, duration_t timeout, job_id_t* pJobID = nullptr );
template < typename F, typename... Args >
inline void once(
    const queue_id_t& id, F&& f, duration_t timeout, job_id_t* pJobID, Args&&... args ) {
    std::function< decltype( f( args... ) )() > fn =
        std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
    once( id, fn, timeout, pJobID );
}
template < typename F, typename... Args >
inline void once( F&& f, duration_t timeout, job_id_t* pJobID, Args&&... args ) {
    std::function< decltype( f( args... ) )() > fn =
        std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
    once( fn, timeout, pJobID );
}

// template only: once_with_future() is like once() but returns std::future < specified args >
template < typename F, typename... Args >
inline auto once_with_future( const queue_id_t& id, F&& f, duration_t timeout, Args&&... args )
    -> std::future< decltype( f( args... ) ) > {
    std::function< decltype( f( args... ) )() > fn =
        std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
    auto task_ptr = std::make_shared< std::packaged_task< decltype( f( args... ) )() > >( fn );
    std::function< void() > wrapper_func = [task_ptr]() -> void { ( *task_ptr )(); };
    once( id, wrapper_func, timeout );
    return task_ptr->get_future();
}
template < typename F, typename... Args >
inline auto once_with_future( F&& f, duration_t timeout, Args&&... args )
    -> std::future< decltype( f( args... ) ) > {
    std::function< decltype( f( args... ) )() > fn =
        std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
    auto task_ptr = std::make_shared< std::packaged_task< decltype( f( args... ) )() > >( fn );
    std::function< void() > wrapper_func = [task_ptr]() -> void { ( *task_ptr )(); };
    once( wrapper_func, timeout );
    return task_ptr->get_future();
}

// template only: async_with_future() is like once_with_future() without duration_t timeout
template < typename F, typename... Args >
inline auto async_with_future( const queue_id_t& id, F&& f, Args&&... args )
    -> std::future< decltype( f( args... ) ) > {
    std::function< decltype( f( args... ) )() > fn =
        std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
    auto task_ptr = std::make_shared< std::packaged_task< decltype( f( args... ) )() > >( fn );
    std::function< void() > wrapper_func = [task_ptr]() -> void { ( *task_ptr )(); };
    once( id, wrapper_func, duration_t( duration_base_t( 0 ) ) );
    return task_ptr->get_future();
}
template < typename F, typename... Args >
inline auto async_with_future( F&& f, Args&&... args ) -> std::future< decltype( f( args... ) ) > {
    std::function< decltype( f( args... ) )() > fn =
        std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
    auto task_ptr = std::make_shared< std::packaged_task< decltype( f( args... ) )() > >( fn );
    std::function< void() > wrapper_func = [task_ptr]() -> void { ( *task_ptr )(); };
    once( wrapper_func, duration_t( duration_base_t( 0 ) ) );
    return task_ptr->get_future();
}

// template only: async_without_future() is like async() without  duration_t timeout, duration_t
// interval, job_id_t * pJobID and without future
template < typename F, typename... Args >
inline void async_without_future( const queue_id_t& id, F&& f, Args&&... args ) {
    std::function< decltype( f( args... ) )() > fn =
        std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
    async( id, fn );
}
template < typename F, typename... Args >
inline void async_without_future( F&& f, Args&&... args ) {
    std::function< decltype( f( args... ) )() > fn =
        std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
    async( fn );
}

extern void repeat(
    const queue_id_t& id, job_t /*&*/ fn, duration_t interval, job_id_t* pJobID = nullptr );
extern void repeat( job_t /*&*/ fn, duration_t interval, job_id_t* pJobID = nullptr );
template < typename F, typename... Args >
inline void repeat(
    const queue_id_t& id, F&& f, duration_t interval, job_id_t* pJobID, Args&&... args ) {
    std::function< decltype( f( args... ) )() > fn =
        std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
    repeat( id, fn, interval, pJobID );
}
template < typename F, typename... Args >
inline void repeat( F&& f, duration_t interval, job_id_t* pJobID, Args&&... args ) {
    std::function< decltype( f( args... ) )() > fn =
        std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
    repeat( fn, interval, pJobID );
}

extern bool stop( const job_id_t& jobID );  // stops periodical work

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template < typename value_t >
class one_per_thread {
    typedef std::thread::id key_t;
    typedef std::map< key_t, value_t > map_t;
    map_t map_;

public:
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;

private:
    mutable mutex_type one_per_thread_mtx_;

public:
    one_per_thread() {}
    one_per_thread( const one_per_thread& ) = delete;
    one_per_thread( one_per_thread&& ) = delete;
    ~one_per_thread() {}  // no need to remove anything here
    one_per_thread& operator=( const one_per_thread& ) = delete;
    one_per_thread& operator=( one_per_thread&& ) = delete;
    mutex_type& one_per_thread_mtx() const { return one_per_thread_mtx_; }
    static key_t current_key() { return std::this_thread::get_id(); }
    bool get( const key_t& k, value_t& v ) {
        lock_type lock( one_per_thread_mtx() );
        auto itFind = map_.find( k ), itEnd = map_.end();
        if ( itFind == itEnd )
            return false;
        v = itFind->second;
        return true;
    }
    bool get( value_t& v ) { return get( current_key(), v ); }
    void set( const key_t& k, const value_t& v ) {
        lock_type lock( one_per_thread_mtx() );
        map_[k] = v;
    }
    bool set( const value_t& v ) { return set( current_key(), v ); }
    bool remove( const key_t& k ) {
        lock_type lock( one_per_thread_mtx() );
        auto itFind = map_.find( k ), itEnd = map_.end();
        if ( itFind == itEnd )
            return false;
        map_.erase( itFind );
        return true;
    }
    bool remove() { return remove( current_key() ); }
    bool push( const key_t& k, const value_t& v, value_t& prev ) {
        lock_type lock( one_per_thread_mtx() );
        auto itFind = map_.find( k ), itEnd = map_.end();
        bool have_previous = ( itFind != itEnd ) ? true : false;
        if ( have_previous )
            prev = itFind->second;
        map_[k] = v;
        return have_previous;
    }
    bool push( const value_t& v, value_t& prev ) { return push( current_key(), v, prev ); }
    void pop( const key_t& k, const value_t& prev, bool have_previous ) {
        if ( have_previous )
            set( k, prev );
        else
            remove( k );
    }
    void pop( const value_t& prev, bool have_previous ) {
        return pop( current_key(), prev, have_previous );
    }
    class make_current {
    public:
        one_per_thread& storage_;
        value_t previous_;
        bool have_previous_ = false;
        make_current( one_per_thread& storage, const value_t& v ) : storage_( storage ) {
            have_previous_ = storage_.push( v, previous_ );
        }
        make_current( const make_current& ) = delete;
        make_current( make_current&& ) = delete;
        ~make_current() { storage_.pop( previous_, have_previous_ ); }
        make_current& operator=( const make_current& ) = delete;
        make_current& operator=( make_current&& ) = delete;
    };  /// class make_current
};      /// template class one_per_thread

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class loop : public ref_retain_release {
    typedef one_per_thread< loop_ptr_t > one_per_thread_t;
    static one_per_thread_t g_one_per_thread;
    //
    std::atomic< uv_loop_s* > p_uvLoop_;
    std::atomic_bool cancelMode_, isAlive_;
    //
public:
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;

private:
    mutable mutex_type loop_mtx_;

public:
    typedef std::function< void() > on_check_jobs_t;
    on_check_jobs_t on_check_jobs_;
    //
    loop();
    loop( const loop& ) = delete;
    loop( loop&& ) = delete;
    ~loop();
    loop& operator=( const loop& ) = delete;
    loop& operator=( loop&& ) = delete;
    uv_loop_s* internal_handle() { return p_uvLoop_; }
    mutex_type& loop_mtx() const;
    static loop_ptr_t get_current();
    bool is_running() const;
    void cancel();
    bool wait_until_startup( duration_t timeout = duration_t( 0 ),  // zero means no timeout
        duration_t step = duration_t( 10 * 1000 * 1000 ) );
    bool wait( duration_t timeout = duration_t( 0 ),  // zero means no timeout
        duration_t step = duration_t( 10 * 1000 * 1000 ) );
    void run();
    void on_idle();
    void on_state_check();
    bool on_check_cancel_mode();
    void on_check_jobs();

private:
    //
    struct job_data_t : public ref_retain_release {
        loop_ptr_t pLoop_;
        job_id_t id_;
        job_t fn_;
        duration_t timeout_ = duration_t( 0 ), interval_;
        void* p_uvTimer_ = nullptr;
        std::atomic_bool needsCancel_;
        job_data_t( loop_ptr_t pLoop );
        job_data_t( const loop& ) = delete;
        job_data_t( loop&& ) = delete;
        ~job_data_t();
        job_data_t& operator=( const loop& ) = delete;
        job_data_t& operator=( loop&& ) = delete;
        void cancel();
        void init();
        void on_timer();
        void on_invoke();
    };  /// struct job_data_t
    //
    typedef skutils::retain_release_ptr< job_data_t, dispatch_shared_traits< job_data_t > >
        job_data_ptr_t;
    typedef std::map< job_id_t, job_data_ptr_t > map_jobs_t;
    map_jobs_t jobs_;
    //
    set_job_ids_t impl_job_get_all_ids();
    void impl_job_add(
        const job_id_t& id, job_t /*&*/ fn, duration_t timeout, duration_t interval );
    bool impl_job_remove( const job_id_t& id, bool isForce );
    bool impl_job_present( const job_id_t& id, duration_t* p_timeout = nullptr,
        duration_t* p_interval = nullptr, void** pp_uvTimer = nullptr );
    size_t impl_job_remove_all();
    //
public:
    //
    set_job_ids_t job_get_all_ids();
    bool job_remove( const job_id_t& id );
    bool job_present( const job_id_t& id, duration_t* p_timeout = nullptr,
        duration_t* p_interval = nullptr, void** pp_uvTimer = nullptr );
    size_t job_remove_all();
    void job_add( const job_id_t& id, job_t /*&*/ fn, duration_t timeout, duration_t interval );
    template < typename F, typename... Args >
    inline void job_add(
        const job_id_t& id, F&& f, duration_t timeout, duration_t interval, Args&&... args ) {
        std::function< decltype( f( args... ) )() > fn =
            std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
        job_add( id, fn, timeout, interval );
    }
    void job_add_once( const job_id_t& id, job_t /*&*/ fn, duration_t timeout );  // like JavaScript
                                                                                  // setTimeout()
    template < typename F, typename... Args >
    inline void job_add_once( const job_id_t& id, F&& f, duration_t timeout, Args&&... args ) {
        std::function< decltype( f( args... ) )() > fn =
            std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
        job_add_once( id, fn, timeout );
    }  // like JavaScript setTimeout()
    void job_add_periodic(
        const job_id_t& id, job_t /*&*/ fn, duration_t interval );  // like JavaScript setInterval()
    template < typename F, typename... Args >
    inline void job_add_periodic( const job_id_t& id, F&& f, duration_t interval, Args&&... args ) {
        std::function< decltype( f( args... ) )() > fn =
            std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
        job_add_periodic( id, fn, interval );
    }  // like JavaScript setInterval()
    //
    template < typename F, typename... Args >
    inline auto job_add_once_with_future( const job_id_t& id, F&& f, duration_t timeout,
        Args&&... args ) -> std::future< decltype( f( args... ) ) > {
        std::function< decltype( f( args... ) )() > fn =
            std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
        auto task_ptr = std::make_shared< std::packaged_task< decltype( f( args... ) )() > >( fn );
        std::function< void() > wrapper_func = [task_ptr]() -> void { ( *task_ptr )(); };
        job_add_once( id, wrapper_func, timeout );
        return task_ptr->get_future();
    }
    //
    job_state_event_pre_handler_t on_job_will_add_;
    job_state_event_post_handler_t on_job_did_added_;
    virtual bool on_job_will_add( const job_id_t& id );
    virtual void on_job_did_added( const job_id_t& id );
    job_state_event_pre_handler_t on_job_will_remove_;
    job_state_event_post_handler_t on_job_did_removed_;
    virtual bool on_job_will_remove( const job_id_t& id );
    virtual void on_job_did_removed( const job_id_t& id );
    job_state_event_pre_handler_t on_job_will_execute_;
    job_state_event_post_handler_t on_job_did_executed_;
    job_exception_handler_t on_job_exception_;
    virtual bool on_job_will_execute( const job_id_t& id );
    virtual void on_job_did_executed( const job_id_t& id );
    virtual void on_job_exception( const job_id_t& id, std::exception* pe );
    //
    void invoke_in_loop( fn_invoke_t fn );
    //
    friend class domain;
    friend struct job_data_t;
};  /// class loop

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class queue : public ref_retain_release {
    typedef one_per_thread< queue_ptr_t > one_per_thread_t;
    static one_per_thread_t g_one_per_thread;
    //
    domain_ptr_t pDomain_;
    const queue_id_t id_;
    atomic_priority_t priority_, accumulator_;
    std::atomic_bool is_removed_, is_running_, auto_remove_after_first_job_;
    //
public:
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;

private:
    mutable mutex_type mtx_jobs_;
    mutable mutex_type mtx_run_;
    //
    job_queue_t jobs_;
    std::atomic_size_t async_job_count_;  // cached value of jobs_.size()
    //
    std::atomic_size_t awaiting_sync_run_;

public:
    //
    queue( domain_ptr_t pDomain, const queue_id_t& id,
        priority_t pri = SKUTILS_DISPATCH_PRIORITY_NORMAL );
    queue( const queue& ) = delete;
    queue( queue&& ) = delete;
    ~queue();
    queue& operator=( const queue& ) = delete;
    queue& operator=( queue&& ) = delete;

public:
    mutex_type& mtx_jobs() const;
    mutex_type& mtx_run() const;
    static queue_ptr_t get_current();
    priority_t priority() const;
    queue& priority( priority_t pri );
    bool auto_remove_after_first_job() const;
    queue& auto_remove_after_first_job( bool b );
    bool is_removed() const;
    bool is_running() const;

private:
    size_t impl_job_cancel_all();
    bool impl_job_add( job_t /*&*/ fn, duration_t timeout, duration_t interval,
        job_id_t* pJobID );  // if both timeout and interval are zero, then invoke once
                             // asyncronously
    bool impl_job_run_sync( job_t /*&*/ fn );
    bool impl_job_run( job_t /*&*/ fn );  // run explicity specified job synchronosly
    bool impl_job_run();                  // fetch first asynchronosly stored job and run
public:
    domain_ptr_t get_domain();
    bool is_detached();
    queue_id_t get_id() const;
    size_t job_cancel_all();
    size_t async_job_count() const;
    bool job_add( job_t /*&*/ fn,
        duration_t timeout = duration_t( 0 ),  // if both timeout and interval are zero, then invoke
                                               // once asyncronously
        duration_t interval = duration_t( 0 ),
        job_id_t* pJobID = nullptr  // periodical job id
    );
    template < typename F, typename... Args >
    inline bool job_add(
        F&& f, duration_t timeout, duration_t interval, job_id_t* pJobID, Args&&... args ) {
        std::function< decltype( f( args... ) )() > fn =
            std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
        return job_add( fn, timeout, interval, pJobID );
    }
    bool job_add_once( job_t /*&*/ fn, duration_t timeout,
        job_id_t* pJobID = nullptr  // periodical job id
    );                              // like JavaScript setTimeout()
    template < typename F, typename... Args >
    inline bool job_add_once( F&& f, duration_t timeout, job_id_t* pJobID, Args&&... args ) {
        std::function< decltype( f( args... ) )() > fn =
            std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
        return job_add_once( fn, timeout, pJobID );
    }  // like JavaScript setTimeout()
    template < typename F, typename... Args >
    inline auto job_add_once_with_future( F&& f, duration_t timeout, Args&&... args )
        -> std::future< decltype( f( args... ) ) > {
        std::function< decltype( f( args... ) )() > fn =
            std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
        auto task_ptr = std::make_shared< std::packaged_task< decltype( f( args... ) )() > >( fn );
        std::function< void() > wrapper_func = [task_ptr]() -> void { ( *task_ptr )(); };
        job_add_once( wrapper_func, timeout );
        return task_ptr->get_future();
    }
    bool job_add_periodic( job_t /*&*/ fn, duration_t interval,
        job_id_t* pJobID = nullptr  // periodical job id
    );                              // like JavaScript setInterval()
    template < typename F, typename... Args >
    inline bool job_add_periodic( F&& f, duration_t interval, job_id_t* pJobID, Args&&... args ) {
        std::function< decltype( f( args... ) )() > fn =
            std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
        return job_add_periodic( fn, interval, pJobID );
    }  // like JavaScript setInterval()
    bool job_run_sync( job_t /*&*/ fn );
    template < typename F, typename... Args >
    inline bool job_run_sync( F&& f, Args&&... args ) {
        std::function< decltype( f( args... ) )() > fn =
            std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
        return job_run_sync( fn );
    }
    bool job_run( job_t /*&*/ fn );  // run explicity specified job synchronosly
    template < typename F, typename... Args >
    inline bool job_run( F&& f, Args&&... args ) {
        std::function< decltype( f( args... ) )() > fn =
            std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
        return job_run( fn );
    }
    bool job_run();  // fetch first asynchronosly stored job and run
    //
    friend class domain;
};  /// class queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class domain : public ref_retain_release {
public:
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;

private:
    std::atomic_size_t async_job_count_;  // sum of all cached values of jobs_.size() from all
                                          // queues
    priority_t accumulator_base_;
    // mutable mutex_type domain_mtx_;
    // mutable mutex_type mtx_with_jobs_;
    //
    std::atomic_bool shutdown_flag_;
    typedef std::mutex fetch_mutex_type;
    fetch_mutex_type fetch_mutex_;
    std::condition_variable fetch_lock_;
    //
    typedef std::map< queue_id_t, queue_ptr_t > map_queues_t;
    map_queues_t queues_;
    //
    struct queues_with_jobs_compare_t {
        bool operator()( const queue_ptr_t& q1, const queue_ptr_t& q2 ) const {
            priority_t acc1 = q1->accumulator_, acc2 = q2->accumulator_;
            if ( acc1 < acc2 )
                return true;
            if ( acc1 > acc2 )
                return false;
            priority_t pri1 = q1->priority(), pri2 = q2->priority();
            if ( pri1 < pri2 )
                return true;
            if ( pri1 > pri2 )
                return false;
            queue_id_t id1 = q1->get_id(), id2 = q2->get_id();
            if ( id1 < id2 )
                return true;
            return false;
        }
    };  // struct queues_with_jobs_compare_t
    typedef std::set< queue_ptr_t, queues_with_jobs_compare_t > queues_with_jobs_set_t;
    queues_with_jobs_set_t with_jobs_;
    //
    loop_ptr_t pLoop_;
    std::thread loop_thread_;
    //
    skutils::thread_pool thread_pool_;
    std::atomic_size_t cntRunningThreads_;
    //
    std::atomic_uint64_t decrease_accumulators_counter_, decrease_accumulators_period_;
    //
public:
    domain( const size_t nNumberOfThreads = 0,  // 0 means use CPU count
        const size_t nQueueLimit = 0 );
    domain( const domain& ) = delete;
    domain( domain&& ) = delete;
    virtual ~domain();
    domain& operator=( const domain& ) = delete;
    domain& operator=( domain&& ) = delete;
    mutex_type& domain_mtx() const;
    mutex_type& mtx_with_jobs() const;
    static domain_ptr_t get_current();

private:
    //
    size_t async_job_count() const;
    set_queue_ids_t impl_queue_get_all_names();
    queue_ptr_t impl_queue_add( const queue_id_t& id );
    queue_ptr_t impl_queue_get( const queue_id_t& id, bool isAutoAdd = false );
    bool impl_queue_remove( const queue_id_t& id );
    size_t impl_queue_remove_all();
    void impl_startup( size_t nWaitMilliSeconds = size_t( -1 ) );
    void impl_shutdown();
    queue_ptr_t impl_find_queue_to_run();  // find queue with minimal accumulator and remove it from
                                           // with_jobs_
    void impl_decrease_accumulators( priority_t acc_subtract );

public:
    //
    set_queue_ids_t queue_get_all_names();
    queue_ptr_t queue_add( const queue_id_t& id );
    queue_ptr_t queue_get( const queue_id_t& id, bool isAutoAdd = false );
    bool queue_remove( const queue_id_t& id );
    size_t queue_remove_all();
    void startup( size_t nWaitMilliSeconds = size_t( -1 ) );
    void shutdown();
    bool run_one();  // returns true if task job was executed
    loop_ptr_t get_loop();
    uint64_t decrease_accumulators_period() const;
    domain& decrease_accumulators_period( uint64_t n );
    //
private:
    void on_queue_job_added( queue& q );
    void on_queue_job_complete( queue& q );
    //
    friend class queue;
};  /// class domain

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace dispatch
};  // namespace skutils

#endif  /// (!defined SKUTILS_DISPATCH_H)
