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

#include "OverlayDB.h"
#include "SHA3.h"
#include "TrieDB.h"
#include <libdevcore/Common.h>
#include <libdevcore/db.h>
#include <thread>

namespace dev {
namespace {
inline db::Slice toSlice( h256 const& _h ) {
    return db::Slice( reinterpret_cast< char const* >( _h.data() ), _h.size );
}

inline db::Slice toSlice( std::string const& _str ) {
    return db::Slice( _str.data(), _str.size() );
}

inline db::Slice toSlice( bytes const& _b ) {
    return db::Slice( reinterpret_cast< char const* >( &_b[0] ), _b.size() );
}

}  // namespace

OverlayDB::~OverlayDB() = default;

void OverlayDB::commit() {
    if ( m_db ) {
        auto writeBatch = m_db->createWriteBatch();
//      cnote << "Committing nodes to disk DB:";
#if DEV_GUARDED_DB
        DEV_READ_GUARDED( x_this )
#endif
        {
            for ( auto const& i : m_main ) {
                if ( i.second.second )
                    writeBatch->insert( toSlice( i.first ), toSlice( i.second.first ) );
                //              cnote << i.first << "#" << m_main[i.first].second;
            }
            for ( auto const& i : m_aux )
                if ( i.second.second ) {
                    bytes b = i.first.asBytes();
                    b.push_back( 255 );  // for aux
                    writeBatch->insert( toSlice( b ), toSlice( i.second.first ) );
                }
        }

        for ( unsigned i = 0; i < 10; ++i ) {
            try {
                m_db->commit( std::move( writeBatch ) );
                break;
            } catch ( boost::exception const& ex ) {
                if ( i == 9 ) {
                    cwarn << "Fail writing to state database. Bombing out.";
                    exit( -1 );
                }
                cwarn << "Error writing to state database: " << boost::diagnostic_information( ex );
                cwarn << "Sleeping for" << ( i + 1 ) << "seconds, then retrying.";
                std::this_thread::sleep_for( std::chrono::seconds( i + 1 ) );
            }
        }
#if DEV_GUARDED_DB
        DEV_WRITE_GUARDED( x_this )
#endif
        {
            m_aux.clear();
            m_main.clear();
        }
    }
}

bytes OverlayDB::lookupAux( h256 const& _h ) const {
    bytes ret = MemoryDB::lookupAux( _h );
    if ( !ret.empty() || !m_db )
        return ret;

    bytes b = _h.asBytes();
    b.push_back( 255 );  // for aux
    std::string const v = m_db->lookup( toSlice( b ) );
    if ( v.empty() )
        cwarn << "Aux not found: " << _h;

    return asBytes( v );
}

void OverlayDB::rollback() {
#if DEV_GUARDED_DB
    WriteGuard l( x_this );
#endif
    m_main.clear();
}

std::string OverlayDB::lookup( h256 const& _h ) const {
    std::string ret = MemoryDB::lookup( _h );
    if ( !ret.empty() || !m_db )
        return ret;

    return m_db->lookup( toSlice( _h ) );
}

bool OverlayDB::exists( h256 const& _h ) const {
    if ( MemoryDB::exists( _h ) )
        return true;
    return m_db && m_db->exists( toSlice( _h ) );
}


void OverlayDB::insert( h256 const& _h, bytesConstRef _v ) {
    MemoryDB::insert(_h, _v);

   if (m_commitOnEveryInsert) {
        // commit immediately to save memory
        commit();
   }
}

void OverlayDB::kill( h256 const& _h ) {
#if ETH_PARANOIA || 1
    if ( !MemoryDB::kill( _h ) ) {
        if ( m_db ) {
            if ( !m_db->exists( toSlice( _h ) ) ) {
                // No point node ref decreasing for EmptyTrie since we never bother incrementing it
                // in the first place for empty storage tries.
                if ( _h != EmptyTrie )
                    cnote << "Decreasing DB node ref count below zero with no DB node. Probably "
                             "have a corrupt Trie."
                          << _h;
                // TODO: for 1.1: ref-counted triedb.
            }
        }
    }
#else
    MemoryDB::kill( _h );
#endif
}

}  // namespace dev
