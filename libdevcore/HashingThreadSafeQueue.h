/*
    Copyright (C) 2019-present, SKALE Labs

    This file is part of skaled.

    skaled is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skaled is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skaled.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * @file Exceptions.cpp
 * @author Dima Litvinov
 * @date 2019
 */

#pragma once

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <deque>
#include <unordered_set>

class abort_exception : public std::exception {
public:
    const char* what() const noexcept override {
        return "Wait of thread safe queue is aborted because termination request was received";
    }
};

template < class T, class F, bool USE_HASH = true >
class HashingThreadSafeQueue {
public:
    static int waitTimeout;

public:
    typedef T value_type;
    typedef std::size_t size_type;
    typedef T& reference;
    typedef const T& const_reference;

public:
    bool empty() const {
        boost::shared_lock< boost::shared_mutex > lock( mutex );
        return queue.empty();
    }

    size_type size() const {
        boost::shared_lock< boost::shared_mutex > lock( mutex );
        return queue.size();
    }

    template < class... Args >
    void emplace( Args&&... args ) {
        boost::unique_lock< boost::shared_mutex > lock( mutex );
        queue.emplace( args... );  // Crazy shit %)
        if ( USE_HASH )
            hash.insert( queue.back() );
        assert( num_waiting <= 1 );
        cond.notify_one();
    }

    void push( const value_type& val ) {
        boost::unique_lock< boost::shared_mutex > lock( mutex );
        queue.push_back( val );
        if ( USE_HASH )
            hash.insert( val );
        assert( num_waiting <= 1 );
        cond.notify_one();
    }

    void unpop( const value_type& val ) {
        boost::unique_lock< boost::shared_mutex > lock( mutex );
        queue.push_front( val );
        if ( USE_HASH )
            hash.insert( val );
        assert( num_waiting <= 1 );
        cond.notify_one();
    }

    value_type pop() {
        boost::unique_lock< boost::shared_mutex > lock( mutex );

        if ( queue.empty() )
            throw std::length_error( "Pop()int empty CutomQueue" );

        value_type val = std::move( queue.front() );  // TODO Optimize copying?
        if ( USE_HASH )
            hash.erase( val );
        queue.pop_front();

        return val;
    }

    value_type popSync() {
        boost::upgrade_lock< boost::shared_mutex > lock( this->mutex );

        if ( !this->empty() ) {              // TODO deduplicate with pop_WITH_LOCK()?
            value_type val = queue.front();  // TODO Optimize copying?
            if ( USE_HASH )
                hash.erase( val );
            queue.pop_front();

            return val;
        }

        boost::upgrade_to_unique_lock< boost::shared_mutex > upgrade( lock );

        while ( true ) {
            ++num_waiting;
            assert( num_waiting == 1 );
            cond.wait_for(
                mutex, boost::chrono::milliseconds( HashingThreadSafeQueue::waitTimeout ) );
            --num_waiting;

            if ( !queue.empty() ) {
                value_type val = queue.front();  // TODO Optimize copying?
                if ( USE_HASH )
                    hash.erase( val );
                queue.pop_front();
                return val;
            } else if ( abort_requested )
                throw abort_exception();
            else
                continue;
        }  // while

    }  // popSync()

    void abortWaiting() {
        boost::unique_lock< boost::shared_mutex > lock( mutex );
        assert( num_waiting <= 1 );
        this->abort_requested = true;
    }

    bool contains( const value_type& val ) {
        assert( USE_HASH );
        if ( !USE_HASH )
            return false;
        boost::shared_lock< boost::shared_mutex > lock( mutex );
        return hash.find( val ) != hash.end();
    }

    bool erase( const value_type& val ) {
        assert( USE_HASH );
        if ( !USE_HASH )
            return false;

        boost::upgrade_lock< boost::shared_mutex > lock( mutex );

        if ( !this->contains( val ) )
            return false;

        boost::upgrade_to_unique_lock< boost::shared_mutex > upgrade( lock );

        hash.erase( val );

        auto it =
            std::find( queue.begin(), queue.end(), val );  // TODO Some day optimize this search
        assert( it != queue.end() );
        queue.erase( it );

        return true;
    }

    const std::unordered_set< T >& known() const {
        assert( USE_HASH );
        return hash;
    }

private:
    std::deque< T > queue;
    std::unordered_set< T, F > hash;
    mutable boost::shared_mutex mutex;
    boost::condition_variable_any cond;
    bool abort_requested = false;
    int num_waiting = 0;  // for assert
};

template < class T, class F, bool USE_HASH >
int HashingThreadSafeQueue< T, F, USE_HASH >::waitTimeout = 100;
