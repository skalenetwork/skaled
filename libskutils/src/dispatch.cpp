#include <skutils/console_colors.h>
#include <skutils/dispatch.h>
#include <skutils/utils.h>
#include <time.h>
#include <uv.h>
#include <algorithm>
#include <exception>
#include <iostream>
#include <stdexcept>

#define LOCAL_DEBUG_TRACE( x )
// static inline void LOCAL_DEBUG_TRACE( const std::string & x ) { if( x.empty() ) return;
// std::cout.flush(); std::cerr.flush(); std::cout << x << "\n"; std::cout.flush(); }

namespace skutils {
namespace dispatch {

skutils::multithreading::recursive_mutex_type& get_dispatch_mtx() {
    // static skutils::multithreading::recursive_mutex_type g_dispatch_mtx(
    // "skutils::dispatch::g_dispatch_mtx" ); return g_dispatch_mtx;
    return skutils::get_ref_mtx();
}

static void stat_sleep( duration_t how_much ) {
    // auto nNanoSeconds = how_much.count();
    // struct timespec ts{ time_t(nNanoSeconds/1000000000), long(nNanoSeconds%1000000000) };
    // nanosleep( &ts,nullptr );
    std::this_thread::sleep_for( how_much );
}
bool sleep_while_true(  // returns false if timeout is reached and fn() never returned false
    while_true_t fn,
    duration_t timeout,        // = duration_t(0) // zero means no timeout
    duration_t step,           // = duration_t(10*1000*1000)
    size_t* p_nSleepStepsDone  // = nullptr
) {
    bool isTimeout = false;
    size_t nSleepStepsDone = 0;
    if ( fn ) {
        uint64_t last_time = uv_hrtime(), dur = 0, ns_timeout = timeout.count();
        for ( ; fn(); ) {
            ++nSleepStepsDone;
            stat_sleep( step );
            uint64_t next_time = uv_hrtime();
            dur += next_time - last_time;
            next_time = last_time;
            if ( ns_timeout && dur >= ns_timeout ) {
                isTimeout = true;
                break;
            }
        }
    }
    if ( p_nSleepStepsDone )
        ( *p_nSleepStepsDone ) = nSleepStepsDone;
    return ( !isTimeout );
}

domain_ptr_t default_domain( const size_t nNumberOfThreads,  // = 0 // 0 means use CPU count
    const size_t nQueueLimit                                 // = 0
) {
    static domain_ptr_t g_ptr( new domain( nNumberOfThreads, nQueueLimit ) );
    g_ptr->startup();
    return g_ptr;
}

const queue_id_t& get_default_queue_id() {
    static const queue_id_t g_idDefaultQueue( "default" );
    return g_idDefaultQueue;
}
queue_ptr_t get_default() {
    domain_ptr_t pDomain = default_domain();
    queue_ptr_t pQueue( pDomain->queue_get( get_default_queue_id(), true ) );
    if ( !pQueue )
        throw std::runtime_error( "dispatch simple get_default() API failed" );
    return pQueue;
}
set_queue_ids_t get_all_names() {  // get all queue names in default domain
    domain_ptr_t pDomain = default_domain();
    return pDomain->queue_get_all_names();
}
queue_ptr_t get( const queue_id_t& id,
    bool isAutoAdd  // = true
) {                 // add queue into default domain
    domain_ptr_t pDomain = default_domain();
    queue_ptr_t pQueue( pDomain->queue_get( id, isAutoAdd ) );
    if ( !pQueue )
        throw std::runtime_error( "dispatch simple get() API failed, bad queue " + id );
    return pQueue;
}
std::string generate_id( void* ptr /*= nullptr*/, const char* strSys /*= nullptr*/ ) {
    static std::mutex g_mtx;
    std::lock_guard< std::mutex > lock( g_mtx );
    static volatile uint64_t g_counter_ = 0;
    std::stringstream ss;
    ss << "auto";
    if ( ptr )
        ss << " " << ptr;
    if ( strSys && strSys[0] != '\0' )
        ss << " " << strSys;
    ss << " " << cc::now2string( true, false, false ) << " " << g_counter_;
    ++g_counter_;
    std::string s = ss.str();
    return s;
}
queue_ptr_t get_auto_removeable() {
    queue_id_t id( generate_id( nullptr, "auto_queue" ) );
    queue_ptr_t pQueue( get( id ) );
    if ( !pQueue )
        throw std::runtime_error(
            "dispatch simple get_auto_removeable() API failed, bad auto queue " + id );
    pQueue->auto_remove_after_first_job( true );
    return pQueue;
}
queue_ptr_t add( const queue_id_t& id,
    priority_t pri  // = SKUTILS_DISPATCH_PRIORITY_NORMAL
) {                 // add queue into default domain
    domain_ptr_t pDomain = default_domain();
    queue_ptr_t pQueue( pDomain->queue_add( id ) );
    if ( !pQueue )
        throw std::runtime_error( "dispatch simple add() API failed, bad queue " + id );
    pQueue->priority( pri );
    return pQueue;
}
void shutdown() {  // shutdown default domain
    domain_ptr_t pDomain = default_domain();
    pDomain->shutdown();
}
size_t remove_all() {  // remove all queues from default domain
    domain_ptr_t pDomain = default_domain();
    return pDomain->queue_remove_all();
}
bool remove( const queue_id_t& id ) {  // add queue from default domain
    domain_ptr_t pDomain = default_domain();
    return pDomain->queue_remove( id );
}
priority_t priority( const queue_id_t& id ) {  // get
    queue_ptr_t pQueue( get( id ) );
    if ( !pQueue )
        throw std::runtime_error(
            "dispatch simple priority() API (getter) failed, bad queue " + id );
    return pQueue->priority();
}
void priority( const queue_id_t& id, priority_t pri ) {  // set
    queue_ptr_t pQueue( get( id ) );
    if ( !pQueue )
        throw std::runtime_error(
            "dispatch simple priority() API (setter) failed, bad queue " + id );
    pQueue->priority( pri );
}

void async( const queue_id_t& id, job_t /*&*/ fn, duration_t timeout /*= duration_t(0)*/,
    duration_t interval /*= duration_t(0)*/, job_id_t* pJobID /*= nullptr*/ ) {
    queue_ptr_t pQueue( id.empty() ? get_auto_removeable() : get( id ) );
    if ( !pQueue )
        throw std::runtime_error( "dispatch simple async() API failed, bad queue " + id );
    bool b = pQueue->job_add( fn, timeout, interval, pJobID );
    if ( !b )
        throw std::runtime_error( "dispatch simple async() API failed" );
}
void async( job_t /*&*/ fn, duration_t timeout /*= duration_t(0)*/,
    duration_t interval /*= duration_t(0)*/, job_id_t* pJobID /*= nullptr*/ ) {
    async( queue_id_t( "" ), fn, timeout, interval, pJobID );
}
void sync( const queue_id_t& id, job_t /*&*/ fn ) {
    if ( id.empty() )
        throw std::runtime_error( "dispatch simple sync() API failed, needs non-empty queue id" );
    queue_ptr_t pQueue( get( id ) );
    if ( !pQueue )
        throw std::runtime_error( "dispatch simple sync() API failed, bad queue " + id );
    bool b = pQueue->job_run_sync( fn );
    if ( !b )
        throw std::runtime_error( "dispatch simple sync() API failed" );
}
void once(
    const queue_id_t& id, job_t /*&*/ fn, duration_t timeout, job_id_t* pJobID /*= nullptr*/ ) {
    queue_ptr_t pQueue( id.empty() ? get_auto_removeable() : get( id ) );
    if ( !pQueue )
        throw std::runtime_error( "dispatch simple once() API failed, bad queue " + id );
    bool b = pQueue->job_add_once( fn, timeout, pJobID );
    if ( !b )
        throw std::runtime_error( "dispatch simple once() API failed" );
}
void once( job_t /*&*/ fn, duration_t timeout, job_id_t* pJobID /*= nullptr*/ ) {
    once( queue_id_t( "" ), fn, timeout, pJobID );
}
void repeat(
    const queue_id_t& id, job_t /*&*/ fn, duration_t interval, job_id_t* pJobID /*= nullptr*/ ) {
    queue_ptr_t pQueue( id.empty() ? get_auto_removeable() : get( id ) );
    if ( !pQueue )
        throw std::runtime_error( "dispatch simple repeat() API failed, bad queue " + id );
    bool b = pQueue->job_add_periodic( fn, interval, pJobID );
    if ( !b )
        throw std::runtime_error( "dispatch simple repeat() API failed" );
}
void repeat( job_t /*&*/ fn, duration_t interval, job_id_t* pJobID /*= nullptr*/ ) {
    repeat( queue_id_t( "" ), fn, interval, pJobID );
}
bool stop( const job_id_t& jobID ) {  // stops periodical work
    domain_ptr_t pDomain = default_domain();
    return pDomain->get_loop()->job_remove( jobID );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

loop::one_per_thread_t loop::g_one_per_thread;

loop::loop() : p_uvLoop_( nullptr ), cancelMode_( false ) {}
loop::~loop() {
    job_remove_all();
    cancel();
    wait();
}
loop::mutex_type& loop::loop_mtx() const {
    return loop_mtx_;
}
loop_ptr_t loop::get_current() {
    loop_ptr_t pLoop;
    if ( !g_one_per_thread.get( pLoop ) )
        pLoop = nullptr;
    return pLoop;
}
bool loop::is_running() const {
    uv_loop_t* p = ( uv_loop_t* ) ( void* ) p_uvLoop_;
    if ( !p )
        return false;
    if ( !uv_loop_alive( p ) )
        return false;
    return true;
}
void loop::cancel() {
    cancelMode_ = true;
}
bool loop::wait_until_startup( duration_t timeout,  // = duration_t(0) // zero means no timeout
    duration_t step                                 // = duration_t(10*1000*1000)
) {
    return sleep_while_true( [this]() -> bool { return ( !is_running() ); }, timeout, step );
}
bool loop::wait( duration_t timeout,  // = duration_t(0) // zero means no timeout
    duration_t step                   // = duration_t(10*1000*1000)
) {
    return sleep_while_true( [this]() -> bool { return is_running(); }, timeout, step );
}
void loop::run() {
    uv_loop_t uvLoop;
    uv_idle_t uvIdler;
    uv_timer_t uvTimerStateCheck;
    {  // block
        lock_type lock( loop_mtx() );
        if ( is_running() )
            throw std::runtime_error(
                skutils::tools::format( "loop %p is already running", this ) );
        isAlive_ = false;
        try {
            uv_loop_init( &uvLoop );
            uvLoop.data = ( void* ) this;
            //
            uv_idle_init( &uvLoop, &uvIdler );
            uvIdler.data = ( void* ) this;
            //
            uv_timer_init( &uvLoop, &uvTimerStateCheck );
            uvTimerStateCheck.data = ( void* ) this;
            //
            p_uvLoop_ = ( &uvLoop );
            cancelMode_ = false;
            //
            uv_idle_start( &uvIdler, []( uv_idle_t* p_uvIdler ) {
                loop* pLoop = ( loop* ) ( p_uvIdler->data );
                pLoop->on_idle();
            } );
            uv_timer_start( &uvTimerStateCheck,
                []( uv_timer_t* p_uvTimer ) {
                    loop* pLoop = ( loop* ) ( p_uvTimer->data );
                    pLoop->on_state_check();
                },
                200,  // timeout milliseconds
                200   // repeat milliseconds
            );
        } catch ( ... ) {
            p_uvLoop_ = nullptr;
        }
        if ( !is_running() )
            throw std::runtime_error( skutils::tools::format( "loop %p init failed", this ) );
    }  // block
    try {
        {  // block
            loop_ptr_t ptr( this );
            one_per_thread_t::make_current curr( g_one_per_thread, ptr );
            uv_run( &uvLoop, UV_RUN_DEFAULT );
        }  // block
        p_uvLoop_ = nullptr;
        uv_loop_close( &uvLoop );
        cancelMode_ = false;
    } catch ( ... ) {
        p_uvLoop_ = nullptr;
        throw;
    }
    p_uvLoop_ = nullptr;
}

void loop::on_idle() {
    isAlive_ = true;
    if ( on_check_cancel_mode() )
        return;
    on_check_jobs();
}
void loop::on_state_check() {
    isAlive_ = true;
    if ( on_check_cancel_mode() )
        return;
    on_check_jobs();
}
bool loop::on_check_cancel_mode() {
    if ( cancelMode_ ) {
        // cancelMode_ = false;
        uv_loop_t* p_uvLoop = ( uv_loop_t* ) ( void* ) p_uvLoop_;
        if ( p_uvLoop )
            uv_stop( p_uvLoop );
        return true;
    }
    return false;
}
void loop::on_check_jobs() {
    if ( on_check_jobs_ )
        on_check_jobs_();
}

loop::job_data_t::job_data_t( loop_ptr_t pLoop ) : pLoop_( pLoop ), needsCancel_( false ) {
    p_uvTimer_ = calloc( 1, sizeof( uv_timer_t ) );
    if ( !p_uvTimer_ )
        throw std::runtime_error( "loop job timer allocation error" );
}
loop::job_data_t::~job_data_t() {
    if ( p_uvTimer_ ) {
        cancel();
        free( p_uvTimer_ );
        p_uvTimer_ = nullptr;
    }
    pLoop_ = nullptr;
}
void loop::job_data_t::cancel() {
    if ( needsCancel_ ) {
        needsCancel_ = false;
        uv_timer_t* p_uvTimer = ( uv_timer_t* ) p_uvTimer_;
        if ( p_uvTimer ) {
            if ( pLoop_->is_running() )
                uv_timer_stop( p_uvTimer );
        }
        pLoop_ = nullptr;
    }
}
void loop::job_data_t::init() {
    if ( !pLoop_ )
        return;
    uv_loop_t* p_uvLoop = ( uv_loop_t* ) ( void* ) pLoop_->p_uvLoop_;
    uv_timer_t* p_uvTimer = ( uv_timer_t* ) p_uvTimer_;
    uv_timer_init( p_uvLoop, p_uvTimer );
    p_uvTimer->data = ( void* ) this;
    needsCancel_ = true;
    // nanoseconds
    uint64_t ns_timeout = timeout_.count(), ns_interval = interval_.count();
    // milliseconds
    uint64_t ms_timeout =
        ( ns_timeout / ( 1000 * 1000 ) ) + ( ( ( ns_timeout % ( 1000 * 1000 ) ) != 0 ) ? 1 : 0 );
    uint64_t ms_interval =
        ( ns_interval / ( 1000 * 1000 ) ) + ( ( ( ns_interval % ( 1000 * 1000 ) ) != 0 ) ? 1 : 0 );
    uv_timer_start( p_uvTimer,
        []( uv_timer_t* pTimer ) {
            loop::job_data_t* pJobData = ( loop::job_data_t* ) ( pTimer->data );
            pJobData->on_timer();
        },
        ms_timeout, ms_interval );
}
void loop::job_data_t::on_timer() {
    on_invoke();
}
void loop::job_data_t::on_invoke() {
    job_id_t id( id_ );
    loop_ptr_t pLoop( pLoop_ );
    try {
        if ( !pLoop )
            return;
        // loop_ptr_t ptr( &loop_ );
        // one_per_thread_t::make_current curr( g_one_per_thread, ptr );
        if ( !pLoop->on_job_will_execute( id ) )
            return;
        if ( fn_ )
            fn_();
        pLoop->on_job_did_executed( id );
    } catch ( std::exception& e ) {
        pLoop->on_job_exception( id, &e );
    } catch ( ... ) {
        pLoop->on_job_exception( id, nullptr );
    }
    uint64_t ns_interval = interval_.count();
    if ( ns_interval == 0 ) {
        cancel();
        pLoop->job_remove( id );
    }
}

set_job_ids_t loop::impl_job_get_all_ids() {
    set_queue_ids_t all_job_ids;
    for ( const auto& entry : jobs_ ) {
        all_job_ids.insert( entry.first );
    }
    return all_job_ids;
}
void loop::impl_job_add(
    const job_id_t& id, job_t /*&*/ fn, duration_t timeout, duration_t interval ) {
    if ( impl_job_present( id ) )
        throw std::runtime_error(
            "loop need unique job identifier, but specified \"" + id + "\" is not unique" );
    if ( !fn )
        throw std::runtime_error( "loop cannot add empty job" );
    if ( !is_running() )
        throw std::runtime_error( "loop is not running, cannot add job" );
    if ( !on_job_will_add( id ) )
        throw std::runtime_error( "job adding is disabled by callback" );
    job_data_ptr_t pJobData( new job_data_t( this ) );
    pJobData->id_ = id;
    pJobData->fn_ = fn;
    pJobData->timeout_ = timeout;
    pJobData->interval_ = interval;
    jobs_[id] = pJobData.get();
    pJobData->init();
    on_job_did_added( id );
}
bool loop::impl_job_remove( const job_id_t& id, bool isForce ) {
    job_id_t cached_id( id );
    auto itFind = jobs_.find( cached_id ), itEnd = jobs_.end();
    if ( itFind == itEnd )
        return false;
    if ( !on_job_will_remove( cached_id ) ) {
        if ( !isForce )
            return false;
    }
    job_data_ptr_t pJobData = itFind->second;
    pJobData->cancel();
    jobs_.erase( cached_id );
    // delete pJobData;
    on_job_did_removed( cached_id );
    return true;
}
bool loop::impl_job_present( const job_id_t& id,
    duration_t* p_timeout,   // = nullptr
    duration_t* p_interval,  // = nullptr
    void** pp_uvTimer        // = nullptr
) {
    if ( p_timeout )
        ( *p_timeout ) = duration_t( 0 );
    if ( p_interval )
        ( *p_interval ) = duration_t( 0 );
    if ( pp_uvTimer )
        ( *pp_uvTimer ) = nullptr;
    auto itFind = jobs_.find( id ), itEnd = jobs_.end();
    if ( itFind == itEnd )
        return false;
    job_data_ptr_t pJobData = itFind->second;
    if ( p_timeout )
        ( *p_timeout ) = pJobData->timeout_;
    if ( p_interval )
        ( *p_interval ) = pJobData->interval_;
    if ( pp_uvTimer )
        ( *pp_uvTimer ) = pJobData->p_uvTimer_;
    return true;
}
size_t loop::impl_job_remove_all() {
    size_t cntRemoved = 0;
    set_job_ids_t all_job_ids = impl_job_get_all_ids();
    for ( const auto& id : all_job_ids ) {
        if ( impl_job_remove( id, true ) )
            cntRemoved++;
    }
    return cntRemoved;
}

set_job_ids_t loop::job_get_all_ids() {
    lock_type lock( loop_mtx() );
    return impl_job_get_all_ids();
}
bool loop::job_remove( const job_id_t& id ) {
    lock_type lock( loop_mtx() );
    return impl_job_remove( id, false );
}
bool loop::job_present( const job_id_t& id,
    duration_t* p_timeout,   // = nullptr
    duration_t* p_interval,  // = nullptr
    void** pp_uvTimer        // = nullptr
) {
    lock_type lock( loop_mtx() );
    return job_present( id, p_timeout, p_interval, pp_uvTimer );
}
size_t loop::job_remove_all() {
    lock_type lock( loop_mtx() );
    return impl_job_remove_all();
}
void loop::job_add( const job_id_t& id, job_t /*&*/ fn, duration_t timeout, duration_t interval ) {
    lock_type lock( loop_mtx() );
    return impl_job_add( id, fn, timeout, interval );
}
void loop::job_add_once(
    const job_id_t& id, job_t /*&*/ fn, duration_t timeout ) {  // like JavaScript setTimeout()
    job_add( id, fn, timeout, duration_t( 0 ) );
}
void loop::job_add_periodic(
    const job_id_t& id, job_t /*&*/ fn, duration_t interval ) {  // like JavaScript setInterval()
    job_add( id, fn, duration_t( 0 ), interval );
}

bool loop::on_job_will_add( const job_id_t& id ) {
    if ( on_job_will_add_ ) {
        if ( !on_job_will_add_( id ) )
            return false;
    }
    return true;
}
void loop::on_job_did_added( const job_id_t& id ) {
    if ( on_job_did_added_ )
        on_job_did_added_( id );
}
bool loop::on_job_will_remove( const job_id_t& id ) {
    if ( on_job_will_remove_ ) {
        if ( !on_job_will_remove_( id ) )
            return false;
    }
    return true;
}
void loop::on_job_did_removed( const job_id_t& id ) {
    if ( on_job_did_removed_ )
        on_job_did_removed_( id );
}
bool loop::on_job_will_execute( const job_id_t& id ) {
    if ( on_job_will_execute_ ) {
        if ( !on_job_will_execute_( id ) )
            return false;
    }
    return true;
}
void loop::on_job_did_executed( const job_id_t& id ) {
    if ( on_job_did_executed_ )
        on_job_did_executed_( id );
}
void loop::on_job_exception( const job_id_t& id, std::exception* pe ) {
    if ( on_job_exception_ )
        on_job_exception_( id, pe );
}

struct invoke_in_loop_data : public uv_async_t {
    fn_invoke_t fn_;
};

void loop::invoke_in_loop( fn_invoke_t fn ) {
    if ( !fn )
        return;
    uv_loop_t* uvl = ( uv_loop_t* ) ( void* ) p_uvLoop_;
    invoke_in_loop_data* idt = new invoke_in_loop_data;
    // idt.data = ...;
    idt->fn_ = fn;
    uv_async_init( uvl, idt, []( uv_async_t* uva ) {
        invoke_in_loop_data* idt = ( invoke_in_loop_data* ) uva;
        try {
            idt->fn_();
        } catch ( ... ) {
        }
        delete idt;
    } );
    uv_async_send( idt );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

queue::one_per_thread_t queue::g_one_per_thread;

queue::queue( domain_ptr_t pDomain, const queue_id_t& id,
    priority_t pri  // = SKUTILS_DISPATCH_PRIORITY_NORMAL
    )
    : pDomain_( pDomain ),
      id_( id ),
      priority_( pri ),
      accumulator_( pDomain->accumulator_base_ ),
      is_removed_( false ),
      is_running_( false ),
      auto_remove_after_first_job_( false ),
      mtx_jobs_( skutils::tools::format( "skutils::dispatch::queue-%p/mutex/jobs", this ) ),
      mtx_run_( skutils::tools::format( "skutils::dispatch::queue-%p/mutex/run", this ) ),
      async_job_count_( 0 )  // cached value of jobs_.size()
      ,
      awaiting_sync_run_( 0 ) {}
queue::~queue() {
    job_cancel_all();
}
queue::mutex_type& queue::mtx_jobs() const {
    return mtx_jobs_;
}
queue::mutex_type& queue::mtx_run() const {
    return mtx_run_;
}
domain_ptr_t queue::get_domain() {
    return pDomain_;
}
bool queue::is_detached() {
    if ( !pDomain_ )
        return true;
    return false;
}
queue_id_t queue::get_id() const {
    return id_;
}
queue_ptr_t queue::get_current() {
    queue_ptr_t pQueue;
    if ( !g_one_per_thread.get( pQueue ) )
        pQueue = nullptr;
    return pQueue;
}
priority_t queue::priority() const {
    // lock_type lock( mtx_jobs() );
    priority_t pri = priority_;
    return pri;
}
queue& queue::priority( priority_t pri ) {
    // lock_type lock( mtx_jobs() );
    priority_ = pri;
    return ( *this );
}

bool queue::auto_remove_after_first_job() const {
    // lock_type lock( mtx_jobs() );
    bool b = auto_remove_after_first_job_;
    return b;
}
queue& queue::auto_remove_after_first_job( bool b ) {
    // lock_type lock( mtx_jobs() );
    auto_remove_after_first_job_ = b;
    return ( *this );
}
bool queue::is_removed() const {
    return is_removed_;
}
bool queue::is_running() const {
    return is_running_;
}

size_t queue::impl_job_cancel_all() {
    size_t cnt = 0, async_job_count = 0;
    {  // block
        lock_type lock( mtx_jobs() );
        cnt = jobs_.size();
        jobs_.clear();
        async_job_count = size_t( async_job_count_ );
    }  // block
    domain_ptr_t pDomain = pDomain_;
    if ( pDomain )
        pDomain->async_job_count_ -= async_job_count;  // async_job_count_
    async_job_count_ = 0;                              // cached value of jobs_.size()
    return cnt;
}
bool queue::impl_job_add( job_t /*&*/ fn, duration_t timeout, duration_t interval,
    job_id_t* pJobID ) {  // if both timeout and interval are zero, then invoke once asyncronously
    try {
        if ( pJobID )
            pJobID->clear();
        if ( !fn )
            return false;
        if ( is_removed() )
            return false;
        domain_ptr_t pDomain = pDomain_;
        if ( !pDomain )
            return false;
        if ( timeout.count() || interval.count() ) {
            loop_ptr_t pLoop = pDomain->get_loop();
            if ( !pLoop )
                return false;
            job_id_t id = generate_id( ( void* ) this, "queue" );
            if ( pJobID )
                ( *pJobID ) = id;
            queue_ptr_t pThis( this );
            pLoop->job_add( id,
                [pLoop, id, pThis, fn]() -> void {
                    queue_ptr_t& pQueue = const_cast< queue_ptr_t& >( pThis );
                    if ( pQueue->is_removed() ) {
                        loop_ptr_t& l = const_cast< loop_ptr_t& >( pLoop );
                        l->job_remove( id );
                    } else
                        pQueue->job_add( fn );
                },
                timeout, interval );
        } else {
            {  // block
                lock_type lock( mtx_jobs() );
                jobs_.push_back( fn );
            }  // block
            ++pDomain->async_job_count_;
            ++async_job_count_;  // cached value of jobs_.size()
            pDomain->on_queue_job_added( *this );
        }
        return true;
    } catch ( ... ) {
    }
    return false;
}
bool queue::impl_job_run() {  // fetch first asynchronosly stored job and run
    if ( is_removed() )
        return false;
    if ( awaiting_sync_run_ > 0 )
        return false;
    domain_ptr_t pDomain = pDomain_;
    if ( !pDomain )
        return false;
    job_t fn;
    {  // block
        skutils::multithreading::threadNameAppender tn( "/q-" + get_id() );
        lock_type lock( mtx_jobs() );
        if ( jobs_.empty() )
            return false;
        fn = jobs_.front();
        jobs_.pop_front();
        --async_job_count_;  // cached value of jobs_.size()
    }                        // block
    --pDomain->async_job_count_;
    if ( !impl_job_run( fn ) )
        return false;
    accumulator_ += priority();
    pDomain->on_queue_job_complete( *this );
    return true;
}
bool queue::impl_job_run_sync( job_t /*&*/ fn ) {
    domain_ptr_t pDomain = pDomain_;
    if ( !pDomain )
        return false;
    ++awaiting_sync_run_;
    bool rv = false;
    try {
        rv = impl_job_run( fn );
        --awaiting_sync_run_;
        accumulator_ += priority();
        pDomain->on_queue_job_complete( *this );
    } catch ( ... ) {
        --awaiting_sync_run_;
        rv = false;
    }
    if ( awaiting_sync_run_ == 0 )
        pDomain->fetch_lock_.notify_one();
    return rv;
}
bool queue::impl_job_run( job_t /*&*/ fn ) {  // run explicity specified job synchronosly
    if ( !fn )
        return false;
    if ( is_removed() )
        return false;
    domain_ptr_t pDomain = pDomain_;
    if ( !pDomain )
        return false;
    queue_id_t queue_id = get_id();
    try {
        try {
            skutils::multithreading::threadNameAppender tn( "/q-" + get_id() );
            lock_type lock( mtx_run() );
            is_running_ = true;
            queue_ptr_t ptr( this );
            one_per_thread_t::make_current curr( g_one_per_thread, ptr );
            fn();
            is_running_ = false;
        } catch ( ... ) {
            is_running_ = false;
            throw;
        }
    } catch ( ... ) {
    }
    try {
        if ( auto_remove_after_first_job() )
            pDomain->queue_remove( queue_id );
    } catch ( ... ) {
    }
    return true;
}

size_t queue::job_cancel_all() {
    if ( is_removed() )
        return false;
    return impl_job_cancel_all();
}
size_t queue::async_job_count() const {
    if ( is_removed() )
        return 0;
    size_t cntJobs = async_job_count_;  // cached value of jobs_.size()
    return cntJobs;
}
bool queue::job_add( job_t /*&*/ fn,
    duration_t timeout,   // = duration_t(0) // if both timeout and interval are zero, then invoke
                          // once asyncronously
    duration_t interval,  // = 0
    job_id_t* pJobID      // = nullptr // periodical job id
) {
    if ( pJobID )
        pJobID->clear();
    if ( !fn )
        return false;
    if ( is_removed() )
        return false;
    // lock_type lock( mtx_jobs() );
    return impl_job_add( fn, timeout, interval, pJobID );
}
bool queue::job_add_once( job_t /*&*/ fn, duration_t timeout,
    job_id_t* pJobID  // = nullptr // periodical job id
) {                   // like JavaScript setTimeout()
    if ( pJobID )
        pJobID->clear();
    if ( !fn )
        return false;
    if ( is_removed() )
        return false;
    // lock_type lock( mtx_jobs() );
    return impl_job_add( fn, timeout, duration_t( 0 ), pJobID );  // like JavaScript setTimeout()
}
bool queue::job_add_periodic( job_t /*&*/ fn, duration_t interval,
    job_id_t* pJobID  // = nullptr // periodical job id
) {                   // like JavaScript setInterval()
    if ( pJobID )
        pJobID->clear();
    if ( !fn )
        return false;
    if ( is_removed() )
        return false;
    // lock_type lock( mtx_jobs() );
    return impl_job_add( fn, duration_t( 0 ), interval, pJobID );  // like JavaScript setInterval()
}
bool queue::job_run_sync( job_t /*&*/ fn ) {
    if ( !fn )
        return false;
    if ( is_removed() )
        return false;
    // lock_type lock( mtx_run() );
    return impl_job_run_sync( fn );
}
bool queue::job_run( job_t /*&*/ fn ) {  // run explicity specified job synchronosly
    if ( !fn )
        return false;
    if ( is_removed() )
        return false;
    // lock_type lock( mtx_run() );
    return impl_job_run( fn );
}
bool queue::job_run() {  // fetch first asynchronosly stored job and run
    if ( is_removed() )
        return false;
    // lock_type lock( mtx_run() );
    return impl_job_run();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

domain::domain( const size_t nNumberOfThreads,  // = 0 // 0 means use CPU count
    const size_t nQueueLimit                    // = 0
    )
    : async_job_count_( 0 ),
      accumulator_base_( 0 )
      //	, domain_mtx_( skutils::tools::format("skutils::dispatch::domain-%p/mutex/main", this )
      //) 	, mtx_with_jobs_( skutils::tools::format("skutils::dispatch::domain-%p/mutex/with_jobs",
      // this ) )
      ,
      shutdown_flag_( true ),
      thread_pool_(
          ( nNumberOfThreads > 0 ) ? nNumberOfThreads : skutils::tools::cpu_count(), nQueueLimit ),
      cntRunningThreads_( 0 ),
      decrease_accumulators_counter_( uint64_t( 0 ) ),
      decrease_accumulators_period_( uint64_t( 1000 ) * uint64_t( 1000 ) )  // rare enough
{
    startup();
}
domain::~domain() {
    shutdown();
}
domain::mutex_type& domain::domain_mtx() const {
    return get_dispatch_mtx();
}
domain::mutex_type& domain::mtx_with_jobs() const {
    return domain_mtx();
}
domain_ptr_t domain::get_current() {
    queue_ptr_t pQueue;
    if ( !queue::g_one_per_thread.get( pQueue ) ) {
        domain_ptr_t pDomain;
        return pDomain;
    }
    domain_ptr_t pDomain = pQueue->get_domain();
    return pDomain;
}

size_t domain::async_job_count() const {
    if ( shutdown_flag_ )
        return 0;
    size_t cntJobs = async_job_count_;  // sum of all cached values of jobs_.size() from all queues
    return cntJobs;
}
set_queue_ids_t domain::impl_queue_get_all_names() {
    set_queue_ids_t all_queue_ids;
    for ( const auto& entry : queues_ ) {
        all_queue_ids.insert( entry.first );
    }
    return all_queue_ids;
}
queue_ptr_t domain::impl_queue_add( const queue_id_t& id ) {
    queue_ptr_t pQueue( impl_queue_get( id ) );
    if ( pQueue ) {
        pQueue = nullptr;  // already exist, return null
        return pQueue;
    }
    if ( !shutdown_flag_ ) {
        pQueue = new queue( domain_ptr_t( this ), id );
        queues_[id] = pQueue;
    }
    return pQueue;
}
queue_ptr_t domain::impl_queue_get( const queue_id_t& id,
    bool isAutoAdd  // = false
) {
    queue_ptr_t pQueue;
    auto itFind = queues_.find( id ), itEnd = queues_.end();
    if ( itFind == itEnd ) {
        if ( isAutoAdd )
            return impl_queue_add( id );
        return pQueue;
    }
    pQueue = itFind->second;
    return pQueue;
}
bool domain::impl_queue_remove( const queue_id_t& id ) {
    {  // block
        queue_ptr_t pQueue;
        {  // block
            lock_type lock( domain_mtx() );
            auto itFind = queues_.find( id ), itEnd = queues_.end();
            if ( itFind == itEnd )
                return false;
            pQueue = itFind->second;
            queues_.erase( id );
        }  // block
        pQueue->is_removed_ = true;
        sleep_while_true( [&]() -> bool { return pQueue->is_running(); } );  // wait for async tasks
                                                                             // to complete, if any
        pQueue->pDomain_ = nullptr;
        async_job_count_ -= size_t( pQueue->async_job_count_ );
        // delete q;
    }
    {  // block
        lock_type lock_with_jobs( mtx_with_jobs() );
        queues_with_jobs_set_t::iterator itFrom = with_jobs_.begin(), itTo = with_jobs_.end();
        queues_with_jobs_set_t::iterator itFound =
            std::find_if( itFrom, itTo, [&id]( const queue_ptr_t& pQueue ) -> bool {
                if ( pQueue->get_id() == id )
                    return true;
                return false;
            } );
        if ( itFound != itTo ) {
            with_jobs_.erase( itFound );
            LOCAL_DEBUG_TRACE( cc::sunny( "domain::impl_queue_remove()" ) + cc::debug( " did " ) +
                               cc::error( "erased" ) + cc::debug( " already-removed queue " ) +
                               cc::bright( id ) + cc::debug( ", " ) +
                               cc::sunny( "with_jobs_.size()" ) + cc::debug( "=" ) +
                               cc::size10( with_jobs_.size() ) );
        }
    }  // block
    return true;
}
size_t domain::impl_queue_remove_all() {
    size_t cntRemoved = 0;
    set_queue_ids_t all_queue_ids;
    {  // block
        lock_type lock( domain_mtx() );
        all_queue_ids = impl_queue_get_all_names();
    }  // block
    for ( const auto& id : all_queue_ids ) {
        if ( impl_queue_remove( id ) )
            cntRemoved++;
    }
    return cntRemoved;
}
void domain::impl_startup( size_t nWaitMilliSeconds /*= size_t(-1)*/ ) {
    if ( !shutdown_flag_ )
        return;
    lock_type lock( domain_mtx() );
    //			if( ! shutdown_flag_ )
    //				return;
    shutdown_flag_ = false;
    size_t idxThread, cntThreads = thread_pool_.number_of_threads();
    if ( cntThreads == 0 )
        throw std::runtime_error( "dispatch domain failed to initialize thread pool" );
    // init thread pool
    std::atomic_size_t cntFailedToStartThreads;
    cntFailedToStartThreads = 0;
    size_t cntThreadsToStart = cntThreads;
    for ( ; true; ) {
        for ( idxThread = 0; idxThread < cntThreadsToStart; ++idxThread ) {
            bool bThreadStartedOK = thread_pool_.safe_submit_without_future( [this]() {
                ++cntRunningThreads_;
                try {
                    for ( ; true; ) {
                        if ( shutdown_flag_ )
                            break;
                        {  // block
                            std::unique_lock< fetch_mutex_type > lock( fetch_mutex_ );
                            fetch_lock_.wait( lock );
                        }  // block
                        if ( shutdown_flag_ )
                            break;
                        //
                        // run_one();
                        //
                        for ( ; run_one(); ) {
                            if ( shutdown_flag_ )
                                break;
                            // fetch_lock_.notify_one(); // spread the work into other threads
                        }
                    }  /// for( ; true ; )
                } catch ( ... ) {
                }
                --cntRunningThreads_;
            } );
            if ( !bThreadStartedOK )
                ++cntFailedToStartThreads;
        }  // for( idxThread = 0; idxThread < cntThreadsToStart; ++ idxThread )
        cntThreadsToStart = size_t( cntFailedToStartThreads );
        if ( cntThreadsToStart == 0 )
            break;
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }  // for ( ; true; )
    size_t cntStartedAndRunningThreads = size_t( cntRunningThreads_ );
    size_t cntWaitAttempts =
        ( nWaitMilliSeconds == 0 || nWaitMilliSeconds == size_t( -1 ) ) ? 3000 : nWaitMilliSeconds;
    for ( size_t idxWaitAttempt = 0; idxWaitAttempt < cntWaitAttempts; ++idxWaitAttempt ) {
        if ( cntStartedAndRunningThreads == cntThreads )
            break;
        std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
        cntStartedAndRunningThreads = size_t( cntRunningThreads_ );
    }
    if ( cntStartedAndRunningThreads != cntThreads )
        throw std::runtime_error(
            "dispatch domain failed to initialize all threads in thread pool" );
}
void domain::impl_shutdown() {
    // tell threads to shutdown
    shutdown_flag_ = true;
    fetch_lock_.notify_all();
    // tell loop to shutdown
    loop_ptr_t pLoop( pLoop_ );
    pLoop_ = nullptr;
    if ( pLoop )
        pLoop->cancel();
    // wait threads to shutdown
    size_t cntThreads = thread_pool_.number_of_threads();
    if ( cntThreads > 0 ) {
        for ( ; true; ) {
            shutdown_flag_ = true;
            fetch_lock_.notify_all();
            size_t cntRunningThreads = size_t( cntRunningThreads_ );
            if ( cntRunningThreads == 0 )
                break;
            std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
        }  // for( ; true; )
    }      // if( cntThreads > 0 )
    // wait loop to shutdown
    if ( pLoop ) {
        pLoop->wait();
        try {
            if ( loop_thread_.joinable() )
                loop_thread_.join();
        } catch ( ... ) {
        }
    }  // if( pLoop )
    // shutdown, remove all queues
    queue_remove_all();
}
queue_ptr_t domain::impl_find_queue_to_run() {  // find queue with minimal accumulator and remove it
                                                // from with_jobs_
    queue_ptr_t pQueueFound;
    if ( shutdown_flag_ )
        return pQueueFound;
    lock_type lock_with_jobs( mtx_with_jobs() );
    queues_with_jobs_set_t::iterator itWalk = with_jobs_.begin(), itEnd = with_jobs_.end();
    for ( ; itWalk != itEnd; ) {
        queue_ptr_t& pQueue = const_cast< queue_ptr_t& >( *itWalk );
        if ( pQueue->is_removed() ) {
            LOCAL_DEBUG_TRACE( cc::sunny( "domain::impl_find_queue_to_run()" ) +
                               cc::debug( " will " ) + cc::success( "erase" ) +
                               cc::debug( " pre-removed queue " ) + cc::bright( pQueue->get_id() ) +
                               cc::debug( ", " ) + cc::sunny( "with_jobs_.size()" ) +
                               cc::debug( "=" ) + cc::size10( with_jobs_.size() ) );
            itWalk = with_jobs_.erase( itWalk );
            itEnd = with_jobs_.end();
            continue;
        }
        if ( pQueue->async_job_count() == 0 ) {
            LOCAL_DEBUG_TRACE( cc::sunny( "domain::impl_find_queue_to_run()" ) +
                               cc::debug( " will " ) + cc::success( "erase" ) +
                               cc::debug( " no-jobs queue " ) + cc::bright( pQueue->get_id() ) +
                               cc::debug( ", " ) + cc::sunny( "with_jobs_.size()" ) +
                               cc::debug( "=" ) + cc::size10( with_jobs_.size() ) );
            itWalk = with_jobs_.erase( itWalk );
            itEnd = with_jobs_.end();
            continue;
        }
        if ( pQueue->awaiting_sync_run_ > 0 ) {
            //++ itWalk;
            LOCAL_DEBUG_TRACE( cc::sunny( "domain::impl_find_queue_to_run()" ) +
                               cc::debug( " will " ) + cc::success( "erase" ) +
                               cc::debug( " awaiting sync-run queue " ) +
                               cc::bright( pQueue->get_id() ) + cc::debug( ", " ) +
                               cc::sunny( "with_jobs_.size()" ) + cc::debug( "=" ) +
                               cc::size10( with_jobs_.size() ) );
            itWalk = with_jobs_.erase( itWalk );
            itEnd = with_jobs_.end();
            continue;
        }
        pQueue->is_running_ = true;  // mark it as running
        pQueueFound = pQueue;
        with_jobs_.erase( itWalk );  // remove it from with_jobs_
        LOCAL_DEBUG_TRACE( cc::sunny( "domain::impl_find_queue_to_run()" ) + cc::debug( " did " ) +
                           cc::success( "successfully" ) + cc::debug( " fetched queue " ) +
                           cc::bright( pQueueFound->get_id() ) + cc::debug( ", " ) +
                           cc::sunny( "with_jobs_.size()" ) + cc::debug( "=" ) +
                           cc::size10( with_jobs_.size() ) );
        break;
    }  // for( ; itWalk != itEnd; )
    return pQueueFound;
}
void domain::impl_decrease_accumulators( priority_t acc_subtract ) {
    if ( acc_subtract == 0 )
        return;
    ++decrease_accumulators_counter_;
    uint64_t counter = uint64_t( decrease_accumulators_counter_ ),
             period = uint64_t( decrease_accumulators_period_ );
    if ( period > 1 ) {
        uint64_t rest = counter % period;
        if ( rest )
            return;                          // skip
        decrease_accumulators_counter_ = 0;  // reset
    }                                        // if( period > 1 )
    if ( accumulator_base_ >= acc_subtract )
        accumulator_base_ -= acc_subtract;
    else
        accumulator_base_ = 0;
    for ( auto& entry : queues_ ) {
        // auto & queue_id = entry.first;
        queue_ptr_t& pQueue = entry.second;
        if ( pQueue->accumulator_ >= acc_subtract )
            pQueue->accumulator_ -= acc_subtract;
        else
            pQueue->accumulator_ = 0;
    }  // for( auto & entry : queues_ )
}

set_queue_ids_t domain::queue_get_all_names() {
    lock_type lock( domain_mtx() );
    return impl_queue_get_all_names();
}
queue_ptr_t domain::queue_add( const queue_id_t& id ) {
    lock_type lock( domain_mtx() );
    return impl_queue_add( id );
}
queue_ptr_t domain::queue_get( const queue_id_t& id,
    bool isAutoAdd  // = false
) {
    lock_type lock( domain_mtx() );
    queue_ptr_t q = impl_queue_get( id, isAutoAdd );
    return q;
}
bool domain::queue_remove( const queue_id_t& id ) {
    return impl_queue_remove( id );
}
size_t domain::queue_remove_all() {
    return impl_queue_remove_all();
}
void domain::startup( size_t nWaitMilliSeconds /*= size_t(-1)*/ ) {
    impl_startup( nWaitMilliSeconds );
}
void domain::shutdown() {
    impl_shutdown();
}
bool domain::run_one() {  // returns true if task job was executed
    if ( shutdown_flag_ )
        return false;
    queue_ptr_t pQueue;
    {  // block
        lock_type lock( domain_mtx() );
        pQueue = impl_find_queue_to_run();
        if ( !pQueue )
            return false;
        priority_t acc_subtract = accumulator_base_ = pQueue->accumulator_;
        if ( acc_subtract > 0 )
            impl_decrease_accumulators( acc_subtract );
    }  // block
    if ( !pQueue->job_run() )
        return false;
    return true;
}
loop_ptr_t domain::get_loop() {
    lock_type lock( domain_mtx() );
    if ( pLoop_ )
        return pLoop_;
    loop* pLoop = new loop;
    pLoop_ = pLoop;
    pLoop->on_check_jobs_ = [this]() -> void {
        bool bJobsEmpty = true;
        {  // block
            lock_type lock_with_jobs( mtx_with_jobs() );
            bJobsEmpty = with_jobs_.empty();
        }  // block
        if ( !bJobsEmpty ) {
            fetch_lock_.notify_all();
            // fetch_lock_.notify_one();
        }
    };
    // init loop
    //			pLoop->on_job_will_add_ = [&] ( const skutils::dispatch::job_id_t & id ) -> bool {
    //					return true;
    //				};
    //			pLoop->on_job_did_added_ = [&] ( const skutils::dispatch::job_id_t & id ) -> void {
    //				};
    //			pLoop->on_job_will_remove_ = [&] ( const skutils::dispatch::job_id_t & id ) -> bool
    //{ 					return true;
    //				};
    //			pLoop->on_job_did_removed_ = [&] ( const skutils::dispatch::job_id_t & id ) -> void
    //{
    //				};
    //			pLoop->on_job_will_execute_ = [&] ( const skutils::dispatch::job_id_t & id ) -> bool
    //{ 					return true;
    //				};
    //			pLoop->on_job_did_executed_ = [&] ( const skutils::dispatch::job_id_t & id ) -> void
    //{
    //				};
    //			pLoop->on_job_exception_ = [&] ( const skutils::dispatch::job_id_t & id,
    // std::exception
    //* pe ) -> void {
    //				};
    loop_thread_ = std::thread( [pLoop]() -> void { pLoop->run(); } );
    //			skutils::dispatch::sleep_while_true(
    //				[ pLoop ] () -> bool {
    //					return (!pLoop->isAlive_);
    //				} );
    pLoop->wait_until_startup();
    return pLoop_;
}

uint64_t domain::decrease_accumulators_period() const {
    size_t n = uint64_t( decrease_accumulators_period_ );
    return n;
}
domain& domain::decrease_accumulators_period( uint64_t n ) {
    decrease_accumulators_period_ = n;
    return ( *this );
}

void domain::on_queue_job_added( queue& q ) {
    {  // block
        lock_type lock_with_jobs( mtx_with_jobs() );
        size_t cntJobs = q.async_job_count();
        if ( cntJobs > 0 && ( !q.is_running() ) ) {
            with_jobs_.insert( &q );
            LOCAL_DEBUG_TRACE( cc::sunny( "domain::on_queue_job_added()" ) +
                               cc::debug( " did added work-able queue " ) +
                               cc::bright( q.get_id() ) + cc::debug( ", " ) +
                               cc::sunny( "with_jobs_.size()" ) + cc::debug( "=" ) +
                               cc::size10( with_jobs_.size() ) );
        }
    }  // block
    // fetch_lock_.notify_all();
    fetch_lock_.notify_one();
}
void domain::on_queue_job_complete( queue& q ) {
    {  // block
        lock_type lock_with_jobs( mtx_with_jobs() );
        size_t cntJobs = q.async_job_count();
        if ( cntJobs > 0 && ( !q.is_running() ) ) {
            with_jobs_.insert( &q );
            LOCAL_DEBUG_TRACE( cc::sunny( "domain::on_queue_job_complete()" ) +
                               cc::debug( " did added work-able queue " ) +
                               cc::bright( q.get_id() ) + cc::debug( ", " ) +
                               cc::sunny( "with_jobs_.size()" ) + cc::debug( "=" ) +
                               cc::size10( with_jobs_.size() ) );
        }
    }  // block
    // fetch_lock_.notify_all();
    fetch_lock_.notify_one();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace dispatch
};  // namespace skutils
