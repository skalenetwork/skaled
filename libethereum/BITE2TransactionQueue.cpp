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

#ifdef BITE

#include "BITE2TransactionQueue.h"
#include <libethcore/Exceptions.h>

using namespace dev;
using namespace dev::eth;

std::deque< Transaction > BITE2TransactionQueue::debug_pendingBITE2Transactions() const {
    // requires lock because called from JSON RPC API
    ReadGuard l( m_lock );
    CHECK_EXPRESSION( m_current );
    return *m_current;
}

std::shared_ptr< std::deque< Transaction > > BITE2TransactionQueue::pendingBITE2Transactions()
    const {
    // no lock - always called AFTER all txns in block are executed
    // no new txns can be added at this point
    CHECK_EXPRESSION( m_current );
    return m_current;
}

void BITE2TransactionQueue::addTemp( Transaction&& _t ) {
    WriteGuard l( m_lock );
    CHECK_EXPRESSION( m_current );
    CHECK_EXPRESSION( _t.isCTX() );
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

    if ( m_currentHeadIndex == m_current->size() - 1 )
        return {};

    std::vector< h256 > res;
    res.reserve( m_current->size() - 1 - m_currentHeadIndex );
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
    m_currentHeadIndex = m_current->size() - 1;
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

bool BITE2TransactionQueue::dropGood( const Transaction& _t ) {
    if ( _t.isCTX() ) {
        WriteGuard l( m_lock );
        CHECK_EXPRESSION( m_current );
        CHECK_EXPRESSION( !m_current->empty() );
        // BITE2 transactions are stored separately
        // they are also stored in the strict order
        // Check that we indeed are dropping the front transaction
        CHECK_EXPRESSION( _t == m_current->front() );
        m_current->pop_front();
        if ( m_current->empty() ) {
            m_currentHeadIndex = 0;
            m_empty = true;
        } else {
            CHECK_EXPRESSION( m_currentHeadIndex > 0 );
            m_currentHeadIndex -= 1;
        }
        return true;
    }
    return false;
}

void BITE2TransactionQueue::setQueueOnInit( std::deque< Transaction >&& _ctxQueue ) {
    WriteGuard l( m_lock );
    CHECK_EXPRESSION( m_current );
    m_current = std::make_shared< std::deque< Transaction > >( std::move( _ctxQueue ) );
    m_currentHeadIndex = m_current->empty() ? 0 : m_current->size() - 1;
    m_empty = m_current->empty();
    BOOST_LOG( m_loggerInfo ) << "BITE2 queue initialized with " << m_current->size() << " CTXs";
}

std::vector< dev::h256 > BITE2TransactionQueue::getNCTXOrigins( size_t _n ) const {
    CHECK_EXPRESSION( _n <= m_current->size() );
    std::vector< dev::h256 > res;
    res.reserve( _n );

    for ( size_t i = 0; i < _n; ++i ) {
        res.push_back( m_current->at( i ).getCTXOrigin() );
    }

    return res;
}

#endif  // BITE
