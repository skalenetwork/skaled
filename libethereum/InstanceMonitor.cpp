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

const std::string InstanceMonitor::rotation_info_file_name = "rotation.txt";
const std::string InstanceMonitor::rotation_flag_file_name = ".rotation";

void InstanceMonitor::performRotation() {
    createFlagFile();
    fs::remove( m_rotationInfoFilePath );
    ExitHandler::exitHandler( SIGTERM, ExitHandler::ec_rotation_complete );
    LOG( m_logger ) << "Rotation is completed. Instance is exiting";
}

void InstanceMonitor::initRotationParams( uint64_t _finishTimestamp ) {
    nlohmann::json rotationJson = nlohmann::json::object();
    rotationJson["timestamp"] = _finishTimestamp;

    std::ofstream rotationInfoFile( m_rotationInfoFilePath.string() );
    rotationInfoFile << rotationJson;

    m_finishTimestamp = _finishTimestamp;
    LOG( m_logger ) << "Set rotation time to " << m_finishTimestamp;
}

bool InstanceMonitor::isTimeToRotate( uint64_t _finishTimestamp ) {
    if ( !fs::exists( m_rotationInfoFilePath ) ) {
        return false;
    }
    return m_finishTimestamp <= _finishTimestamp;
}

void InstanceMonitor::restoreRotationParams() {
    if ( fs::exists( m_rotationInfoFilePath ) ) {
        std::ifstream rotationInfoFile( m_rotationInfoFilePath.string() );
        auto rotationJson = nlohmann::json::parse( rotationInfoFile );
        m_finishTimestamp = rotationJson["timestamp"].get< uint64_t >();
    }
}

void InstanceMonitor::createFlagFile() {
    LOG( m_logger ) << "Creating flag file " << m_rotationFlagFilePath.string();
    std::ofstream( m_rotationFlagFilePath.string() );
}

void InstanceMonitor::removeFlagFile() {
    LOG( m_logger ) << "Removing flag file " << m_rotationFlagFilePath.string();
    if ( fs::exists( m_rotationFlagFilePath ) ) {
        fs::remove( m_rotationFlagFilePath );
    }
}
