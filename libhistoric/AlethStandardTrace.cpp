// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include "AlethStandardTrace.h"
#include "AlethExtVM.h"
#include "libevm/LegacyVM.h"
#include <jsonrpccpp/common/exception.h>
#include <skutils/eth_utils.h>



// therefore we limit the  memory and storage entries returned to 1024 to avoid
// denial of service attack.
// see here https://banteg.mirror.xyz/3dbuIlaHh30IPITWzfT1MFfSg6fxSssMqJ7TcjaWecM
#define MAX_MEMORY_VALUES_RETURNED 1024
#define MAX_STORAGE_VALUES_RETURNED 1024


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
        for ( unsigned i = 0; ( i < memory.size() && i < MAX_MEMORY_VALUES_RETURNED ); i += 32 ) {
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
    return jsonTrace;
}

void eth::AlethStandardTrace::finalizeTrace(
    ExecutionResult& _er, HistoricState& _stateBefore, HistoricState& _stateAfter ) {
    switch ( m_options.tracerType ) {
    case TraceType::DEFAULT_TRACER:
        deftraceFinalizeTrace( _er );
        break;
    case TraceType::PRESTATE_TRACER:
        pstraceFinalizeTrace( _stateBefore, _stateAfter );
        break;
    case TraceType::CALL_TRACER:
        break;
    }
}
void eth::AlethStandardTrace::pstraceFinalizeTrace(
    const HistoricState& _stateBefore, const HistoricState& _stateAfter ) {
    Json::Value preDiff( Json::objectValue );
    Json::Value postDiff( Json::objectValue );

    for ( auto&& item : m_accessedAccounts ) {
        if ( m_options.prestateDiffMode ) {
            pstraceAddAccountPreDiffToTrace( preDiff, _stateBefore, _stateAfter, item );
            pstraceAddAccountPostDiffToTracer( postDiff, _stateBefore, _stateAfter, item );
        } else {
            pstraceAddAllAccessedAccountPreValuesToTrace( jsonTrace, _stateBefore, item );
        }
    };

    // diff mode set pre and post
    if ( m_options.prestateDiffMode ) {
        jsonTrace["pre"] = preDiff;
        jsonTrace["post"] = postDiff;
    }
}
void eth::AlethStandardTrace::deftraceFinalizeTrace( const ExecutionResult& _er ) {
    jsonTrace["gas"] = ( uint64_t ) _er.gasUsed;
    jsonTrace["structLogs"] = *m_defaultOpTrace;
    auto failed = _er.excepted != TransactionException::None;
    jsonTrace["failed"] = failed;
    if ( !failed && getOptions().enableReturnData ) {
        jsonTrace["returnValue"] = toHex( _er.output );
    } else {
        std::string errMessage;
        if ( _er.excepted == TransactionException::RevertInstruction ) {
            errMessage = skutils::eth::call_error_message_2_str( _er.output );
        }
        // return message in two fields for compatibility with different tools
        jsonTrace["returnValue"] = errMessage;
        jsonTrace["error"] = errMessage;
    }
}


// this function returns original values (pre) to result
void eth::AlethStandardTrace::pstraceAddAllAccessedAccountPreValuesToTrace( Json::Value& _trace,
    const HistoricState& _stateBefore,
    const Address& _address ) {
    Json::Value storagePreValues;
    // if this _address did not exist, we do not include it in the diff
    if ( !_stateBefore.addressInUse( _address ) )
        return;
    storagePreValues["balance"] = toCompactHexPrefixed( _stateBefore.balance( _address ) );
    storagePreValues["nonce"] = ( uint64_t ) _stateBefore.getNonce( _address );

    bytes const& code = _stateBefore.code( _address );
    if ( code != NullBytes ) {
        storagePreValues["code"] = toHexPrefixed( code );
    }

    Json::Value storagePairs;
    if ( m_accessedStorageValues.find( _address ) != m_accessedStorageValues.end() ) {
        for ( auto&& it : m_accessedStorageValues[_address] ) {
            if ( _stateBefore.addressInUse( _address ) ) {
                auto& storageAddress = it.first;
                auto originalValue = _stateBefore.originalStorageValue( _address, storageAddress );
                if ( originalValue ) {
                    storagePairs[toHex( storageAddress )] = toHex( originalValue );
                    // return limited number of values to prevent DOS attacks
                    storageValuesReturnedAll++;
                    if (storageValuesReturnedAll >= MAX_STORAGE_VALUES_RETURNED )
                        break;
                }
            }
        }
    }

    if ( !storagePairs.empty() ) {
        storagePreValues["storage"] = storagePairs;
    }

    // if nothing changed we do not add it to the diff
    if ( !storagePreValues.empty() )
        _trace[toHexPrefixed( _address )] = storagePreValues;
}


void eth::AlethStandardTrace::pstraceAddAccountPostDiffToTracer( Json::Value& _postDiffTrace,
    const HistoricState& _stateBefore, const HistoricState& _statePost,
    const Address& _address ) {
    Json::Value value( Json::objectValue );


    // if this address does not exist post-transaction we dot include it in the trace
    if ( !_statePost.addressInUse( _address ) )
        return;

    auto balancePost = _statePost.balance( ( _address ) );
    auto noncePost = _statePost.getNonce( ( _address ) );
    auto& codePost = _statePost.code( _address ) );


    // if the new address, ot if the value changed, include in post trace
    if ( !_stateBefore.addressInUse( _address ) || _stateBefore.balance( _address ) != balancePost ) {
        value["balancePost"] = toCompactHexPrefixed( balancePost );
    }
    if ( !_stateBefore.addressInUse( _address ) || _stateBefore.getNonce( _address ) != noncePost ) {
        value["noncePost"] = ( uint64_t ) noncePost;
    }
    if ( !_stateBefore.addressInUse( _address ) || _stateBefore.code( _address ) != codePost ) {
        value["codePost"] = toHexPrefixed( codePost );
    }


    // post diffs for storage values
    if ( m_accessedStorageValues.find( _address ) != m_accessedStorageValues.end() ) {
        Json::Value storagePairs( Json::objectValue );

        // iterate over all accessed storage values
        for ( auto&& it : m_accessedStorageValues[_address] ) {
            auto& storageAddress = it.first;
            auto& storageValue = it.second;

            bool includePair;
            if ( !_stateBefore.addressInUse( _address ) ) {
                // a new storage pair created. Include it in post diff
                includePair = true;
            } else if ( storageValue == 0 ) {
                // the value has been deleted. We do not include it in post diff
                includePair = false;
            } else {
                // see if the storage value has been changed
                includePair =
                    _stateBefore.originalStorageValue( _address, storageAddress ) != storageValue;
            }

            if ( includePair ) {
                storagePairs[toHex( storageAddress )] = toHex( storageValue );
                // return limited number of storage pairs to prevent DOS attacks
                storageValuesReturnedPost++;
                if (storageValuesReturnedPost >= MAX_STORAGE_VALUES_RETURNED )
                    break;
            }
        }

        if ( !storagePairs.empty() )
            value["storage"] = storagePairs;
    }

    _postDiffTrace[toHexPrefixed( _address )] = value;
}


void eth::AlethStandardTrace::pstraceAddAccountPreDiffToTrace( Json::Value& _preDiffTrace,
    const HistoricState& _statePre, const HistoricState& _statePost,
    const Address& _address ) {

    Json::Value value( Json::objectValue );

    // balance diff
    if ( !_statePre.addressInUse( _address ) )
        return;

    auto balance = _statePre.balance( ( _address ) );
    auto& code = _statePre.code( ( _address ) );
    auto nonce = _statePre.getNonce( ( _address ) );


    if ( !_statePost.addressInUse( _address ) || _statePost.balance( _address ) != balance ) {
        value["balance"] = toCompactHexPrefixed( balance );
    }
    if ( !_statePost.addressInUse( _address ) || _statePost.getNonce( _address ) != nonce ) {
        value["nonce"] = ( uint64_t ) nonce;
    }
    if ( !_statePost.addressInUse( _address ) || _statePost.code( _address ) != code ) {
        value["code"] = toHexPrefixed( code );
    }


    if ( m_accessedStorageValues.find( _address ) != m_accessedStorageValues.end() ) {
        Json::Value storagePairs;

        for ( auto&& it : m_accessedStorageValues[_address] ) {
            auto& storageAddress = it.first;
            auto& storageValuePost = it.second;
            bool includePair;
            if ( !_statePost.addressInUse( _address ) ) {
                // contract has been deleted. Include in diff
                includePair = true;
            } else if ( it.second == 0 ) {
                // storage has been deleted. Do not include
                includePair = false;
            } else {
                includePair =
                    _statePre.originalStorageValue( _address, storageAddress ) != storageValuePost;
            }

            if ( includePair ) {
                storagePairs[toHex( it.first )] =
                    toHex( _statePre.originalStorageValue( _address, it.first ) );
                // return limited number of storage pairs to prevent DOS attacks
                storageValuesReturnedPre++;
                if (storageValuesReturnedPre >= MAX_STORAGE_VALUES_RETURNED )
                    break;

            }
        }

        if ( !storagePairs.empty() )
            value["storage"] = storagePairs;
    }

    if ( !value.empty() )
        _preDiffTrace[toHexPrefixed( _address )] = value;
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
