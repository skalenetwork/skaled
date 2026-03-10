/*
    Modifications Copyright (C) 2018-2026 SKALE Labs

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

#ifdef BITE2

#include "BITE2TransactionQueue.h"
#include <libethcore/Exceptions.h>

using namespace dev;
using namespace dev::eth;

std::vector< Transaction > BITE2TransactionQueue::debug_pendingBITE2Transactions() const {
    // requires lock because called from JSON RPC API
    ReadGuard l( m_lock );
    CHECK_EXPRESSION( m_current );
    return *m_current;
}

const std::vector< Transaction >& BITE2TransactionQueue::pendingBITE2Transactions() const {
    // no lock - called strictly AFTER finalize(), no new txns can be added at this point
    CHECK_EXPRESSION( m_current );
    return *m_current;
}

void BITE2TransactionQueue::addTemp( Transaction&& _t ) {
    WriteGuard l( m_lock );
    CHECK_EXPRESSION( m_current );
    BOOST_LOG( m_loggerTrace ) << "BITE2 txn arrived";
    m_current->push_back( std::move( _t ) );
}

std::vector< h256 > BITE2TransactionQueue::getTempHashes() const {
    CHECK_EXPRESSION( m_current );
    // if there are no committed transactions
    // we return hashes of all transactions in m_current
    if ( m_empty ) {
        std::vector< h256 > res;
        res.reserve( m_current->size() );
        for ( const auto& tx : *m_current )
            res.push_back( tx.sha3() );
        return res;
    }

    if ( m_currentHeadIndex == m_current->size() )
        return {};

    std::vector< h256 > res;
    res.reserve( m_current->size() - m_currentHeadIndex );
    // m_currentHeadIndex always points to the last committed CTX,
    // so we return hashes of all CTXs starting after m_currentHeadIndex
    for ( size_t i = m_currentHeadIndex + 1; i < m_current->size(); ++i ) {
        res.push_back( m_current->at( i ).sha3() );
    }
    return res;
}

void BITE2TransactionQueue::commitTemp() {
    CHECK_EXPRESSION( m_current );
    if ( m_current->empty() )
        return;
    // m_currentHeadIndex should always point to last element during block execution
    // used as checkpoint for rollbacks
    m_currentHeadIndex.store( m_current->size() - 1, std::memory_order_relaxed );
    m_empty = false;
}

void BITE2TransactionQueue::clearTemp() {
    CHECK_EXPRESSION( m_current );
    // delete all temporary CTXs until last checkpoint
    WriteGuard l( m_lock );
    if ( m_empty ) {
        m_current->clear();
        return;
    }

    while ( m_current->size() > m_currentHeadIndex + 1 ) {
        m_current->pop_back();
    }
}

void BITE2TransactionQueue::clear() {
    WriteGuard l( m_lock );
    CHECK_EXPRESSION( m_current );
    m_current->clear();
    m_currentHeadIndex.store( 0, std::memory_order_relaxed );
    m_empty = true;
}

std::shared_ptr< std::vector< Transaction > > BITE2TransactionQueue::finalizeAndGetCtxs() {
    CHECK_EXPRESSION( m_current );
    // prepare for the next block processing - skaled may delete CTXs added into blockchain
    // m_currentHeadIndex points to first not yet verified CTX
    m_currentHeadIndex = 0;
    return m_current;
}

bool BITE2TransactionQueue::dropGood( const Transaction& _t ) {
    if ( _t.isCTX() ) {
        CHECK_EXPRESSION( m_current );
        // BITE2 transactions are stored separately
        // they are also stored in the strict order
        CHECK_EXPRESSION( m_currentHeadIndex < m_current->size() );
        // Check that we indeed are dropping the front transaction
        CHECK_EXPRESSION( _t == ( *m_current )[m_currentHeadIndex] );
        m_currentHeadIndex.fetch_add( 1, std::memory_order_relaxed );
        return true;
    }
    return false;
}

void BITE2TransactionQueue::setQueueOnInit( Transactions&& _ctxQueue ) {
    WriteGuard l( m_lock );
    CHECK_EXPRESSION( m_current );
    m_current = std::make_shared< std::vector< Transaction > >( std::move( _ctxQueue ) );
    m_currentHeadIndex.store( 0, std::memory_order_relaxed );
    m_empty = m_current->empty();
    BOOST_LOG( m_loggerInfo ) << "BITE2 queue initialized with " << m_current->size() << " CTXs";
}


#endif  // BITE2
