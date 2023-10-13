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


namespace dev {
namespace eth {


void eth::AlethStandardTrace::calltraceFinalize(
    ExecutionResult& _er, const HistoricState& _stateBefore, const HistoricState& _stateAfter ) {
    auto totalGasUsed = (uint64_t) _er.gasUsed;
    STATE_CHECK(topFunctionCall)
    STATE_CHECK(topFunctionCall == this->lastFunctionCall)
    topFunctionCall->setGasUsed( totalGasUsed );
    topFunctionCall->printTrace( m_jsonTrace, 0 );
}


}  // namespace eth
}  // namespace dev
