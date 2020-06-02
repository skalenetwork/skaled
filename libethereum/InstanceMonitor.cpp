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
 * @file SkaleHost.cpp
 * @author Dmytro Nazarenko
 * @date 2020
 */

#include "InstanceMonitor.h"
#include <csignal>
#include <iostream>
#include <json.hpp>

#include <libdevcore/Common.h>

using namespace dev;
namespace fs = boost::filesystem;

const std::string InstanceMonitor::rotation_file_name = "rotation.txt";

void InstanceMonitor::performRotation() {
    fs::remove( m_rotationFilePath );
    ExitHandler::exitHandler( SIGTERM );
}

void InstanceMonitor::initRotationParams( uint64_t _timestamp, bool _isExit ) {
    nlohmann::json rotationJson = nlohmann::json::object();
    rotationJson["timestamp"] = _timestamp;
    rotationJson["isExit"] = _isExit;

    std::ofstream rotationFile( m_rotationFilePath.string() );
    rotationFile << rotationJson;

    m_finishTimestamp = _timestamp;
    m_isExit = _isExit;
}

bool InstanceMonitor::isTimeToRotate( uint64_t _timestamp ) {
    if ( !fs::exists( m_rotationFilePath ) ) {
        return false;
    }
    return m_finishTimestamp <= _timestamp;
}

void InstanceMonitor::restoreRotationParams() {
    if ( fs::exists( m_rotationFilePath ) ) {
        std::ifstream rotateFile( m_rotationFilePath.string() );
        auto rotateJson = nlohmann::json::parse( rotateFile );
        auto rotateTimestamp = rotateJson["timestamp"].get< uint64_t >();
        auto isExit = rotateJson["isExit"].get< bool >();
        m_finishTimestamp = rotateTimestamp;
        m_isExit = isExit;
    }
}
