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

#ifdef HISTORIC_STATE

#include "AlethStandardTrace.h"
#include "FunctionCallRecord.h"
#include "TraceOptions.h"
#include <jsonrpccpp/client.h>

namespace dev::eth {

TraceOptions eth::AlethStandardTrace::getOptions() const {
    STATE_CHECK( m_isFinalized )
    return m_options;
}

void AlethStandardTrace::analyzeInstructionAndRecordNeededInformation( uint64_t, Instruction& _inst,
    uint64_t _lastOpGas, uint64_t _gasRemaining, const ExtVMFace* _face, AlethExtVM& _ext,
    const LegacyVM* _vm ) {
    STATE_CHECK( _face )
    STATE_CHECK( _vm )
    STATE_CHECK( !m_isFinalized )

    // check if instruction depth changed. This means a function has been called or has returned
    processFunctionCallOrReturnIfHappened( _ext, _vm, ( uint64_t ) _gasRemaining );


    m_accessedAccounts.insert( _ext.myAddress );


    // main analysis switch
    // analyze and record acceses to storage and accounts. as well as return data and logs
    vector< uint8_t > returnData;
    uint64_t logTopicsCount = 0;
    switch ( _inst ) {
        // record storage accesses
    case Instruction::SLOAD:
        // SLOAD - record storage access
        // the stackSize() check prevents malicios code crashing the tracer
        // by issuing SLOAD with nothing on the stack
        if ( _vm->stackSize() > 0 ) {
            m_accessedStorageValues[_ext.myAddress][_vm->getStackElement( 0 )] =
                _ext.store( _vm->getStackElement( 0 ) );
        }
        break;
    case Instruction::SSTORE:
        // STORAGE - record storage access
        if ( _vm->stackSize() > 1 ) {
            m_accessedStorageValues[_ext.myAddress][_vm->getStackElement( 0 )] =
                _vm->getStackElement( 1 );
        }
        break;
    // NOW HANDLE CONTRACT FUNCTION CALL INSTRUCTIONS
    case Instruction::CALL:
    case Instruction::CALLCODE:
    case Instruction::DELEGATECALL:
    case Instruction::STATICCALL:
        // record the contract that is called
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
        if ( _vm->stackSize() < 2 + logTopicsCount )  // incorrectly issued log instruction
            break;
        auto logData = extractSmartContractMemoryByteArrayFromStackPointer( _vm );
        vector< u256 > topics;
        for ( uint64_t i = 0; i < logTopicsCount; i++ ) {
            topics.push_back( _vm->getStackElement( 2 + i ) );
        };
        getCurrentlyExecutingFunctionCall()->addLogEntry( logData, topics );
    }
    default:
        break;
    }
    // record the instruction
    m_lastOpRecord = OpExecutionRecord( _ext.depth, _inst, _gasRemaining, _lastOpGas );
}
void AlethStandardTrace::processFunctionCallOrReturnIfHappened(
    const AlethExtVM& _ext, const LegacyVM* _vm, uint64_t _gasRemaining ) {
    STATE_CHECK( !m_isFinalized )
    STATE_CHECK( _vm )

    auto currentDepth = _ext.depth;

    // check if instruction depth changed. This means a function has been called or has returned

    if ( currentDepth == m_lastOpRecord.m_depth + 1 ) {
        // we are beginning to execute a new function
        auto data = _ext.data.toVector();
        recordFunctionIsCalled( _ext.caller, _ext.myAddress, _gasRemaining, data, _ext.value );
    } else if ( currentDepth == m_lastOpRecord.m_depth - 1 ) {
        auto status = _vm->getAndClearLastCallStatus();
        recordFunctionReturned( status, _vm->getReturnData(),
            getCurrentlyExecutingFunctionCall()->getFunctionGasLimit() - _gasRemaining );
    } else {
        // depth did not increase or decrease by one, therefore it should be the same
        STATE_CHECK( currentDepth == m_lastOpRecord.m_depth )
    }
}
const Address& AlethStandardTrace::getFrom() const {
    STATE_CHECK( m_isFinalized )
    return m_from;
}

vector< uint8_t > AlethStandardTrace::extractSmartContractMemoryByteArrayFromStackPointer(
    const LegacyVM* _vm ) {
    STATE_CHECK( _vm )

    vector< uint8_t > result{};

    if ( _vm->stackSize() > 2 ) {
        auto b = ( uint32_t ) _vm->getStackElement( 0 );
        auto s = ( uint32_t ) _vm->getStackElement( 1 );
        if ( _vm->memory().size() > b + s ) {
            result = { _vm->memory().begin() + b, _vm->memory().begin() + b + s };
        }
    }
    return result;
}


void AlethStandardTrace::recordFunctionIsCalled( const Address& _from, const Address& _to,
    uint64_t _gasLimit, const vector< uint8_t >& _inputData, const u256& _value ) {
    STATE_CHECK( !m_isFinalized )

    auto functionCall =
        make_shared< FunctionCallRecord >( m_lastOpRecord.m_op, _from, _to, _gasLimit,
            m_currentlyExecutingFunctionCall, _inputData, _value, m_lastOpRecord.m_depth + 1 );

    if ( m_lastOpRecord.m_depth >= 0 ) {
        // we are not in the top smartcontract call
        // add this call to the as a nested call to the currently
        // executing function call
        STATE_CHECK( getCurrentlyExecutingFunctionCall()->getDepth() == m_lastOpRecord.m_depth )
        getCurrentlyExecutingFunctionCall()->addNestedCall( functionCall );
    } else {
        // the top function is called
        // this happens at the beginning of the execution. When this happens, we init
        // m_lastOpRecord.m_depth to -1
        STATE_CHECK( m_lastOpRecord.m_depth == -1 )
        STATE_CHECK( !m_currentlyExecutingFunctionCall )
        // at init, m_topFuntionCall is null, set it now.
        setTopFunctionCall( functionCall );
    }
    // set the currently executing call to the funtionCall we just created
    setCurrentlyExecutingFunctionCall( functionCall );
}

void AlethStandardTrace::setTopFunctionCall(
    const shared_ptr< FunctionCallRecord >& _topFunctionCall ) {
    STATE_CHECK( _topFunctionCall )
    STATE_CHECK( !m_isFinalized )
    m_topFunctionCall = _topFunctionCall;
}

void AlethStandardTrace::recordFunctionReturned(
    evmc_status_code _status, const vector< uint8_t >& _returnData, uint64_t _gasUsed ) {
    STATE_CHECK( m_lastOpRecord.m_gasRemaining >= m_lastOpRecord.m_opGas )
    STATE_CHECK( m_currentlyExecutingFunctionCall )
    STATE_CHECK( !m_isFinalized )

    // record return values
    getCurrentlyExecutingFunctionCall()->setReturnValues( _status, _returnData, _gasUsed );

    if ( m_currentlyExecutingFunctionCall == m_topFunctionCall ) {
        // the top function returned. This is the end of the execution.
        return;
    } else {
        // move m_currentlyExecutingFunctionCall to the parent function
        // we are using a weak pointer here to avoid circular references in shared pointers
        auto parentCall = getCurrentlyExecutingFunctionCall()->getParentCall().lock();
        setCurrentlyExecutingFunctionCall( parentCall );
    }
}

// the getter functions are called by printer classes after the trace has been generated
const shared_ptr< FunctionCallRecord >& AlethStandardTrace::getTopFunctionCall() const {
    return m_topFunctionCall;
}

// get the printed result. This happens at the end of the execution
Json::Value AlethStandardTrace::getJSONResult() const {
    STATE_CHECK( m_isFinalized )
    STATE_CHECK( !m_jsonTrace.isNull() )
    return m_jsonTrace;
}

AlethStandardTrace::AlethStandardTrace(
    Transaction& _t, const Address& _blockAuthor, const TraceOptions& _options, bool _isCall )
    : m_defaultOpTrace{ std::make_shared< Json::Value >() },
      m_from{ _t.from() },
      m_to( _t.to() ),
      m_options( _options ),
      // if it is a call trace, the transaction does not have signature
      // therefore, its hash should not include signature
      m_txHash( _t.sha3( _isCall ? dev::eth::WithoutSignature : dev::eth::WithSignature ) ),
      m_lastOpRecord(
          // the top function is executed at depth 0
          // therefore it is called from depth -1
          -1,
          // when we start execution a user transaction the top level function can  be a call
          // or a contract create
          _t.isCreation() ? Instruction::CREATE : Instruction::CALL, 0, 0 ),
      m_noopTracePrinter( *this ),
      m_fourByteTracePrinter( *this ),
      m_callTracePrinter( *this ),
      m_replayTracePrinter( *this ),
      m_prestateTracePrinter( *this ),
      m_defaultTracePrinter( *this ),
      m_tracePrinters{ { TraceType::DEFAULT_TRACER, m_defaultTracePrinter },
          { TraceType::PRESTATE_TRACER, m_prestateTracePrinter },
          { TraceType::CALL_TRACER, m_callTracePrinter },
          { TraceType::REPLAY_TRACER, m_replayTracePrinter },
          { TraceType::FOUR_BYTE_TRACER, m_fourByteTracePrinter },
          { TraceType::NOOP_TRACER, m_noopTracePrinter } },
      m_blockAuthor( _blockAuthor ),
      m_isCall( _isCall ) {
    // mark from and to accounts as accessed
    m_accessedAccounts.insert( m_from );
    m_accessedAccounts.insert( m_to );
}
void AlethStandardTrace::setOriginalFromBalance( const u256& _originalFromBalance ) {
    STATE_CHECK( !m_isFinalized )
    m_originalFromBalance = _originalFromBalance;
}

/*
 * This function is called by EVM on each instruction
 */
void AlethStandardTrace::operator()( uint64_t _counter, uint64_t _pc, Instruction _inst, bigint,
    bigint _gasOpGas, bigint _gasRemaining, VMFace const* _vm, ExtVMFace const* _ext ) {
    STATE_CHECK( !m_isFinalized )
    if ( _counter ) {
        recordMinerPayment( u256( _gasOpGas ) );
    }

    recordInstructionIsExecuted( _pc, _inst, _gasOpGas, _gasRemaining, _vm, _ext );
}

// this will be called each time before an instruction is executed by evm
void AlethStandardTrace::recordInstructionIsExecuted( uint64_t _pc, Instruction _inst,
    bigint _gasOpGas, bigint _gasRemaining, VMFace const* _vm, ExtVMFace const* _voidExt ) {
    STATE_CHECK( _vm )
    STATE_CHECK( _voidExt )
    STATE_CHECK( !m_isFinalized )

    // remove const qualifier since we need to set tracing values in AlethExtVM
    AlethExtVM& ext = ( AlethExtVM& ) ( *_voidExt );
    auto vm = dynamic_cast< LegacyVM const* >( _vm );
    if ( !vm ) {
        BOOST_THROW_EXCEPTION( std::runtime_error( std::string( "Null _vm in" ) + __FUNCTION__ ) );
    }

    analyzeInstructionAndRecordNeededInformation(
        _pc, _inst, ( uint64_t ) _gasOpGas, ( uint64_t ) _gasRemaining, _voidExt, ext, vm );

    if ( m_options.tracerType == TraceType::DEFAULT_TRACER ||
         m_options.tracerType == TraceType::ALL_TRACER )
        appendOpToStandardOpTrace( _pc, _inst, _gasOpGas, _gasRemaining, _voidExt, ext, vm );
}

// append instruction record to the default trace log that logs every instruction
void AlethStandardTrace::appendOpToStandardOpTrace( uint64_t _pc, Instruction& _inst,
    const bigint& _gasCost, const bigint& _gas, const ExtVMFace* _ext, AlethExtVM& _alethExt,
    const LegacyVM* _vm ) {
    Json::Value r( Json::objectValue );

    STATE_CHECK( !m_isFinalized )
    STATE_CHECK( _vm )
    STATE_CHECK( _ext )

    if ( !m_options.disableStack ) {
        Json::Value stack( Json::arrayValue );
        // Try extracting information about the stack from the VM is supported.
        for ( auto const& i : _vm->stack() ) {
            string stackStr = toGethCompatibleCompactHexPrefixed( i );
            stack.append( stackStr );
        }
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

    string instructionStr( instructionInfo( _inst ).name );

    // make strings compatible to geth trace
    if ( instructionStr == "JUMPCI" ) {
        instructionStr = "JUMPI";
    } else if ( instructionStr == "JUMPC" ) {
        instructionStr = "JUMP";
    } else if ( instructionStr == "SHA3" ) {
        instructionStr = "KECCAK256";
    }


    r["op"] = instructionStr;
    r["pc"] = _pc;
    r["gas"] = static_cast< uint64_t >( _gas );
    r["gasCost"] = static_cast< uint64_t >( _gasCost );
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

    m_defaultOpTrace->append( r );
}

string AlethStandardTrace::toGethCompatibleCompactHexPrefixed( const u256& _value ) {
    auto hexStr = toCompactHex( _value );
    // now make it compatible with the way geth prints string
    if ( hexStr.empty() ) {
        hexStr = "0";
    } else if ( hexStr.front() == '0' ) {
        hexStr = hexStr.substr( 1 );
    }
    return "0x" + hexStr;
}

// execution completed.  Now finalize the trace and use the tracer that the user requested
// to print the resulting trace to json
void eth::AlethStandardTrace::finalizeAndPrintTrace(
    ExecutionResult& _er, HistoricState& _statePre, HistoricState& _statePost ) {
    auto totalGasUsed = ( uint64_t ) _er.gasUsed;
    auto statusCode = AlethExtVM::transactionExceptionToEvmcStatusCode( _er.excepted );

    STATE_CHECK( m_topFunctionCall == m_currentlyExecutingFunctionCall )

    // if transaction is not just ETH transfer
    // record return of the top function.
    if ( getTopFunctionCall() ) {
        recordFunctionReturned( statusCode, _er.output, totalGasUsed );
    }
    // we are done. Set the trace to finalized
    STATE_CHECK( !m_isFinalized.exchange( true ) )
    // now print trace
    printTrace( _er, _statePre, _statePost );
}
void eth::AlethStandardTrace::printTrace( ExecutionResult& _er, const HistoricState& _statePre,
    const HistoricState& _statePost ) {  // now print the trace
    m_jsonTrace = Json::Value( Json::objectValue );
    // now run the trace that the user wants based on options provided
    if ( m_tracePrinters.count( m_options.tracerType ) > 0 ) {
        m_tracePrinters.at( m_options.tracerType ).print( m_jsonTrace, _er, _statePre, _statePost );
    } else if ( m_options.tracerType == TraceType::ALL_TRACER ) {
        printAllTraces( m_jsonTrace, _er, _statePre, _statePost );
    } else {
        // this should never happen
        STATE_CHECK( false );
    }
}

// print all supported traces. This is useful for testing.
void eth::AlethStandardTrace::printAllTraces( Json::Value& _jsonTrace, ExecutionResult& _er,
    const HistoricState& _statePre, const HistoricState& _statePost ) {
    STATE_CHECK( _jsonTrace.isObject() )
    STATE_CHECK( m_isFinalized )
    Json::Value result = Json::Value( Json::ValueType::objectValue );

    for ( auto&& entry : m_tracePrinters ) {
        TracePrinter& printer = entry.second;
        printer.print( result, _er, _statePre, _statePost );
        m_jsonTrace[printer.getJsonName()] = result;
        result.clear();
    }
}

const h256& AlethStandardTrace::getTxHash() const {
    STATE_CHECK( m_isFinalized )
    return m_txHash;
}

const set< Address >& AlethStandardTrace::getAccessedAccounts() const {
    STATE_CHECK( m_isFinalized )
    return m_accessedAccounts;
}

const map< Address, map< u256, u256 > >& AlethStandardTrace::getAccessedStorageValues() const {
    STATE_CHECK( m_isFinalized )
    return m_accessedStorageValues;
}

const shared_ptr< Json::Value >& AlethStandardTrace::getDefaultOpTrace() const {
    STATE_CHECK( m_isFinalized )
    return m_defaultOpTrace;
}

const shared_ptr< FunctionCallRecord >& AlethStandardTrace::getCurrentlyExecutingFunctionCall()
    const {
    STATE_CHECK( !m_isFinalized )
    STATE_CHECK( m_currentlyExecutingFunctionCall )
    return m_currentlyExecutingFunctionCall;
}

void AlethStandardTrace::setCurrentlyExecutingFunctionCall(
    const shared_ptr< FunctionCallRecord >& _currentlyExecutingFunctionCall ) {
    STATE_CHECK( !m_isFinalized )
    STATE_CHECK( _currentlyExecutingFunctionCall )
    m_currentlyExecutingFunctionCall = _currentlyExecutingFunctionCall;
}
const Address& AlethStandardTrace::getBlockAuthor() const {
    return m_blockAuthor;
}
const u256& AlethStandardTrace::getMinerPayment() const {
    return m_minerPayment;
}
void AlethStandardTrace::recordMinerPayment( u256 _minerGasPayment ) {
    STATE_CHECK( !m_isFinalized )
    m_minerPayment = _minerGasPayment;
    // add miner to the list of accessed accounts, since the miner is paid
    // transaction fee
    m_accessedAccounts.insert( m_blockAuthor );
}
bool AlethStandardTrace::isCall() const {
    return m_isCall;
}
const u256& AlethStandardTrace::getOriginalFromBalance() const {
    return m_originalFromBalance;
}
}  // namespace dev::eth

#endif
