/*
Copyright (C) 2023-present, SKALE Labs

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


#ifdef HISTORIC_STATE


#include "TraceOptions.h"
#include "boost/throw_exception.hpp"


using namespace std;

namespace dev::eth {

const map< string, TraceType > TraceOptions::s_stringToTracerMap = {
    { "", TraceType::DEFAULT_TRACER }, { "callTracer", TraceType::CALL_TRACER },
    { "prestateTracer", TraceType::PRESTATE_TRACER }, { "replayTracer", TraceType::REPLAY_TRACER },
    { "4byteTracer", TraceType::FOUR_BYTE_TRACER }, { "noopTracer", TraceType::NOOP_TRACER },
    { "allTracer", TraceType::ALL_TRACER }
};

TraceOptions TraceOptions::make( Json::Value const& _json ) {
    TraceOptions op;

    if ( !_json.isObject() )
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS, "Invalid options" ) );

    if ( !_json["disableStorage"].empty() )
        op.disableStorage = _json["disableStorage"].asBool();

    if ( !_json["enableMemory"].empty() )
        op.enableMemory = _json["enableMemory"].asBool();
    if ( !_json["disableStack"].empty() )
        op.disableStack = _json["disableStack"].asBool();
    if ( !_json["enableReturnData"].empty() )
        op.enableReturnData = _json["enableReturnData"].asBool();

    if ( !_json["tracer"].empty() ) {
        auto tracerStr = _json["tracer"].asString();

        if ( s_stringToTracerMap.count( tracerStr ) ) {
            op.tracerType = s_stringToTracerMap.at( tracerStr );
        } else {
            BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException(
                jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS, "Invalid tracer type:" + tracerStr ) );
        }
    }

    if ( !_json["tracerConfig"].empty() && _json["tracerConfig"].isObject() ) {
        if ( !_json["tracerConfig"]["diffMode"].empty() &&
             _json["tracerConfig"]["diffMode"].isBool() ) {
            op.prestateDiffMode = _json["tracerConfig"]["diffMode"].asBool();
        }

        if ( !_json["tracerConfig"]["onlyTopCall"].empty() &&
             _json["tracerConfig"]["onlyTopCall"].isBool() ) {
            op.onlyTopCall = _json["tracerConfig"]["onlyTopCall"].asBool();
        }

        if ( !_json["tracerConfig"]["withLog"].empty() &&
             _json["tracerConfig"]["withLog"].isBool() ) {
            op.withLog = _json["tracerConfig"]["withLog"].asBool();
        }
    }

    return op;
}
}  // namespace dev::eth

#endif