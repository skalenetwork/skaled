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

#pragma once

#ifdef BITE2

#include "Transaction.h"
#include <libdevcore/Common.h>
#include <libdevcore/Guards.h>
#include <libdevcore/Log.h>

#include <deque>

namespace dev {
namespace eth {

class BITE2TransactionQueue {
public:
    /// Thread-safe, locks and returns a copy. For Debug/RPC.
    std::deque< Transaction > debug_pendingBITE2Transactions() const;

    /// No lock, returns reference to entire buffer. For internal logic.
    std::shared_ptr< std::deque< Transaction > > pendingBITE2Transactions() const;

    // addTemp(), getTempHashes(), commitTemp(), clearTemp()
    // are always called from the same thread
    // and pendingBITE2Transactions() always called AFTER methods above are executed
    // therefore, there is no need in extra synchronization
    // only debug_pendingBITE2Transactions requires synchronization
    // because it is used by external JSON RPC API

    void addTemp( Transaction&& _t );
    /// Get hashes of CTXs crafted by transaction that is being executed now
    std::vector< h256 > getTempHashes() const;
    void commitTemp();
    void clearTemp();

    // Returns true if transaction was a BITE2 transaction and was handled
    // Returns false if it's not a BITE2 transaction.
    bool dropGood( const Transaction& _t );

    /// Set the queue on startup with CTXs that were created in the previous block
    template < LinearContainer C >
    void setQueueOnInit( C&& _ctxQueue );

private:
    std::shared_ptr< std::deque< Transaction > > m_current =
        std::make_shared< std::deque< Transaction > >();
    size_t m_currentHeadIndex = 0;
    bool m_empty = true;
    mutable SharedMutex m_lock;

    Logger m_loggerInfo{ createLogger( VerbosityInfo, "BITE2Queue" ) };
    Logger m_loggerWarning{ createLogger( VerbosityWarning, "BITE2Queue" ) };
    Logger m_loggerTrace{ createLogger( VerbosityTrace, "BITE2Queue" ) };
};

template < LinearContainer C >
void BITE2TransactionQueue::setQueueOnInit( C&& _ctxQueue ) {
    WriteGuard l( m_lock );
    CHECK_EXPRESSION( m_current );
    m_current = std::make_shared< std::deque< Transaction > >(
        std::make_move_iterator( _ctxQueue.begin() ), std::make_move_iterator( _ctxQueue.end() ) );
    m_currentHeadIndex = 0;
    m_empty = m_current->empty();
    BOOST_LOG( m_loggerInfo ) << "BITE2 queue initialized with " << m_current->size() << " CTXs";
}

}  // namespace eth
}  // namespace dev

#endif  // BITE2
