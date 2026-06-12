/*
    Modifications Copyright (C) 2018-2019 SKALE Labs

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
/** @file CommonData.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "CommonData.h"
#include <random>

#include "Exceptions.h"
#include "RLP.h"

using namespace std;
using namespace dev;

namespace {
int fromHexChar( char _i ) noexcept {
    if ( _i >= '0' && _i <= '9' )
        return _i - '0';
    if ( _i >= 'a' && _i <= 'f' )
        return _i - 'a' + 10;
    if ( _i >= 'A' && _i <= 'F' )
        return _i - 'A' + 10;
    return -1;
}
}  // namespace

bool dev::isHex( string const& _s ) noexcept {
    auto it = _s.begin();
    if ( _s.compare( 0, 2, "0x" ) == 0 )
        it += 2;
    return std::all_of( it, _s.end(), []( char c ) { return fromHexChar( c ) != -1; } );
}

std::string dev::escaped( std::string const& _s, bool _all ) {
    static const map< char, char > prettyEscapes{ { '\r', 'r' }, { '\n', 'n' }, { '\t', 't' },
        { '\v', 'v' } };
    std::string ret;
    ret.reserve( _s.size() + 2 );
    ret.push_back( '"' );
    for ( auto i : _s )
        if ( i == '"' && !_all )
            ret += "\\\"";
        else if ( i == '\\' && !_all )
            ret += "\\\\";
        else if ( prettyEscapes.count( i ) && !_all ) {
            ret += '\\';
            ret += prettyEscapes.find( i )->second;
        } else if ( i < ' ' || _all ) {
            ret += "\\x";
            ret.push_back( "0123456789abcdef"[( uint8_t ) i / 16] );
            ret.push_back( "0123456789abcdef"[( uint8_t ) i % 16] );
        } else
            ret.push_back( i );
    ret.push_back( '"' );
    return ret;
}


bytes dev::fromHex( std::string const& _s, WhenError _throw ) {
    unsigned s = ( _s.size() >= 2 && _s[0] == '0' && _s[1] == 'x' ) ? 2 : 0;
    std::vector< uint8_t > ret;
    ret.reserve( ( _s.size() - s + 1 ) / 2 );

    if ( _s.size() % 2 ) {
        int h = fromHexChar( _s[s++] );
        if ( h != -1 )
            ret.push_back( h );
        else if ( _throw == WhenError::Throw )
            BOOST_THROW_EXCEPTION( BadHexCharacter() );
        else
            return bytes();
    }
    for ( unsigned i = s; i < _s.size(); i += 2 ) {
        int h = fromHexChar( _s[i] );
        int l = fromHexChar( _s[i + 1] );
        if ( h != -1 && l != -1 )
            ret.push_back( ( _byte_ )( h * 16 + l ) );
        else if ( _throw == WhenError::Throw )
            BOOST_THROW_EXCEPTION( BadHexCharacter() );
        else
            return bytes();
    }
    return ret;
}

bytes dev::asNibbles( bytesConstRef const& _s ) {
    std::vector< uint8_t > ret;
    ret.reserve( _s.size() * 2 );
    for ( auto i : _s ) {
        ret.push_back( i / 16 );
        ret.push_back( i % 16 );
    }
    return ret;
}

std::string dev::toString( string32 const& _s ) {
    std::string ret;
    for ( unsigned i = 0; i < 32 && _s[i]; ++i )
        ret.push_back( _s[i] );
    return ret;
}

// Parse _count bytes of _in starting with _begin offset as big endian int.
// If there's not enough bytes in _in, consider it infinitely right-padded with zeroes.
bigint dev::parseBigEndianRightPadded(
    bytesConstRef _in, bigint const& _begin, bigint const& _count ) {
    if ( _begin > _in.count() )
        return 0;

    if ( _count > numeric_limits< size_t >::max() / 8 )
        // Otherwise, the return value would not fit in the memory.
        BOOST_THROW_EXCEPTION( ValueTooLarge() );

    size_t const begin{ _begin };
    size_t const count{ _count };

    // crop _in, not going beyond its size
    bytesConstRef cropped = _in.cropped( begin, min( count, _in.count() - begin ) );

    bigint ret = fromBigEndian< bigint >( cropped );
    // shift as if we had right-padding zeroes
    assert( count - cropped.count() <= numeric_limits< size_t >::max() / 8 );
    if ( count - cropped.count() > numeric_limits< size_t >::max() / 8 )
        BOOST_THROW_EXCEPTION( ValueTooLarge() );
    ret <<= 8 * ( count - cropped.count() );

    return ret;
}

#ifdef BITE
dev::bytes dev::abiEncodeArray( const std::vector< dev::bytes >& _elements ) {
    dev::bytes result;

    // Write array length
    dev::bytes lengthBytes = toBigEndian( dev::u256( _elements.size() ) );
    result.insert( result.end(), lengthBytes.begin(), lengthBytes.end() );

    // Calculate and write element offsets
    size_t dataOffset = _elements.size() * dev::h256::size;  // After all offset fields
    for ( const auto& elem : _elements ) {
        dev::bytes offsetBytes = toBigEndian( dev::u256( dataOffset ) );
        result.insert( result.end(), offsetBytes.begin(), offsetBytes.end() );

        size_t paddedSize = dev::h256::size + elem.size();  // length + data
        paddedSize = ( paddedSize + 31 ) / 32 * 32;         // round up to 32
        dataOffset += paddedSize;
    }

    // Write element data
    for ( const auto& elem : _elements ) {
        // Write element length
        dev::bytes elemLengthBytes = toBigEndian( dev::u256( elem.size() ) );
        result.insert( result.end(), elemLengthBytes.begin(), elemLengthBytes.end() );

        // Write element data
        result.insert( result.end(), elem.begin(), elem.end() );

        // Pad to 32 bytes
        size_t padding = ( 32 - ( elem.size() % 32 ) ) % 32;
        result.insert( result.end(), padding, 0 );
    }

    return result;
}

dev::bytes dev::rlpToAbiEncodedArrays( const dev::bytes& _rlpData ) {
    // Parse RLP: RLP(RLP(arg0_1, arg0_2, ...), RLP(arg1_1, arg1_2, ...))
    // Convert to ABI format: abi.encode(bytes[] args1, bytes[] args2)

    RLP rlp( _rlpData );
    if ( !rlp.isList() || rlp.itemCount() != 2 )
        throw std::runtime_error( "rlpToAbiEncodedArrays: expected RLP list with 2 elements" );

    // Parse first array (encrypted args)
    RLP array1Rlp = rlp[0];
    if ( !array1Rlp.isList() )
        throw std::runtime_error( "rlpToAbiEncodedArrays: first element must be a list" );

    std::vector< dev::bytes > array1Elements;
    array1Elements.reserve( array1Rlp.itemCount() );
    for ( size_t i = 0; i < array1Rlp.itemCount(); ++i ) {
        array1Elements.push_back( array1Rlp[i].toBytes() );
    }

    // Parse second array (plaintext args)
    RLP array2Rlp = rlp[1];
    if ( !array2Rlp.isList() )
        throw std::runtime_error( "rlpToAbiEncodedArrays: second element must be a list" );

    std::vector< dev::bytes > array2Elements;
    array2Elements.reserve( array2Rlp.itemCount() );
    for ( size_t i = 0; i < array2Rlp.itemCount(); ++i ) {
        array2Elements.push_back( array2Rlp[i].toBytes() );
    }

    // Calculate total size and offsets
    // ABI format: offset1(32) + offset2(32) + array1_data + array2_data
    size_t offset1 = 2 * dev::h256::size;  // After two offset fields

    // Calculate array1 size: length(32) + count*offset(32) + all elements (length + data padded to
    // 32)
    size_t array1Size = dev::h256::size;                    // length field
    array1Size += array1Elements.size() * dev::h256::size;  // offsets to elements
    for ( const auto& elem : array1Elements ) {
        size_t paddedSize = dev::h256::size + elem.size();  // length + data
        paddedSize = ( paddedSize + 31 ) / 32 * 32;         // round up to 32
        array1Size += paddedSize;
    }

    size_t offset2 = offset1 + array1Size;

    // Build result
    dev::bytes result;

    // Write offset1
    dev::bytes offset1Bytes = toBigEndian( dev::u256( offset1 ) );
    result.insert( result.end(), offset1Bytes.begin(), offset1Bytes.end() );

    // Write offset2
    dev::bytes offset2Bytes = toBigEndian( dev::u256( offset2 ) );
    result.insert( result.end(), offset2Bytes.begin(), offset2Bytes.end() );

    // Encode both arrays
    auto array1Encoded = dev::abiEncodeArray( array1Elements );
    auto array2Encoded = dev::abiEncodeArray( array2Elements );

    result.insert( result.end(), array1Encoded.begin(), array1Encoded.end() );
    result.insert( result.end(), array2Encoded.begin(), array2Encoded.end() );

    return result;
}
#endif
