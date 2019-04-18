#if ( !defined __SKUTILS_ATOMIC_SHARED_PTR_H )
#define __SKUTILS_ATOMIC_SHARED_PTR_H 1

#include <skutils/multithreading.h>
#include <atomic>
#include <exception>
#include <list>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <typeinfo>

namespace skutils {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern skutils::multithreading::recursive_mutex_type& get_ref_mtx();
extern void done_ref_mtx();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template < typename T >
class atomic_shared_traits {
public:
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;
    typedef std::shared_ptr< T > stored_t;
    static mutex_type& mtx() {
        //			static mutex_type g_mtx( "RMTX-ATOMIC-SHARED-TRAITS" );
        //			return g_mtx;
        return skutils::get_ref_mtx();
    }
};  /// template class atomic_shared_traits

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class ref_retain_release {
protected:
    mutable std::atomic_size_t ref_cnt_;
    mutable std::atomic_bool ref_is_zombie_;
    std::string zombie_desc_;

public:
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;
    ref_retain_release() : ref_cnt_( 1 ), ref_is_zombie_( false ) {}
    ref_retain_release( const ref_retain_release& ) = delete;
    ref_retain_release( ref_retain_release&& ) = delete;
    ref_retain_release& operator=( const ref_retain_release& ) = delete;
    ref_retain_release& operator=( ref_retain_release&& ) = delete;
    virtual ~ref_retain_release() {}
    virtual mutex_type& ref_mtx() const {
        //			static mutex_type g_mtx( "RMTX-RR" );
        //			return g_mtx;
        return skutils::get_ref_mtx();
    }
    virtual std::string ref_zombie_desc() const { return zombie_desc_; }
    virtual void ref_zombie_desc( const std::string& strZombieDescription ) {
        zombie_desc_ = strZombieDescription;
    }
    virtual void ref_destroy() { delete this; }
    virtual void ref_zombie_op() {}
    virtual void ref_zombie_retain() { ref_zombie_op(); }
    virtual void ref_zombie_release() { ref_zombie_op(); }
    virtual size_t ref_count() const { return ref_cnt_; }
    virtual bool ref_is_zombie() const { return ref_is_zombie_; }
    virtual void ref_mark_as_zombie() { ref_is_zombie_ = true; }
    virtual bool ref_check_zombie_mode( const char* /*strLogMessage*/ ) const {
        if ( !ref_is_zombie() )
            return false;
        return true;
    }
    virtual size_t ref_retain() {
        lock_type lock( ref_mtx() );
        if ( ref_is_zombie_ ) {
            ref_zombie_retain();
            return ref_cnt_;
        } else {
            ++ref_cnt_;
            return ref_cnt_;
        }
    }
    virtual size_t ref_release() {
        if ( ref_is_zombie_ ) {
            ref_zombie_release();
            return 0;
        }
        size_t n = 0;
        {  // block
            lock_type lock( ref_mtx() );
            --ref_cnt_;
            n = ref_cnt_;
        }  // block
        if ( n == 0 ) {
            ref_mark_as_zombie();
            ref_is_zombie_ = true;
            ref_destroy();
        }
        return n;
    }
};  /// class ref_retain_release

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template < typename T, typename TR = atomic_shared_traits< T > >
class retain_release_ptr {
public:
protected:
    typedef std::atomic< T* > stored_t;
    stored_t stored_;
    typedef typename TR::mutex_type mutex_type;
    typedef typename TR::lock_type lock_type;

private:
    void throw_nullptr_access_attempt( T* ptr ) const {
        std::stringstream ss;
        ss << "retain_release_ptr/" << ( typeid( ptr ).name() ) << "/" << ( ( void* ) this )
           << " NULL access attempt";
        throw std::runtime_error( ss.str() );
    }

public:
    virtual mutex_type& ref_mtx() const { return TR::mtx(); }
    //		static retain_release_ptr < T, TR > make( const T * pRawOther = nullptr ) {
    //			lock_type lock( / *ref_mtx()* / TR::mtx() );
    //			retain_release_ptr < T, TR > p( pRawOther );
    //			return p;
    //		}
    T* get() {
        // lock_type lock( ref_mtx() );
        return stored_;
    }
    const T* get() const {
        // lock_type lock( ref_mtx() );
        return stored_;
    }
    T* get_safe() {
        T* pRawThis = get();
        if ( pRawThis == nullptr )
            throw_nullptr_access_attempt( pRawThis );
        return pRawThis;
    }
    const T* get_safe() const {
        const T* pRawThis = get();
        if ( pRawThis == nullptr )
            throw_nullptr_access_attempt( const_cast< T* >( pRawThis ) );
        return pRawThis;
    }
    T* get_unconst() const { return const_cast< retain_release_ptr< T, TR >* >( this )->get(); }
    void ref_retain() {
        lock_type lock( ref_mtx() );
        T* p = get();
        if ( p )
            p->ref_retain();
    }
    void ref_release() {
        lock_type lock( ref_mtx() );
        T* p = stored_;
        if ( p ) {
            size_t cnt = p->ref_release();
            if ( cnt == 0 )
                stored_ = nullptr;
        }
    }
    void attach( T* pRawOther = nullptr ) {
        lock_type lock( ref_mtx() );
        T* pRawThis = const_cast< T* >( get() );
        if ( ( ( void* ) pRawThis ) == ( ( void* ) pRawOther ) )
            return;
        ref_release();
        stored_ = pRawOther;  // no-retain
    }
    void attach( const T* pRawOther = nullptr ) { attach( const_cast< T* >( pRawOther ) ); }
    void assign( const T* pRawOther = nullptr ) {
        lock_type lock( ref_mtx() );
        T* pRawThis = const_cast< T* >( get() );
        if ( ( ( void* ) pRawThis ) == ( ( void* ) pRawOther ) )
            return;
        ref_release();
        stored_ = nullptr;
        T* y = const_cast< T* >( pRawOther );
        stored_ = y;
        ref_retain();
    }
    void assign( const retain_release_ptr< T, TR >& refOther ) {
        lock_type lock( ref_mtx() );
        T* pRawThis = const_cast< T* >( get() );
        T* pRawOther = const_cast< T* >( refOther.get() );
        if ( ( ( void* ) pRawThis ) == ( ( void* ) pRawOther ) )
            return;
        stored_ = pRawOther;
        ref_retain();
    }
    void move( retain_release_ptr< T, TR >& refOther ) {
        lock_type lock( ref_mtx() );
        assign( refOther );
        refOther.clear();
    }
    void clear() { reset(); }
    int compare( const T* r ) const {
        const T* l = get();
        if ( l < r )
            return -1;
        if ( l > r )
            return 1;
        return 0;
    }
    int compare( const retain_release_ptr< T, TR >& refOther ) const {
        return compare( refOther.get() );
    }
    retain_release_ptr() : stored_( nullptr ) {}
    retain_release_ptr( T* pRawOther ) : stored_( nullptr ) { assign( pRawOther ); }
    retain_release_ptr( const T* pRawOther ) : stored_( nullptr ) { assign( pRawOther ); }
    retain_release_ptr( const retain_release_ptr< T, TR >& refOther ) : stored_( nullptr ) {
        assign( refOther );
    }
    retain_release_ptr( retain_release_ptr< T, TR >&& refOther ) : stored_( nullptr ) {
        move( refOther );
    }
    ~retain_release_ptr() { clear(); }
    void reset( T* pRawOther = nullptr ) { assign( pRawOther ); }
    void reset( const retain_release_ptr< T, TR >& refOther ) { assign( refOther ); }
    bool empty() const {
        if ( get() )
            return false;
        return true;
    }
    operator bool() const { return ( !empty() ); }
    bool operator!() const { return empty(); }
    T* operator->() { return get_safe(); }
    const T* operator->() const { return get_safe(); }
    T& operator*() { return ( *get_safe() ); }
    const T& operator*() const { return ( *get_safe() ); }
    bool operator==( const T* pRawOther ) const {
        return ( compare( pRawOther ) == 0 ) ? true : false;
    }
    bool operator==( const retain_release_ptr< T, TR >& refOther ) const {
        return ( compare( refOther ) == 0 ) ? true : false;
    }
    bool operator!=( const T* pRawOther ) const {
        return ( compare( pRawOther ) != 0 ) ? true : false;
    }
    bool operator!=( const retain_release_ptr< T, TR >& refOther ) const {
        return ( compare( refOther ) != 0 ) ? true : false;
    }
    bool operator<( const T* pRawOther ) const {
        return ( compare( pRawOther ) < 0 ) ? true : false;
    }
    bool operator<( const retain_release_ptr< T, TR >& refOther ) const {
        return ( compare( refOther ) < 0 ) ? true : false;
    }
    bool operator<=( const T* pRawOther ) const {
        return ( compare( pRawOther ) <= 0 ) ? true : false;
    }
    bool operator<=( const retain_release_ptr< T, TR >& refOther ) const {
        return ( compare( refOther ) <= 0 ) ? true : false;
    }
    bool operator>( const T* pRawOther ) const {
        return ( compare( pRawOther ) > 0 ) ? true : false;
    }
    bool operator>( const retain_release_ptr< T, TR >& refOther ) const {
        return ( compare( refOther ) > 0 ) ? true : false;
    }
    bool operator>=( const T* pRawOther ) const {
        return ( compare( pRawOther ) >= 0 ) ? true : false;
    }
    bool operator>=( const retain_release_ptr< T, TR >& refOther ) const {
        return ( compare( refOther ) >= 0 ) ? true : false;
    }
    retain_release_ptr< T, TR >& operator=( const T* pRawOther ) {
        assign( pRawOther );
        return ( *this );
    }
    retain_release_ptr< T, TR >& operator=( const retain_release_ptr< T, TR >& refOther ) {
        assign( refOther );
        return ( *this );
    }
    retain_release_ptr< T, TR >& operator=( retain_release_ptr< T, TR >&& refOther ) {
        move( refOther );
        return ( *this );
    }
};  /// template class retain_release_ptr

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template < typename T >
class zombie_pool {
    size_t max_zombie_queue_size_;
    typedef std::list< T* > zombie_queue_t;
    zombie_queue_t zombie_queue_;
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;
    mutable mutex_type zombie_mtx_;
    mutex_type& mtx() const { return zombie_mtx_; }

public:
    size_t size() const {
        lock_type lock( mtx() );
        return zombie_queue_.size();
    }
    bool pop() {
        lock_type lock( mtx() );
        if ( zombie_queue_.empty() )
            return false;
        typename zombie_queue_t::iterator zombie_it = zombie_queue_.begin();
        T* p = ( *zombie_it );
        zombie_queue_.erase( zombie_it );
        try {
            delete p;
        } catch ( ... ) {
        }
        return true;
    }
    size_t push( T* p ) {
        lock_type lock( mtx() );
        zombie_queue_.push_back( p );
        while ( size() > max_zombie_queue_size_ )
            pop();
        return zombie_queue_.size();
    }
    bool empty() const {
        lock_type lock( mtx() );
        return zombie_queue_.empty();
    }
    void clear() {
        lock_type lock( mtx() );
        while ( !empty() )
            pop();
    }
    zombie_pool( size_t max_zombie_queue_size = 256 )
        : max_zombie_queue_size_( max_zombie_queue_size ), zombie_mtx_( ( "RMTX-ZOMBIE-POOL" ) ) {}
    zombie_pool( const zombie_pool& ) = delete;
    zombie_pool( zombie_pool&& ) = delete;
    virtual ~zombie_pool() { clear(); }
    zombie_pool operator=( const zombie_pool& ) = delete;
    zombie_pool operator=( zombie_pool&& ) = delete;
};  /// template < typename T > class zombie_pool

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace skutils

#endif  /// (!defined __SKUTILS_ATOMIC_SHARED_PTR_H)
