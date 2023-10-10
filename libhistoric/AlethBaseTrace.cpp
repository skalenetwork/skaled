
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
    const bigint& _gasUsed, const bigint& _gasLimit, const ExtVMFace* _face, AlethExtVM& _ext, const LegacyVM* _vm ) {
    // record the account access

    STATE_CHECK( _face );
    STATE_CHECK( _vm );

    auto currentDepth = _ext.depth;

    if (currentDepth == lastDepth + 1) {
        auto data = _ext.data.toVector();
        functionCalled(lastInstruction, _ext.caller, _ext.myAddress, (uint64_t ) _gasLimit,
          data, _ext.value, 0, 0);
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
    lastDepth = currentDepth;
    lastInstruction = _inst;
}


void AlethBaseTrace::functionCalled( Instruction _type, const Address& _from, const Address& _to,
    uint64_t _gasLimit, const std::vector< uint8_t >& _inputData, const u256& _value,
    uint64_t _retOffset, uint64_t _retSize ) {

    auto nestedCall =
        std::make_shared< FunctionCall >( _type, _from, _to, _gasLimit,
        lastFunctionCall, _inputData,
            _value, lastDepth + 1, _retOffset, _retSize );

    if (lastDepth >= 0) {
        // not the fist call
        STATE_CHECK( lastFunctionCall );
        STATE_CHECK( lastFunctionCall->getDepth() == lastDepth)
        lastFunctionCall->addNestedCall( nestedCall );
        lastFunctionCall = nestedCall;
    } else {
        STATE_CHECK(!lastFunctionCall )
    }
    lastFunctionCall = nestedCall;
}

void AlethBaseTrace::functionReturned( std::vector< uint8_t >& _outputData, uint64_t _gasUsed,
    std::string& _error, std::string& _revertReason ) {
    lastFunctionCall->setOutputData( _outputData );
    lastFunctionCall->setError( _error );
    lastFunctionCall->setGasUsed( _gasUsed );
    lastFunctionCall->setRevertReason( _revertReason );

    if ( lastFunctionCall == topFunctionCall ) {
        // the top function returned
        return;
    }

    auto parentCall = lastFunctionCall->getParentCall().lock();

    if ( !parentCall ) {
        BOOST_THROW_EXCEPTION(
            std::runtime_error( std::string( "Null parentcall in " ) + __FUNCTION__ ) );
    }

    lastFunctionCall = parentCall;
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
int64_t AlethBaseTrace::FunctionCall::getDepth() const {
    return depth;
}


AlethBaseTrace::FunctionCall::FunctionCall( Instruction _type, const Address& _from,
    const Address& _to, uint64_t _gas, const std::weak_ptr< FunctionCall >& _parentCall,
    const std::vector< uint8_t >& _inputData, const u256& _value, int64_t _depth, uint64_t _retOffset,
    uint64_t _retSize )
    : type( _type ),
      from( _from ),
      to( _to ),
      gas( _gas ),
      parentCall( _parentCall ),
      inputData( _inputData ),
      value( _value ),
      depth( _depth ),
      _retOffset( _retOffset ),
      _retSize( _retSize ) {
    STATE_CHECK(depth >= 0)
}
const Address& AlethBaseTrace::FunctionCall::getFrom() const {
    return from;
}
const Address& AlethBaseTrace::FunctionCall::getTo() const {
    return to;
}


}  // namespace eth
}  // namespace dev
