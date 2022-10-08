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
/** @file MemoryDB_S.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "MemoryDB.h"
#include "Common.h"
#include "CommonData.h"
using namespace std;
using namespace dev;

namespace dev {
namespace db {

std::unordered_map< h256, std::string > MemoryDB_S::get() const {
#if DEV_GUARDED_DB
    ReadGuard l( x_this );
#endif
    std::unordered_map< h256, std::string > ret;
    for ( auto const& i : m_main )
        if ( !m_enforceRefs || i.second.second > 0 )
            ret.insert( make_pair( i.first, i.second.first ) );
    return ret;
}

MemoryDB_S& MemoryDB_S::operator=( MemoryDB_S const& _c ) {
    if ( this == &_c )
        return *this;
#if DEV_GUARDED_DB
    ReadGuard l( _c.x_this );
    WriteGuard l2( x_this );
#endif
    m_main = _c.m_main;
    m_aux = _c.m_aux;
    return *this;
}

std::string MemoryDB_S::lookup( h256 const& _h ) const {
#if DEV_GUARDED_DB
    ReadGuard l( x_this );
#endif
    auto it = m_main.find( _h );
    if ( it != m_main.end() ) {
        if ( !m_enforceRefs || it->second.second > 0 )
            return it->second.first;
        else
            cwarn << "Lookup required for value with refcount == 0. This is probably a critical "
                     "trie issue"
                  << _h;
    }
    return std::string();
}

bool MemoryDB_S::exists( h256 const& _h ) const {
#if DEV_GUARDED_DB
    ReadGuard l( x_this );
#endif
    auto it = m_main.find( _h );
    if ( it != m_main.end() && ( !m_enforceRefs || it->second.second > 0 ) )
        return true;
    return false;
}

void MemoryDB_S::insert( h256 const& _h, bytesConstRef _v ) {
#if DEV_GUARDED_DB
    WriteGuard l( x_this );
#endif
    auto it = m_main.find( _h );
    if ( it != m_main.end() ) {
        it->second.first = _v.toString();
        it->second.second++;
    } else
        m_main[_h] = make_pair( _v.toString(), 1 );
#if ETH_PARANOIA
    cdebug << "INST" << _h << "=>" << m_main[_h].second;
#endif
}

bool MemoryDB_S::kill( h256 const& _h ) {
#if DEV_GUARDED_DB
    ReadGuard l( x_this );
#endif
    if ( m_main.count( _h ) ) {
        if ( m_main[_h].second > 0 ) {
            m_main[_h].second--;
            return true;
        }
#if ETH_PARANOIA
        else {
            // If we get to this point, then there was probably a node in the level DB which we need
            // to remove and which we have previously used as part of the memory-based MemoryDB_S.
            // Nothing to be worried about *as long as the node exists in the DB*.
            cdebug << "NOKILL-WAS" << _h;
        }
        cdebug << "KILL" << _h << "=>" << m_main[_h].second;
    } else {
        cdebug << "NOKILL" << _h;
#endif
    }
    return false;
}

bytes MemoryDB_S::lookupAux( h256 const& _h ) const {
#if DEV_GUARDED_DB
    ReadGuard l( x_this );
#endif
    auto it = m_aux.find( _h );
    if ( it != m_aux.end() && ( !m_enforceRefs || it->second.second ) )
        return it->second.first;
    return bytes();
}

void MemoryDB_S::removeAux( h256 const& _h ) {
#if DEV_GUARDED_DB
    WriteGuard l( x_this );
#endif
    m_aux[_h].second = false;
}

void MemoryDB_S::insertAux( h256 const& _h, bytesConstRef _v ) {
#if DEV_GUARDED_DB
    WriteGuard l( x_this );
#endif
    m_aux[_h] = make_pair( _v.toBytes(), true );
}

void MemoryDB_S::purge() {
#if DEV_GUARDED_DB
    WriteGuard l( x_this );
#endif
    // purge m_main
    for ( auto it = m_main.begin(); it != m_main.end(); )
        if ( it->second.second )
            ++it;
        else
            it = m_main.erase( it );

    // purge m_aux
    for ( auto it = m_aux.begin(); it != m_aux.end(); )
        if ( it->second.second )
            ++it;
        else
            it = m_aux.erase( it );
}

h256Hash MemoryDB_S::keys() const {
#if DEV_GUARDED_DB
    ReadGuard l( x_this );
#endif
    h256Hash ret;
    for ( auto const& i : m_main )
        if ( i.second.second )
            ret.insert( i.first );
    return ret;
}

//// A database must implement the `forEach` method that allows the caller
//// to pass in a function `f`, which will be called with the key and value
//// of each record in the database. If `f` returns false, the `forEach`
//// method must return immediately.
// void MemoryDB_S::forEach(std::function<bool(Slice, Slice)> _f) const
//{
//#if DEV_GUARDED_DB
//    ReadGuard l( x_this );
//#endif
//    for (auto const& e : m_main)
//    {
//        if (!_f(Slice(e.first), Slice(e.second.first)))
//        {
//            return;
//        }
//    }
//}

}  // namespace db

}  // namespace dev
