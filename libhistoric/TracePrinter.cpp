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
#include "NoopTracePrinter.h"
#include "TraceStructuresAndDefs.h"

namespace dev::eth {

// we try to be compatible with geth messages as much as we can
string TracePrinter::getEvmErrorDescription( evmc_status_code _error ) {
    // this function should not be called if the status code is success
    STATE_CHECK( _error != EVMC_SUCCESS );
    switch ( _error ) {
    case EVMC_FAILURE:
        return "evm failure";
    case EVMC_REVERT:
        return "execution reverted";
    case EVMC_OUT_OF_GAS:
        return "out of gas";
    case EVMC_INVALID_INSTRUCTION:
        return "invalid instruction";
    case EVMC_UNDEFINED_INSTRUCTION:
        return "undefined instruction";
    case EVMC_STACK_OVERFLOW:
        return "stack overflow";
    case EVMC_STACK_UNDERFLOW:
        return "stack underflow";
    case EVMC_BAD_JUMP_DESTINATION:
        return "invalid jump";
    case EVMC_INVALID_MEMORY_ACCESS:
        return "invalid memory access";
    case EVMC_CALL_DEPTH_EXCEEDED:
        return "call depth exceeded";
    case EVMC_STATIC_MODE_VIOLATION:
        return "static mode violation";
    case EVMC_PRECOMPILE_FAILURE:
        return "precompile failure";
    case EVMC_CONTRACT_VALIDATION_FAILURE:
        return "contract validation failure";
    case EVMC_ARGUMENT_OUT_OF_RANGE:
        return "argument out of range";
    case EVMC_INTERNAL_ERROR:
        return "internal error";
    case EVMC_REJECTED:
        return "evm rejected";
    case EVMC_OUT_OF_MEMORY:
        return "out of memory";
    default:
        return "unexpected EVM error status code";
    };
}

TracePrinter::TracePrinter( AlethStandardTrace& _standardTrace, const string _jsonName )
    : m_trace( _standardTrace ), m_jsonName( _jsonName ) {}

const string& TracePrinter::getJsonName() const {
    return m_jsonName;
}


// this will return true if the contract existed before the transaction happened
bool TracePrinter::isPreExistingContract(
    const HistoricState& _statePre, const Address& _address ) {
    return _statePre.addressHasCode( _address );
}


// this will return true if the address is a contract that has been created
// during the current transaction and has not been deleted
bool TracePrinter::isNewContract(
    const HistoricState& _statePre, const HistoricState& _statePost, const Address& _address ) {
    auto isNewContract =
        !_statePre.addressHasCode( _address ) && _statePost.addressHasCode( _address );
    return isNewContract;
}

}  // namespace dev::eth

#endif