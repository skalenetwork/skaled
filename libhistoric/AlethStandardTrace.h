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

#include "AlethExtVM.h"
#include "AlethTraceBase.h"
#include "json/json.h"
#include "libdevcore/Common.h"
#include "libevm/Instruction.h"
#include "libevm/LegacyVM.h"
#include "libevm/VMFace.h"
#include <cstdint>

namespace Json {
class Value;
}

namespace dev {
namespace eth {


class AlethStandardTrace : public AlethTraceBase {
public:
    // Append json trace to given (array) value
    explicit AlethStandardTrace( Transaction& _t, Json::Value const& _options );

    void operator()( uint64_t _steps, uint64_t _pc, Instruction _inst, bigint _newMemSize,
        bigint _gasOpGas, bigint _gasRemaining, VMFace const* _vm, ExtVMFace const* _voidExt );

    OnOpFunc onOp() {
        return [=]( uint64_t _steps, uint64_t _PC, Instruction _inst, bigint _newMemSize,
                   bigint _gasCost, bigint _gas, VMFace const* _vm, ExtVMFace const* _extVM ) {
            ( *this )( _steps, _PC, _inst, _newMemSize, _gasCost, _gas, _vm, _extVM );
        };
    }


    void finalizeTrace(
        ExecutionResult& _er, HistoricState& _stateBefore, HistoricState& _stateAfter );

private:
    std::shared_ptr< Json::Value > m_defaultOpTrace = nullptr;

    uint64_t storageValuesReturnedPre = 0;
    uint64_t storageValuesReturnedPost = 0;
    uint64_t storageValuesReturnedAll = 0;

    void appendOpToStandardOpTrace( uint64_t _pc, Instruction& _inst, const bigint& _gasCost,
        const bigint& _gas, const ExtVMFace* _ext, AlethExtVM& _alethExt, const LegacyVM* _vm );

    void pstracePrintAllAccessedAccountPreValues(
        Json::Value& _trace, const HistoricState& _stateBefore, const Address& _address );

    void pstracePrintAccountPreDiff( Json::Value& _preDiffTrace,
        const HistoricState& _statePre, const HistoricState& _statePost, const Address& _address );

    void pstracePrintAccountPostDiff( Json::Value& _postDiffTrace,
        const HistoricState& _stateBefore, const HistoricState& _statePost,
        const Address& _address );

    void deftracePrint( const ExecutionResult& _er, const HistoricState&,
        const HistoricState& );

    void pstracePrint(
        ExecutionResult& _er, const HistoricState& _stateBefore, const HistoricState& _stateAfter );

    void pstraceDiffPrint(
        ExecutionResult&, const HistoricState& _stateBefore, const HistoricState& _stateAfter );

    void calltracePrint(
        ExecutionResult&, const HistoricState&, const HistoricState& );

    void replayTracePrint(
        ExecutionResult&, const HistoricState&, const HistoricState& );
    void printParityFunctionTrace( shared_ptr< FunctionCall > _function, Json::Value& _outputArray,
        Json::Value _address);
};
}  // namespace eth
}  // namespace dev
