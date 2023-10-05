// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include "AlethStandardTrace.h"
#include "AlethExtVM.h"
#include "libevm/LegacyVM.h"
#include <jsonrpccpp/common/exception.h>
#include <skutils/eth_utils.h>

// memory tracing in geth is  inefficient ahd hardly used
// see here https://banteg.mirror.xyz/3dbuIlaHh30IPITWzfT1MFfSg6fxSssMqJ7TcjaWecM
// therefore we limit the entries to 256
#define MAX_MEMORY_ENTRIES_RETURNED 256

namespace dev {
namespace eth {


const std::map< std::string, AlethStandardTrace::TraceType >
    AlethStandardTrace::stringToTracerMap = { { "", AlethStandardTrace::TraceType::DEFAULT_TRACER },
        { "callTracer", AlethStandardTrace::TraceType::CALL_TRACER },
        { "prestateTracer", AlethStandardTrace::TraceType::PRESTATE_TRACER } };

AlethStandardTrace::AlethStandardTrace( Address& _from, Json::Value const& _options )
    : m_result{ std::make_shared< Json::Value >() }, m_from{ _from } {
    m_options = debugOptions( _options );
}
bool AlethStandardTrace::logStorage( Instruction _inst ) {
    return _inst == Instruction::SSTORE || _inst == Instruction::SLOAD;
}


void AlethStandardTrace::operator()( uint64_t, uint64_t PC, Instruction inst, bigint,
    bigint gasCost, bigint gas, VMFace const* _vm, ExtVMFace const* voidExt ) {
    // remove const qualifier since we need to set tracing values in AlethExtVM
    AlethExtVM& ext = ( AlethExtVM& ) ( *voidExt );
    auto vm = dynamic_cast< LegacyVM const* >( _vm );

    if ( !vm ) {
        BOOST_THROW_EXCEPTION( std::runtime_error( std::string( "Null _vm in" ) + __FUNCTION__ ) );
    }

    switch ( m_options.tracerType ) {
    case TraceType::DEFAULT_TRACER:
        doDefaultTrace( PC, inst, gasCost, gas, voidExt, ext, vm );
        break;
    case TraceType::CALL_TRACER:
        doCallTrace( PC, inst, gasCost, gas, voidExt, ext, vm );
        break;
    case TraceType::PRESTATE_TRACER:
        doPrestateTrace( PC, inst, gasCost, gas, voidExt, ext, vm );
        break;
    }
}
void AlethStandardTrace::doDefaultTrace( uint64_t PC, Instruction& inst, const bigint& gasCost,
    const bigint& gas, const ExtVMFace* voidExt, AlethExtVM& ext, const LegacyVM* vm ) {
    Json::Value r( Json::objectValue );


    // if tracing is enabled, store the accessed value
    // you need at least one element on the stack for SLOAD and two for SSTORE
    if ( !m_options.disableStorage && inst == Instruction::SLOAD && vm->stackSize() > 0 ) {
        m_accessedStateValues[ext.myAddress][vm->getStackElement( 0 )] =
            ext.store( vm->getStackElement( 0 ) );
    }

    if ( !m_options.disableStorage && inst == Instruction::SSTORE && vm->stackSize() > 1 ) {
        m_accessedStateValues[ext.myAddress][vm->getStackElement( 0 )] = vm->getStackElement( 1 );
    }

    if ( !m_options.disableStack ) {
        Json::Value stack( Json::arrayValue );
        // Try extracting information about the stack from the VM is supported.
        for ( auto const& i : vm->stack() )
            stack.append( toCompactHexPrefixed( i, 1 ) );
        r["stack"] = stack;
    }

    if ( vm ) {
        bytes const& memory = vm->memory();

        Json::Value memJson( Json::arrayValue );
        if ( m_options.enableMemory ) {
            for ( unsigned i = 0; ( i < memory.size() && i < MAX_MEMORY_ENTRIES_RETURNED );
                  i += 32 ) {
                bytesConstRef memRef( memory.data() + i, 32 );
                memJson.append( toHex( memRef ) );
            }
            r["memory"] = memJson;
        }
        r["memSize"] = static_cast< uint64_t >( memory.size() );
    }

    r["op"] = static_cast< uint8_t >( inst );
    r["opName"] = instructionInfo( inst ).name;
    r["pc"] = PC;
    r["gas"] = static_cast< uint64_t >( gas );
    r["gasCost"] = static_cast< uint64_t >( gasCost );
    r["depth"] = voidExt->depth + 1;  // depth in standard trace is 1-based
    auto refund = ext.sub.refunds;
    if ( refund > 0 ) {
        r["refund"] = ext.sub.refunds;
    }
    if ( !m_options.disableStorage ) {
        if ( logStorage( inst ) ) {
            Json::Value storage( Json::objectValue );
            for ( auto const& i : m_accessedStateValues[ext.myAddress] )
                storage[toHex( i.first )] = toHex( i.second );
            r["storage"] = storage;
        }
    }

    if ( inst == Instruction::REVERT ) {
        // reverted. Set error message
        bytes const& memory = vm->memory();
        // message offset and size are the last two elements
        auto b = ( uint64_t ) vm->getStackElement( 0 );
        auto s = ( uint64_t ) vm->getStackElement( 1 );
        std::vector< uint8_t > errorMessage( memory.begin() + b, memory.begin() + b + s );
        r["error"] = skutils::eth::call_error_message_2_str( errorMessage );
    }

    m_result->append( r );
}
const std::shared_ptr< Json::Value >& eth::AlethStandardTrace::getResult() const {
    return m_result;
}
const eth::AlethStandardTrace::DebugOptions& eth::AlethStandardTrace::getOptions() const {
    return m_options;
}

void AlethStandardTrace::doCallTrace( uint64_t PC, Instruction& inst, const bigint& gasCost,
    const bigint& gas, const ExtVMFace* voidExt, AlethExtVM& ext, const LegacyVM* vm ) {
    Json::Value r( Json::objectValue );

    Json::Value stack( Json::arrayValue );
    if ( vm && !m_options.disableStack ) {
        // Try extracting information about the stack from the VM is supported.
        for ( auto const& i : vm->stack() )
            stack.append( toCompactHexPrefixed( i, 1 ) );
        r["stack"] = stack;
    }

    if ( m_lastInst.size() == voidExt->depth ) {
        // starting a new context
        assert( m_lastInst.size() == voidExt->depth );
        m_lastInst.push_back( inst );
    } else if ( m_lastInst.size() == voidExt->depth + 2 ) {
        m_lastInst.pop_back();
    } else if ( m_lastInst.size() == voidExt->depth + 1 ) {
        // continuing in previous context
        m_lastInst.back() = inst;
    } else {
        cwarn << "Tracing VM and more than one new/deleted stack frame between steps!";
        cwarn << "Attempting naive recovery...";
        m_lastInst.resize( voidExt->depth + 1 );
    }

    if ( vm ) {
        bytes const& memory = vm->memory();

        Json::Value memJson( Json::arrayValue );
        if ( m_options.enableMemory ) {
            for ( unsigned i = 0; i < memory.size(); i += 32 ) {
                bytesConstRef memRef( memory.data() + i, 32 );
                memJson.append( toHex( memRef ) );
            }
            r["memory"] = memJson;
        }
        r["memSize"] = static_cast< uint64_t >( memory.size() );
    }

    r["op"] = static_cast< uint8_t >( inst );
    r["opName"] = instructionInfo( inst ).name;
    r["pc"] = PC;
    r["gas"] = static_cast< uint64_t >( gas );
    r["gasCost"] = static_cast< uint64_t >( gasCost );
    r["depth"] = voidExt->depth + 1;  // depth in standard trace is 1-based
    auto refund = ext.sub.refunds;
    if ( refund > 0 ) {
        r["refund"] = ext.sub.refunds;
    }
    if ( !m_options.disableStorage ) {
        if ( logStorage( inst ) ) {
            Json::Value storage( Json::objectValue );
            for ( auto const& i : m_accessedStateValues[ext.myAddress] )
                storage[toHex( i.first )] = toHex( i.second );
            r["storage"] = storage;
        }
    }

    if ( inst == Instruction::REVERT ) {
        // reverted. Set error message
        bytes const& memory = vm->memory();
        auto st = vm->stack();
        // message offset and size are the last two elements
        uint64_t b = ( uint64_t ) * ( st.end() - 1 );
        uint64_t s = ( uint64_t ) * ( st.end() - 2 );
        std::vector< uint8_t > errorMessage( memory.begin() + b, memory.begin() + b + s );
        r["error"] = skutils::eth::call_error_message_2_str( errorMessage );
    }

    m_result->append( r );
}

void AlethStandardTrace::doPrestateTrace( uint64_t PC, Instruction& inst, const bigint& gasCost,
    const bigint& gas, const ExtVMFace* voidExt, AlethExtVM& ext, const LegacyVM* vm ) {
    Json::Value r( Json::objectValue );

    Json::Value stack( Json::arrayValue );
    if ( vm && !m_options.disableStack ) {
        // Try extracting information about the stack from the VM is supported.
        for ( auto const& i : vm->stack() )
            stack.append( toCompactHexPrefixed( i, 1 ) );
        r["stack"] = stack;
    }

    if ( m_lastInst.size() == voidExt->depth ) {
        // starting a new context
        assert( m_lastInst.size() == voidExt->depth );
        m_lastInst.push_back( inst );
    } else if ( m_lastInst.size() == voidExt->depth + 2 ) {
        m_lastInst.pop_back();
    } else if ( m_lastInst.size() == voidExt->depth + 1 ) {
        // continuing in previous context
        m_lastInst.back() = inst;
    } else {
        cwarn << "Tracing VM and more than one new/deleted stack frame between steps!";
        cwarn << "Attempting naive recovery...";
        m_lastInst.resize( voidExt->depth + 1 );
    }

    if ( vm ) {
        bytes const& memory = vm->memory();

        Json::Value memJson( Json::arrayValue );
        if ( m_options.enableMemory ) {
            for ( unsigned i = 0; i < memory.size(); i += 32 ) {
                bytesConstRef memRef( memory.data() + i, 32 );
                memJson.append( toHex( memRef ) );
            }
            r["memory"] = memJson;
        }
        r["memSize"] = static_cast< uint64_t >( memory.size() );
    }

    r["op"] = static_cast< uint8_t >( inst );
    r["opName"] = instructionInfo( inst ).name;
    r["pc"] = PC;
    r["gas"] = static_cast< uint64_t >( gas );
    r["gasCost"] = static_cast< uint64_t >( gasCost );
    r["depth"] = voidExt->depth + 1;  // depth in standard trace is 1-based
    auto refund = ext.sub.refunds;
    if ( refund > 0 ) {
        r["refund"] = ext.sub.refunds;
    }
    if ( !m_options.disableStorage ) {
        if ( logStorage( inst ) ) {
            Json::Value storage( Json::objectValue );
            for ( auto const& i : m_accessedStateValues[ext.myAddress] )
                storage[toHex( i.first )] = toHex( i.second );
            r["storage"] = storage;
        }
    }

    if ( inst == Instruction::REVERT ) {
        // reverted. Set error message
        bytes const& memory = vm->memory();
        auto st = vm->stack();
        // message offset and size are the last two elements
        uint64_t b = ( uint64_t ) * ( st.end() - 1 );
        uint64_t s = ( uint64_t ) * ( st.end() - 2 );
        std::vector< uint8_t > errorMessage( memory.begin() + b, memory.begin() + b + s );
        r["error"] = skutils::eth::call_error_message_2_str( errorMessage );
    }

    m_result->append( r );
}

AlethStandardTrace::DebugOptions AlethStandardTrace::debugOptions( Json::Value const& _json ) {
    AlethStandardTrace::DebugOptions op;
    if ( !_json.isObject() || _json.empty() )
        return op;
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

        op.enableReturnData = _json["enableReturnData"].asBool();
    }
    return op;
}

}  // namespace eth
}  // namespace dev
