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
#include "CallTracePrinter.h"
#include "DefaultTracePrinter.h"
#include "FourByteTracePrinter.h"
#include "NoopTracePrinter.h"
#include "PrestateTracePrinter.h"
#include "ReplayTracePrinter.h"
#include "TraceOptions.h"
#include "TraceStructuresAndDefs.h"
#include "json/json.h"
#include "libdevcore/Common.h"
#include "libevm/Instruction.h"
#include "libevm/LegacyVM.h"
#include "libevm/VMFace.h"
#include <cstdint>

namespace Json {
class Value;
}

namespace dev::eth {

// It is important that trace functions do not throw exceptions and do not modify state
// so that they do not interfere with EVM execution

class FunctionCall;

class AlethStandardTrace {
public:
    // Append json trace to given (array) value
    explicit AlethStandardTrace( Transaction& _t, const TraceOptions& _options );

    // this function will be executed on each EVM instruction
    void operator()( uint64_t _steps, uint64_t _pc, Instruction _inst, bigint _newMemSize,
        bigint _gasOpGas, bigint _gasRemaining, VMFace const* _vm, ExtVMFace const* _voidExt );


    OnOpFunc functionToExecuteOnEachOperation() {
        return [=]( uint64_t _steps, uint64_t _pc, Instruction _inst, bigint _newMemSize,
                   bigint _gasCost, bigint _gas, VMFace const* _vm, ExtVMFace const* _extVM ) {
            ( *this )( _steps, _pc, _inst, _newMemSize, _gasCost, _gas, _vm, _extVM );
        };
    }

    // this function will be called at the end of executions
    void finalizeTrace(
        ExecutionResult& _er, HistoricState& _stateBefore, HistoricState& _stateAfter );

    [[nodiscard]] Json::Value getJSONResult() const;
    [[nodiscard]] const shared_ptr< FunctionCall >& getTopFunctionCall() const;
    [[nodiscard]] TraceOptions getOptions() const;
    [[nodiscard]] const map< Address, map< u256, u256 > >& getAccessedStorageValues() const;
    [[nodiscard]] const set< Address >& getAccessedAccounts() const;
    [[nodiscard]] const h256& getTxHash() const;
    [[nodiscard]] const shared_ptr< Json::Value >& getDefaultOpTrace() const;

private:
    void recordInstructionExecution( uint64_t _pc, Instruction _inst, bigint _gasOpGas,
        bigint _gasRemaining, VMFace const* _vm, ExtVMFace const* _voidExt );

    void recordFunctionIsCalled( const Address& _from, const Address& _to, uint64_t _gasLimit,
        const vector< uint8_t >& _inputData, const u256& _value );

    void recordFunctionReturned(
        evmc_status_code _status, const vector< uint8_t >& _returnData, uint64_t _gasUsed );

    void recordAccessesToAccountsAndStorageValues( uint64_t, Instruction& _inst,
        uint64_t _lastOpGas, uint64_t _gasRemaining, const ExtVMFace* _face, AlethExtVM& _ext,
        const LegacyVM* _vm );

    [[nodiscard]] static vector< uint8_t > extractMemoryByteArrayFromStackPointer(
        const LegacyVM* _vm );

    void processFunctionCallOrReturnIfHappened(
        const AlethExtVM& _ext, const LegacyVM* _vm, uint64_t _gasRemaining );

    void appendOpToStandardOpTrace( uint64_t _pc, Instruction& _inst, const bigint& _gasCost,
        const bigint& _gas, const ExtVMFace* _ext, AlethExtVM& _alethExt, const LegacyVM* _vm );

    void printAllTraces( Json::Value& _jsonTrace, ExecutionResult& _er,
        const HistoricState& _stateBefore, const HistoricState& _stateAfter );

    shared_ptr< FunctionCall > m_topFunctionCall;
    shared_ptr< FunctionCall > m_currentlyExecutingFunctionCall;
    vector< Instruction > m_lastInst;
    shared_ptr< Json::Value > m_defaultOpTrace = nullptr;
    Json::FastWriter m_fastWriter;
    Address m_from;
    Address m_to;
    TraceOptions m_options;
    h256 m_txHash;
    Json::Value m_jsonTrace;
    // set of all storage values accessed during execution
    set< Address > m_accessedAccounts;
    // map of all storage addresses accessed (read or write) during execution
    // for each storage address the current value if recorded
    map< Address, map< u256, u256 > > m_accessedStorageValues;
    OpExecutionRecord m_lastOpRecord;
    std::atomic< bool > m_isFinalized = false;
    NoopTracePrinter m_noopTracePrinter;
    FourByteTracePrinter m_fourByteTracePrinter;
    CallTracePrinter m_callTracePrinter;
    ReplayTracePrinter m_replayTracePrinter;
    PrestateTracePrinter m_prestateTracePrinter;
    DefaultTracePrinter m_defaultTracePrinter;

    const map< TraceType, TracePrinter& > m_tracePrinters;
};
}  // namespace dev::eth
