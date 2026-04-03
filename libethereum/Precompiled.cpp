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
/** @file Precompiled.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "Precompiled.h"
#ifdef BITE2
#include "BITEConstants.h"
#endif
#include "PrecompiledHelpers.h"

#include <cryptopp/files.h>
#include <cryptopp/hex.h>
#include <cryptopp/sha.h>
#include <libdevcore/CommonJS.h>
#include <libdevcore/FileSystem.h>
#include <libdevcore/Log.h>
#ifdef BITE2
#include <libdevcore/RLP.h>
#endif
#include <libdevcore/SHA3.h>
#include <libdevcore/microprofile.h>
#include <libdevcrypto/Common.h>
#include <libdevcrypto/Hash.h>
#include <libdevcrypto/LibSnark.h>
#include <libethcore/ChainOperationParams.h>
#include <libethcore/Common.h>
#include <libethereum/Client.h>
#include <libethereum/SchainPatch.h>
#include <libethereum/SkaleHost.h>
#include <libskale/State.h>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <mutex>

#include <secp256k1_sha256.h>

#include <skutils/console_colors.h>

#include <exception>
#include <functional>
#include <sstream>
#include <string>


namespace dev {
namespace eth {

std::shared_ptr< skutils::json_config_file_accessor > g_configAccesssor;
std::shared_ptr< SkaleHost > g_skaleHost;

};  // namespace eth
};  // namespace dev

using namespace std;
using namespace dev;
using namespace dev::eth;

namespace fs = boost::filesystem;

PrecompiledRegistrar* PrecompiledRegistrar::s_this = nullptr;

PrecompiledExecutor const& PrecompiledRegistrar::executor( std::string const& _name ) {
    if ( !get()->m_execs.count( _name ) )
        BOOST_THROW_EXCEPTION( ExecutorNotFound() );
    return get()->m_execs[_name];
}

PrecompiledPricer const& PrecompiledRegistrar::pricer( std::string const& _name ) {
    if ( !get()->m_pricers.count( _name ) )
        BOOST_THROW_EXCEPTION( PricerNotFound() );
    return get()->m_pricers[_name];
}

namespace {

ETH_REGISTER_PRECOMPILED( ecrecover )( bytesConstRef _in, const PrecompiledCallContext& ) {
    struct {
        h256 hash;
        h256 v;
        h256 r;
        h256 s;
    } in;

    memcpy( &in, _in.data(), min( _in.size(), sizeof( in ) ) );

    h256 ret;
    u256 v = static_cast< u256 >( in.v );
    if ( v >= 27 && v <= 28 ) {
        SignatureStruct sig( in.r, in.s, static_cast< _byte_ >( static_cast< int >( v ) - 27 ) );
        if ( sig.isValid() ) {
            try {
                if ( Public rec = recover( sig, in.hash ) ) {
                    ret = dev::sha3( rec );
                    memset( ret.data(), 0, 12 );
                    return { true, ret.asBytes() };
                }
            } catch ( ... ) {
            }
        }
    }
    return { true, {} };
}

ETH_REGISTER_PRECOMPILED( sha256 )( bytesConstRef _in, const PrecompiledCallContext& ) {
    return { true, dev::sha256( _in ).asBytes() };
}

ETH_REGISTER_PRECOMPILED( ripemd160 )( bytesConstRef _in, const PrecompiledCallContext& ) {
    return { true, h256( dev::ripemd160( _in ), h256::AlignRight ).asBytes() };
}

ETH_REGISTER_PRECOMPILED( identity )( bytesConstRef _in, const PrecompiledCallContext& ) {
    MICROPROFILE_SCOPEI( "VM", "identity", MP_RED );
    return { true, _in.toBytes() };
}

ETH_REGISTER_PRECOMPILED( modexp )( bytesConstRef _in, const PrecompiledCallContext& ) {
    bigint const baseLength( parseBigEndianRightPadded( _in, 0, 32 ) );
    bigint const expLength( parseBigEndianRightPadded( _in, 32, 32 ) );
    bigint const modLength( parseBigEndianRightPadded( _in, 64, 32 ) );
    assert( modLength <= numeric_limits< size_t >::max() / 8 );   // Otherwise gas should be too
                                                                  // expensive.
    assert( baseLength <= numeric_limits< size_t >::max() / 8 );  // Otherwise, gas should be too
                                                                  // expensive.
    if ( modLength == 0 && baseLength == 0 )
        return { true, bytes{} };  // This is a special case where expLength can be very big.
    assert( expLength <= numeric_limits< size_t >::max() / 8 );

    bigint const base( parseBigEndianRightPadded( _in, 96, baseLength ) );
    bigint const exp( parseBigEndianRightPadded( _in, 96 + baseLength, expLength ) );
    bigint const mod( parseBigEndianRightPadded( _in, 96 + baseLength + expLength, modLength ) );

    bigint const result = mod != 0 ? boost::multiprecision::powm( base, exp, mod ) : bigint{ 0 };

    size_t const retLength( modLength );
    bytes ret( retLength );
    toBigEndian( result, ret );

    return { true, ret };
}

namespace {
bigint expLengthAdjust( bigint const& _expOffset, bigint const& _expLength, bytesConstRef _in ) {
    if ( _expLength <= 32 ) {
        bigint const exp( parseBigEndianRightPadded( _in, _expOffset, _expLength ) );
        return exp ? msb( exp ) : 0;
    } else {
        bigint const expFirstWord( parseBigEndianRightPadded( _in, _expOffset, 32 ) );
        size_t const highestBit( expFirstWord ? msb( expFirstWord ) : 0 );
        return 8 * ( _expLength - 32 ) + highestBit;
    }
}

bigint multComplexity( bigint const& _x ) {
    if ( _x <= 64 )
        return _x * _x;
    if ( _x <= 1024 )
        return ( _x * _x ) / 4 + 96 * _x - 3072;
    else
        return ( _x * _x ) / 16 + 480 * _x - 199680;
}
}  // namespace

ETH_REGISTER_PRECOMPILED_PRICER( modexp )
( bytesConstRef _in, ChainOperationParams const&, u256 const& ) {
    bigint const baseLength( parseBigEndianRightPadded( _in, 0, 32 ) );
    bigint const expLength( parseBigEndianRightPadded( _in, 32, 32 ) );
    bigint const modLength( parseBigEndianRightPadded( _in, 64, 32 ) );

    bigint const maxLength( max( modLength, baseLength ) );
    bigint const adjustedExpLength( expLengthAdjust( baseLength + 96, expLength, _in ) );

    return multComplexity( maxLength ) * max< bigint >( adjustedExpLength, 1 ) / 20;
}

ETH_REGISTER_PRECOMPILED( alt_bn128_G1_add )( bytesConstRef _in, const PrecompiledCallContext& ) {
    return dev::crypto::alt_bn128_G1_add( _in );
}

ETH_REGISTER_PRECOMPILED_PRICER( alt_bn128_G1_add )
( bytesConstRef /*_in*/, ChainOperationParams const& _chainParams, u256 const& _blockNumber ) {
    return _blockNumber < _chainParams.getIstanbulForkBlock() ? 500 : 150;
}

ETH_REGISTER_PRECOMPILED( alt_bn128_G1_mul )( bytesConstRef _in, const PrecompiledCallContext& ) {
    return dev::crypto::alt_bn128_G1_mul( _in );
}

ETH_REGISTER_PRECOMPILED_PRICER( alt_bn128_G1_mul )
( bytesConstRef /*_in*/, ChainOperationParams const& _chainParams, u256 const& _blockNumber ) {
    return _blockNumber < _chainParams.getIstanbulForkBlock() ? 40000 : 6000;
}

ETH_REGISTER_PRECOMPILED( alt_bn128_pairing_product )
( bytesConstRef _in, const PrecompiledCallContext& ) {
    return dev::crypto::alt_bn128_pairing_product( _in );
}

ETH_REGISTER_PRECOMPILED_PRICER( alt_bn128_pairing_product )
( bytesConstRef _in, ChainOperationParams const& _chainParams, u256 const& _blockNumber ) {
    auto const k = _in.size() / 192;
    return _blockNumber < _chainParams.getIstanbulForkBlock() ? 100000 + k * 80000 :
                                                                45000 + k * 34000;
}

#ifndef FAIR

// TODO: check file name and file existance
ETH_REGISTER_FS_PRECOMPILED( createFile )
( bytesConstRef _in, const PrecompiledCallContext&, skale::OverlayFS* _overlayFS ) {
    if ( !_overlayFS )
        throw runtime_error( "_overlayFS is nullptr " );

    try {
        auto rawAddress = _in.cropped( 12, 20 ).toBytes();
        std::string address;
        boost::algorithm::hex( rawAddress.begin(), rawAddress.end(), back_inserter( address ) );

        size_t filenameLength;
        std::string rawFilename;
        convertBytesToString( _in, 32, rawFilename, filenameLength );
        size_t const filenameBlocksCount = ( filenameLength + 31 ) / UINT256_SIZE;
        bigint const byteFileSize( parseBigEndianRightPadded(
            _in, 64 + filenameBlocksCount * UINT256_SIZE, UINT256_SIZE ) );
        size_t const fileSize = byteFileSize.convert_to< size_t >();
        const fs::path filePath( rawFilename );
        const fs::path fsDirectoryPath = getFileStorageDir( Address( address ) );
        if ( !fs::exists( fsDirectoryPath ) ) {
            _overlayFS->createDirectory( fsDirectoryPath.string() );
        }
        const fs::path fsFilePath = fsDirectoryPath / filePath.parent_path();
        if ( filePath.filename().extension() == "._hash" ) {
            throw std::runtime_error(
                "createFile() failed because _hash extension is not allowed" );
        }
        _overlayFS->createFile( ( fsFilePath / filePath.filename() ).string(), fileSize );

        u256 code = 1;
        bytes response = toBigEndian( code );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) ) << "Exception in createFile: " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) ) << "Unknown exception in createFile\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };
}

ETH_REGISTER_FS_PRECOMPILED( uploadChunk )
( bytesConstRef _in, const PrecompiledCallContext&, skale::OverlayFS* _overlayFS ) {
    if ( !_overlayFS )
        throw runtime_error( "_overlayFS is nullptr " );

    try {
        auto rawAddress = _in.cropped( 12, 20 ).toBytes();
        std::string address;
        boost::algorithm::hex( rawAddress.begin(), rawAddress.end(), back_inserter( address ) );

        size_t filenameLength;
        std::string filename;
        convertBytesToString( _in, 32, filename, filenameLength );
        size_t const filenameBlocksCount = ( filenameLength + 31 ) / UINT256_SIZE;

        bigint const bytePosition( parseBigEndianRightPadded(
            _in, 64 + filenameBlocksCount * UINT256_SIZE, UINT256_SIZE ) );
        size_t const position = bytePosition.convert_to< size_t >();

        bigint const byteDataLength( parseBigEndianRightPadded(
            _in, 96 + filenameBlocksCount * UINT256_SIZE, UINT256_SIZE ) );
        size_t const dataLength = byteDataLength.convert_to< size_t >();

        const fs::path filePath = getFileStorageDir( Address( address ) ) / filename;
        if ( position + dataLength > statComputeFileSize( filePath.c_str() ) ) {
            throw std::runtime_error(
                "uploadChunk() failed because chunk gets out of the file bounds" );
        }
        const _byte_* data =
            _in.cropped( 128 + filenameBlocksCount * UINT256_SIZE, dataLength ).data();

        _overlayFS->writeChunk( filePath.string(), position, dataLength, data );

        u256 code = 1;
        bytes response = toBigEndian( code );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Exception in uploadChunk: " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) ) << "Unknown exception in uploadChunk\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };
}

ETH_REGISTER_PRECOMPILED( readChunk )( bytesConstRef _in, const PrecompiledCallContext& ) {
    MICROPROFILE_SCOPEI( "VM", "readChunk", MP_ORANGERED );
    try {
        auto rawAddress = _in.cropped( 12, 20 ).toBytes();
        std::string address;
        boost::algorithm::hex( rawAddress.begin(), rawAddress.end(), back_inserter( address ) );

        size_t filenameLength;
        std::string filename;
        convertBytesToString( _in, 32, filename, filenameLength );
        size_t const filenameBlocksCount = ( filenameLength + UINT256_SIZE - 1 ) / UINT256_SIZE;

        bigint const bytePosition( parseBigEndianRightPadded(
            _in, 64 + filenameBlocksCount * UINT256_SIZE, UINT256_SIZE ) );
        size_t const position = bytePosition.convert_to< size_t >();

        bigint const byteChunkLength( parseBigEndianRightPadded(
            _in, 96 + filenameBlocksCount * UINT256_SIZE, UINT256_SIZE ) );
        size_t const chunkLength = byteChunkLength.convert_to< size_t >();

        const fs::path filePath = getFileStorageDir( Address( address ) ) / filename;
        const fs::path canonicalPath = fs::canonical( filePath );
        if ( canonicalPath.string().find( getFileStorageDir( Address( address ) ).c_str(), 0 ) !=
             0 ) {
            throw std::runtime_error( "readChunk() failed because file couldn't be read" );
        }
        if ( position > statComputeFileSize( filePath.c_str() ) ||
             position + chunkLength > statComputeFileSize( filePath.c_str() ) ) {
            throw std::runtime_error(
                "readChunk() failed because chunk gets out of the file bounds" );
        }

        std::ifstream infile( filePath.string(), std::ios_base::binary );
        infile.seekg( static_cast< long >( position ) );
        bytes buffer( chunkLength );
        infile.read(
            reinterpret_cast< char* >( &buffer[0] ), static_cast< long >( buffer.size() ) );
        return { true, buffer };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) ) << "Exception in readChunk: " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) ) << "Unknown exception in readChunk\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };
}

ETH_REGISTER_PRECOMPILED( getFileSize )( bytesConstRef _in, const PrecompiledCallContext& ) {
    try {
        auto rawAddress = _in.cropped( 12, 20 ).toBytes();
        std::string address;
        boost::algorithm::hex( rawAddress.begin(), rawAddress.end(), back_inserter( address ) );

        size_t filenameLength;
        std::string filename;
        convertBytesToString( _in, 32, filename, filenameLength );

        const fs::path filePath = getFileStorageDir( Address( address ) ) / filename;
        const fs::path canonicalPath = fs::canonical( filePath );
        if ( canonicalPath.string().find( getFileStorageDir( Address( address ) ).c_str(), 0 ) !=
             0 ) {
            throw std::runtime_error( "getFileSize() failed because file couldn't be read" );
        }

        size_t const fileSize = statComputeFileSize( filePath.c_str() );
        bytes response = toBigEndian( static_cast< u256 >( fileSize ) );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Exception in getFileSize: " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) ) << "Unknown exception in getFileSize\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };
}

ETH_REGISTER_FS_PRECOMPILED( deleteFile )
( bytesConstRef _in, const PrecompiledCallContext&, skale::OverlayFS* _overlayFS ) {
    if ( !_overlayFS )
        throw runtime_error( "_overlayFS is nullptr " );

    try {
        auto rawAddress = _in.cropped( 12, 20 ).toBytes();
        std::string address;
        boost::algorithm::hex( rawAddress.begin(), rawAddress.end(), back_inserter( address ) );
        size_t filenameLength;
        std::string filename;
        convertBytesToString( _in, 32, filename, filenameLength );

        const fs::path filePath = getFileStorageDir( Address( address ) ) / filename;

        _overlayFS->deleteFile( filePath.string() );
        _overlayFS->deleteFile( filePath.string() + "._hash" );

        u256 code = 1;
        bytes response = toBigEndian( code );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) ) << "Exception in deleteFile: " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) ) << "Unknown exception in deleteFile\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };
}

ETH_REGISTER_FS_PRECOMPILED( createDirectory )
( bytesConstRef _in, const PrecompiledCallContext&, skale::OverlayFS* _overlayFS ) {
    if ( !_overlayFS )
        throw runtime_error( "_overlayFS is nullptr " );

    try {
        auto rawAddress = _in.cropped( 12, 20 ).toBytes();
        std::string address;
        boost::algorithm::hex( rawAddress.begin(), rawAddress.end(), back_inserter( address ) );
        size_t directoryPathLength;
        std::string directoryPath;
        convertBytesToString( _in, 32, directoryPath, directoryPathLength );

        const fs::path absolutePath = getFileStorageDir( Address( address ) ) / directoryPath;
        _overlayFS->createDirectory( absolutePath.string() );

        u256 code = 1;
        bytes response = toBigEndian( code );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Exception in createDirectory: " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) ) << "Unknown exception in createDirectory\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };
}

ETH_REGISTER_FS_PRECOMPILED( deleteDirectory )
( bytesConstRef _in, const PrecompiledCallContext&, skale::OverlayFS* _overlayFS ) {
    if ( !_overlayFS )
        throw runtime_error( "_overlayFS is nullptr " );

    try {
        auto rawAddress = _in.cropped( 12, 20 ).toBytes();
        std::string address;
        boost::algorithm::hex( rawAddress.begin(), rawAddress.end(), back_inserter( address ) );
        size_t directoryPathLength;
        std::string directoryPath;
        convertBytesToString( _in, 32, directoryPath, directoryPathLength );

        const fs::path absolutePath = getFileStorageDir( Address( address ) ) / directoryPath;
        if ( !fs::exists( absolutePath ) ) {
            throw std::runtime_error( "deleteDirectory() failed because directory not exists" );
        }

        const std::string absolutePathStr = absolutePath.string();

        _overlayFS->deleteFile( absolutePathStr + "._hash" );
        _overlayFS->deleteDirectory( absolutePath.string() );

        u256 code = 1;
        bytes response = toBigEndian( code );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Exception in deleteDirectory: " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) ) << "Unknown exception in deleteDirectory\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };
}

ETH_REGISTER_FS_PRECOMPILED( calculateFileHash )
( bytesConstRef _in, const PrecompiledCallContext&, skale::OverlayFS* _overlayFS ) {
    try {
        auto rawAddress = _in.cropped( 12, 20 ).toBytes();
        std::string address;
        boost::algorithm::hex( rawAddress.begin(), rawAddress.end(), back_inserter( address ) );

        size_t filenameLength;
        std::string filename;
        convertBytesToString( _in, 32, filename, filenameLength );

        const fs::path filePath = getFileStorageDir( Address( address ) ) / filename;

        if ( !fs::exists( filePath ) ) {
            throw std::runtime_error( "calculateFileHash() failed because file does not exist" );
        }

        _overlayFS->calculateFileHash( filePath.string() );

        u256 code = 1;
        bytes response = toBigEndian( code );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Exception in calculateFileHash: " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) ) << "Unknown exception in calculateFileHash\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };
}

ETH_REGISTER_PRECOMPILED( logTextMessage )( bytesConstRef _in, const PrecompiledCallContext& ) {
    try {
        if ( !g_configAccesssor )
            throw std::runtime_error( "Config accessor was not initialized" );
        nlohmann::json joConfig = g_configAccesssor->getConfigJSON();
        bool bLoggingIsEnabledForContracts =
            joConfig["skaleConfig"]["contractSettings"]["common"]["enableContractLogMessages"]
                .get< bool >();
        if ( !bLoggingIsEnabledForContracts ) {
            u256 code = 1;
            bytes response = toBigEndian( code );
            return { true, response };
        }

        auto rawAddress = _in.cropped( 12, 20 ).toBytes();
        std::string address;
        boost::algorithm::hex( rawAddress.begin(), rawAddress.end(), back_inserter( address ) );

        bigint const byteMessageType( parseBigEndianRightPadded( _in, 32, UINT256_SIZE ) );
        size_t const nMessageType = byteMessageType.convert_to< size_t >();

        size_t lengthString;
        std::string rawString;
        convertBytesToString( _in, 64, rawString, lengthString );

        typedef std::function< std::string( const std::string& s ) > fnColorizer_t;
        fnColorizer_t fnHeader = []( const std::string& s ) -> std::string { return s; };
        fnColorizer_t fnText = []( const std::string& s ) -> std::string { return s; };

        std::string strMessageTypeDesc = "";
        switch ( nMessageType ) {
        case 0:
        default:  // normal message
            strMessageTypeDesc = "normal";
            break;
        case 1:  // debug message
            fnHeader = []( const std::string& s ) -> std::string { return s; };
            fnText = []( const std::string& s ) -> std::string { return s; };
            strMessageTypeDesc = "debug";
            break;
        case 2:  // trace message
            fnHeader = []( const std::string& s ) -> std::string { return s; };
            fnText = []( const std::string& s ) -> std::string { return s; };
            strMessageTypeDesc = "trace";
            break;
        case 3:  // warning message
            fnHeader = []( const std::string& s ) -> std::string { return s; };
            fnText = []( const std::string& s ) -> std::string { return s; };
            strMessageTypeDesc = "warning";
            break;
        case 4:  // error message
            fnHeader = []( const std::string& s ) -> std::string { return s; };
            fnText = []( const std::string& s ) -> std::string { return s; };
            strMessageTypeDesc = "error";
            break;
        case 5:  // fatal message
            fnHeader = []( const std::string& s ) -> std::string { return s; };
            fnText = []( const std::string& s ) -> std::string { return s; };
            strMessageTypeDesc = "FATAL";
            break;
        }
        std::stringstream ss;
        ss << fnHeader( "SmartContract " + strMessageTypeDesc + " message from " + address + ":" )
           << fnText( " " + rawString );

        switch ( nMessageType ) {
        case 0:
        default:  // normal message
            BOOST_LOG( getLogger( VerbosityInfo ) ) << ss.str();
            break;
        case 1:  // debug message
            BOOST_LOG( getLogger( VerbosityDebug ) ) << ss.str();
            break;
        case 2:  // trace message
            BOOST_LOG( getLogger( VerbosityTrace ) ) << ss.str();
            break;
        case 3:  // warning message
            BOOST_LOG( getLogger( VerbosityWarning ) ) << ss.str();
            break;
        case 4:  // error message
            BOOST_LOG( getLogger( VerbosityError ) ) << ss.str();
            break;
        case 5:  // fatal message
            BOOST_LOG( getLogger( VerbosityDebug ) ) << ss.str();
            break;
        }

        u256 code = 1;
        bytes response = toBigEndian( code );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/logTextMessage(): " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Unknown exception in precompiled/logTextMessage()\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}

/*
 * this precompiled contract is designed to get access to specific integer config values
 * and works as key / values map
 * input: bytes - length + path to config variable
 * output: bytes - config variable value
 *
 * variables available through this precompiled contract:
 * 1. id - node id for INDEX node in schain group for current block number
 * 2. schainIndex - schain index for INDEX node in schain group for current block number
 * to access those variables one should use the following scheme:
 * prefix=skaleConfig.sChain.nodes - to access corresponding structure inside skaled
 * index - node index user wants to get access to
 * field - the field user wants to request
 *
 * example:
 * to request the value for 1-st node (1 based) for the node id field the input should be
 * input=skaleConfig.sChain.nodes.0.id (inside skaled node indexes are 0 based)
 * so one should pass the following as calldata:
 * toBytes( input.length + toBytes(input) )
 */
ETH_REGISTER_PRECOMPILED( getConfigVariableUint256 )
( bytesConstRef _in, const PrecompiledCallContext& ) {
    try {
        size_t lengthName;
        std::string rawName;
        convertBytesToString( _in, 0, rawName, lengthName );
        if ( !statIsAccessibleJsonPath( rawName ) )
            throw std::runtime_error(
                "Security poicy violation, inaccessible configuration JSON path: " + rawName );

        if ( !g_configAccesssor )
            throw std::runtime_error( "Config accessor was not initialized" );

        std::string strValue;
        // call to skaleConfig.sChain.nodes means call to the historic data
        // need to proccess it in a different way
        // TODO Check if this precompiled can be called on historic block
        if ( isCallToHistoricData( rawName ) &&
             PrecompiledConfigPatch::isEnabledInWorkingBlock() ) {
            if ( !g_skaleHost )
                throw std::runtime_error( "SkaleHost accessor was not initialized" );

            std::string field;
            unsigned id;
            std::tie( field, id ) = parseHistoricFieldRequest( rawName );
            if ( field == "id" ) {
                strValue = g_skaleHost->getHistoricNodeId( id );
            } else if ( field == "schainIndex" ) {
                strValue = g_skaleHost->getHistoricNodeIndex( id );
            } else {
                throw std::runtime_error( "Incorrect config field" );
            }
        } else {
            nlohmann::json joConfig = g_configAccesssor->getConfigJSON();
            nlohmann::json joValue =
                skutils::json_config_file_accessor::stat_extract_at_path( joConfig, rawName );
            strValue = skutils::tools::trim_copy(
                joValue.is_string() ? joValue.get< std::string >() : joValue.dump() );
        }

        dev::u256 uValue = jsToInt( strValue );
        bytes response = toBigEndian( uValue );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/getConfigVariableUint256(): " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Unknown exception in precompiled/getConfigVariableUint256()\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}

ETH_REGISTER_PRECOMPILED( getConfigVariableAddress )
( bytesConstRef _in, const PrecompiledCallContext& ) {
    try {
        size_t lengthName;
        std::string rawName;
        convertBytesToString( _in, 0, rawName, lengthName );
        if ( !statIsAccessibleJsonPath( rawName ) )
            throw std::runtime_error(
                "Security poicy violation, inaccessible configuration JSON path: " + rawName );

        if ( !g_configAccesssor )
            throw std::runtime_error( "Config accessor was not initialized" );

        nlohmann::json joConfig = g_configAccesssor->getConfigJSON();
        nlohmann::json joValue =
            skutils::json_config_file_accessor::stat_extract_at_path( joConfig, rawName );
        std::string strValue = skutils::tools::trim_copy(
            joValue.is_string() ? joValue.get< std::string >() : joValue.dump() );

        dev::u256 uValue( strValue );
        bytes response = toBigEndian( uValue );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/getConfigVariableAddress(): " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Unknown exception in precompiled/getConfigVariableAddress()\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}

/*
 * this precompiled contract is designed to get access to specific config values that are
 * strings and works as key / values map input: bytes - length + path to config variable output:
 * bytes - config variable value
 *
 * variables available through this precompiled contract:
 * 1. publicKey - ETH public key for INDEX node in schain group for current block number
 * to access those variables one should use the following scheme:
 * prefix=skaleConfig.sChain.nodes - to access corresponding structure inside skaled
 * index - node index user wants to get access to
 * field - the field user wants to request
 *
 * example:
 * to request the value for 2-nd node (1 based) for the publicKey field the input should be
 * input=skaleConfig.sChain.nodes.1.publicKey (inside skaled node indexes are 0 based)
 * so one should pass the following as calldata
 * toBytes( input.length + toBytes(input) )
 */
ETH_REGISTER_PRECOMPILED( getConfigVariableString )
( bytesConstRef _in, const PrecompiledCallContext& ) {
    try {
        size_t lengthName;
        std::string rawName;
        convertBytesToString( _in, 0, rawName, lengthName );
        if ( !statIsAccessibleJsonPath( rawName ) )
            throw std::runtime_error(
                "Security poicy violation, inaccessible configuration JSON path: " + rawName );

        if ( !g_configAccesssor )
            throw std::runtime_error( "Config accessor was not initialized" );
        std::string strValue;
        // call to skaleConfig.sChain.nodes means call to the historic data
        // need to proccess it in a different way
        // TODO Check if this precompiled can be called on historic block
        if ( isCallToHistoricData( rawName ) &&
             PrecompiledConfigPatch::isEnabledInWorkingBlock() ) {
            if ( !g_skaleHost )
                throw std::runtime_error( "SkaleHost accessor was not initialized" );

            std::string field;
            unsigned id;
            std::tie( field, id ) = parseHistoricFieldRequest( rawName );
            if ( field == "publicKey" ) {
                strValue = g_skaleHost->getHistoricNodePublicKey( id );
            } else {
                throw std::runtime_error( "Incorrect config field" );
            }
        } else {
            nlohmann::json joConfig = g_configAccesssor->getConfigJSON();
            nlohmann::json joValue =
                skutils::json_config_file_accessor::stat_extract_at_path( joConfig, rawName );
            strValue = skutils::tools::trim_copy(
                joValue.is_string() ? joValue.get< std::string >() : joValue.dump() );
        }
        bytes response = dev::fromHex( strValue );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/getConfigVariableString(): " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Unknown exception in precompiled/getConfigVariableString()\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}


ETH_REGISTER_PRECOMPILED( fnReserved0x16 )( bytesConstRef, const PrecompiledCallContext& ) {
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}

ETH_REGISTER_PRECOMPILED( getConfigPermissionFlag )
( bytesConstRef _in, const PrecompiledCallContext& ) {
    try {
        dev::u256 uValue;
        uValue = 0;

        auto rawAddressParameter = _in.cropped( 12, 20 ).toBytes();
        std::string addressParameter;
        boost::algorithm::hex( rawAddressParameter.begin(), rawAddressParameter.end(),
            back_inserter( addressParameter ) );
        dev::u256 uParameter = statS2A( addressParameter );

        size_t lengthName;
        std::string rawName;
        convertBytesToString( _in, 32, rawName, lengthName );
        if ( !statIsAccessibleJsonPath( rawName ) )
            throw std::runtime_error(
                "Security poicy violation, inaccessible configuration JSON path: " + rawName );

        if ( !g_configAccesssor )
            throw std::runtime_error( "Config accessor was not initialized" );
        nlohmann::json joConfig = g_configAccesssor->getConfigJSON();
        nlohmann::json joValue =
            skutils::json_config_file_accessor::stat_extract_at_path( joConfig, rawName );
        if ( joValue.is_object() ) {
            auto itWalk = joValue.cbegin(), itEnd = joValue.cend();
            for ( ; itWalk != itEnd; ++itWalk ) {
                std::string strKey = itWalk.key();
                dev::u256 uKey = statS2A( strKey );
                if ( uKey == uParameter ) {
                    nlohmann::json joFlag = itWalk.value();
                    if ( joFlag.is_number_integer() ) {
                        if ( joFlag.get< int >() != 0 )
                            uValue = 1;
                    } else if ( joFlag.is_number_float() ) {
                        if ( joFlag.get< double >() != 0.0 )
                            uValue = 1;
                    } else if ( joFlag.is_boolean() ) {
                        if ( joFlag.get< bool >() )
                            uValue = 1;
                    }
                    break;
                }
            }
        }

        bytes response = toBigEndian( uValue );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/getConfigPermissionFlag(): " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Unknown exception in precompiled/getConfigPermissionFlag()\n";
    }
    dev::u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}
#endif

ETH_REGISTER_PRECOMPILED( getBlockRandom )( bytesConstRef, const PrecompiledCallContext& _ctx ) {
    try {
        if ( !g_skaleHost )
            throw std::runtime_error( "SkaleHost accessor was not initialized" );
        unsigned blockNumberToCall = _ctx.blockNumber.convert_to< unsigned >();
        dev::u256 uValue = g_skaleHost->getBlockRandom( blockNumberToCall, !_ctx.isReadOnly );
        bytes response = toBigEndian( uValue );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/getBlockRandom(): " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Unknown exception in precompiled/getBlockRandom()\n";
    }
    dev::u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}

#ifdef BITE2

ETH_REGISTER_PRECOMPILED( submitCTX )( bytesConstRef _in, const PrecompiledCallContext& _ctx ) {
    try {
        // cannot be called from read-only context (e.g. eth_call, eth_estimateGas,
        // debug_traceTransaction) simply return success here
        if ( _ctx.isReadOnly )
            return { true, toBigEndian( dev::u256( SubmitCTXStatus::SUCCESS ) ) };

        // Parse ABI-encoded input: abi.encode(bytes walletAndSignature, address destination,
        // uint256 gasLimit, bytes data) walletAndSignature = wallet_address(20) + r(32) + s(32) +
        // v(32) = 116 bytes Format: offset_to_walletAndSignature(32) + destination(32) +
        // gasLimit(32) + offset_to_data(32) + walletAndSignature_data(116) + data_data

        if ( _in.size() < BITE2_TRANSACTION_SUBMITION_INPUT_DATA_MIN_LEN )
            return { false, toBigEndian( dev::u256( SubmitCTXStatus::INPUT_TOO_SHORT ) ) };

        // Read offset to walletAndSignature
        bigint const walletAndSigOffset( parseBigEndianRightPadded( _in, 0, dev::h256::size ) );

        // Extract destination address from second 32 bytes (skip first 12 bytes of padding)
        dev::Address destination( _in.cropped( dev::h256::size + 12, dev::Address::size ) );
        if ( destination == dev::ZeroAddress )
            return { false, toBigEndian( dev::u256( SubmitCTXStatus::INVALID_DESTINATION ) ) };

        // Extract gas limit from third 32 bytes
        bigint const gas( parseBigEndianRightPadded( _in, 2 * dev::h256::size, dev::h256::size ) );
        if ( gas <= 0 )
            return { false, toBigEndian( dev::u256( SubmitCTXStatus::INVALID_GAS_LIMIT ) ) };

        // Read offset to data from fourth 32 bytes
        bigint const dataOffset(
            parseBigEndianRightPadded( _in, 3 * dev::h256::size, dev::h256::size ) );

        // Extract walletAndSignature data at the offset (has length prefix)
        if ( _in.size() < walletAndSigOffset.convert_to< size_t >() + dev::h256::size )
            return { false,
                toBigEndian( dev::u256( SubmitCTXStatus::WALLET_AND_SIG_OFFSET_OUT_OF_BOUNDS ) ) };

        bigint const walletAndSigLength(
            parseBigEndianRightPadded( _in, walletAndSigOffset, dev::h256::size ) );
        if ( walletAndSigLength != WALLET_AND_SIGNATURE_LENGTH )
            return { false,
                toBigEndian( dev::u256( SubmitCTXStatus::INVALID_WALLET_AND_SIG_LENGTH ) ) };

        if ( _in.size() < walletAndSigOffset.convert_to< size_t >() + dev::h256::size +
                              WALLET_AND_SIGNATURE_LENGTH )
            return { false,
                toBigEndian( dev::u256( SubmitCTXStatus::WALLET_AND_SIG_DATA_TOO_SHORT ) ) };

        dev::bytes walletAndSigBytes =
            _in.cropped( walletAndSigOffset.convert_to< size_t >() + dev::h256::size,
                   WALLET_AND_SIGNATURE_LENGTH )
                .toBytes();

        // Extract wallet address (first 20 bytes)
        dev::Address walletAddress( dev::bytes(
            walletAndSigBytes.begin(), walletAndSigBytes.begin() + dev::Address::size ) );
        if ( walletAddress == dev::ZeroAddress )
            return { false, toBigEndian( dev::u256( SubmitCTXStatus::INVALID_WALLET_ADDRESS ) ) };
        // verify account is not active
        if ( g_skaleHost->client().countAt( walletAddress ) > 0 )
            return { false, toBigEndian( dev::u256( SubmitCTXStatus::WALLET_ALREADY_ACTIVE ) ) };

        // Parse signature from remaining bytes: r(32), s(32), v(32)
        dev::h256 r( dev::bytes( walletAndSigBytes.begin() + dev::Address::size,
            walletAndSigBytes.begin() + dev::Address::size + dev::h256::size ) );
        dev::h256 s( dev::bytes( walletAndSigBytes.begin() + dev::Address::size + dev::h256::size,
            walletAndSigBytes.begin() + dev::Address::size + 2 * dev::h256::size ) );
        dev::h256 vBytes(
            dev::bytes( walletAndSigBytes.begin() + dev::Address::size + 2 * dev::h256::size,
                walletAndSigBytes.begin() + WALLET_AND_SIGNATURE_LENGTH ) );
        _byte_ v = dev::h256::Arith( vBytes ).convert_to< _byte_ >();

        SignatureStruct signature( r, s, v );
        if ( !signature.isValid() )
            return { false, toBigEndian( dev::u256( SubmitCTXStatus::INVALID_SIGNATURE ) ) };

        // Extract transaction data at the offset (has length prefix)
        if ( _in.size() < dataOffset.convert_to< size_t >() + dev::h256::size )
            return { false,
                toBigEndian( dev::u256( SubmitCTXStatus::DATA_OFFSET_OUT_OF_BOUNDS ) ) };

        bigint const dataLength( parseBigEndianRightPadded( _in, dataOffset, dev::h256::size ) );
        if ( _in.size() < dataOffset.convert_to< size_t >() + dev::h256::size +
                              dataLength.convert_to< size_t >() )
            return { false, toBigEndian( dev::u256( SubmitCTXStatus::DATA_TOO_SHORT ) ) };

        dev::bytes txnData = _in.cropped( dataOffset.convert_to< size_t >() + dev::h256::size,
                                    dataLength.convert_to< size_t >() )
                                 .toBytes();

        // Convert ABI-encoded data to RLP
        dev::bytes rlpEncodedData;
        size_t encryptedArgsCount = 0;
        try {
            auto [rlpData, count] = abiEncodedArraysToRlp( txnData );
            rlpEncodedData = std::move( rlpData );
            encryptedArgsCount = count;
        } catch ( std::exception& ex ) {
            std::string strError = ex.what();
            if ( strError.empty() )
                strError = "exception without description";
            BOOST_LOG( getLogger( VerbosityError ) )
                << "Exception in precompiled/submitCTX/abiEncodedArraysToRlp(): " << strError
                << "\n";
            return { false,
                toBigEndian( dev::u256( SubmitCTXStatus::ABI_TO_RLP_CONVERSION_FAILED ) ) };
        } catch ( ... ) {
            BOOST_LOG( getLogger( VerbosityError ) )
                << "Unknown exception in precompiled/submitCTX/abiEncodedArraysToRlp()\n";
            return { false, toBigEndian( dev::u256( SubmitCTXStatus::ABI_TO_RLP_UNKNOWN_ERROR ) ) };
        }

        // add onDecrypt function selector at the beginning
        rlpEncodedData.insert( rlpEncodedData.begin(), ON_DECRYPT_FUNCTION_SELECTOR.begin(),
            ON_DECRYPT_FUNCTION_SELECTOR.end() );

        // Construct signed transaction from RLP
        RLPStream rlpStream;
        rlpStream.appendList( 9 );  // nonce, gasPrice, gas, to, value, data, v, r, s
        rlpStream << 0 << g_skaleHost->getGasPrice() << gas.convert_to< dev::u256 >();
        rlpStream << destination << 0 << rlpEncodedData;
        rlpStream << signature.v + 27 << signature.r << signature.s;

        dev::bytes signedTxnRlp = rlpStream.out();

        // Construct transaction from RLP
        Transaction signedTransaction( signedTxnRlp, CheckTransaction::Everything );
        signedTransaction.setBITE2EncryptedArgsSize( encryptedArgsCount );

        if ( signedTransaction.isInvalid() )
            return { false, toBigEndian( dev::u256( SubmitCTXStatus::INVALID_TRANSACTION ) ) };

        // push txn to BITE2 queue
        g_skaleHost->pushToBITE2Queue( std::move( signedTransaction ) );

        // Return success
        return { true, toBigEndian( dev::u256( SubmitCTXStatus::SUCCESS ) ) };

    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/submitCTX(): " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Unknown exception in precompiled/submitCTX()\n";
    }
    dev::u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}
#endif

#ifndef FAIR
ETH_REGISTER_PRECOMPILED( addBalance )
( [[maybe_unused]] bytesConstRef _in, const PrecompiledCallContext& ) {
    dev::u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}

ETH_REGISTER_PRECOMPILED( getIMABLSPublicKey )( bytesConstRef, const PrecompiledCallContext& ) {
    try {
        if ( !g_skaleHost )
            throw std::runtime_error( "SkaleHost accessor was not initialized" );
        auto imaBLSPublicKey = g_skaleHost->getCurrentBLSPublicKey();
        bytes response = toBigEndian( dev::u256( imaBLSPublicKey[0] ) ) +
                         toBigEndian( dev::u256( imaBLSPublicKey[1] ) ) +
                         toBigEndian( dev::u256( imaBLSPublicKey[2] ) ) +
                         toBigEndian( dev::u256( imaBLSPublicKey[3] ) );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/getCurrentBLSPublicKey(): " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Unknown exception in precompiled/getCurrentBLSPublicKey()\n";
    }
    dev::u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}
#endif

}  // namespace
