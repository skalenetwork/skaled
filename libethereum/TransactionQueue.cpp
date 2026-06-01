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
#include <libethereum/SchainPatch.h>

#include <algorithm>
#include <vector>

using namespace std;
using namespace dev;
using namespace dev::eth;

namespace {
constexpr size_t c_maxDroppedTransactionCount = 1024;

}  // namespace

TransactionQueue::TransactionQueue( unsigned _limit, unsigned _futureLimit,
    unsigned _currentLimitBytes, unsigned _futureLimitBytes )
    : m_dropped{ c_maxDroppedTransactionCount },
      m_current( PriorityCompare{ *this } ),
      m_limit( _limit ),
      m_futureLimit( _futureLimit ),
      m_currentSizeBytesLimit( _currentLimitBytes ),
      m_futureSizeBytesLimit( _futureLimitBytes ) {
    m_readyCondNotifier = this->onReady( [this]() {
        this->m_cond.notify_all();
        return;
    } );
}

TransactionQueue::~TransactionQueue() {}

ImportResult TransactionQueue::import( bytesConstRef _transactionRLP, IfDropped _ik,
    bool _allowFutureQueue, u256 const& _stateNonce ) {
    try {
        Transaction t = Transaction( _transactionRLP, CheckTransaction::Everything, false,
            EIP1559TransactionsPatch::isEnabledInWorkingBlock(),
            InvalidTransactionFormatPatch::isEnabledInWorkingBlock()
#ifdef BITE
                ,
            Bite2Patch::isEnabledInWorkingBlock()
#endif  // BITE
        );
        return import( t, _ik, _allowFutureQueue, _stateNonce );
    } catch ( Exception const& ) {
        return ImportResult::Malformed;
    }
}

ImportResult TransactionQueue::check_WITH_LOCK( h256 const& _h, IfDropped _ik ) {
    if ( m_known.count( _h ) )
        return ImportResult::AlreadyKnown;

    if ( m_dropped.touch( _h ) && _ik == IfDropped::Ignore )
        return ImportResult::AlreadyInChain;

    return ImportResult::Success;
}

ImportResult TransactionQueue::import( Transaction const& _transaction, IfDropped _ik,
    bool _allowFutureQueue, u256 const& _stateNonce ) {
    if ( _transaction.hasZeroSignature() )
        return ImportResult::ZeroSignature;
    // Check if we already know this transaction.
    h256 h = _transaction.sha3( WithSignature );

    ImportResult ret;
    {
        MICROPROFILE_SCOPEI( "TransactionQueue", "import", MP_THISTLE );
        UpgradableGuard l( m_lock );

        if ( !isExactFutureTransactionQueued_WITH_LOCK( h, _transaction ) ) {
            auto ir = check_WITH_LOCK( h, _ik );
            if ( ir != ImportResult::Success )
                return ir;
        }

        {
            _transaction.safeSender();  // Perform EC recovery outside of the write lock

            UpgradeGuard ul( l );
            ret = manageImport_WITH_LOCK( h, _transaction, _allowFutureQueue, _stateNonce );
        }
    }
    return ret;
}

bool TransactionQueue::isExactFutureTransactionQueued_WITH_LOCK(
    h256 const& _h, Transaction const& _transaction ) const {
    auto fs = m_future.find( _transaction.from() );
    if ( fs == m_future.end() )
        return false;

    auto existing = fs->second.find( _transaction.nonce() );
    return existing != fs->second.end() && existing->second.transaction.sha3() == _h;
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

Transactions TransactionQueue::topTransactions( unsigned _limit ) {
    ReadGuard l( m_lock );
    return topTransactions_WITH_LOCK( _limit );
}

Transactions TransactionQueue::topTransactions_WITH_LOCK( unsigned _limit ) {
    MICROPROFILE_SCOPEI( "TransactionQueue", "topTransactions_WITH_LOCK_cat", MP_PAPAYAWHIP );

    Transactions top_transactions;
    std::vector< PriorityQueue::node_type > found;

    for ( PriorityQueue::iterator transaction_ptr = m_current.begin();
          top_transactions.size() < _limit && transaction_ptr != m_current.end();
          ++transaction_ptr ) {
        top_transactions.push_back( transaction_ptr->transaction );
        found.push_back( m_current.extract( transaction_ptr ) );
    }

    // set all at once
    for ( PriorityQueue::node_type& queue_node : found ) {
        m_current.insert( std::move( queue_node ) );
    }


    // HACK For IS-348
    auto saved_txns = top_transactions;
    std::stable_sort( top_transactions.begin(), top_transactions.end(),
        TransactionQueue::PriorityCompare{ *this } );
    bool found_difference = false;
    for ( size_t i = 0; i < top_transactions.size(); ++i ) {
        if ( top_transactions[i].sha3() != saved_txns[i].sha3() )
            found_difference = true;
    }
    if ( found_difference ) {
        BOOST_LOG( m_loggerError ) << "IS-348 bug detected. Wrong transaction order in "
                                      "block proposal was fixed by workaround :(";
        BOOST_LOG( m_loggerTrace ) << "<i> <old> <new>";
        for ( size_t i = 0; i < top_transactions.size(); ++i ) {
            BOOST_LOG( m_loggerTrace )
                << i << " " << saved_txns[i].sha3() << " " << top_transactions[i].sha3();
        }
    }

    return top_transactions;
}

// note - this function is currently only used when tracing is enabled
bool TransactionQueue::isTransactionKnown( h256& _hash ) const {
    h256Hash rv;
    {  // block
        ReadGuard l( m_lock );
        return m_known.count( _hash ) > 0;
    }
}


// note - this function is heavy and is only used in tests
const h256Hash TransactionQueue::knownTransactions() const {
    h256Hash rv;
    {  // block
        ReadGuard l( m_lock );
        rv = m_known;
    }  // block
    return rv;
}

ImportResult TransactionQueue::manageImport_WITH_LOCK( h256 const& _h,
    Transaction const& _transaction, bool _allowFutureQueue, u256 const& _stateNonce ) {
    try {
        assert( _h == _transaction.sha3() );

        // check if same nonce tx already in CTQ
        auto cs = m_currentByAddressAndNonce.find( _transaction.from() );
        if ( cs != m_currentByAddressAndNonce.end() ) {
            auto t = cs->second.find( _transaction.nonce() );
            if ( t != cs->second.end() ) {
                return ImportResult::SameNonceAlreadyInQueue;
            }
        }

        // try finding a tx with same nonce in future queue first
        auto fs = m_future.find( _transaction.from() );
        if ( fs != m_future.end() ) {
            auto t = fs->second.find( _transaction.nonce() );
            if ( t != fs->second.end() ) {
                // exact same tx already in future
                if ( t->second.transaction.sha3() != _h )
                    return ImportResult::SameNonceAlreadyInQueue;

                auto eraseFuture = [&]() {
                    --m_futureSize;
                    m_futureSizeBytes -= t->second.transaction.toBytes().size();
                    m_known.erase( _h );
                    fs->second.erase( t );
                    if ( fs->second.empty() )
                        m_future.erase( fs );
                };

                // tx with same nonce already in FTQ & has old nonce -> remove
                if ( _transaction.nonce() < _stateNonce ) {
                    eraseFuture();
                    return ImportResult::AlreadyInChain;
                }

                // tx with same nonce already in FTQ & has future nonce -> leave it be
                if ( !isCurrentNonceCompatible_WITH_LOCK( _transaction, _stateNonce ) )
                    return ImportResult::Success;

                // at this point, the tx is:
                // - in FTQ
                // - has same nonce as the new tx
                // - has nonce compatible with CTQ (i.e. not too old, not too in the future)

                if ( !hasCurrentCapacity_WITH_LOCK( _transaction ) ) {
                    m_blockedPromotions[_transaction.from()] = _transaction.nonce();
                    return ImportResult::QueueIsFull;
                }

                // move it from future to current
                eraseFuture();
                ImportResult ret = insertCurrent_WITH_LOCK( make_pair( _h, _transaction ) );
                if ( ret == ImportResult::Success ) {
                    BOOST_LOG( m_loggerTrace ) << "Queued vaguely legit-looking transaction " << _h;
                    m_onReady();
                }
                return ret;
            }
        }

        // at this point - tx was not in FTQ

        // old tx -> discard
        if ( _transaction.nonce() < _stateNonce )
            return ImportResult::AlreadyInChain;

        ImportResult ret = ImportResult::QueueIsFull;
        // if compatible with CTQ - try insert in CTQ - may fail due to queue full
        if ( isCurrentNonceCompatible_WITH_LOCK( _transaction, _stateNonce ) ) {
            // may fail insertion if queue full
            ret = insertCurrent_WITH_LOCK( make_pair( _h, _transaction ) );
            if ( ret == ImportResult::Success ) {
                BOOST_LOG( m_loggerTrace ) << "Queued vaguely legit-looking transaction " << _h;
                m_onReady();
            }
            // if future -> try insert in FTQ
        } else if ( _allowFutureQueue && _transaction.nonce() > _stateNonce ) {
            // may fail insertion if queue full
            ret = insertFuture_WITH_LOCK( make_pair( _h, _transaction ) );
            if ( ret == ImportResult::Success )
                BOOST_LOG( m_loggerTrace ) << "Queued future transaction " << _h;
        }

        if ( ret == ImportResult::QueueIsFull ) {
            BOOST_LOG( m_loggerWarning )
                << "Transaction queue is full. Rejecting transaction " << _h;
        }
        return ret;
    } catch ( Exception const& _e ) {
        BOOST_LOG( m_loggerTrace )
            << "Ignoring invalid transaction: " << diagnostic_information( _e );
        return ImportResult::Malformed;
    } catch ( std::exception const& _e ) {
        BOOST_LOG( m_loggerTrace ) << "Ignoring invalid transaction: " << _e.what();
        return ImportResult::Malformed;
    }
}

u256 TransactionQueue::maxNonce( Address const& _a ) const {
    ReadGuard l( m_lock );
    return maxNonce_WITH_LOCK( _a );
}

u256 TransactionQueue::maxCurrentNonce( Address const& _a ) const {
    ReadGuard l( m_lock );
    return maxCurrentNonce_WITH_LOCK( _a );
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

u256 TransactionQueue::maxCurrentNonce_WITH_LOCK( Address const& _a ) const {
    u256 ret = 0;
    auto cs = m_currentByAddressAndNonce.find( _a );
    if ( cs != m_currentByAddressAndNonce.end() && !cs->second.empty() )
        ret = cs->second.rbegin()->first + 1;
    return ret;
}

ImportResult TransactionQueue::insertCurrent_WITH_LOCK( std::pair< h256, Transaction > const& _p ) {
    if ( m_currentByHash.count( _p.first ) ) {
        BOOST_LOG( m_loggerWarning ) << "Transaction hash" << _p.first << "already in current";
        return ImportResult::Success;
    }

    Transaction const& t = _p.second;
    if ( !hasCurrentCapacity_WITH_LOCK( t ) )
        return ImportResult::QueueIsFull;

    // Insert into current
    auto inserted = m_currentByAddressAndNonce[t.from()].insert(
        std::make_pair( t.nonce(), PriorityQueue::iterator() ) );
    PriorityQueue::iterator handle = m_current.emplace( VerifiedTransaction( t ) );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
    inserted.first->second = handle;
    m_currentByHash[_p.first] = handle;
#pragma GCC diagnostic pop
    m_currentSizeBytes += t.toBytes().size();

    // Move following transactions from future to current
    makeCurrent_WITH_LOCK( t );
    m_known.insert( _p.first );
    return ImportResult::Success;
}

ImportResult TransactionQueue::insertFuture_WITH_LOCK( std::pair< h256, Transaction > const& _p ) {
    Transaction const& t = _p.second;
    size_t const transactionSizeBytes = t.toBytes().size();

    if ( m_futureSize + 1 > m_futureLimit ||
         m_futureSizeBytes + transactionSizeBytes > m_futureSizeBytesLimit )
        return ImportResult::QueueIsFull;

    m_future[t.from()].emplace( t.nonce(), VerifiedTransaction( t ) );
    ++m_futureSize;
    m_futureSizeBytes += transactionSizeBytes;
    m_known.insert( _p.first );

    return ImportResult::Success;
}

bool TransactionQueue::hasCurrentCapacity_WITH_LOCK( Transaction const& _transaction ) const {
    size_t const transactionSizeBytes = _transaction.toBytes().size();
    return m_current.size() + 1 <= m_limit &&
           m_currentSizeBytes + transactionSizeBytes <= m_currentSizeBytesLimit;
}

bool TransactionQueue::isCurrentNonceCompatible_WITH_LOCK(
    Transaction const& _transaction, u256 const& _stateNonce ) const {
    auto cs = m_currentByAddressAndNonce.find( _transaction.from() );
    if ( cs == m_currentByAddressAndNonce.end() || cs->second.empty() ) {
        return _transaction.nonce() == _stateNonce;
    }

    u256 expectedNonce = _stateNonce;
    auto it = cs->second.lower_bound( expectedNonce );
    while ( it != cs->second.end() && it->first == expectedNonce ) {
        ++expectedNonce;
        ++it;
    }

    return _transaction.nonce() == expectedNonce;
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
    m_currentSizeBytes -= ( *t->second ).transaction.toBytes().size();
    m_current.erase( t->second );
    m_currentByHash.erase( t );
    if ( it->second.empty() )
        m_currentByAddressAndNonce.erase( it );
    m_known.erase( _txHash );
    return true;
}

bool TransactionQueue::removeFuture_WITH_LOCK( Transaction const& _transaction ) {
    auto fs = m_future.find( _transaction.from() );
    if ( fs == m_future.end() )
        return false;

    auto ft = fs->second.find( _transaction.nonce() );
    if ( ft == fs->second.end() || ft->second.transaction.sha3() != _transaction.sha3() )
        return false;

    m_futureSizeBytes -= ft->second.transaction.toBytes().size();
    --m_futureSize;
    m_known.erase( _transaction.sha3() );
    fs->second.erase( ft );
    if ( fs->second.empty() )
        m_future.erase( fs );

    auto blockedPromotion = m_blockedPromotions.find( _transaction.from() );
    if ( blockedPromotion != m_blockedPromotions.end() &&
         blockedPromotion->second <= _transaction.nonce() )
        m_blockedPromotions.erase( blockedPromotion );

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

bool TransactionQueue::setFuture_WITH_LOCK( h256 const& _txHash ) {
    auto it = m_currentByHash.find( _txHash );
    if ( it == m_currentByHash.end() )
        return false;

    VerifiedTransaction const& st = *( it->second );

    Address from = st.transaction.from();
    auto& queue = m_currentByAddressAndNonce[from];
    auto& target = m_future[from];
    auto cutoff = queue.lower_bound( st.transaction.nonce() );
    bool movedToFuture = false;
    for ( auto m = cutoff; m != queue.end(); ++m ) {
        VerifiedTransaction& t = const_cast< VerifiedTransaction& >(
            *( m->second ) );  // set has only const iterators. Since we are moving out of container
                               // that's fine
        m_currentByHash.erase( t.transaction.sha3() );
        m_currentSizeBytes -= t.transaction.toBytes().size();
        m_futureSizeBytes += t.transaction.toBytes().size();
        target.emplace( t.transaction.nonce(), move( t ) );
        m_current.erase( m->second );
        ++m_futureSize;
        movedToFuture = true;
    }
    queue.erase( cutoff, queue.end() );
    if ( queue.empty() )
        m_currentByAddressAndNonce.erase( from );
    if ( movedToFuture )
        m_blockedPromotions.erase( from );

    while ( m_futureSize > m_futureLimit || m_futureSizeBytes > m_futureSizeBytesLimit ) {
        // TODO: priority queue for future transactions
        // For now just drop random chain end
        --m_futureSize;
        m_futureSizeBytes -= m_future.begin()->second.rbegin()->second.transaction.toBytes().size();
        auto erasedHash = m_future.begin()->second.rbegin()->second.transaction.sha3();
        BOOST_LOG( m_loggerTrace ) << "Dropping out of bounds future transaction " << erasedHash;
        m_known.erase( erasedHash );
        m_future.begin()->second.erase( --m_future.begin()->second.end() );
        if ( m_future.begin()->second.empty() )
            m_future.erase( m_future.begin() );
    }

    if ( movedToFuture )
        return retryBlockedPromotions_WITH_LOCK();

    return false;
}

// Note - this function is only used for tests
void TransactionQueue::setFuture( h256 const& _txHash ) {
    bool readyChanged = false;
    {
        WriteGuard l( m_lock );
        readyChanged = setFuture_WITH_LOCK( _txHash );
    }
    if ( readyChanged )
        notifyReady();
}

bool TransactionQueue::makeCurrent_WITH_LOCK( Transaction const& _t ) {
    MICROPROFILE_SCOPEI( "TransactionQueue", "makeCurrent_WITH_LOCK", MP_DEEPSKYBLUE );

    return promoteFutureTransactions_WITH_LOCK( _t.from(), _t.nonce() + 1 );
}

bool TransactionQueue::promoteFutureTransactions_WITH_LOCK(
    Address const& _from, u256 const& _nonce ) {
    auto fs = m_future.find( _from );
    // no such sender in FTQ but in blocked promotions - remove from blocked promotions
    if ( fs == m_future.end() ) {
        m_blockedPromotions.erase( _from );
        return false;
    }

    auto fb = fs->second.find( _nonce );
    // tx with nonce is in FTQ already - remove from blocked promotions if it was there
    if ( fb == fs->second.end() ) {
        auto blockedPromotion = m_blockedPromotions.find( _from );
        if ( blockedPromotion != m_blockedPromotions.end() && blockedPromotion->second == _nonce )
            m_blockedPromotions.erase( blockedPromotion );
        return false;
    }

    bool newCurrent = false;
    u256 nonce = _nonce;
    auto ft = fb;
    while ( ft != fs->second.end() && ft->second.transaction.nonce() == nonce ) {
        if ( !hasCurrentCapacity_WITH_LOCK( ft->second.transaction ) ) {
            m_blockedPromotions[_from] = nonce;
            break;
        }

        auto inserted = m_currentByAddressAndNonce[_from].insert(
            std::make_pair( ft->second.transaction.nonce(), PriorityQueue::iterator() ) );
        PriorityQueue::iterator handle = m_current.emplace( move( ft->second ) );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
        inserted.first->second = handle;
        m_currentByHash[( *handle ).transaction.sha3()] = handle;
#pragma GCC diagnostic pop
        m_futureSizeBytes -= ( *handle ).transaction.toBytes().size();
        m_currentSizeBytes += ( *handle ).transaction.toBytes().size();
        --m_futureSize;
        ++ft;
        ++nonce;
        newCurrent = true;
    }
    fs->second.erase( fb, ft );
    if ( fs->second.empty() )
        m_future.erase( _from );

    if ( newCurrent ) {
        auto blockedPromotion = m_blockedPromotions.find( _from );
        if ( blockedPromotion != m_blockedPromotions.end() && blockedPromotion->second < nonce )
            m_blockedPromotions.erase( blockedPromotion );
    }

    return newCurrent;
}

bool TransactionQueue::retryBlockedPromotions_WITH_LOCK() {
    if ( m_blockedPromotions.empty() )
        return false;

    std::vector< std::pair< Address, u256 > > blockedPromotions;
    blockedPromotions.reserve( m_blockedPromotions.size() );
    for ( auto const& blockedPromotion : m_blockedPromotions )
        blockedPromotions.push_back( blockedPromotion );

    bool readyChanged = false;
    for ( auto const& blockedPromotion : blockedPromotions ) {
        // early exit if no capacity in CTQ
        if ( m_current.size() >= m_limit || m_currentSizeBytes >= m_currentSizeBytesLimit )
            return readyChanged;

        auto current = m_blockedPromotions.find( blockedPromotion.first );
        if ( current == m_blockedPromotions.end() || current->second != blockedPromotion.second )
            continue;
        readyChanged =
            promoteFutureTransactions_WITH_LOCK( blockedPromotion.first, blockedPromotion.second ) ||
            readyChanged;
    }

    return readyChanged;
}

void TransactionQueue::invalidateBlockedPromotion_WITH_LOCK(
    Address const& _from, u256 const& _nonce ) {
    auto blockedPromotion = m_blockedPromotions.find( _from );
    if ( blockedPromotion != m_blockedPromotions.end() && _nonce < blockedPromotion->second )
        m_blockedPromotions.erase( blockedPromotion );
}

void TransactionQueue::drop( h256 const& _txHash ) {
    bool readyChanged = false;
    {
        UpgradableGuard l( m_lock );

        if ( !m_known.count( _txHash ) )
            return;

        UpgradeGuard ul( l );
        m_dropped.insert( _txHash, true );

        auto current = m_currentByHash.find( _txHash );
        if ( current != m_currentByHash.end() )
            invalidateBlockedPromotion_WITH_LOCK(
                ( *current->second ).transaction.from(), ( *current->second ).transaction.nonce() );

        if ( remove_WITH_LOCK( _txHash ) )
            readyChanged = retryBlockedPromotions_WITH_LOCK();
    }

    if ( readyChanged )
        notifyReady();
}

TransactionQueue::DropGoodResult TransactionQueue::dropGood(
    Transaction const& _t, ReadyNotification _notification ) {
    MICROPROFILE_SCOPEI( "TransactionQueue", "dropGood", MP_CORNSILK );
    MICROPROFILE_ENTERI( "TransactionQueue", "lock", MP_OLDLACE );
    DropGoodResult result;
    {
        WriteGuard l( m_lock );
        MICROPROFILE_LEAVE();
        result = dropGood_WITH_LOCK( _t );
    }

    if ( result.readyChanged && _notification == ReadyNotification::Notify )
        notifyReady();

    return result;
}

TransactionQueue::DropGoodResult TransactionQueue::dropGood_WITH_LOCK( Transaction const& _t ) {
    DropGoodResult result;

#ifdef BITE
    // BITE transactions are stored separately
    // they are also stored in the strict order
    // delete and return
    if ( m_bite2Queue.dropGood( _t ) ) {
        result.removed = true;
        return result;
    }
#endif

    bool removedCurrent = false;
    bool removedFuture = false;
    Address removedFrom;
    u256 removedNonce;
    bool removedCurrentInfo = false;

    if ( m_known.count( _t.sha3() ) ) {
        auto current = m_currentByHash.find( _t.sha3() );
        if ( current != m_currentByHash.end() ) {
            removedFrom = ( *current->second ).transaction.from();
            removedNonce = ( *current->second ).transaction.nonce();
            removedCurrentInfo = true;
        }
        removedCurrent = remove_WITH_LOCK( _t.sha3() );
        if ( !removedCurrent && !_t.isInvalid() )
            removedFuture = removeFuture_WITH_LOCK( _t );
    }
    result.removed = removedCurrent || removedFuture;

    if ( !_t.isInvalid() ) {
        auto blockedPromotion = m_blockedPromotions.find( _t.from() );
        if ( blockedPromotion != m_blockedPromotions.end() &&
             blockedPromotion->second <= _t.nonce() )
            m_blockedPromotions.erase( blockedPromotion );
        result.readyChanged = makeCurrent_WITH_LOCK( _t ) || result.readyChanged;
    } else if ( removedCurrentInfo )
        invalidateBlockedPromotion_WITH_LOCK( removedFrom, removedNonce );

    if ( removedCurrent )
        result.readyChanged = retryBlockedPromotions_WITH_LOCK() || result.readyChanged;

    return result;
}

void TransactionQueue::dropMany( h256Hash const& _txHashes ) {
    bool readyChanged = false;
    bool removedCurrent = false;
    {
        WriteGuard l( m_lock );

        for ( auto&& _txHash : _txHashes ) {
            if ( !m_known.count( _txHash ) )
                continue;
            m_dropped.insert( _txHash, true );
            auto current = m_currentByHash.find( _txHash );
            if ( current != m_currentByHash.end() )
                invalidateBlockedPromotion_WITH_LOCK( ( *current->second ).transaction.from(),
                    ( *current->second ).transaction.nonce() );
            removedCurrent = remove_WITH_LOCK( _txHash ) || removedCurrent;
        }

        if ( removedCurrent )
            readyChanged = retryBlockedPromotions_WITH_LOCK();
    }

    if ( readyChanged )
        notifyReady();
}

void TransactionQueue::notifyReady() {
    m_onReady();
}

void TransactionQueue::clear() {
    WriteGuard l( m_lock );
    m_known.clear();
    m_current.clear();
    m_currentSizeBytes = 0;
    m_dropped.clear();
    m_currentByAddressAndNonce.clear();
    m_currentByHash.clear();
    m_future.clear();
    m_futureSize = 0;
    m_futureSizeBytes = 0;
    m_blockedPromotions.clear();
}

Transactions TransactionQueue::debugGetFutureTransactions() const {
    Transactions res;
    ReadGuard l( m_lock );
    for ( auto addressAndMap : m_future ) {
        for ( auto nonceAndTransaction : addressAndMap.second ) {
            res.push_back( nonceAndTransaction.second.transaction );
        }  // for nonce
    }      // for address
    return res;
}

#ifdef BITE
std::shared_ptr< std::deque< Transaction > > TransactionQueue::pendingBITE2Transactions() const {
    return m_bite2Queue.pendingBITE2Transactions();
}

std::deque< Transaction > TransactionQueue::debug_pendingBITE2Transactions() const {
    return m_bite2Queue.debug_pendingBITE2Transactions();
}

void TransactionQueue::addTempBITE2Transaction( dev::eth::Transaction&& _transaction ) {
    m_bite2Queue.addTemp( std::move( _transaction ) );
}

std::vector< h256 > TransactionQueue::getTempBITE2Hashes() const {
    return m_bite2Queue.getTempHashes();
}

void TransactionQueue::commitTempBITE2Transactions() {
    m_bite2Queue.commitTemp();
}

void TransactionQueue::clearTempBITE2Transactions() {
    m_bite2Queue.clearTemp();
}

std::vector< dev::h256 > TransactionQueue::getNCTXOrigins( size_t _n ) const {
    return m_bite2Queue.getNCTXOrigins( _n );
}

std::optional< std::vector< dev::h256 > >
TransactionQueue::validateNextExpectedBITE2CTXsAndGetOrigins(
    std::vector< Transaction > const& _ctxs ) const {
    return m_bite2Queue.validateNextExpectedCTXsAndGetOrigins( _ctxs );
}

void TransactionQueue::setBITE2QueueOnInit( std::deque< Transaction >&& _ctxQueue ) {
    return m_bite2Queue.setQueueOnInit( std::move( _ctxQueue ) );
}
#endif
