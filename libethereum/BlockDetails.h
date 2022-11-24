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
/** @file BlockDetails.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include "TransactionReceipt.h"
#include <libdevcore/Log.h>
#include <libdevcore/RLP.h>
#include <libethcore/Counter.h>
#include <unordered_map>

namespace dev {
namespace eth {

// TODO: OPTIMISE: constructors take bytes, RLP used only in necessary classes.

static const unsigned c_bloomIndexSize = 16;
static const unsigned c_bloomIndexLevels = 2;

static const unsigned c_invalidNumber = ( unsigned ) -1;

#pragma warning( push )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"

struct BlockDetails {
    BlockDetails() : number( c_invalidNumber ), totalDifficulty( Invalid256 ) {}
    BlockDetails( unsigned _n, u256 _tD, h256 _p, h256s _c, size_t _blockBytes )
        : number( _n ),
          totalDifficulty( _tD ),
          parent( _p ),
          children( _c ),
          blockSizeBytes( _blockBytes ) {}
    BlockDetails( RLP const& _r );
    bytes rlp() const;

    bool isNull() const { return number == c_invalidNumber; }
    explicit operator bool() const { return !isNull(); }

    unsigned number = c_invalidNumber;
    u256 totalDifficulty = Invalid256;
    h256 parent;
    h256s children;
    size_t blockSizeBytes = 0;

    unsigned size;

    Counter< BlockDetails > c;

public:
    static uint64_t howMany() { return Counter< BlockDetails >::howMany(); }
};

struct BlockLogBlooms {
    BlockLogBlooms() {}
    BlockLogBlooms( RLP const& _r ) {
        blooms = _r.toVector< LogBloom >();
        size = _r.data().size();
    }
    bytes rlp() const {
        bytes r = dev::rlp( blooms );
        size = r.size();
        return r;
    }

    LogBlooms blooms;
    mutable unsigned size;
};

struct BlocksBlooms {
    BlocksBlooms() {}
    BlocksBlooms( RLP const& _r ) {
        blooms = _r.toArray< LogBloom, c_bloomIndexSize >();
        size = _r.data().size();
    }
    bytes rlp() const {
        bytes r = dev::rlp( blooms );
        size = r.size();
        return r;
    }

    std::array< LogBloom, c_bloomIndexSize > blooms;
    mutable unsigned size;
};

struct BlockReceipts {
    BlockReceipts() {}
    BlockReceipts( RLP const& _r ) {
        for ( auto const& i : _r )
            receipts.emplace_back( i.data() );
        size = _r.data().size();
    }
    bytes rlp() const {
        RLPStream s( receipts.size() );
        for ( TransactionReceipt const& i : receipts )
            i.streamRLP( s );
        size = s.out().size();
        return s.out();
    }

    TransactionReceipts receipts;
    mutable unsigned size = 0;
};

struct BlockHash {
    BlockHash() {}
    BlockHash( h256 const& _h ) : value( _h ) {}
    BlockHash( RLP const& _r ) { value = _r.toHash< h256 >(); }
    bytes rlp() const { return dev::rlp( value ); }

    h256 value;
    static const unsigned size = 65;
};

struct TransactionAddress {
    TransactionAddress() {}
    TransactionAddress( RLP const& _rlp ) {
        blockHash = _rlp[0].toHash< h256 >();
        index = _rlp[1].toInt< unsigned >();
    }
    bytes rlp() const {
        RLPStream s( 2 );
        s << blockHash << index;
        return s.out();
    }

    explicit operator bool() const { return !!blockHash; }

    h256 blockHash;
    unsigned index = 0;

    static const unsigned size = 67;
};

using BlockDetailsHash = std::unordered_map< h256, BlockDetails >;
using BlockLogBloomsHash = std::unordered_map< h256, BlockLogBlooms >;
using BlockReceiptsHash = std::unordered_map< h256, BlockReceipts >;
using TransactionAddressHash = std::unordered_map< h256, TransactionAddress >;
using BlockHashHash = std::map< uint64_t, BlockHash >;
using BlocksBloomsHash = std::unordered_map< h256, BlocksBlooms >;

static const BlockDetails NullBlockDetails;
static const BlockLogBlooms NullBlockLogBlooms;
static const BlockReceipts NullBlockReceipts;
static const TransactionAddress NullTransactionAddress;
static const BlockHash NullBlockHash;
static const BlocksBlooms NullBlocksBlooms;


}  // namespace eth
}  // namespace dev


#pragma warning( pop )
#pragma GCC diagnostic pop