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

#include "AlethStandardTrace.h"

namespace dev {
namespace eth {

AlethStandardTrace::AlethStandardTrace( Transaction& _t, Json::Value const& _options )
    : AlethTraceBase( _t, _options ), m_defaultOpTrace{ std::make_shared< Json::Value >() } {}

/*
 * This function is called on each EVM op
 */
void AlethStandardTrace::operator()( uint64_t, uint64_t _pc, Instruction _inst, bigint,
    bigint _gasOpGas, bigint _gasRemaining, VMFace const* _vm, ExtVMFace const* _ext ) {
    STATE_CHECK( _vm )
    STATE_CHECK( _ext )


    // remove const qualifier since we need to set tracing values in AlethExtVM
    AlethExtVM& ext = ( AlethExtVM& ) ( *_ext );
    auto vm = dynamic_cast< LegacyVM const* >( _vm );
    if ( !vm ) {
        BOOST_THROW_EXCEPTION( std::runtime_error( std::string( "Null _vm in" ) + __FUNCTION__ ) );
    }

    recordAccessesToAccountsAndStorageValues(
        _pc, _inst, ( uint64_t ) _gasOpGas, ( uint64_t ) _gasRemaining, _ext, ext, vm );

    if ( m_options.tracerType == TraceType::STANDARD_TRACER )
        appendOpToStandardOpTrace( _pc, _inst, _gasOpGas, _gasRemaining, _ext, ext, vm );
}

void AlethStandardTrace::appendOpToStandardOpTrace( uint64_t _pc, Instruction& _inst,
    const bigint& _gasCost, const bigint& _gas, const ExtVMFace* _ext, AlethExtVM& _alethExt,
    const LegacyVM* _vm ) {
    Json::Value r( Json::objectValue );

    STATE_CHECK( _vm )
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


void eth::AlethStandardTrace::finalizeTrace(
    ExecutionResult& _er, HistoricState& _stateBefore, HistoricState& _stateAfter ) {
    auto totalGasUsed = ( uint64_t ) _er.gasUsed;
    auto statusCode = AlethExtVM::transactionExceptionToEvmcStatusCode(_er.excepted);

    STATE_CHECK( m_topFunctionCall )
    STATE_CHECK( m_topFunctionCall == m_currentlyExecutingFunctionCall )
    functionReturned( statusCode, _er.output, totalGasUsed );


    switch ( m_options.tracerType ) {
    case TraceType::STANDARD_TRACER:
        deftracePrint( _er, _stateBefore, _stateAfter );
        break;
    case TraceType::PRESTATE_TRACER:
        pstracePrint( _er, _stateBefore, _stateAfter );
        break;
    case TraceType::CALL_TRACER:
        calltracePrint( _er, _stateBefore, _stateAfter );
        break;
    case TraceType::REPLAY_TRACER:
        replayTracePrint( _er, _stateBefore, _stateAfter );
        break;
    case TraceType::FOUR_BYTE_TRACER:
        fourByteTracePrint( _er, _stateBefore, _stateAfter );
        break;
    case TraceType::NOOP_TRACER:
        noopTracePrint( _er, _stateBefore, _stateAfter );
        break;
    }
}


void eth::AlethStandardTrace::deftracePrint(
    const ExecutionResult& _er, const HistoricState&, const HistoricState& ) {
    m_jsonTrace["gas"] = ( uint64_t ) _er.gasUsed;
    m_jsonTrace["structLogs"] = *m_defaultOpTrace;
    auto failed = _er.excepted != TransactionException::None;
    m_jsonTrace["failed"] = failed;
    if ( !failed)  {
        if (getOptions().enableReturnData) {
            m_jsonTrace["returnValue"] = toHex( _er.output );
        }
    } else {
        auto statusCode = AlethExtVM::transactionExceptionToEvmcStatusCode(_er.excepted);
        string errMessage = evmErrorDescription(statusCode);
        // return message in two fields for compatibility with different tools
        m_jsonTrace["returnValue"] = errMessage;
        m_jsonTrace["error"] = errMessage;
    }
}


}  // namespace eth
}  // namespace dev
