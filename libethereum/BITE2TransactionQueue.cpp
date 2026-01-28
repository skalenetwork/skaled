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

void BITE2TransactionQueue::import( Transaction&& _t ) {
    WriteGuard l( m_lock );
    BOOST_LOG( m_loggerTrace ) << "BITE2 txn arrived";
    m_current.push_back( std::move( _t ) );
}

std::vector< Transaction > BITE2TransactionQueue::debug_pendingBITE2Transactions() const {
    ReadGuard l( m_lock );
    return m_current;
}

const std::vector< Transaction >& BITE2TransactionQueue::pendingBITE2Transactions() const {
    return m_current;
}

void BITE2TransactionQueue::addTemp( Transaction&& _t ) {
    m_temp.emplace_back( std::move( _t ) );
}

void BITE2TransactionQueue::commitTemp() {
    WriteGuard l( m_lock );
    for ( auto& tx : m_temp ) {
        m_current.push_back( std::move( tx ) );
    }
    m_temp.clear();
}

void BITE2TransactionQueue::clearTemp() {
    m_temp.clear();
}

void BITE2TransactionQueue::clear() {
    WriteGuard l( m_lock );
    m_current.clear();
    m_currentHeadIndex.store( 0, std::memory_order_relaxed );
    m_temp.clear();
}

bool BITE2TransactionQueue::dropGood( const Transaction& _t ) {
    if ( _t.isCTX() ) {
        // BITE2 transactions are stored separately
        // they are also stored in the strict order
        CHECK_EXPRESSION( m_currentHeadIndex < m_current.size() );
        // Check that we indeed are dropping the front transaction
        CHECK_EXPRESSION( _t == m_current[m_currentHeadIndex] );
        m_currentHeadIndex.fetch_add( 1, std::memory_order_relaxed );
        return true;
    }
    return false;
}


#endif  // BITE2
