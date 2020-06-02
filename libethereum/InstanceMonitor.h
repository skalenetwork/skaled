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
 * @file SkaleHost.h
 * @author Dmytro Nazarenko
 * @date 2018
 */

#pragma once

#include <libdevcore/FileSystem.h>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

class InstanceMonitor {
public:
    explicit InstanceMonitor( const boost::filesystem::path& _configPath )
        : m_configPath( _configPath ),
          m_finishTimestamp( 0 ),
          m_isExit( false ),
          m_rotationFilePath( dev::getDataDir() / rotation_file_name ) {
        restoreRotationParams();
    }
    void performRotation();
    void initRotationParams( uint64_t _timestamp, bool _isExit );
    bool isTimeToRotate( uint64_t _timestamp );

protected:
    void restoreRotationParams();
    [[nodiscard]] uint64_t finishTimestamp() const { return m_finishTimestamp; }

        [[nodiscard]] bool isExit() const {
        return m_isExit;
    }

    [[nodiscard]] fs::path rotationFilePath() const { return m_rotationFilePath; }

    private : boost::filesystem::path const m_configPath;
    uint64_t m_finishTimestamp;
    bool m_isExit;
    const fs::path m_rotationFilePath;

    static const std::string rotation_file_name;
};
