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


void eth::AlethStandardTrace::fourByteTracePrint(
    ExecutionResult& , const HistoricState& , const HistoricState&  ) {
    m_jsonTrace = Json::Value(Json::ValueType::objectValue);
    std::map<string, uint64_t> callMap;

    m_topFunctionCall->collectFourByteTrace( callMap);
    {
        for (auto&& key : callMap) {
            m_jsonTrace[key.first] = to_string(key.second);
        }
    }
}


}  // namespace eth
}  // namespace dev
