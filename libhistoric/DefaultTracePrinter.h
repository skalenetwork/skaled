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

#pragma once


#include "TraceOptions.h"
#include "TracePrinter.h"

namespace Json {
class Value;
}

namespace dev::eth {

struct ExecutionResult;
class HistoricState;

class DefaultTracePrinter : public TracePrinter {
public:
    explicit DefaultTracePrinter( AlethStandardTrace& standardTrace );

    virtual void print( Json::Value& _jsonTrace, const ExecutionResult&, const HistoricState&,
        const HistoricState& ) override;

private:
    static void appendOpToDefaultTrace( std::shared_ptr< OpExecutionRecord > _opExecutionRecord,
        std::shared_ptr< Json::Value >& _defaultTrace, TraceOptions& _traceOptions );
};
}  // namespace dev::eth
