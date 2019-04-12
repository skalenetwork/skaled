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
/** @file State.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "State.h"

#include "Block.h"
#include "BlockChain.h"
#include "Defaults.h"
#include "ExtVM.h"
#include "TransactionQueue.h"
#include <libdevcore/Assertions.h>
#include <libdevcore/DBImpl.h>
#include <libdevcore/TrieHash.h>
#include <libevm/VMFactory.h>
#include <boost/filesystem.hpp>
#include <boost/timer.hpp>

#include <libdevcore/microprofile.h>

using namespace std;
using namespace dev;
using namespace dev::eth;
namespace fs = boost::filesystem;

template < class DB >
AddressHash dev::eth::commit( AccountMap const& _cache, SecureTrieDB< Address, DB >& _state ) {
    AddressHash ret;
    for ( auto const& i : _cache )
        if ( i.second.isDirty() ) {
            if ( !i.second.isAlive() )
                _state.remove( i.first );
            else {
                RLPStream s( 4 );
                s << i.second.nonce() << i.second.balance();

                if ( i.second.storageOverlay().empty() ) {
                    assert( i.second.baseRoot() );
                    s.append( i.second.baseRoot() );
                } else {
                    SecureTrieDB< h256, DB > storageDB( _state.db(), i.second.baseRoot() );
                    for ( auto const& j : i.second.storageOverlay() )
                        if ( j.second )
                            storageDB.insert( j.first, rlp( j.second ) );
                        else
                            storageDB.remove( j.first );
                    assert( storageDB.root() );
                    s.append( storageDB.root() );
                }

                if ( i.second.hasNewCode() ) {
                    h256 ch = i.second.codeHash();
                    // Store the size of the code
                    CodeSizeCache::instance().store( ch, i.second.code().size() );
                    _state.db()->insert( ch, &i.second.code() );
                    s << ch;
                } else
                    s << i.second.codeHash();

                _state.insert( i.first, &s.out() );
            }
            ret.insert( i.first );
        }
    return ret;
}


template AddressHash dev::eth::commit< OverlayDB >(
    AccountMap const& _cache, SecureTrieDB< Address, OverlayDB >& _state );
template AddressHash dev::eth::commit< MemoryDB >(
    AccountMap const& _cache, SecureTrieDB< Address, MemoryDB >& _state );
