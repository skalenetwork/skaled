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
#include <deque>
#include <functional>
#include <mutex>
#include <optional>

#ifdef BITE
#include "BITE2TransactionQueue.h"
#endif

namespace dev {
namespace eth {

/**
 * @brief Queue of verified transactions.
 *
 * Maintains two sub-queues:
 *   - CTQ (current queue): transactions whose nonce equals the next expected nonce for their
 *     sender, ordered by nonce distance and gas price.
 *   - FTQ (future queue): transactions whose nonce is ahead of the next expected nonce and
 *     therefore not yet executable. Requires allowFutureQueue to be enabled at import time.
 *
 * FTQ transactions are promoted to CTQ automatically once the preceding nonce becomes current
 * (via makeCurrent_WITH_LOCK). However, if CTQ is at capacity when a promotion is attempted,
 * the transaction stays in FTQ and the blocked nonce is recorded in m_blockedPromotions.
 * Promotions for that sender are retried whenever CTQ capacity is freed (e.g. on drop or
 * dropGood).
 *
 * Example: sender A has nonce 5 in FTQ. Transaction with nonce 4 is accepted (dropGood).
 * makeCurrent_WITH_LOCK tries to promote nonce 5 to CTQ, but CTQ is full.
 * m_blockedPromotions[A] = 5 is recorded. Later, an unrelated transaction is dropped from CTQ,
 * freeing a slot. retryBlockedPromotions_WITH_LOCK fires and promotes nonce 5 successfully.
 *
 */
class TransactionQueue {
public:
    enum class ReadyNotification { Notify, Defer };

    struct DropGoodResult {
        bool removed = false;
        bool readyChanged = false;
    };

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

    /// Import an RLP-encoded transaction.
    /// @param _allowFutureQueue If true, valid future nonce transactions may be queued in FTQ.
    /// @param _stateNonce Current nonce for the transaction sender according to chain state.
    ImportResult import(
        bytes const& _tx, IfDropped _ik, bool _allowFutureQueue, u256 const& _stateNonce ) {
        return import( &_tx, _ik, _allowFutureQueue, _stateNonce );
    }

    /// Import a decoded transaction. See the RLP overload for queue placement parameters.
    ImportResult import(
        Transaction const& _tx, IfDropped _ik, bool _allowFutureQueue, u256 const& _stateNonce );

    /// Remove a transaction from the current queue by hash.
    /// @param _txHash Transaction hash
    void drop( h256 const& _txHash );

    /// Remove many transactions from the current queue by hash.
    /// @param _txHashes Transaction hashes
    void dropMany( h256Hash const& _txHashes );

    /// Get number of pending transactions for account.
    /// @returns Pending transaction count.
    unsigned waiting( Address const& _a ) const;

    /// Get top current transactions from the queue. Returned transactions are not removed
    /// automatically.
    /// @param _limit Max number of transactions to return.
    /// @param _avoid Transactions to avoid returning.
    /// @returns up to _limit transactions ordered by nonce and gas price.
    Transactions topTransactions( unsigned _limit, h256Hash const& _avoid = h256Hash() ) const;

    /// Get top current transactions using the category-aware ordering path.
    Transactions topTransactions( unsigned _limit );

    /// Get top current transactions that satisfy pred.
    template < class Pred >
    Transactions topTransactions( unsigned _limit, Pred pred ) const;

    /// Synchronous version of topTransactions.
    template < class... Args >
    Transactions topTransactionsSync( unsigned _limit, Args... args ) const;
    template < class... Args >
    Transactions topTransactionsSync( unsigned _limit, Args... args );

    Transactions debugGetFutureTransactions() const;

    /// Get hashes of all known transactions in current and future queues.
    // This is a heavy operation and should be used with caution.
    const h256Hash knownTransactions() const;

    /// Check whether a transaction hash is known to the queue.
    bool isTransactionKnown( h256& _hash ) const;

    /// Get one greater than the highest queued nonce for an account across CTQ and FTQ.
    u256 maxNonce( Address const& _a ) const;

    /// Get one greater than the highest queued nonce for an account in CTQ only.
    u256 maxCurrentNonce( Address const& _a ) const;

    /// Move this transaction and any following same-sender current transactions to FTQ.
    /// Moved transactions are not returned by topTransactions until their preceding nonces are
    /// imported or marked with dropGood.
    /// @param _t Transaction hash
    void setFuture( h256 const& _t );

    /// Remove a transaction consumed by block execution and promote following future transactions
    /// when possible.
    /// @param _t Accepted transaction
    DropGoodResult dropGood( Transaction const& _t,
        ReadyNotification _notification = ReadyNotification::Notify );

    /// Notify ready listeners after a caller deferred queue-ready notification.
    void notifyReady();

#ifdef BITE
    /// Get all pending BITE2 transactions. Returned transactions are not removed from the queue
    /// automatically. For internal logic.
    std::shared_ptr< std::deque< Transaction > > pendingBITE2Transactions() const;

    /// Get all pending BITE2 transactions. Returned transactions are not removed from the queue
    /// automatically. For Debug/RPC.
    std::deque< Transaction > debug_pendingBITE2Transactions() const;

    /// Add BITE2 txn as temporary
    void addTempBITE2Transaction( dev::eth::Transaction&& _transaction );
    /// Get hashes of temporary CTXs in queue
    std::vector< dev::h256 > getTempBITE2Hashes() const;
    /// Move BITE2 txn from temporary to permanent
    void commitTempBITE2Transactions();
    /// Get origin for first N CTXs in queue
    std::vector< dev::h256 > getNCTXOrigins( size_t _n ) const;

    /// Verifies CTXs exactly match the next expected pending BITE2 CTXs and returns their origins.
    std::optional< std::vector< dev::h256 > > validateNextExpectedBITE2CTXsAndGetOrigins(
        std::vector< Transaction > const& _ctxs ) const;

    void clearTempBITE2Transactions();

    void setBITE2QueueOnInit( std::deque< Transaction >&& _ctxQueue );
#endif

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
        Status ret{};
        ret.unverified = 0;
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
        // Record creation time so transactions received earlier keep priority when nonce height and
        // gas price are equal.
        uint64_t creationTimeMs;

        VerifiedTransaction( Transaction const& _t ) : transaction( _t ) {
            creationTimeMs = std::chrono::duration_cast< std::chrono::milliseconds >(
                std::chrono::system_clock::now().time_since_epoch() )
                                 .count();
        }
        VerifiedTransaction( VerifiedTransaction&& _t )
            : transaction( std::move( _t.transaction ) ) {
            creationTimeMs = _t.creationTimeMs;
        }

        VerifiedTransaction( VerifiedTransaction const& ) = default;  // Needed by queue operations.
        VerifiedTransaction& operator=( VerifiedTransaction const& ) = delete;

        Transaction transaction;  ///< Transaction data

        Counter< VerifiedTransaction > c;

    public:
        static uint64_t howMany() { return Counter< VerifiedTransaction >::howMany(); }
    };

    // private:
    // HACK for IS-348
    struct PriorityCompare {
        TransactionQueue& queue;
        /// Compare transaction by nonce height and gas price.
        bool operator()(
            VerifiedTransaction const& _first, VerifiedTransaction const& _second ) const {
            // HACK special case for "dummy" transaction - it is always to the left of others with
            // the same category

            if ( !_first.transaction && _second.transaction )
                return false;
            else if ( _first.transaction && !_second.transaction )
                return true;
            else if ( !_first.transaction && !_second.transaction )
                return false;

            auto it1 = queue.m_currentByAddressAndNonce.find( _first.transaction.sender() );
            auto it2 = queue.m_currentByAddressAndNonce.find( _second.transaction.sender() );

            if ( it1 == queue.m_currentByAddressAndNonce.end() ||
                 it2 == queue.m_currentByAddressAndNonce.end() )
                return _first.creationTimeMs < _second.creationTimeMs;

            u256 const& height1 = _first.transaction.nonce() - it1->second.begin()->first;

            u256 const& height2 = _second.transaction.nonce() - it2->second.begin()->first;

            if ( height1 != height2 ) {
                // Prefer transactions closer to the sender's lowest current queued nonce.
                return height1 < height2;
            }

            // For the same height, prefer transactions with larger gas price.
            if ( _first.transaction.gasPrice() != _second.transaction.gasPrice() ) {
                return _first.transaction.gasPrice() > _second.transaction.gasPrice();
            }

            // If height and gas price are equal, prefer the transaction received earlier.

            return _first.creationTimeMs < _second.creationTimeMs;
        }
    };

private:
    // Current transactions are stored in a set ordered by PriorityCompare. The comparator depends
    // on each sender's lowest current nonce, so callers that change nonce ranges may need to
    // refresh ordering explicitly.
    using PriorityQueue = boost::container::multiset< VerifiedTransaction, PriorityCompare >;

    /**
     * Decodes and imports a transaction using the caller-provided sender state nonce.
     * @param _allowFutureQueue If true, valid future nonce transactions may be queued in FTQ.
     * @param _stateNonce Current nonce for the transaction sender according to chain state.
     */
    ImportResult import(
        bytesConstRef _tx, IfDropped _ik, bool _allowFutureQueue, u256 const& _stateNonce );

    ImportResult check_WITH_LOCK( h256 const& _h, IfDropped _ik );

    /**
     * Places a checked transaction in CTQ or FTQ, using _stateNonce to decide whether the nonce is
     * current, future, or already in chain. Re-importing an exact FTQ transaction can promote it to
     * CTQ once it becomes current-compatible.
     */
    ImportResult manageImport_WITH_LOCK( h256 const& _h, Transaction const& _transaction,
        bool _allowFutureQueue, u256 const& _stateNonce );

    /**
     * Returns true when FTQ already contains this exact transaction at the same sender and nonce.
     */
    bool isExactFutureTransactionQueued_WITH_LOCK(
        h256 const& _h, Transaction const& _transaction ) const;

    Transactions topTransactions_WITH_LOCK(
        unsigned _limit, h256Hash const& _avoid = h256Hash() ) const;
    template < class Pred >
    Transactions topTransactions_WITH_LOCK( unsigned _limit, Pred _pred ) const;
    Transactions topTransactions_WITH_LOCK( unsigned _limit );

    /**
     * Inserts a current-compatible transaction into CTQ without evicting existing transactions.
     * @returns Success if the transaction was inserted, or was already present by hash.
     *          QueueIsFull when CTQ lacks count or byte capacity.
     */
    ImportResult insertCurrent_WITH_LOCK( std::pair< h256, Transaction > const& _p );

    /**
     * Inserts a future nonce transaction into FTQ without evicting existing transactions.
     * @returns Success if the transaction was inserted.
     *          QueueIsFull when FTQ lacks count or byte capacity.
     */
    ImportResult insertFuture_WITH_LOCK( std::pair< h256, Transaction > const& _p );

    /// Returns whether adding _transaction would fit within CTQ count and byte limits.
    bool hasCurrentCapacity_WITH_LOCK( Transaction const& _transaction ) const;

    /**
     * Returns whether _transaction is the next nonce CTQ can currently accept for its sender.
     * The check starts at _stateNonce and walks the sender's contiguous CTQ nonce range, so gaps
     * created by drops can be filled even when higher nonces are already queued.
     */
    bool isCurrentNonceCompatible_WITH_LOCK(
        Transaction const& _transaction, u256 const& _stateNonce ) const;

    /**
     * Promotes the sender's contiguous FTQ range starting at _nonce into CTQ while capacity allows.
     * If CTQ fills up, records the first nonce that could not be promoted in m_blockedPromotions.
     * @returns true if at least one transaction was moved to CTQ.
     */
    bool promoteFutureTransactions_WITH_LOCK( Address const& _from, u256 const& _nonce );

    /// Retries cached FTQ-to-CTQ promotions after CTQ capacity may have been freed.
    /// @returns true if at least one transaction was moved to CTQ.
    bool retryBlockedPromotions_WITH_LOCK();

    /**
     * Clears _from's cached promotion when dropping _nonce breaks the contiguous path to the cached
     * future transaction.
     */
    void invalidateBlockedPromotion_WITH_LOCK( Address const& _from, u256 const& _nonce );

    /// Promotes future transactions that immediately follow the newly current transaction _t.
    /// @returns true if at least one transaction was moved to CTQ.
    bool makeCurrent_WITH_LOCK( Transaction const& _t );

    DropGoodResult dropGood_WITH_LOCK( Transaction const& _t );

    /// Removes a transaction from CTQ and its secondary indexes.
    bool remove_WITH_LOCK( h256 const& _txHash );

    /// Removes an exact transaction from FTQ and clears any blocked promotion that depended on it.
    bool removeFuture_WITH_LOCK( Transaction const& _transaction );

    u256 maxNonce_WITH_LOCK( Address const& _a ) const;
    u256 maxCurrentNonce_WITH_LOCK( Address const& _a ) const;
    bool setFuture_WITH_LOCK( h256 const& _t );

    mutable SharedMutex m_lock;                    ///< General lock.
    mutable boost::condition_variable_any m_cond;  // for wait/notify
    Handler<> m_readyCondNotifier;

    h256Hash m_known;  ///< Hashes of transactions in CTQ and FTQ.

    std::unordered_map< h256, std::function< void( ImportResult ) > > m_callbacks;  ///< Called
                                                                                    ///< once.
    LruCache< h256, bool > m_dropped;  ///< Transactions that have previously been dropped

    PriorityQueue m_current;
    std::unordered_map< h256, PriorityQueue::iterator > m_currentByHash;  ///< CTQ hash to iterator.

    std::unordered_map< Address, std::map< u256, PriorityQueue::iterator > >
        m_currentByAddressAndNonce;  ///< CTQ transactions grouped by account and nonce.
    std::unordered_map< Address, std::map< u256, VerifiedTransaction > >
        m_future;  ///< FTQ
                   ///< transactions
                   ///< grouped by
                   ///< account and
                   ///< nonce.

    // For each sender, stores the first FTQ nonce that was ready for CTQ but could not be promoted
    // because CTQ was full. Entries are retried when CTQ capacity may have become available.
    std::unordered_map< Address, u256 > m_blockedPromotions;

    Signal<> m_onReady;  ///< Called when a subsequent call to import transactions will return a
                         ///< non-empty container. Be nice and exit fast.
    Signal< ImportResult, h256 const&, h512 const& > m_onImport;  ///< Called for each import
                                                                  ///< attempt. Arguments are
                                                                  ///< result, transaction id, and
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

#ifdef BITE
    BITE2TransactionQueue m_bite2Queue;
#endif

    Logger m_loggerInfo{ createLogger( VerbosityInfo, "TransactionQueue" ) };
    Logger m_loggerDebug{ createLogger( VerbosityDebug, "TransactionQueue" ) };
    Logger m_loggerTrace{ createLogger( VerbosityTrace, "TransactionQueue" ) };
    Logger m_loggerError{ createLogger( VerbosityError, "TransactionQueue" ) };
    Logger m_loggerWarning{ createLogger( VerbosityWarning, "TransactionQueue" ) };
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
