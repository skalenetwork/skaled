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


#include "FourByteTracePrinter.h"
#include "AlethStandardTrace.h"
#include "FunctionCallRecord.h"
#include "TraceStructuresAndDefs.h"

namespace dev::eth {

// fourbyte trace printer as implemented by geth
void FourByteTracePrinter::print(
    Json::Value& _jsonTrace, const ExecutionResult&, const HistoricState&, const HistoricState& ) {
    STATE_CHECK( _jsonTrace.isObject() )
    std::map< string, uint64_t > callMap;

    auto topFunctionCallRecord = m_trace.getTopFunctionCall();
    if ( !topFunctionCallRecord ) {
        // no bytecodes were executed, this was purely ETH transfer
        // print nothing
        return;
    }

    topFunctionCallRecord->collectFourByteTrace( callMap );
    for ( auto&& key : callMap ) {
        _jsonTrace[key.first] = key.second;
    }

    // there is a special case of contract creation. In this case, geth adds
    // a special entry to the fourbyte trace as specified below

    if ( m_trace.isContractCreation() ) {
        addContractCreationEntry( _jsonTrace );
    }
}
void FourByteTracePrinter::addContractCreationEntry( Json::Value& _jsonTrace ) const {
    auto inputBytes = m_trace.getInputData();
    // input data needs to be at least four bytes, otherwise it is an incorrect
    // Solidity constructor call
    if ( inputBytes.size() < 4 ) {
        return;
    }
    // the format geth uses in this case. Take first 8 symbols of hex and add hex size
    auto key =
        toHexPrefixed( inputBytes ).substr( 0, 10 ) + "-" + to_string( inputBytes.size() - 4 );
    _jsonTrace[key] = 1;
}

FourByteTracePrinter::FourByteTracePrinter( AlethStandardTrace& standardTrace )
    : TracePrinter( standardTrace, "4byteTrace" ) {}


}  // namespace dev::eth

#endif