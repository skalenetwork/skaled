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
    /*
    "trace":[{"action":{"from":"0x9d945d909ca91937d19563e30bb4dac12c860189","callType":"call",
  "gas":"0x1fafcc","input":"0x300001c90b","to":"0x1b02da8cb0d097eb8d57a175b88c7d8b47997506","value":"0x0"},
  "result":{"gasUsed":"0x26456","output":"0x0005bf"},"subtraces":5,"traceAddress":[],"type":"call"},
     */

    STATE_CHECK(_functionCall);

    Json::Value functionTrace(Json::objectValue);
    functionTrace["action"] = Json::nullValue;
    functionTrace["result"] = Json::nullValue;
    functionTrace["subtraces"] = Json::nullValue;
    functionTrace["traceAddress"] = _functionCall->getParityAddress();
    functionTrace["type"] = _functionCall->getParityTraceType();

    _outputArray.append(functionTrace);
}


}  // namespace eth
}  // namespace dev
