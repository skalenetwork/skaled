#include <skutils/multithreading.h>
#include <skutils/utils.h>

namespace skutils {

namespace multithreading {

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string getThreadName() {
    char name[128];
    memset( name, 0, sizeof( name ) );
    pthread_getname_np( pthread_self(), name, sizeof( name ) / sizeof( name[0] ) - 1 );
    return std::string( name );
}
void setThreadName( const char* name ) {
#if defined( __APPLE__ )
    pthread_setname_np( name );
#else
    pthread_setname_np( pthread_self(), name );
#endif
}
void setThreadName( const std::string& name ) {
    setThreadName( name.c_str() );
}

fn_on_exception_t& on_exception_handler() {
    static fn_on_exception_t g_fn;  // returns false to cancel exception throwing
    return g_fn;
}

static void stat_exception( const std::string& s ) {
    try {
        fn_on_exception_t& fn = on_exception_handler();
        if ( fn ) {
            if ( !fn( s ) )  // returns false to cancel exception throwing
                return;
        }
    } catch ( ... ) {
    }
    throw std::runtime_error( s );
}

#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )

const char* mutex_action_type_2_str( e_mutex_action_type_t emat ) {
    switch ( emat ) {
    case e_mutex_action_type_t::emat_init:
        return "init";
    case e_mutex_action_type_t::emat_done:
        return "done";
    case e_mutex_action_type_t::emat_validate:
        return "validate";
    case e_mutex_action_type_t::emat_will_lock:
        return "lock";
    case e_mutex_action_type_t::emat_did_locked_success:
        return "lock-success";
    case e_mutex_action_type_t::emat_did_locked_fail:
        return "lock-fail";
    case e_mutex_action_type_t::emat_will_unlock:
        return "unlock";
    case e_mutex_action_type_t::emat_did_unlocked_success:
        return "unlock-success";
    case e_mutex_action_type_t::emat_did_unlocked_fail:
        return "unlock-fail";
    case e_mutex_action_type_t::emat_will_try_lock:
        return "try-lock";
    case e_mutex_action_type_t::emat_did_try_locked_success:
        return "try-lock-success";
    case e_mutex_action_type_t::emat_did_try_locked_fail:
        return "try-lock-fail";
    default:
        break;
    }  /// switch( emat )
    return "emat-unknown";
}

fn_on_mutex_action_t& on_mutex_action_handler() {  // invoked on all actions(including errors)
    static fn_on_mutex_action_t g_fn;
    return g_fn;
}
fn_on_mutex_action_t& on_mutex_error_handler() {  // invoked only on errors
    static fn_on_mutex_action_t g_fn;
    return g_fn;
}
static void stat_mutex_action( e_mutex_action_type_t emat, pthread_lockable* pLockable ) {
    if ( is_error_mutex_action( emat ) ) {
        try {
            fn_on_mutex_action_t& fn = on_mutex_error_handler();  // invoked only on errors
            if ( fn )
                fn( emat, pLockable );
        } catch ( ... ) {
        }
    }
    try {
        fn_on_mutex_action_t& fn =
            on_mutex_action_handler();  // invoked on all actions(including errors)
        if ( fn )
            fn( emat, pLockable );
    } catch ( ... ) {
    }
}
bool is_error_mutex_action( e_mutex_action_type_t emat ) {
    switch ( emat ) {
    case e_mutex_action_type_t::emat_init_fail:
    case e_mutex_action_type_t::emat_done_empty:
    case e_mutex_action_type_t::emat_did_locked_fail:
    case e_mutex_action_type_t::emat_did_unlocked_fail:
    case e_mutex_action_type_t::emat_did_try_locked_fail:
        return true;
    default:
        return false;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool stat_validator( pthread_lockable* pLockable, pthread_mutex_t* pMtx,
    e_mutex_action_type_t emat, const char* operation_name, const char* recursive_suffix_str ) {
    if ( pLockable == nullptr )
        stat_exception( "attempt to validate null lockable pointer" );
    if ( pMtx == nullptr )
        stat_exception( "attempt to validate null mutex pointer" );
    std::string e, name;
    {  // block for g_mtx
        static std::mutex g_mtx;
        std::lock_guard< std::mutex > lock( g_mtx );
        static std::set< pthread_lockable* > g_set_lockables;
        static std::set< pthread_mutex_t* > g_set_mutexes;
        static std::set< std::string > g_set_names;
        name = pLockable->lockable_name();
        switch ( emat ) {
        case e_mutex_action_type_t::emat_init:
            if ( g_set_lockables.find( pLockable ) != g_set_lockables.end() )
                e = skutils::tools::format(
                    "attempt to initialize non-unique lockable %p/%p/%s/%s during operation %s",
                    pLockable, pMtx, name.c_str(), recursive_suffix_str, operation_name );
            if ( g_set_mutexes.find( pMtx ) != g_set_mutexes.end() )
                e = skutils::tools::format(
                    "attempt to initialize non-unique mutex %p/%p/%s/%s during operation %s",
                    pLockable, pMtx, name.c_str(), recursive_suffix_str, operation_name );
            if ( g_set_names.find( name ) != g_set_names.end() )
                e = skutils::tools::format(
                    "attempt to initialize non-unique name %p/%p/%s/%s during operation %s",
                    pLockable, pMtx, name.c_str(), recursive_suffix_str, operation_name );
            if ( e.empty() ) {
                g_set_lockables.insert( pLockable );
                g_set_mutexes.insert( pMtx );
                g_set_names.insert( name );
            }
            break;
        case e_mutex_action_type_t::emat_done:
            if ( g_set_lockables.find( pLockable ) == g_set_lockables.end() )
                e = skutils::tools::format(
                    "attempt to deinitialize non-existing lockable %p/%p/%s/%s during operation %s",
                    pLockable, pMtx, name.c_str(), recursive_suffix_str, operation_name );
            if ( g_set_mutexes.find( pMtx ) == g_set_mutexes.end() )
                e = skutils::tools::format(
                    "attempt to deinitialize non-existing mutex %p/%p/%s/%s during operation %s",
                    pLockable, pMtx, name.c_str(), recursive_suffix_str, operation_name );
            if ( g_set_names.find( name ) == g_set_names.end() )
                e = skutils::tools::format(
                    "attempt to deinitialize non-existing name %p/%p/%s/%s during operation %s",
                    pLockable, pMtx, name.c_str(), recursive_suffix_str, operation_name );
            if ( e.empty() ) {
                g_set_lockables.erase( pLockable );
                g_set_mutexes.erase( pMtx );
                g_set_names.erase( name );
            }
            break;
        case e_mutex_action_type_t::emat_validate:
            if ( g_set_lockables.find( pLockable ) == g_set_lockables.end() )
                e = skutils::tools::format(
                    "attempt to access non-existing lockable %p/%p/%s/%s during operation %s",
                    pLockable, pMtx, name.c_str(), recursive_suffix_str, operation_name );
            if ( g_set_mutexes.find( pMtx ) == g_set_mutexes.end() )
                e = skutils::tools::format(
                    "attempt to access non-existing mutex %p/%p/%s/%s during operation %s",
                    pLockable, pMtx, name.c_str(), recursive_suffix_str, operation_name );
            if ( g_set_names.find( name ) == g_set_names.end() )
                e = skutils::tools::format(
                    "attempt to access non-existing name %p/%p/%s/%s during operation %s",
                    pLockable, pMtx, name.c_str(), recursive_suffix_str, operation_name );
            break;
        default:
            e = skutils::tools::format(
                "attempt to perform bad validation call %p/%p/%s/%s during operation %s", pLockable,
                pMtx, name.c_str(), recursive_suffix_str, operation_name );
            break;
        }  // switch( emat )
    }      // block for g_mtx
    if ( !e.empty() )
        stat_exception( e );
    return true;
}

#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

pthread_lockable::pthread_lockable() {
    lockable_name( nullptr );
}
pthread_lockable::pthread_lockable( const char* name ) {
    lockable_name( name );
}
pthread_lockable::pthread_lockable( const std::string& name ) {
    lockable_name( name.c_str() );
}
pthread_lockable::~pthread_lockable() {}
const char* pthread_lockable::lockable_name() const {
    return ( !name_.empty() ) ? name_.c_str() : "unnamed";
}
void pthread_lockable::lockable_name( const char* name ) {
    if ( name == nullptr || name[0] == '\0' )
        name_ = skutils::tools::format( "unnamed-%p", this );
    else
        name_ = skutils::tools::format( "%s-%p", name, this );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

basic_pthread_mutex::basic_pthread_mutex( bool is_recursive )
    : initialized_( false ), is_recursive_( is_recursive ) {
    init( is_recursive );
}
basic_pthread_mutex::basic_pthread_mutex( bool is_recursive, const char* name )
    : pthread_lockable( name ), initialized_( false ), is_recursive_( is_recursive ) {
    init( is_recursive );
}
basic_pthread_mutex::basic_pthread_mutex( bool is_recursive, const std::string& name )
    : pthread_lockable( name ), initialized_( false ), is_recursive_( is_recursive ) {
    init( is_recursive );
}
basic_pthread_mutex::~basic_pthread_mutex() {
    if ( initialized_ ) {
#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )
        stat_validator( this, &mtx_, e_mutex_action_type_t::emat_done,
            "skutils::multithreading::basic_pthread_mutex::dtor",
            helper_recursive_suffix_string() );
#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)
        initialized_ = false;
        pthread_mutex_destroy( &mtx_ );
#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )
        stat_mutex_action( e_mutex_action_type_t::emat_done, this );
#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)
    } else {
#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )
        stat_mutex_action( e_mutex_action_type_t::emat_done_empty, this );
#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)
    }
}
void basic_pthread_mutex::init( bool is_recursive ) {
    is_recursive_ = is_recursive;
    if ( is_recursive ) {
        pthread_mutexattr_init( &attr_ );
        pthread_mutexattr_settype( &attr_, PTHREAD_MUTEX_RECURSIVE );
        int n = pthread_mutex_init( &mtx_, &attr_ );
        if ( n != 0 ) {
#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )
            stat_mutex_action( e_mutex_action_type_t::emat_init_fail, this );
#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)
            stat_exception( skutils::tools::format(
                "failed create skutils::multithreading::basic_pthread_mutex %p/%p/%s/%s", this,
                &mtx_, lockable_name(), helper_recursive_suffix_string() ) );
        }
    } else {
        int n = pthread_mutex_init( &mtx_, nullptr );
        if ( n != 0 ) {
#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )
            stat_mutex_action( e_mutex_action_type_t::emat_init_fail, this );
#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)
            stat_exception( skutils::tools::format(
                "failed create skutils::multithreading::basic_pthread_mutex %p/%p/%s/%s", this,
                &mtx_, lockable_name(), helper_recursive_suffix_string() ) );
        }
    }
#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )
    stat_validator( this, &mtx_, e_mutex_action_type_t::emat_init,
        "skutils::multithreading::basic_pthread_mutex::ctor-1", helper_recursive_suffix_string() );
#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)
    initialized_ = true;
#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )
    stat_validator( this, &mtx_, e_mutex_action_type_t::emat_validate,
        "skutils::multithreading::basic_pthread_mutex::ctor-2", helper_recursive_suffix_string() );
    stat_mutex_action( e_mutex_action_type_t::emat_init, this );
#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)
}
bool basic_pthread_mutex::is_recursive_mutex() const {
    return is_recursive_;
}
const char* basic_pthread_mutex::helper_recursive_suffix_string() const {
    return is_recursive_mutex() ? "recursive" : "non-recursive";
}
void basic_pthread_mutex::lock() {
    if ( !initialized_ )
        stat_exception(
            skutils::tools::format( "attempt to lock un-initialized "
                                    "skutils::multithreading::basic_pthread_mutex %p/%p/%s/%s",
                this, &mtx_, lockable_name(), helper_recursive_suffix_string() ) );
#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )
    stat_validator( this, &mtx_, e_mutex_action_type_t::emat_validate,
        "skutils::multithreading::basic_pthread_mutex::lock", helper_recursive_suffix_string() );
    stat_mutex_action( e_mutex_action_type_t::emat_will_lock, this );
#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)
    int n = pthread_mutex_lock( &mtx_ );
    if ( n != 0 ) {
#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )
        stat_mutex_action( e_mutex_action_type_t::emat_did_locked_fail, this );
#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)
        stat_exception( skutils::tools::format(
            "failed to lock skutils::multithreading::basic_pthread_mutex %p/%p/%s/%s", this, &mtx_,
            lockable_name(), helper_recursive_suffix_string() ) );
        return;
    }
#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )
    stat_mutex_action( e_mutex_action_type_t::emat_did_locked_success, this );
#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)
}
void basic_pthread_mutex::unlock() {
    if ( !initialized_ )
        stat_exception(
            skutils::tools::format( "attempt to un-lock un-initialized "
                                    "skutils::multithreading::basic_pthread_mutex %p/%p/%s/%s",
                this, &mtx_, lockable_name(), helper_recursive_suffix_string() ) );
#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )
    stat_validator( this, &mtx_, e_mutex_action_type_t::emat_validate,
        "skutils::multithreading::basic_pthread_mutex::unlock", helper_recursive_suffix_string() );
    stat_mutex_action( e_mutex_action_type_t::emat_will_unlock, this );
#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)
    int n = pthread_mutex_unlock( &mtx_ );
    if ( n != 0 ) {
#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )
        stat_mutex_action( e_mutex_action_type_t::emat_did_unlocked_fail, this );
#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)
        stat_exception(
            skutils::tools::format( "failed to un-lock un-initialized "
                                    "skutils::multithreading::basic_pthread_mutex %p/%p/%s/%s",
                this, &mtx_, lockable_name(), helper_recursive_suffix_string() ) );
        return;
    }
#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )
    stat_mutex_action( e_mutex_action_type_t::emat_did_unlocked_success, this );
#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)
}
bool basic_pthread_mutex::try_lock() {
    if ( !initialized_ )
        stat_exception(
            skutils::tools::format( "attempt to try-lock un-initialized "
                                    "skutils::multithreading::basic_pthread_mutex %p/%p/%s/%s",
                this, &mtx_, lockable_name(), helper_recursive_suffix_string() ) );
#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )
    stat_validator( this, &mtx_, e_mutex_action_type_t::emat_validate,
        "skutils::multithreading::basic_pthread_mutex::try_lock",
        helper_recursive_suffix_string() );
    stat_mutex_action( e_mutex_action_type_t::emat_will_try_lock, this );
#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)
    int n = pthread_mutex_trylock( &mtx_ );
    if ( n != 0 ) {
#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )
        stat_mutex_action( e_mutex_action_type_t::emat_did_try_locked_fail, this );
#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)
        return false;
    }
#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )
    stat_mutex_action( e_mutex_action_type_t::emat_did_try_locked_success, this );
#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

pthread_mutex::pthread_mutex() : basic_pthread_mutex( false ) {}
pthread_mutex::pthread_mutex( const char* name ) : basic_pthread_mutex( false, name ) {}
pthread_mutex::pthread_mutex( const std::string& name ) : basic_pthread_mutex( false, name ) {}
pthread_mutex::~pthread_mutex() {}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

pthread_recursive_mutex::pthread_recursive_mutex() : basic_pthread_mutex( true ) {}
pthread_recursive_mutex::pthread_recursive_mutex( const char* name )
    : basic_pthread_mutex( true, name ) {}
pthread_recursive_mutex::pthread_recursive_mutex( const std::string& name )
    : basic_pthread_mutex( true, name ) {}
pthread_recursive_mutex::~pthread_recursive_mutex() {}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace multithreading

};  // namespace skutils
