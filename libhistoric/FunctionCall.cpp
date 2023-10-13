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


namespace dev {
namespace eth {


void FunctionCall::setGasUsed( uint64_t _gasUsed ) {
    FunctionCall::gasUsed = _gasUsed;
}
uint64_t FunctionCall::getFunctionGasLimit() const {
    return functionGasLimit;
}
void FunctionCall::setOutputData( const std::vector< uint8_t >& _outputData ) {
    FunctionCall::outputData = _outputData;
}

void FunctionCall::addNestedCall( std::shared_ptr< FunctionCall >& _nestedCall ) {
    STATE_CHECK( _nestedCall );
    this->nestedCalls.push_back( _nestedCall );
}
void FunctionCall::setError( const std::string& _error ) {
    completedWithError = true;
    error = _error;
}
void FunctionCall::setRevertReason( const std::string& _revertReason ) {
    reverted = true;
    revertReason = _revertReason;
}
const std::weak_ptr< FunctionCall >& FunctionCall::getParentCall() const {
    return parentCall;
}
int64_t FunctionCall::getDepth() const {
    return depth;
}

void FunctionCall::printFunctionExecutionDetail( Json::Value& _jsonTrace ) {
    _jsonTrace["type"] = instructionInfo( type ).name;
    _jsonTrace["from"] = toHex( from );
    if ( type != Instruction::CREATE && type != Instruction::CREATE2 ) {
        _jsonTrace["to"] = toHex( to );
    }
    _jsonTrace["gas"] = toCompactHexPrefixed( functionGasLimit );
    _jsonTrace["gasUsed"] = toCompactHexPrefixed( gasUsed );
    if ( !error.empty() ) {
        _jsonTrace["error"] = error;
    }
    if ( !revertReason.empty() ) {
        _jsonTrace["revertReason"] = revertReason;
    }

    _jsonTrace["value"] = toCompactHexPrefixed( value );
}


void FunctionCall::printTrace( Json::Value& _jsonTrace, int64_t _depth ) {
    // prevent Denial of service
    STATE_CHECK( _depth < MAX_TRACE_DEPTH )
    STATE_CHECK( _depth == this->depth )
    printFunctionExecutionDetail( _jsonTrace );
    if ( !nestedCalls.empty() ) {
        _jsonTrace["calls"] = Json::arrayValue;
        uint32_t i = 0;
        for ( auto&& nestedCall : nestedCalls ) {
            _jsonTrace["calls"].append( Json::objectValue );
            nestedCall->printTrace( _jsonTrace["nestedCalls"][i], _depth + 1 );
            i++;
        }
    }
}


FunctionCall::FunctionCall( Instruction _type, const Address& _from, const Address& _to,
    uint64_t _functionGasLimit, const std::weak_ptr< FunctionCall >& _parentCall,
    const std::vector< uint8_t >& _inputData, const u256& _value, int64_t _depth )
    : type( _type ),
      from( _from ),
      to( _to ),
      functionGasLimit( _functionGasLimit ),
      parentCall( _parentCall ),
      inputData( _inputData ),
      value( _value ),
      depth( _depth ) {
    STATE_CHECK( depth >= 0 )
}
const Address& FunctionCall::getFrom() const {
    return from;
}
const Address& FunctionCall::getTo() const {
    return to;
}

bool FunctionCall::hasReverted() const {
    return reverted;
}
bool FunctionCall::hasError() const {
    return completedWithError;
}


}  // namespace eth
}  // namespace dev
