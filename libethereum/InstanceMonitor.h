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
        : m_rotationInfoFilePath( _rotationInfoFileDirPath / rotation_info_file_name ),
          m_statusAndControl( _statusAndControl ) {
        reportExitTimeReached( false );
    }
    void prepareRotation();
    void initRotationParams( uint64_t _finishTimestamp );
    bool isTimeToRotate( uint64_t _blockTimestamp ) const;

protected:
    [[nodiscard]] uint64_t rotationTimestamp() const;
    [[nodiscard]] fs::path rotationInfoFilePath() const { return m_rotationInfoFilePath; }

    const fs::path m_rotationInfoFilePath;
    std::shared_ptr< StatusAndControl > m_statusAndControl;

    static const std::string rotation_info_file_name;

    void reportExitTimeReached( bool _reached );

    class InvalidRotationInfoFileException : public std::exception {
    protected:
        std::string what_str;

    public:
        boost::filesystem::path path;

        InvalidRotationInfoFileException( const boost::filesystem::path& _path ) : path( _path ) {
            what_str = "File " + path.string() + " is malformed or missing";
        }
        virtual const char* what() const noexcept override { return what_str.c_str(); }
    };


private:
    mutable dev::Logger m_infoLogger{ createLogger( dev::VerbosityInfo, "instance-monitor" ) };
    mutable dev::Logger m_errorLogger{ createLogger( dev::VerbosityError, "instance-monitor" ) };
};
