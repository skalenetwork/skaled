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


#define STATE_CHECK( _EXPRESSION_ )                                             \
    if ( !( _EXPRESSION_ ) ) {                                                  \
        auto __msg__ = std::string( "State check failed::" ) + #_EXPRESSION_ + " " + \
                       std::string( __FILE__ ) + ":" + std::to_string( __LINE__ );        \
        throw std::runtime_error( __msg__);                                             \
    }


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
        FunctionCall( Instruction _type, const Address& _from, const Address& _to, uint64_t _gas,
            const std::weak_ptr< FunctionCall >& _parentCall,
            const std::vector< uint8_t >& _inputData, const u256& _value, uint64_t _depth,
            uint64_t _retOffset, uint64_t _retSize );
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
        std::weak_ptr< FunctionCall > parentCall;
        std::vector< uint8_t > inputData;
        std::vector< uint8_t > outputData;
        std::string error;
        std::string revertReason;
        u256 value;

    public:
        const Address& getFrom() const;
        const Address& getTo() const;

    private:
        uint64_t depth = 0;
        uint64_t _retOffset = 0;
        uint64_t _retSize = 0;
    };

    std::shared_ptr< FunctionCall > topFunctionCall;
    std::shared_ptr< FunctionCall > currentFunctionCall;


    AlethBaseTrace( Transaction& _t, Json::Value const& _options );


    [[nodiscard]] const DebugOptions& getOptions() const;

    void functionCalled( Instruction _type, const Address& _from, const Address& _to, uint64_t _gas,
        const std::vector< uint8_t >& _inputData, const u256& _value, uint64_t _retOffset,
        uint64_t _retSize );

    void functionReturned( std::vector< uint8_t >& _outputData, uint64_t _gasUsed,
        std::string& _error, std::string& _revertReason );


    void recordAccessesToAccountsAndStorageValues( uint64_t _pc, Instruction& _inst,
        const bigint& _gasCost, const bigint& _gas, const ExtVMFace* _voidExt, AlethExtVM& _ext,
        const LegacyVM* _vm );

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
}  // namespace devCHECK_STATE(_face);