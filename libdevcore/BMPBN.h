/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skaled.

    ska;ed is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skaled is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file BMPBM.h
 * @author Sergiy Lavrynenko <sergiy@skalelabs.com>
 * @date 2014
 *
 * BMPBN - Boost Multi Precision Big Number tool and conversion helper APIs
 * Type T everywhere should be kind of u256, boost::multiprecision::number,
 * boost::multiprecision::cpp_int or similar; the APIs in this namesapce are based on:
 * https://groups.google.com/forum/#!topic/boost-list/lsboSQ5lHgY
 */

#pragma once

#include <stdint.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Common.h"

namespace dev {
namespace BMPBN {

template < class T >
inline size_t lengthOf( T value ) {  // we need non-cost copied value here
    if ( value.is_zero() )
        return 1;
    if ( value.sign() < 0 )
        value = ~value;
    size_t length = 0;
    uint8_t lastByte = 0;
    do {
        lastByte = value.template convert_to< uint8_t >();
        value >>= 8;
        ++length;
    } while ( !value.is_zero() );
    if ( lastByte >= 0x80 )
        ++length;
    return length;
}

template < class T >
inline void encode( T value, uint8_t* output, size_t length ) {  // we need non-cost copied value
                                                                 // here
    if ( output == nullptr || length == 0 )
        return;
    memset( output, 0, length );
    if ( value.is_zero() )
        ( *output ) = 0;
    else if ( value.sign() > 0 )
        while ( length-- > 0 ) {
            *( output++ ) = value.template convert_to< uint8_t >();
            value >>= 8;
        }
    else {
        value = ~value;
        while ( length-- > 0 ) {
            *( output++ ) = ~value.template convert_to< uint8_t >();
            value >>= 8;
        }
    }
}

template < class T >
inline std::vector< uint8_t > encode2vec(
    T value, bool bIsInversive = false ) {  // we need non-cost copied value here
    std::vector< uint8_t > vec;
    if ( value.is_zero() )
        vec.push_back( 0 );
    else if ( value.sign() > 0 )
        while ( !value.is_zero() ) {
            uint8_t b = value.template convert_to< uint8_t >();
            if ( bIsInversive )
                vec.insert( vec.begin(), b );
            else
                vec.push_back( b );
            value >>= 8;
        }
    else {
        value = ~value;
        while ( value.is_zero() ) {
            uint8_t b = ~value.template convert_to< uint8_t >();
            if ( bIsInversive )
                vec.insert( vec.begin(), b );
            else
                vec.push_back( b );
            value >>= 8;
        }
    }
    return vec;
}

template < class T >
inline T decode( const uint8_t* input, size_t length ) {
    T result( 0 );
    if ( input == nullptr || length == 0 )
        return result;
    int bits = -8;
    while ( length-- > 1 )
        result |= T( *( input++ ) ) << ( bits += 8 );
    uint8_t a = *( input++ );
    result |= T( a ) << ( bits += 8 );
    if ( a >= 0x80 )
        result |= T( -1 ) << ( bits + 8 );
    return result;
}

template < class T >
inline std::string toHexStringWithPadding(
    const T& value, size_t nPadding = std::string::npos, bool bWith0x = true ) {
    std::stringstream ss;
    if ( bWith0x )
        ss << "0x";
    if ( nPadding != std::string::npos )
        ss << std::setfill( '0' ) << std::setw( nPadding );
    ss << std::hex << value;
    return ss.str();
}

};  // namespace BMPBN
};  // namespace dev
