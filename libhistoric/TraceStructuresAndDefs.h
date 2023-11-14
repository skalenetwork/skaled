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

#include "libevm/Instruction.h"

//  we limit the  memory and storage entries returned to avoid
// denial of service attack.
// see here https://banteg.mirror.xyz/3dbuIlaHh30IPITWzfT1MFfSg6fxSssMqJ7TcjaWecM
constexpr uint64_t MAX_MEMORY_VALUES_RETURNED = 1024;
constexpr uint64_t MAX_STORAGE_VALUES_RETURNED = 1024;
constexpr int64_t MAX_TRACE_DEPTH = 256;


#define STATE_CHECK( _EXPRESSION_ )                                             \
    if ( !( _EXPRESSION_ ) ) {                                                  \
        auto __msg__ = string( "State check failed::" ) + #_EXPRESSION_ + " " + \
                       string( __FILE__ ) + ":" + to_string( __LINE__ );        \
        throw VMTracingError( __msg__ );                                        \
    }


namespace dev::eth {

using std::string, std::shared_ptr, std::make_shared, std::to_string, std::set, std::map,
    std::vector;


enum struct TraceType {
    DEFAULT_TRACER,
    PRESTATE_TRACER,
    CALL_TRACER,
    REPLAY_TRACER,
    FOUR_BYTE_TRACER,
    NOOP_TRACER,
    ALL_TRACER
};

struct DebugOptions {
    bool disableStorage = false;
    bool enableMemory = false;
    bool disableStack = false;
    bool enableReturnData = false;
    bool prestateDiffMode = false;
    bool onlyTopCall = false;
    bool withLog = false;
    TraceType tracerType = TraceType::DEFAULT_TRACER;
};

struct LogRecord {
    LogRecord( const vector< uint8_t >& _data, const vector< u256 >& _topics )
        : m_data( _data ), m_topics( _topics ) {}

    const vector< uint8_t > m_data;
    const vector< u256 > m_topics;
};

struct OpExecutionRecord {
    // this is top level record to enter the transaction
    // the first function is executed at depth 0, as it was called form depth -1
    explicit OpExecutionRecord( Instruction _op ) : OpExecutionRecord( -1, _op, 0, 0 ){};

    OpExecutionRecord( int64_t _depth, Instruction _op, uint64_t _gasRemaining, uint64_t _opGas );

    int64_t m_depth;
    Instruction m_op;
    uint64_t m_gasRemaining;
    uint64_t m_opGas;
};

}  // namespace dev::eth
