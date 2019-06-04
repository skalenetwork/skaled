#if ( !defined __SKUTILS_MULTITHREADING_H )
#define __SKUTILS_MULTITHREADING_H 1

#include <pthread.h>
#include <exception>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

//#define __SKUTILS_MULTITHREADING_DEBUG__ 1

namespace skutils {

namespace multithreading {

extern std::string getThreadName();
extern void setThreadName( const char* name );
extern void setThreadName( const std::string& name );

class threadNamer {
    std::string m_oldName;

public:
    threadNamer( const char* name ) : m_oldName( getThreadName() ) { setThreadName( name ); }
    threadNamer( const std::string& name ) : m_oldName( getThreadName() ) { setThreadName( name ); }
    threadNamer() { setThreadName( m_oldName ); }
};  /// class threadNamer

class threadNameAppender {
    std::string m_oldName;

public:
    threadNameAppender( const char* nameSuffix ) : m_oldName( getThreadName() ) {
        setThreadName( m_oldName + nameSuffix );
    }
    threadNameAppender( const std::string& nameSuffix ) : m_oldName( getThreadName() ) {
        setThreadName( m_oldName + nameSuffix );
    }
    threadNameAppender() { setThreadName( m_oldName ); }
};  /// class threadNameAppender

class pthread_lockable;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::function< bool( const std::string& s ) > fn_on_exception_t;  // returns false to cancel
                                                                          // exception throwing
extern fn_on_exception_t& on_exception_handler();

#if ( defined __SKUTILS_MULTITHREADING_DEBUG__ )

enum class e_mutex_action_type_t {
    emat_init,
    emat_done,
    emat_validate,
    emat_init_fail,
    emat_done_empty,
    emat_will_lock,
    emat_did_locked_success,
    emat_did_locked_fail,
    emat_will_unlock,
    emat_did_unlocked_success,
    emat_did_unlocked_fail,
    emat_will_try_lock,
    emat_did_try_locked_success,
    emat_did_try_locked_fail
};  /// enum class e_mutex_action_type_t
extern const char* mutex_action_type_2_str( e_mutex_action_type_t emat );

typedef std::function< void( e_mutex_action_type_t emat, pthread_lockable* pLockable ) >
    fn_on_mutex_action_t;
extern fn_on_mutex_action_t& on_mutex_action_handler();  // invoked on all actions(including errors)
extern fn_on_mutex_action_t& on_mutex_error_handler();   // invoked only on errors
extern bool is_error_mutex_action( e_mutex_action_type_t emat );

#endif  /// (defined __SKUTILS_MULTITHREADING_DEBUG__)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class pthread_lockable {
    std::string name_;

public:
    pthread_lockable();
    pthread_lockable( const char* name );
    pthread_lockable( const std::string& name );
    pthread_lockable( const pthread_lockable& ) = delete;
    pthread_lockable( pthread_lockable&& ) = delete;
    virtual ~pthread_lockable();
    const char* lockable_name() const;

protected:
    void lockable_name( const char* name );

public:
    pthread_lockable& operator=( const pthread_lockable& ) = delete;
    pthread_lockable& operator=( pthread_lockable&& ) = delete;
    virtual void lock() = 0;
    virtual void unlock() = 0;
    virtual bool try_lock() = 0;
};  /// class pthread_lockable

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class basic_pthread_mutex : public pthread_lockable {
    volatile bool initialized_, is_recursive_;
    pthread_mutex_t mtx_;
    pthread_mutexattr_t attr_;
    void init( bool is_recursive );
    const char* helper_recursive_suffix_string() const;

public:
    basic_pthread_mutex( bool is_recursive );
    basic_pthread_mutex( bool is_recursive, const char* name );
    basic_pthread_mutex( bool is_recursive, const std::string& name );
    basic_pthread_mutex( const basic_pthread_mutex& ) = delete;
    basic_pthread_mutex( basic_pthread_mutex&& ) = delete;
    ~basic_pthread_mutex() override;
    basic_pthread_mutex& operator=( const basic_pthread_mutex& ) = delete;
    basic_pthread_mutex& operator=( basic_pthread_mutex&& ) = delete;
    bool is_recursive_mutex() const;
    void lock() override;
    void unlock() override;
    bool try_lock() override;
};  /// class basic_pthread_mutex

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class pthread_mutex : public basic_pthread_mutex {
    volatile bool initialized_;

public:
    pthread_mutex();
    pthread_mutex( const char* name );
    pthread_mutex( const std::string& name );
    pthread_mutex( const pthread_mutex& ) = delete;
    pthread_mutex( pthread_mutex&& ) = delete;
    ~pthread_mutex() override;
    pthread_mutex& operator=( const pthread_mutex& ) = delete;
    pthread_mutex& operator=( pthread_mutex&& ) = delete;
};  /// class pthread_mutex

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class pthread_recursive_mutex : public basic_pthread_mutex {
    volatile bool initialized_;

public:
    pthread_recursive_mutex();
    pthread_recursive_mutex( const char* name );
    pthread_recursive_mutex( const std::string& name );
    pthread_recursive_mutex( const pthread_recursive_mutex& ) = delete;
    pthread_recursive_mutex( pthread_recursive_mutex&& ) = delete;
    virtual ~pthread_recursive_mutex() override;
    pthread_recursive_mutex& operator=( const pthread_recursive_mutex& ) = delete;
    pthread_recursive_mutex& operator=( pthread_recursive_mutex&& ) = delete;
};  /// class pthread_recursive_mutex

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef pthread_mutex mutex_type;  // typedef std::mutex mutex_type;
                                   //		typedef std::lock_guard  < mutex_type > lock_type;
                                   //		typedef std::unique_lock < mutex_type >
                                   // unique_lock_type;

typedef pthread_recursive_mutex recursive_mutex_type;  // typedef std::recursive_mutex
                                                       // recursive_mutex_type;
//		typedef std::lock_guard  < recursive_mutex_type > recursive_lock_type;
//		typedef std::unique_lock < recursive_mutex_type > unique_recursive_lock_type;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace multithreading

};  // namespace skutils

#endif  /// (!defined __SKUTILS_MULTITHREADING_H)
