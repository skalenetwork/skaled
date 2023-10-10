// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#pragma once

#include "AlethBaseTrace.h"
#include "AlethExtVM.h"
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




class AlethStandardTrace : public AlethBaseTrace {
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

    Json::Value getJSONResult() const;


private:

    std::shared_ptr< Json::Value > m_defaultOpTrace;

    uint64_t  storageValuesReturnedPre = 0;
    uint64_t  storageValuesReturnedPost = 0;
    uint64_t  storageValuesReturnedAll = 0;

    void appendOpToDefaultOpTrace( uint64_t _pc, Instruction& _inst, const bigint& _gasCost,
        const bigint& _gas, const ExtVMFace* _ext, AlethExtVM& _alethExt, const LegacyVM* _vm );

    void pstraceAddAllAccessedAccountPreValuesToTrace( Json::Value& _trace, const HistoricState& _stateBefore,
        const Address& _address );

    void pstraceAddAccountPreDiffToTrace( Json::Value& _preDiffTrace, const HistoricState& _statePre,
        const HistoricState& _statePost,
        const Address& _address );

    void pstraceAddAccountPostDiffToTracer( Json::Value& _postDiffTrace, const HistoricState& _stateBefore,
        const HistoricState& _statePost,
        const Address& _address );
    void deftraceFinalizeTrace( const ExecutionResult& _er );

    void pstraceFinalizeTrace(
        const HistoricState& _stateBefore, const HistoricState& _stateAfter );
};
}  // namespace eth
}  // namespace dev
