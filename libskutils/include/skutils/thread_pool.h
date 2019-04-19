#if ( !defined __SKUTILS_THREAD_POOL_H )
#define __SKUTILS_THREAD_POOL_H 1

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

#include <skutils/multithreading.h>

namespace skutils {

template < typename element_type, typename mutex_type = skutils::multithreading::mutex_type >
class thread_safe_queue {
private:
    std::queue< element_type > queue_;
    mutable mutex_type mutex_;
    typedef std::lock_guard< mutex_type > lock_type;
    volatile size_t limit_;  // zero queue size limit means not limited at all
public:
    thread_safe_queue( const size_t nQueueLimit = 0 )
        : mutex_( "MTX-THREAD-SAFE-QUEUE" ), limit_( nQueueLimit ) {}
    thread_safe_queue( const thread_safe_queue& ) = delete;
    thread_safe_queue( thread_safe_queue&& ) = delete;
    thread_safe_queue& operator=( const thread_safe_queue& ) = delete;
    thread_safe_queue& operator=( thread_safe_queue&& ) = delete;
    ~thread_safe_queue() {}
    bool empty() {
        lock_type lock( mutex_ );
        return queue_.empty();
    }
    size_t size() const {
        lock_type lock( mutex_ );
        size_t cnt = queue_.size();
        return cnt;
    }
    size_t limit() const { return limit_; }
    thread_safe_queue& limit( size_t nQueueLimit ) {
        limit_ = nQueueLimit;
        return ( *this );
    }
    void enqueue( element_type& t ) {
        lock_type lock( mutex_ );
        size_t cnt = queue_.size();
        if ( limit_ > 0 && cnt >= limit_ )
            throw std::runtime_error( "queue size limit reached" );
        queue_.push( t );
    }
    bool dequeue( element_type& t ) {
        lock_type lock( mutex_ );
        if ( queue_.empty() )
            return false;
        t = std::move( queue_.front() );
        queue_.pop();
        return true;
    }
};  /// template class thread_safe_queue

class thread_pool {
private:
    class worker {
    private:
        thread_pool* pool_;
        size_t id_;

    public:
        worker( thread_pool* p, const size_t i );
        ~worker();
        void invoke();
        void operator()();
        thread_pool* get_pool() { return pool_; }
        const thread_pool* get_pool() const { return pool_; }
    };  /// class worker

    thread_safe_queue< std::function< void() > > queue_;
    std::vector< std::thread > threads_;
    volatile bool shutdown_flag_;
    typedef std::mutex mutex_type;  // typedef skutils::multithreading::mutex_type mutex_type;
    mutex_type conditional_mutex_;  // skutils::multithreading::mutex_type conditional_mutex_;
    std::condition_variable_any conditional_lock_;  // std::condition_variable_any
                                                    // conditional_lock_;
    void init();
    void shutdown();

public:
    thread_pool( const size_t nNumberOfThreads, const size_t nQueueLimit = 0 );
    thread_pool( const thread_pool& ) = delete;
    thread_pool( thread_pool&& ) = delete;
    thread_pool& operator=( const thread_pool& ) = delete;
    thread_pool& operator=( thread_pool&& ) = delete;
    ~thread_pool();
    size_t number_of_threads() const { return threads_.size(); }
    size_t queue_size() const { return queue_.size(); }
    size_t queue_limit() const { return queue_.limit(); }
    thread_pool& queue_limit( size_t n ) {
        queue_.limit( n );
        return ( *this );
    }
    template < typename F, typename... Args >
    auto submit( F&& f, Args&&... args ) -> std::future< decltype( f( args... ) ) > {
        // create a function with bounded parameters ready to execute
        std::function< decltype( f( args... ) )() > func =
            std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
        // encapsulate it into a shared ptr in order to be able to copy construct / assign
        auto task_ptr =
            std::make_shared< std::packaged_task< decltype( f( args... ) )() > >( func );
        // wrap packaged task into void function
        std::function< void() > wrapper_func = [task_ptr]() { ( *task_ptr )(); };
        // enqueue generic wrapper function
        queue_.enqueue( wrapper_func );
        // wake up one thread if its waiting
        conditional_lock_.notify_one();
        // return future from promise
        return task_ptr->get_future();
    }
    template < typename F, typename... Args >
    void submit_without_future( F&& f, Args&&... args ) {
        std::function< decltype( f( args... ) )() > func =
            std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
        queue_.enqueue( func );
        // wake up one thread if its waiting
        conditional_lock_.notify_one();
    }
    template < typename F, typename... Args >
    bool safe_submit_without_future( F&& f, Args&&... args ) {
        try {
            std::function< decltype( f( args... ) )() > func =
                std::bind( std::forward< F >( f ), std::forward< Args >( args )... );
            queue_.enqueue( func );
            // wake up one thread if its waiting
            conditional_lock_.notify_one();
            return true;
        } catch ( ... ) {
        }
        return false;
    }
};  /// class thread_pool

};  // namespace skutils

#endif  /// (!defined __SKUTILS_THREAD_POOL_H)
