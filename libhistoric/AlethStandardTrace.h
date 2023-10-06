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
        bool prestateDebugMode = false;
        TraceType tracerType = TraceType::DEFAULT_TRACER;
    };


    class AccountInfo {
        uint64_t nonce = 0;
        u256 balance = 0;
        std::vector< uint8_t > code;
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

    void generateJSONResult(
        ExecutionResult& _er, HistoricState& _stateBefore, HistoricState& _stateAfter );

    Json::Value getJSONResult() const;


private:
    std::vector< Instruction > m_lastInst;
    std::shared_ptr< Json::Value > m_defaultOpTrace;
    Json::FastWriter m_fastWriter;
    Address m_from;
    Address m_to;
    DebugOptions m_options;
    Json::Value jsonResult;

    std::map< Address, std::map< u256, u256 > > m_accessedStorageValues;  ///< accessed values map.
                                                                        ///< Used for tracing
    std::map< Address, AccountInfo > m_accessedAccounts;  ///< accessed values map. Used for tracing


    static bool logStorage( Instruction _inst );


    static const std::map< std::string, AlethStandardTrace::TraceType > stringToTracerMap;
    void recordAccessesToAccountsAndStorageValues( uint64_t PC, Instruction& inst, const bigint& gasCost, const bigint& gas,
        const ExtVMFace* voidExt, AlethExtVM& ext,
        const LegacyVM* vm );

    void appendOpToDefaultOpTrace( uint64_t PC, Instruction& inst, const bigint& gasCost,
        const bigint& gas, const ExtVMFace* voidExt, AlethExtVM& ext, const LegacyVM* vm );
};
}  // namespace eth
}  // namespace dev
