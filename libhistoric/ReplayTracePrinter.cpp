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


#include "ReplayTracePrinter.h"
#include "AlethStandardTrace.h"
#include "FunctionCallRecord.h"
#include "TraceStructuresAndDefs.h"

namespace dev::eth {

// print replay trace as implemented by parity
void ReplayTracePrinter::print( Json::Value& _jsonTrace, const ExecutionResult& _er,
    const HistoricState&, const HistoricState& ) {
    STATE_CHECK( _jsonTrace.isObject() )
    _jsonTrace["vmTrace"] = Json::Value::null;
    _jsonTrace["stateDiff"] = Json::Value::null;
    _jsonTrace["transactionHash"] = toHexPrefixed( m_trace.getTxHash() );
    _jsonTrace["output"] = toHexPrefixed( _er.output );
    auto failed = _er.excepted != TransactionException::None;

    if ( failed ) {
        _jsonTrace["error"] = getEvmErrorDescription( m_trace.getEVMCStatusCode() );
    }

    Json::Value functionTraceArray( Json::arrayValue );
    Json::Value emptyAddress( Json::arrayValue );

    auto topFunctionCallRecord = m_trace.getTopFunctionCall();


    // if topFunctionCallRecord is null
    // it means that no bytecodes were executed, this was purely ETH transfer
    // print nothing
    if ( topFunctionCallRecord ) {
        topFunctionCallRecord->printParityFunctionTrace( functionTraceArray, emptyAddress );
    }
    _jsonTrace["trace"] = functionTraceArray;
}

ReplayTracePrinter::ReplayTracePrinter( AlethStandardTrace& _standardTrace )
    : TracePrinter( _standardTrace, "replayTrace" ) {}

}  // namespace dev::eth

#endif
