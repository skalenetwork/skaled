
#include "AlethBaseTrace.h"

namespace dev {
namespace eth {

const eth::AlethBaseTrace::DebugOptions& eth::AlethBaseTrace::getOptions() const {
    return m_options;
}


AlethBaseTrace::DebugOptions AlethBaseTrace::debugOptions( Json::Value const& _json ) {
    AlethBaseTrace::DebugOptions op;

    if ( !_json.isObject() )
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS, "Invalid options" ) );

    bool option;
    if ( !_json["disableStorage"].empty() )
        op.disableStorage = _json["disableStorage"].asBool();

    if ( !_json["enableMemory"].empty() )
        op.enableMemory = _json["enableMemory"].asBool();
    if ( !_json["disableStack"].empty() )
        op.disableStack = _json["disableStack"].asBool();
    if ( !_json["enableReturnData"].empty() )
        op.enableReturnData = _json["enableReturnData"].asBool();

    if ( !_json["tracer"].empty() ) {
        auto tracerStr = _json["tracer"].asString();

        if ( stringToTracerMap.count( tracerStr ) ) {
            op.tracerType = stringToTracerMap.at( tracerStr );
        } else {
            BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException(
                jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS, "Invalid tracer type:" + tracerStr ) );
        }
    }

    if ( !_json["tracerConfig"].empty() && _json["tracerConfig"].isObject() ) {
        if ( !_json["tracerConfig"]["diffMode"].empty() &&
             _json["tracerConfig"]["diffMode"].isBool() ) {
            op.prestateDiffMode = _json["tracerConfig"]["diffMode"].asBool();
        }
    }

    return op;
}

const std::map< std::string, AlethBaseTrace::TraceType > AlethBaseTrace::stringToTracerMap = {
    { "", AlethBaseTrace::TraceType::DEFAULT_TRACER },
    { "callTracer", AlethBaseTrace::TraceType::CALL_TRACER },
    { "prestateTracer", AlethBaseTrace::TraceType::PRESTATE_TRACER }
};

AlethBaseTrace::AlethBaseTrace( Transaction& _t, Json::Value const& _options )
    : m_from{ _t.from() }, m_to( _t.to() ) {
    m_options = debugOptions( _options );
    // mark from and to accounts as accessed
    m_accessedAccounts.insert( m_from );
    m_accessedAccounts.insert( m_to );

    resetLastReturnVariables();
}

void AlethBaseTrace::recordAccessesToAccountsAndStorageValues( uint64_t, Instruction& _inst,
    const bigint& _lastOpGas, const bigint& _gasRemaining, const ExtVMFace* _face, AlethExtVM& _ext,
    const LegacyVM* _vm ) {
    // record the account access

    STATE_CHECK( _face )
    STATE_CHECK( _vm )

    auto currentDepth = _ext.depth;

    if ( currentDepth == lastDepth + 1 ) {
        // we are beginning to execute a new function
        auto data = _ext.data.toVector();
        functionCalled( _ext.caller, _ext.myAddress, ( uint64_t ) _gasRemaining, data, _ext.value );
    } else if ( currentDepth == lastDepth - 1 ) {
        auto status = _vm->getAndClearLastCallStatus();
        functionReturned( status );
    } else {
        // we should not have skipped frames
        STATE_CHECK( currentDepth == lastDepth )
    }

    m_accessedAccounts.insert( _ext.myAddress );

    // record storage accesses
    switch ( _inst ) {
    case Instruction::SLOAD:
        if ( _vm->stackSize() > 0 ) {
            m_accessedStorageValues[_ext.myAddress][_vm->getStackElement( 0 )] =
                _ext.store( _vm->getStackElement( 0 ) );
        }
        break;
    case Instruction::SSTORE:
        if ( _vm->stackSize() > 1 ) {
            m_accessedStorageValues[_ext.myAddress][_vm->getStackElement( 0 )] =
                _vm->getStackElement( 1 );
        }
        break;
    case Instruction::CALL:
    case Instruction::CALLCODE:
    case Instruction::DELEGATECALL:
    case Instruction::STATICCALL:

        if ( _vm->stackSize() > 1 ) {
            auto address = asAddress( _vm->getStackElement( 1 ) );
            m_accessedAccounts.insert( address );
        }
        break;
    case Instruction::STOP:
        lastHasReverted = false;
        lastHasError = false;
        lastReturnData = std::vector< uint8_t >();
        break;
    case Instruction::INVALID:
        lastHasReverted = false;
        lastHasError = true;
        lastReturnData = std::vector< uint8_t >();
        lastError = "EVM_INVALID_OPCODE";
        break;
    case Instruction::RETURN:
        lastHasReverted = false;
        lastHasError = false;
        break;
    case Instruction::REVERT:
        lastHasReverted = true;
        lastHasError = false;
        lastError = "EVM_REVERT";
        extractReturnData( _vm );
        break;
    case Instruction::SUICIDE:
        lastHasReverted = false;
        lastHasError = false;
        if ( _vm->stackSize() > 0 ) {
            m_accessedAccounts.insert( asAddress( _vm->getStackElement( 0 ) ) );
        }
        lastReturnData = std::vector< uint8_t >();
        break;
    default:
        break;
    }
    lastDepth = currentDepth;
    lastInstruction = _inst;
    lastGasRemaining = ( uint64_t ) _gasRemaining;
    lastInstructionGas = ( uint64_t ) _lastOpGas;
}
void AlethBaseTrace::extractReturnData( const LegacyVM* _vm ) {
    if ( _vm->stackSize() > 2 ) {
        auto b = ( uint32_t ) _vm->getStackElement( 0 );
        auto s = ( uint32_t ) _vm->getStackElement( 1 );
        if ( _vm->memory().size() > b + s ) {
            lastReturnData =
                std::vector< uint8_t >( _vm->memory().begin() + b, _vm->memory().begin() + b + s );
        }
    }
}


void AlethBaseTrace::functionCalled( const Address& _from, const Address& _to, uint64_t _gasLimit,
    const std::vector< uint8_t >& _inputData, const u256& _value ) {
    auto nestedCall = std::make_shared< FunctionCall >( lastInstruction, _from, _to, _gasLimit,
        lastFunctionCall, _inputData, _value, lastDepth + 1 );

    if ( lastDepth >= 0 ) {
        // not the fist call
        STATE_CHECK( lastFunctionCall )
        STATE_CHECK( lastFunctionCall->getDepth() == lastDepth )
        lastFunctionCall->addNestedCall( nestedCall );
        lastFunctionCall = nestedCall;
    } else {
        STATE_CHECK( !lastFunctionCall )
    }
    lastFunctionCall = nestedCall;
}


std::string AlethBaseTrace::evmErrorDescription( evmc_status_code _error ) {
    switch ( _error ) {
    case EVMC_SUCCESS:
        return "EVM_SUCCESS";
    case EVMC_FAILURE:
        return "EVM_FAILURE";
    case EVMC_OUT_OF_GAS:
        return "EVM_OUT_OF_GAS";
    case EVMC_INVALID_INSTRUCTION:
        return "EVM_INVALID_INSTRUCTION";
    case EVMC_UNDEFINED_INSTRUCTION:
        return "EVM_UNDEFINED_INSTRUCTION";
    case EVMC_STACK_OVERFLOW:
        return "EVM_STACK_OVERFLOW";
    case EVMC_STACK_UNDERFLOW:
        return "EVM_STACK_UNDERFLOW";
    case EVMC_BAD_JUMP_DESTINATION:
        return "EVM_BAD_JUMP_DESTINATION";
    case EVMC_INVALID_MEMORY_ACCESS:
        return "EVM_INVALID_MEMORY_ACCESS";
    case EVMC_CALL_DEPTH_EXCEEDED:
        return "EVM_CALL_DEPTH_EXCEEDED";
    case EVMC_STATIC_MODE_VIOLATION:
        return "EVM_STATIC_MODE_VIOLATION";
    case EVMC_PRECOMPILE_FAILURE:
        return "EVM_PRECOMPILE_FAILURE";
    case EVMC_CONTRACT_VALIDATION_FAILURE:
        return "EVM_CONTRACT_VALIDATION_FAILURE";
    case EVMC_ARGUMENT_OUT_OF_RANGE:
        return "EVM_ARGUMENT_OUT_OF_RANGE";
    case EVMC_INTERNAL_ERROR:
        return "EVM_INTERNAL_ERROR";
    case EVMC_REJECTED:
        return "EVM_REJECTED";
    case EVMC_OUT_OF_MEMORY:
        return "EVM_OUT_OF_MEMORY";
    default:
        return "UNKNOWN_ERROR";
    };
}
void AlethBaseTrace::functionReturned( evmc_status_code _status ) {
    STATE_CHECK( lastGasRemaining >= lastInstructionGas )

    uint64_t gasRemainingOnReturn = lastGasRemaining - lastInstructionGas;

    if ( lastInstruction == Instruction::INVALID ) {
        // invalid instruction consumers all gas
        gasRemainingOnReturn = 0;
    }

    lastFunctionCall->setGasUsed( lastFunctionCall->getFunctionGasLimit() - gasRemainingOnReturn );

    if ( _status != evmc_status_code::EVMC_SUCCESS ) {
        lastFunctionCall->setError( evmErrorDescription(_status) );
    }

    if ( lastHasReverted ) {
        lastFunctionCall->setRevertReason(
            std::string( lastReturnData.begin(), lastReturnData.end() ) );
    } else {
        lastFunctionCall->setOutputData( lastReturnData );
    }

    resetLastReturnVariables();


    if ( lastFunctionCall == topFunctionCall ) {
        // the top function returned
        return;
    }

    auto parentCall = lastFunctionCall->getParentCall().lock();

    STATE_CHECK( parentCall )
    lastFunctionCall = parentCall;
}
void AlethBaseTrace::resetLastReturnVariables() {  // reset variables.
    lastInstruction = Instruction::STOP;
    lastGasRemaining = 0;
    lastInstructionGas = 0;
    lastReturnData = std::vector< uint8_t >();
    lastHasReverted = false;
    lastHasError = false;
}


void AlethBaseTrace::FunctionCall::setGasUsed( uint64_t _gasUsed ) {
    FunctionCall::gasUsed = _gasUsed;
}
uint64_t AlethBaseTrace::FunctionCall::getFunctionGasLimit() const {
    return functionGasLimit;
}
void AlethBaseTrace::FunctionCall::setOutputData( const std::vector< uint8_t >& _outputData ) {
    FunctionCall::outputData = _outputData;
}

void AlethBaseTrace::FunctionCall::addNestedCall( std::shared_ptr< FunctionCall >& _nestedCall ) {
    STATE_CHECK( _nestedCall );
    this->nestedCalls.push_back( _nestedCall );
}
void AlethBaseTrace::FunctionCall::setError( const std::string& _error ) {
    completedWithError = true;
    error = _error;
}
void AlethBaseTrace::FunctionCall::setRevertReason( const std::string& _revertReason ) {
    reverted = true;
    revertReason = _revertReason;
}
const std::weak_ptr< AlethBaseTrace::FunctionCall >& AlethBaseTrace::FunctionCall::getParentCall()
    const {
    return parentCall;
}
int64_t AlethBaseTrace::FunctionCall::getDepth() const {
    return depth;
}


AlethBaseTrace::FunctionCall::FunctionCall( Instruction _type, const Address& _from,
    const Address& _to, uint64_t _functionGasLimit,
    const std::weak_ptr< FunctionCall >& _parentCall, const std::vector< uint8_t >& _inputData,
    const u256& _value, int64_t _depth )
    : type( _type ),
      from( _from ),
      to( _to ),
      functionGasLimit( _functionGasLimit ),
      parentCall( _parentCall ),
      inputData( _inputData ),
      value( _value ),
      depth( _depth ) {
    STATE_CHECK( depth >= 0 )
}
const Address& AlethBaseTrace::FunctionCall::getFrom() const {
    return from;
}
const Address& AlethBaseTrace::FunctionCall::getTo() const {
    return to;
}

bool AlethBaseTrace::FunctionCall::hasReverted() const {
    return reverted;
}
bool AlethBaseTrace::FunctionCall::hasError() const {
    return completedWithError;
}


}  // namespace eth
}  // namespace dev
