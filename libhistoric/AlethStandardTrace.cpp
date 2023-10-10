// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include "AlethStandardTrace.h"

namespace dev {
namespace eth {

AlethStandardTrace::AlethStandardTrace( Transaction& _t, Json::Value const& _options )
    : AlethBaseTrace( _t, _options ), m_defaultOpTrace{ std::make_shared< Json::Value >() } {}

/*
 * This function is called on each EVM op
 */
void AlethStandardTrace::operator()( uint64_t, uint64_t _pc, Instruction _inst, bigint,
    bigint _gasUsed, bigint _gasLimit, VMFace const* _vm, ExtVMFace const* _ext ) {


    STATE_CHECK(_vm)
    STATE_CHECK(_ext)


    // remove const qualifier since we need to set tracing values in AlethExtVM
    AlethExtVM& ext = ( AlethExtVM& ) ( *_ext );
    auto vm = dynamic_cast< LegacyVM const* >( _vm );
    if ( !vm ) {
        BOOST_THROW_EXCEPTION( std::runtime_error( std::string( "Null _vm in" ) + __FUNCTION__ ) );
    }

    recordAccessesToAccountsAndStorageValues( _pc, _inst, _gasUsed, _gasLimit, _ext, ext, vm );

    if ( m_options.tracerType == TraceType::DEFAULT_TRACER )
        appendOpToDefaultOpTrace( _pc, _inst, _gasUsed, _gasLimit, _ext, ext, vm );
}

void AlethStandardTrace::appendOpToDefaultOpTrace( uint64_t _pc, Instruction& _inst,
    const bigint& _gasCost, const bigint& _gas, const ExtVMFace* _ext, AlethExtVM& _alethExt,
    const LegacyVM* _vm ) {
    Json::Value r( Json::objectValue );

    STATE_CHECK(_vm)
    STATE_CHECK( _ext )

    if ( !m_options.disableStack ) {
        Json::Value stack( Json::arrayValue );
        // Try extracting information about the stack from the VM is supported.
        for ( auto const& i : _vm->stack() )
            stack.append( toCompactHexPrefixed( i, 1 ) );
        r["stack"] = stack;
    }

    bytes const& memory = _vm->memory();
    Json::Value memJson( Json::arrayValue );
    if ( m_options.enableMemory ) {
        for ( unsigned i = 0; ( i < memory.size() && i < MAX_MEMORY_VALUES_RETURNED ); i += 32 ) {
            bytesConstRef memRef( memory.data() + i, 32 );
            memJson.append( toHex( memRef ) );
        }
        r["memory"] = memJson;
    }
    r["memSize"] = static_cast< uint64_t >( memory.size() );


    r["op"] = static_cast< uint8_t >( _inst );
    r["opName"] = instructionInfo( _inst ).name;
    r["pc"] = _pc;
    r["_gas"] = static_cast< uint64_t >( _gas );
    r["_gasCost"] = static_cast< uint64_t >( _gasCost );
    r["depth"] = _ext->depth + 1;  // depth in standard trace is 1-based
    auto refund = _alethExt.sub.refunds;
    if ( refund > 0 ) {
        r["refund"] = _alethExt.sub.refunds;
    }
    if ( !m_options.disableStorage ) {
        if ( _inst == Instruction::SSTORE || _inst == Instruction::SLOAD ) {
            Json::Value storage( Json::objectValue );
            for ( auto const& i : m_accessedStorageValues[_alethExt.myAddress] )
                storage[toHex( i.first )] = toHex( i.second );
            r["storage"] = storage;
        }
    }

    if ( _inst == Instruction::REVERT ) {
        // reverted. Set error message
        // message offset and size are the last two elements
        auto b = ( uint64_t ) _vm->getStackElement( 0 );
        auto s = ( uint64_t ) _vm->getStackElement( 1 );
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
void eth::AlethStandardTrace::pstraceAddAllAccessedAccountPreValuesToTrace(
    Json::Value& _trace, const HistoricState& _stateBefore, const Address& _address ) {
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
                    if ( storageValuesReturnedAll >= MAX_STORAGE_VALUES_RETURNED )
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
    const HistoricState& _stateBefore, const HistoricState& _statePost, const Address& _address ) {
    Json::Value value( Json::objectValue );


    // if this address does not exist post-transaction we dot include it in the trace
    if ( !_statePost.addressInUse( _address ) )
        return;

    auto balancePost = _statePost.balance( _address );
    auto noncePost = _statePost.getNonce( _address );
    auto& codePost = _statePost.code( _address );


    // if the new address, ot if the value changed, include in post trace
    if ( !_stateBefore.addressInUse( _address ) ||
         _stateBefore.balance( _address ) != balancePost ) {
        value["balance"] = toCompactHexPrefixed( balancePost );
    }
    if ( !_stateBefore.addressInUse( _address ) ||
         _stateBefore.getNonce( _address ) != noncePost ) {
        value["nonce"] = ( uint64_t ) noncePost;
    }
    if ( !_stateBefore.addressInUse( _address ) || _stateBefore.code( _address ) != codePost ) {
        if ( codePost != NullBytes ) {
            value["code"] = toHexPrefixed( codePost );
        }
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
                if ( storageValuesReturnedPost >= MAX_STORAGE_VALUES_RETURNED )
                    break;
            }
        }

        if ( !storagePairs.empty() )
            value["storage"] = storagePairs;
    }

    _postDiffTrace[toHexPrefixed( _address )] = value;
}


void eth::AlethStandardTrace::pstraceAddAccountPreDiffToTrace( Json::Value& _preDiffTrace,
    const HistoricState& _statePre, const HistoricState& _statePost, const Address& _address ) {
    Json::Value value( Json::objectValue );

    // balance diff
    if ( !_statePre.addressInUse( _address ) )
        return;

    auto balance = _statePre.balance( _address );
    auto& code = _statePre.code( _address );
    auto nonce = _statePre.getNonce( _address );


    if ( !_statePost.addressInUse( _address ) || _statePost.balance( _address ) != balance ) {
        value["balance"] = toCompactHexPrefixed( balance );
    }
    if ( !_statePost.addressInUse( _address ) || _statePost.getNonce( _address ) != nonce ) {
        value["nonce"] = ( uint64_t ) nonce;
    }
    if ( !_statePost.addressInUse( _address ) || _statePost.code( _address ) != code ) {
        if ( code != NullBytes ) {
            value["code"] = toHexPrefixed( code );
        }
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
                if ( storageValuesReturnedPre >= MAX_STORAGE_VALUES_RETURNED )
                    break;
            }
        }

        if ( !storagePairs.empty() )
            value["storage"] = storagePairs;
    }

    if ( !value.empty() )
        _preDiffTrace[toHexPrefixed( _address )] = value;
}

}  // namespace eth
}  // namespace dev
