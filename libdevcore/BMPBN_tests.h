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

#include "BMPBN.h"

#include <skutils/console_colors.h>

#include <sstream>

namespace dev {
namespace BMPBN {

template < class T >
inline bool test1( const std::string& s, bool bIsVerbose ) {
    std::stringstream ss;
    T x( s );
    uint8_t buffer[256];
    size_t length = lengthOf( x );  // lengthOf< T >( x )
    size_t index = 0;
    encode< T >( x, buffer, length );
    T y = decode< T >( buffer, length );
    if ( bIsVerbose )
        ss << std::hex << std::setw( 2 ) << std::setfill( '0' ) << int( buffer[0] );
    if ( bIsVerbose ) {
        while ( --length > 0 )
            ss << cc::debug( ":" ) << std::setw( 2 ) << std::setfill( '0' )
               << int( buffer[++index] );
    }
    if ( bIsVerbose )
        ss << cc::debug( " = " ) << std::dec << std::setw( 1 ) << x << cc::debug( ", " );
    if ( x != y ) {
        if ( bIsVerbose )
            std::cout << "    " << ss.str() << cc::fatal( "FAILED" ) << "\n";
        return false;
    }
    // if ( bIsVerbose )
    //    std::cout << "    " << cc::success( "OK for " ) << cc::info( s ) << "\n";
    return true;
}  // namespace rpc

template < class T >
inline bool test(
    bool bIncludeNegative, bool bIncludeHuge, bool bIsVerbose, const char* strTypeDescription ) {
    if ( bIsVerbose && strTypeDescription && strTypeDescription[0] )
        std::cout << cc::debug( "Testing conversion of " ) << cc::info( strTypeDescription )
                  << cc::debug( "..." ) << "\n";
    bool bOKay = true;
    //
    if ( !test1< T >( std::string( "0" ), bIsVerbose ) )
        bOKay = false;
    if ( !test1< T >( std::string( "1" ), bIsVerbose ) )
        bOKay = false;
    if ( !test1< T >( std::string( "-1" ), bIsVerbose ) )
        bOKay = false;
    //
    if ( !test1< T >( std::string( "32767" ), bIsVerbose ) )
        bOKay = false;
    if ( bIncludeNegative )
        if ( !test1< T >( std::string( "-32767" ), bIsVerbose ) )
            bOKay = false;
    //
    if ( !test1< T >( std::string( "32768" ), bIsVerbose ) )
        bOKay = false;
    if ( bIncludeNegative )
        if ( !test1< T >( std::string( "-32768" ), bIsVerbose ) )
            bOKay = false;
    if ( !test1< T >( std::string( "32769" ), bIsVerbose ) )
        bOKay = false;
    if ( bIncludeNegative )
        if ( !test1< T >( std::string( "-32769" ), bIsVerbose ) )
            bOKay = false;
    //
    if ( !test1< T >( std::string( "65535" ), bIsVerbose ) )
        bOKay = false;
    if ( bIncludeNegative )
        if ( !test1< T >( std::string( "-65535" ), bIsVerbose ) )
            bOKay = false;
    if ( !test1< T >( std::string( "65536" ), bIsVerbose ) )
        bOKay = false;
    if ( bIncludeNegative )
        if ( !test1< T >( std::string( "-65536" ), bIsVerbose ) )
            bOKay = false;
    //
    if ( !test1< T >( std::string( "2147483647" ), bIsVerbose ) )
        bOKay = false;
    if ( bIncludeNegative )
        if ( !test1< T >( std::string( "-2147483647" ), bIsVerbose ) )
            bOKay = false;
    //
    if ( !test1< T >( std::string( "2147483648" ), bIsVerbose ) )
        bOKay = false;
    if ( bIncludeNegative )
        if ( !test1< T >( std::string( "-2147483648" ), bIsVerbose ) )
            bOKay = false;
    //
    if ( !test1< T >( std::string( "9223372036854775807" ), bIsVerbose ) )
        bOKay = false;
    if ( bIncludeNegative )
        if ( !test1< T >( std::string( "-9223372036854775807" ), bIsVerbose ) )
            bOKay = false;
    //
    if ( bIncludeHuge ) {
        if ( !test1< T >( std::string( "9223372036854775808" ), bIsVerbose ) )
            bOKay = false;
        if ( bIncludeNegative )
            if ( !test1< T >( std::string( "-9223372036854775808" ), bIsVerbose ) )
                bOKay = false;
        //

        if ( !test1< T >( std::string( "170141183460469231731687303715884105727" ), bIsVerbose ) )
            bOKay = false;
        if ( bIncludeNegative )
            if ( !test1< T >(
                     std::string( "-170141183460469231731687303715884105727" ), bIsVerbose ) )
                bOKay = false;
        if ( !test1< T >( std::string( "170141183460469231731687303715884105728" ), bIsVerbose ) )
            bOKay = false;
        if ( bIncludeNegative )
            if ( !test1< T >(
                     std::string( "-170141183460469231731687303715884105728" ), bIsVerbose ) )
                bOKay = false;
        //
        if ( !test1< T >(
                 std::string( "184467440737095516151844674407370955161518446744073709551615" ),
                 bIsVerbose ) )
            bOKay = false;
        if ( bIncludeNegative )
            if ( !test1< T >(
                     std::string( "-184467440737095516151844674407370955161518446744073709551615" ),
                     bIsVerbose ) )
                bOKay = false;
    }
    if ( bIsVerbose && strTypeDescription && strTypeDescription[0] ) {
        if ( bOKay )
            std::cout << cc::success( "Successfull conversion test of " )
                      << cc::info( strTypeDescription ) << cc::success( "." ) << "\n";
        else
            std::cout << cc::fatal( "FAILED" ) << cc::error( " conversion test of " )
                      << cc::warn( strTypeDescription ) << cc::error( "." ) << "\n";
    }
    return bOKay;
}  // namespace BMPBN

template < class T >
inline bool test_limit_limbs_and_halves(
    const char* strStartValue, size_t nBits, bool bIsVerbose ) {
    if ( bIsVerbose )
        std::cout << cc::debug( "Testing limit margin of " ) << cc::num10( nBits )
                  << cc::debug( " bit values..." ) << "\n";
    bool bOKay = true;
    if ( !test1< T >( std::string( strStartValue ), bIsVerbose ) )
        bOKay = false;
    T n( strStartValue );
    for ( size_t i = 0; i < nBits; ++i ) {
        n /= 2;
        if ( i == 0 )
            ++n;
        if ( !test1< T >( std::string( n.str() ), bIsVerbose ) )
            bOKay = false;
    }
    if ( bIsVerbose ) {
        if ( bOKay )
            std::cout << cc::success( "Successfull conversion test of " ) << cc::num10( nBits )
                      << cc::success( " bit values." ) << "\n";
        else
            std::cout << cc::fatal( "FAILED" ) << cc::error( " conversion test of " )
                      << cc::num10( nBits ) << cc::num10( nBits ) << cc::debug( " bit values" )
                      << "\n";
    }
    return bOKay;
}

};  // namespace BMPBN
};  // namespace dev
