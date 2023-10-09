#pragma once

#include "AlethExtVM.h"
#include "libevm/LegacyVM.h"
#include <jsonrpccpp/common/exception.h>
#include <skutils/eth_utils.h>

// therefore we limit the  memory and storage entries returned to 1024 to avoid
// denial of service attack.
// see here https://banteg.mirror.xyz/3dbuIlaHh30IPITWzfT1MFfSg6fxSssMqJ7TcjaWecM
#define MAX_MEMORY_VALUES_RETURNED 1024
#define MAX_STORAGE_VALUES_RETURNED 1024

namespace dev {
namespace eth {


class AlethBaseTrace {
protected:
    enum class TraceType { DEFAULT_TRACER, PRESTATE_TRACER, CALL_TRACER };

    struct DebugOptions {
        bool disableStorage = false;
        bool enableMemory = false;
        bool disableStack = false;
        bool enableReturnData = false;
        bool prestateDiffMode = false;
        TraceType tracerType = TraceType::DEFAULT_TRACER;
    };

    AlethBaseTrace( Transaction& _t, Json::Value const& _options );

    const DebugOptions& getOptions() const;

    void recordAccessesToAccountsAndStorageValues( uint64_t PC, Instruction& inst,
        const bigint& gasCost, const bigint& gas, const ExtVMFace* voidExt, AlethExtVM& ext,
        const LegacyVM* vm );

    AlethBaseTrace::DebugOptions debugOptions( Json::Value const& _json );

    std::vector< Instruction > m_lastInst;
    std::shared_ptr< Json::Value > m_defaultOpTrace;
    Json::FastWriter m_fastWriter;
    Address m_from;
    Address m_to;
    DebugOptions m_options;
    Json::Value jsonTrace;
    static const std::map< std::string, AlethBaseTrace::TraceType > stringToTracerMap;


    // set of all storage values accessed during execution
    std::set< Address > m_accessedAccounts;
    // map of all storage addresses accessed (read or write) during execution
    // for each storage address the current value if recorded
    std::map< Address, std::map< u256, u256 > > m_accessedStorageValues;
};
}  // namespace eth
}  // namespace dev