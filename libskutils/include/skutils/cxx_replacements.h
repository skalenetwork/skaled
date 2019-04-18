#if ( !defined __SKUTILS_CXX_REPLACEMENTS_H )
#define __SKUTILS_CXX_REPLACEMENTS_H 1

#include <condition_variable>
#include <mutex>
#include <thread>

#include <stdint.h>

/*
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <exception>
#include <functional>
*/

namespace skutils {

extern void sleep_for_microseconds( uint64_t n );
extern void sleep_for_milliseconds( uint64_t n );
extern void sleep_for_seconds( uint64_t n );

bool is_nan( float f );
bool is_nan( double d );

/*
    typedef int32_t status_t;
    typedef int64_t msecs_t; // milli-seconds
    typedef int64_t nsecs_t; // nano-seconds
    class condition_variable;

    class mutex {
    public:
        enum {
            PRIVATE = 0,
            SHARED  = 1
        };
        mutex();
        mutex( const char * name );
        mutex( int type, const char * name = nullptr );
        ~mutex();
        status_t lock();
        void unlock();
        status_t try_lock();
    private:
        friend class condition_variable;
        mutex( const mutex & ) = delete;
        mutex & operator = ( const mutex & ) = delete;
        pthread_mutex_t mutex_;
    }; /// class mutex

    class recursive_mutex {
    public:
        enum {
            PRIVATE = 0,
            SHARED  = 1
        };
        recursive_mutex();
        recursive_mutex( const char * name );
        recursive_mutex( int type, const char * name = nullptr );
        ~recursive_mutex();
        status_t lock();
        void unlock();
        status_t try_lock();
    private:
        friend class condition_variable;
        recursive_mutex( const recursive_mutex & ) = delete;
        recursive_mutex & operator = ( const recursive_mutex & ) = delete;
        pthread_mutex_t mutex_;
    }; /// class recursive_mutex

    template < typename lock_object_t > class lock_guard {
        lock_object_t & mutex_;
    public:
        lock_guard( lock_object_t & m ) : mutex_( m  ) { mutex_.lock(); }
        lock_guard( lock_object_t * m ) : mutex_( *m ) { mutex_.lock(); }
        ~lock_guard() { mutex_.unlock(); }
        friend class condition_variable;
    }; /// class lock_guard

    template < typename lock_object_t > class unique_lock : public lock_guard<lock_object_t> {
    public:
        unique_lock( lock_object_t & m ) : lock_guard < lock_object_t >( m ) { }
        unique_lock( lock_object_t * m ) : lock_guard < lock_object_t >( m ) { }
        ~unique_lock() { }
    }; /// class unique_lock

    class condition_variable {
    public:
        enum {
            PRIVATE = 0,
            SHARED = 1
        };
        enum WakeUpType {
            WAKE_UP_ONE = 0,
            WAKE_UP_ALL = 1
        };
        condition_variable();
        condition_variable( int type );
        ~condition_variable();
        status_t wait( mutex & m );
        status_t waitRelative( mutex & m, nsecs_t reltime );
        status_t wait( lock_guard<mutex> & m ) { return wait( m.mutex_ ); }
        status_t waitRelative( lock_guard<mutex> & m, nsecs_t reltime ) { return waitRelative(
m.mutex_, reltime ); } void signal(); void signal( WakeUpType type ) { if( type == WAKE_UP_ONE )
                signal();
            else
                broadcast();
        }
        void broadcast();
        void notify_one() { signal(); }
        void notify_all() { broadcast(); }
    private:
//    #if defined(HAVE_PTHREADS)
        pthread_cond_t cond_;
//    #else
//        void*   mState;
//    #endif
    }; /// class condition_variable

    class thread {
        pthread_t t_;
        static void * stat_thread_proc( void * opaque );
        void * thread_proc();
    public:
        typedef std::function < void * ( void * ) > fn_t;
    private:
        void * arg_;
        fn_t fn_;
        bool create_fn( fn_t fn, void * arg );
    public:
        template < typename callable_t > bool create( callable_t fn, void * arg ) {
            fn_t fnx = [&]( void * arg ) { fn( arg ); return nullptr; };
            return create_fn( fnx, arg );
        }
        thread();
        thread( thread & t ) = delete;
        thread( const thread & ) = delete;
        thread( thread && t );
//        thread( fn_t fn, void * arg )
//            : t_( pthread_t(nullptr) )
//            , arg_( nullptr )
//        {
//            create( fn, arg );
//        }
        template < typename callable_t > thread( callable_t fn, void * arg )
            : t_( pthread_t(nullptr) )
            , arg_( nullptr )
        {
            create( fn, arg );
        }
        ~thread();
        thread & operator=( const thread & ) = delete;
        thread & operator=( thread && t );
        void swap( thread & t );
        bool joinable() const;
        void join();
        pthread_t detach();
        friend thread get_this_thread();
    }; /// class thread

    mutex & get_interlocket_mutex();
    thread get_this_tinterlocked_inchread();

    namespace this_thread {
        void sleep_for_nanoseconds( const nsecs_t & sleep_duration );
        template< class Rep, class Period >
        inline void sleep_for( const std::chrono::duration< Rep, Period > & sleep_duration ) {
            sleep_for_nanoseconds(
std::chrono::duration_cast<std::chrono::nanoseconds>(sleep_duration).count() );
        }
    }; /// namespace this_thread

    template < typename T > inline T & interlocked_inc( T & x ) {
        lock_guard<mutex> lock( get_interlocket_mutex() );
        ++ x;
        return x;
    }
    template < typename T > inline T & interlocked_dec( T & x ) {
        lock_guard<mutex> lock( get_interlocket_mutex() );
        -- x;
        return x;
    }

    template < class T > class shared_ptr {
        struct aux {
            unsigned count;
            aux() :count(1) { }
            virtual void destroy() = 0;
            virtual ~aux() { } // must be polymorphic
        };
        template < class U, class Deleter > struct auximpl : public aux {
            U * p;
            Deleter d;
            auximpl( U * pu, Deleter x ) :p( pu ), d( x ) { }
            virtual void destroy() { d( p ); }
        };
        template < class U > struct default_deleter {
            void operator()( U * p ) const { delete p; }
        };
        aux * pa;
        T * pt;
        void inc() { if( pa ) interlocked_inc( pa->count ); }
        void dec() {
            if( pa && (!interlocked_dec(pa->count)) )
                {  pa->destroy(); delete pa; }
        }
    public:
        shared_ptr() : pa(), pt() { }
        template < class U, class Deleter > shared_ptr( U * pu, Deleter d ) : pa( new
auximpl<U,Deleter>(pu,d)), pt(pu) { } template< class U > explicit shared_ptr( U * pu ) :pa( new
auximpl<U,default_deleter<U> >(pu,default_deleter<U>()) ), pt(pu) {} shared_ptr( const shared_ptr &
s ) : pa(s.pa), pt(s.pt) { inc(); } template< class U > shared_ptr( const shared_ptr<U> & s)
:pa(s.pa), pt(s.pt) { inc(); } ~shared_ptr() { dec(); } shared_ptr & operator=( const shared_ptr & s
) { if( this != &s ) { dec(); pa = s.pa; pt=s.pt; inc();
            }
            return *this;
        }
        T* operator->() const { return pt; }
        T& operator*() const { return *pt; }
        void reset( T * ptr = nullptr ) {
            shared_ptr<T> p( ptr );
            (*this) = p;
        }
        T* get() const {
            return pt;
        }
        operator bool () const { return ( pt != nullptr ) ? true : false; }
        bool operator! () const { return ( pt == nullptr ) ? true : false; }
    }; /// class shared_ptr

    template < class U > struct default_deleter {
        void operator()( U * p ) const { delete p; }
    };

    template < typename T, typename D = default_deleter<T> > class unique_ptr {
    public:
        typedef T element_type;
        typedef D deleter_type;
        typedef T * pointer;
        explicit unique_ptr( pointer p = nullptr )
            : ptr_(p)
            , deleter_( D() )
        {
        }
        unique_ptr( pointer p, deleter_type d )
            : ptr_(p)
            , deleter_(d)
        {
        }
        ~unique_ptr() {
            reset();
        }
        operator bool () const { return ( ptr_ != nullptr ) ? true : false; }
        bool operator! () const { return ( ptr_ == nullptr ) ? true : false; }
        pointer get() const {
            return ptr_;
        }
        pointer release() {
            pointer tmp = ptr_;
            ptr_ = 0;
            return tmp;
        }
        void reset( pointer p = nullptr ) {
            if( ptr_ != p ){
                if( ptr_ )
                    deleter_( ptr_ );
                ptr_ = p;
            }
        }
        void swap( unique_ptr & u ) {
            T * x = ptr_;
            ptr_ = u.ptr_;
            u.ptr_ = x;
        }
        unique_ptr( const unique_ptr & ) = delete;
        unique_ptr & operator=( const unique_ptr & ) = delete;
    private:
        pointer ptr_;
        deleter_type deleter_;
    }; /// class unique_ptr

    class basic_string {
        char * buf_;
    public:
        basic_string() : buf_( nullptr ) { }
        basic_string( const basic_string & other ) : buf_( ::strdup( other.buf_ ) ) { }
        basic_string( basic_string && other ) : buf_( nullptr ) { swap( other ); }
        basic_string( const char * sz ) : buf_( ::strdup( sz ) ) { }
        //basic_string( const char * sz, size_t len );
        ~basic_string() { clear(); }
        basic_string & operator = ( const basic_string & other ) { assign( other.buf_ ); }
        basic_string & operator = ( basic_string && other ) { if( buf_ != other.buf_ ) { clear();
swap(other); } return (*this); } const char * c_str() const { return ( buf_ == nullptr ) ? "" :
buf_; } protected: char * nonconst_c_str() { static char e[]=""; return ( buf_ == nullptr ) ? e :
buf_; } public: size_t size() const { return ( buf_ == nullptr ) ? 0 : (::strlen(buf_)); } size_t
length() const { return size(); } bool empty() const { return ( buf_ == nullptr || buf_[0] == '\0' )
? true : false; } void clear() { if( buf_ ) { ::free(buf_); buf_ = nullptr; } } void append( const
char * first, const char * last ) { if( first && last ) { size_t n = size_t(last-first) + size() +
1; if( n == 0 ) { clear(); return;
                }
                char * p = (char*)::calloc( 1, n );
                if( buf_ )
                    ::strcat( p, buf_ );
                size_t x = ::strlen( p );
                for( ; first != last; ++ first, ++x )
                    p[x] = (*first);
                clear();
                buf_ = p;
            }
        }
        void append( const char * s ) {
            if( s == nullptr || s[0] == '\0' )
                return;
            append( s, s + ::strlen(s) );
        }
        void append( const basic_string & other ) {
            append( other.buf_ );
        }
        void assign( const char * s ) {
            if( s != buf_ ) {
                clear(); append( s );
            }
        }
        void shrink_to_fit() { }
        void swap( basic_string & other ) {
            char * p = buf_;
            buf_ = other.buf_;
            other.buf_ = p;
        }
        basic_string & operator += ( const basic_string & other ) { append( other ); return (*this);
} basic_string & operator += ( const char * other ) { append( other ); return (*this); }
        basic_string & operator += ( char c ) { const char * s = &c; append( s, s + 1 ); return
(*this); } int compare( const char * s ) const { if( buf_ == s ) return 0; if( (!buf_) && s ) return
-1; if( buf_ && (!s) ) return 1; return ::strcmp( buf_, s );
        }
        int compare( const basic_string & s ) const {
            return compare( s.c_str() );
        }
        bool operator == ( const basic_string & s ) const { return ( compare( s ) == 0 ) ? true :
false; } bool operator != ( const basic_string & s ) const { return ( compare( s ) != 0 ) ? true :
false; } bool operator <  ( const basic_string & s ) const { return ( compare( s ) <  0 ) ? true :
false; } bool operator <= ( const basic_string & s ) const { return ( compare( s ) <= 0 ) ? true :
false; } bool operator >  ( const basic_string & s ) const { return ( compare( s ) >  0 ) ? true :
false; } bool operator >= ( const basic_string & s ) const { return ( compare( s ) >= 0 ) ? true :
false; } bool operator == ( const char * s ) const { return ( compare( s ) == 0 ) ? true : false; }
        bool operator != ( const char * s ) const { return ( compare( s ) != 0 ) ? true : false; }
        bool operator <  ( const char * s ) const { return ( compare( s ) <  0 ) ? true : false; }
        bool operator <= ( const char * s ) const { return ( compare( s ) <= 0 ) ? true : false; }
        bool operator >  ( const char * s ) const { return ( compare( s ) >  0 ) ? true : false; }
        bool operator >= ( const char * s ) const { return ( compare( s ) >= 0 ) ? true : false; }
        typedef char * iterator;
        typedef const char * const_iterator;
        iterator begin() { return nonconst_c_str(); }
        iterator end() { return nonconst_c_str() + size(); }
        const_iterator begin() const { return c_str(); }
        const_iterator end() const { return c_str() + size(); }
        const_iterator cbegin() const { return c_str(); }
        const_iterator cend() const { return c_str() + size(); }
        char & front() { return *begin(); }
        char & back() { iterator it = end(); -- it; return *it; }
        const char & front() const { return *cbegin(); }
        const char & back() const { const_iterator it = cend(); -- it; return *it; }
        char & at( size_t i ) { return *(begin()+1); }
        const char & at( size_t i ) const { return *(cbegin()+1); }
        char & operator[]( size_t i ) { return at(i); }
        const char & operator[]( size_t i ) const { return at(i); }
        void erase( const_iterator it ) {
            if( it == end() )
                return;
            basic_string s;
            s.append( cbegin(), it );
            ++ it;
            s.append( it, cend() );
            (*this) = s;
        }
        void erase( const_iterator itFrom, const_iterator itTo ) {
            if( itFrom == end() )
                return;
            basic_string s;
            s.append( cbegin(), itFrom );
            s.append( itTo, cend() );
            (*this) = s;
        }
    };

    inline basic_string operator + ( const basic_string & a, const basic_string & b ) { basic_string
c; c.append(a); c.append( b ); return c; }

    typedef basic_string string;
*/

};  // namespace skutils


#endif  /// (!defined __SKUTILS_CXX_REPLACEMENTS_H)
