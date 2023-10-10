
#include "AlethBaseTrace.h"

namespace dev {
namespace eth {

const eth::AlethBaseTrace::DebugOptions& eth::AlethBaseTrace::getOptions() const {
    return m_options;
}


AlethBaseTrace::DebugOptions AlethBaseTrace::debugOptions( Json::Value const& _json ) {
    AlethBaseTrace::DebugOptions op;

    STATE_CHECK( _json.isObject() && !_json.empty() )

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
}

void AlethBaseTrace::recordAccessesToAccountsAndStorageValues( uint64_t, Instruction& _inst,
    const bigint&, const bigint&, const ExtVMFace* _face, AlethExtVM& _ext, const LegacyVM* _vm ) {
    // record the account access

    STATE_CHECK( _face );
    STATE_CHECK( _vm );

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
        if ( _vm->stackSize() > 6 ) {
            uint64_t gas = ( uint64_t ) _vm->getStackElement( 0 );
            auto address = asAddress( _vm->getStackElement( 1 ) );
            auto& value = _vm->getStackElement( 2 );
            uint64_t argsOffset = ( uint64_t ) _vm->getStackElement( 3 );
            uint64_t argsSize = ( uint64_t ) _vm->getStackElement( 4 );
            uint64_t retOffset = ( uint64_t ) _vm->getStackElement( 5 );
            uint64_t retSize = ( uint64_t ) _vm->getStackElement( 6 );
            m_accessedAccounts.insert( address );
            STATE_CHECK(_vm->memory().size() > argsOffset + argsSize);
            std::vector< uint8_t > data(_vm->memory().begin() + argsOffset,
                _vm->memory().end() + argsOffset + argsSize);

            functionCalled( _inst, currentFunctionCall->getTo(), address, gas, data, value,
                retOffset, retSize );
        }
        break;
    case Instruction::DELEGATECALL:
    case Instruction::STATICCALL:
        if ( _vm->stackSize() > 5 ) {
            uint64_t gas = ( uint64_t ) _vm->getStackElement( 0 );
            auto address = asAddress( _vm->getStackElement( 1 ) );
            uint64_t argsOffset = ( uint64_t ) _vm->getStackElement( 2 );
            uint64_t argsSize = ( uint64_t ) _vm->getStackElement( 3 );
            uint64_t retOffset = ( uint64_t ) _vm->getStackElement( 4 );
            uint64_t retSize = ( uint64_t ) _vm->getStackElement( 5 );
            m_accessedAccounts.insert( address );

            STATE_CHECK(_vm->memory().size() > argsOffset + argsSize);
            std::vector< uint8_t > data(_vm->memory().begin() + argsOffset,
                _vm->memory().end() + argsOffset + argsSize);
            functionCalled( _inst, currentFunctionCall->getTo(), address, gas, data, 0,
                retOffset, retSize );
        }
        break;
    case Instruction::BALANCE:
    case Instruction::EXTCODESIZE:
    case Instruction::EXTCODECOPY:
    case Instruction::EXTCODEHASH:
    case Instruction::SUICIDE:
        if ( _vm->stackSize() > 0 ) {
            m_accessedAccounts.insert( asAddress( _vm->getStackElement( 0 ) ) );
        }
        break;
    default:
        break;
    }
}


void AlethBaseTrace::functionCalled( Instruction _type, const Address& _from, const Address& _to,
    uint64_t _gas, const std::vector< uint8_t >& _inputData, const u256& _value,
    uint64_t _retOffset, uint64_t _retSize ) {
    if ( !currentFunctionCall ) {
        BOOST_THROW_EXCEPTION(
            std::runtime_error( std::string( "Null current function in " ) + __FUNCTION__ ) );
    }

    auto nestedCall =
        std::make_shared< FunctionCall >( _type, _from, _to, _gas, currentFunctionCall, _inputData,
            _value, currentFunctionCall->getDepth() + 1, _retOffset, _retSize );

    currentFunctionCall->addNestedCall( nestedCall );
    currentFunctionCall = nestedCall;
}

void AlethBaseTrace::functionReturned( std::vector< uint8_t >& _outputData, uint64_t _gasUsed,
    std::string& _error, std::string& _revertReason ) {
    currentFunctionCall->setOutputData( _outputData );
    currentFunctionCall->setError( _error );
    currentFunctionCall->setGasUsed( _gasUsed );
    currentFunctionCall->setRevertReason( _revertReason );

    if ( currentFunctionCall == topFunctionCall ) {
        // the top function returned
        return;
    }

    auto parentCall = currentFunctionCall->getParentCall().lock();

    if ( !parentCall ) {
        BOOST_THROW_EXCEPTION(
            std::runtime_error( std::string( "Null parentcall in " ) + __FUNCTION__ ) );
    }

    currentFunctionCall = parentCall;
}


void AlethBaseTrace::FunctionCall::setGasUsed( uint64_t _gasUsed ) {
    FunctionCall::gasUsed = _gasUsed;
}
void AlethBaseTrace::FunctionCall::setOutputData( const std::vector< uint8_t >& _outputData ) {
    FunctionCall::outputData = _outputData;
}

void AlethBaseTrace::FunctionCall::addNestedCall( std::shared_ptr< FunctionCall >& _nestedCall ) {
    if ( !_nestedCall ) {
        BOOST_THROW_EXCEPTION(
            std::runtime_error( std::string( "Null nested call in " ) + __FUNCTION__ ) );
    }
    this->nestedCalls.push_back( _nestedCall );
}
void AlethBaseTrace::FunctionCall::setError( const std::string& _error ) {
    FunctionCall::error = _error;
}
void AlethBaseTrace::FunctionCall::setRevertReason( const std::string& _revertReason ) {
    FunctionCall::revertReason = _revertReason;
}


const std::weak_ptr< AlethBaseTrace::FunctionCall >& AlethBaseTrace::FunctionCall::getParentCall()
    const {
    return parentCall;
}
uint64_t AlethBaseTrace::FunctionCall::getDepth() const {
    return depth;
}


AlethBaseTrace::FunctionCall::FunctionCall( Instruction _type, const Address& _from,
    const Address& _to, uint64_t _gas, const std::weak_ptr< FunctionCall >& _parentCall,
    const std::vector< uint8_t >& _inputData, const u256& _value, uint64_t depth, uint64_t _retOffset,
    uint64_t _retSize )
    : type( _type ),
      from( _from ),
      to( _to ),
      gas( _gas ),
      parentCall( _parentCall ),
      inputData( _inputData ),
      value( _value ),
      depth( depth ),
      _retOffset( _retOffset ),
      _retSize( _retSize ) {}
const Address& AlethBaseTrace::FunctionCall::getFrom() const {
    return from;
}
const Address& AlethBaseTrace::FunctionCall::getTo() const {
    return to;
}


}  // namespace eth
}  // namespace dev
