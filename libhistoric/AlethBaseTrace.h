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


// It is important that trace functions do not throw exceptions and do not modify state
// so that they do not interfere with EVM execution

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
        FunctionCall( Instruction _type, const Address& _from, const Address& _to,
            uint64_t _functionGasLimit, const std::weak_ptr< FunctionCall >& _parentCall,
            const std::vector< uint8_t >& _inputData, const u256& _value, int64_t _depth );
        int64_t getDepth() const ;
        void setGasUsed( uint64_t _gasUsed ) ;
        void setOutputData( const std::vector< uint8_t >& _outputData ) ;
        void addNestedCall( std::shared_ptr< FunctionCall >& _nestedCall ) ;
        void setError( const std::string& _error ) ;
        void setRevertReason( const std::string& _revertReason ) ;
        [[nodiscard]] const std::weak_ptr< FunctionCall >& getParentCall() const ;

        bool hasReverted() const ;
        bool hasError() const ;
        const Address& getFrom() const ;
        const Address& getTo() const ;
        uint64_t getFunctionGasLimit() const ;


    private:
        Instruction type;
        Address from;
        Address to;
        uint64_t functionGasLimit = 0;
        uint64_t gasUsed = 0;
        std::vector< std::shared_ptr< FunctionCall > > nestedCalls;
        std::weak_ptr< FunctionCall > parentCall;
        std::vector< uint8_t > inputData;
        std::vector< uint8_t > outputData;
        bool reverted = false;
        bool completedWithError = false;
        std::string error;
        std::string revertReason;
        u256 value;
        int64_t depth = 0;


    };

    std::shared_ptr< FunctionCall > topFunctionCall;
    std::shared_ptr< FunctionCall > lastFunctionCall;


    AlethBaseTrace( Transaction& _t, Json::Value const& _options ) ;


    [[nodiscard]] const DebugOptions& getOptions() const ;

    void functionCalled( const Address& _from, const Address& _to, uint64_t _gasLimit,
        const std::vector< uint8_t >& _inputData, const u256& _value ) ;

    void functionReturned() ;

    void recordAccessesToAccountsAndStorageValues( uint64_t _pc, Instruction& _inst,
        const bigint& _lastOpGas, const bigint& _gasRemaining, const ExtVMFace* _voidExt, AlethExtVM& _ext,
        const LegacyVM* _vm ) ;

    AlethBaseTrace::DebugOptions debugOptions( Json::Value const& _json ) ;

    void resetLastReturnVariables() ;

    void extractReturnData( const LegacyVM* _vm ) ;


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

    uint64_t lastInstructionGas = 0;
    uint64_t lastGasRemaining = 0;
    int64_t lastDepth = -1;
    Instruction lastInstruction = Instruction::CALL;
    std::vector<uint8_t> lastReturnData;
    bool lastHasReverted = false;
    bool lastHasError = false;
    std::string lastError;
};
}  // namespace eth
}  // namespace devCHECK_STATE(_face);