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
void CallTracePrinter::print( Json::Value& _jsonTrace, const ExecutionResult&, const HistoricState&,
    const HistoricState& _statePost ) {
    STATE_CHECK( _jsonTrace.isObject() )

    // first print error description if the transaction failed
    if ( m_trace.isFailed() ) {
        _jsonTrace["error"] = getEvmErrorDescription( m_trace.getEVMCStatusCode() );
    }

    // now deal with the cases of simple ETH transfer vs contract interaction
    if ( m_trace.isSimpleTransfer() ) {
        // no bytecode was executed
        printTransferTrace( _jsonTrace );
    } else {
        printContractTransactionTrace( _jsonTrace, _statePost );
    }
}
void CallTracePrinter::printContractTransactionTrace(
    Json::Value& _jsonTrace, const HistoricState& _statePost ) {
    auto topFunctionCall = m_trace.getTopFunctionCall();
    STATE_CHECK( topFunctionCall );
    // call trace on the top Solidity function call in the stack
    // this will also recursively call printTrace on nested calls if exist
    topFunctionCall->printTrace( _jsonTrace, _statePost, 0, m_trace.getOptions() );

    // for the top function the gas limit that geth prints is the transaction gas limit
    // and not the gas limit given to the top level function, which is smaller because of 21000
    // transaction cost and potentially eth transfer cost
    _jsonTrace["gas"] =
        AlethStandardTrace::toGethCompatibleCompactHexPrefixed( m_trace.getGasLimit() );

    // handle the case of a transaction that deploys a contract
    // in this case geth prints transaction input data as input
    // end prints to as newly created contract address
    if ( m_trace.isContractCreation() ) {
        _jsonTrace["input"] = toHexPrefixed( m_trace.getInputData() );
        _jsonTrace["to"] = toHexPrefixed( m_trace.getDeployedContractAddress() );
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