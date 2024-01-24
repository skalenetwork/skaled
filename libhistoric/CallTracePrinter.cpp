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


#include "CallTracePrinter.h"
#include "AlethStandardTrace.h"
#include "FunctionCallRecord.h"
#include "TraceStructuresAndDefs.h"

namespace dev::eth {

// call tracer as implemented by geth
void CallTracePrinter::print(
    Json::Value& _jsonTrace, const ExecutionResult&, const HistoricState&, const HistoricState& ) {
    STATE_CHECK( _jsonTrace.isObject() )

    auto topFunctionCallRecord = m_trace.getTopFunctionCall();
    if ( !topFunctionCallRecord ) {
        // no bytecodes were executed
        printTransferTrace( _jsonTrace );
    } else {
        topFunctionCallRecord->printTrace( _jsonTrace, 0, m_trace.getOptions() );
    }


    if ( m_trace.isFailed() ) {
        _jsonTrace["error"] = getEvmErrorDescription( m_trace.getEVMCStatusCode() );
    }
}

CallTracePrinter::CallTracePrinter( AlethStandardTrace& _standardTrace )
    : TracePrinter( _standardTrace, "callTrace" ) {}


void CallTracePrinter::printTransferTrace( Json::Value& _jsonTrace ) {
    STATE_CHECK( _jsonTrace.isObject() )

    _jsonTrace["type"] = "CALL";
    _jsonTrace["from"] = toHexPrefixed( m_trace.getFrom() );
    _jsonTrace["to"] = toHexPrefixed( m_trace.getTo() );

    _jsonTrace["gas"] =
        AlethStandardTrace::toGethCompatibleCompactHexPrefixed( m_trace.getGasLimit() );
    _jsonTrace["gasUsed"] =
        AlethStandardTrace::toGethCompatibleCompactHexPrefixed( m_trace.getTotalGasUsed() );


    _jsonTrace["value"] =
        AlethStandardTrace::toGethCompatibleCompactHexPrefixed( m_trace.getValue() );

    _jsonTrace["input"] = toHexPrefixed( m_trace.getInputData() );
}


}  // namespace dev::eth


#endif