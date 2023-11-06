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


namespace dev {
namespace eth {


void eth::AlethStandardTrace::replayTracePrint(
    ExecutionResult& _er, const HistoricState&, const HistoricState& ) {
    m_jsonTrace["vmTrace"] = Json::Value::null;
    m_jsonTrace["stateDiff"] = Json::Value::null;
    m_jsonTrace["transactionHash"] = toHexPrefixed( m_hash );
    m_jsonTrace["output"] = toHexPrefixed( _er.output );
    auto failed = _er.excepted != TransactionException::None;
    if (failed) {
        auto statusCode = AlethExtVM::transactionExceptionToEvmcStatusCode(_er.excepted);
        string errMessage = evmErrorDescription(statusCode);
        m_jsonTrace["error"] = errMessage;
    }

    Json::Value functionTraceArray(Json::arrayValue);
    printParityFunctionTrace(m_topFunctionCall, functionTraceArray);
    m_jsonTrace["trace"] = functionTraceArray;
}

void AlethStandardTrace::printParityFunctionTrace(
    shared_ptr< FunctionCall > _functionCall, Json::Value& _outputArray ) {
}


}  // namespace eth
}  // namespace dev
