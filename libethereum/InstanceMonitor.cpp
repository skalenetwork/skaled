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
#include <iostream>
#include <json.hpp>

#include <libdevcore/Common.h>
#include <libdevcore/StatusAndControl.h>

using namespace dev;
namespace fs = boost::filesystem;

const std::string InstanceMonitor::rotation_info_file_name = "rotation.txt";

void InstanceMonitor::prepareRotation() {
    reportExitTimeReached( true );
    fs::remove( m_rotationInfoFilePath );
}

void InstanceMonitor::initRotationParams( uint64_t _finishTimestamp ) {
    nlohmann::json rotationJson = nlohmann::json::object();
    rotationJson["timestamp"] = _finishTimestamp;

    std::ofstream rotationInfoFile( m_rotationInfoFilePath.string() );
    rotationInfoFile << rotationJson;

    LOG( m_logger ) << "Set rotation time to " << _finishTimestamp;
}

bool InstanceMonitor::isTimeToRotate( uint64_t _finishTimestamp ) {
    if ( !fs::exists( m_rotationInfoFilePath ) ) {
        return false;
    }
    return getRotationTimestamp() <= _finishTimestamp;
}

uint64_t InstanceMonitor::getRotationTimestamp() {
    std::ifstream rotationInfoFile( m_rotationInfoFilePath.string() );
    auto rotationJson = nlohmann::json::parse( rotationInfoFile );
    return rotationJson["timestamp"].get< uint64_t >();
}

void InstanceMonitor::reportExitTimeReached( bool _reached ) {
    if ( m_statusAndControl ) {
        LOG( m_logger ) << "Setting ExitTimeReached = " << _reached;
        m_statusAndControl->setExitState( StatusAndControl::ExitTimeReached, _reached );
    } else
        LOG( m_logger ) << "Simulating setting ExitTimeReached = " << _reached;
}
