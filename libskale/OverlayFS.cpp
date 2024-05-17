/*
    Copyright (C) 2018-present, SKALE Labs

    This file is part of skaled.

    skaled is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skaled is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skaled.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * @file OverlayFS.cpp
 * @author Dmytro Nazarenko
 * @date 2022
 */

#include "OverlayFS.h"
#include <libdevcrypto/Hash.h>
#include <libethereum/SchainPatch.h>
#include <secp256k1_sha256.h>
#include <fstream>


namespace fs = boost::filesystem;
namespace skale {

bool CreateFileOp::execute() {
    try {
        std::fstream file;
        file.open( this->filePath, std::ios::out );
        if ( fileSize > 0 ) {
            file.seekp( static_cast< long >( fileSize ) - 1 );
            file.write( "0", 1 );
        }
        return true;
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        LOG( m_logger ) << "Exception in CreateFileOp: " << strError << "\n";
    } catch ( ... ) {
        LOG( m_logger ) << "Unknown exception in CreateFileOp\n";
    }
    return false;
}

bool CreateDirectoryOp::execute() {
    try {
        bool isCreated = fs::create_directories( this->path );

        if ( !isCreated ) {
            throw std::runtime_error( "CreateDirectoryOp failed because cannot create directory" );
        }
        return true;
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        LOG( m_logger ) << "Exception in createDirectoryOp: " << strError << "\n";
    } catch ( ... ) {
        LOG( m_logger ) << "Unknown exception in createDirectoryOp\n";
    }
    return false;
}

bool DeleteFileOp::execute() {
    try {
        bool isDeleted = boost::filesystem::remove( this->path );
        if ( !isDeleted ) {
            throw std::runtime_error( "DeleteFileOp failed because cannot delete file" );
        }
        return true;
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        LOG( m_logger ) << "Exception in DeleteFileOp: " << strError << "\n";
    } catch ( ... ) {
        LOG( m_logger ) << "Unknown exception in DeleteFileOp\n";
    }
    return false;
}

bool DeleteDirectoryOp::execute() {
    try {
        bool isDeleted = boost::filesystem::remove_all( this->path );
        if ( !isDeleted ) {
            throw std::runtime_error( "DeleteDirectoryOp failed because cannot delete directory" );
        }
        return true;
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        LOG( m_logger ) << "Exception in DeleteDirectoryOp: " << strError << "\n";
    } catch ( ... ) {
        LOG( m_logger ) << "Unknown exception in DeleteDirectoryOp\n";
    }
    return false;
}

bool WriteChunkOp::execute() {
    try {
        std::fstream file;
        file.open( this->path, std::ios::binary | std::ios::out | std::ios::in );
        file.seekp( static_cast< long >( this->position ) );
        file.write( reinterpret_cast< const char* >( &this->data[0] ), this->dataLength );
        return true;
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        LOG( m_logger ) << "Exception in WriteChunkOp: " << strError << "\n";
    } catch ( ... ) {
        LOG( m_logger ) << "Unknown exception in WriteChunkOp\n";
    }
    return false;
}

bool CalculateFileHash::execute() {
    try {
        std::ifstream file( this->path );
        file.seekg( 0, std::ios::end );
        size_t fileSize = file.tellg();
        std::string fileContent( fileSize, ' ' );
        file.seekg( 0 );
        file.read( &fileContent[0], fileSize );

        const std::string fileHashName = this->path + "._hash";

        std::string relativePath = this->path.substr( this->path.find( "filestorage" ) );

        dev::h256 filePathHash = dev::sha256( relativePath );

        dev::h256 fileContentHash = dev::sha256( fileContent );

        secp256k1_sha256_t ctx;
        secp256k1_sha256_initialize( &ctx );
        secp256k1_sha256_write( &ctx, filePathHash.data(), filePathHash.size );
        secp256k1_sha256_write( &ctx, fileContentHash.data(), fileContentHash.size );

        dev::h256 commonFileHash;
        secp256k1_sha256_finalize( &ctx, commonFileHash.data() );

        std::fstream fileHash;
        fileHash.open( fileHashName, std::ios::binary | std::ios::out );
        fileHash.clear();
        fileHash << commonFileHash;
        fileHash.close();
        return true;
    } catch ( std::exception& ex ) {
        std::string strError = ex.what();
        if ( strError.empty() )
            strError = "exception without description";
        LOG( m_logger ) << "Exception in WriteHashFileOp: " << strError << "\n";
    } catch ( ... ) {
        LOG( m_logger ) << "Unknown exception in WriteHashFileOp\n";
    }
    return false;
}

void OverlayFS::createDirectory( const std::string& path ) {
    auto operation = std::make_shared< CreateDirectoryOp >( path );
    if ( isCacheEnabled() )
        m_cache.push_back( operation );
    else
        operation->execute();
}

void OverlayFS::createFile( const std::string& filePath, const size_t fileSize ) {
    auto operation = std::make_shared< CreateFileOp >( filePath, fileSize );
    if ( isCacheEnabled() )
        m_cache.push_back( operation );
    else
        operation->execute();
}

void OverlayFS::deleteFile( const std::string& filePath ) {
    auto operation = std::make_shared< DeleteFileOp >( filePath );
    if ( isCacheEnabled() )
        m_cache.push_back( operation );
    else
        operation->execute();
}

void OverlayFS::deleteDirectory( const std::string& path ) {
    auto operation = std::make_shared< DeleteDirectoryOp >( path );
    if ( m_isCacheEnabled )
        m_cache.push_back( operation );
    else
        operation->execute();
}

void OverlayFS::writeChunk( const std::string& filePath, const size_t position,
    const size_t dataLength, const _byte_* data ) {
    auto operation = std::make_shared< WriteChunkOp >( filePath, position, dataLength, data );
    if ( isCacheEnabled() )
        m_cache.push_back( operation );
    else
        operation->execute();
}

void OverlayFS::calculateFileHash( const std::string& filePath ) {
    auto operation = std::make_shared< CalculateFileHash >( filePath );
    if ( isCacheEnabled() )
        m_cache.push_back( operation );
    else
        operation->execute();
}

void OverlayFS::commit() {
    for ( size_t i = 0; i < m_cache.size(); ++i ) {
        m_cache[i]->execute();
    }
    m_cache.clear();
}
}  // namespace skale
