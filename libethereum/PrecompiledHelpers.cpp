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
/** @file PrecompiledHelpers.cpp
 * @author SKALE Labs
 * @date 2024
 *
 * Helper functions for precompiled contracts
 */

#include "PrecompiledHelpers.h"
#ifdef BITE2
#include <libconsensus/node/ConsensusInterface.h>
#endif

#include <libdevcore/CommonJS.h>
#include <libdevcore/FileSystem.h>
#include <libdevcore/SHA3.h>
#include <libethcore/Common.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <fstream>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <vector>

using namespace std;

namespace dev {
namespace eth {

// Parse _count bytes of _in starting with _begin offset as big endian int.
// If there's not enough bytes in _in, consider it infinitely right-padded with zeroes.
bigint parseBigEndianRightPadded( bytesConstRef _in, bigint const& _begin, bigint const& _count ) {
    if ( _begin > _in.count() )
        return 0;
    assert( _count <= numeric_limits< size_t >::max() / 8 );  // Otherwise, the return value would
                                                              // not fit in the memory.

    size_t const begin{ _begin };
    size_t const count{ _count };

    // crop _in, not going beyond its size
    bytesConstRef cropped = _in.cropped( begin, min( count, _in.count() - begin ) );

    bigint ret = fromBigEndian< bigint >( cropped );
    // shift as if we had right-padding zeroes
    assert( count - cropped.count() <= numeric_limits< size_t >::max() / 8 );
    ret <<= 8 * ( count - cropped.count() );

    return ret;
}

Logger& getLogger( int a_severity ) {
    static std::mutex g_mtx;
    std::lock_guard< std::mutex > lock( g_mtx );
    typedef std::map< int, Logger > map_loggers_t;
    static map_loggers_t g_mapLoggers;
    if ( g_mapLoggers.find( a_severity ) == g_mapLoggers.end() )
        g_mapLoggers[a_severity] = Logger( boost::log::keywords::severity = a_severity,
            boost::log::keywords::channel = "precompiled-contracts" );
    Logger& logger = g_mapLoggers[a_severity];
    return logger;
}

#ifndef FAIR

static constexpr size_t UINT256_SIZE = 32;

void convertBytesToString(
    bytesConstRef _in, size_t _startPosition, std::string& _out, size_t& _stringLength ) {
    if ( _in.size() < UINT256_SIZE ) {
        throw std::runtime_error( "Input is too short - invalid input in convertBytesToString()" );
    }
    bigint const sstringLength( parseBigEndianRightPadded( _in, _startPosition, UINT256_SIZE ) );
    if ( sstringLength < 0 ) {
        throw std::runtime_error(
            "Negative string length - invalid input in convertBytesToString()" );
    }
    _stringLength = sstringLength.convert_to< size_t >();
    if ( _startPosition + UINT256_SIZE + _stringLength > _in.size() ) {
        throw std::runtime_error( "Invalid input in convertBytesToString()" );
    }
    vector_ref< const unsigned char > byteFilename =
        _in.cropped( _startPosition + UINT256_SIZE, _stringLength );
    _out = std::string( ( char* ) byteFilename.data(), _stringLength );
}

size_t statComputeFileSize( const char* _strFileName ) {
    std::ifstream file( _strFileName, std::ios::binary );
    file.exceptions( std::ifstream::failbit | std::ifstream::badbit );
    file.seekg( 0, std::ios::end );
    size_t n = size_t( file.tellg() );
    return n;
}

boost::filesystem::path getFileStorageDir( const Address& _address ) {
    return dev::getDataDir() / "filestorage" / _address.hex();
}

static const std::list< std::string > g_listReadableConfigParts{ "skaleConfig.sChain.nodes.",
    "skaleConfig.nodeInfo.wallets.ima.n" };

bool statIsAccessibleJsonPath( const std::string& strPath ) {
    if ( strPath.empty() )
        return false;
    std::list< std::string >::const_iterator itWalk = g_listReadableConfigParts.cbegin(),
                                             itEnd = g_listReadableConfigParts.cend();
    for ( ; itWalk != itEnd; ++itWalk ) {
        const std::string strWildCard = ( *itWalk );
        if ( boost::algorithm::starts_with( strPath, strWildCard ) )
            return true;
    }
    return false;
}

size_t statCalcStringBytesCountInPages32( size_t len_str ) {
    size_t rv = 32, blocks = len_str / 32 + ( ( ( len_str % 32 ) != 0 ) ? 1 : 0 );
    rv += blocks * 32;
    return rv;
}

void statCheckOuputStringSizeOverflow( std::string& s ) {
    static const size_t g_maxLen = 1024 * 1024 - 1;
    size_t len = s.length();
    if ( len > g_maxLen )
        s.erase( s.begin() + len, s.end() );
}

bytes& statBytesAddPad32( bytes& rv ) {
    while ( ( rv.size() % 32 ) != 0 )
        rv.push_back( 0 );
    return rv;
}

bytes statStringToBytesWithLength( std::string& s ) {
    statCheckOuputStringSizeOverflow( s );
    dev::u256 uLength( s.length() );
    bytes rv = toBigEndian( uLength );
    statBytesAddPad32( rv );
    for ( std::string::const_iterator it = s.cbegin(); it != s.cend(); ++it )
        rv.push_back( ( *it ) );
    statBytesAddPad32( rv );
    return rv;
}

dev::u256 statParseU256HexOrDec( const std::string& strValue ) {
    if ( strValue.empty() )
        return dev::u256( 0 );
    const size_t cnt = strValue.length();
    if ( cnt >= 2 && strValue[0] == '0' && ( strValue[1] == 'x' || strValue[1] == 'X' ) ) {
        dev::u256 uValue( strValue.c_str() );
        return uValue;
    }
    dev::u256 uValue = 0;
    for ( size_t i = 0; i < cnt; ++i ) {
        char chr = strValue[i];
        if ( !( '0' <= chr && chr <= '9' ) )
            throw std::runtime_error( "Bad u256 value \"" + strValue + "\" cannot be parsed" );
        int nDigit = int( chr - '0' );
        uValue *= 10;
        uValue += nDigit;
    }
    return uValue;
}

bool isCallToHistoricData( const std::string& callData ) {
    // in C++ 20 there is string::starts_with, but we do not use C++ 20 yet
    return boost::algorithm::starts_with( callData, "skaleConfig.sChain.nodes." );
}

std::pair< std::string, unsigned > parseHistoricFieldRequest( std::string callData ) {
    std::vector< std::string > splitted;
    boost::split( splitted, callData, boost::is_any_of( "." ) );
    // first 3 elements are skaleConfig, sChain, nodes - it was checked before
    unsigned id = std::stoul( splitted.at( 3 ) );
    std::string fieldName;
    std::set< std::string > allowedValues{ "id", "schainIndex", "publicKey" };
    fieldName = splitted.at( 4 );
    if ( allowedValues.count( fieldName ) ) {
        return { fieldName, id };
    } else {
        BOOST_THROW_EXCEPTION( std::runtime_error( "Unknown field:" + fieldName ) );
    }
    return { fieldName, id };
}

dev::u256 statS2A( const std::string& saIn ) {
    std::string sa;
    if ( !( saIn.length() > 2 && saIn[0] == '0' && ( saIn[1] == 'x' || saIn[1] == 'X' ) ) )
        sa = "0x" + saIn;
    else
        sa = saIn;
    dev::u256 u( sa.c_str() );
    return u;
}

#endif  // FAIR

#ifdef BITE2

std::pair< RLPStream, size_t > parseAbiEncodedBytesArray( bytesConstRef dataRef,
    bigint const& arrayOffset, const std::string& arrayName, bool validateMinLength ) {
    if ( dataRef.size() < arrayOffset.convert_to< size_t >() + dev::h256::size )
        throw std::runtime_error(
            "parseAbiEncodedBytesArray: input too short for " + arrayName + " array" );

    bigint const arrayLength( parseBigEndianRightPadded( dataRef, arrayOffset, dev::h256::size ) );
    if ( arrayLength < 0 )
        throw std::runtime_error( "parseAbiEncodedBytesArray: invalid " + arrayName + " length" );

    size_t arrayCount = arrayLength.convert_to< size_t >();
    size_t arrayBase = arrayOffset.convert_to< size_t >() + dev::h256::size;

    RLPStream arrayStream;
    arrayStream.appendList( arrayCount );

    for ( size_t i = 0; i < arrayCount; ++i ) {
        if ( dataRef.size() < arrayBase + i * dev::h256::size + dev::h256::size )
            throw std::runtime_error(
                "parseAbiEncodedBytesArray: input too short for " + arrayName + " element offset" );

        bigint elemOffset( parseBigEndianRightPadded(
            dataRef, arrayBase + i * dev::h256::size, dev::h256::size ) );
        size_t elemPos = arrayOffset.convert_to< size_t >() + dev::h256::size +
                         elemOffset.convert_to< size_t >();

        if ( dataRef.size() < elemPos + dev::h256::size )
            throw std::runtime_error(
                "parseAbiEncodedBytesArray: input too short for " + arrayName + " element length" );

        bigint elemLength( parseBigEndianRightPadded( dataRef, elemPos, dev::h256::size ) );
        if ( dataRef.size() < elemPos + dev::h256::size + elemLength.convert_to< size_t >() )
            throw std::runtime_error(
                "parseAbiEncodedBytesArray: input too short for " + arrayName + " element data" );

        // Validate encrypted element length if required
        if ( validateMinLength && elemLength.convert_to< size_t >() < BITE_CIPHERTEXT_MIN_LEN )
            throw std::runtime_error(
                "parseAbiEncodedBytesArray: encrypted argument too short, must be at least " +
                std::to_string( BITE_CIPHERTEXT_MIN_LEN ) + " bytes" );

        dev::bytes elemData =
            dataRef.cropped( elemPos + dev::h256::size, elemLength.convert_to< size_t >() )
                .toBytes();
        arrayStream << elemData;
    }

    return { arrayStream, arrayCount };
}

std::pair< dev::bytes, size_t > abiEncodedArraysToRlp( const dev::bytes& _abiEncodedArrays ) {
    // Parse ABI-encoded data: abi.encode(bytes[] encryptedArgs, bytes[] plaintextArgs)
    // ABI format: offset_to_encryptedArgs(32) + offset_to_plaintextArgs(32) + encryptedArgs_data +
    // plaintextArgs_data where encryptedArgs_data = length(32) + offset_to_elem0(32) + ... +
    // elem0_length(32) + elem0_data + ...

    bytesConstRef dataRef( _abiEncodedArrays.data(), _abiEncodedArrays.size() );

    if ( dataRef.size() < 2 * dev::h256::size )
        throw std::runtime_error( "abiEncodedArraysToRlp: input too short for two array offsets" );

    // Read offsets to the two arrays
    bigint const encryptedArgsOffset( parseBigEndianRightPadded( dataRef, 0, dev::h256::size ) );
    bigint const plaintextArgsOffset(
        parseBigEndianRightPadded( dataRef, dev::h256::size, dev::h256::size ) );

    // Parse both arrays
    auto [encryptedArgsStream, encryptedArgsCount] =
        parseAbiEncodedBytesArray( dataRef, encryptedArgsOffset, "encryptedArgs", true );
    auto [plaintextArgsStream, plaintextArgsCount] =
        parseAbiEncodedBytesArray( dataRef, plaintextArgsOffset, "plaintextArgs", false );

    // Create final RLP: RLP(RLP(encryptedArgs[0], ...), RLP(plaintextArgs[0], ...))
    RLPStream finalStream;
    finalStream.appendList( 2 );
    finalStream.appendRaw( encryptedArgsStream.out() );
    finalStream.appendRaw( plaintextArgsStream.out() );

    return { finalStream.out(), encryptedArgsCount };
}

#endif  // BITE2

}  // namespace eth
}  // namespace dev
