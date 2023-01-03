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
/** @file CommonJS.h
 * @authors:
 *   Gav Wood <i@gavwood.com>
 *   Marek Kotewicz <marek@ethdev.com>
 * @date 2014
 */

#pragma once

#include "CommonData.h"
#include "CommonIO.h"
#include "FixedHash.h"
#include <string>

namespace dev {
inline std::string toJS( _byte_ _b ) {
    return "0x" + std::to_string( _b );
}

template < unsigned S >
std::string toJS( FixedHash< S > const& _h ) {
    return toHexPrefixed( _h.ref() );
}

template < unsigned N >
std::string toJS( boost::multiprecision::number< boost::multiprecision::cpp_int_backend< N, N,
        boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void > > const&
        _n ) {
    std::string h = toHex( toCompactBigEndian( _n, 1 ) );
    // remove first 0, if it is necessary;
    std::string res = h[0] != '0' ? h : h.substr( 1 );
    return "0x" + res;
}

inline std::string toJS( bytes const& _n, std::size_t _padding = 0 ) {
    bytes n = _n;
    n.resize( std::max< unsigned >( n.size(), _padding ) );
    return toHexPrefixed( n );
}

template < unsigned T >
std::string toJS( SecureFixedHash< T > const& _i ) {
    std::stringstream stream;
    stream << "0x" << _i.makeInsecure().hex();
    return stream.str();
}

template < typename T >
std::string toJS( T const& _i ) {
    std::stringstream stream;
    stream << "0x" << std::hex << _i;
    return stream.str();
}

template < typename T >
std::string value_to_js( T const& _x, size_t nCharactersCount ) {
    std::string s = toJS< T >( _x );
    size_t n = s.length();
    if ( n >= ( nCharactersCount + 2 ) )  // +2 is "0x"
        return s;
    if ( n > 2 && ( s[0] == '0' && s[1] == 'x' ) )
        s = s.substr( 2, n - 2 );
    while ( s.length() < nCharactersCount )
        s = "0" + s;
    return "0x" + s;
}

template < typename T >
std::string address_to_js( T const& _x ) {
    return value_to_js< T >( _x, 40 );
}

template < typename T >
std::string u256_to_js( T const& _x ) {
    return value_to_js< T >( _x, 64 );
}

enum class OnFailed { InterpretRaw, Empty, Throw };

/// Convert string to byte array. Input parameter is hex, optionally prefixed by "0x".
/// Returns empty array if invalid input.
bytes jsToBytes( std::string const& _s, OnFailed _f = OnFailed::Empty );
/// Add '0' on, or remove items from, the front of @a _b until it is of length @a _l.
bytes padded( bytes _b, unsigned _l );
/// Add '0' on, or remove items from,  the back of @a _b until it is of length @a _l.
bytes paddedRight( bytes _b, unsigned _l );
/// Removing all trailing '0'. Returns empty array if input contains only '0' char.
bytes unpadded( bytes _s );
/// Remove all 0 byte on the head of @a _s.
bytes unpadLeft( bytes _s );
/// Convert h256 into user-readable string (by directly using std::string constructor). If it can't
/// be interpreted as an ASCII string, empty string is returned.
std::string fromRaw( h256 _n );

template < unsigned N >
FixedHash< N > jsToFixed( std::string const& _s ) {
    if ( _s.substr( 0, 2 ) == "0x" )
        // Hex
        return FixedHash< N >(
            _s.substr( 2 + std::max< unsigned >( N * 2, _s.size() - 2 ) - N * 2 ) );
    else if ( _s.find_first_not_of( "0123456789" ) == std::string::npos )
        // Decimal
        return ( typename FixedHash< N >::Arith )( _s );
    else
        // Binary
        throw std::invalid_argument( "Wrong input format: jsToFixed()" );  // FAIL
}

template < unsigned N >
boost::multiprecision::number< boost::multiprecision::cpp_int_backend< N * 8, N * 8,
    boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void > >
jsToInt( std::string const& _s ) {
    if ( _s.substr( 0, 2 ) == "0x" )
        // Hex
        return fromBigEndian< boost::multiprecision::number< boost::multiprecision::cpp_int_backend<
            N * 8, N * 8, boost::multiprecision::unsigned_magnitude,
            boost::multiprecision::unchecked, void > > >( fromHex( _s.substr( 2 ) ) );
    else if ( _s.find_first_not_of( "0123456789" ) == std::string::npos )
        // Decimal
        return boost::multiprecision::number< boost::multiprecision::cpp_int_backend< N * 8, N * 8,
            boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void > >(
            _s );
    else
        // Binary
        throw std::invalid_argument( "Wrong input format: jsToInt<>()" );  // FAIL
}

inline u256 jsToU256( std::string const& _s ) {
    return jsToInt< 32 >( _s );
}

/// Convert a string representation of a number to an int
/// String can be a normal decimal number, or a hex prefixed by 0x or 0X, or an octal if prefixed by
/// 0 Returns 0 in case of failure
inline int jsToInt( std::string const& _s ) {
    try {
        return std::stoi( _s, nullptr, 0 );
    } catch ( const std::exception& ) {
        throw std::invalid_argument( "Wrong input format: jsToInt()" );
    }
}

inline std::string jsToDecimal( std::string const& _s ) {
    return toString( jsToU256( _s ) );
}

}  // namespace dev
