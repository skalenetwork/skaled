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


#include "FunctionCall.h"
#include "AlethStandardTrace.h"
#include <boost/algorithm/string.hpp>

// disable recursive warning since we are using recursion
#pragma clang diagnostic ignored "-Wrecursive-macro"


// this class collect information about functions called during execution

namespace dev::eth {

void FunctionCall::setGasUsed( uint64_t _gasUsed ) {
    m_gasUsed = _gasUsed;
}
uint64_t FunctionCall::getFunctionGasLimit() const {
    return m_functionGasLimit;
}
void FunctionCall::setOutputData( const vector< uint8_t >& _outputData ) {
    m_outputData = _outputData;
}

void FunctionCall::addNestedCall( shared_ptr< FunctionCall >& _nestedCall ) {
    STATE_CHECK( _nestedCall );
    m_nestedCalls.push_back( _nestedCall );
}
void FunctionCall::setError( const string& _error ) {
    m_error = _error;
}
void FunctionCall::setRevertReason( const string& _revertReason ) {
    m_reverted = true;
    m_revertReason = _revertReason;
}
const weak_ptr< FunctionCall >& FunctionCall::getParentCall() const {
    return m_parentCall;
}
int64_t FunctionCall::getDepth() const {
    return m_depth;
}

void FunctionCall::printFunctionExecutionDetail(
    Json::Value& _jsonTrace, const TraceOptions& _debugOptions ) {
    STATE_CHECK( _jsonTrace.isObject() )

    _jsonTrace["type"] = instructionInfo( m_type ).name;
    _jsonTrace["from"] = toHexPrefixed( m_from );
    if ( m_type != Instruction::CREATE && m_type != Instruction::CREATE2 ) {
        _jsonTrace["to"] = toHexPrefixed( m_to );
    }
    _jsonTrace["gas"] = toCompactHexPrefixed( m_functionGasLimit );
    _jsonTrace["gasUsed"] = toCompactHexPrefixed( m_gasUsed );
    if ( !m_error.empty() ) {
        _jsonTrace["error"] = m_error;
    }
    if ( !m_revertReason.empty() ) {
        _jsonTrace["revertReason"] = m_revertReason;
    }

    _jsonTrace["value"] = toCompactHexPrefixed( m_value );

    if ( !m_outputData.empty() ) {
        _jsonTrace["output"] = toHexPrefixed( m_outputData );
    }

    if ( !m_inputData.empty() ) {
        _jsonTrace["input"] = toHexPrefixed( m_inputData );
    }

    if ( !_debugOptions.withLog )
        return;

    if ( !m_logRecords.empty() && m_type != Instruction::CREATE &&
         m_type != Instruction::CREATE2 ) {
        _jsonTrace["logs"] = Json::arrayValue;
        for ( auto&& log : m_logRecords ) {
            // no logs in contract creation
            _jsonTrace["logs"] = Json::arrayValue;
            if ( m_type != Instruction::CREATE && m_type != Instruction::CREATE2 ) {
                Json::Value currentLogRecord = Json::objectValue;
                currentLogRecord["address"] = toHexPrefixed( m_to );
                currentLogRecord["data"] = toHexPrefixed( log.m_data );
                currentLogRecord["topics"] = Json::arrayValue;
                for ( auto&& topic : log.m_topics ) {
                    currentLogRecord["topics"].append( toHexPrefixed( topic ) );
                }
                _jsonTrace["logs"].append( currentLogRecord );
            }
        }
    }
}

void FunctionCall::printTrace(
    Json::Value& _jsonTrace, int64_t _depth, const TraceOptions& _debugOptions ) {
    STATE_CHECK( _jsonTrace.isObject() )
    // prevent Denial of service
    STATE_CHECK( _depth < MAX_TRACE_DEPTH )
    STATE_CHECK( _depth == m_depth )

    printFunctionExecutionDetail( _jsonTrace, _debugOptions );

    if ( !m_nestedCalls.empty() ) {
        _jsonTrace["calls"] = Json::arrayValue;
        uint32_t i = 0;
        if ( _debugOptions.onlyTopCall )
            return;
        for ( auto&& nestedCall : m_nestedCalls ) {
            _jsonTrace["calls"].append( Json::objectValue );
            nestedCall->printTrace( _jsonTrace["calls"][i], _depth + 1, _debugOptions );
            i++;
        }
    }
}


FunctionCall::FunctionCall( Instruction _type, const Address& _from, const Address& _to,
    uint64_t _functionGasLimit, const weak_ptr< FunctionCall >& _parentCall,
    const vector< uint8_t >& _inputData, const u256& _value, int64_t _depth )
    : m_type( _type ),
      m_from( _from ),
      m_to( _to ),
      m_functionGasLimit( _functionGasLimit ),
      m_parentCall( _parentCall ),
      m_inputData( _inputData ),
      m_value( _value ),
      m_depth( _depth ) {
    STATE_CHECK( m_depth >= 0 )
}


void FunctionCall::addLogEntry( const vector< uint8_t >& _data, const vector< u256 >& _topics ) {
    m_logRecords.emplace_back( _data, _topics );
}
string FunctionCall::getParityTraceType() {
    return boost::algorithm::to_lower_copy( string( instructionInfo( m_type ).name ) );
}
void FunctionCall::printParityFunctionTrace( Json::Value& _outputArray, Json::Value _address ) {
    Json::Value functionTrace( Json::objectValue );

    Json::Value action( Json::objectValue );
    action["from"] = toHexPrefixed( m_from );
    action["to"] = toHexPrefixed( m_to );
    action["gas"] = toHexPrefixed( u256( m_functionGasLimit ) );
    action["input"] = toHexPrefixed( m_inputData );
    action["value"] = toHexPrefixed( m_value );
    action["callType"] = getParityTraceType();
    functionTrace["action"] = action;


    Json::Value result( Json::objectValue );
    result["gasUsed"] = toHexPrefixed( u256( m_gasUsed ) );
    result["output"] = toHexPrefixed( m_outputData );
    functionTrace["result"] = result;

    functionTrace["subtraces"] = m_nestedCalls.size();
    functionTrace["traceAddress"] = _address;
    functionTrace["type"] = getParityTraceType();
    _outputArray.append( functionTrace );

    for ( uint64_t i = 0; i < m_nestedCalls.size(); i++ ) {
        auto nestedFunctionAddress = _address;
        nestedFunctionAddress.append( i );
        auto nestedCall = m_nestedCalls.at( i );
        STATE_CHECK( nestedCall );
        nestedCall->printParityFunctionTrace( _outputArray, nestedFunctionAddress );
    }
}


void FunctionCall::collectFourByteTrace( std::map< string, uint64_t >& _callMap ) {
    Json::Value functionTrace( Json::objectValue );

    constexpr int FOUR_BYTES = 4;

    // a call with less than four bytes of data is not a valid solidity call
    if ( m_inputData.size() >= FOUR_BYTES ) {
        vector< uint8_t > fourBytes( m_inputData.begin(), m_inputData.begin() + FOUR_BYTES );
        auto key = toHexPrefixed( fourBytes )
                       .append( "-" )
                       .append( to_string( m_inputData.size() - FOUR_BYTES ) );
        auto count = _callMap[key] + 1;
        _callMap[key] = count;
    }

    for ( uint64_t i = 0; i < m_nestedCalls.size(); i++ ) {
        auto nestedCall = m_nestedCalls.at( i );
        STATE_CHECK( nestedCall );
        nestedCall->collectFourByteTrace( _callMap );
    }
}


void FunctionCall::setReturnValues(
    evmc_status_code _status, const vector< uint8_t >& _returnData, uint64_t _gasUsed ) {
    setGasUsed( _gasUsed );

    if ( _status != evmc_status_code::EVMC_SUCCESS ) {
        setError( TracePrinter::getEvmErrorDescription( _status ) );
    }

    if ( _status == evmc_status_code::EVMC_REVERT ) {
        setRevertReason( string( _returnData.begin(), _returnData.end() ) );
    } else {
        setOutputData( _returnData );
    }
}

OpExecutionRecord::OpExecutionRecord(
    int64_t _depth, Instruction _op, uint64_t _gasRemaining, uint64_t _opGas )
    : m_depth( _depth ), m_op( _op ), m_gasRemaining( _gasRemaining ), m_opGas( _opGas ) {}
}  // namespace dev::eth
   // namespace dev
