/*
    Copyright (C) 2018-present, SKALE Labs

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
 * @file ThreadSafeQueue.h
 * @author Bogdan Bliznyuk
 * @date 2018
 */

#ifndef CPP_ETHEREUM_THREADSAFEQUEUE_H
#define CPP_ETHEREUM_THREADSAFEQUEUE_H

#include <condition_variable>
#include <mutex>
#include <queue>


template < class T >
class ThreadSafeQueue {
    std::queue< T > mQueue;
    std::mutex mMutex;
    std::condition_variable mCondition;

public:
    T pop() {
        std::unique_lock< std::mutex > mlock( mMutex );
        while ( mQueue.empty() ) {
            mCondition.wait( mlock );
        }
        auto item = mQueue.front();
        mQueue.pop();
        return item;
    }

    void push( const T& item ) {
        std::unique_lock< std::mutex > mlock( mMutex );
        mQueue.push( item );
        mlock.unlock();
        mCondition.notify_one();
    }

    size_t size() { return mQueue.size(); }
};


#endif  // CPP_ETHEREUM_THREADSAFEQUEUE_H
