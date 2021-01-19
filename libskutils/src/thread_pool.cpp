// note: based on https://github.com/mtrebi/thread-pool
#include <skutils/thread_pool.h>
#include <skutils/utils.h>

namespace skutils {

thread_pool::worker::worker( thread_pool* p, const size_t i ) : pool_( p ), id_( i ) {}

thread_pool::worker::~worker() {}

void thread_pool::worker::invoke() {
    skutils::multithreading::threadNamer tn( skutils::tools::format( "p%zu", id_ ) );
    std::function< void() > func;
    bool was_dequeued = false;
    for ( ;; ) {
        {  // block
            std::unique_lock< mutex_type > lock( pool_->conditional_mutex_ );

            if ( pool_->shutdown_flag_ )
                break;

            if ( pool_->queue_.empty() ) {
                pool_->conditional_lock_.wait( lock );
            }

            if ( !pool_->shutdown_flag_ )
                was_dequeued = pool_->queue_.dequeue( func );
        }  // block
        if ( was_dequeued ) {
            try {
                func();
            } catch ( ... ) {
            }
        }
    }  // while( ! pool_->shutdown_flag_ )
}
void thread_pool::worker::operator()() {
    invoke();
}

thread_pool::thread_pool( const size_t nNumberOfThreads,
    const size_t nQueueLimit  // = 0
    )
    : queue_( nQueueLimit ),
      threads_( std::vector< std::thread >( nNumberOfThreads ) ),
      shutdown_flag_( false )
//, conditional_mutex_( "MTX-THEAD-POOL-CONDITIONAL" )
{
    init();
}

thread_pool::~thread_pool() {
    shutdown();
}

void thread_pool::init() {
    size_t i, cnt = threads_.size();
    for ( i = 0; i < cnt; ++i )
        threads_[i] = std::thread( worker( this, i ) );
}

void thread_pool::shutdown() {
    {
        std::unique_lock< mutex_type > lock( conditional_mutex_ );
        shutdown_flag_ = true;
        conditional_lock_.notify_all();
    }

    size_t i, cnt = threads_.size();
    for ( i = 0; i < cnt; ++i ) {
        try {
            if ( threads_[i].joinable() )
                threads_[i].join();
        } catch ( ... ) {
        }
        conditional_lock_.notify_all();
    }
    threads_.clear();
}

};  // namespace skutils
