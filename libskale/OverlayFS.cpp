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
        file.write( ( char* ) this->data, this->dataLength );
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

bool WriteHashFileOp::execute() {
    try {
        std::fstream fileHash;
        fileHash.open( this->path, std::ios::binary | std::ios::out );
        fileHash.clear();
        fileHash << this->commonFileHash;
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
    m_cache.push_back( std::make_shared< CreateDirectoryOp >( path ) );
}

void OverlayFS::createFile( const std::string& filePath, const size_t fileSize ) {
    m_cache.push_back( std::make_shared< CreateFileOp >( filePath, fileSize ) );
}

void OverlayFS::deleteFile( const std::string& filePath ) {
    m_cache.push_back( std::make_shared< DeleteFileOp >( filePath ) );
}

void OverlayFS::deleteDirectory( const std::string& path ) {
    m_cache.push_back( std::make_shared< DeleteDirectoryOp >( path ) );
}

void OverlayFS::writeChunk( const std::string& filePath, const size_t position, const size_t dataLength, const _byte_* data ) {
    m_cache.push_back( std::make_shared< WriteChunkOp >( filePath, position, dataLength, data ) );
}

void OverlayFS::writeHashFile( const std::string& filePath, const dev::h256& commonFileHash ) {
    m_cache.push_back( std::make_shared< WriteHashFileOp >( filePath, commonFileHash ) );
}

void OverlayFS::commit() {
    for ( size_t i = 0; i < m_cache.size(); ++i ) {
        m_cache[i]->execute();
    }
}

}  // namespace skale
