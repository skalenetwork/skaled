/*
    Modifications Copyright (C) 2018-2026 SKALE Labs

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
/** @file PrecompiledHelpers.h
 * @author SKALE Labs
 * @date 2025
 *
 * Helper functions for precompiled contracts
 */

#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/CommonData.h>
#include <libdevcore/Log.h>
#ifdef BITE2
#include <libdevcore/RLP.h>
#endif
#include <libethcore/Common.h>
#include <boost/filesystem.hpp>
#include <string>
#include <utility>

namespace dev {
namespace eth {

// Get logger for precompiled contracts
Logger& getLogger( int a_severity = VerbosityTrace );

#ifndef FAIR
// Convert bytes to string with length
void convertBytesToString(
    bytesConstRef _in, size_t _startPosition, std::string& _out, size_t& _stringLength );

// Compute file size
size_t statComputeFileSize( const char* _strFileName );

// Get file storage directory for an address
boost::filesystem::path getFileStorageDir( const Address& _address );

// Check if JSON path is accessible for security
bool statIsAccessibleJsonPath( const std::string& strPath );

// Calculate string bytes count in 32-byte pages
size_t statCalcStringBytesCountInPages32( size_t len_str );

// Check and truncate output string if it's too long
void statCheckOuputStringSizeOverflow( std::string& s );

// Add 32-byte padding to bytes
bytes& statBytesAddPad32( bytes& rv );

// Convert string to bytes with length prefix
bytes statStringToBytesWithLength( std::string& s );

// Parse u256 from hex or decimal string
dev::u256 statParseU256HexOrDec( const std::string& strValue );

// Check if call is to historic data
bool isCallToHistoricData( const std::string& callData );

// Parse historic field request
std::pair< std::string, unsigned > parseHistoricFieldRequest( std::string callData );

// Convert hex string to address (u256)
dev::u256 statS2A( const std::string& saIn );

#endif

}  // namespace eth
}  // namespace dev
