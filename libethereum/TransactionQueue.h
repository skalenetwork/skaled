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
/** @file TransactionQueue.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include "Transaction.h"

#include <libdevcore/microprofile.h>

#include <boost/container/set.hpp>

#include <libdevcore/Common.h>
#include <libdevcore/Guards.h>
#include <libdevcore/Log.h>
#include <libdevcore/LruCache.h>
#include <libethcore/Common.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace dev {
namespace eth {

/**
 * @brief A queue of Transactions, each stored as RLP.
 * Maintains a transaction queue sorted by nonce diff and gas price.
 * @threadsafe
 */
class TransactionQueue {
public:
    struct Limits {
        size_t currentLimit;
        size_t futureLimit;
        size_t currentLimitBytes = 12322916;
        size_t futureLimitBytes = 24645833;
    };

    /// @brief TransactionQueue
    /// @param _limit Maximum number of pending transactions in the queue.
    /// @param _futureLimit Maximum number of future nonce transactions.
    /// @param _currentLimitBytes Maximum size of pending transactions in the queue in bytes.
    /// @param _futureLimitBytes Maximum size of future nonce transactions in bytes.
    TransactionQueue( unsigned _limit = 1024, unsigned _futureLimit = 1024,
        unsigned _currentLimitBytes = 12322916, unsigned _futureLimitBytes = 24645833 );
    TransactionQueue( Limits const& _l )
        : TransactionQueue(
              _l.currentLimit, _l.futureLimit, _l.currentLimitBytes, _l.futureLimitBytes ) {}
    ~TransactionQueue();
    void HandleDestruction();
    /// Add transaction to the queue to be verified and imported.
    /// @param _data RLP encoded transaction data.
    /// @param _nodeId Optional network identified of a node transaction comes from.
    void enqueue( RLP const& _data, h512 const& _nodeId );

    /// Verify and add transaction to the queue synchronously.
    /// @param _tx RLP encoded transaction data.
    /// @param _ik Set to Retry to force re-adding a transaction that was previously dropped.
    /// @param _isFuture True if transaction should be put in future queue
    /// @returns Import result code.
    ImportResult import(
        bytes const& _tx, IfDropped _ik = IfDropped::Ignore, bool _isFuture = false ) {
        return import( &_tx, _ik, _isFuture );
    }

    /// Verify and add transaction to the queue synchronously.
    /// @param _tx Transaction data.
    /// @param _ik Set to Retry to force re-adding a transaction that was previously dropped.
    /// @param _isFuture True if transaction should be put in future queue
    /// @returns Import result code.
    ImportResult import(
        Transaction const& _tx, IfDropped _ik = IfDropped::Ignore, bool _isFuture = false );

    /// Remove transaction from the queue
    /// @param _txHash Transaction hash
    void drop( h256 const& _txHash );

    int getCategory( const h256& hash ) { return m_currentByHash[hash]->category; }

    /// Get number of pending transactions for account.
    /// @returns Pending transaction count.
    unsigned waiting( Address const& _a ) const;

    /// Get top transactions from the queue. Returned transactions are not removed from the queue
    /// automatically.
    /// @param _limit Max number of transactions to return.
    /// @param _avoid Transactions to avoid returning.
    /// @returns up to _limit transactions ordered by nonce and gas price.
    Transactions topTransactions( unsigned _limit, h256Hash const& _avoid = h256Hash() ) const;

    // same with categories
    Transactions topTransactions( unsigned _limit, int _maxCategory = 0, int _setCategory = -1 );

    // generalization of previous
    template < class Pred >
    Transactions topTransactions( unsigned _limit, Pred pred ) const;

    /// Synchronuous version of topTransactions
    template < class... Args >
    Transactions topTransactionsSync( unsigned _limit, Args... args ) const;
    template < class... Args >
    Transactions topTransactionsSync( unsigned _limit, Args... args );

    Transactions debugGetFutureTransactions() const;

    /// Get a hash set of transactions in the queue
    /// @returns A hash set of all transactions in the queue
    // this is really heavy operation and should be used with caution
    const h256Hash knownTransactions() const;

    // Check if transaction is in the queue
    bool isTransactionKnown( h256& _hash ) const;

    /// Get max nonce for an account
    /// @returns Max transaction nonce for account in the queue
    u256 maxNonce( Address const& _a ) const;

    /// Get max nonce from current queue for an account
    /// @returns Max transaction nonce for account in the queue
    u256 maxCurrentNonce( Address const& _a ) const;

    /// Mark transaction as future. It wont be returned in topTransactions list until a transaction
    /// with a preceeding nonce is imported or marked with dropGood
    /// @param _t Transaction hash
    void setFuture( h256 const& _t );

    /// Drop a transaction from the list if exists and move following future transactions to current
    /// (if any)
    /// @param _t Transaction hash
    void dropGood( Transaction const& _t );

    struct Status {
        size_t current;
        size_t future;
        size_t unverified;
        size_t dropped;
        size_t currentBytes;
        size_t futureBytes;
    };
    /// @returns the status of the transaction queue.
    Status status() const {
        Status ret;
        DEV_GUARDED( x_queue ) { ret.unverified = m_unverified.size(); }
        ReadGuard l( m_lock );
        ret.dropped = m_dropped.size();
        ret.current = m_currentByHash.size();
        ret.future = m_futureSize;
        ret.currentBytes = m_currentSizeBytes;
        ret.futureBytes = m_futureSizeBytes;
        return ret;
    }

    /// @returns the transaction limits on current/future.
    Limits limits() const {
        return Limits{ m_limit, m_futureLimit, m_currentSizeBytes, m_futureSizeBytes };
    }

    /// @returns the number of tx in future queue.
    size_t futureSize() const { return m_futureSize; }

    /// Clear the queue
    void clear();

    /// Register a handler that will be called once there is a new transaction imported
    template < class T >
    Handler<> onReady( T const& _t ) {
        return m_onReady.add( _t );
    }

    /// Register a handler that will be called once asynchronous verification is complete an
    /// transaction has been imported
    template < class T >
    Handler< ImportResult, h256 const&, h512 const& > onImport( T const& _t ) {
        return m_onImport.add( _t );
    }

    /// Register a handler that will be called once asynchronous verification is complete an
    /// transaction has been imported
    template < class T >
    Handler< h256 const& > onReplaced( T const& _t ) {
        return m_onReplaced.add( _t );
    }

public:
    /// Verified and imported transaction
    struct VerifiedTransaction {
        VerifiedTransaction( Transaction const& _t ) : transaction( _t ) {}
        VerifiedTransaction( VerifiedTransaction&& _t )
            : transaction( std::move( _t.transaction ) ) {}

        VerifiedTransaction( VerifiedTransaction const& ) = default;  // XXX removed "delete" for
                                                                      // tricks with queue
        VerifiedTransaction& operator=( VerifiedTransaction const& ) = delete;

        Transaction transaction;  ///< Transaction data
        int category = 0;         // for sorting

        Counter< VerifiedTransaction > c;

    public:
        static uint64_t howMany() { return Counter< VerifiedTransaction >::howMany(); }
    };

    /// Transaction pending verification
    struct UnverifiedTransaction {
        UnverifiedTransaction() {}
        UnverifiedTransaction( bytesConstRef const& _t, h512 const& _nodeId )
            : transaction( _t.toBytes() ), nodeId( _nodeId ) {}
        UnverifiedTransaction( UnverifiedTransaction&& _t )
            : transaction( std::move( _t.transaction ) ), nodeId( std::move( _t.nodeId ) ) {}
        UnverifiedTransaction& operator=( UnverifiedTransaction&& _other ) {
            assert( &_other != this );

            transaction = std::move( _other.transaction );
            nodeId = std::move( _other.nodeId );
            return *this;
        }

        UnverifiedTransaction( UnverifiedTransaction const& ) = delete;
        UnverifiedTransaction& operator=( UnverifiedTransaction const& ) = delete;

        bytes transaction;  ///< RLP encoded transaction data
        h512 nodeId;        ///< Network Id of the peer transaction comes from

        Counter< UnverifiedTransaction > c;

    public:
        static uint64_t howMany() { return Counter< UnverifiedTransaction >::howMany(); }
    };

    // private:
    // HACK for IS-348
    struct PriorityCompare {
        TransactionQueue& queue;
        /// Compare transaction by nonce height and gas price.
        bool operator()(
            VerifiedTransaction const& _first, VerifiedTransaction const& _second ) const {
            int cat1 = _first.category;
            int cat2 = _second.category;

            // HACK special case for "dummy" transaction - it is always to the left of others with
            // the same category
            if ( !_first.transaction && _second.transaction )
                return cat1 >= cat2;
            else if ( _first.transaction && !_second.transaction )
                return cat1 > cat2;
            else if ( !_first.transaction && !_second.transaction )
                return cat1 < cat2;

            u256 const& height1 =
                _first.transaction.nonce() -
                queue.m_currentByAddressAndNonce[_first.transaction.sender()].begin()->first;

            u256 const& height2 =
                _second.transaction.nonce() -
                queue.m_currentByAddressAndNonce[_second.transaction.sender()].begin()->first;

            return cat1 > cat2 ||
                   ( cat1 == cat2 &&
                       ( height1 < height2 ||
                           ( height1 == height2 &&
                               _first.transaction.gasPrice() > _second.transaction.gasPrice() ) ) );
        }
    };

private:
    // Use a set with dynamic comparator for minmax priority queue. The comparator takes into
    // account min account nonce. Updating it does not affect the order.
    using PriorityQueue = boost::container::multiset< VerifiedTransaction, PriorityCompare >;

    ImportResult import(
        bytesConstRef _tx, IfDropped _ik = IfDropped::Ignore, bool _isFuture = false );
    ImportResult check_WITH_LOCK( h256 const& _h, IfDropped _ik );
    ImportResult manageImport_WITH_LOCK( h256 const& _h, Transaction const& _transaction );

    Transactions topTransactions_WITH_LOCK(
        unsigned _limit, h256Hash const& _avoid = h256Hash() ) const;
    template < class Pred >
    Transactions topTransactions_WITH_LOCK( unsigned _limit, Pred _pred ) const;
    Transactions topTransactions_WITH_LOCK(
        unsigned _limit, int _maxCategory = 0, int _setCategory = -1 );

    void insertCurrent_WITH_LOCK( std::pair< h256, Transaction > const& _p );
    void makeCurrent_WITH_LOCK( Transaction const& _t );
    bool remove_WITH_LOCK( h256 const& _txHash );
    u256 maxNonce_WITH_LOCK( Address const& _a ) const;
    u256 maxCurrentNonce_WITH_LOCK( Address const& _a ) const;
    void setFuture_WITH_LOCK( h256 const& _t );
    void verifierBody();

    mutable SharedMutex m_lock;                    ///< General lock.
    mutable boost::condition_variable_any m_cond;  // for wait/notify
    Handler<> m_readyCondNotifier;

    h256Hash m_known;  ///< Headers of transactions in both sets.

    std::unordered_map< h256, std::function< void( ImportResult ) > > m_callbacks;  ///< Called
                                                                                    ///< once.
    LruCache< h256, bool > m_dropped;  ///< Transactions that have previously been dropped

    PriorityQueue m_current;
    std::unordered_map< h256, PriorityQueue::iterator > m_currentByHash;  ///< Transaction hash to
                                                                          ///< set ref

    std::unordered_map< Address, std::map< u256, PriorityQueue::iterator > >
        m_currentByAddressAndNonce;  ///< Transactions grouped by account and nonce
    std::unordered_map< Address, std::map< u256, VerifiedTransaction > > m_future;  /// Future
                                                                                    /// transactions

    Signal<> m_onReady;  ///< Called when a subsequent call to import transactions will return a
                         ///< non-empty container. Be nice and exit fast.
    Signal< ImportResult, h256 const&, h512 const& > m_onImport;  ///< Called for each import
                                                                  ///< attempt. Arguments are
                                                                  ///< result, transaction id an
                                                                  ///< node id. Be nice and exit
                                                                  ///< fast.
    Signal< h256 const& > m_onReplaced;  ///< Called when transaction is dropped during a call to
                                         ///< import() to make room for another transaction.
    unsigned m_limit;                    ///< Max number of pending transactions
    unsigned m_futureLimit;              ///< Max number of future transactions
    unsigned m_futureSize = 0;           ///< Current number of future transactions

    unsigned m_currentSizeBytesLimit = 0;  // max pending queue size in bytes
    unsigned m_currentSizeBytes = 0;       // current pending queue size in bytes
    unsigned m_futureSizeBytesLimit = 0;   // max future queue size in bytes
    unsigned m_futureSizeBytes = 0;        // current future queue size in bytes

    std::condition_variable m_queueReady;  ///< Signaled when m_unverified has a new entry.
    std::vector< std::thread > m_verifiers;
    std::deque< UnverifiedTransaction > m_unverified;  ///< Pending verification queue
    mutable Mutex x_queue;                             ///< Verification queue mutex
    std::atomic_bool m_aborting;                       ///< Exit condition for verifier.

    Logger m_logger{ createLogger( VerbosityInfo, "tq" ) };
    Logger m_loggerDetail{ createLogger( VerbosityDebug, "tq" ) };
};

template < class... Args >
Transactions TransactionQueue::topTransactionsSync( unsigned _limit, Args... args ) const {
    UpgradableGuard rGuard( m_lock );
    Transactions res;

    res = topTransactions_WITH_LOCK( _limit, args... );
    // TODO Why wait_for needs exclusive lock?!
    if ( res.size() == 0 ) {
        UpgradeGuard wGuard( rGuard );
        MICROPROFILE_SCOPEI( "TransactionQueue", "wait_for txns 100", MP_DIMGRAY );
        m_cond.wait_for( *wGuard.mutex(),
            boost::chrono::milliseconds( 100 ) );  // TODO 100 ms was chosen randomly. it's used in
                                                   // nice thread termination in ConsensusStub
    } else
        return res;
    res = topTransactions_WITH_LOCK( _limit, args... );
    return res;
}

template < class... Args >
Transactions TransactionQueue::topTransactionsSync( unsigned _limit, Args... args ) {
    UpgradableGuard rGuard( m_lock );
    Transactions res;

    res = topTransactions_WITH_LOCK( _limit, args... );
    // TODO Why wait_for needs exclusive lock?!
    if ( res.size() == 0 ) {
        UpgradeGuard wGuard( rGuard );
        MICROPROFILE_SCOPEI( "TransactionQueue", "wait_for txns 100", MP_DIMGRAY );
        m_cond.wait_for( *wGuard.mutex(),
            boost::chrono::milliseconds( 100 ) );  // TODO 100 ms was chosen randomly. it's used in
                                                   // nice thread termination in ConsensusStub
    } else
        return res;
    res = topTransactions_WITH_LOCK( _limit, args... );
    return res;
}

template < class Pred >
Transactions TransactionQueue::topTransactions( unsigned _limit, Pred _pred ) const {
    ReadGuard l( m_lock );
    return topTransactions_WITH_LOCK( _limit, _pred );
}

template < class Pred >
Transactions TransactionQueue::topTransactions_WITH_LOCK( unsigned _limit, Pred _pred ) const {
    MICROPROFILE_SCOPEI( "TransactionQueue", "topTransactions_WITH_LOCK", MP_AZURE );
    Transactions ret;
    for ( auto t = m_current.begin(); ret.size() < _limit && t != m_current.end(); ++t )
        if ( _pred( t->transaction ) )
            ret.push_back( t->transaction );
    return ret;
}

}  // namespace eth
}  // namespace dev
