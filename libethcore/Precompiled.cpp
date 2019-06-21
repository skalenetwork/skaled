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
#include <libdevcore/Log.h>
#include <libdevcore/SHA3.h>
#include <libdevcore/microprofile.h>
#include <libdevcrypto/Common.h>
#include <libdevcrypto/Hash.h>
#include <libdevcrypto/LibSnark.h>
#include <libethcore/Common.h>
#include <boost/algorithm/hex.hpp>
#include <mutex>

using namespace std;
using namespace dev;
using namespace dev::eth;

namespace fs = boost::filesystem;

std::unique_ptr< PrecompiledRegistrar > PrecompiledRegistrar::s_this;

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
                    return {true, ret.asBytes()};
                }
            } catch ( ... ) {
            }
        }
    }
    return {true, {}};
}

ETH_REGISTER_PRECOMPILED( sha256 )( bytesConstRef _in ) {
    return {true, dev::sha256( _in ).asBytes()};
}

ETH_REGISTER_PRECOMPILED( ripemd160 )( bytesConstRef _in ) {
    return {true, h256( dev::ripemd160( _in ), h256::AlignRight ).asBytes()};
}

ETH_REGISTER_PRECOMPILED( identity )( bytesConstRef _in ) {
    MICROPROFILE_SCOPEI( "VM", "identity", MP_RED );
    return {true, _in.toBytes()};
}

// Parse _count bytes of _in starting with _begin offset as big endian int.
// If there's not enough bytes in _in, consider it infinitely right-padded with zeroes.
bigint parseBigEndianRightPadded( bytesConstRef _in, bigint const& _begin, bigint const& _count ) {
    if ( _begin > _in.count() )
        return 0;
    assert( _count <= numeric_limits< size_t >::max() / 8 );  // Otherwise, the return value would
                                                              // not fit in the memory.

    size_t const begin{_begin};
    size_t const count{_count};

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
        return {true, bytes{}};  // This is a special case where expLength can be very big.
    assert( expLength <= numeric_limits< size_t >::max() / 8 );

    bigint const base( parseBigEndianRightPadded( _in, 96, baseLength ) );
    bigint const exp( parseBigEndianRightPadded( _in, 96 + baseLength, expLength ) );
    bigint const mod( parseBigEndianRightPadded( _in, 96 + baseLength + expLength, modLength ) );

    bigint const result = mod != 0 ? boost::multiprecision::powm( base, exp, mod ) : bigint{0};

    size_t const retLength( modLength );
    bytes ret( retLength );
    toBigEndian( result, ret );

    return {true, ret};
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

ETH_REGISTER_PRECOMPILED_PRICER( modexp )( bytesConstRef _in ) {
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

ETH_REGISTER_PRECOMPILED( alt_bn128_G1_mul )( bytesConstRef _in ) {
    return dev::crypto::alt_bn128_G1_mul( _in );
}

ETH_REGISTER_PRECOMPILED( alt_bn128_pairing_product )( bytesConstRef _in ) {
    return dev::crypto::alt_bn128_pairing_product( _in );
}

ETH_REGISTER_PRECOMPILED_PRICER( alt_bn128_pairing_product )( bytesConstRef _in ) {
    return 100000 + ( _in.size() / 192 ) * 80000;
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
    bigint const sstringLength( parseBigEndianRightPadded( _in, _startPosition, UINT256_SIZE ) );
    _stringLength = sstringLength.convert_to< size_t >();
    vector_ref< const unsigned char > byteFilename =
        _in.cropped( _startPosition + 32, _stringLength );
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
ETH_REGISTER_PRECOMPILED( createFile )( bytesConstRef _in ) {
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
        if ( fileSize > FILE_MAX_SIZE ) {
            std::stringstream ss;
            ss << "createFile() failed because requested file size " << fileSize
               << " exceeds supported limit " << FILE_MAX_SIZE;
            throw std::runtime_error( ss.str() );
        }
        const fs::path filePath( rawFilename );
        const fs::path fsDirectoryPath = getFileStorageDir( Address( address ) );
        if ( !fs::exists( fsDirectoryPath ) ) {
            bool isCreated = fs::create_directories( fsDirectoryPath );
            if ( !isCreated ) {
                throw std::runtime_error(
                    "createFile() failed because cannot create subdirectory" );
            }
        }
        const fs::path fsFilePath = fsDirectoryPath / filePath.parent_path();
        if ( !fs::exists( fsFilePath ) ) {
            throw std::runtime_error( "createFile() failed because directory not exists" );
        }
        fstream file;
        file.open( ( fsFilePath / filePath.filename() ).string(), ios::out );
        if ( fileSize > 0 ) {
            file.seekp( static_cast< long >( fileSize ) - 1 );
            file.write( "0", 1 );
        }

        u256 code = 1;
        bytes response = toBigEndian( code );
        return {true, response};
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
    return {false, response};
}

ETH_REGISTER_PRECOMPILED( uploadChunk )( bytesConstRef _in ) {
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

        fstream file;
        file.open( filePath.string(), ios::binary | ios::out | ios::in );
        file.seekp( static_cast< long >( position ) );
        file.write( ( char* ) data, dataLength );

        u256 code = 1;
        bytes response = toBigEndian( code );
        return {true, response};
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
    return {false, response};
}

// TODO: Check vulnerabilities
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
        if ( position > stat_compute_file_size( filePath.c_str() ) ) {
            throw std::runtime_error(
                "readChunk() failed because chunk gets out of the file bounds" );
        }

        std::ifstream infile( filePath.string(), std::ios_base::binary );
        infile.seekg( static_cast< long >( position ) );
        bytes buffer( chunkLength );
        infile.read(
            reinterpret_cast< char* >( &buffer[0] ), static_cast< long >( buffer.size() ) );
        return {true, buffer};
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
    return {false, response};
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
        size_t const fileSize = stat_compute_file_size( filePath.c_str() );
        bytes response = toBigEndian( static_cast< u256 >( fileSize ) );
        return {true, response};
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
    return {false, response};
}

ETH_REGISTER_PRECOMPILED( deleteFile )( bytesConstRef _in ) {
    try {
        auto rawAddress = _in.cropped( 12, 20 ).toBytes();
        std::string address;
        boost::algorithm::hex( rawAddress.begin(), rawAddress.end(), back_inserter( address ) );
        size_t filenameLength;
        std::string filename;
        convertBytesToString( _in, 32, filename, filenameLength );

        const fs::path filePath = getFileStorageDir( Address( address ) ) / filename;
        if ( remove( filePath.c_str() ) != 0 ) {
            throw std::runtime_error( "File cannot be deleted" );
        }
        u256 code = 1;
        bytes response = toBigEndian( code );
        return {true, response};
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
    return {false, response};
}

ETH_REGISTER_PRECOMPILED( createDirectory )( bytesConstRef _in ) {
    try {
        auto rawAddress = _in.cropped( 12, 20 ).toBytes();
        std::string address;
        boost::algorithm::hex( rawAddress.begin(), rawAddress.end(), back_inserter( address ) );
        size_t directoryPathLength;
        std::string directoryPath;
        convertBytesToString( _in, 32, directoryPath, directoryPathLength );

        const fs::path absolutePath = getFileStorageDir( Address( address ) ) / directoryPath;
        bool isCreated = fs::create_directories( absolutePath );
        if ( !isCreated ) {
            throw std::runtime_error( "createDirectory() failed because cannot create directory" );
        }
        u256 code = 1;
        bytes response = toBigEndian( code );
        return {true, response};
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
    return {false, response};
}

ETH_REGISTER_PRECOMPILED( deleteDirectory )( bytesConstRef _in ) {
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
        fs::remove_all( absolutePath );
        u256 code = 1;
        bytes response = toBigEndian( code );
        return {true, response};
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
    return {false, response};
}

}  // namespace
