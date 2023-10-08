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

AlethStandardTrace::AlethStandardTrace( Transaction& _t, Json::Value const& _options )
    : m_defaultOpTrace{ std::make_shared< Json::Value >() }, m_from{ _t.from() }, m_to( _t.to() ) {
    m_options = debugOptions( _options );
    // mark from and to accounts as accessed
    m_accessedAccounts[m_from];
    m_accessedAccounts[m_to];
}


/*
 * This function is called on each EVM op
 */
void AlethStandardTrace::operator()( uint64_t, uint64_t PC, Instruction inst, bigint,
    bigint gasCost, bigint gas, VMFace const* _vm, ExtVMFace const* voidExt ) {
    // remove const qualifier since we need to set tracing values in AlethExtVM
    AlethExtVM& ext = ( AlethExtVM& ) ( *voidExt );
    auto vm = dynamic_cast< LegacyVM const* >( _vm );
    if ( !vm ) {
        BOOST_THROW_EXCEPTION( std::runtime_error( std::string( "Null _vm in" ) + __FUNCTION__ ) );
    }

    recordAccessesToAccountsAndStorageValues( PC, inst, gasCost, gas, voidExt, ext, vm );

    if ( m_options.tracerType == TraceType::DEFAULT_TRACER )
        appendOpToDefaultOpTrace( PC, inst, gasCost, gas, voidExt, ext, vm );
}
void AlethStandardTrace::recordAccessesToAccountsAndStorageValues( uint64_t PC, Instruction& inst,
    const bigint& gasCost, const bigint& gas, const ExtVMFace* voidExt, AlethExtVM& ext,
    const LegacyVM* vm ) {
    // record the account access
    m_accessedAccounts[ext.myAddress];


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
            m_accessedAccounts[asAddress( vm->getStackElement( 1 ) )];
        }
        break;
    case Instruction::BALANCE:
    case Instruction::EXTCODESIZE:
    case Instruction::EXTCODECOPY:
    case Instruction::EXTCODEHASH:
    case Instruction::SUICIDE:
        if ( vm->stackSize() > 0 ) {
            m_accessedAccounts[asAddress( vm->getStackElement( 0 ) )];
        }
        break;
    default:
        break;
    }
}


void AlethStandardTrace::appendOpToDefaultOpTrace( uint64_t PC, Instruction& inst,
    const bigint& gasCost, const bigint& gas, const ExtVMFace* voidExt, AlethExtVM& ext,
    const LegacyVM* vm ) {
    Json::Value r( Json::objectValue );

    if ( !m_options.disableStack ) {
        Json::Value stack( Json::arrayValue );
        // Try extracting information about the stack from the VM is supported.
        for ( auto const& i : vm->stack() )
            stack.append( toCompactHexPrefixed( i, 1 ) );
        r["stack"] = stack;
    }


    bytes const& memory = vm->memory();
    Json::Value memJson( Json::arrayValue );
    if ( m_options.enableMemory ) {
        for ( unsigned i = 0; ( i < memory.size() && i < MAX_MEMORY_ENTRIES_RETURNED ); i += 32 ) {
            bytesConstRef memRef( memory.data() + i, 32 );
            memJson.append( toHex( memRef ) );
        }
        r["memory"] = memJson;
    }
    r["memSize"] = static_cast< uint64_t >( memory.size() );


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
        if ( inst == Instruction::SSTORE || inst == Instruction::SLOAD ) {
            Json::Value storage( Json::objectValue );
            for ( auto const& i : m_accessedStorageValues[ext.myAddress] )
                storage[toHex( i.first )] = toHex( i.second );
            r["storage"] = storage;
        }
    }

    if ( inst == Instruction::REVERT ) {
        // reverted. Set error message
        // message offset and size are the last two elements
        auto b = ( uint64_t ) vm->getStackElement( 0 );
        auto s = ( uint64_t ) vm->getStackElement( 1 );
        std::vector< uint8_t > errorMessage( memory.begin() + b, memory.begin() + b + s );
        r["error"] = skutils::eth::call_error_message_2_str( errorMessage );
    }

    m_defaultOpTrace->append( r );
}

Json::Value eth::AlethStandardTrace::getJSONResult() const {
    return jsonResult;
}

void eth::AlethStandardTrace::finishTracing(
    ExecutionResult& _er, HistoricState& _stateBefore, HistoricState& _stateAfter ) {
    switch ( m_options.tracerType ) {
    case TraceType::DEFAULT_TRACER:
        generateDefaultTraceJSONResult( _er );
        break;
    case TraceType::PRESTATE_TRACER:
        generatePrestateTraceJSONResult( _stateBefore, _stateAfter );
        break;
    case TraceType::CALL_TRACER:
        break;
    }
}
void eth::AlethStandardTrace::generatePrestateTraceJSONResult(
    const HistoricState& _stateBefore, const HistoricState& _stateAfter ) {


     Json::Value preResult;
     Json::Value postResult;

     for ( auto&& item : m_accessedAccounts ) {
        if ( m_options.prestateDiffMode ) {
            prestateAddAccountDiffToResultBefore( preResult, _stateBefore, _stateAfter, item );
            prestateAddAccountDiffToResultBefore( postResult, _stateBefore, _stateAfter, item );
        } else {
            prestateAddAccountOriginalValueToResult(jsonResult, _stateBefore, item);
        }
    };

    if ( m_options.prestateDiffMode ) {
        jsonResult["pre"] = preResult;
        jsonResult["post"] = postResult;
    } else {
        jsonResult = preResult;
    }
}
void eth::AlethStandardTrace::generateDefaultTraceJSONResult( const ExecutionResult& _er ) {
    jsonResult["gas"] = ( uint64_t ) _er.gasUsed;
    jsonResult["structLogs"] = *m_defaultOpTrace;
    auto failed = _er.excepted != TransactionException::None;
    jsonResult["failed"] = failed;
    if ( !failed && getOptions().enableReturnData ) {
        jsonResult["returnValue"] = toHex( _er.output );
    } else {
        std::string errMessage;
        if ( _er.excepted == TransactionException::RevertInstruction ) {
            errMessage = skutils::eth::call_error_message_2_str( _er.output );
        }
        // return message in two fields for compatibility with different tools
        jsonResult["returnValue"] = errMessage;
        jsonResult["error"] = errMessage;
    }
}


void eth::AlethStandardTrace::prestateAddAccountOriginalValueToResult( Json::Value& _result,
    const HistoricState& _stateBefore,
    const std::pair< const Address, AlethStandardTrace::AccountInfo >& item ) {
    auto address = item.first;
    Json::Value value;
    if ( !_stateBefore.addressInUse( address ) )
        return;
    value["balance"] = toCompactHexPrefixed( _stateBefore.balance( address ) );
    value["nonce"] = ( uint64_t ) _stateBefore.getNonce( address );

    bytes const& code = _stateBefore.code( address );
    if ( code != NullBytes ) {
        value["code"] = toHexPrefixed( code );
    }

    Json::Value storagePairs;
    if ( m_accessedStorageValues.find( address ) != m_accessedStorageValues.end() ) {
        for ( auto&& it : m_accessedStorageValues[address] ) {
            if ( _stateBefore.addressInUse( address ) ) {
                auto originalValue = _stateBefore.originalStorageValue( address, it.first );
                if ( originalValue ) {
                    storagePairs[toHex( it.first )] = toHex( originalValue );
                }
            }
        }
    }

    if ( !storagePairs.empty() ) {
        value["storage"] = storagePairs;
    }

    _result[toHexPrefixed( address )] = value;
}


void eth::AlethStandardTrace::prestateAddAccountDiffToResultAfter( Json::Value& _result,
    const HistoricState& _stateBefore, const HistoricState& _stateAfter,
    const std::pair< const Address, AlethStandardTrace::AccountInfo >& item ) {
    auto address = item.first;
    Json::Value value;


    if ( !_stateAfter.addressInUse( address ) )
        return;

    auto balance = _stateAfter.balance( ( address ) );
    auto nonce = _stateAfter.getNonce( ( address ) );
    auto code = _stateAfter.code( ( address ) );


    if ( !_stateBefore.addressInUse( address ) || _stateBefore.balance( address ) != balance ) {
        value["balance"] = toCompactHexPrefixed( balance );
    }
    if ( !_stateBefore.addressInUse( address ) || _stateBefore.getNonce( address ) != nonce ) {
        value["nonce"] = ( uint64_t ) nonce;
    }
    if ( !_stateBefore.addressInUse( address ) || _stateBefore.code( address ) != code ) {
        value["code"] = toHexPrefixed( code );
    }


    if ( m_accessedStorageValues.find( address ) != m_accessedStorageValues.end() ) {
        Json::Value storagePairs;
        for ( auto&& it : m_accessedStorageValues[address] ) {
            bool includePair = false;
            if ( !_stateBefore.addressInUse( address ) ) {
                includePair = true;
            } else if ( it.second == 0 ) {
                includePair = false;
            } else {
                includePair = _stateBefore.originalStorageValue( address, it.first ) != it.second;
            }

            if ( includePair ) {
                storagePairs[toHex( it.first )] = toHex( it.second );
            }
        }

        if ( storagePairs.size() > 0 )
            value["storage"] = storagePairs;
    }

    _result[toHexPrefixed( address )] = value;
}


void eth::AlethStandardTrace::prestateAddAccountDiffToResultBefore( Json::Value& _result,
    const HistoricState& _stateBefore, const HistoricState& _stateAfter,
    const std::pair< const Address, AlethStandardTrace::AccountInfo >& item ) {
    auto address = item.first;
    Json::Value value;

    // balance diff
    if ( !_stateBefore.addressInUse( address ) )
        return;

    auto balance = _stateBefore.balance( ( address ) );
    auto code = _stateBefore.code( ( address ) );
    auto nonce = _stateBefore.getNonce( ( address ) );


    if ( !_stateAfter.addressInUse( address ) || _stateAfter.balance( address ) != balance ) {
        value["balance"] = toCompactHexPrefixed( balance );
    }
    if ( !_stateAfter.addressInUse( address ) || _stateAfter.getNonce( address ) != nonce ) {
        value["nonce"] = ( uint64_t ) nonce;
    }
    if ( !_stateAfter.addressInUse( address ) || _stateAfter.code( address ) != code ) {
        value["code"] = toHexPrefixed( code );
    }


    if ( m_accessedStorageValues.find( address ) != m_accessedStorageValues.end() ) {
        Json::Value storagePairs;
        for ( auto&& it : m_accessedStorageValues[address] ) {
            bool includePair = false;
            if ( !_stateAfter.addressInUse( address ) ) {
                includePair = true;
            } else if ( it.second == 0 ) {
                includePair = false;
            }

            else {
                includePair = _stateBefore.originalStorageValue( address, it.first ) != it.second;
            }

            if ( includePair ) {
                storagePairs[toHex( it.first )] = toHex( it.second );
            }
        }

        if ( storagePairs.size() > 0 )
            value["storage"] = storagePairs;
    }

    _result[toHexPrefixed( address )] = value;
}


const eth::AlethStandardTrace::DebugOptions& eth::AlethStandardTrace::getOptions() const {
    return m_options;
}


AlethStandardTrace::DebugOptions AlethStandardTrace::debugOptions( Json::Value const& _json ) {
    AlethStandardTrace::DebugOptions op;

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

}  // namespace eth
}  // namespace dev
