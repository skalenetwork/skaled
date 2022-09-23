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
    virtual bool execute() = 0;
};

class CreateFileOp : public BaseOp {
public:    
    CreateFileOp(const std::string& filePath, const size_t fileSize);
    bool execute() override;
private:
    const std::string filePath;
    const size_t fileSize;
    Logger m_fsLogger{ createLogger( VerbosityDebug, "fs" ) };
};

class CreateDirectoryOp : public BaseOp {
public:    
    CreateDirectoryOp(const std::string& path);
    bool execute() override;
private:
    const std::string path;
    Logger m_fsLogger{ createLogger( VerbosityDebug, "fs" ) };
};

class OverlayFS {
public:
    explicit OverlayFS();

    virtual ~OverlayFS() = default;

    // Copyable
    OverlayFS( OverlayFS const& ) = default;
    OverlayFS& operator=( OverlayFS const& ) = default;
    // Movable
    OverlayFS( OverlayFS&& ) = default;
    OverlayFS& operator=( OverlayFS&& ) = default;

    void commit();
    void reset();
    bool empty() const;

    void createFile( const std::string& filePath, const size_t fileSize );
    void createDirectory( const std::string& path );
    void writeChunk( const std::string& filePath, const size_t position, const size_t dataLength, const _byte_* data );
    void deleteFile( const std::string& filePath );
    void deleteDirectory( const std::string& path );
    void writeHashFile( const std::string& filePath );

private:
    std::vector< std::shared_ptr< BaseOp > > m_cache; // vector of filestorage operations for current state
};

}  // namespace skale
