#include <skutils/console_colors.h>
#include <skutils/dispatch.h>
#include <skutils/task_performance.h>
#include <skutils/utils.h>
#include <time.h>
#include <uv.h>
#include <algorithm>
#include <exception>
#include <iostream>
#include <stdexcept>

//#define __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_ASYNC_JOB_STATES__ 1
//#define __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_QUEUE_STATES__ 1
//#define __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ 1

#define LOCAL_DEBUG_TRACE( x )
// static inline void LOCAL_DEBUG_TRACE( const std::string & x ) { if( x.empty()
// ) return; std::cout.flush(); std::cerr.flush(); std::cout << x << "\n";
// std::cout.flush(); }

namespace skutils {
namespace dispatch {

skutils::multithreading::recursive_mutex_type& get_dispatch_mtx() {
    // static skutils::multithreading::recursive_mutex_type g_dispatch_mtx(
    //    "skutils::dispatch::g_dispatch_mtx" );
    // return g_dispatch_mtx;
    return skutils::get_ref_mtx();
}

static void stat_sleep( duration_t how_much ) {
    // auto nNanoSeconds = how_much.count();
    // struct timespec ts{ time_t(nNanoSeconds/1000000000),
    // long(nNanoSeconds%1000000000) }; nanosleep( &ts,nullptr );
    std::this_thread::sleep_for( how_much );
}
bool sleep_while_true(  // returns false if timeout is reached and fn() never
                        // returned false
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

loop::loop() : p_uvLoop_( nullptr ), cancelMode_( false ) {
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
    std::cout << skutils::tools::format( "dispatch loop ctor %p\n", this );
    std::cout.flush();
#endif
#if ( defined __SKUTILS_DISPATCH_ENABLE_ASYNC_INIT_CALL_FOR_TASK_TIMERS__ )
    if ( p_uvAsyncInitForTimers_ == nullptr )
        p_uvAsyncInitForTimers_ = ( void* ) calloc( 1, sizeof( uv_async_t ) );
#endif  // ( defined __SKUTILS_DISPATCH_ENABLE_ASYNC_INIT_CALL_FOR_TASK_TIMERS__
        // )
}
loop::~loop() {
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
    std::cout << skutils::tools::format( "dispatch loop dtor start %p\n", this );
    std::cout.flush();
#endif
    job_remove_all();
    cancel();
    wait();
#if ( defined __SKUTILS_DISPATCH_ENABLE_ASYNC_INIT_CALL_FOR_TASK_TIMERS__ )
    if ( p_uvAsyncInitForTimers_ != nullptr ) {
        free( p_uvAsyncInitForTimers_ );
        p_uvAsyncInitForTimers_ = nullptr;
    }
#endif  // ( defined __SKUTILS_DISPATCH_ENABLE_ASYNC_INIT_CALL_FOR_TASK_TIMERS__
        // )
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
    std::cout << skutils::tools::format( "dispatch loop dtor finish %p\n", this );
    std::cout.flush();
#endif
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
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
    std::cout << skutils::tools::format( "dispatch loop wait %p\n", this );
    std::cout.flush();
#endif
    return sleep_while_true( [this]() -> bool { return is_running(); }, timeout, step );
}
void loop::run() {
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
    std::cout << skutils::tools::format( "dispatch loop run start %p\n", this );
    std::cout.flush();
#endif
    pending_timer_remove_all();
    //
    bool bHaveLoop = false;
    uv_loop_t uvLoop;
    //
    // bool bHaveIdler = false;
    // uv_idle_t uvIdler;
    //
    bool bHaveTimerStateCheck = false;
    uv_timer_t uvTimerStateCheck;
    auto fnCleanupHere = [&]() -> void {
        lock_type lock( loop_mtx() );
        // if( bHaveIdler ) {
        //     bHaveIdler = false;
        //     uv_idle_stop( &uvIdler );
        // }
        if ( bHaveTimerStateCheck ) {
            bHaveTimerStateCheck = false;
            uv_timer_stop( &uvTimerStateCheck );
        }
        if ( bHaveLoop ) {
            bHaveLoop = false;
            uv_loop_close( &uvLoop );
        }
    };
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
            // uv_idle_init( &uvLoop, &uvIdler );
            // uvIdler.data = ( void* ) this;
            // bHaveIdler = true;
            //
            uv_timer_init( &uvLoop, &uvTimerStateCheck );
            uvTimerStateCheck.data = ( void* ) this;
            //
            p_uvLoop_ = ( &uvLoop );
            cancelMode_ = false;
            //
            // uv_idle_start( &uvIdler, []( uv_idle_t* p_uvIdler ) {
            //     loop* pLoop = ( loop* ) ( p_uvIdler->data );
            //     pLoop->on_idle();
            // } );
            //
            uv_timer_start(
                &uvTimerStateCheck,
                []( uv_timer_t* p_uvTimer ) {
                    loop* pLoop = ( loop* ) ( p_uvTimer->data );
                    pLoop->on_state_check();
                },
                100,  // 200,  // timeout milliseconds
                100   // 200   // repeat milliseconds
            );
            bHaveTimerStateCheck = true;
            //
#if ( defined __SKUTILS_DISPATCH_ENABLE_ASYNC_INIT_CALL_FOR_TASK_TIMERS__ )
            if ( p_uvAsyncInitForTimers_ != nullptr ) {
                uv_async_t* p_uvAsyncInit = ( uv_async_t* ) p_uvAsyncInitForTimers_;
                p_uvAsyncInit->data = ( void* ) this;
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
                std::cout << skutils::tools::format(
                    "dispatch loop initiate pending timer init %p\n", this );
                std::cout.flush();
#endif
                uv_async_init( &uvLoop, p_uvAsyncInit, []( uv_async_t* h ) -> void {
                    loop* pLoop = ( loop* ) ( h->data );
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
                    std::cout << skutils::tools::format(
                        "dispatch loop pending timer callback %p\n", pLoop );
                    std::cout.flush();
#endif
                    if ( pLoop )
                        pLoop->pending_timer_init();
                } );
            }  // if ( p_uvAsyncInitForTimers_ != nullptr )
#endif         // ( defined __SKUTILS_DISPATCH_ENABLE_ASYNC_INIT_CALL_FOR_TASK_TIMERS__
               // )
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
            bHaveLoop = true;
            uv_run( &uvLoop, UV_RUN_DEFAULT );
        }  // block
        p_uvLoop_ = nullptr;
        fnCleanupHere();
        cancelMode_ = false;
    } catch ( ... ) {
        p_uvLoop_ = nullptr;
        fnCleanupHere();
        throw;
    }
    p_uvLoop_ = nullptr;
    pending_timer_remove_all();
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
    std::cout << skutils::tools::format( "dispatch loop run finish %p\n", this );
    std::cout.flush();
#endif
}

void loop::pending_timer_add( void* p_uvTimer, void* pTimerData,
    void ( *pFnCb )( void* uv_timer_handle ), uint64_t timeout, uint64_t interval ) {
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
    std::cout << skutils::tools::format( "dispatch loop add pending timer %p\n", this );
    std::cout.flush();
#endif
    pending_timer_lock_type lock( pending_timer_mtx_ );
#if ( defined __SKUTILS_DISPATCH_ENABLE_ASYNC_INIT_CALL_FOR_TASK_TIMERS__ )
    if ( p_uvAsyncInitForTimers_ == nullptr )
        return;
#endif  // ( defined __SKUTILS_DISPATCH_ENABLE_ASYNC_INIT_CALL_FOR_TASK_TIMERS__
        // )
    pending_timer_t rpt;
    rpt.pUvTimer_ = p_uvTimer;
    rpt.pFnCb_ = pFnCb;
    rpt.timeout_ = timeout;
    rpt.interval_ = interval;
    rpt.pTimerData_ = pTimerData;
    pending_timer_list_.push_back( rpt );
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
    std::cout << skutils::tools::format(
        "dispatch loop before async send pending timer %p\n", this );
    std::cout.flush();
#endif
#if ( defined __SKUTILS_DISPATCH_ENABLE_ASYNC_INIT_CALL_FOR_TASK_TIMERS__ )
    uv_async_send( ( uv_async_t* ) p_uvAsyncInitForTimers_ );
#endif  // ( defined __SKUTILS_DISPATCH_ENABLE_ASYNC_INIT_CALL_FOR_TASK_TIMERS__
        // )
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
    std::cout << skutils::tools::format(
        "dispatch loop after async send pending timer %p\n", this );
    std::cout.flush();
#endif
}
void loop::pending_timer_remove_all() {
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
    std::cout << skutils::tools::format( "dispatch loop remove all pending timers %p\n", this );
    std::cout.flush();
#endif
    pending_timer_lock_type lock( pending_timer_mtx_ );
    pending_timer_list_.clear();
}
void loop::pending_timer_init() {
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
    std::cout << skutils::tools::format( "dispatch loop init pending timer %p\n", this );
    std::cout.flush();
#endif
    pending_timer_lock_type lock( pending_timer_mtx_ );
    pending_timer_list_t::iterator itWalk = pending_timer_list_.begin(),
                                   itEnd = pending_timer_list_.end();
    for ( ; itWalk != itEnd; ++itWalk ) {
        pending_timer_t& rpt = ( *itWalk );
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
        std::cout << skutils::tools::format(
            "dispatch loop before start pending timer %p\n", this );
        std::cout.flush();
#endif
        //
        uv_timer_t* p_uvTimer = ( uv_timer_t* ) rpt.pUvTimer_;
        uv_timer_init( p_uvLoop_, p_uvTimer );
        p_uvTimer->data = rpt.pTimerData_;
        //
        uv_timer_start( ( uv_timer_t* ) rpt.pUvTimer_, ( uv_timer_cb ) rpt.pFnCb_, rpt.timeout_,
            rpt.interval_ );
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
        std::cout << skutils::tools::format( "dispatch loop after start pending timer %p\n", this );
        std::cout.flush();
#endif
    }
    pending_timer_list_.clear();
}

// void loop::on_idle() {
//    //#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
//    //    std::cout << skutils::tools::format( "dispatch loop idle %p\n", this );
//    //    std::cout.flush();
//    //#endif
//    isAlive_ = true;
//    if ( on_check_cancel_mode() )
//        return;
//    pending_timer_init();
//    on_check_jobs();
//}

void loop::on_state_check() {
    //#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
    //    std::cout << skutils::tools::format( "dispatch loop state check %p\n",
    //    this ); std::cout.flush();
    //#endif
    isAlive_ = true;
    if ( on_check_cancel_mode() )
        return;
    pending_timer_init();  // l_sergiy: we need this called here because on_idle() was removed
    on_check_jobs();
}

bool loop::on_check_cancel_mode() {
    //#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
    //    std::cout << skutils::tools::format( "dispatch loop check cancel mode
    //    %p\n", this ); std::cout.flush();
    //#endif
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
    if ( cancelMode_ )
        return;
    //#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
    //    std::cout << skutils::tools::format( "dispatch loop check jobs %p\n",
    //    this ); std::cout.flush();
    //#endif
    if ( on_check_jobs_ )
        on_check_jobs_();
}

loop::job_data_t::job_data_t( loop_ptr_t pLoop ) : pLoop_( pLoop ), needsCancel_( false ) {
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_ASYNC_JOB_STATES__ )
    std::cout << skutils::tools::format( "dispatch async job ctor %p\n", this );
    std::cout.flush();
#endif
    p_uvTimer_ = calloc( 1, sizeof( uv_timer_t ) );
    if ( !p_uvTimer_ )
        throw std::runtime_error( "loop job timer allocation error" );
}
loop::job_data_t::~job_data_t() {
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_ASYNC_JOB_STATES__ )
    std::cout << skutils::tools::format( "dispatch async job dtor start %p\n", this );
    std::cout.flush();
#endif
    if ( p_uvTimer_ ) {
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_LOOP_STATES__ )
        std::cout << skutils::tools::format( "dispatch loop free job timer %p\n", this );
        std::cout.flush();
#endif
        cancel();
        free( p_uvTimer_ );
        p_uvTimer_ = nullptr;
    }
    pLoop_ = nullptr;
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_ASYNC_JOB_STATES__ )
    std::cout << skutils::tools::format( "dispatch async job dtor finish %p\n", this );
    std::cout.flush();
#endif
}
void loop::job_data_t::cancel() {
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_ASYNC_JOB_STATES__ )
    std::cout << skutils::tools::format( "dispatch async job cancel %p\n", this );
    std::cout.flush();
#endif
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
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_ASYNC_JOB_STATES__ )
    std::cout << skutils::tools::format( "dispatch async job init %p\n", this );
    std::cout.flush();
#endif
    if ( !pLoop_ )
        return;
    // uv_loop_t* p_uvLoop = ( uv_loop_t* ) ( void* ) pLoop_->p_uvLoop_;
    needsCancel_ = true;
    // nanoseconds
    uint64_t ns_timeout = timeout_.count(), ns_interval = interval_.count();
    // milliseconds
    uint64_t ms_timeout =
        ( ns_timeout / ( 1000 * 1000 ) ) + ( ( ( ns_timeout % ( 1000 * 1000 ) ) != 0 ) ? 1 : 0 );
    uint64_t ms_interval =
        ( ns_interval / ( 1000 * 1000 ) ) + ( ( ( ns_interval % ( 1000 * 1000 ) ) != 0 ) ? 1 : 0 );
    //
    pLoop_->pending_timer_add(
        p_uvTimer_, ( void* ) this,
        []( void* h ) -> void {
            uv_timer_t* pTimer = ( uv_timer_t* ) h;
            loop::job_data_t* pJobData = ( loop::job_data_t* ) ( pTimer->data );
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_ASYNC_JOB_STATES__ )
            std::cout << skutils::tools::format(
                "dispatch async job timer fn call %p\n", pJobData );
            std::cout.flush();
#endif
            pJobData->on_timer();
        },
        ms_timeout, ms_interval );
    //
}
void loop::job_data_t::on_timer() {
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_ASYNC_JOB_STATES__ )
    std::cout << skutils::tools::format( "dispatch async job on_timer %p\n", this );
    std::cout.flush();
#endif
    on_invoke();
}
void loop::job_data_t::on_invoke() {
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_ASYNC_JOB_STATES__ )
    std::cout << skutils::tools::format( "dispatch async job on_invoke begin %p\n", this );
    std::cout.flush();
#endif
    job_id_t id( id_ );
    loop_ptr_t pLoop( pLoop_ );
    try {
        if ( !pLoop )
            return;
        // loop_ptr_t ptr( &loop_ );
        // one_per_thread_t::make_current curr( g_one_per_thread, ptr );
        if ( !pLoop->on_job_will_execute( id ) )
            return;
        if ( fn_ ) {
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_ASYNC_JOB_STATES__ )
            std::cout << skutils::tools::format(
                "dispatch async job on_invoke will call fn %p\n", this );
            std::cout.flush();
#endif
            fn_();
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_ASYNC_JOB_STATES__ )
            std::cout << skutils::tools::format(
                "dispatch async job on_invoke did called fn %p\n", this );
            std::cout.flush();
#endif
        }
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
    on_job_was_added( id );
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
    return impl_job_present( id, p_timeout, p_interval, pp_uvTimer );
}
size_t loop::job_remove_all() {
    lock_type lock( loop_mtx() );
    return impl_job_remove_all();
}
void loop::job_add( const job_id_t& id, job_t /*&*/ fn, duration_t timeout, duration_t interval ) {
    lock_type lock( loop_mtx() );
    return impl_job_add( id, fn, timeout, interval );
}
void loop::job_add_once( const job_id_t& id, job_t /*&*/ fn,
    duration_t timeout ) {  // like JavaScript setTimeout()
    job_add( id, fn, timeout, duration_t( 0 ) );
}
void loop::job_add_periodic( const job_id_t& id, job_t /*&*/ fn,
    duration_t interval ) {  // like JavaScript setInterval()
    job_add( id, fn, duration_t( 0 ), interval );
}

bool loop::on_job_will_add( const job_id_t& id ) {
    if ( on_job_will_add_ ) {
        if ( !on_job_will_add_( id ) )
            return false;
    }
    return true;
}
void loop::on_job_was_added( const job_id_t& id ) {
    if ( on_job_was_added_ )
        on_job_was_added_( id );
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
      // mtx_jobs_(skutils::tools::format("skutils::dispatch::queue-%p/mutex/jobs",
      // this)),
      mtx_run_( skutils::tools::format( "skutils::dispatch::queue-%p/mutex/run", this ) ),
      async_job_count_( 0 )  // cached value of jobs_.size()
      ,
      awaiting_sync_run_( 0 ) {
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_QUEUE_STATES__ )
    std::cout << skutils::tools::format( "dispatch queue %s ctor %p\n", id_.c_str(), this );
    std::cout.flush();
#endif
}
queue::~queue() {
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_QUEUE_STATES__ )
    std::cout << skutils::tools::format( "dispatch queue %s dtor start %p\n", id_.c_str(), this );
    std::cout.flush();
#endif
    job_cancel_all();
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_QUEUE_STATES__ )
    std::cout << skutils::tools::format( "dispatch queue %s dtor end %p\n", id_.c_str(), this );
    std::cout.flush();
#endif
}
queue::mutex_type& queue::mtx_jobs() const {
    return skutils::get_ref_mtx();
    // return mtx_jobs_;
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
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_QUEUE_STATES__ )
    std::cout << skutils::tools::format(
        "dispatch queue %s cancel all jobs %p\n", id_.c_str(), this );
    std::cout.flush();
#endif
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
    job_id_t* pJobID ) {  // if both timeout and interval are zero, then invoke
                          // once asynchronously
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_QUEUE_STATES__ )
    std::cout << skutils::tools::format( "dispatch queue %s add job %p\n", id_.c_str(), this );
    std::cout.flush();
#endif
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
            pLoop->job_add(
                id,
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
bool queue::impl_job_run() {  // fetch first asynchronously stored job and run
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_QUEUE_STATES__ )
    std::cout << skutils::tools::format( "dispatch queue %s try run job %p\n", id_.c_str(), this );
    std::cout.flush();
#endif
    if ( is_removed() )
        return false;
    if ( awaiting_sync_run_ > 0 )
        return false;
    domain_ptr_t pDomain = pDomain_;
    if ( !pDomain )
        return false;
    job_t fn;
    {  // block
        skutils::multithreading::setThreadName( "dq-" + get_id() );
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
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_QUEUE_STATES__ )
    std::cout << skutils::tools::format(
        "dispatch queue %s try run job sync %p\n", id_.c_str(), this );
    std::cout.flush();
#endif
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
bool queue::impl_job_run( job_t /*&*/ fn ) {  // run explicitly specified job synchronosly
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
            skutils::multithreading::setThreadName( "dq-" + get_id() );
            lock_type lock( mtx_run() );
            is_running_ = true;
            queue_ptr_t ptr( this );
            one_per_thread_t::make_current curr( g_one_per_thread, ptr );
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_QUEUE_STATES__ )
            std::cout << skutils::tools::format(
                "dispatch queue %s before run job fn %p\n", id_.c_str(), this );
            std::cout.flush();
#endif
            //
            std::string strPerformanceQueueName =
                skutils::tools::format( "dispatch/queue/%s", id_.c_str() );
            std::string strPerformanceActionName =
                skutils::tools::format( "task %zu", nTaskNumberInQueue_++ );
            skutils::task::performance::action a(
                strPerformanceQueueName, strPerformanceActionName );
            //
            fn();
#if ( defined __SKUTILS_DISPATCH_DEBUG_CONSOLE_TRACE_QUEUE_STATES__ )
            std::cout << skutils::tools::format(
                "dispatch queue %s after run job fn %p\n", id_.c_str(), this );
            std::cout.flush();
#endif
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
    duration_t timeout,   // = duration_t(0) // if both timeout and interval are
                          // zero, then invoke once asynchronously
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
    return impl_job_add( fn, timeout, duration_t( 0 ),
        pJobID );  // like JavaScript setTimeout()
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
    return impl_job_add( fn, duration_t( 0 ), interval,
        pJobID );  // like JavaScript setInterval()
}
bool queue::job_run_sync( job_t /*&*/ fn ) {
    if ( !fn )
        return false;
    if ( is_removed() )
        return false;
    // lock_type lock( mtx_run() );
    return impl_job_run_sync( fn );
}
bool queue::job_run( job_t /*&*/ fn ) {  // run explicitly specified job synchronosly
    if ( !fn )
        return false;
    if ( is_removed() )
        return false;
    // lock_type lock( mtx_run() );
    return impl_job_run( fn );
}
bool queue::job_run() {  // fetch first asynchronously stored job and run
    if ( is_removed() )
        return false;
    // lock_type lock( mtx_run() );
    return impl_job_run();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::atomic_bool domain::g_bVerboseDispatchThreadDetailsLogging = false;

domain::domain( const size_t nNumberOfThreads,  // = 0 // 0 means use CPU count
    const size_t nQueueLimit                    // = 0
    )
    : async_job_count_( 0 ),
      accumulator_base_( 0 )
      //	, domain_mtx_(
      // skutils::tools::format("skutils::dispatch::domain-%p/mutex/main", this
      // )
      //) 	, mtx_with_jobs_(
      // skutils::tools::format("skutils::dispatch::domain-%p/mutex/with_jobs",
      // this ) )
      ,
      shutdown_flag_( true ),
      thread_pool_(
          ( nNumberOfThreads > 0 ) ? nNumberOfThreads : skutils::tools::cpu_count(), nQueueLimit ),
      cntRunningThreads_( 0 ),
      cntStartTestedThreads_( 0 ),
      decrease_accumulators_counter_( uint64_t( 0 ) ),
      decrease_accumulators_period_( uint64_t( 1000 ) * uint64_t( 1000 ) )  // rare enough
{
    try {
        startup();
    } catch ( ... ) {
        shutdown();
        throw;
    }
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
    size_t cntJobs = async_job_count_;  // sum of all cached values of jobs_.size()
                                        // from all queues
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
    size_t idxThread, cntThreadsInPool = thread_pool_.number_of_threads();
    if ( cntThreadsInPool == 0 )
        throw std::runtime_error( "dispatch domain failed to initialize thread pool" );
    // init thread pool
    size_t cntThreadsToStart = cntThreadsInPool;
    for ( size_t cntThreadStartupAttempts = cntThreadsInPool * 2; cntThreadStartupAttempts != 0;
          --cntThreadStartupAttempts ) {
        std::atomic_size_t cntFailedToStartThreads;
        cntFailedToStartThreads = 0;
        for ( idxThread = 0; idxThread < cntThreadsToStart; ++idxThread ) {
            std::string strPerformanceQueueName = skutils::tools::format(
                "dispatch/thread/%zu", idxThread );  // notice - no domain reference
            std::string strError;
            static const size_t cntAttempts = 5;
            for ( size_t idxAttempt = 0; idxAttempt < cntAttempts; ++idxAttempt ) {
                if ( idxAttempt > 0 ) {
                    strError.clear();
                    std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
                }
                try {
                    thread_pool_.safe_submit_without_future_te(
                        [this, strPerformanceQueueName, idxThread, cntThreadsToStart]() {
                            ++cntRunningThreads_;
                            ++cntStartTestedThreads_;
                            try {
                                if ( g_bVerboseDispatchThreadDetailsLogging ) {
                                    std::string strThreadStartupMessage =
                                        cc::deep_note( "Dispatch:" ) + " " +
                                        cc::debug( "Started thread " ) + cc::size10( idxThread ) +
                                        cc::debug( " of " ) + cc::size10( cntThreadsToStart ) +
                                        cc::debug( ", have " ) +
                                        cc::size10( size_t( cntRunningThreads_ ) ) +
                                        cc::debug( " running thread(s)" ) + "\n";
                                    std::cout << strThreadStartupMessage;
                                    std::cout.flush();
                                }
                                size_t nTaskNumberInThisThread = 0;
                                for ( ; true; ) {
                                    if ( shutdown_flag_ )
                                        break;
                                    {  // block
                                        std::unique_lock< fetch_mutex_type > lock( fetch_mutex_ );
                                        fetch_lock_.wait( lock );
                                    }  // block
                                    if ( shutdown_flag_ )
                                        break;
                                    for ( ; true; ) {
                                        //
                                        std::string strPerformanceActionName =
                                            skutils::tools::format(
                                                "task %zu", nTaskNumberInThisThread++ );
                                        skutils::task::performance::action a(
                                            strPerformanceQueueName, strPerformanceActionName );
                                        //
                                        if ( !run_one() )
                                            break;
                                        if ( shutdown_flag_ )
                                            break;
                                        // fetch_lock_.notify_one(); // spread the work into other
                                        // threads
                                    }
                                }  /// for( ; true ; )
                            } catch ( const std::exception& ex ) {
                                std::string strError( ex.what() );
                                if ( strError.empty() )
                                    strError = "Exception without description";
                                std::string strErrorMessage =
                                    cc::deep_note( "Dispatch:" ) + " " +
                                    cc::fatal( "CRITICAL ERROR:" ) +
                                    cc::error( "Got exception in thread " ) +
                                    cc::size10( idxThread ) + cc::error( " of " ) +
                                    cc::size10( cntThreadsToStart ) + cc::error( ", have " ) +
                                    cc::size10( size_t( cntRunningThreads_ ) ) +
                                    cc::error( " running threads, exception info: " ) +
                                    cc::warn( strError ) + "\n";
                                std::cout << strErrorMessage;
                                std::cout.flush();
                            } catch ( ... ) {
                                std::string strErrorMessage =
                                    cc::deep_note( "Dispatch:" ) + " " +
                                    cc::fatal( "CRITICAL ERROR:" ) +
                                    cc::error( "Got exception in thread " ) +
                                    cc::size10( idxThread ) + cc::error( " of " ) +
                                    cc::size10( cntThreadsToStart ) + cc::error( ", have " ) +
                                    cc::size10( size_t( cntRunningThreads_ ) ) +
                                    cc::error( " running threads, exception info: " ) +
                                    cc::warn( "Unknown exception" ) + "\n";
                                std::cout << strErrorMessage;
                                std::cout.flush();
                            }
                            --cntRunningThreads_;
                            if ( g_bVerboseDispatchThreadDetailsLogging ) {
                                std::string strThreadFinalMessage =
                                    cc::deep_note( "Dispatch:" ) + " " +
                                    cc::debug( "Exiting thread " ) + cc::size10( idxThread ) +
                                    cc::debug( " of " ) + cc::size10( cntThreadsToStart ) +
                                    cc::debug( ", have " ) +
                                    cc::size10( size_t( cntRunningThreads_ ) ) +
                                    cc::debug( " running thread(s)" ) + "\n";
                                std::cout << strThreadFinalMessage;
                                std::cout.flush();
                            }
                        } );
                } catch ( std::exception& ex ) {
                    strError = ex.what();
                    if ( strError.empty() )
                        strError = "exception without description";
                } catch ( ... ) {
                    strError = "unknown description";
                }
                if ( strError.empty() )
                    break;
                std::string strErrorMessage =
                    cc::deep_note( "Dispatch:" ) + " " + cc::fatal( "CRITICAL ERROR:" ) +
                    cc::error( " Failed submit initialization task for the " ) +
                    cc::info( strPerformanceQueueName ) + cc::error( " queue at attempt " ) +
                    cc::size10( idxAttempt ) + cc::error( " of " ) + cc::size10( cntAttempts ) +
                    cc::error( ", error is: " ) + cc::warn( strError ) + "\n";
                std::cout << strErrorMessage;
                std::cout.flush();
            }  // for( size_t idxAttempt = 0; idxAttempt < 3; ++ idxAttempt ) {
            if ( !strError.empty() ) {
                ++cntFailedToStartThreads;
                throw std::runtime_error( strError );
            }
        }  // for ( idxThread = 0; idxThread < cntThreadsToStart; ++idxThread ) {
        cntThreadsToStart = size_t( cntFailedToStartThreads );
        if ( cntThreadsToStart == 0 )
            break;
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
    }  // for ( size_t cntThreadStartupAttempts = 1; cntThreadStartupAttempts != 0;
    // --cntThreadStartupAttempts )
    thread_pool_.notify_all();  // faster encueued call processing here because we knew we did
                                // submit first calls above
    size_t cntStartedAndRunningThreads = size_t( cntStartTestedThreads_ );
    size_t cntWaitAttempts =
        ( nWaitMilliSeconds == 0 || nWaitMilliSeconds == size_t( -1 ) ) ? 10000 : nWaitMilliSeconds;
    for ( size_t idxWaitAttempt = 0; idxWaitAttempt < cntWaitAttempts; ++idxWaitAttempt ) {
        if ( cntStartedAndRunningThreads == cntThreadsInPool )
            break;
        std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
        cntStartedAndRunningThreads = size_t( cntStartTestedThreads_ );
    }
    if ( cntStartedAndRunningThreads != cntThreadsInPool ) {
        fetch_lock_.notify_all();  // notify earlier
        // throw std::runtime_error(
        //     "dispatch domain failed to initialize all threads in thread pool" );
        std::string strWarningMessage =
            cc::deep_note( "Dispatch:" ) + " " + cc::warn( "WARNING: expected " ) +
            cc::size10( size_t( cntThreadsInPool ) ) +
            cc::warn( " threads in pool to be started at this time but have " ) +
            cc::size10( size_t( cntStartedAndRunningThreads ) ) + cc::warn( ", startup is slow!" ) +
            "\n";
        std::cout << strWarningMessage;
        std::cout.flush();
    } else {
        std::string strSuccessMessage = cc::deep_note( "Dispatch:" ) + " " +
                                        cc::success( "Have all " ) +
                                        cc::size10( size_t( cntThreadsInPool ) ) +
                                        cc::success( " threads in pool started fast" ) + "\n";
        std::cout << strSuccessMessage;
        std::cout.flush();
    }
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
            if ( g_bVerboseDispatchThreadDetailsLogging ) {
                std::string strMessage = cc::deep_note( "Dispatch:" ) + " " + cc::debug( "Have " ) +
                                         cc::size10( size_t( cntRunningThreads_ ) ) +
                                         cc::debug( " thread(s) still running..." ) + "\n";
                std::cout << strMessage;
                std::cout.flush();
            }
            shutdown_flag_ = true;
            fetch_lock_.notify_all();
            size_t cntRunningThreads = size_t( cntRunningThreads_ );
            if ( cntRunningThreads == 0 )
                break;
            std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
        }  // for( ; true; )
    }      // if( cntThreads > 0 )
    std::cout << cc::deep_note( "Dispatch:" ) + " " + cc::success( "All threads stopped" ) + "\n";
    std::cout.flush();
    // wait loop to shutdown
    if ( pLoop ) {
        if ( g_bVerboseDispatchThreadDetailsLogging ) {
            std::cout << cc::deep_note( "Dispatch:" ) + " " +
                             cc::debug( "Waiting for dispatch loop" ) + "\n";
            std::cout.flush();
        }
        pLoop->wait();
        try {
            if ( g_bVerboseDispatchThreadDetailsLogging ) {
                std::cout << cc::deep_note( "Dispatch:" ) + " " +
                                 cc::debug( "Stopping for dispatch loop" ) + "\n";
                std::cout.flush();
            }
            if ( loop_thread_.joinable() )
                loop_thread_.join();
            std::cout << cc::deep_note( "Dispatch:" ) + " " +
                             cc::success( "Dispatch loop stopped" ) + "\n";
            std::cout.flush();
        } catch ( ... ) {
        }
    }  // if( pLoop )
    // shutdown, remove all queues
    if ( g_bVerboseDispatchThreadDetailsLogging ) {
        std::cout << cc::deep_note( "Dispatch:" ) + " " + cc::debug( "Removing dispatch queues" ) +
                         "\n";
        std::cout.flush();
    }
    queue_remove_all();
    std::cout << cc::deep_note( "Dispatch:" ) + " " + cc::success( "All dispatch queues removed" ) +
                     "\n";
    std::cout.flush();
}
queue_ptr_t domain::impl_find_queue_to_run() {  // find queue with minimal accumulator and
                                                // remove it from with_jobs_
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
    domain_ptr_t pThisDomain = this;
    pLoop->on_check_jobs_ = [pThisDomain]() -> void {
        // if ( pLoop->cancelMode_ )
        //    return;
        if ( pThisDomain->shutdown_flag_ )
            return;
        bool bJobsEmpty = true;
        {  // block
            lock_type lock_with_jobs( pThisDomain->mtx_with_jobs() );
            bJobsEmpty = pThisDomain->with_jobs_.empty();
        }  // block
        if ( !bJobsEmpty ) {
            pThisDomain.get_unconst()->fetch_lock_.notify_all();
            // pThisDomain.get_unconst()->fetch_lock_.notify_one();
        }
    };
    // init loop
    //			pLoop->on_job_will_add_ = [&] ( const
    // skutils::dispatch::job_id_t & id ) -> bool {
    // return true;
    //				};
    //			pLoop->on_job_was_added_ = [&] ( const
    // skutils::dispatch::job_id_t & id ) -> void {
    //				};
    //			pLoop->on_job_will_remove_ = [&] ( const
    // skutils::dispatch::job_id_t & id ) -> bool
    //{ 					return true;
    //				};
    //			pLoop->on_job_did_removed_ = [&] ( const
    // skutils::dispatch::job_id_t & id ) -> void
    //{
    //				};
    //			pLoop->on_job_will_execute_ = [&] ( const
    // skutils::dispatch::job_id_t & id ) -> bool
    //{ 					return true;
    //				};
    //			pLoop->on_job_did_executed_ = [&] ( const
    // skutils::dispatch::job_id_t & id ) -> void
    //{
    //				};
    //			pLoop->on_job_exception_ = [&] ( const
    // skutils::dispatch::job_id_t & id,
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
