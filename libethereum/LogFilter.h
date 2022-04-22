/*
    Modifications Copyright (C) 2018 SKALE Labs

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
/** @file LogFilter.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include "TransactionReceipt.h"
#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>
#include <libethcore/Common.h>

#ifdef __INTEL_COMPILER
#pragma warning( disable : 1098 )  // the qualifier on this friend declaration is ignored
#endif

namespace skale {
class State;
}

namespace dev {
namespace eth {
class LogFilter;
}

namespace eth {
/// Simple stream output for the StateDiff.
std::ostream& operator<<( std::ostream& _out, dev::eth::LogFilter const& _s );

class Block;

class LogFilter {
public:
    LogFilter( BlockNumber _earliest = 0, unsigned _latest = PendingBlock )
        : m_earliest( _earliest ), m_latest( _latest ) {}

    void streamRLP( RLPStream& _s ) const;
    h256 sha3() const;

    /// hash of earliest block which should be filtered
    BlockNumber earliest() const { return m_earliest; }

    /// hash of latest block which should be filtered
    BlockNumber latest() const { return m_latest; }

    /// Range filter is a filter which doesn't care about addresses or topics
    /// Matches are all entries from earliest to latest
    /// @returns true if addresses and topics are unspecified
    bool isRangeFilter() const;

    /// @returns bloom possibilities for all addresses and topics
    std::vector< LogBloom > bloomPossibilities() const;

    bool matches( LogBloom _bloom ) const;
    bool matches( Block const& _b, unsigned _i ) const;
    LogEntries matches( TransactionReceipt const& _r ) const;

    LogFilter address( Address _a ) {
        if ( std::find( m_addresses.begin(), m_addresses.end(), _a ) == m_addresses.end() )
            m_addresses.push_back( _a );
        return *this;
    }
    AddressHash getAddresses() const { return m_addresses; }
    LogFilter topic( unsigned _index, h256 const& _t ) {
        if ( _index < 4 && std::find( m_topics[_index].begin(), m_topics[_index].end(), _t ) ==
                               m_topics[_index].end() )
            m_topics[_index].push_back( _t );
        return *this;
    }
    std::array< h256Hash, 4 > getTopics() const { return m_topics; }
    LogFilter withEarliest( BlockNumber _e ) {
        m_earliest = _e;
        return *this;
    }
    LogFilter withLatest( BlockNumber _e ) {
        m_latest = _e;
        return *this;
    }

    friend std::ostream& dev::eth::operator<<( std::ostream& _out, dev::eth::LogFilter const& _s );

private:
    std::vector< Address > m_addresses;
    std::array< std::vector< h256 >, 4 > m_topics;
    BlockNumber m_earliest = 0;
    BlockNumber m_latest = PendingBlock;
};

}  // namespace eth

}  // namespace dev
