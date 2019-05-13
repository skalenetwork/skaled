/*
    Modifications Copyright (C) 2018-2019 SKALE Labs

    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file TransactionQueue.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "TransactionQueue.h"

#include "Transaction.h"
#include <libdevcore/Log.h>
#include <libethcore/Exceptions.h>

#include <list>
#include <thread>
#include <vector>

using namespace std;
using namespace dev;
using namespace dev::eth;

const size_t c_maxVerificationQueueSize = 8192;

TransactionQueue::TransactionQueue( unsigned _limit, unsigned _futureLimit )
    : m_current( PriorityCompare{*this} ),
      m_limit( _limit ),
      m_futureLimit( _futureLimit ),
      m_aborting( false ) {
    m_readyCondNotifier = this->onReady( [this]() {
        this->m_cond.notify_all();
        return;
    } );

    unsigned verifierThreads = 0;  // std::max( thread::hardware_concurrency(), 3U ) - 2U;
    for ( unsigned i = 0; i < verifierThreads; ++i )
        m_verifiers.emplace_back( [this, i]() {
            setThreadName( "txcheck" + toString( i ) );
            this->verifierBody();
        } );
}

TransactionQueue::~TransactionQueue() {
    HandleDestruction();
}

void TransactionQueue::HandleDestruction() {
    std::list< std::thread > listAwait;
    {
        DEV_GUARDED( x_queue ) {
            m_aborting = true;
            m_queueReady.notify_all();
            for ( auto& i : m_verifiers ) {
                try {
                    if ( i.joinable() )
                        listAwait.push_back( std::move( i ) );
                } catch ( ... ) {
                }
            }
            m_verifiers.clear();
        }
    }
    for ( auto& i : listAwait ) {
        try {
            if ( i.joinable() )
                i.join();
        } catch ( ... ) {
        }
    }
}

ImportResult TransactionQueue::import( bytesConstRef _transactionRLP, IfDropped _ik ) {
    try {
        Transaction t = Transaction( _transactionRLP, CheckTransaction::Everything );
        return import( t, _ik );
    } catch ( Exception const& ) {
        return ImportResult::Malformed;
    }
}

ImportResult TransactionQueue::check_WITH_LOCK( h256 const& _h, IfDropped _ik ) {
    if ( m_known.count( _h ) )
        return ImportResult::AlreadyKnown;

    if ( m_dropped.count( _h ) && _ik == IfDropped::Ignore )
        return ImportResult::AlreadyInChain;

    return ImportResult::Success;
}

ImportResult TransactionQueue::import( Transaction const& _transaction, IfDropped _ik ) {
    if ( _transaction.hasZeroSignature() )
        return ImportResult::ZeroSignature;
    // Check if we already know this transaction.
    h256 h = _transaction.sha3( WithSignature );

    ImportResult ret;
    {
        MICROPROFILE_SCOPEI( "TransactionQueue", "import", MP_THISTLE );
        UpgradableGuard l( m_lock );
        auto ir = check_WITH_LOCK( h, _ik );
        if ( ir != ImportResult::Success )
            return ir;

        {
            _transaction.safeSender();  // Perform EC recovery outside of the write lock
            UpgradeGuard ul( l );
            ret = manageImport_WITH_LOCK( h, _transaction );
        }
    }
    return ret;
}

Transactions TransactionQueue::topTransactions( unsigned _limit, h256Hash const& _avoid ) const {
    return topTransactions(
        _limit, [&]( const Transaction& t ) -> bool { return _avoid.count( t.sha3() ) == 0; } );
}

Transactions TransactionQueue::topTransactions_WITH_LOCK(
    unsigned _limit, h256Hash const& _avoid ) const {
    return topTransactions_WITH_LOCK(
        _limit, [&]( const Transaction& t ) -> bool { return _avoid.count( t.sha3() ) == 0; } );
}

Transactions TransactionQueue::topTransactions(
    unsigned _limit, int _maxCategory, int _setCategory ) {
    ReadGuard l( m_lock );
    return topTransactions_WITH_LOCK( _limit, _maxCategory, _setCategory );
}

Transactions TransactionQueue::topTransactions_WITH_LOCK(
    unsigned _limit, int _maxCategory, int _setCategory ) {
    MICROPROFILE_SCOPEI( "TransactionQueue", "topTransactions_WITH_LOCK_cat", MP_PAPAYAWHIP );

    Transactions topTransactions;
    std::vector< PriorityQueue::node_type > found;

    VerifiedTransaction dummy = VerifiedTransaction( Transaction() );
    dummy.category = _maxCategory;

    PriorityQueue::iterator my_begin = m_current.lower_bound( dummy );

    for ( PriorityQueue::iterator transaction_ptr = my_begin;
          topTransactions.size() < _limit && transaction_ptr != m_current.end();
          ++transaction_ptr ) {
        topTransactions.push_back( transaction_ptr->transaction );
        if ( _setCategory >= 0 ) {
            found.push_back( m_current.extract( transaction_ptr ) );
        }
    }

    // set all at once
    if ( _setCategory >= 0 ) {
        for ( PriorityQueue::node_type& queueNode : found ) {
            queueNode.value().category = _setCategory;
            m_current.insert( std::move( queueNode ) );
        }
    }

    return topTransactions;
}

const h256Hash& TransactionQueue::knownTransactions() const {
    ReadGuard l( m_lock );
    return m_known;
}

ImportResult TransactionQueue::manageImport_WITH_LOCK(
    h256 const& _h, Transaction const& _transaction ) {
    try {
        assert( _h == _transaction.sha3() );
        // Remove any prior transaction with the same nonce but a lower gas price.
        // Bomb out if there's a prior transaction with higher gas price.
        auto cs = m_currentByAddressAndNonce.find( _transaction.from() );
        if ( cs != m_currentByAddressAndNonce.end() ) {
            auto t = cs->second.find( _transaction.nonce() );
            if ( t != cs->second.end() ) {
                return ImportResult::SameNonceAlreadyInQueue;
            }
        }
        auto fs = m_future.find( _transaction.from() );
        if ( fs != m_future.end() ) {
            auto t = fs->second.find( _transaction.nonce() );
            if ( t != fs->second.end() ) {
                return ImportResult::SameNonceAlreadyInQueue;
            }
        }
        // If valid, append to transactions.
        insertCurrent_WITH_LOCK( make_pair( _h, _transaction ) );
        LOG( m_loggerDetail ) << "Queued vaguely legit-looking transaction " << _h;

        while ( m_current.size() > m_limit ) {
            LOG( m_loggerDetail ) << "Dropping out of bounds transaction " << _h;
            remove_WITH_LOCK( m_current.rbegin()->transaction.sha3() );
        }

        m_onReady();
    } catch ( Exception const& _e ) {
        LOG( m_loggerDetail ) << "Ignoring invalid transaction: " << diagnostic_information( _e );
        return ImportResult::Malformed;
    } catch ( std::exception const& _e ) {
        LOG( m_loggerDetail ) << "Ignoring invalid transaction: " << _e.what();
        return ImportResult::Malformed;
    }

    return ImportResult::Success;
}

u256 TransactionQueue::maxNonce( Address const& _a ) const {
    ReadGuard l( m_lock );
    return maxNonce_WITH_LOCK( _a );
}

u256 TransactionQueue::maxNonce_WITH_LOCK( Address const& _a ) const {
    u256 ret = 0;
    auto cs = m_currentByAddressAndNonce.find( _a );
    if ( cs != m_currentByAddressAndNonce.end() && !cs->second.empty() )
        ret = cs->second.rbegin()->first + 1;
    auto fs = m_future.find( _a );
    if ( fs != m_future.end() && !fs->second.empty() )
        ret = std::max( ret, fs->second.rbegin()->first + 1 );
    return ret;
}

void TransactionQueue::insertCurrent_WITH_LOCK( std::pair< h256, Transaction > const& _p ) {
    if ( m_currentByHash.count( _p.first ) ) {
        cwarn << "Transaction hash" << _p.first << "already in current?!";
        return;
    }

    Transaction const& t = _p.second;
    // Insert into current
    auto inserted = m_currentByAddressAndNonce[t.from()].insert(
        std::make_pair( t.nonce(), PriorityQueue::iterator() ) );
    PriorityQueue::iterator handle = m_current.emplace( VerifiedTransaction( t ) );
    inserted.first->second = handle;
    m_currentByHash[_p.first] = handle;

    // Move following transactions from future to current
    makeCurrent_WITH_LOCK( t );
    m_known.insert( _p.first );
}

bool TransactionQueue::remove_WITH_LOCK( h256 const& _txHash ) {
    MICROPROFILE_SCOPEI( "TransactionQueue", "remove_WITH_LOCK", MP_LIGHTGOLDENRODYELLOW );

    auto t = m_currentByHash.find( _txHash );
    if ( t == m_currentByHash.end() )
        return false;

    Address from = ( *t->second ).transaction.from();
    auto it = m_currentByAddressAndNonce.find( from );
    assert( it != m_currentByAddressAndNonce.end() );
    it->second.erase( ( *t->second ).transaction.nonce() );
    m_current.erase( t->second );
    m_currentByHash.erase( t );
    if ( it->second.empty() )
        m_currentByAddressAndNonce.erase( it );
    m_known.erase( _txHash );
    return true;
}

unsigned TransactionQueue::waiting( Address const& _a ) const {
    ReadGuard l( m_lock );
    unsigned ret = 0;
    auto cs = m_currentByAddressAndNonce.find( _a );
    if ( cs != m_currentByAddressAndNonce.end() )
        ret = cs->second.size();
    auto fs = m_future.find( _a );
    if ( fs != m_future.end() )
        ret += fs->second.size();
    return ret;
}

void TransactionQueue::setFuture( h256 const& _txHash ) {
    WriteGuard l( m_lock );
    auto it = m_currentByHash.find( _txHash );
    if ( it == m_currentByHash.end() )
        return;

    VerifiedTransaction const& st = *( it->second );

    Address from = st.transaction.from();
    auto& queue = m_currentByAddressAndNonce[from];
    auto& target = m_future[from];
    auto cutoff = queue.lower_bound( st.transaction.nonce() );
    for ( auto m = cutoff; m != queue.end(); ++m ) {
        VerifiedTransaction& t = const_cast< VerifiedTransaction& >(
            *( m->second ) );  // set has only const iterators. Since we are moving out of container
                               // that's fine
        m_currentByHash.erase( t.transaction.sha3() );
        target.emplace( t.transaction.nonce(), move( t ) );
        m_current.erase( m->second );
        ++m_futureSize;
    }
    queue.erase( cutoff, queue.end() );
    if ( queue.empty() )
        m_currentByAddressAndNonce.erase( from );
}

void TransactionQueue::makeCurrent_WITH_LOCK( Transaction const& _t ) {
    MICROPROFILE_SCOPEI( "TransactionQueue", "makeCurrent_WITH_LOCK", MP_DEEPSKYBLUE );

    bool newCurrent = false;
    auto fs = m_future.find( _t.from() );
    if ( fs != m_future.end() ) {
        u256 nonce = _t.nonce() + 1;
        auto fb = fs->second.find( nonce );
        if ( fb != fs->second.end() ) {
            auto ft = fb;
            while ( ft != fs->second.end() && ft->second.transaction.nonce() == nonce ) {
                auto inserted = m_currentByAddressAndNonce[_t.from()].insert(
                    std::make_pair( ft->second.transaction.nonce(), PriorityQueue::iterator() ) );
                PriorityQueue::iterator handle = m_current.emplace( move( ft->second ) );
                inserted.first->second = handle;
                m_currentByHash[( *handle ).transaction.sha3()] = handle;
                --m_futureSize;
                ++ft;
                ++nonce;
                newCurrent = true;
            }
            fs->second.erase( fb, ft );
            if ( fs->second.empty() )
                m_future.erase( _t.from() );
        }
    }

    while ( m_futureSize > m_futureLimit ) {
        // TODO: priority queue for future transactions
        // For now just drop random chain end
        --m_futureSize;
        LOG( m_loggerDetail ) << "Dropping out of bounds future transaction "
                              << m_future.begin()->second.rbegin()->second.transaction.sha3();
        m_future.begin()->second.erase( --m_future.begin()->second.end() );
        if ( m_future.begin()->second.empty() )
            m_future.erase( m_future.begin() );
    }

    if ( newCurrent )
        m_onReady();
}

void TransactionQueue::drop( h256 const& _txHash ) {
    UpgradableGuard l( m_lock );

    if ( !m_known.count( _txHash ) )
        return;

    UpgradeGuard ul( l );
    m_dropped.insert( _txHash );
    remove_WITH_LOCK( _txHash );
}

void TransactionQueue::dropGood( Transaction const& _t ) {
    MICROPROFILE_SCOPEI( "TransactionQueue", "dropGood", MP_CORNSILK );
    MICROPROFILE_ENTERI( "TransactionQueue", "lock", MP_OLDLACE );
    WriteGuard l( m_lock );
    MICROPROFILE_LEAVE();

    if ( !_t.isInvalid() )
        makeCurrent_WITH_LOCK( _t );

    if ( !m_known.count( _t.sha3() ) )
        return;

    remove_WITH_LOCK( _t.sha3() );
}

void TransactionQueue::clear() {
    WriteGuard l( m_lock );
    m_known.clear();
    m_current.clear();
    m_dropped.clear();
    m_currentByAddressAndNonce.clear();
    m_currentByHash.clear();
    m_future.clear();
    m_futureSize = 0;
}

void TransactionQueue::enqueue( RLP const& _data, h512 const& _nodeId ) {
    bool queued = false;
    {
        Guard l( x_queue );
        unsigned itemCount = _data.itemCount();
        for ( unsigned i = 0; i < itemCount; ++i ) {
            if ( m_unverified.size() >= c_maxVerificationQueueSize ) {
                LOG( m_logger ) << "Transaction verification queue is full. Dropping "
                                << itemCount - i << " transactions";
                break;
            }
            m_unverified.emplace_back( UnverifiedTransaction( _data[i].data(), _nodeId ) );
            queued = true;
        }
    }
    if ( queued )
        m_queueReady.notify_all();
}

void TransactionQueue::verifierBody() {
    while ( !m_aborting ) {
        UnverifiedTransaction work;

        {  // block
            MICROPROFILE_SCOPEI( "TransactionQueue", "unique_lock<Mutex> l(x_queue)", MP_DIMGRAY );
            unique_lock< Mutex > l( x_queue );
            {
                MICROPROFILE_SCOPEI( "TransactionQueue", "m_queueReady.wait", MP_DIMGRAY );
                m_queueReady.wait(
                    l, [&]() { return bool( m_aborting ) || ( !m_unverified.empty() ); } );
            }
            if ( m_aborting )
                return;
            MICROPROFILE_ENTERI(
                "TransactionQueue", "verifierBody while", MP_LIGHTGOLDENRODYELLOW );
            work = move( m_unverified.front() );
            m_unverified.pop_front();
        }  // block

        try {
            Transaction t( work.transaction, CheckTransaction::Cheap );  // Signature will be
                                                                         // checked later
            ImportResult ir = import( t );
            m_onImport( ir, t.sha3(), work.nodeId );
        } catch ( ... ) {
            // should not happen as exceptions are handled in import.
            cwarn << "Bad transaction:" << boost::current_exception_diagnostic_information();
        }
        MICROPROFILE_LEAVE();
    }
}
