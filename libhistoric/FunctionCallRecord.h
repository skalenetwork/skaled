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


#pragma once


#include "AlethExtVM.h"
#include "libevm/LegacyVM.h"
#include <jsonrpccpp/common/exception.h>
#include <skutils/eth_utils.h>


namespace dev::eth {

using std::string, std::shared_ptr, std::make_shared, std::to_string, std::set, std::map,
    std::vector, std::weak_ptr;


struct TraceOptions;

// It is important that trace functions do not throw exceptions and do not modify state
// so that they do not interfere with EVM execution

struct LogRecord;

struct OpExecutionRecord;

class FunctionCallRecord {
public:
    FunctionCallRecord( Instruction _type, const Address& _from, const Address& _to,
        uint64_t _functionGasLimit, const weak_ptr< FunctionCallRecord >& _parentCall,
        const vector< uint8_t >& _inputData, const u256& _value, int64_t _depth,
        uint64_t _gasRemainingBeforeCall );

    [[nodiscard]] int64_t getDepth() const;

    [[nodiscard]] const weak_ptr< FunctionCallRecord >& getParentCall() const;

    [[nodiscard]] uint64_t getFunctionGasLimit() const;

    [[nodiscard]] string getParityTraceType();

    void setGasUsed( uint64_t _gasUsed );

    void setOutputData( const vector< uint8_t >& _outputData );

    void addNestedCall( shared_ptr< FunctionCallRecord >& _nestedCall );

    void setError( const string& _error );

    void setRevertReason( const bytes& _encodedRevertReason );

    void printTrace( Json::Value& _jsonTrace, const HistoricState& _statePost, int64_t _depth,
        const TraceOptions& _debugOptions );

    void printFunctionExecutionDetail( Json::Value& _jsonTrace, const HistoricState& _statePost,
        const TraceOptions& _debugOptions );

    void addLogEntry( const vector< uint8_t >& _data, const vector< u256 >& _topics );

    void printParityFunctionTrace( Json::Value& _outputArray, Json::Value _address );

    void collectFourByteTrace( std::map< string, uint64_t >& _callMap );

    void setReturnValues(
        evmc_status_code _status, const vector< uint8_t >& _returnData, uint64_t _gasUsed );

    Instruction getType() const;

    uint64_t getGasUsed() const;
    uint64_t getGasRemainingBeforeCall() const;

    static uint32_t bytesToUint32( const std::vector< uint8_t >& _bytes, size_t _startIndex );

private:
    Instruction m_type;
    Address m_from;
    Address m_to;
    uint64_t m_functionGasLimit = 0;
    uint64_t m_gasUsed = 0;
    vector< shared_ptr< FunctionCallRecord > > m_nestedCalls;
    weak_ptr< FunctionCallRecord > m_parentCall;


private:
    vector< uint8_t > m_inputData;
    vector< uint8_t > m_outputData;
    bool m_reverted = false;
    string m_error;
    string m_revertReason;
    u256 m_value;
    int64_t m_depth = 0;
    vector< LogRecord > m_logRecords;
    uint64_t m_gasRemainingBeforeCall;
};


}  // namespace dev::eth
