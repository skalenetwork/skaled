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


#include "FunctionCall.h"
#include "AlethTraceBase.h"

namespace dev {
namespace eth {

const eth::AlethTraceBase::DebugOptions& eth::AlethTraceBase::getOptions() const {
    return m_options;
}


AlethTraceBase::DebugOptions AlethTraceBase::debugOptions( Json::Value const& _json ) {
    AlethTraceBase::DebugOptions op;

    if ( !_json.isObject() )
        BOOST_THROW_EXCEPTION( jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS, "Invalid options" ) );

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

        if ( s_stringToTracerMap.count( tracerStr ) ) {
            op.tracerType = s_stringToTracerMap.at( tracerStr );
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

const std::map< std::string, AlethTraceBase::TraceType > AlethTraceBase::s_stringToTracerMap = {
    { "", AlethTraceBase::TraceType::STANDARD_TRACER },
    { "callTracer", AlethTraceBase::TraceType::CALL_TRACER },
    { "prestateTracer", AlethTraceBase::TraceType::PRESTATE_TRACER }
};

AlethTraceBase::AlethTraceBase( Transaction& _t, Json::Value const& _options )
    : m_from{ _t.from() }, m_to( _t.to() ) {
    m_options = debugOptions( _options );
    // mark from and to accounts as accessed
    m_accessedAccounts.insert( m_from );
    m_accessedAccounts.insert( m_to );
    m_isCreate = _t.isCreation();

    // when we start execution a user transaction the top level function can  be a call
    // or a contract create
    if (m_isCreate) {
        m_lastInstruction = Instruction::CREATE;
    } else {
        m_lastInstruction = Instruction::CALL;
    }
}

void AlethTraceBase::recordAccessesToAccountsAndStorageValues( uint64_t, Instruction& _inst,
    const bigint& _lastOpGas, const bigint& _gasRemaining, const ExtVMFace* _face, AlethExtVM& _ext,
    const LegacyVM* _vm ) {
    // record the account access

    STATE_CHECK( _face )
    STATE_CHECK( _vm )

    auto currentDepth = _ext.depth;

    if ( currentDepth == m_lastDepth + 1 ) {
        // we are beginning to execute a new function
        auto data = _ext.data.toVector();
        functionCalled( _ext.caller, _ext.myAddress, ( uint64_t ) _gasRemaining, data, _ext.value );
    } else if ( currentDepth == m_lastDepth - 1 ) {
        auto status = _vm->getAndClearLastCallStatus();
        functionReturned( status );
    } else {
        // we should not have skipped frames
        STATE_CHECK( currentDepth == m_lastDepth )
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
    // NOW HANDLE FUNCTION CALL INSTRUCTIONS
    case Instruction::CALL:
    case Instruction::CALLCODE:
    case Instruction::DELEGATECALL:
    case Instruction::STATICCALL:

        if ( _vm->stackSize() > 1 ) {
            auto address = asAddress( _vm->getStackElement( 1 ) );
            m_accessedAccounts.insert( address );
        }
        break;
    // NOW HANDLE FUNCTION RETURN INSTRUCTIONS: STOP INVALID REVERT AND SUICIDE
    case Instruction::STOP:
        setNoErrors();
        break;
    case Instruction::INVALID:
        setNoErrors();
        m_lastHasError = true;
        m_lastError = "EVM_INVALID_OPCODE";
        break;
    case Instruction::RETURN:
        setNoErrors();
        break;
    case Instruction::REVERT:
        setNoErrors();
        m_lastHasReverted = true;
        m_lastHasError = true;
        m_lastError = "EVM_REVERT";
        extractReturnData( _vm );
        break;
    case Instruction::SUICIDE:
        setNoErrors();
        if ( _vm->stackSize() > 0 ) {
            m_accessedAccounts.insert( asAddress( _vm->getStackElement( 0 ) ) );
        }
        break;
    default:
        break;
    }
    m_lastDepth = currentDepth;
    m_lastInstruction = _inst;
    m_lastGasRemaining = ( uint64_t ) _gasRemaining;
    m_lastInstructionGas = ( uint64_t ) _lastOpGas;
}


void AlethTraceBase::extractReturnData( const LegacyVM* _vm ) {
    if ( _vm->stackSize() > 2 ) {
        auto b = ( uint32_t ) _vm->getStackElement( 0 );
        auto s = ( uint32_t ) _vm->getStackElement( 1 );
        if ( _vm->memory().size() > b + s ) {
            m_lastReturnData =
                std::vector< uint8_t >( _vm->memory().begin() + b, _vm->memory().begin() + b + s );
        }
    }
}


void AlethTraceBase::functionCalled( const Address& _from, const Address& _to, uint64_t _gasLimit,
    const std::vector< uint8_t >& _inputData, const u256& _value ) {
    auto nestedCall = std::make_shared< FunctionCall >( m_lastInstruction, _from, _to, _gasLimit,
        lastFunctionCall, _inputData, _value, m_lastDepth + 1 );

    if ( m_lastDepth >= 0 ) {
        // not the fist call
        STATE_CHECK( lastFunctionCall )
        STATE_CHECK( lastFunctionCall->getDepth() == m_lastDepth )
        lastFunctionCall->addNestedCall( nestedCall );
        lastFunctionCall = nestedCall;
    } else {
        STATE_CHECK( !lastFunctionCall )
        topFunctionCall = nestedCall;
    }
    lastFunctionCall = nestedCall;
}


void AlethTraceBase::functionReturned( evmc_status_code _status ) {
    STATE_CHECK( m_lastGasRemaining >= m_lastInstructionGas )

    uint64_t gasRemainingOnReturn = m_lastGasRemaining - m_lastInstructionGas;

    if ( m_lastInstruction == Instruction::INVALID ) {
        // invalid instruction consumers all gas
        gasRemainingOnReturn = 0;
    }

    lastFunctionCall->setGasUsed( lastFunctionCall->getFunctionGasLimit() - gasRemainingOnReturn );

    if ( _status != evmc_status_code::EVMC_SUCCESS ) {
        lastFunctionCall->setError( evmErrorDescription( _status ) );
    }

    if ( m_lastHasReverted ) {
        lastFunctionCall->setRevertReason(
            std::string( m_lastReturnData.begin(), m_lastReturnData.end() ) );
    } else {
        lastFunctionCall->setOutputData( m_lastReturnData );
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


std::string AlethTraceBase::evmErrorDescription( evmc_status_code _error ) {
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

void AlethTraceBase::resetLastReturnVariables() {  // reset variables.
    m_lastInstruction = Instruction::STOP;
    m_lastGasRemaining = 0;
    m_lastInstructionGas = 0;
    m_lastReturnData = std::vector< uint8_t >();
    m_lastHasReverted = false;
    m_lastHasError = false;
}



void AlethTraceBase::setNoErrors() {
    m_lastHasReverted = false;
    m_lastHasError = false;
    m_lastError = "";
    m_lastReturnData = std::vector< uint8_t >();
}

Json::Value eth::AlethTraceBase::getJSONResult() const {
    return m_jsonTrace;
}





}  // namespace eth
}  // namespace dev
