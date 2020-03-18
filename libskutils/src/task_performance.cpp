#include <skutils/console_colors.h>
#include <skutils/task_performance.h>
#include <skutils/utils.h>

#include <exception>

namespace skutils {
namespace task {
namespace performance {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

lockable::lockable() {}

lockable::~lockable() {}

lockable::mutex_type& lockable::mtx() const {
    return mtx_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

describable::describable( const string& strName, const json& jsn )
    : strName_( strName ), json_( jsn ) {
    if ( strName_.empty() )
        throw std::runtime_error(
            "Attempt to instatiate performance describable instance without name provided" );
}

describable::~describable() {}

const string& describable::get_name() const {
    return strName_;
}

const json& describable::get_json() const {
    return json_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

index_holder::index_holder() : nextItemIndex_( 0 ) {}

index_holder::~index_holder() {}

index_type index_holder::alloc_index() {
    index_type n = nextItemIndex_++;
    return n;
}


index_type index_holder::get_index() const {
    index_type n = nextItemIndex_;
    return n;
}

void index_holder::reset() {
    nextItemIndex_ = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

item::item(
    const string& strName, const json& jsn, queue_ptr pQueue, index_type indexQ, index_type indexT )
    : describable( strName, jsn ),
      pQueue_( pQueue ),
      indexQ_( indexQ ),
      indexT_( indexT ),
      tpStart_( clock::now() ),
      isFinished_( false ) {
    if ( !pQueue_ )
        throw std::runtime_error( "Attempt to instatiate performance item without queue provided" );
}

item::~item() {}

queue_ptr item::get_queue() const {
    return pQueue_;
}

tracker_ptr item::get_tracker() const {
    return get_queue()->get_tracker();
}

index_type item::get_index_in_queue() const {
    return indexQ_;
}
index_type item::get_index_in_tracker() const {
    return indexT_;
}

json item::compose_json() const {
    json jsn = json::object();
    jsn["name"] = get_name();
    jsn["iq"] = get_index_in_queue();
    jsn["it"] = get_index_in_tracker();
    jsn["jsn"] = get_json();
    jsn["fin"] = is_funished();
    jsn["tsStart"] = tp_start_s();
    jsn["tsEnd"] = tp_end_s();
    return jsn;
}

void item::finish() {
    if ( isFinished_ )
        return;
    isFinished_ = true;
    tpEnd_ = clock::now();
}

bool item::is_funished() const {
    bool b = isFinished_;
    return b;
}

time_point item::tp_start() const {
    return tpStart_;
}

time_point item::tp_end() const {
    return isFinished_ ? tpEnd_ : clock::now();
}

string item::tp_start_s( bool isUTC, bool isDaysInsteadOfYMD ) const {
    string s = cc::time2string( tp_start(), isUTC, isDaysInsteadOfYMD, false );
    return s;
}

string item::tp_end_s( bool isUTC, bool isDaysInsteadOfYMD ) const {
    string s = cc::time2string( tp_end(), isUTC, isDaysInsteadOfYMD, false );
    return s;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

queue::queue( const string& strName, const json& jsn, tracker_ptr pTracker )
    : describable( strName, jsn ), pTracker_( pTracker ? pTracker : get_default_tracker() ) {
    if ( !pTracker_ )
        throw std::runtime_error(
            "Attempt to instatiate performance queue without tracker provided" );
}

queue::~queue() {
    reset();
}

void queue::reset() {
    lockable::lock_type lock( mtx() );
    map_.clear();
    index_holder::reset();
}

tracker_ptr queue::get_tracker() const {
    return pTracker_;
}

item_ptr queue::new_item( const string& strName, const json& jsn ) {
    item_ptr pItem;
    {  // block
        lockable::lock_type lock( mtx() );
        queue_ptr pThisQueue = this;
        index_type indexQ = alloc_index();
        index_type indexT = get_tracker()->alloc_index();
        pItem = item_ptr::make( strName, jsn, pThisQueue, indexQ, indexT );
        map_[indexQ] = pItem;
    }  // block
    return pItem;
}

json queue::compose_json( index_type minIndexT ) const {
    json jsn = json::array();
    {  // block
        lockable::lock_type lock( mtx() );
        map_type::iterator itWalk = map_.begin(), itEnd = map_.end();
        for ( ; itWalk != itEnd; ++itWalk ) {
            // size_t indexQ = itWalk->first;
            item_ptr pItem = itWalk->second;
            if ( pItem->get_index_in_tracker() < minIndexT )
                continue;
            jsn.push_back( pItem->compose_json() );
        }
    }  // block
    return jsn;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

tracker_ptr get_default_tracker() {
    static tracker_ptr g_pDefaultTracker = tracker_ptr::make();
    return g_pDefaultTracker;
}

tracker::tracker() : enabled_( false ) {}

tracker::~tracker() {
    set_enabled( false );
    reset();
}

void tracker::reset() {
    lockable::lock_type lock( mtx() );
    map_.clear();
    index_holder::reset();
}

size_t tracker::get_max_item_count() const {
    size_t n = maxItemCount_;
    return n;
}

void tracker::set_max_item_count( size_t n ) {
    maxItemCount_ = n;
}

bool tracker::is_enabled() const {
    bool b = enabled_;
    return b;
}

void tracker::set_enabled( bool b ) {
    enabled_ = b;
    if ( !enabled_ )
        reset();
}

queue_ptr tracker::get_queue( const string& strName ) {
    queue_ptr pQueue;
    {  // block
        lockable::lock_type lock( mtx() );
        tracker_ptr pThisTracker = this;
        map_type::iterator itFind = map_.find( strName ), itEnd = map_.end();
        if ( itFind != itEnd )
            pQueue = itFind->second;
        else {
            pQueue = queue_ptr::make( strName, json::object(), pThisTracker );
            map_[strName] = pQueue;
        }
    }  // block
    return pQueue;
}

json tracker::compose_json( index_type minIndexT ) const {
    json jsn = json::object();
    json jsnQueues = json::object();
    size_t idxFetchPointNextTime = 0;
    {  // block
        lockable::lock_type lock( mtx() );
        idxFetchPointNextTime = get_index();
        map_type::iterator itWalk = map_.begin(), itEnd = map_.end();
        for ( ; itWalk != itEnd; ++itWalk ) {
            string strName = itWalk->first;
            queue_ptr pQueue = itWalk->second;
            jsnQueues[strName] = pQueue->compose_json( minIndexT );
        }
    }  // block
    jsn["queues"] = jsnQueues;
    jsn["nextTimeFetchIndex"] = idxFetchPointNextTime;
    return jsn;
}

void tracker::cancel() {
    if ( !is_enabled() )
        return;
    lockable::lock_type lock( mtx() );
    set_enabled( false );
    reset();
}

void tracker::start() {
    set_enabled( true );
}

json tracker::stop( index_type minIndexT ) {
    json jsn = json::object();
    {  // block
        lockable::lock_type lock( mtx() );
        jsn = compose_json( minIndexT );
        set_enabled( false );
        reset();
    }  // block
    return jsn;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

action::action( const string& strQueueName, const string& strActionName, const json& jsnAction,
    tracker_ptr pTracker ) {
    init( strQueueName, strActionName, jsnAction, pTracker );
}

action::action( const string& strQueueName, const string& strActionName, const json& jsnAction ) {
    init( strQueueName, strActionName, jsnAction, get_default_tracker() );
}

action::action( const string& strQueueName, const string& strActionName ) {
    init( strQueueName, strActionName, json::object(), get_default_tracker() );
}

action::~action() {
    finish();
}

void action::init( const string& strQueueName, const string& strActionName, const json& jsnAction,
    tracker_ptr pTracker ) {
    isSkipped_ = false;
    if ( !pTracker ) {
        pTracker = get_default_tracker();
        if ( !pTracker )
            throw std::runtime_error(
                "Attempt to instatiate performance action without tracker provided" );
    }
    if ( !pTracker->is_enabled() ) {
        isSkipped_ = true;
        return;
    }
    if ( pTracker->get_index() >= pTracker->get_max_item_count() ) {
        isSkipped_ = true;
        return;
    }
    queue_ptr pQueue = pTracker->get_queue( strQueueName );
    pItem_ = pQueue->new_item( strActionName, jsnAction );
}


item_ptr action::get_item() const {
    if ( isSkipped_ )
        throw std::runtime_error( "Attempt to access performance task acion in skipped state" );
    return pItem_;
}

queue_ptr action::get_queue() const {
    return get_item()->get_queue();
}

tracker_ptr action::get_tracker() const {
    return get_queue()->get_tracker();
}

void action::finish() {
    if ( isSkipped_ )
        return;
    get_item()->finish();
}

bool action::is_skipped() const {
    return isSkipped_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace performance
};  // namespace task
};  // namespace skutils
