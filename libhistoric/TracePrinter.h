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

#include "TraceStructuresAndDefs.h"
#include "evmc/evmc.h"
#include "libdevcore/Address.h"
#include <string>

namespace Json {
class Value;
}

namespace dev::eth {

struct ExecutionResult;
class HistoricState;
class AlethStandardTrace;

class TracePrinter {
public:
    TracePrinter( AlethStandardTrace& _standardTrace, const std::string jsonName );

    virtual void print( Json::Value& _jsonTrace, const ExecutionResult&, const HistoricState&,
        const HistoricState& ) = 0;

    [[nodiscard]] const std::string& getJsonName() const;

    static std::string getEvmErrorDescription( evmc_status_code _error );

protected:
    [[nodiscard]] static bool isNewContract(
        const HistoricState& _statePre, const HistoricState& _statePost, const Address& _address );

    [[nodiscard]] static bool isPreExistingContract(
        const HistoricState& _statePre, const Address& _address );

    AlethStandardTrace& m_trace;
    const std::string m_jsonName;
};
}  // namespace dev::eth
