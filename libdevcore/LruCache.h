// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#pragma once

#include <any>
#include <list>
#include <unordered_map>

namespace dev {
template < class Key, class Value, class Hash = std::hash< Key > >
class LruCache {
    using key_type = Key;
    using value_type = Value;
    using list_type = std::list< std::pair< key_type, value_type > >;
    using map_type = std::unordered_map< key_type, typename list_type::iterator, Hash >;

public:
    explicit LruCache( size_t _capacity ) : m_capacity( _capacity ) {}

    size_t insert( key_type const& _key, value_type const& _val ) {
        auto iter = m_index.find( _key );
        if ( iter == m_index.end() ) {
            if ( m_index.size() == m_capacity ) {
                m_index.erase( m_data.back().first );
                m_data.pop_back();
            }
            m_data.push_front( { _key, _val } );
            m_index[_key] = m_data.begin();
        } else
            m_data.splice( m_data.begin(), m_data, iter->second );

        return m_index.size();
    }

    size_t insertOrUpdate( key_type const& _key, value_type const& _val ) {
        auto iter = m_index.find( _key );
        if ( iter == m_index.end() ) {
            if ( m_index.size() == m_capacity ) {
                m_index.erase( m_data.back().first );
                m_data.pop_back();
            }
            m_data.push_front( { _key, _val } );
            m_index[_key] = m_data.begin();
        } else {
            iter->second->second = _val;
            m_data.splice( m_data.begin(), m_data, iter->second );
        }

        return m_index.size();
    }

    std::any get( key_type const& _key ) {
        auto iter = m_index.find( _key );
        if ( iter != m_index.end() ) {
            m_data.splice( m_data.begin(), m_data, iter->second );
            return iter->second->second;
        }
        return std::any();
    }

    size_t remove( key_type const& _key ) {
        auto iter = m_index.find( _key );
        if ( iter != m_index.end() ) {
            m_data.erase( iter->second );
            m_index.erase( iter );
        }

        return m_index.size();
    }

    bool touch( key_type const& _key ) {
        auto iter = m_index.find( _key );
        if ( iter != m_index.end() ) {
            m_data.splice( m_data.begin(), m_data, iter->second );
            return true;
        }
        return false;
    }

    bool contains( key_type const& _key ) const { return m_index.find( _key ) != m_index.end(); }

    bool contains( key_type const& _key, value_type const& _value ) const {
        auto iter = m_index.find( _key );
        return iter != m_index.end() && ( *( iter->second ) ).second == _value;
    }

    bool empty() const noexcept { return m_index.empty(); }

    size_t size() const noexcept { return m_index.size(); }

    size_t capacity() const noexcept { return m_capacity; }

    void clear() noexcept {
        m_index.clear();
        m_data.clear();
    }

    // Expose data iterator for testing purposes
    typename list_type::const_iterator cbegin() const noexcept { return m_data.cbegin(); }
    typename list_type::iterator begin() noexcept { return m_data.begin(); }
    typename list_type::const_iterator cend() const noexcept { return m_data.cend(); }
    typename list_type::iterator end() noexcept { return m_data.end(); }

private:
    list_type m_data;
    map_type m_index;
    size_t m_capacity;
};
}  // namespace dev
