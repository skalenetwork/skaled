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
#include <libdevcore/Log.h>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

class StatusAndControl;

class InstanceMonitor {
public:
    explicit InstanceMonitor( const boost::filesystem::path& _rotationInfoFileDirPath,
        std::shared_ptr< StatusAndControl > _statusAndControl = nullptr )
        : m_finishTimestamp( 0 ),
          m_rotationInfoFilePath( _rotationInfoFileDirPath / rotation_info_file_name ),
          m_statusAndControl( _statusAndControl ) {
        restoreRotationParams();
        reportExitTimeReached( false );
    }
    void prepareRotation();
    void initRotationParams( uint64_t _finishTimestamp );
    bool isTimeToRotate( uint64_t _finishTimestamp );

protected:
    void restoreRotationParams();
    [[nodiscard]] uint64_t finishTimestamp() const { return m_finishTimestamp; }

    [[nodiscard]] fs::path rotationInfoFilePath() const { return m_rotationInfoFilePath; }

    uint64_t m_finishTimestamp;
    const fs::path m_rotationInfoFilePath;
    std::shared_ptr< StatusAndControl > m_statusAndControl;

    static const std::string rotation_info_file_name;

    void reportExitTimeReached( bool _reached );

private:
    dev::Logger m_logger{ createLogger( dev::VerbosityInfo, "instance-monitor" ) };
};
