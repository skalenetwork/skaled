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

    class FunctionCall {
    public:
        FunctionCall( Instruction type, const Address& from, const Address& to, uint64_t gas,
            const std::weak_ptr< FunctionCall >& parentCall,
            const std::vector< uint8_t >& inputData, const u256& value, uint64_t depth );
        uint64_t getDepth() const;
        void setGasUsed( uint64_t _gasUsed );
        void setOutputData( const std::vector< uint8_t >& _outputData );
        void addNestedCall( std::shared_ptr< FunctionCall >& _nestedCall );
        void setError( const std::string& _error );
        void setRevertReason( const std::string& _revertReason );
        [[nodiscard]] const std::weak_ptr< FunctionCall >& getParentCall() const;


    private:
        Instruction type;
        Address from;
        Address to;
        uint64_t gas = 0;
        uint64_t gasUsed = 0;
        std::vector< std::shared_ptr< FunctionCall > > nestedCalls;
        std::weak_ptr<FunctionCall> parentCall;
        std::vector< uint8_t > inputData;
        std::vector< uint8_t > outputData;
        std::string error;
        std::string revertReason;
        u256 value;
        uint64_t depth = 0;
    };

    std::shared_ptr< FunctionCall > topFunctionCall;
    std::shared_ptr< FunctionCall > currentFunctionCall;


    AlethBaseTrace( Transaction& _t, Json::Value const& _options );


    [[nodiscard]] const DebugOptions& getOptions() const;

    void functionCalled( Instruction _type, const Address& _from, const Address& _to, uint64_t _gas,
        const std::vector< uint8_t >& _inputData, const u256& _value );

    void functionReturned( std::vector< uint8_t >& _outputData, uint64_t _gasUsed,
        std::string& _error, std::string& _revertReason );


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