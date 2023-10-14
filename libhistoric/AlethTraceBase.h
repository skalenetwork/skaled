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
#include "libevm/LegacyVM.h"
#include <jsonrpccpp/common/exception.h>
#include <skutils/eth_utils.h>

// therefore we limit the  memory and storage entries returned to 1024 to avoid
// denial of service attack.
// see here https://banteg.mirror.xyz/3dbuIlaHh30IPITWzfT1MFfSg6fxSssMqJ7TcjaWecM
#define MAX_MEMORY_VALUES_RETURNED 1024
#define MAX_STORAGE_VALUES_RETURNED 1024
#define MAX_TRACE_DEPTH 256


#define STATE_CHECK( _EXPRESSION_ )                                             \
    if ( !( _EXPRESSION_ ) ) {                                                  \
        auto __msg__ = std::string( "State check failed::" ) + #_EXPRESSION_ + " " + \
                       std::string( __FILE__ ) + ":" + std::to_string( __LINE__ );        \
        throw std::runtime_error( __msg__);                                             \
    }


namespace dev {
namespace eth {

class FunctionCall;


// It is important that trace functions do not throw exceptions and do not modify state
// so that they do not interfere with EVM execution

class AlethTraceBase {

public:

    Json::Value getJSONResult() const;

protected:
    enum class TraceType { STANDARD_TRACER, PRESTATE_TRACER, CALL_TRACER };

    struct DebugOptions {
        bool disableStorage = false;
        bool enableMemory = false;
        bool disableStack = false;
        bool enableReturnData = false;
        bool prestateDiffMode = false;
        TraceType tracerType = TraceType::STANDARD_TRACER;
    };


    std::shared_ptr< FunctionCall > topFunctionCall;
    std::shared_ptr< FunctionCall > currentlyExecutingFunctionCall;


    AlethTraceBase( Transaction& _t, Json::Value const& _options ) ;



    [[nodiscard]] const DebugOptions& getOptions() const ;

    void functionCalled( const Address& _from, const Address& _to, uint64_t _gasLimit,
        const std::vector< uint8_t >& _inputData, const u256& _value ) ;

    void functionReturned(evmc_status_code _status) ;

    void recordAccessesToAccountsAndStorageValues( uint64_t _pc, Instruction& _inst,
        const bigint& _lastOpGas, const bigint& _gasRemaining, const ExtVMFace* _voidExt, AlethExtVM& _ext,
        const LegacyVM* _vm ) ;

    AlethTraceBase::DebugOptions debugOptions( Json::Value const& _json ) ;

    void resetLastReturnVariables() ;

    std::shared_ptr<std::vector<uint8_t>> extractMemoryByteArrayFromStackPointer( const LegacyVM* _vm ) ;

    std::string evmErrorDescription( evmc_status_code _error );


    std::vector< Instruction > m_lastInst;
    std::shared_ptr< Json::Value > m_defaultOpTrace;
    Json::FastWriter m_fastWriter;
    Address m_from;
    Address m_to;
    DebugOptions m_options;
    Json::Value m_jsonTrace;

    static const std::map< std::string, AlethTraceBase::TraceType > s_stringToTracerMap;


    // set of all storage values accessed during execution
    std::set< Address > m_accessedAccounts;
    // map of all storage addresses accessed (read or write) during execution
    // for each storage address the current value if recorded
    std::map< Address, std::map< u256, u256 > > m_accessedStorageValues;

    bool m_isCreate = false;

    uint64_t m_totalGasLimit = 0;


    uint64_t m_lastInstructionGas = 0;
    uint64_t m_lastGasRemaining = 0;
    int64_t m_lastDepth = -1;
    Instruction m_lastInstruction;
    std::shared_ptr<std::vector<uint8_t>> m_lastReturnData;
    bool m_lastHasReverted = false;
    bool m_lastHasError = false;
    std::string m_lastError;
    uint64_t m_lastFunctionGasLimit = 0;
    void resetVarsOnFunctionReturn();
    void processFunctionCallOrReturnIfHappened(
        const AlethExtVM& _ext, const LegacyVM* _vm);
};
}  // namespace eth
}  // namespace devCHECK_STATE(_face);