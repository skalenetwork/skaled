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

#include "FunctionCall.h"
#include "AlethStandardTrace.h"


namespace dev::eth {

void eth::AlethStandardTrace::deftracePrint( Json::Value& _jsonTrace, const ExecutionResult& _er,
    const HistoricState&, const HistoricState& ) {
    STATE_CHECK( _jsonTrace.isObject() )
    _jsonTrace["gas"] = ( uint64_t ) _er.gasUsed;
    STATE_CHECK(m_defaultOpTrace);
    _jsonTrace["structLogs"] = *m_defaultOpTrace;
    auto failed = _er.excepted != TransactionException::None;
    _jsonTrace["failed"] = failed;
    if ( !failed ) {
        if ( getOptions().enableReturnData ) {
            _jsonTrace["returnValue"] = toHex( _er.output );
        }
    } else {
        auto statusCode = AlethExtVM::transactionExceptionToEvmcStatusCode( _er.excepted );
        string errMessage = TracePrinter::evmErrorDescription( statusCode );
        // return message in two fields for compatibility with different tools
        _jsonTrace["returnValue"] = errMessage;
        _jsonTrace["error"] = errMessage;
    }
}

}
