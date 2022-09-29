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
        LOG( m_logger ) << "Exception in createFileOp: " << strError << "\n";
    } catch ( ... ) {
        LOG( m_logger ) << "Unknown exception in createFileOp\n";
    }
    return false;
}

bool CreateDirectoryOp::execute() {
    try {
        bool isCreated = fs::create_directories( this->path );

        if ( !isCreated ) {
            throw std::runtime_error( "createDirectoryOp failed because cannot create directory" );
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

void OverlayFS::createDirectory( const std::string& path ) {
    m_cache.push_back( std::make_shared <CreateDirectoryOp> (path) );
}

void OverlayFS::createFile( const std::string& filePath, const size_t fileSize ) {
    m_cache.push_back( std::make_shared <CreateFileOp> (filePath, fileSize) );
}

void OverlayFS::commit() {
    for ( size_t i = 0; i < m_cache.size(); ++i ) {
        m_cache[i]->execute();
    }
}

}  // namespace skale
