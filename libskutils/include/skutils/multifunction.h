#if ( !defined __SKUTILS_MULTIFUNCTION_H )
#define __SKUTILS_MULTIFUNCTION_H 1

#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <string>

namespace skutils {

namespace multifunction_util {

typedef std::string token_t;

template < typename R, typename... Args >
struct multifunction_traits {
    typedef R return_type;
};
template < typename R, typename Arg >
struct multifunction_traits< R, Arg > {
    typedef R return_type;
    typedef Arg argument_type;
};
template < typename R, typename Arg1, typename Arg2 >
struct multifunction_traits< R, Arg1, Arg2 > {
    typedef R return_type;
    typedef Arg1 first_argument_type;
    typedef Arg2 second_argument_type;
};

template < typename L, typename R, typename... Args >
struct invoker {
    static R invoke_all( bool ignore_exceptions, bool return_on_first_false,
        bool return_on_first_true, L const& lst, Args... args ) {
        R ret;
        for ( auto item : lst ) {
            try {
                ret = item.second( args... );
                if ( return_on_first_false && ( !ret ) )
                    return ret;
                if ( return_on_first_true && ret )
                    return ret;
            } catch ( ... ) {
                if ( !ignore_exceptions )
                    throw;
            }
        }
        return ret;
    }
};
template < typename L, typename... Args >
struct invoker< L, void, Args... > {
    static void invoke_all( bool ignore_exceptions,
        bool,  // return_on_first_false
        bool,  // return_on_first_true
        L const& lst, Args... args ) {
        for ( auto item : lst ) {
            try {
                item.second( args... );
            } catch ( ... ) {
                if ( !ignore_exceptions )
                    throw;
            }
        }
    }
};

};  // namespace multifunction_util

static inline multifunction_util::token_t multifunction_token_namer(
    void* obj, const char* desc = nullptr ) {
    multifunction_util::token_t t;
    if ( !obj ) {
        char tmp[sizeof( void* ) * sizeof( char ) * 10];
        ::sprintf( tmp, "%p", obj );
        t += tmp;
        if ( desc && desc[0] != '\0' ) {
            t += " - ";
            t += desc;
        }
    } else if ( desc && desc[0] != '\0' )
        t = desc;
    return t;
}

template < typename R, typename... Args >
class multifunction;

template < typename R, typename... Args >
class multifunction< R( Args... ) >
    : public multifunction_util::multifunction_traits< R, Args... > {
public:
    typedef multifunction_util::token_t token_t;
    typedef std::function< R( Args... ) > function_t;
    typedef std::pair< token_t, function_t > list_item_t;
    typedef std::list< list_item_t > list_t;
    typedef std::map< token_t, function_t > map_t;
    typedef list_item_t value_type;
    typedef list_item_t& reference;
    typedef const list_item_t& const_reference;
    typedef typename list_t::iterator iterator;
    typedef typename list_t::const_iterator const_iterator;
    typedef typename list_t::reverse_iterator reverse_iterator;
    typedef typename list_t::const_reverse_iterator const_reverse_iterator;

private:
    list_t list_;
    map_t map_;

public:
    bool ignore_exceptions_ : 1, return_on_first_false_ : 1, return_on_first_true_ : 1;

    template < typename F >
    F& get( const token_t& t ) {
        return map_[t];
    }
    template < typename F >
    F& operator[]( const token_t& t ) {
        return get( t );
    }

    bool contains( const token_t& t ) const {
        bool b = ( map_.find( t ) != map_.end() ) ? true : false;
        return b;
    }
    bool have( const token_t& t ) const { return contains( t ); }

    template < typename F >
    token_t push_back( const token_t& t, F fn ) {
        if ( contains( t ) )
            return t;
        list_.push_back( list_item_t( t, fn ) );
        map_[t] = fn;
        return t;
    }
    template < typename F >
    token_t push_back( F fn ) {
        token_t t;
        return push_back( t, fn );
    }
    token_t push_back( const_reference item ) { return push_back( item.first, item.second ); }
    template < typename F >
    token_t operator+=( F fn ) {
        return push_back( fn );
    }
    token_t operator+=( const_reference item ) { return push_back( item ); }

    template < typename F >
    token_t push_front( const token_t& t, F fn ) {
        if ( contains( t ) )
            return t;
        list_.push_front( list_item_t( t, fn ) );
        map_[t] = fn;
        return t;
    }
    template < typename F >
    token_t push_front( F fn ) {
        token_t t;
        return push_front( t, fn );
    }
    token_t push_front( const_reference item ) { return push_front( item.first, item.second ); }

    bool erase( const token_t& t ) {
        if ( !contains( t ) )
            return false;
        map_.erase( t );
        auto itWalk = list_.begin(), itEnd = list_.end();
        for ( ; itWalk != itEnd; ++itWalk ) {
            if ( itWalk->first == t ) {
                list_.erase( itWalk );
                return true;
            }
        }
        return false;
    }
    bool operator-=( const token_t& t ) { return erase( t ); }
    bool erase( const_reference item ) { return erase( item.first ); }
    bool operator-=( const_reference item ) { return erase( item ); }
    bool erase( iterator it ) { return ( it == end() ) ? false : erase( token_t( it->first ) ); }
    bool operator-=( iterator it ) { return erase( it ); }

    R invoke_all( Args... args ) const {
        return multifunction_util::invoker< list_t, R, Args... >::invoke_all(
            ignore_exceptions_, return_on_first_false_, return_on_first_true_, list_, args... );
    }
    R operator()( Args... args ) const { return invoke_all( args... ); }

    void clear() {
        list_.clear();
        map_.clear();
    }
    bool empty() const { return list_.empty(); }
    operator bool() const { return !empty(); }
    bool operator!() const { return empty(); }
    size_t size() const {
        size_t cnt = list_.size();
        return cnt;
    }

    multifunction( bool ignore_exceptions = false, bool return_on_first_false = false,
        bool return_on_first_true = false )
        : ignore_exceptions_( ignore_exceptions ),
          return_on_first_false_( return_on_first_false ),
          return_on_first_true_( return_on_first_true ) {}
    multifunction( multifunction const& ) = default;
    multifunction( multifunction&& ) = default;
    multifunction( const_reference item ) { push_back( item ); }
    multifunction( const token_t& t, function_t fn ) { push_back( t, fn ); }
    multifunction( function_t fn ) { push_back( fn ); }
    multifunction& operator=( multifunction const& ) = default;
    multifunction& operator=( multifunction&& ) = default;
    multifunction& operator=( const_reference item ) {
        clear();
        push_back( item );
        return ( *this );
    }
    multifunction& operator=( function_t fn ) {
        clear();
        push_back( fn );
        return ( *this );
    }
    ~multifunction() = default;

    bool ignore_exceptions() const { return ignore_exceptions_; }
    multifunction& ignore_exceptions( bool b ) {
        ignore_exceptions_ = b;
        return ( *this );
    }
    bool return_on_first_false() const { return return_on_first_false_; }
    multifunction& return_on_first_false( bool b ) {
        return_on_first_false_ = b;
        return ( *this );
    }
    bool return_on_first_true() const { return return_on_first_true_; }
    multifunction& return_on_first_true( bool b ) {
        return_on_first_true_ = b;
        return ( *this );
    }

    void reset() { clear(); }
    void reset( const_reference item ) {
        clear();
        push_back( item );
    }
    void reset( const token_t& t, function_t fn ) {
        clear();
        push_back( t, fn );
    }
    void reset( function_t fn ) {
        clear();
        push_back( fn );
    }
    void reset( multifunction const& other ) { ( *this ) = other; }

    reference front() { return list_.front(); }
    reference back() { return list_.back(); }
    iterator begin() { return list_.begin(); }
    iterator end() { return list_.end(); }
    const_iterator cbegin() const { return list_.cbegin(); }
    const_iterator cend() const { return list_.cend(); }
    const_iterator begin() const { return cbegin(); }
    const_iterator end() const { return cend(); }
    reverse_iterator rbegin() { return list_.rbegin(); }
    reverse_iterator rend() { return list_.rend(); }
    const_reverse_iterator crbegin() const { return list_.crbegin(); }
    const_reverse_iterator crend() const { return list_.crend(); }
    const_reverse_iterator rbegin() const { return crbegin(); }
    const_reverse_iterator rend() const { return crend(); }
    iterator find( const token_t& t ) {
        iterator itWalk = begin(), itEnd = end();
        for ( ; itWalk != itEnd; ++itWalk )
            if ( itWalk->first == t )
                return itWalk;
        return itEnd;
    }
    const_iterator find( const token_t& t ) const {
        const_iterator itWalk = cbegin(), itEnd = cend();
        for ( ; itWalk != itEnd; ++itWalk )
            if ( itWalk->first == t )
                return t;
        return itEnd;
    }
    iterator find( const_reference item ) { return find( item.first ); }
    const_iterator find( const_reference item ) const { return find( item.first ); }
    reverse_iterator rfind( const token_t& t ) {
        reverse_iterator itWalk = rbegin(), itEnd = rend();
        for ( ; itWalk != itEnd; ++itWalk )
            if ( itWalk->first == t )
                return itWalk;
        return itEnd;
    }
    const_reverse_iterator rfind( const token_t& t ) const {
        const_reverse_iterator itWalk = crbegin(), itEnd = crend();
        for ( ; itWalk != itEnd; ++itWalk )
            if ( itWalk->first == t )
                return itWalk;
        return itEnd;
    }
    reverse_iterator rfind( const_reference item ) { return rfind( item.first ); }
    const_reverse_iterator rfind( const_reference item ) const { return rfind( item.first ); }
    reference at( size_t i ) { return ( *( std::advance( begin(), i ) ) ); }
    const_reference at( size_t i ) const { return ( *( std::advance( cbegin(), i ) ) ); }
    reference operator[]( size_t i ) { return at( i ); }
    const_reference operator[]( size_t i ) const { return at( i ); }
    reference at( const token_t& t ) { return ( *find( t ) ); }
    const_reference at( const token_t& t ) const { return ( *find( t ) ); }
    reference operator[]( const token_t& t ) { return at( t ); }
    const_reference operator[]( const token_t& t ) const { return at( t ); }
};  // template class multifunction

};  // namespace skutils

#endif  /// (!defined __SKUTILS_MULTIFUNCTION_H)
