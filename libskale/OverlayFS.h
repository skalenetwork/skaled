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
 * @file OverlayFS.h
 * @author Dmytro Nazarenko
 * @date 2022
 */

#pragma once

#include <functional>
#include <memory>

#include <libdevcore/Common.h>
#include <libdevcore/Log.h>

namespace skale {

class BaseOp {
public:
    virtual bool execute() = 0;
    dev::Logger m_logger{ createLogger( dev::VerbosityDebug, "fs" ) };
};

class CreateFileOp : public BaseOp {
public:
    CreateFileOp( const std::string& _filePath, const size_t _fileSize )
        : filePath( _filePath ), fileSize( _fileSize ) {}
    bool execute() override;

private:
    const std::string filePath;
    const size_t fileSize;
};

class CreateDirectoryOp : public BaseOp {
public:
    CreateDirectoryOp( const std::string& _path ) : path( _path ) {}
    bool execute() override;

private:
    const std::string path;
};

class DeleteFileOp : public BaseOp {
public:
    DeleteFileOp( const std::string& _path ) : path( _path ) {}
    bool execute() override;

private:
    const std::string path;
};

class DeleteDirectoryOp : public BaseOp {
public:
    DeleteDirectoryOp( const std::string& _path ) : path( _path ) {}
    bool execute() override;

private:
    const std::string path;
};

class WriteChunkOp : public BaseOp {
public:
    WriteChunkOp( const std::string& _path, const size_t _position, const size_t _dataLength,
        const _byte_* _data )
        : path( _path ), position( _position ), dataLength( _dataLength ), data( _data ) {}
    bool execute() override;

private:
    const std::string path;
    const size_t position;
    const size_t dataLength;
    const _byte_* data;
};

class calculateFileHashOp : public BaseOp {
public:
    calculateFileHashOp( const std::string& _path )
        : path( _path ) {}
    bool execute() override;

private:
    const std::string path;
};

class OverlayFS {
public:
    OverlayFS( bool _enableCache = true ) : m_isCacheEnabled( _enableCache ){};

    virtual ~OverlayFS() = default;

    // Copyable
    OverlayFS( OverlayFS const& ) = default;
    OverlayFS& operator=( OverlayFS const& ) = default;
    // Movable
    OverlayFS( OverlayFS&& ) = default;
    OverlayFS& operator=( OverlayFS&& ) = default;

    void commit();
    void reset() { m_cache.clear(); };
    bool empty() const { return m_cache.empty(); }
    bool isCacheEnabled() { return m_isCacheEnabled; }

    void createFile( const std::string& filePath, const size_t fileSize );
    void createDirectory( const std::string& path );
    void writeChunk( const std::string& filePath, const size_t position, const size_t dataLength,
        const _byte_* data );
    void deleteFile( const std::string& filePath );
    void deleteDirectory( const std::string& path );
    void calculateFileHash( const std::string& filePath );

private:
    std::vector< std::shared_ptr< BaseOp > > m_cache;  // vector of filestorage operations for
                                                       // current state
    bool m_isCacheEnabled;
};

}  // namespace skale
