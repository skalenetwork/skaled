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

#pragma once

#include <jsonrpccpp/client.h>
#include <map>
#include <sstream>
#include <string>

namespace dev::eth {

enum class TraceType {
    DEFAULT_TRACER,
    PRESTATE_TRACER,
    CALL_TRACER,
    REPLAY_TRACER,
    FOUR_BYTE_TRACER,
    NOOP_TRACER,
    ALL_TRACER
};

class TraceOptions {
public:
    bool disableStorage = false;
    bool enableMemory = false;
    bool disableStack = false;
    // geth enables return data by default
    bool enableReturnData = true;
    bool prestateDiffMode = false;
    bool onlyTopCall = false;
    bool withLog = false;

    [[nodiscard]] std::string toString() {
        std::stringstream s;
        s << ( uint64_t ) tracerType;
        s << disableStorage;
        s << enableMemory;
        s << disableStack;
        s << enableReturnData;
        s << prestateDiffMode;
        s << onlyTopCall;
        s << withLog;
        return s.str();
    }


    TraceType tracerType = TraceType::DEFAULT_TRACER;

    static const std::map< std::string, TraceType > s_stringToTracerMap;
    [[nodiscard]] static TraceOptions make( Json::Value const& _json );
};

}  // namespace dev::eth
