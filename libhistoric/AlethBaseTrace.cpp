
#include "AlethBaseTrace.h"

namespace dev {
namespace eth {

const eth::AlethBaseTrace::DebugOptions& eth::AlethBaseTrace::getOptions() const {
    return m_options;
}


AlethBaseTrace::DebugOptions AlethBaseTrace::debugOptions( Json::Value const& _json ) {
    AlethBaseTrace::DebugOptions op;

    if ( !_json.isObject() || _json.empty() )
        return op;

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

void AlethBaseTrace::recordAccessesToAccountsAndStorageValues( uint64_t PC, Instruction& inst,
    const bigint& gasCost, const bigint& gas, const ExtVMFace* voidExt, AlethExtVM& ext,
    const LegacyVM* vm ) {
    // record the account access
    m_accessedAccounts.insert( ext.myAddress );

    // record storage accesses
    switch ( inst ) {
    case Instruction::SLOAD:
        if ( vm->stackSize() > 0 ) {
            m_accessedStorageValues[ext.myAddress][vm->getStackElement( 0 )] =
                ext.store( vm->getStackElement( 0 ) );
        }
        break;
    case Instruction::SSTORE:
        if ( vm->stackSize() > 1 ) {
            m_accessedStorageValues[ext.myAddress][vm->getStackElement( 0 )] =
                vm->getStackElement( 1 );
        }
        break;
    case Instruction::DELEGATECALL:
    case Instruction::STATICCALL:
    case Instruction::CALL:
    case Instruction::CALLCODE:
        if ( vm->stackSize() > 1 ) {
            m_accessedAccounts.insert( asAddress( vm->getStackElement( 1 ) ) );
        }
        break;
    case Instruction::BALANCE:
    case Instruction::EXTCODESIZE:
    case Instruction::EXTCODECOPY:
    case Instruction::EXTCODEHASH:
    case Instruction::SUICIDE:
        if ( vm->stackSize() > 0 ) {
            m_accessedAccounts.insert( asAddress( vm->getStackElement( 0 ) ) );
        }
        break;
    default:
        break;
    }
}

void AlethBaseTrace::FunctionCall::setGasUsed( uint64_t gasUsed ) {
    FunctionCall::gasUsed = gasUsed;
}
void AlethBaseTrace::FunctionCall::setOutputData( const std::vector< uint8_t >& outputData ) {
    FunctionCall::outputData = outputData;
}

void AlethBaseTrace::FunctionCall::addNestedCall( std::shared_ptr<FunctionCall>& _nestedCall) {
    if (!_nestedCall) {
        BOOST_THROW_EXCEPTION(std::runtime_error(std::string("Null nested call in ") + __FUNCTION__ ));
    }
    this->nestedCalls.push_back(_nestedCall);
}
void AlethBaseTrace::FunctionCall::setError( const std::string& error ) {
    FunctionCall::error = error;
}
void AlethBaseTrace::FunctionCall::setRevertReason( const std::string& revertReason ) {
    FunctionCall::revertReason = revertReason;
}
AlethBaseTrace::FunctionCall::FunctionCall( Instruction type, const Address& from,
    const Address& to, uint64_t gas, const std::vector< uint8_t >& inputData, const u256& value )
    : type( type ), from( from ), to( to ), gas( gas ), inputData( inputData ), value( value ) {}


}  // namespace eth
}  // namespace dev
