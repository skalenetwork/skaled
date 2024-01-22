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
#include "TraceStructuresAndDefs.h"


namespace dev::eth {

// default tracer as implemented by geth. Runs when no specific tracer is specified
void DefaultTracePrinter::print(
    Json::Value& _jsonTrace, const ExecutionResult&, const HistoricState&, const HistoricState& ) {
    STATE_CHECK( _jsonTrace.isObject() )
    _jsonTrace["gas"] = m_trace.getTotalGasUsed();
    auto defaultOpTrace = m_trace.getDefaultOpTrace();
    STATE_CHECK( defaultOpTrace );
    if ( defaultOpTrace->empty() ) {
        // make it compatible with geth in cases where
        // no contract was called so there is no trace
        _jsonTrace["structLogs"] = Json::Value( Json::arrayValue );
    } else {
        _jsonTrace["structLogs"] = *defaultOpTrace;
    }

    _jsonTrace["failed"] = m_trace.isFailed();
    if ( !m_trace.isFailed() ) {
        if ( m_trace.getOptions().enableReturnData ) {
            _jsonTrace["returnValue"] = toHex( m_trace.getOutput() );
        }
    } else {
        string errMessage = getEvmErrorDescription( m_trace.getStatusCode() );
        _jsonTrace["returnValue"] = errMessage;
    }
}
DefaultTracePrinter::DefaultTracePrinter( AlethStandardTrace& standardTrace )
    : TracePrinter( standardTrace, "defaultTrace" ) {}

}  // namespace dev::eth

#endif