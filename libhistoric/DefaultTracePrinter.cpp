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
            Json::Value &_jsonTrace, const ExecutionResult &, const HistoricState &, const HistoricState &) {
        STATE_CHECK(_jsonTrace.isObject())
        auto opRecordsSequence = m_trace.getOpRecordsSequence();
        STATE_CHECK(opRecordsSequence);
        auto opTrace = std::make_shared<Json::Value>();
        auto options = m_trace.getOptions();

        for (uint64_t i = 1; i < opRecordsSequence->size(); i++) { // skip first dummy entry
            auto executionRecord = opRecordsSequence->at(i);
            AlethStandardTrace::appendOpToDefaultTrace(executionRecord, opTrace, options);
        }

        if (opTrace->empty()) {
            // make it compatible with geth in cases where
            // no contract was called so there is no opTrace
            _jsonTrace["structLogs"] = Json::Value(Json::arrayValue);
        } else {
            _jsonTrace["structLogs"] = *opTrace;
        }

        _jsonTrace["failed"] = m_trace.isFailed();
        _jsonTrace["gas"] = m_trace.getTotalGasUsed();

        if (!m_trace.isFailed()) {
            if (m_trace.getOptions().enableReturnData) {
                _jsonTrace["returnValue"] = toHex(m_trace.getOutput());
            }
        } else {
            _jsonTrace["returnValue"] = "";
        }
    }

    DefaultTracePrinter::DefaultTracePrinter(AlethStandardTrace &standardTrace)
            : TracePrinter(standardTrace, "defaultTrace") {}

}  // namespace dev::eth

#endif