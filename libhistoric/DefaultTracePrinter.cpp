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

#include "AlethStandardTrace.h"

#include "DefaultTracePrinter.h"
#include "FunctionCallRecord.h"
#include "TraceStructuresAndDefs.h"


namespace dev::eth {

// default tracer as implemented by geth. Runs when no specific tracer is specified
void DefaultTracePrinter::print(
    Json::Value& _jsonTrace, const ExecutionResult&, const HistoricState&, const HistoricState& ) {
    STATE_CHECK( _jsonTrace.isObject() )
    auto opRecordsSequence = m_trace.getOpRecordsSequence();
    STATE_CHECK( opRecordsSequence );
    auto opTrace = std::make_shared< Json::Value >();
    auto options = m_trace.getOptions();

    for ( uint64_t i = 1; i < opRecordsSequence->size(); i++ ) {  // skip first dummy entry
        auto executionRecord = opRecordsSequence->at( i );

        // geth reports function gas cost as op cost for CALL and DELEGATE CALL
        auto newFunction = m_trace.getNewFunction( i );
        if ( newFunction ) {
            executionRecord->m_opGas = newFunction->getGasUsed();
        }

        appendOpToDefaultTrace( executionRecord, opTrace, options );
    }

    if ( opTrace->empty() ) {
        // make it compatible with geth in cases where
        // no contract was called so there is no opTrace
        _jsonTrace["structLogs"] = Json::Value( Json::arrayValue );
    } else {
        _jsonTrace["structLogs"] = *opTrace;
    }

    _jsonTrace["failed"] = m_trace.isFailed();
    _jsonTrace["gas"] = m_trace.getTotalGasUsed();

    if ( m_trace.getOptions().enableReturnData ) {
        _jsonTrace["returnValue"] = toHex( m_trace.getOutput() );
    }
}

DefaultTracePrinter::DefaultTracePrinter( AlethStandardTrace& standardTrace )
    : TracePrinter( standardTrace, "defaultTrace" ) {}


void DefaultTracePrinter::appendOpToDefaultTrace(
    std::shared_ptr< OpExecutionRecord > _opExecutionRecord,
    std::shared_ptr< Json::Value >& _defaultTrace, TraceOptions& _traceOptions ) {
    Json::Value result( Json::objectValue );

    STATE_CHECK( _defaultTrace );
    STATE_CHECK( _opExecutionRecord )

    result["op"] = _opExecutionRecord->m_opName;
    result["pc"] = _opExecutionRecord->m_pc;
    result["gas"] = _opExecutionRecord->m_gasRemaining;


    result["gasCost"] = static_cast< uint64_t >( _opExecutionRecord->m_opGas );


    result["depth"] = _opExecutionRecord->m_depth + 1;  // depth in standard trace is 1-based

    if ( _opExecutionRecord->m_refund > 0 ) {
        result["refund"] = _opExecutionRecord->m_refund;
    }

    if ( !_traceOptions.disableStack ) {
        Json::Value stack( Json::arrayValue );
        // Try extracting information about the stack from the VM is supported.
        STATE_CHECK( _opExecutionRecord->m_stack )
        for ( auto const& i : *_opExecutionRecord->m_stack ) {
            string stackStr = AlethStandardTrace::toGethCompatibleCompactHexPrefixed( i );
            stack.append( stackStr );
        }
        result["stack"] = stack;
    }

    Json::Value memJson( Json::arrayValue );
    if ( _traceOptions.enableMemory ) {
        STATE_CHECK( _opExecutionRecord->m_memory )
        for ( unsigned i = 0;
              ( i < _opExecutionRecord->m_memory->size() && i < MAX_MEMORY_VALUES_RETURNED );
              i += 32 ) {
            bytesConstRef memRef( _opExecutionRecord->m_memory->data() + i, 32 );
            memJson.append( toHex( memRef ) );
        }
        result["memory"] = memJson;
    }

    if ( !_traceOptions.disableStorage ) {
        if ( _opExecutionRecord->m_op == Instruction::SSTORE ||
             _opExecutionRecord->m_op == Instruction::SLOAD ) {
            Json::Value storage( Json::objectValue );
            STATE_CHECK( _opExecutionRecord->m_accessedStorageValues )
            for ( auto const& i : *_opExecutionRecord->m_accessedStorageValues )
                storage[toHex( i.first )] = toHex( i.second );
            result["storage"] = storage;
        }
    }

    _defaultTrace->append( result );
}

}  // namespace dev::eth

#endif