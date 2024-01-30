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

#include "TracePrinter.h"
#include "libdevcore/Address.h"

namespace Json {
class Value;
}

namespace dev::eth {

struct ExecutionResult;
class HistoricState;

class PrestateTracePrinter : public TracePrinter {
public:
    void print( Json::Value& _jsonTrace, const ExecutionResult&, const HistoricState&,
        const HistoricState& ) override;

    explicit PrestateTracePrinter( AlethStandardTrace& standardTrace );

    [[nodiscard]] u256 getMinerBalancePre( const HistoricState& _statePre ) const;
    [[nodiscard]] u256 getMinerBalancePost( const HistoricState& _statePre ) const;

private:
    void printDiff( Json::Value& _jsonTrace, const ExecutionResult&, const HistoricState& _statePre,
        const HistoricState& _statePost );

    void printAllAccessedAccountPreValues( Json::Value& _jsonTrace, const HistoricState& _statePre,
        const HistoricState& _statePost, const Address& _address );

    void printAccountPreDiff( Json::Value& _preDiffTrace, const HistoricState& _statePre,
        const HistoricState& _statePost, const Address& _address );

    void printAccountPostDiff( Json::Value& _postDiffTrace, const HistoricState& _statePre,
        const HistoricState& _statePost, const Address& _address );

    uint64_t m_storageValuesReturnedPre = 0;
    uint64_t m_storageValuesReturnedPost = 0;
    uint64_t m_storageValuesReturnedAll = 0;
    void printPre(
        Json::Value& _jsonTrace, const HistoricState& _statePre, const HistoricState& _statePost );

    void printMinerBalanceChange(
        const HistoricState& _statePre, Json::Value& preDiff, Json::Value& postDiff ) const;
    void printNonce( const HistoricState& _statePre, const HistoricState& _statePost,
        const Address& _address, Json::Value& accountPreValues ) const;
    void printPreDiffNonce( const HistoricState& _statePre, const HistoricState& _statePost,
        const Address& _address, Json::Value& _diff ) const;
    void printPostDiffNonce( const HistoricState& _statePre,
        const HistoricState& _statePost, const Address& _address, Json::Value& _diff ) const;
};
}  // namespace dev::eth
