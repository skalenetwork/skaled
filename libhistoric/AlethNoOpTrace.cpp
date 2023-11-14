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


void eth::AlethStandardTrace::noopTracePrint(
    Json::Value& _jsonTrace, ExecutionResult&, const HistoricState&, const HistoricState& ) {
    _jsonTrace = Json::Value(Json::ValueType::objectValue);
    // do nothing
}


}  // namespace eth
}  // namespace dev
