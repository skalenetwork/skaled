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
#include <memory>

namespace Json {
class Value;
}

namespace dev::eth {

// It is important that trace functions do not throw exceptions and do not modify state
// so that they do not interfere with EVM execution

class FunctionCallRecord;

// This class collects information during EVM execution.  The oollected information
// is then used by trace printers to print the trace requested by the user

class AlethStandardTrace {
public:
    // Append json trace to given (array) value
    explicit AlethStandardTrace( Transaction& _t, const Address& _blockAuthor,
        const TraceOptions& _options, bool _isCall = false );

    // this function is executed on each operation
    [[nodiscard]] OnOpFunc functionToExecuteOnEachOperation() {
        return [=]( uint64_t _steps, uint64_t _pc, Instruction _inst, bigint _newMemSize,
                   bigint _gasCost, bigint _gas, VMFace const* _vm, ExtVMFace const* _extVM ) {
            ( *this )( _steps, _pc, _inst, _newMemSize, _gasCost, _gas, _vm, _extVM );
        };
    }

    // this function will be called at the end of executions
    void finalizeAndPrintTrace(
        ExecutionResult& _er, HistoricState& _statePre, HistoricState& _statePost );


    // this is to set original from balance for calls
    // in a geth call, the from account balance is always incremented to
    // make sure account has enough funds for block gas limit of gas
    // we need to save original from account balance since it is printed in trace
    void setOriginalFromBalance( const u256& _originalFromBalance );

    [[nodiscard]] Json::Value getJSONResult() const;

    [[nodiscard]] const std::shared_ptr< FunctionCallRecord >& getTopFunctionCall() const;

    [[nodiscard]] TraceOptions getOptions() const;

    [[nodiscard]] const std::map< Address, std::map< u256, u256 > >& getAccessedStorageValues()
        const;

    [[nodiscard]] const std::set< Address >& getAccessedAccounts() const;

    [[nodiscard]] const h256& getTxHash() const;

    [[nodiscard]] const shared_ptr< vector< shared_ptr< OpExecutionRecord > > >&
    getOpRecordsSequence() const;

    [[nodiscard]] const Address& getDeployedContractAddress() const;

    [[nodiscard]] const std::shared_ptr< FunctionCallRecord >& getCurrentlyExecutingFunctionCall()
        const;

    [[nodiscard]] const Address& getBlockAuthor() const;

    [[nodiscard]] const u256& getMinerPayment() const;

    [[nodiscard]] const u256& getOriginalFromBalance() const;

    [[nodiscard]] bool isCall() const;

    [[nodiscard]] const Address& getFrom() const;

    [[nodiscard]] uint64_t getTotalGasUsed() const;

    [[nodiscard]] const u256& getGasLimit() const;

    [[nodiscard]] const u256& getValue() const;

    [[nodiscard]] const bytes& getInputData() const;

    [[nodiscard]] const Address& getTo() const;

    [[nodiscard]] const u256& getGasPrice() const;

    [[nodiscard]] const bytes& getOutput() const;

    [[nodiscard]] bool isFailed() const;

    [[nodiscard]] evmc_status_code getEVMCStatusCode() const;

    [[nodiscard]] bool isSimpleTransfer();

    [[nodiscard]] bool isContractCreation();

    [[nodiscard]] shared_ptr< FunctionCallRecord > getNewFunction( uint64_t _executionCounter );

    [[nodiscard]] static string toGethCompatibleCompactHexPrefixed( const u256& _value );


private:
    void setCurrentlyExecutingFunctionCall(
        const std::shared_ptr< FunctionCallRecord >& _currentlyExecutingFunctionCall );

    void setTopFunctionCall( const std::shared_ptr< FunctionCallRecord >& _topFunctionCall );

    // this operator will be executed by skaled on each EVM instruction
    void operator()( uint64_t _steps, uint64_t _pc, Instruction _inst, bigint _newMemSize,
        bigint _gasOpGas, bigint _gasRemaining, VMFace const* _vm, ExtVMFace const* _voidExt );

    // called to record execution of each instruction
    void recordInstructionIsExecuted( uint64_t _pc, Instruction _inst, bigint _gasOpGas,
        bigint _gasRemaining, VMFace const* _vm, ExtVMFace const* _voidExt );

    // called to record function execution when a function of this or other contract is called
    void recordFunctionIsCalled( const Address& _from, const Address& _to, uint64_t _gasLimit,
        const std::vector< std::uint8_t >& _inputData, const u256& _value );

    // called when a function returns
    void recordFunctionReturned( evmc_status_code _status,
        const std::vector< std::uint8_t >& _returnData, uint64_t _gasUsed );

    // analyze instruction and record function calls, returns and storage value
    // accesses
    void analyzeInstructionAndRecordNeededInformation( uint64_t, Instruction& _inst,
        uint64_t _gasRemaining, const ExtVMFace* _face, AlethExtVM& _ext, const LegacyVM* _vm );

    // get the currently executing smartcontract memory from EVM
    [[nodiscard]] static std::vector< std::uint8_t >
    extractSmartContractMemoryByteArrayFromStackPointer( const LegacyVM* _vm );

    // this is called when the function call depth of the current instruction is different from the
    // previous instruction. This happens when a function is called or returned.
    void processFunctionCallOrReturnIfHappened(
        const AlethExtVM& _ext, const LegacyVM* _vm, std::uint64_t _gasRemaining );


    // print all supported traces. This can be used for QA
    void printAllTraces( Json::Value& _jsonTrace, ExecutionResult& _er,
        const HistoricState& _statePre, const HistoricState& _statePost );

    void recordMinerPayment( u256 _minerGasPayment );

    void printTrace(
        ExecutionResult& _er, const HistoricState& _statePre, const HistoricState& _statePost );

    [[nodiscard]] vector< uint8_t > getInputData( const AlethExtVM& _ext ) const;

    void recordMinerFeePayment( HistoricState& _statePost );

    [[nodiscard]] std::shared_ptr< OpExecutionRecord > getLastOpRecord() const;

    std::shared_ptr< FunctionCallRecord > m_topFunctionCall;
    std::shared_ptr< FunctionCallRecord > m_currentlyExecutingFunctionCall;
    std::vector< Instruction > m_lastInst;
    Json::FastWriter m_fastWriter;
    Address m_from;
    Address m_to;
    TraceOptions m_options;
    h256 m_txHash;
    Json::Value m_jsonTrace;
    // set of all storage values accessed during execution
    std::set< Address > m_accessedAccounts;
    // std::map of all storage addresses accessed (read or write) during execution
    // for each storage address the current value if recorded
    std::map< Address, std::map< dev::u256, dev::u256 > > m_accessedStorageValues;

    std::shared_ptr< std::vector< std::shared_ptr< OpExecutionRecord > > >
        m_executionRecordSequence = nullptr;
    std::atomic< bool > m_isFinalized = false;
    NoopTracePrinter m_noopTracePrinter;
    FourByteTracePrinter m_fourByteTracePrinter;
    CallTracePrinter m_callTracePrinter;
    ReplayTracePrinter m_replayTracePrinter;
    PrestateTracePrinter m_prestateTracePrinter;
    DefaultTracePrinter m_defaultTracePrinter;

    const std::map< TraceType, TracePrinter& > m_tracePrinters;

    const Address m_blockAuthor;
    u256 m_minerPayment;
    u256 m_originalFromBalance;
    bool m_isCall;
    uint64_t m_totalGasUsed;
    u256 m_value;
    u256 m_gasLimit;
    bytes m_inputData;
    u256 m_gasPrice;
    bytes m_output;
    evmc_status_code m_evmcStatusCode;
    // this will include deployed contract address if the transaction was CREATE
    Address m_deployedContractAddress;

    // this map maps CALL or DELEGATECALL instruction counter to the corresponding
    // function record
    std::map< uint64_t, shared_ptr< FunctionCallRecord > > m_callInstructionCounterToFunctionRecord;

    shared_ptr< OpExecutionRecord > createOpExecutionRecord( uint64_t _pc, Instruction& _inst,
        const bigint& _gasOpGas, const bigint& _gasRemaining, const AlethExtVM& ext,
        const LegacyVM* _vm );
};
}  // namespace dev::eth
