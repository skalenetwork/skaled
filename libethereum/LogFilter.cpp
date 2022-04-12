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
/** @file LogFilter.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "LogFilter.h"

#include "Block.h"
#include <libdevcore/SHA3.h>
using namespace std;
using namespace dev;
using namespace dev::eth;

std::ostream& dev::eth::operator<<( std::ostream& _out, LogFilter const& _s ) {
    // TODO
    _out << "(@" << _s.m_addresses << "#" << _s.m_topics << ">" << _s.m_earliest << "-"
         << _s.m_latest << "< )";
    return _out;
}

void LogFilter::streamRLP( RLPStream& _s ) const {
    _s.appendList( 4 ) << m_addresses << m_topics << m_earliest << m_latest;
}

h256 LogFilter::sha3() const {
    RLPStream s;
    streamRLP( s );
    return dev::sha3( s.out() );
}

bool LogFilter::isRangeFilter() const {
    if ( m_addresses.size() )
        return false;

    for ( auto const& t : m_topics )
        if ( t.size() )
            return false;

    return true;
}

bool LogFilter::matches( LogBloom _bloom ) const {
    if ( m_addresses.size() ) {
        for ( auto const& i : m_addresses )
            if ( _bloom.containsBloom< 3 >( dev::sha3( i ) ) )
                goto OK1;
        return false;
    }
OK1:
    for ( auto const& t : m_topics )
        if ( t.size() ) {
            for ( auto const& i : t )
                if ( _bloom.containsBloom< 3 >( dev::sha3( i ) ) )
                    goto OK2;
            return false;
        OK2:;
        }
    return true;
}

bool LogFilter::matches( Block const& _s, unsigned _i ) const {
    return matches( _s.receipt( _i ) ).size() > 0;
}

vector< LogBloom > LogFilter::bloomPossibilities() const {
    // return combination of each of the addresses/topics
    vector< LogBloom > ret;

    // [a, t0, t1...]
    std::vector< size_t > iter( m_topics.size() + 1 );  // every address + every topic OR

    // true if success, false if overflow
    auto inc_iter = [&]() -> bool {
        for ( size_t pos = iter.size() - 1; pos != ( size_t ) -1; --pos ) {
            size_t overflow = pos == 0 ? m_addresses.size() : m_topics[pos - 1].size();

            if ( overflow == 0 )  // skip empty dimensions
                continue;

            iter[pos]++;
            if ( iter[pos] == overflow ) {
                iter[pos] = 0;
                continue;  // to pos-1
            } else
                return true;
        }  // for pos

        return false;
    };

    // for all combinations!
    for ( ;; ) {
        LogBloom b;
        for ( size_t i = 0; i < iter.size(); ++i ) {
            if ( i == 0 && m_addresses.size() )
                b.shiftBloom< 3 >( dev::sha3( m_addresses[iter[0]] ) );
            else if ( i > 0 && m_topics[i - 1].size() )
                b.shiftBloom< 3 >( dev::sha3( m_topics[i - 1][iter[i]] ) );
        }  // for item in iter

        if ( b != LogBloom() )
            ret.push_back( b );

        if ( !inc_iter() )
            break;
    }  // for

    return ret;
}

LogEntries LogFilter::matches( TransactionReceipt const& _m ) const {
    // there are no addresses or topics to filter
    if ( isRangeFilter() )
        return _m.log();

    LogEntries ret;
    if ( matches( _m.bloom() ) )
        for ( LogEntry const& e : _m.log() ) {
            if ( !m_addresses.empty() &&
                 find( m_addresses.begin(), m_addresses.end(), e.address ) == m_addresses.end() )
                goto continue2;
            for ( unsigned i = 0; i < 4; ++i )
                if ( !m_topics[i].empty() &&
                     ( e.topics.size() < i || find( m_topics[i].begin(), m_topics[i].end(),
                                                  e.topics[i] ) == m_topics[i].end() ) )
                    goto continue2;
            ret.push_back( e );
        continue2:;
        }
    return ret;
}
