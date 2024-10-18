#if ( !defined __SKUTILS_TASK_PERFORMANCE_H )
#define __SKUTILS_TASK_PERFORMANCE_H 1

#include <atomic>
#include <map>
#include <mutex>
#include <string>

#include <skutils/atomic_shared_ptr.h>
#include <skutils/multithreading.h>


#include <inttypes.h>

#include <json.hpp>

namespace skutils {
namespace task {
namespace performance {

typedef std::chrono::system_clock clock;
typedef std::chrono::time_point< clock > time_point;

typedef size_t index_type;
typedef std::atomic_size_t atomic_index_type;

typedef std::atomic_bool atomic_bool;

typedef std::string string;

typedef nlohmann::json json;

typedef skutils::multithreading::mutex_type mutex_type;
typedef skutils::multithreading::recursive_mutex_type recursive_mutex_type;

class item;
class queue;
class tracker;
class action;

typedef skutils::retain_release_ptr< item > item_ptr;
typedef skutils::retain_release_ptr< queue > queue_ptr;
typedef skutils::retain_release_ptr< tracker > tracker_ptr;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class lockable {
public:
    typedef recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;

    lockable();
    lockable( const lockable& ) = delete;
    lockable( lockable&& ) = delete;
    virtual ~lockable();
    lockable& operator=( const lockable& ) = delete;
    lockable& operator=( lockable&& ) = delete;
    mutex_type& mtx() const;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class describable {
    const string strName_;
    json jsonIn_, jsonOut_, jsonErr_;

public:
    describable( const string& strName, const json& jsnIn );
    describable( const describable& ) = delete;
    describable( describable&& ) = delete;
    virtual ~describable();
    describable& operator=( const describable& ) = delete;
    describable& operator=( describable&& ) = delete;
    const string& get_name() const;
    virtual json get_json_in() const;
    virtual json get_json_out() const;
    virtual json get_json_err() const;
    virtual void set_json_in( const json& jsn );
    virtual void set_json_out( const json& jsn );
    virtual void set_json_err( const json& jsn );
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class index_holder {
    atomic_index_type nextItemIndex_;

public:
    index_holder();
    index_holder( const index_holder& ) = delete;
    index_holder( index_holder&& ) = delete;
    virtual ~index_holder();
    index_holder& operator=( const index_holder& ) = delete;
    index_holder& operator=( index_holder&& ) = delete;
    index_type alloc_index();
    index_type get_index() const;
    void reset();
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class time_holder {
    time_point tpStart_, tpEnd_;
    atomic_bool isRunning_;

public:
    time_holder( bool isRunning );
    time_holder( const time_holder& ) = delete;
    time_holder( time_holder&& ) = delete;
    virtual ~time_holder();
    time_holder& operator=( const time_holder& ) = delete;
    time_holder& operator=( index_holder&& ) = delete;
    bool is_finished() const;
    virtual bool is_running() const;
    virtual void set_running( bool b = true );
    time_point tp_start() const;
    time_point tp_end() const;
    std::chrono::nanoseconds tp_duration() const;
    string tp_start_s( bool isUTC = true, bool isDaysInsteadOfYMD = false ) const;
    string tp_end_s( bool isUTC = true, bool isDaysInsteadOfYMD = false ) const;
    string tp_duration_s() const;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class item : public skutils::ref_retain_release, public describable, public time_holder {
    mutable queue_ptr pQueue_;
    index_type indexQ_, indexT_;

public:
    item( const string& strName, const json& jsn, queue_ptr pQueue, index_type indexQ,
        index_type indexT );
    item( const item& ) = delete;
    item( item&& ) = delete;
    virtual ~item();
    item& operator=( const item& ) = delete;
    item& operator=( item&& ) = delete;
    queue_ptr get_queue() const;
    tracker_ptr get_tracker() const;
    index_type get_index_in_queue() const;
    index_type get_index_in_tracker() const;
    json compose_json() const;
    bool is_running() const override;
    void set_running( bool b = true ) override;
    void finish();
    json get_json_in() const override;
    json get_json_out() const override;
    json get_json_err() const override;
    void set_json_in( const json& jsn ) override;
    void set_json_out( const json& jsn ) override;
    void set_json_err( const json& jsn ) override;
};


class queue : public skutils::ref_retain_release,
              public describable,
              public lockable,
              public index_holder {
    mutable tracker_ptr pTracker_;

    typedef std::map< index_type, item_ptr > map_type;
    mutable map_type map_;

public:
    queue( const string& strName, const json& jsn, tracker_ptr pTracker );
    queue( const queue& ) = delete;
    queue( queue&& ) = delete;
    virtual ~queue();
    queue& operator=( const queue& ) = delete;
    queue& operator=( queue&& ) = delete;

private:
    void reset();

public:
    tracker_ptr get_tracker() const;
    item_ptr new_item( const string& strName, const json& jsn );
    json compose_json( index_type minIndexT = 0 ) const;
};

class tracker : public skutils::ref_retain_release,
                public lockable,
                public index_holder,
                public time_holder {
    typedef std::map< string, queue_ptr > map_type;
    mutable map_type map_;

    // make sure that performance tracking is disabled by default
    atomic_bool isEnabled_ = false;
    atomic_index_type safeMaxItemCount_ = 10 * 1000 * 1000;
    atomic_index_type sessionMaxItemCount_ = 0;  // zero means use safeMaxItemCount_
    string strFirstEncounteredStopReason_;

public:
    tracker();
    tracker( const tracker& ) = delete;
    tracker( tracker&& ) = delete;
    virtual ~tracker();
    tracker& operator=( const tracker& ) = delete;
    tracker& operator=( tracker&& ) = delete;

private:
    void reset();

public:
    bool is_enabled() const;
    void set_enabled( bool b );
    size_t get_safe_max_item_count() const;
    void set_safe_max_item_count( size_t n );
    size_t get_session_max_item_count() const;
    void set_session_max_item_count( size_t n );
    bool is_running() const override;
    void set_running( bool b = true ) override;
    queue_ptr get_queue( const string& strName );
    json compose_json( index_type minIndexT = 0 ) const;
    void cancel();
    void start();
    json stop( index_type minIndexT = 0 );
    void came_accross_with_possible_session_stop_reason( const string& strPossibleStopReason );
    string get_first_encountered_stop_reason() const;
};

extern tracker_ptr get_default_tracker();


};  // namespace performance
};  // namespace task
};  // namespace skutils

#endif  /// (!defined __SKUTILS_TASK_PERFORMANCE_H)
