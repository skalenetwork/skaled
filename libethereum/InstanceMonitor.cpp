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

const std::string InstanceMonitor::rotation_info_file_name = "rotation.json";

void InstanceMonitor::prepareRotation() {
    reportExitTimeReached( true );
    fs::remove( m_rotationInfoFilePath );
}

void InstanceMonitor::initRotationParams( uint64_t _finishTimestamp ) {
    try {
        nlohmann::json rotationJson = nlohmann::json::object();
        rotationJson["timestamp"] = _finishTimestamp;

        std::ofstream rotationInfoFile( m_rotationInfoFilePath.string() );
        rotationInfoFile << rotationJson;

        LOG( m_infoLogger ) << "Set rotation time to " << _finishTimestamp;
    } catch ( ... ) {
        LOG( m_errorLogger ) << "Setting rotation timestamp failed";
        throw_with_nested( std::runtime_error( "cannot save rotation timestamp" ) );
    }
}

bool InstanceMonitor::isTimeToRotate( uint64_t _blockTimestamp ) const {
    if ( !fs::exists( m_rotationInfoFilePath ) ) {
        return false;
    }
    try {
        auto _rotationTimestamp = rotationTimestamp();
        return _rotationTimestamp <= _blockTimestamp;
    } catch ( InvalidRotationInfoFileException& ex ) {
        return false;
    }
}

uint64_t InstanceMonitor::rotationTimestamp() const {
    std::ifstream rotationInfoFile( m_rotationInfoFilePath.string() );
    try {
        auto rotationJson = nlohmann::json::parse( rotationInfoFile );
        auto timestamp = rotationJson["timestamp"].get< uint64_t >();
        LOG( m_infoLogger ) << "Rotation scheduled for " << timestamp;
        return timestamp;
    } catch ( ... ) {
        LOG( m_errorLogger ) << "Rotation file is malformed or missing";
        throw InvalidRotationInfoFileException( m_rotationInfoFilePath );
    }
}

void InstanceMonitor::reportExitTimeReached( bool _reached ) {
    if ( m_statusAndControl ) {
        LOG( m_infoLogger ) << "Setting ExitTimeReached = " << _reached;
        m_statusAndControl->setExitState( StatusAndControl::ExitTimeReached, _reached );
    } else
        LOG( m_infoLogger ) << "Simulating setting ExitTimeReached = " << _reached;
}
