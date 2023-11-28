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

#include "AlethStandardTrace.h"
#include "FunctionCall.h"
#include "TraceStructuresAndDefs.h"
#include "ReplayTracePrinter.h"


namespace dev::eth {

// print replay trace as implemented by parity
void ReplayTracePrinter::print(
    Json::Value& _jsonTrace, const ExecutionResult& _er, const HistoricState&, const HistoricState& ) {
    STATE_CHECK( _jsonTrace.isObject() )
    _jsonTrace["vmTrace"] = Json::Value::null;
    _jsonTrace["stateDiff"] = Json::Value::null;
    _jsonTrace["transactionHash"] = toHexPrefixed( m_standardTrace.getTxHash());
    _jsonTrace["output"] = toHexPrefixed( _er.output );
    auto failed = _er.excepted != TransactionException::None;
    if ( failed ) {
        auto statusCode = AlethExtVM::transactionExceptionToEvmcStatusCode( _er.excepted );
        string errMessage = getEvmErrorDescription( statusCode );
        _jsonTrace["error"] = errMessage;
    }

    Json::Value functionTraceArray( Json::arrayValue );
    Json::Value emptyAddress( Json::arrayValue );

    m_standardTrace.getTopFunctionCall()->printParityFunctionTrace( functionTraceArray, emptyAddress );
    _jsonTrace["trace"] = functionTraceArray;
}




ReplayTracePrinter::ReplayTracePrinter( AlethStandardTrace& _standardTrace )
    : TracePrinter( _standardTrace,  "replayTrace" ) {}


}  // namespace dev::eth
