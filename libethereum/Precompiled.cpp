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

ETH_REGISTER_PRECOMPILED( ecrecover )( bytesConstRef _in ) {
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

ETH_REGISTER_PRECOMPILED( sha256 )( bytesConstRef _in ) {
    return { true, dev::sha256( _in ).asBytes() };
}

ETH_REGISTER_PRECOMPILED( ripemd160 )( bytesConstRef _in ) {
    return { true, h256( dev::ripemd160( _in ), h256::AlignRight ).asBytes() };
}

ETH_REGISTER_PRECOMPILED( identity )( bytesConstRef _in ) {
    MICROPROFILE_SCOPEI( "VM", "identity", MP_RED );
    return { true, _in.toBytes() };
}

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

ETH_REGISTER_PRECOMPILED( modexp )( bytesConstRef _in ) {
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

ETH_REGISTER_PRECOMPILED( alt_bn128_G1_add )( bytesConstRef _in ) {
    return dev::crypto::alt_bn128_G1_add( _in );
}

ETH_REGISTER_PRECOMPILED_PRICER( alt_bn128_G1_add )
( bytesConstRef /*_in*/, ChainOperationParams const& _chainParams, u256 const& _blockNumber ) {
    return _blockNumber < _chainParams.istanbulForkBlock ? 500 : 150;
}

ETH_REGISTER_PRECOMPILED( alt_bn128_G1_mul )( bytesConstRef _in ) {
    return dev::crypto::alt_bn128_G1_mul( _in );
}

ETH_REGISTER_PRECOMPILED_PRICER( alt_bn128_G1_mul )
( bytesConstRef /*_in*/, ChainOperationParams const& _chainParams, u256 const& _blockNumber ) {
    return _blockNumber < _chainParams.istanbulForkBlock ? 40000 : 6000;
}

ETH_REGISTER_PRECOMPILED( alt_bn128_pairing_product )( bytesConstRef _in ) {
    return dev::crypto::alt_bn128_pairing_product( _in );
}

ETH_REGISTER_PRECOMPILED_PRICER( alt_bn128_pairing_product )
( bytesConstRef _in, ChainOperationParams const& _chainParams, u256 const& _blockNumber ) {
    auto const k = _in.size() / 192;
    return _blockNumber < _chainParams.istanbulForkBlock ? 100000 + k * 80000 : 45000 + k * 34000;
}

static Logger& getLogger( int a_severity = VerbosityTrace ) {
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

static void convertBytesToString(
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

static size_t stat_compute_file_size( const char* _strFileName ) {
    std::ifstream file( _strFileName, ios::binary );
    file.exceptions( std::ifstream::failbit | std::ifstream::badbit );
    file.seekg( 0, std::ios::end );
    size_t n = size_t( file.tellg() );
    return n;
}

boost::filesystem::path getFileStorageDir( const Address& _address ) {
    return dev::getDataDir() / "filestorage" / _address.hex();
}

// TODO: check file name and file existance
ETH_REGISTER_FS_PRECOMPILED( createFile )( bytesConstRef _in, skale::OverlayFS* _overlayFS ) {
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
        LOG( getLogger( VerbosityError ) ) << "Exception in createFile: " << strError << "\n";
    } catch ( ... ) {
        LOG( getLogger( VerbosityError ) ) << "Unknown exception in createFile\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };
}

ETH_REGISTER_FS_PRECOMPILED( uploadChunk )( bytesConstRef _in, skale::OverlayFS* _overlayFS ) {
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
        if ( position + dataLength > stat_compute_file_size( filePath.c_str() ) ) {
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
        LOG( getLogger( VerbosityError ) ) << "Exception in uploadChunk: " << strError << "\n";
    } catch ( ... ) {
        LOG( getLogger( VerbosityError ) ) << "Unknown exception in uploadChunk\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };
}

ETH_REGISTER_PRECOMPILED( readChunk )( bytesConstRef _in ) {
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
        if ( position > stat_compute_file_size( filePath.c_str() ) ||
             position + chunkLength > stat_compute_file_size( filePath.c_str() ) ) {
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
        LOG( getLogger( VerbosityError ) ) << "Exception in readChunk: " << strError << "\n";
    } catch ( ... ) {
        LOG( getLogger( VerbosityError ) ) << "Unknown exception in readChunk\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };
}

ETH_REGISTER_PRECOMPILED( getFileSize )( bytesConstRef _in ) {
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

        size_t const fileSize = stat_compute_file_size( filePath.c_str() );
        bytes response = toBigEndian( static_cast< u256 >( fileSize ) );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        LOG( getLogger( VerbosityError ) ) << "Exception in getFileSize: " << strError << "\n";
    } catch ( ... ) {
        LOG( getLogger( VerbosityError ) ) << "Unknown exception in getFileSize\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };
}

ETH_REGISTER_FS_PRECOMPILED( deleteFile )( bytesConstRef _in, skale::OverlayFS* _overlayFS ) {
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
        LOG( getLogger( VerbosityError ) ) << "Exception in deleteFile: " << strError << "\n";
    } catch ( ... ) {
        LOG( getLogger( VerbosityError ) ) << "Unknown exception in deleteFile\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };
}

ETH_REGISTER_FS_PRECOMPILED( createDirectory )( bytesConstRef _in, skale::OverlayFS* _overlayFS ) {
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
        LOG( getLogger( VerbosityError ) ) << "Exception in createDirectory: " << strError << "\n";
    } catch ( ... ) {
        LOG( getLogger( VerbosityError ) ) << "Unknown exception in createDirectory\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };
}

ETH_REGISTER_FS_PRECOMPILED( deleteDirectory )( bytesConstRef _in, skale::OverlayFS* _overlayFS ) {
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
        LOG( getLogger( VerbosityError ) ) << "Exception in deleteDirectory: " << strError << "\n";
    } catch ( ... ) {
        LOG( getLogger( VerbosityError ) ) << "Unknown exception in deleteDirectory\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };
}

ETH_REGISTER_FS_PRECOMPILED( calculateFileHash )
( bytesConstRef _in, skale::OverlayFS* _overlayFS ) {
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
        LOG( getLogger( VerbosityError ) )
            << "Exception in calculateFileHash: " << strError << "\n";
    } catch ( ... ) {
        LOG( getLogger( VerbosityError ) ) << "Unknown exception in calculateFileHash\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };
}

ETH_REGISTER_PRECOMPILED( logTextMessage )( bytesConstRef _in ) {
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
        fnColorizer_t fnHeader = []( const std::string& s ) -> std::string {
            return cc::info( s );
        };
        fnColorizer_t fnText = []( const std::string& s ) -> std::string {
            return cc::normal( s );
        };

        std::string strMessageTypeDesc = "";
        switch ( nMessageType ) {
        case 0:
        default:  // normal message
            strMessageTypeDesc = "normal";
            break;
        case 1:  // debug message
            fnHeader = []( const std::string& s ) -> std::string { return cc::normal( s ); };
            fnText = []( const std::string& s ) -> std::string { return cc::debug( s ); };
            strMessageTypeDesc = "debug";
            break;
        case 2:  // trace message
            fnHeader = []( const std::string& s ) -> std::string { return cc::debug( s ); };
            fnText = []( const std::string& s ) -> std::string { return cc::debug( s ); };
            strMessageTypeDesc = "trace";
            break;
        case 3:  // warning message
            fnHeader = []( const std::string& s ) -> std::string { return cc::warn( s ); };
            fnText = []( const std::string& s ) -> std::string { return cc::warn( s ); };
            strMessageTypeDesc = "warning";
            break;
        case 4:  // error message
            fnHeader = []( const std::string& s ) -> std::string { return cc::error( s ); };
            fnText = []( const std::string& s ) -> std::string { return cc::error( s ); };
            strMessageTypeDesc = "error";
            break;
        case 5:  // fatal message
            fnHeader = []( const std::string& s ) -> std::string { return cc::fatal( s ); };
            fnText = []( const std::string& s ) -> std::string { return cc::error( s ); };
            strMessageTypeDesc = "FATAL";
            break;
        }
        std::stringstream ss;
        ss << fnHeader( "SmartContract " + strMessageTypeDesc + " message from " + address + ":" )
           << fnText( " " + rawString );

        switch ( nMessageType ) {
        case 0:
        default:  // normal message
            LOG( getLogger( VerbosityInfo ) ) << ss.str();
            break;
        case 1:  // debug message
            LOG( getLogger( VerbosityDebug ) ) << ss.str();
            break;
        case 2:  // trace message
            LOG( getLogger( VerbosityTrace ) ) << ss.str();
            break;
        case 3:  // warning message
            LOG( getLogger( VerbosityWarning ) ) << ss.str();
            break;
        case 4:  // error message
            LOG( getLogger( VerbosityError ) ) << ss.str();
            break;
        case 5:  // fatal message
            LOG( getLogger( VerbosityDebug ) ) << ss.str();
            break;
        }

        u256 code = 1;
        bytes response = toBigEndian( code );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/logTextMessage(): " << strError << "\n";
    } catch ( ... ) {
        LOG( getLogger( VerbosityError ) ) << "Unknown exception in precompiled/logTextMessage()\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}

static const std::list< std::string > g_listReadableConfigParts{ "skaleConfig.sChain.nodes.",
    "skaleConfig.nodeInfo.wallets.ima.n" };

static bool stat_is_accessible_json_path( const std::string& strPath ) {
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

static size_t stat_calc_string_bytes_count_in_pages_32( size_t len_str ) {
    size_t rv = 32, blocks = len_str / 32 + ( ( ( len_str % 32 ) != 0 ) ? 1 : 0 );
    rv += blocks * 32;
    return rv;
}

static void stat_check_ouput_string_size_overflow( std::string& s ) {
    static const size_t g_maxLen = 1024 * 1024 - 1;
    size_t len = s.length();
    if ( len > g_maxLen )
        s.erase( s.begin() + len, s.end() );
}

static bytes& stat_bytes_add_pad_32( bytes& rv ) {
    while ( ( rv.size() % 32 ) != 0 )
        rv.push_back( 0 );
    return rv;
}

static bytes stat_string_to_bytes_with_length( std::string& s ) {
    stat_check_ouput_string_size_overflow( s );
    dev::u256 uLength( s.length() );
    bytes rv = toBigEndian( uLength );
    stat_bytes_add_pad_32( rv );
    for ( std::string::const_iterator it = s.cbegin(); it != s.cend(); ++it )
        rv.push_back( ( *it ) );
    stat_bytes_add_pad_32( rv );
    return rv;
}

static dev::u256 stat_parse_u256_hex_or_dec( const std::string& strValue ) {
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

static bool isCallToHistoricData( const std::string& callData ) {
    // in C++ 20 there is string::starts_with, but we do not use C++ 20 yet
    return boost::algorithm::starts_with( callData, "skaleConfig.sChain.nodes." );
}

static std::pair< std::string, unsigned > parseHistoricFieldRequest( std::string callData ) {
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
ETH_REGISTER_PRECOMPILED( getConfigVariableUint256 )( bytesConstRef _in ) {
    try {
        size_t lengthName;
        std::string rawName;
        convertBytesToString( _in, 0, rawName, lengthName );
        if ( !stat_is_accessible_json_path( rawName ) )
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
        LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/getConfigVariableUint256(): " << strError << "\n";
    } catch ( ... ) {
        LOG( getLogger( VerbosityError ) )
            << "Unknown exception in precompiled/getConfigVariableUint256()\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}

ETH_REGISTER_PRECOMPILED( getConfigVariableAddress )( bytesConstRef _in ) {
    try {
        size_t lengthName;
        std::string rawName;
        convertBytesToString( _in, 0, rawName, lengthName );
        if ( !stat_is_accessible_json_path( rawName ) )
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
        LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/getConfigVariableAddress(): " << strError << "\n";
    } catch ( ... ) {
        LOG( getLogger( VerbosityError ) )
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
ETH_REGISTER_PRECOMPILED( getConfigVariableString )( bytesConstRef _in ) {
    try {
        size_t lengthName;
        std::string rawName;
        convertBytesToString( _in, 0, rawName, lengthName );
        if ( !stat_is_accessible_json_path( rawName ) )
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
        LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/getConfigVariableString(): " << strError << "\n";
    } catch ( ... ) {
        LOG( getLogger( VerbosityError ) )
            << "Unknown exception in precompiled/getConfigVariableString()\n";
    }
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}

ETH_REGISTER_PRECOMPILED( fnReserved0x16 )( bytesConstRef /*_in*/ ) {
    u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}

static dev::u256 stat_s2a( const std::string& saIn ) {
    std::string sa;
    if ( !( saIn.length() > 2 && saIn[0] == '0' && ( saIn[1] == 'x' || saIn[1] == 'X' ) ) )
        sa = "0x" + saIn;
    else
        sa = saIn;
    dev::u256 u( sa.c_str() );
    return u;
}

ETH_REGISTER_PRECOMPILED( getConfigPermissionFlag )( bytesConstRef _in ) {
    try {
        dev::u256 uValue;
        uValue = 0;

        auto rawAddressParameter = _in.cropped( 12, 20 ).toBytes();
        std::string addressParameter;
        boost::algorithm::hex( rawAddressParameter.begin(), rawAddressParameter.end(),
            back_inserter( addressParameter ) );
        dev::u256 uParameter = stat_s2a( addressParameter );

        size_t lengthName;
        std::string rawName;
        convertBytesToString( _in, 32, rawName, lengthName );
        if ( !stat_is_accessible_json_path( rawName ) )
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
                dev::u256 uKey = stat_s2a( strKey );
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
        LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/getConfigPermissionFlag(): " << strError << "\n";
    } catch ( ... ) {
        LOG( getLogger( VerbosityError ) )
            << "Unknown exception in precompiled/getConfigPermissionFlag()\n";
    }
    dev::u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}

ETH_REGISTER_PRECOMPILED( getBlockRandom )( bytesConstRef ) {
    try {
        if ( !g_skaleHost )
            throw std::runtime_error( "SkaleHost accessor was not initialized" );
        dev::u256 uValue = g_skaleHost->getBlockRandom();
        bytes response = toBigEndian( uValue );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/getBlockRandom(): " << strError << "\n";
    } catch ( ... ) {
        LOG( getLogger( VerbosityError ) ) << "Unknown exception in precompiled/getBlockRandom()\n";
    }
    dev::u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}

ETH_REGISTER_PRECOMPILED( addBalance )( [[maybe_unused]] bytesConstRef _in ) {
    dev::u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}

ETH_REGISTER_PRECOMPILED( getIMABLSPublicKey )( bytesConstRef ) {
    try {
        if ( !g_skaleHost )
            throw std::runtime_error( "SkaleHost accessor was not initialized" );
        auto imaBLSPublicKey = g_skaleHost->getIMABLSPublicKey();
        bytes response = toBigEndian( dev::u256( imaBLSPublicKey[0] ) ) +
                         toBigEndian( dev::u256( imaBLSPublicKey[1] ) ) +
                         toBigEndian( dev::u256( imaBLSPublicKey[2] ) ) +
                         toBigEndian( dev::u256( imaBLSPublicKey[3] ) );
        return { true, response };
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        LOG( getLogger( VerbosityError ) )
            << "Exception in precompiled/getIMABLSPublicKey(): " << strError << "\n";
    } catch ( ... ) {
        LOG( getLogger( VerbosityError ) )
            << "Unknown exception in precompiled/getIMABLSPublicKey()\n";
    }
    dev::u256 code = 0;
    bytes response = toBigEndian( code );
    return { false, response };  // 1st false - means bad error occur
}

}  // namespace
