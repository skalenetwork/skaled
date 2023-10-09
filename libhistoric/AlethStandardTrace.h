// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#pragma once

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


class AlethStandardTrace {
public:
    enum class TraceType { DEFAULT_TRACER, PRESTATE_TRACER, CALL_TRACER };

    struct DebugOptions {
        bool disableStorage = false;
        bool enableMemory = false;
        bool disableStack = false;
        bool enableReturnData = false;
        bool prestateDiffMode = false;
        TraceType tracerType = TraceType::DEFAULT_TRACER;
    };

    AlethStandardTrace::DebugOptions debugOptions( Json::Value const& _json );

    // Append json trace to given (array) value
    explicit AlethStandardTrace( Transaction& _t, Json::Value const& _options );

    void operator()( uint64_t _steps, uint64_t _PC, Instruction _inst, bigint _newMemSize,
        bigint _gasCost, bigint _gas, VMFace const* _vm, ExtVMFace const* _extVM );

    OnOpFunc onOp() {
        return [=]( uint64_t _steps, uint64_t _PC, Instruction _inst, bigint _newMemSize,
                   bigint _gasCost, bigint _gas, VMFace const* _vm, ExtVMFace const* _extVM ) {
            ( *this )( _steps, _PC, _inst, _newMemSize, _gasCost, _gas, _vm, _extVM );
        };
    }
    const DebugOptions& getOptions() const;

    void finalizeTrace(
        ExecutionResult& _er, HistoricState& _stateBefore, HistoricState& _stateAfter );

    Json::Value getJSONResult() const;


private:
    std::vector< Instruction > m_lastInst;
    std::shared_ptr< Json::Value > m_defaultOpTrace;
    Json::FastWriter m_fastWriter;
    Address m_from;
    Address m_to;
    DebugOptions m_options;
    Json::Value jsonTrace;


    // set of all storage values accessed during execution
    std::set< Address> m_accessedAccounts;
    // map of all storage addresses accessed (read or write) during execution
    // for each storage address the current value if recorded
    std::map< Address, std::map< u256, u256 > > m_accessedStorageValues;
    uint64_t  storageValuesReturnedPre = 0;
    uint64_t  storageValuesReturnedPost = 0;
    uint64_t  storageValuesReturnedAll = 0;


    static const std::map< std::string, AlethStandardTrace::TraceType > stringToTracerMap;
    void recordAccessesToAccountsAndStorageValues( uint64_t PC, Instruction& inst,
        const bigint& gasCost, const bigint& gas, const ExtVMFace* voidExt, AlethExtVM& ext,
        const LegacyVM* vm );

    void appendOpToDefaultOpTrace( uint64_t PC, Instruction& inst, const bigint& gasCost,
        const bigint& gas, const ExtVMFace* voidExt, AlethExtVM& ext, const LegacyVM* vm );

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
