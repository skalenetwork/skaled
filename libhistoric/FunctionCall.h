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


namespace dev {
namespace eth {


// It is important that trace functions do not throw exceptions and do not modify state
// so that they do not interfere with EVM execution


class FunctionCall {
public:
    FunctionCall( Instruction _type, const Address& _from, const Address& _to,
        uint64_t _functionGasLimit, const std::weak_ptr< FunctionCall >& _parentCall,
        const std::vector< uint8_t >& _inputData, const u256& _value, int64_t _depth );
    int64_t getDepth() const;
    void setGasUsed( uint64_t _gasUsed );
    void setOutputData( const std::vector< uint8_t >& _outputData );
    void addNestedCall( std::shared_ptr< FunctionCall >& _nestedCall );
    void setError( const std::string& _error );
    void setRevertReason( const std::string& _revertReason );
    [[nodiscard]] const std::weak_ptr< FunctionCall >& getParentCall() const;

    bool hasReverted() const;
    bool hasError() const;
    const Address& getFrom() const;
    const Address& getTo() const;
    uint64_t getFunctionGasLimit() const;

    void printTrace( Json::Value& _jsonTrace, int64_t _depth );
    void printFunctionExecutionDetail( Json::Value& _jsonTrace );

private:
    Instruction type;
    Address from;
    Address to;
    uint64_t functionGasLimit = 0;
    uint64_t gasUsed = 0;
    std::vector< std::shared_ptr< FunctionCall > > nestedCalls;
    std::weak_ptr< FunctionCall > parentCall;
    std::vector< uint8_t > inputData;
    std::vector< uint8_t > outputData;
    bool reverted = false;
    bool completedWithError = false;
    std::string error;
    std::string revertReason;
    u256 value;
    int64_t depth = 0;
};


}  // namespace eth
}  // namespace dev