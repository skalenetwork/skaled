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
#include <libdevcore/FileSystem.h>
#include <json.hpp>

namespace fs = boost::filesystem;

const fs::path InstanceMonitor::rotation_info_file_path = dev::getDataDir() / "rotation.txt";

void InstanceMonitor::performRotation() {}

void InstanceMonitor::initRotationParams( uint64_t _timestamp, bool _isExit ) {
    nlohmann::json rotationJson = nlohmann::json::object();
    rotationJson["timestamp"] = _timestamp;
    rotationJson["isExit"] = _isExit;

    std::ofstream rotationFile( rotation_info_file_path.string() );
    rotationFile << rotationJson;
}

bool InstanceMonitor::isTimeToRotate( uint64_t _timestamp ) {
    return getFinishTimestamp() <= _timestamp;
}

void InstanceMonitor::restoreRotationParams() {
    if ( InstanceMonitor::m_finishTimestamp == 0 && fs::exists( rotation_info_file_path ) ) {
        std::ifstream rotateFile( rotation_info_file_path.string() );
        auto rotateJson = nlohmann::json::parse( rotateFile );
        auto rotateTimestamp = rotateJson["timestamp"].get< uint64_t >();
        auto isExit = rotateJson["isExit"].get< uint64_t >();
        m_finishTimestamp = rotateTimestamp;
        m_isExit = isExit;
    }
}

void InstanceMonitor::restart() {}

void InstanceMonitor::shutdown() {}

uint64_t InstanceMonitor::getFinishTimestamp() {
    restoreRotationParams();
    return m_finishTimestamp;
}

bool InstanceMonitor::getIsExit() {
    restoreRotationParams();
    return m_isExit;
}
