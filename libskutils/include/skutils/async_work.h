#if ( !defined __SKUTILS_ASYNCWORK_H )
#define __SKUTILS_ASYNCWORK_H 1

#include <skutils/multifunction.h>
#include <skutils/multithreading.h>
#include <chrono>
#include <map>
#include <mutex>
#include <thread>

namespace skutils {
namespace async {

template < typename data_src_t, typename data_dst_t,
    typename mutex_type = skutils::multithreading::mutex_type >
class work {
public:
    typedef skutils::multifunction< void( const data_src_t&, data_dst_t& ) > fn_work_handler_t;
    typedef skutils::multifunction< void( const data_src_t&, const data_dst_t&, bool ) >
        fn_completion_handler_t;  // data, erase results flag
private:
    typedef std::pair< std::thread::native_handle_type, fn_completion_handler_t >
        pair_thread_info_t;
    typedef std::map< data_src_t, pair_thread_info_t > map_threads_t;
    map_threads_t map_threads_;
    typedef std::map< data_src_t, data_dst_t > map_results_t;
    map_results_t map_results_;
    typedef std::lock_guard< mutex_type > lock_type;
    mutable mutex_type mutex_;
    mutable bool shutdown_mode_ : 1;

public:
    work() : shutdown_mode_( false ) {}
    virtual ~work() {
        {  // block
            lock_type lock( mutex_ );
            shutdown_mode_ = true;
        }  // block
        wait_complete_all();
    }
    virtual bool shutdown_mode() const { return shutdown_mode_; }
    virtual void wait_complete_all() {
        while ( true ) {
            {  // block
                lock_type lock( mutex_ );
                if ( map_threads_.size() == 0 )
                    break;
            }  // block
            std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
        }  // while( true )
        lock_type lock( mutex_ );
        map_results_.clear();
    }
    virtual bool request( const data_src_t& src, fn_work_handler_t fn_work_handler,
        fn_completion_handler_t fn_completion_handler ) {
        try {
            lock_type lock( mutex_ );
            if ( shutdown_mode() )
                return false;
            auto itThread = map_threads_.find( src );
            if ( itThread != map_threads_.end() ) {
                // important, current fn_work_handler is ignored here
                itThread->second.second += fn_completion_handler;  // append completion handler
                return true;                                       // work is running
            }
            if ( map_results_.find( src ) != map_results_.end() )
                return true;  // work is comple, result is ready to get with complete_open_hole()
            volatile bool start_flag = false, responce_flag = false;
            std::thread t( std::bind(
                [&]( data_src_t from, fn_work_handler_t fn_work ) {  // not reference!
                    while ( !start_flag )
                        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
                    responce_flag = true;
                    data_dst_t dst;  // clear dst
                    fn_work( from, dst );
                    lock_type lk( mutex_ );
                    fn_completion_handler_t fn_completion = map_threads_[from].second;
                    map_threads_.erase( from );
                    map_results_[from] = dst;
                    bool erase_results = false;
                    fn_completion( from, dst, erase_results );
                    if ( erase_results )
                        map_results_.erase( from );
                },
                src, fn_work_handler  // not reference!
                ) );
            if ( !t.joinable() )
                return false;
            std::thread::native_handle_type h = t.native_handle();
            start_flag = true;
            while ( !responce_flag )
                std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
            t.detach();
            map_threads_[src] = pair_thread_info_t( h, fn_completion_handler );
            return true;
        } catch ( ... ) {
            return false;
        }
    }
    bool request( const data_src_t& src, fn_work_handler_t fn_work_handler ) {
        fn_completion_handler_t fn_completion_handler;
        return request( src, fn_work_handler, fn_completion_handler );
    }
    virtual bool result( const data_src_t& src, data_dst_t& dst,
        size_t nSleepAttemptsCount = 0,  // zero means unlimited
        size_t nSleepStepMilliseconds = 100 ) {
        dst = data_dst_t();  // clear dst
        size_t nSleepAttemptIndex = 0;
        bool needSleep = false;
        for ( ; true; ) {
            if ( needSleep ) {
                ++nSleepAttemptIndex;
                if ( nSleepAttemptsCount > 0 && nSleepAttemptIndex >= nSleepAttemptsCount )
                    break;
                std::this_thread::sleep_for( std::chrono::milliseconds(
                    ( nSleepStepMilliseconds >= 10 ) ? nSleepStepMilliseconds : 10 ) );
                needSleep = false;
            }
            lock_type lock( mutex_ );
            if ( shutdown_mode() )
                return false;
            if ( map_threads_.find( src ) != map_threads_.end() ) {
                needSleep = true;
                continue;  // work still is running
            }
            auto itResult = map_results_.find( src );
            if ( itResult == map_results_.end() )
                return false;  // something went wrong
            dst = itResult->second;
            map_results_.erase( itResult );
            return true;  // good, we do have successful result
        }                 // for( ; true; )
        return false;
    }
};  /// template class work

template < typename interval_t = std::chrono::milliseconds, typename fn_t =
                                                                std::function< void() > >
class timer {
    std::thread timer_thread_;
    bool is_running_ = false;
    bool is_single_shot_ = true;
    interval_t i_ = interval_t( 0 );
    fn_t fn_;
    void impl_sleep_then_timeout() {
        std::this_thread::sleep_for( i_ );
        if ( running() )
            fn()();
    }
    void impl_temporize() {
        if ( is_single_shot_ )
            impl_sleep_then_timeout();
        else {
            while ( running() )
                impl_sleep_then_timeout();
        }
        is_running_ = false;
    }

public:
    timer() {}
    timer( const fn_t& fn ) : fn_( fn ) {}
    timer( const fn_t& fn, const interval_t& i, bool is_ss = true )
        : is_single_shot_( is_ss ), i_( i ), fn_( fn ) {}
    timer( const timer& ) = delete;
    timer( timer&& ) = delete;
    timer& operator=( const timer& ) = delete;
    timer& operator=( timer&& ) = delete;
    virtual ~timer() { stop(); }
    timer& start( bool is_multi_thread = true ) {
        if ( running() )
            return ( *this );
        is_running_ = true;
        if ( is_multi_thread )
            timer_thread_ = std::thread( &timer::impl_temporize, this );
        else
            impl_temporize();
        return ( *this );
    }
    bool running() const { return is_running_; }
    timer& stop() {
        is_running_ = false;
        try {
            if ( timer_thread_.joinable() )
                timer_thread_.join();
        } catch ( ... ) {
        }
        return ( *this );
    }
    bool is_single_shot() const { return is_single_shot_; }
    timer& set_single_shot( bool is_ss ) {
        if ( !running() )
            is_single_shot_ = is_ss;
        return ( *this );
    }
    const interval_t& interval() const { return i_; }
    timer& set_interval( const interval_t& i ) {
        if ( !running() )
            i_ = i;
        return ( *this );
    }
    const fn_t& fn() const { return fn_; }
    timer& set_fn( const fn_t& fn ) {
        if ( !running() )
            fn_ = fn;
        return ( *this );
    }
    timer& run(
        const fn_t& fn, const interval_t& i, bool is_ss = true, bool is_multi_thread = true ) {
        return stop().set_fn( fn ).set_interval( i ).set_single_shot( is_ss ).start(
            is_multi_thread );
    }
    timer& run(
        const interval_t& i, const fn_t& fn, bool is_ss = true, bool is_multi_thread = true ) {
        return stop().set_fn( fn ).set_interval( i ).set_single_shot( is_ss ).start(
            is_multi_thread );
    }
};  // template class timer

};  // namespace async
};  // namespace skutils

#endif  /// (!defined __SKUTILS_ASYNCWORK_H)
