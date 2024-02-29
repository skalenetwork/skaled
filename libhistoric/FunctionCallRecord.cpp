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

#include "FunctionCallRecord.h"
#include "AlethStandardTrace.h"
#include <boost/algorithm/string.hpp>

// disable recursive warning since we are using recursion
#pragma clang diagnostic ignored "-Wrecursive-macro"


// this class collect information about functions called during execution

namespace dev::eth {

void FunctionCallRecord::setGasUsed( uint64_t _gasUsed ) {
    STATE_CHECK( _gasUsed < std::numeric_limits< uint32_t >::max() );
    m_gasUsed = _gasUsed;
}

uint64_t FunctionCallRecord::getFunctionGasLimit() const {
    return m_functionGasLimit;
}

void FunctionCallRecord::setOutputData( const vector< uint8_t >& _outputData ) {
    m_outputData = _outputData;
}

void FunctionCallRecord::addNestedCall( shared_ptr< FunctionCallRecord >& _nestedCall ) {
    STATE_CHECK( _nestedCall );
    m_nestedCalls.push_back( _nestedCall );
}

void FunctionCallRecord::setError( const string& _error ) {
    m_error = _error;
}

// decode 4 consequitive byes in a byte array to uint32_t
uint32_t FunctionCallRecord::bytesToUint32(
    const std::vector< uint8_t >& _bytes, size_t _startIndex ) {
    STATE_CHECK( ( _startIndex + 4 ) < _bytes.size() )

    return ( uint32_t( _bytes[_startIndex] ) << 24 ) |
           ( uint32_t( _bytes[_startIndex + 1] ) << 16 ) |
           ( uint32_t( _bytes[_startIndex + 2] ) << 8 ) | uint32_t( _bytes[_startIndex + 3] );
}

constexpr uint64_t FUNCTION_SELECTOR_SIZE = 4;
constexpr uint64_t OFFSET_SIZE = 32;
constexpr uint64_t STRING_LENGTH_SIZE = 32;

void FunctionCallRecord::setRevertReason( const bytes& _encodedRevertReason ) {
    static unsigned char functionSelector[] = { 0x08, 0xC3, 0x79, 0xA0 };

    m_reverted = true;

    // Check if the encoded data is at least long enough for a function selector, offset, and
    // length.
    if ( _encodedRevertReason.size() < FUNCTION_SELECTOR_SIZE + OFFSET_SIZE + STRING_LENGTH_SIZE ) {
        // incorrect encoding - setting revert reason to empty
        m_revertReason = "";
        return;
    }


    if ( !std::equal( functionSelector, functionSelector + 4, _encodedRevertReason.begin() ) ) {
        m_revertReason = "";
    }

    u256 offset;

    boost::multiprecision::import_bits( offset,
        _encodedRevertReason.begin() + FUNCTION_SELECTOR_SIZE,
        _encodedRevertReason.begin() + FUNCTION_SELECTOR_SIZE + OFFSET_SIZE );

    // offset is always 0x20
    if ( offset != u256( 0x20 ) ) {
        // incorrect offset - setting revert reason to empty
        m_revertReason = "";
        return;
    }


    u256 stringLength;

    boost::multiprecision::import_bits( stringLength,
        _encodedRevertReason.begin() + FUNCTION_SELECTOR_SIZE + OFFSET_SIZE,
        _encodedRevertReason.begin() + FUNCTION_SELECTOR_SIZE + OFFSET_SIZE + STRING_LENGTH_SIZE );

    // Ensure the encoded data is long enough to contain the described string.
    if ( _encodedRevertReason.size() <
         FUNCTION_SELECTOR_SIZE + OFFSET_SIZE + STRING_LENGTH_SIZE + stringLength ) {
        m_revertReason = "";
    }

    // Extract the string itself.
    std::string result;
    for ( size_t i = 0; i < stringLength; ++i ) {
        result += char(
            _encodedRevertReason[FUNCTION_SELECTOR_SIZE + OFFSET_SIZE + STRING_LENGTH_SIZE + i] );
    }

    m_revertReason = result;
}

const weak_ptr< FunctionCallRecord >& FunctionCallRecord::getParentCall() const {
    return m_parentCall;
}

int64_t FunctionCallRecord::getDepth() const {
    return m_depth;
}

void FunctionCallRecord::printFunctionExecutionDetail(
    Json::Value& _jsonTrace, const HistoricState& _statePost, const TraceOptions& _debugOptions ) {
    STATE_CHECK( _jsonTrace.isObject() )

    _jsonTrace["type"] = instructionInfo( m_type ).name;
    _jsonTrace["from"] = toHexPrefixed( m_from );
    _jsonTrace["to"] = toHexPrefixed( m_to );
    _jsonTrace["gas"] =
        AlethStandardTrace::toGethCompatibleCompactHexPrefixed( m_functionGasLimit );
    _jsonTrace["gasUsed"] = AlethStandardTrace::toGethCompatibleCompactHexPrefixed( m_gasUsed );
    if ( !m_error.empty() ) {
        _jsonTrace["error"] = m_error;
    }
    if ( !m_revertReason.empty() ) {
        _jsonTrace["revertReason"] = m_revertReason;
    }

    // geth always prints value when there is not revert
    // in case of revert value is printed only in top lavel functions
    if ( m_error.empty() || m_depth == 0 ) {
        _jsonTrace["value"] = AlethStandardTrace::toGethCompatibleCompactHexPrefixed( m_value );
    }


    // treat the special case when the function is a constructor. Then output data is the code of
    // the constructed contract
    if ( m_type == Instruction::CREATE || m_type == Instruction::CREATE2 ) {
        m_outputData = _statePost.code( m_to );
    }


    if ( !m_outputData.empty() ) {
        _jsonTrace["output"] = toHexPrefixed( m_outputData );
    }

    if ( !m_inputData.empty() ) {
        _jsonTrace["input"] = toHexPrefixed( m_inputData );
    } else {
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

void FunctionCallRecord::printTrace( Json::Value& _jsonTrace, const HistoricState& _statePost,
    int64_t _depth, const TraceOptions& _debugOptions ) {
    STATE_CHECK( _jsonTrace.isObject() )
    // prevent Denial of service
    STATE_CHECK( _depth < MAX_TRACE_DEPTH )
    STATE_CHECK( _depth == m_depth )

    printFunctionExecutionDetail( _jsonTrace, _statePost, _debugOptions );

    if ( !m_nestedCalls.empty() ) {
        if ( _debugOptions.onlyTopCall )
            return;
        _jsonTrace["calls"] = Json::arrayValue;
        for ( uint32_t i = 0; i < m_nestedCalls.size(); i++ ) {
            _jsonTrace["calls"].append( Json::objectValue );
            m_nestedCalls.at( i )->printTrace(
                _jsonTrace["calls"][i], _statePost, _depth + 1, _debugOptions );
        }
    }
}

FunctionCallRecord::FunctionCallRecord( Instruction _type, const Address& _from, const Address& _to,
    uint64_t _functionGasLimit, const weak_ptr< FunctionCallRecord >& _parentCall,
    const vector< uint8_t >& _inputData, const u256& _value, int64_t _depth,
    uint64_t _gasRemainingBeforeCall )
    : m_type( _type ),
      m_from( _from ),
      m_to( _to ),
      m_functionGasLimit( _functionGasLimit ),
      m_parentCall( _parentCall ),
      m_inputData( _inputData ),
      m_value( _value ),
      m_depth( _depth ),
      m_gasRemainingBeforeCall( _gasRemainingBeforeCall ){ STATE_CHECK( m_depth >= 0 ) }

      uint64_t FunctionCallRecord::getGasRemainingBeforeCall() const {
    return m_gasRemainingBeforeCall;
}


void FunctionCallRecord::addLogEntry(
    const vector< uint8_t >& _data, const vector< u256 >& _topics ) {
    m_logRecords.emplace_back( _data, _topics );
}

string FunctionCallRecord::getParityTraceType() {
    return boost::algorithm::to_lower_copy( string( instructionInfo( m_type ).name ) );
}

void FunctionCallRecord::printParityFunctionTrace(
    Json::Value& _outputArray, Json::Value _address ) {
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

void FunctionCallRecord::collectFourByteTrace( std::map< string, uint64_t >& _callMap ) {
    Json::Value functionTrace( Json::objectValue );

    constexpr int FOUR_BYTES = 4;

    if ( m_depth > 0 && ( m_type == Instruction::CREATE || m_type == Instruction::CREATE2 ) ) {
        // geth does not print 4byte traces for constructors called by contracts
        return;
    }

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


void FunctionCallRecord::setReturnValues(
    evmc_status_code _status, const vector< uint8_t >& _returnData, uint64_t _gasUsed ) {
    setGasUsed( _gasUsed );

    if ( _status != evmc_status_code::EVMC_SUCCESS ) {
        setError( TracePrinter::getEvmErrorDescription( _status ) );
    }

    if ( _status == evmc_status_code::EVMC_REVERT ) {
        setRevertReason( _returnData );
    }

    setOutputData( _returnData );
}

Instruction FunctionCallRecord::getType() const {
    return m_type;
}

uint64_t FunctionCallRecord::getGasUsed() const {
    return m_gasUsed;
}

}  // namespace dev::eth
// namespace dev

#endif