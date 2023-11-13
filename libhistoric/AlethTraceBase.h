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

#include "AlethExtVM.h"
#include "FunctionCall.h"
#include "libevm/LegacyVM.h"
#include <jsonrpccpp/common/exception.h>
#include <skutils/eth_utils.h>

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

class FunctionCall;


// It is important that trace functions do not throw exceptions and do not modify state
// so that they do not interfere with EVM execution

enum class TraceType {
    STANDARD_TRACER,
    PRESTATE_TRACER,
    CALL_TRACER,
    REPLAY_TRACER,
    FOUR_BYTE_TRACER,
    NOOP_TRACER,

};

struct DebugOptions {
    bool disableStorage = false;
    bool enableMemory = false;
    bool disableStack = false;
    bool enableReturnData = false;
    bool prestateDiffMode = false;
    bool onlyTopCall;
    bool withLog = false;
    TraceType tracerType = TraceType::STANDARD_TRACER;
};

class AlethTraceBase {
public:
    [[nodiscard]] Json::Value getJSONResult() const;

protected:
    shared_ptr< FunctionCall > m_topFunctionCall;
    shared_ptr< FunctionCall > m_currentlyExecutingFunctionCall;


    AlethTraceBase( Transaction& _t, Json::Value const& _options );


    [[nodiscard]] const DebugOptions& getOptions() const;

    void functionCalled( const Address& _from, const Address& _to, uint64_t _gasLimit,
        const vector< uint8_t >& _inputData, const u256& _value );

    void functionReturned(
        evmc_status_code _status, const vector< uint8_t >& _returnData, uint64_t _gasUsed );

    void recordAccessesToAccountsAndStorageValues( uint64_t, Instruction& _inst,
        uint64_t _lastOpGas, uint64_t _gasRemaining, const ExtVMFace* _face, AlethExtVM& _ext,
        const LegacyVM* _vm );

    DebugOptions debugOptions( Json::Value const& _json );


    [[nodiscard]] static vector< uint8_t > extractMemoryByteArrayFromStackPointer(
        const LegacyVM* _vm );

    [[nodiscard]] string evmErrorDescription( evmc_status_code _error );


    vector< Instruction > m_lastInst;
    shared_ptr< Json::Value > m_defaultOpTrace = nullptr;
    Json::FastWriter m_fastWriter;
    Address m_from;
    Address m_to;
    DebugOptions m_options;
    h256 m_hash;
    Json::Value m_jsonTrace;


    static const map< string, TraceType > s_stringToTracerMap;


    // set of all storage values accessed during execution
    set< Address > m_accessedAccounts;
    // map of all storage addresses accessed (read or write) during execution
    // for each storage address the current value if recorded
    map< Address, map< u256, u256 > > m_accessedStorageValues;

    OpExecutionRecord m_lastOp;


    void processFunctionCallOrReturnIfHappened(
        const AlethExtVM& _ext, const LegacyVM* _vm, uint64_t _gasRemaining );
};
}  // namespace dev::eth
