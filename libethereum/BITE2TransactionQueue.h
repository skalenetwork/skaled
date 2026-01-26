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

#include "Transaction.h"
#include <libdevcore/Guards.h>
#include <libdevcore/Log.h>

#include <deque>
#include <vector>

namespace dev {
namespace eth {

class BITE2TransactionQueue {
public:
    void import( Transaction&& _t );
    
    const std::deque< Transaction >& pending() const;

    void addTemp( Transaction&& _t );
    void commitTemp();
    void clearTemp();
    void clear();

    // Returns true if transaction was a BITE2 transaction (isCTX) and was handled
    // Returns false if it's not a BITE2 transaction.
    bool dropGood( const Transaction& _t );

private:
    std::deque< Transaction > m_current;
    std::vector< Transaction > m_temp;
    mutable SharedMutex m_lock;

    Logger m_loggerTrace{ createLogger( VerbosityTrace, "BITE2Queue" ) };
};

}  // namespace eth
}  // namespace dev
