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
        TraceType tracerType = TraceType::DEFAULT_TRACER;
    };

    AlethStandardTrace::DebugOptions debugOptions( Json::Value const& _json );

    // Append json trace to given (array) value
    explicit AlethStandardTrace( Address& _from, Json::Value const& _options );

    void operator()( uint64_t _steps, uint64_t _PC, Instruction _inst, bigint _newMemSize,
        bigint _gasCost, bigint _gas, VMFace const* _vm, ExtVMFace const* _extVM );

    OnOpFunc onOp() {
        return [=]( uint64_t _steps, uint64_t _PC, Instruction _inst, bigint _newMemSize,
                   bigint _gasCost, bigint _gas, VMFace const* _vm, ExtVMFace const* _extVM ) {
            ( *this )( _steps, _PC, _inst, _newMemSize, _gasCost, _gas, _vm, _extVM );
        };
    }
    const DebugOptions& getOptions() const;
    const std::shared_ptr< Json::Value >& getResult() const;

private:
    std::vector< Instruction > m_lastInst;
    std::shared_ptr< Json::Value > m_result;
    Json::FastWriter m_fastWriter;
    Address m_from;
    DebugOptions m_options;
    std::map< Address, std::map< u256, u256 > > touchedStateBefore;
    std::map< Address, std::map< u256, u256 > > touchedStateAfter;


    static bool logStorage( Instruction _inst );


    static const std::map< std::string, AlethStandardTrace::TraceType > stringToTracerMap;
    void doDefaultTrace( uint64_t PC, Instruction& inst, const bigint& gasCost, const bigint& gas,
        const ExtVMFace* voidExt, dev::eth::AlethExtVM& ext, const dev::eth::LegacyVM* vm );

    void doCallTrace( uint64_t PC, Instruction& inst, const bigint& gasCost, const bigint& gas,
        const ExtVMFace* voidExt, dev::eth::AlethExtVM& ext, const dev::eth::LegacyVM* vm );

    void doPrestateTrace( uint64_t PC, Instruction& inst, const bigint& gasCost, const bigint& gas,
        const ExtVMFace* voidExt, dev::eth::AlethExtVM& ext, const dev::eth::LegacyVM* vm );
};
}  // namespace eth
}  // namespace dev
