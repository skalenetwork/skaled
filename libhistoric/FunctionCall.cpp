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

namespace dev::eth {


void FunctionCall::setGasUsed( uint64_t _gasUsed ) {
    m_gasUsed = _gasUsed;
}
uint64_t FunctionCall::getFunctionGasLimit() const {
    return m_functionGasLimit;
}
void FunctionCall::setOutputData( vector< uint8_t >& _outputData ) {
    m_outputData = _outputData;
}

void FunctionCall::addNestedCall( shared_ptr< FunctionCall >& _nestedCall ) {
    STATE_CHECK( _nestedCall );
    m_nestedCalls.push_back( _nestedCall );
}
void FunctionCall::setError( const string& _error ) {
    m_completedWithError = true;
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

void FunctionCall::printFunctionExecutionDetail( Json::Value& _jsonTrace ) {
    _jsonTrace["type"] = instructionInfo( m_type ).name;
    _jsonTrace["from"] = toHex( m_from );
    if ( m_type != Instruction::CREATE && m_type != Instruction::CREATE2 ) {
        _jsonTrace["to"] = toHex( m_to );
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
        _jsonTrace["output"] = toHex( m_outputData );
    }

    if ( !m_inputData.empty() ) {
        _jsonTrace["input"] = toHex( m_inputData );
    }
}


void FunctionCall::printTrace( Json::Value& _jsonTrace, int64_t _depth ) {
    // prevent Denial of service
    STATE_CHECK( _depth < MAX_TRACE_DEPTH )
    STATE_CHECK( _depth == this->m_depth )
    printFunctionExecutionDetail( _jsonTrace );
    if ( !m_nestedCalls.empty() ) {
        _jsonTrace["calls"] = Json::arrayValue;
        uint32_t i = 0;
        for ( auto&& nestedCall : m_nestedCalls ) {
            _jsonTrace["calls"].append( Json::objectValue );
            nestedCall->printTrace( _jsonTrace["nestedCalls"][i], _depth + 1 );
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
const Address& FunctionCall::getFrom() const {
    return m_from;
}
const Address& FunctionCall::getTo() const {
    return m_to;
}

bool FunctionCall::hasReverted() const {
    return m_reverted;
}
bool FunctionCall::hasError() const {
    return m_completedWithError;
}


void FunctionCall::addLogEntry(
    const vector< uint8_t >& _data, const vector< u256 >& _topics ) {
    m_logRecords.emplace_back( _data, _topics );
}

OpExecutionRecord::OpExecutionRecord( bool _hasReverted, vector< uint8_t > _returnData,
    int64_t _depth, Instruction _op, uint64_t _gasRemaining, uint64_t _opGas )
    : m_hasReverted( _hasReverted ),
      m_returnData( _returnData ),
      m_depth( _depth ),
      m_op( _op ),
      m_gasRemaining( _gasRemaining ),
      m_opGas( _opGas ) {}
}  // namespace dev::eth
   // namespace dev
