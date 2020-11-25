/*
Copyright (C) 2020-present SKALE Labs

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
/** @file SkaleDebugFace.h
* @authors:
*   Oleh Nikolaiev <oleg@skalelabs.com>
* @date 2020
*/

#ifndef SKALEDEBUGFACE_H
#define SKALEDEBUGFACE_H

#include "ModularServer.h"

namespace dev {
namespace rpc {

class SkaleDebugFace : public ServerInterface< SkaleDebugFace > {
public:
    SkaleDebugFace() {
        this->bindAndAddMethod( jsonrpc::Procedure( "skale_performanceTrackingStatus",
                                    jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, NULL ),
            &dev::rpc::SkaleDebugFace::skale_performanceTrackingStatusI );
        this->bindAndAddMethod( jsonrpc::Procedure( "skale_performanceTrackingStart",
                                    jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, NULL ),
            &dev::rpc::SkaleDebugFace::skale_performanceTrackingStartI );
        this->bindAndAddMethod( jsonrpc::Procedure( "skale_performanceTrackingStop",
                                    jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, NULL ),
            &dev::rpc::SkaleDebugFace::skale_performanceTrackingStopI );
        this->bindAndAddMethod( jsonrpc::Procedure( "skale_performanceTrackingFetch",
                                    jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, NULL ),
            &dev::rpc::SkaleDebugFace::skale_performanceTrackingFetchI );
    }

    inline virtual void skale_performanceTrackingStatusI(
        const Json::Value& request, Json::Value& response ) {
        response = this->skale_performanceTrackingStatus( request );
    }
    inline virtual void skale_performanceTrackingStartI(
        const Json::Value& request, Json::Value& response ) {
        response = this->skale_performanceTrackingStart( request );
    }
    inline virtual void skale_performanceTrackingStopI(
        const Json::Value& request, Json::Value& response ) {
        response = this->skale_performanceTrackingStop( request );
    }
    inline virtual void skale_performanceTrackingFetchI(
        const Json::Value& request, Json::Value& response ) {
        response = this->skale_performanceTrackingFetch( request );
    }

    virtual Json::Value skale_performanceTrackingStatus( const Json::Value& request ) = 0;
    virtual Json::Value skale_performanceTrackingStart( const Json::Value& request ) = 0;
    virtual Json::Value skale_performanceTrackingStop( const Json::Value& request ) = 0;
    virtual Json::Value skale_performanceTrackingFetch( const Json::Value& request ) = 0;

};  /// class SkaleDebugFace

}  // namespace rpc
}  // namespace dev

#endif  // SKALEDEBUGFACE_H
