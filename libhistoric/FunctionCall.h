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


// It is important that trace functions do not throw exceptions and do not modify state
// so that they do not interfere with EVM execution

class LogRecord {
public:
    LogRecord( const vector< uint8_t >& _data, const vector< u256 >& _topics )
        : m_data( _data ), m_topics( _topics ) {}

private:
    const vector< uint8_t >  m_data;
    const vector< u256 >  m_topics;
};

class OpExecutionRecord {
public:
    // this is top level record to enter the transaction
    // the first function is executed at depth 0, as it was called form depth -1
    explicit OpExecutionRecord( Instruction _op )
        : OpExecutionRecord( false, vector< uint8_t >(), -1, _op, 0, 0 ){};

    OpExecutionRecord( bool _hasReverted, vector< uint8_t > _returnData, int64_t _depth,
        Instruction _op, uint64_t _gasRemaining, uint64_t _opGas );

    bool m_hasReverted;

    vector< uint8_t > m_returnData;
    int64_t m_depth;
    Instruction m_op;
    uint64_t m_gasRemaining;
    uint64_t m_opGas;
};


class FunctionCall {
public:
    FunctionCall( Instruction _type, const Address& _from, const Address& _to,
        uint64_t _functionGasLimit, const weak_ptr< FunctionCall >& _parentCall,
        const vector< uint8_t >& _inputData, const u256& _value, int64_t _depth );
    [[nodiscard]] int64_t getDepth() const;
    void setGasUsed( uint64_t _gasUsed );
    void setOutputData( vector< uint8_t >& _outputData );
    void addNestedCall( shared_ptr< FunctionCall >& _nestedCall );
    void setError( const string& _error );
    void setRevertReason( const string& _revertReason );
    [[nodiscard]] const weak_ptr< FunctionCall >& getParentCall() const;

    [[nodiscard]] bool hasReverted() const;
    [[nodiscard]] bool hasError() const;
    [[nodiscard]] const Address& getFrom() const;
    [[nodiscard]] const Address& getTo() const;
    [[nodiscard]] uint64_t getFunctionGasLimit() const;

    void printTrace( Json::Value& _jsonTrace, int64_t _depth );
    void printFunctionExecutionDetail( Json::Value& _jsonTrace );

    void addLogEntry( const vector< uint8_t >& _data, const vector< u256 >& _topics );

private:
    Instruction m_type;
    Address m_from;
    Address m_to;
    uint64_t m_functionGasLimit = 0;
    uint64_t m_gasUsed = 0;
    vector< shared_ptr< FunctionCall > > m_nestedCalls;
    weak_ptr< FunctionCall > m_parentCall;
    vector< uint8_t > m_inputData;
    vector< uint8_t > m_outputData;
    bool m_reverted = false;

public:
    uint64_t getMFunctionGasLimit() const;

private:
    bool m_completedWithError = false;
    string m_error;
    string m_revertReason;
    u256 m_value;
    int64_t m_depth = 0;
    vector< LogRecord > m_logRecords;
};


}  // namespace dev::eth
