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
    uint64_t _gasRemaining, const ExtVMFace* _face, AlethExtVM& _ext, const LegacyVM* _vm ) {
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
}

void AlethStandardTrace::processFunctionCallOrReturnIfHappened(
    const AlethExtVM& _ext, const LegacyVM* _vm, uint64_t _gasRemaining ) {
    STATE_CHECK( !m_isFinalized )
    STATE_CHECK( _vm )

    auto currentDepth = _ext.depth;

    // check if instruction depth changed. This means a function has been called or has returned

    if ( currentDepth == getLastOpRecord()->m_depth + 1 ) {
        recordFunctionIsCalled(
            _ext.caller, _ext.myAddress, _gasRemaining, getInputData( _ext ), _ext.value );
    } else if ( currentDepth == getLastOpRecord()->m_depth - 1 ) {
        auto status = _vm->getAndClearLastCallStatus();

        recordFunctionReturned( status, _vm->getReturnData(),
            getCurrentlyExecutingFunctionCall()->getGasRemainingBeforeCall() - _gasRemaining );
    } else {
        // depth did not increase or decrease by one, therefore it should be the same
        STATE_CHECK( currentDepth == getLastOpRecord()->m_depth )
    }
}

vector< uint8_t > AlethStandardTrace::getInputData( const AlethExtVM& _ext ) const {
    if ( getLastOpRecord()->m_op == Instruction::CREATE ||
         getLastOpRecord()->m_op == Instruction::CREATE2 ) {
        // we are in a constructor code, so input to the function is current
        // code
        return _ext.code;
    } else {
        // we are in a regular function so input is inputData field of _ext
        return _ext.data.toVector();
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

    auto functionCall = make_shared< FunctionCallRecord >( getLastOpRecord()->m_op, _from, _to,
        _gasLimit, m_currentlyExecutingFunctionCall, _inputData, _value,
        getLastOpRecord()->m_depth + 1, getLastOpRecord()->m_gasRemaining );

    if ( getLastOpRecord()->m_depth >= 0 ) {
        // we are not in the top smartcontract call
        // add this call to the as a nested call to the currently
        // executing function call
        STATE_CHECK( getCurrentlyExecutingFunctionCall()->getDepth() == getLastOpRecord()->m_depth )
        getCurrentlyExecutingFunctionCall()->addNestedCall( functionCall );

        auto lastOpRecordIndex = m_executionRecordSequence->size() - 1;
        auto lastOp = getLastOpRecord()->m_op;

        if ( lastOp == Instruction::CALL || lastOp == Instruction::DELEGATECALL ||
             lastOp == Instruction::CALLCODE || lastOp == Instruction::STATICCALL ) {
            STATE_CHECK( m_callInstructionCounterToFunctionRecord.count( lastOpRecordIndex ) == 0 );
            m_callInstructionCounterToFunctionRecord.emplace( lastOpRecordIndex, functionCall );
        }

    } else {
        // the top function is called
        // this happens at the beginning of the execution. When this happens, we init
        // m_executionRecordSequence.m_depth to -1
        STATE_CHECK( getLastOpRecord()->m_depth == -1 )
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
    // note that m_gas remaining can be less than m_opGas. This happens in case
    // of out of gas revert
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
    STATE_CHECK( m_isFinalized )
    return m_topFunctionCall;
}

// get the printed result. This happens at the end of the execution
Json::Value AlethStandardTrace::getJSONResult() const {
    STATE_CHECK( m_isFinalized )
    STATE_CHECK( !m_jsonTrace.isNull() )
    return m_jsonTrace;
}

uint64_t AlethStandardTrace::getTotalGasUsed() const {
    STATE_CHECK( m_isFinalized )
    return m_totalGasUsed;
}

AlethStandardTrace::AlethStandardTrace(
    Transaction& _t, const Address& _blockAuthor, const TraceOptions& _options, bool _isCall )
    : m_from{ _t.from() },
      m_to( _t.to() ),
      m_options( _options ),
      // if it is a call trace, the transaction does not have signature
      // therefore, its hash should not include signature
      m_txHash( _t.sha3( _isCall ? dev::eth::WithoutSignature : dev::eth::WithSignature ) ),
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
      m_isCall( _isCall ),
      m_value( _t.value() ),
      m_gasLimit( _t.gas() ),
      m_inputData( _t.data() ),
      m_gasPrice( _t.gasPrice() ) {
    // set the initial lastOpRecord
    m_executionRecordSequence = make_shared< vector< shared_ptr< OpExecutionRecord > > >();
    m_executionRecordSequence->push_back( make_shared< OpExecutionRecord >(
        // the top function is executed at depth 0
        // therefore it is called from depth -1
        -1,
        // when we start execution a user transaction the top level function can  be a call
        // or a contract create
        _t.isCreation() ? Instruction::CREATE : Instruction::CALL, 0, 0, 0, 0, "" ) );


    // mark from and to accounts as accessed
    m_accessedAccounts.insert( m_from );
    m_accessedAccounts.insert( m_to );
}

const u256& AlethStandardTrace::getGasLimit() const {
    STATE_CHECK( m_isFinalized )
    return m_gasLimit;
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
        _pc, _inst, ( uint64_t ) _gasRemaining, _voidExt, ext, vm );

    auto executionRecord = createOpExecutionRecord( _pc, _inst, _gasOpGas, _gasRemaining, ext, vm );
    STATE_CHECK( executionRecord )
    STATE_CHECK( m_executionRecordSequence )

    m_executionRecordSequence->push_back( executionRecord );
}

shared_ptr< OpExecutionRecord > AlethStandardTrace::createOpExecutionRecord( uint64_t _pc,
    Instruction& _inst, const bigint& _gasOpGas, const bigint& _gasRemaining, const AlethExtVM& ext,
    const LegacyVM* _vm ) {
    STATE_CHECK( _vm )

    string opName( instructionInfo( _inst ).name );

    // make strings compatible to geth trace
    if ( opName == "JUMPCI" ) {
        opName = "JUMPI";
    } else if ( opName == "JUMPC" ) {
        opName = "JUMP";
    } else if ( opName == "SHA3" ) {
        opName = "KECCAK256";
    }

    auto executionRecord = std::make_shared< OpExecutionRecord >( ext.depth, _inst,
        ( uint64_t ) _gasRemaining, ( uint64_t ) _gasOpGas, _pc, ext.sub.refunds, opName );

    // this info is only required by DEFAULT_TRACER
    if ( m_options.tracerType == TraceType::DEFAULT_TRACER ||
         m_options.tracerType == TraceType::ALL_TRACER ) {
        if ( !m_options.disableStorage ) {
            if ( _inst == Instruction::SSTORE || _inst == Instruction::SLOAD ) {
                executionRecord->m_accessedStorageValues =
                    std::make_shared< std::map< u256, u256 > >(
                        m_accessedStorageValues[ext.myAddress] );
            }
        }

        if ( !m_options.disableStack ) {
            executionRecord->m_stack = std::make_shared< u256s >( _vm->stack() );
        }

        if ( m_options.enableMemory ) {
            executionRecord->m_memory = make_shared< bytes >( _vm->memory() );
        }
    }

    return executionRecord;
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
    m_totalGasUsed = ( uint64_t ) _er.gasUsed;

    m_output = _er.output;
    m_deployedContractAddress = _er.newAddress;
    m_evmcStatusCode = AlethExtVM::transactionExceptionToEvmcStatusCode( _er.excepted );

    STATE_CHECK( m_topFunctionCall == m_currentlyExecutingFunctionCall )

    // if transaction is not just ETH transfer
    // record return of the top function.
    if ( m_topFunctionCall ) {
        recordFunctionReturned( m_evmcStatusCode, m_output, m_totalGasUsed );
    }

    recordMinerFeePayment( _statePost );

    // we are done. Set the trace to finalized
    STATE_CHECK( !m_isFinalized.exchange( true ) )
    // now print trace
    printTrace( _er, _statePre, _statePost );
}

void eth::AlethStandardTrace::recordMinerFeePayment( HistoricState& _statePost ) {
    if ( !m_isCall ) {  // geth does not record miner fee payments in call traces
        auto fee = m_gasPrice * m_totalGasUsed;
        auto fromPostBalance = _statePost.balance( m_from );
        STATE_CHECK( fromPostBalance >= fee )
        _statePost.setBalance( m_from, fromPostBalance - fee );
        auto minerBalance = _statePost.balance( m_blockAuthor );
        _statePost.setBalance( m_blockAuthor, minerBalance + fee );
    }
}

const bytes& AlethStandardTrace::getOutput() const {
    STATE_CHECK( m_isFinalized )
    return m_output;
}

bool AlethStandardTrace::isFailed() const {
    STATE_CHECK( m_isFinalized )
    return m_evmcStatusCode != EVMC_SUCCESS;
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

const shared_ptr< vector< shared_ptr< OpExecutionRecord > > >&
AlethStandardTrace::getOpRecordsSequence() const {
    STATE_CHECK( m_isFinalized )
    STATE_CHECK( m_executionRecordSequence );
    return m_executionRecordSequence;
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
    STATE_CHECK( m_isFinalized )
    return m_blockAuthor;
}

const u256& AlethStandardTrace::getMinerPayment() const {
    STATE_CHECK( m_isFinalized )
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
    STATE_CHECK( m_isFinalized )
    return m_isCall;
}

const u256& AlethStandardTrace::getOriginalFromBalance() const {
    STATE_CHECK( m_isFinalized )
    return m_originalFromBalance;
}

const bytes& AlethStandardTrace::getInputData() const {
    STATE_CHECK( m_isFinalized )
    return m_inputData;
}

const u256& AlethStandardTrace::getValue() const {
    STATE_CHECK( m_isFinalized )
    return m_value;
}

const Address& AlethStandardTrace::getTo() const {
    STATE_CHECK( m_isFinalized )
    return m_to;
}

const u256& AlethStandardTrace::getGasPrice() const {
    STATE_CHECK( m_isFinalized )
    return m_gasPrice;
}

evmc_status_code AlethStandardTrace::getEVMCStatusCode() const {
    STATE_CHECK( m_isFinalized )
    return m_evmcStatusCode;
}

const Address& AlethStandardTrace::getDeployedContractAddress() const {
    return m_deployedContractAddress;
}

// return true if transaction is a simple Eth transfer
[[nodiscard]] bool AlethStandardTrace::isSimpleTransfer() {
    return !m_topFunctionCall;
}

// return true if transaction is contract creation
bool AlethStandardTrace::isContractCreation() {
    return m_topFunctionCall && m_topFunctionCall->getType() == Instruction::CREATE;
}

std::shared_ptr< OpExecutionRecord > AlethStandardTrace::getLastOpRecord() const {
    STATE_CHECK( !m_isFinalized );
    STATE_CHECK( m_executionRecordSequence );
    STATE_CHECK( !m_executionRecordSequence->empty() )
    auto lastOpRecord = m_executionRecordSequence->back();
    STATE_CHECK( lastOpRecord )
    return lastOpRecord;
}

// this will return function call record if the instruction at a given execution
// counter created a new function
// will return nullptr otherwise
shared_ptr< FunctionCallRecord > AlethStandardTrace::getNewFunction( uint64_t _executionCounter ) {
    if ( m_callInstructionCounterToFunctionRecord.count( _executionCounter ) == 0 ) {
        return nullptr;
    }

    return m_callInstructionCounterToFunctionRecord.at( _executionCounter );
}

}  // namespace dev::eth


#endif
