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

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

class SkaleMonitor {
public:
    explicit SkaleMonitor( const boost::filesystem::path& _configPath )
        : m_configPath( _configPath ), m_finishTimestamp( 0 ), m_isExit( false ) {}
    void performRotation();
    void initRotationParams( uint64_t _timestamp, bool _isExit );
    bool isTimeToRotate( uint64_t _timestamp );

protected:
    uint64_t getFinishTimestamp();
    bool getIsExit();

private:
    boost::filesystem::path const m_configPath;
    uint64_t m_finishTimestamp;
    bool m_isExit;
    static const fs::path rotation_info_file_path;

    void restoreRotationParams();
    void restartInstance();
    void shutDownInstance();
};
