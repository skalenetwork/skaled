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

#ifdef BITE
#include <libconsensus/libBLS/threshold_encryption/TEPublicKey.h>
#include <libconsensus/libBLS/threshold_encryption/ThresholdEncryption.h>
#include <libdevcore/RLP.h>
#include <libethcore/BITECommon.h>
#endif

#include "PrecompiledHelpers.h"

#include <cryptopp/files.h>
#include <cryptopp/hex.h>
#include <cryptopp/sha.h>
#include <libdevcore/CommonJS.h>
#include <libdevcore/FileSystem.h>
#include <libdevcore/Log.h>
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

#ifdef BITE
using namespace dev::bite;
#endif

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

#ifdef BITE

ETH_REGISTER_PRECOMPILED( submitCTX )( bytesConstRef _in, const PrecompiledCallContext& _ctx ) {
    try {
        // Parse ABI-encoded input: abi.encode(uint256 gasLimit, bytes data)
        // Format: gasLimit(32) + offset_to_data(32) + data_length(32) + data_bytes

        if ( _in.size() < 3 * dev::h256::size )
            return { false, toBigEndian( dev::u256( SubmitCTXStatus::INPUT_TOO_SHORT ) ) };

        // Get destination address from context
        dev::Address destination = _ctx.from;
        if ( destination == dev::ZeroAddress )
            return { false, toBigEndian( dev::u256( SubmitCTXStatus::INVALID_DESTINATION ) ) };

        // Extract gas limit from first 32 bytes
        bigint const gas( parseBigEndianRightPadded( _in, 0, dev::h256::size ) );
        if ( gas <= 0 )
            return { false, toBigEndian( dev::u256( SubmitCTXStatus::INVALID_GAS_LIMIT ) ) };

        // Read offset to data from second 32 bytes
        bigint const dataOffset(
            parseBigEndianRightPadded( _in, dev::h256::size, dev::h256::size ) );

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
        uint64_t epochId = _ctx.isReadOnly ? g_skaleHost->client().getGroupIndexForBlockNumber(
                                                 _ctx.latestBlockTimestamp ) :
                                             g_skaleHost->client().getCurrentEpochId();
        try {
            dev::bite::BITEVerificationData verificationData{ epochId, destination.asBytes() };
            auto [rlpData, count] = abiEncodedArraysToRlp( txnData, verificationData );
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

        // Get block random to generate signature
        PrecompiledExecutor exec = PrecompiledRegistrar::executor( "getBlockRandom" );
        auto rngPrecompiledResponse = exec( bytesConstRef(), _ctx );
        // if call to getBlockRandom() fails, return error
        if ( !rngPrecompiledResponse.first )
            return { false, toBigEndian( dev::u256( SubmitCTXStatus::INVALID_SIGNATURE ) ) };

        // generate a signature based on block random and txn index
        SignatureStruct signature =
            dev::makeSignature( rngPrecompiledResponse.second, _ctx.currentTxnIndex );

        // Construct signed transaction from RLP
        RLPStream rlpStream;
        rlpStream.appendList( 9 );  // nonce, gasPrice, gas, to, value, data, v, r, s
        rlpStream << 0 << g_skaleHost->getGasPrice() << gas.convert_to< dev::u256 >();
        rlpStream << destination << 0 << rlpEncodedData;
        rlpStream << signature.v + 27 << dev::u256( signature.r ) << dev::u256( signature.s );

        dev::bytes signedTxnRlp = rlpStream.out();

        // Construct transaction from RLP
        Transaction signedTransaction( signedTxnRlp, CheckTransaction::None );
        signedTransaction.setBITE2EncryptedArgsSize( encryptedArgsCount );

        if ( signedTransaction.isInvalid() )
            return { false, toBigEndian( dev::u256( SubmitCTXStatus::INVALID_TRANSACTION ) ) };

        // Get sender address before moving the transaction
        dev::Address senderAddress = signedTransaction.sender();

        // state must not be changed as a result of executing external calls
        // (e.g. eth_call, eth_estimateGasm, debug_traceBlock)
        // skip adding CTX to BITE2 queue for external calls
        bytes response = senderAddress.asBytes();
        if ( _ctx.isReadOnly ) {
            return { true, response };
        } else {
            // push txn to BITE2 queue
            g_skaleHost->addTempBITE2Transaction( std::move( signedTransaction ) );
        }

        // Return sender address
        return { true, response };

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

ETH_REGISTER_PRECOMPILED( getRandomWalletAndSignatureForCTX )
( bytesConstRef _in, const PrecompiledCallContext& _ctx ) {
    try {
        PrecompiledExecutor exec = PrecompiledRegistrar::executor( "getBlockRandom" );
        auto rngPrecompiledResponse = exec( _in, _ctx );
        // if call to getBlockRandom() fails, return error
        if ( !rngPrecompiledResponse.first )
            return rngPrecompiledResponse;

        // generate a signature based on block random and txn index
        SignatureStruct vrs =
            dev::makeSignature( rngPrecompiledResponse.second, _ctx.currentTxnIndex );

        // Parse ABI-encoded input: abi.encode(address destination, uint256 gasLimit, bytes data)
        // Format: address_value(32) + gasLimit_value(32) + offset_to_bytes(32) + bytes_length(32) +
        // bytes_data ( at least BITE_CIPHERTEXT_MIN_LEN ) bytes
        if ( _in.size() < BITE2_WALLET_GENERATION_INPUT_DATA_MIN_LEN )
            return { false, toBigEndian( dev::u256( GetRandomWalletStatus::INPUT_TOO_SHORT ) ) };

        // Extract address from first 32 bytes (skip first 12 bytes of padding)
        dev::Address destination( _in.cropped( 12, dev::Address::size ) );
        if ( destination == dev::ZeroAddress )
            return { false,
                toBigEndian( dev::u256( GetRandomWalletStatus::INVALID_DESTINATION ) ) };

        // Extract gas limit from next 32 bytes
        bigint const gas( parseBigEndianRightPadded( _in, dev::h256::size, dev::h256::size ) );

        // Read offset to bytes data from third 32 bytes
        bigint const dataOffset(
            parseBigEndianRightPadded( _in, 2 * dev::h256::size, dev::h256::size ) );

        // Extract bytes data at the offset (has length prefix)
        if ( _in.size() < dataOffset.convert_to< size_t >() + dev::h256::size )
            return { false,
                toBigEndian( dev::u256( GetRandomWalletStatus::DATA_OFFSET_OUT_OF_BOUNDS ) ) };

        bigint const dataLength( parseBigEndianRightPadded( _in, dataOffset, dev::h256::size ) );
        if ( _in.size() < dataOffset.convert_to< size_t >() + dev::h256::size +
                              dataLength.convert_to< size_t >() )
            return { false, toBigEndian( dev::u256( GetRandomWalletStatus::DATA_TOO_SHORT ) ) };

        dev::bytes data = _in.cropped( dataOffset.convert_to< size_t >() + dev::h256::size,
                                 dataLength.convert_to< size_t >() )
                              .toBytes();
        if ( data.empty() )
            return { false, toBigEndian( dev::u256( GetRandomWalletStatus::EMPTY_DATA ) ) };

        dev::bytes rlpEncodedData;
        size_t encryptedArgsCount = 0;
        try {
            dev::bite::BITEVerificationData verificationData{ 0, destination.asBytes() };
            auto [rlpData, count] = abiEncodedArraysToRlp( data, verificationData );
            rlpEncodedData = std::move( rlpData );
            encryptedArgsCount = count;
        } catch ( std::exception& ex ) {
            std::string strError = ex.what();
            if ( strError.empty() )
                strError = "exception without description";
            BOOST_LOG( getLogger( VerbosityError ) )
                << "Exception in precompiled/getRandomWalletForCTX/abiEncodedArraysToRlp(): "
                << strError << "\n";
            return { false,
                toBigEndian( dev::u256( GetRandomWalletStatus::ABI_TO_RLP_CONVERSION_FAILED ) ) };
        } catch ( ... ) {
            BOOST_LOG( getLogger( VerbosityError ) )
                << "Unknown exception in "
                   "precompiled/getRandomWalletForCTX/abiEncodedArraysToRlp()\n";
            return { false,
                toBigEndian( dev::u256( GetRandomWalletStatus::ABI_TO_RLP_UNKNOWN_ERROR ) ) };
        }

        // add onDecrypt function selector at the beginning
        rlpEncodedData.insert( rlpEncodedData.begin(), ON_DECRYPT_FUNCTION_SELECTOR.begin(),
            ON_DECRYPT_FUNCTION_SELECTOR.end() );

        // validate gasLimit
        auto evmSchedule = g_skaleHost->client().chainParams().makeEvmSchedule(
            _ctx.latestBlockTimestamp, _ctx.blockNumber );
        if ( TransactionBase::baseGasRequired( false,
                 dev::bytesConstRef( rlpEncodedData.data(), rlpEncodedData.size() ), evmSchedule,
                 false, encryptedArgsCount ) > gas )
            return { false,
                toBigEndian( dev::u256( GetRandomWalletStatus::INSUFFICIENT_GAS_LIMIT ) ) };

        dev::u256 gasPrice =
            g_skaleHost->getGasPrice( _ctx.blockNumber.convert_to< BlockNumber >() );

        // construct unsigned transaction and calculate its hash
        Transaction sampleTransaction(
            0, gasPrice, gas.convert_to< dev::u256 >(), destination, rlpEncodedData, 0 );
        if ( sampleTransaction.isInvalid() )
            return { false,
                toBigEndian( dev::u256( GetRandomWalletStatus::INVALID_TRANSACTION ) ) };

        dev::h256 txnHash = sampleTransaction.sha3( dev::eth::WithoutSignature );

        dev::Public publicKey = recover( vrs, txnHash );

        dev::Address walletAddress = dev::toAddress( publicKey );
        // verify account is not active
        if ( g_skaleHost->client().countAt( walletAddress ) > 0 )
            return { false,
                toBigEndian( dev::u256( GetRandomWalletStatus::WALLET_ALREADY_ACTIVE ) ) };

        // Encode response: address(20 bytes) + r(32 bytes) + s(32 bytes) + v(32 bytes)
        dev::bytes response;
        dev::bytes addressBytes = walletAddress.asBytes();
        response.insert( response.end(), addressBytes.begin(), addressBytes.end() );

        dev::bytes rBytes = toBigEndian( dev::u256( vrs.r ) );
        response.insert( response.end(), rBytes.begin(), rBytes.end() );

        dev::bytes sBytes = toBigEndian( dev::u256( vrs.s ) );
        response.insert( response.end(), sBytes.begin(), sBytes.end() );

        dev::bytes vBytes = toBigEndian( dev::u256( vrs.v ) );
        response.insert( response.end(), vBytes.begin(), vBytes.end() );

        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/getRandomWalletForCTX(): " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Unknown exception in precompiled/getRandomWalletForCTX()\n";
    }
    dev::u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}

ETH_REGISTER_PRECOMPILED( encryptTE )
( bytesConstRef _in, const PrecompiledCallContext& _ctx ) {
    try {
        static constexpr size_t MAX_SIZE_BYTES = 64 * 1024;  // 64KB
        if ( _in.size() > MAX_SIZE_BYTES ) {
            return { false, toBigEndian( dev::u256( 1 ) ) };  // error 1: input too large
        }

        // Input format: abi.encode(bytes data)
        // ABI encoding: [offset_to_data(32)] [data_length(32)] [data(N)]
        // Minimum: 32 (offset) + 32 (length) = 64 bytes
        if ( _in.size() < 64 ) {
            return { false, toBigEndian( dev::u256( 2 ) ) };  // error 2: input too small
        }

        // ABI encoding requires input to be a multiple of 32 bytes
        if ( _in.size() % 32 != 0 ) {
            return { false, toBigEndian( dev::u256( 3 ) ) };  // error 3: input not 32-byte aligned
        }

        if ( !g_skaleHost ) {
            throw std::runtime_error( "SkaleHost accessor was not initialized" );
        }

        size_t headFieldSizeBytes = 32;

        // First 32 bytes: offset to data (should be 32)
        bigint const dataOffset( parseBigEndianRightPadded( _in, 0, headFieldSizeBytes ) );
        const size_t expectedDataOffset = 32;
        if ( dataOffset != expectedDataOffset ) {
            return { false, toBigEndian( dev::u256( 5 ) ) };  // error 5: invalid data offset
        }

        // Read data length at the data offset (position 32)
        if ( _in.size() < expectedDataOffset + headFieldSizeBytes ) {
            return { false, toBigEndian( dev::u256( 6 ) ) };  // error 6: data length mismatch
        }
        bigint const dataLength(
            parseBigEndianRightPadded( _in, expectedDataOffset, headFieldSizeBytes ) );

        // Validate dataLength is non-negative and fits within reasonable bounds
        // Header is 64 bytes (offset + length field), so max data = MAX_SIZE_BYTES - 64
        static constexpr size_t HEADER_SIZE_BYTES = 64;
        if ( dataLength < 0 || dataLength > MAX_SIZE_BYTES - HEADER_SIZE_BYTES ) {
            return { false, toBigEndian( dev::u256( 6 ) ) };  // error 6: data length mismatch
        }
        size_t dataLengthSafe = static_cast< size_t >( dataLength );

        // Calculate data start position and validate bounds
        size_t dataStart = expectedDataOffset + headFieldSizeBytes;
        if ( dataStart + dataLengthSafe > _in.size() ) {
            return { false, toBigEndian( dev::u256( 6 ) ) };  // error 6: data length mismatch
        }

        // Extract data bytes (empty data is allowed)
        std::vector< uint8_t > dataToEncrypt = _in.cropped( dataStart, dataLengthSafe ).toBytes();

        // Validate trailing padding bytes are all zeros (ABI compliance)
        size_t dataEnd = dataStart + dataLengthSafe;
        if ( !std::all_of( _in.data() + dataEnd, _in.data() + _in.size(),
                 []( uint8_t b ) { return b == 0; } ) ) {
            return { false, toBigEndian( dev::u256( 7 ) ) };  // error 7: trailing padding not zeros
        }

        std::vector< libBLS::TEPublicKey > publicKeys;

        // get network public key
        auto blsPublicKeyArray = g_skaleHost->getCurrentBLSPublicKey();

        // convert BLS public key to TE public key
        libBLS::algebra::G2Point publicKeyG2 =
            libBLS::algebra::G2Point::fromString( blsPublicKeyArray, libBLS::Base::DEC );
        publicKeys.emplace_back( publicKeyG2 );

        // Check if committee rotation is soon
        if ( g_skaleHost->client().isCommitteeRotationSoon() ) {
            auto nextCommitteeInfo = g_skaleHost->client().getNextCommitteeBITEInfo();
            // nextCommitteeInfo.first is the public key array
            auto nextBlsPublicKeyArray = nextCommitteeInfo.first;
            libBLS::algebra::G2Point nextPublicKeyG2 =
                libBLS::algebra::G2Point::fromString( nextBlsPublicKeyArray, libBLS::Base::DEC );
            publicKeys.emplace_back( nextPublicKeyG2 );
        }

        // Get deterministic random value for this encryption call
        // SkaleHost handles: Hash(blockRandom || counter)
        unsigned blockNumberToCall = _ctx.blockNumber.convert_to< unsigned >();
        dev::u256 encryptionRandom =
            g_skaleHost->getEncryptionCallRandom( blockNumberToCall, !_ctx.isReadOnly );
        bytes encryptionRandomBytes = toBigEndian( encryptionRandom );

        // Create seed from encryption random (32 bytes)
        h256 seed( encryptionRandomBytes.data(), h256::ConstructFromPointer );

        // Create seed array from encryption random (32 bytes)
        std::array< uint8_t, libBLS::AES_256_KEY_SIZE_BYTES > seedArray;
        std::copy_n( seed.begin(), libBLS::AES_256_KEY_SIZE_BYTES, seedArray.begin() );
        // Use caller's address as the associated data for TE
        auto scAddressBytes = _ctx.from.asBytes();

        // Build EncryptMetaData with seed and SC address as TE AAD
        libBLS::EncryptMetaData metaData;
        metaData.seed = libBLS::Seed256{ seedArray };
        metaData.associatedDataTE =
            std::vector< uint8_t >( scAddressBytes.begin(), scAddressBytes.end() );

        // encrypt using threshold encryption
        libBLS::Ciphertext ciphertext =
            libBLS::ThresholdEncryption::encrypt( dataToEncrypt, publicKeys, metaData );

        // Return: RLP List [epochId, ciphertext]
        uint64_t epochId = g_skaleHost->client().getCurrentEpochId();

        RLPStream rlpStream;
        rlpStream.appendList( 2 );
        rlpStream.append( epochId );
        rlpStream.append( ciphertext.toBytes() );

        bytes response = rlpStream.out();
        return { true, response };

    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/encryptTE(): " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Unknown exception in precompiled/encryptTE()\n";
    }

    dev::u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };
}


ETH_REGISTER_PRECOMPILED( encryptECIES )
( bytesConstRef _in, const PrecompiledCallContext& _ctx ) {
    try {
        static constexpr size_t MAX_SIZE_BYTES = 64 * 1024;  // 64KB

        if ( _in.size() > MAX_SIZE_BYTES ) {
            return { false, toBigEndian( dev::u256( 1 ) ) };  // error 1: input too large
        }

        // Minimum input: at least public key coordinates (64 bytes)
        // Input format: abi.encode(bytes data, bytes32 x, bytes32 y)
        // ABI encoding: [offset_to_data(32)] [x(32)] [y(32)] [data_length(32)] [data(N)]
        // So minimum is: 32 + 32 + 32 + 32 = 128 bytes for empty data
        if ( _in.size() < 128 ) {
            return { false, toBigEndian( dev::u256( 2 ) ) };  // error 2: input too small
        }

        // ABI encoding requires input to be a multiple of 32 bytes
        if ( _in.size() % 32 != 0 ) {
            return { false, toBigEndian( dev::u256( 3 ) ) };  // error 3: input not 32-byte aligned
        }

        if ( !g_skaleHost ) {
            throw std::runtime_error( "SkaleHost accessor was not initialized" );
        }

        size_t offset = 0;
        size_t headFieldSizeBytes = 32;

        // Parse ABI-encoded input
        // First 32 bytes: offset to data
        bigint const dataOffset( parseBigEndianRightPadded( _in, offset, headFieldSizeBytes ) );
        // should be 3 * 32 = 96
        const size_t expectedDataOffset = 3 * 32;
        if ( dataOffset != expectedDataOffset ) {
            return { false, toBigEndian( dev::u256( 4 ) ) };  // error 4: invalid data offset
        }

        // Next 32 bytes: public key x-coordinate
        offset += headFieldSizeBytes;
        bytes pubKeyX = _in.cropped( offset, headFieldSizeBytes ).toBytes();

        // Next 32 bytes: public key y-coordinate
        offset += headFieldSizeBytes;
        bytes pubKeyY = _in.cropped( offset, headFieldSizeBytes ).toBytes();

        // Next 32 bytes: data length
        offset += headFieldSizeBytes;
        bigint const dataLength( parseBigEndianRightPadded( _in, offset, headFieldSizeBytes ) );

        // Validate dataLength is non-negative and fits within reasonable bounds
        // Header is 128 bytes (offset + x + y + length field), so max data = MAX_SIZE_BYTES - 128
        static constexpr size_t HEADER_SIZE_BYTES = 128;
        if ( dataLength < 0 || dataLength > MAX_SIZE_BYTES - HEADER_SIZE_BYTES ) {
            return { false, toBigEndian( dev::u256( 5 ) ) };  // error 5: data length mismatch
        }
        size_t dataLengthSafe = static_cast< size_t >( dataLength );

        // 4 header fields (offset, x, y, length) = 128 bytes
        size_t dataStart = 4 * headFieldSizeBytes;
        if ( dataStart + dataLengthSafe > _in.size() ) {
            return { false, toBigEndian( dev::u256( 5 ) ) };  // error 5: data length mismatch
        }

        // Extract data to encrypt
        bytes dataToEncrypt;
        if ( dataLengthSafe > 0 ) {
            dataToEncrypt = _in.cropped( dataStart, dataLengthSafe ).toBytes();
        }

        // Validate trailing padding bytes are all zeros (ABI compliance)
        size_t dataEnd = dataStart + dataLengthSafe;
        if ( !std::all_of( _in.data() + dataEnd, _in.data() + _in.size(),
                 []( uint8_t b ) { return b == 0; } ) ) {
            return { false, toBigEndian( dev::u256( 6 ) ) };  // error 6: trailing padding not zeros
        }

        // Construct user public key from x,y bytes
        dev::Public userPubKey;
        memcpy( userPubKey.data(), pubKeyX.data(), 32 );
        memcpy( userPubKey.data() + 32, pubKeyY.data(), 32 );

        // Validate public key is on the secp256k1 curve
        if ( !dev::isValidPublicKey( userPubKey ) ) {
            return { false, toBigEndian( dev::u256( 7 ) ) };  // error 7: invalid public key
        }

        // Get deterministic random value for this encryption call
        // SkaleHost handles: Hash(blockRandom || counter)
        unsigned blockNumberToCall = _ctx.blockNumber.convert_to< unsigned >();
        dev::u256 encryptionRandom =
            g_skaleHost->getEncryptionCallRandom( blockNumberToCall, !_ctx.isReadOnly );
        bytes encryptionRandomBytes = toBigEndian( encryptionRandom );

        // Create seed from encryption random (32 bytes)
        h256 seed( encryptionRandomBytes.data(), h256::ConstructFromPointer );

        // Encrypt using ECIES-CBC with deterministic IV based on encryption random
        bytes response =
            dev::encryptECIES_CBC( userPubKey, bytesConstRef( &dataToEncrypt ), &seed );
        if ( response.empty() ) {
            return { false, toBigEndian( dev::u256( 8 ) ) };  // error 8: encryption failed
        }

        return { true, response };

    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/encryptECIES(): " << strError << "\n";
    } catch ( ... ) {
        BOOST_LOG( getLogger( VerbosityError ) )
            << "Unknown exception in precompiled/encryptECIES()\n";
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
