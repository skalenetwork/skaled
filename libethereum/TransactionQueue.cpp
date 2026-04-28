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

#include <list>
#include <thread>
#include <vector>

using namespace std;
using namespace dev;
using namespace dev::eth;

namespace {
constexpr size_t c_maxVerificationQueueSize = 8192;
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
        // WriteGuard l2( m_lock );
        UpgradableGuard l( m_lock );

        if ( !isKnownFuture_WITH_LOCK( h, _transaction ) ) {
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
        // Remove any prior transaction with the same nonce but a lower gas price.
        // Bomb out if there's a prior transaction with higher gas price.
        auto cs = m_currentByAddressAndNonce.find( _transaction.from() );
        if ( cs != m_currentByAddressAndNonce.end() ) {
            auto t = cs->second.find( _transaction.nonce() );
            if ( t != cs->second.end() ) {
                return ImportResult::SameNonceAlreadyInQueue;
            }
        }

        if ( _transaction.nonce() < _stateNonce )
            return ImportResult::AlreadyInChain;

        ImportResult ret = ImportResult::QueueIsFull;
        bool const txNonceIsCompatibleWithCurrentQueue =
            isCurrentNonceCompatible_WITH_LOCK( _transaction, _stateNonce );

        if ( txNonceIsCompatibleWithCurrentQueue ) {
            ret = tryInsertCurrent_WITH_LOCK( make_pair( _h, _transaction ) );
            if ( ret == ImportResult::Success ) {
                BOOST_LOG( m_loggerTrace ) << "Queued vaguely legit-looking transaction " << _h;
                m_onReady();
                return ret;
            }

            // some error other than QueueIsFull happenned (should never happen)
            if ( ret != ImportResult::QueueIsFull )
                return ret;
        }

        // If transaction was not compatible with CTQ, meaning it was a future tx
        // AND FTQ (future tx queue) is disabled
        if ( !_allowFutureQueue ) {
            BOOST_LOG( m_loggerWarning ) << "Transaction queue cannot accept transaction " << _h;
            return ret;  // ret is QueueIsFull
        }

        // If transaction was not compatible with CTQ, meaning it was a future tx
        // AND FTQ is enabled -> try to add to FTQ
        ret = tryInsertFuture_WITH_LOCK( make_pair( _h, _transaction ) );
        if ( ret == ImportResult::Success )
            BOOST_LOG( m_loggerTrace ) << "Queued future transaction " << _h;
        else if ( ret == ImportResult::QueueIsFull )
            BOOST_LOG( m_loggerWarning )
                << "Future transaction queue is full. Dropping transaction " << _h;

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

ImportResult TransactionQueue::tryInsertCurrent_WITH_LOCK(
    std::pair< h256, Transaction > const& _p ) {
    const size_t transactionSizeBytes = _p.second.toBytes().size();

    if ( m_current.size() + 1 > m_limit ||
         m_currentSizeBytes + transactionSizeBytes > m_currentSizeBytesLimit ) {
        return ImportResult::QueueIsFull;
    }

    if ( m_currentByHash.count( _p.first ) ) {
        BOOST_LOG( m_loggerWarning ) << "Transaction hash" << _p.first << "already in current";
        return ImportResult::Success;
    }

    // Defensive: compatibility was checked by the caller, but keep CTQ insertion self-contained.
    Transaction const& t = _p.second;
    auto cs = m_currentByAddressAndNonce.find( t.from() );
    if ( cs != m_currentByAddressAndNonce.end() &&
         cs->second.find( t.nonce() ) != cs->second.end() ) {
        return ImportResult::SameNonceAlreadyInQueue;
    }

    removeFuture_WITH_LOCK( t.from(), t.nonce() );

    // Insert into current
    auto inserted = m_currentByAddressAndNonce[t.from()].insert(
        std::make_pair( t.nonce(), PriorityQueue::iterator() ) );
    PriorityQueue::iterator handle = m_current.emplace( VerifiedTransaction( t ) );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
    inserted.first->second = handle;
    m_currentByHash[_p.first] = handle;
#pragma GCC diagnostic pop
    m_currentSizeBytes += transactionSizeBytes;

    // Move following transactions from future to current
    makeCurrent_WITH_LOCK( t );
    m_known.insert( _p.first );

    return ImportResult::Success;
}

ImportResult TransactionQueue::tryInsertFuture_WITH_LOCK(
    std::pair< h256, Transaction > const& _p ) {
    Transaction const& t = _p.second;
    size_t const transactionSizeBytes = t.toBytes().size();
    auto fs = m_future.find( t.from() );

    if ( fs != m_future.end() ) {
        auto existing = fs->second.find( t.nonce() );
        if ( existing != fs->second.end() ) {
            if ( existing->second.transaction.sha3() == _p.first )
                return ImportResult::Success;  // tx already exists

            // different tx with same nonce
            return ImportResult::SameNonceAlreadyInQueue;
        }
    }

    size_t const nextFutureSize = m_futureSize + 1;
    size_t const nextFutureSizeBytes = m_futureSizeBytes + transactionSizeBytes;
    if ( nextFutureSize > m_futureLimit || nextFutureSizeBytes > m_futureSizeBytesLimit )
        return ImportResult::QueueIsFull;

    ++m_futureSize;
    m_futureSizeBytes += transactionSizeBytes;
    m_future[t.from()].emplace( t.nonce(), VerifiedTransaction( t ) );
    m_known.insert( _p.first );
    return ImportResult::Success;
}

bool TransactionQueue::isCurrentNonceCompatible_WITH_LOCK(
    Transaction const& _transaction, u256 const& _stateNonce ) const {
    auto cs = m_currentByAddressAndNonce.find( _transaction.from() );

    // no txs from same user in queue -> check if nonce is compatible with state nonce
    if ( cs == m_currentByAddressAndNonce.end() || cs->second.empty() )
        return _transaction.nonce() == _stateNonce;

    // Some txs from same user in queue -> check if nonce is compatible with last tx from same user
    // in queue
    return _transaction.nonce() == cs->second.rbegin()->first + 1;
}

bool TransactionQueue::isKnownFuture_WITH_LOCK(
    h256 const& _h, Transaction const& _transaction ) const {
    auto fs = m_future.find( _transaction.from() );
    if ( fs == m_future.end() )
        return false;
    auto existing = fs->second.find( _transaction.nonce() );
    return existing != fs->second.end() && existing->second.transaction.sha3() == _h;
}

void TransactionQueue::removeFuture_WITH_LOCK( Address const& _from, u256 const& _nonce ) {
    auto fs = m_future.find( _from );
    if ( fs == m_future.end() )
        return;

    auto existing = fs->second.find( _nonce );
    if ( existing == fs->second.end() )
        return;

    m_futureSizeBytes -= existing->second.transaction.toBytes().size();
    m_known.erase( existing->second.transaction.sha3() );
    fs->second.erase( existing );
    --m_futureSize;
    if ( fs->second.empty() )
        m_future.erase( fs );
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

void TransactionQueue::setFuture_WITH_LOCK( h256 const& _txHash ) {
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
        m_currentSizeBytes -= t.transaction.toBytes().size();
        m_futureSizeBytes += t.transaction.toBytes().size();
        target.emplace( t.transaction.nonce(), move( t ) );
        m_current.erase( m->second );
        ++m_futureSize;
    }
    queue.erase( cutoff, queue.end() );
    if ( queue.empty() )
        m_currentByAddressAndNonce.erase( from );

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
}

// Note - this function is only used for tests
void TransactionQueue::setFuture( h256 const& _txHash ) {
    WriteGuard l( m_lock );
    return setFuture_WITH_LOCK( _txHash );
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
                size_t const transactionSizeBytes = ft->second.transaction.toBytes().size();
                if ( m_current.size() + 1 > m_limit ||
                     m_currentSizeBytes + transactionSizeBytes > m_currentSizeBytesLimit ) {
                    break;
                }

                auto inserted = m_currentByAddressAndNonce[_t.from()].insert(
                    std::make_pair( ft->second.transaction.nonce(), PriorityQueue::iterator() ) );
                PriorityQueue::iterator handle = m_current.emplace( move( ft->second ) );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
                inserted.first->second = handle;
                m_currentByHash[( *handle ).transaction.sha3()] = handle;
#pragma GCC diagnostic pop
                m_futureSizeBytes -= transactionSizeBytes;
                m_currentSizeBytes += transactionSizeBytes;
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

    if ( newCurrent )
        m_onReady();
}

void TransactionQueue::drop( h256 const& _txHash ) {
    UpgradableGuard l( m_lock );

    if ( !m_known.count( _txHash ) )
        return;

    UpgradeGuard ul( l );
    m_dropped.insert( _txHash, true );
    remove_WITH_LOCK( _txHash );
}

void TransactionQueue::dropGood( Transaction const& _t ) {
    MICROPROFILE_SCOPEI( "TransactionQueue", "dropGood", MP_CORNSILK );
    MICROPROFILE_ENTERI( "TransactionQueue", "lock", MP_OLDLACE );
    WriteGuard l( m_lock );
    MICROPROFILE_LEAVE();

#ifdef BITE
    // BITE transactions are stored separately
    // they are also stored in the strict order
    // delete and return
    if ( m_bite2Queue.dropGood( _t ) )
        return;
#endif

    if ( m_known.count( _t.sha3() ) )
        remove_WITH_LOCK( _t.sha3() );

    if ( !_t.isInvalid() )
        makeCurrent_WITH_LOCK( _t );
}

void TransactionQueue::dropMany( h256Hash const& _txHashes ) {
    WriteGuard l( m_lock );

    for ( auto&& _txHash : _txHashes ) {
        if ( !m_known.count( _txHash ) )
            continue;
        m_dropped.insert( _txHash, true );
        remove_WITH_LOCK( _txHash );
    }
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

void TransactionQueue::setBITE2QueueOnInit( std::deque< Transaction >&& _ctxQueue ) {
    return m_bite2Queue.setQueueOnInit( std::move( _ctxQueue ) );
}
#endif
