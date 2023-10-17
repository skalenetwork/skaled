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
#include <libplatform/libplatform.h>
#include <v8.h>
#include "FunctionCall.h"
#include "AlethTraceBase.h"

namespace dev::eth {


const DebugOptions& eth::AlethTraceBase::getOptions() const {
    return m_options;
}


DebugOptions AlethTraceBase::debugOptions( Json::Value const& _json ) {
    DebugOptions op;

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

        if ( !_json["tracerConfig"]["onlyTopCall"].empty() &&
             _json["tracerConfig"]["onlyTopCall"].isBool() ) {
            op.onlyTopCall = _json["tracerConfig"]["onlyTopCall"].asBool();
        }

        if ( !_json["tracerConfig"]["withLog"].empty() &&
             _json["tracerConfig"]["withLog"].isBool() ) {
            op.withLog = _json["tracerConfig"]["withLog"].asBool();
        }

    }

    return op;
}

const map< string, TraceType > AlethTraceBase::s_stringToTracerMap = {
    { "", TraceType::STANDARD_TRACER },
    { "callTracer", TraceType::CALL_TRACER },
    { "prestateTracer", TraceType::PRESTATE_TRACER }
};

AlethTraceBase::AlethTraceBase( Transaction& _t, Json::Value const& _options )
    : m_from{ _t.from() },
      m_to( _t.to() ),
      m_lastOp(
          // the top function is executed at depth 0
          // therefore it is called from depth -1
          -1,
          // when we start execution a user transaction the top level function can  be a call
          // or a contract create
          _t.isCreation() ? Instruction::CREATE : Instruction::CALL, 0, 0 ) {
    m_options = debugOptions( _options );
    // mark from and to accounts as accessed
    m_accessedAccounts.insert( m_from );
    m_accessedAccounts.insert( m_to );

    static v8::Isolate::CreateParams create_params;
    // use unique pointer so allocator will be deleted after program exit
    static auto allocator = std::unique_ptr<v8::ArrayBuffer::Allocator>(v8::ArrayBuffer::Allocator::NewDefaultAllocator());

        STATE_CHECK(allocator);
    // use C++ 11 magic statics to initialize platform object
    // the lambda code is guaranteed to be called only once in a thread safe way
    static std::unique_ptr<v8::Platform>  platform =  []() {
        v8::V8::InitializeICUDefaultLocation("trace");
        v8::V8::InitializeExternalStartupData("trace    ");
        auto platform = v8::platform::NewDefaultPlatform();
        STATE_CHECK(platform);
        v8::V8::InitializePlatform(platform.get());
        v8::V8::Initialize();
        create_params.array_buffer_allocator = allocator.get();
        return platform;
    }();

    static v8::Isolate* isolate = v8::Isolate::New(create_params);;
    STATE_CHECK(isolate);
    static v8::Isolate::Scope isolate_scope(isolate);  // Enter the isolate
    static v8::HandleScope handle_scope(isolate);  // Handle to local scope
    static v8::Persistent<v8::Context> persistent_context(isolate, v8::Context::New(isolate));

    // reset persistent context so to run a new trace
    //persistent_context.Reset();
    //isolate->Dispose();


    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(isolate, persistent_context);
    context->Enter();
    // Create a string containing the JavaScript source code
    v8::Local<v8::String> source = v8::String::NewFromUtf8(isolate, "'Hello, ' + 'World!'");

    v8::Local<v8::Script> script = v8::Script::Compile(context, source).ToLocalChecked();
    v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

    v8::String::Utf8Value utf8(isolate, result);
    std::cout << *utf8 << std::endl;
    context->Exit();
}

void AlethTraceBase::recordAccessesToAccountsAndStorageValues( uint64_t, Instruction& _inst,
    uint64_t _lastOpGas, uint64_t _gasRemaining, const ExtVMFace* _face, AlethExtVM& _ext,
    const LegacyVM* _vm ) {
    // record the account access

    STATE_CHECK( _face )
    STATE_CHECK( _vm )

    processFunctionCallOrReturnIfHappened( _ext, _vm, ( uint64_t ) _gasRemaining );

    vector< uint8_t > returnData;

    m_accessedAccounts.insert( _ext.myAddress );

    uint64_t logTopicsCount = 0;


    switch ( _inst ) {
        // record storage accesses
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
    // NOW HANDLE SUICIDE
    case Instruction::SUICIDE:
        if ( _vm->stackSize() > 0 ) {
            m_accessedAccounts.insert( asAddress( _vm->getStackElement( 0 ) ) );
        }
        break;
        // NOW HANDLE LOGS
    case Instruction::LOG0:
    case Instruction::LOG1:
    case Instruction::LOG2:
    case Instruction::LOG3:
    case Instruction::LOG4: {
        logTopicsCount = ( uint64_t ) _inst - ( uint64_t ) Instruction::LOG0;
        STATE_CHECK( logTopicsCount <= 4 )
        if ( _vm->stackSize() < 2 + logTopicsCount )
            break;
        auto logData = extractMemoryByteArrayFromStackPointer( _vm );
        vector< u256 > topics;
        for ( uint64_t i = 0; i < logTopicsCount; i++ ) {
            topics.push_back( _vm->getStackElement( 2 + i ) );
        };
        STATE_CHECK( m_currentlyExecutingFunctionCall )
        m_currentlyExecutingFunctionCall->addLogEntry( logData, topics );
    }
    default:
        break;
    }


    m_lastOp = OpExecutionRecord( _ext.depth, _inst, _gasRemaining, _lastOpGas );
}
void AlethTraceBase::processFunctionCallOrReturnIfHappened(
    const AlethExtVM& _ext, const LegacyVM* _vm, uint64_t _gasRemaining ) {
    STATE_CHECK( _vm )

    auto currentDepth = _ext.depth;
    if ( currentDepth == m_lastOp.m_depth + 1 ) {
        // we are beginning to execute a new function
        auto data = _ext.data.toVector();
        functionCalled( _ext.caller, _ext.myAddress, _gasRemaining, data, _ext.value );
    } else if ( currentDepth == m_lastOp.m_depth - 1 ) {
        auto status = _vm->getAndClearLastCallStatus();
        functionReturned( status, _vm->getMReturnData(),
            m_currentlyExecutingFunctionCall->getFunctionGasLimit() - _gasRemaining );
    } else {
        // we should not have skipped frames
        STATE_CHECK( currentDepth == m_lastOp.m_depth )
    }
}


vector< uint8_t > AlethTraceBase::extractMemoryByteArrayFromStackPointer( const LegacyVM* _vm ) {
    STATE_CHECK( _vm )

    vector<uint8_t> result {};

    if ( _vm->stackSize() > 2 ) {
        auto b = ( uint32_t ) _vm->getStackElement( 0 );
        auto s = ( uint32_t ) _vm->getStackElement( 1 );
        if ( _vm->memory().size() > b + s ) {
            result =  {_vm->memory().begin() + b, _vm->memory().begin() + b + s };
        }
    }
    return result;
}


void AlethTraceBase::functionCalled( const Address& _from, const Address& _to, uint64_t _gasLimit,
    const vector< uint8_t >& _inputData, const u256& _value ) {
    auto nestedCall = make_shared< FunctionCall >( m_lastOp.m_op, _from, _to, _gasLimit,
        m_currentlyExecutingFunctionCall, _inputData, _value, m_lastOp.m_depth + 1 );

    if ( m_lastOp.m_depth >= 0 ) {
        // not the fist call
        STATE_CHECK( m_currentlyExecutingFunctionCall )
        STATE_CHECK( m_currentlyExecutingFunctionCall->getDepth() == m_lastOp.m_depth )
        m_currentlyExecutingFunctionCall->addNestedCall( nestedCall );
        m_currentlyExecutingFunctionCall = nestedCall;
    } else {
        STATE_CHECK( !m_currentlyExecutingFunctionCall )
        m_topFunctionCall = nestedCall;
    }
    m_currentlyExecutingFunctionCall = nestedCall;
}


void AlethTraceBase::functionReturned(
    evmc_status_code _status, const vector< uint8_t >& _returnData, uint64_t _gasUsed ) {
    STATE_CHECK( m_lastOp.m_gasRemaining >= m_lastOp.m_opGas )

    m_currentlyExecutingFunctionCall->setGasUsed(_gasUsed);

    if ( _status != evmc_status_code::EVMC_SUCCESS ) {
        m_currentlyExecutingFunctionCall->setError( evmErrorDescription( _status ) );
    }

    if ( _status == evmc_status_code::EVMC_REVERT ) {
        m_currentlyExecutingFunctionCall->setRevertReason(
            string(_returnData.begin(), _returnData.end() ) );
    } else {
        m_currentlyExecutingFunctionCall->setOutputData( _returnData );
    }

    if ( m_currentlyExecutingFunctionCall == m_topFunctionCall ) {
        // the top function returned
        return;
    } else {
        // move m_currentlyExecutingFunctionCall to the parent function
        auto parentCall = m_currentlyExecutingFunctionCall->getParentCall().lock();
        STATE_CHECK( parentCall )
        m_currentlyExecutingFunctionCall = parentCall;
    }
}


// we try to be compatible with geth messages as much as we can
string AlethTraceBase::evmErrorDescription( evmc_status_code _error ) {
    switch ( _error ) {
    case EVMC_SUCCESS:
        return "success";
    case EVMC_FAILURE:
        return "evm failure";
    case EVMC_OUT_OF_GAS:
        return "out of gas";
    case EVMC_INVALID_INSTRUCTION:
        return "invalid instruction";
    case EVMC_UNDEFINED_INSTRUCTION:
        return "undefined instruction";
    case EVMC_STACK_OVERFLOW:
        return "stack overflow";
    case EVMC_STACK_UNDERFLOW:
        return "stack underflow";
    case EVMC_BAD_JUMP_DESTINATION:
        return "invalid jump";
    case EVMC_INVALID_MEMORY_ACCESS:
        return "invalid memory access";
    case EVMC_CALL_DEPTH_EXCEEDED:
        return "call depth exceeded";
    case EVMC_STATIC_MODE_VIOLATION:
        return "static mode violation";
    case EVMC_PRECOMPILE_FAILURE:
        return "precompile failure";
    case EVMC_CONTRACT_VALIDATION_FAILURE:
        return "contract validation failure";
    case EVMC_ARGUMENT_OUT_OF_RANGE:
        return "argument out of range";
    case EVMC_INTERNAL_ERROR:
        return "internal error";
    case EVMC_REJECTED:
        return "evm rejected";
    case EVMC_OUT_OF_MEMORY:
        return "out of memory";
    default:
        return "unknown error";
    };
}


Json::Value eth::AlethTraceBase::getJSONResult() const {
    return m_jsonTrace;
}
}  // namespace dev::eth
