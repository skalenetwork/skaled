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
#include "PrestateTracePrinter.h"
#include "AlethStandardTrace.h"
#include "FunctionCallRecord.h"
#include "TraceStructuresAndDefs.h"

namespace dev::eth {

void PrestateTracePrinter::print( Json::Value& _jsonTrace, const ExecutionResult& _er,
    const HistoricState& _statePre, const HistoricState& _statePost ) {
    STATE_CHECK( _jsonTrace.isObject() );
    if ( m_standardTrace.getOptions().prestateDiffMode ) {
        printDiff( _jsonTrace, _er, _statePre, _statePost );
    } else {
        for ( auto&& item : m_standardTrace.getAccessedAccounts() ) {
            printAllAccessedAccountPreValues( _jsonTrace, _statePre, item );
        };


        auto minerAddress = m_standardTrace.getBlockAuthor();

        if ( m_standardTrace.getAccessedAccounts().count( minerAddress ) == 0 ) {
            // to be compatible with geth always print miner balance
            // miner balance is not in the list of accessed accounts, print it anyway
            Json::Value minerValue;
            minerValue["balance"] =
                AlethStandardTrace::toGethCompatibleCompactHexPrefixed( _statePre.balance( minerAddress ) );
            _jsonTrace[toHexPrefixed( minerAddress )] = minerValue;
        }
    }
}


void PrestateTracePrinter::printDiff( Json::Value& _jsonTrace, const ExecutionResult&,
    const HistoricState& _statePre, const HistoricState& _statePost ) {
    STATE_CHECK( _jsonTrace.isObject() )

    Json::Value preDiff( Json::objectValue );
    Json::Value postDiff( Json::objectValue );

    for ( auto&& item : m_standardTrace.getAccessedAccounts() ) {
        printAccountPreDiff( preDiff, _statePre, _statePost, item );
        printAccountPostDiff( postDiff, _statePre, _statePost, item );
    };

    _jsonTrace["pre"] = preDiff;
    _jsonTrace["post"] = postDiff;
}


// this function returns original values (pre) to result
void PrestateTracePrinter::printAllAccessedAccountPreValues(
    Json::Value& _jsonTrace, const HistoricState& _statePre, const Address& _address ) {
    STATE_CHECK( _jsonTrace.isObject() )


    Json::Value accountPreValues;
    // if this _address did not exist, we do not include it in the diff
    if ( !_statePre.addressInUse( _address ) )
        return;

    auto balance = _statePre.balance( _address );

    if ( m_standardTrace.isCall() && _address == m_standardTrace.getFrom() ) {
        // take into account that for calls balance is modified in the state before execution
        balance = m_standardTrace.getOriginalFromBalance();
    } else {
        // geth does not print nonce for from address in debug_traceCall;
        accountPreValues["nonce"] = ( uint64_t ) _statePre.getNonce( _address );
    }

    accountPreValues["balance"] = AlethStandardTrace::toGethCompatibleCompactHexPrefixed( balance );

    bytes const& code = _statePre.code( _address );
    if ( code != NullBytes ) {
        accountPreValues["code"] = toHexPrefixed( code );
    }

    Json::Value storagePairs;

    auto& accessedStoragedValues = m_standardTrace.getAccessedStorageValues();

    // now print all storage values that were accessed (written or read) during the transaction
    if ( accessedStoragedValues.count( _address ) ) {
        for ( auto&& storageAddressValuePair : accessedStoragedValues.at( _address ) ) {
            auto& storageAddress = storageAddressValuePair.first;
            auto originalValue = _statePre.originalStorageValue( _address, storageAddress );
            storagePairs[toHexPrefixed( storageAddress )] = toHexPrefixed( originalValue );
            // return limited number of values to prevent DOS attacks
            m_storageValuesReturnedAll++;
            if ( m_storageValuesReturnedAll >= MAX_STORAGE_VALUES_RETURNED )
                break;
        }
    }

    if ( storagePairs ) {
        accountPreValues["storage"] = storagePairs;
    }

    // if nothing changed we do not add it to the diff
    if ( accountPreValues )
        _jsonTrace[toHexPrefixed( _address )] = accountPreValues;
}


void PrestateTracePrinter::printAccountPostDiff( Json::Value& _postDiffTrace,
    const HistoricState& _statePre, const HistoricState& _statePost, const Address& _address ) {
    Json::Value value( Json::objectValue );


    // if this address does not exist post-transaction we dot include it in the trace
    if ( !_statePost.addressInUse( _address ) )
        return;

    auto balancePost = _statePost.balance( _address );
    auto noncePost = _statePost.getNonce( _address );
    auto& codePost = _statePost.code( _address );


    // if the new address, ot if the value changed, include in post trace
    if ( !_statePre.addressInUse( _address ) || _statePre.balance( _address ) != balancePost ) {
        value["balance"] = AlethStandardTrace::toGethCompatibleCompactHexPrefixed( balancePost );
    }
    if ( !_statePre.addressInUse( _address ) || _statePre.getNonce( _address ) != noncePost ) {
        value["nonce"] = ( uint64_t ) noncePost;
    }
    if ( !_statePre.addressInUse( _address ) || _statePre.code( _address ) != codePost ) {
        if ( codePost != NullBytes ) {
            value["code"] = toHexPrefixed( codePost );
        }
    }


    // post diffs for storage values
    if ( m_standardTrace.getAccessedStorageValues().find( _address ) !=
         m_standardTrace.getAccessedStorageValues().end() ) {
        Json::Value storagePairs( Json::objectValue );

        // iterate over all accessed storage values
        for ( auto&& it : m_standardTrace.getAccessedStorageValues().at( _address ) ) {
            auto& storageAddress = it.first;
            auto& storageValue = it.second;

            bool includePair;
            if ( !_statePre.addressInUse( _address ) ) {
                // a new storage pair created. Include it in post diff
                includePair = true;
            } else if ( storageValue == 0 ) {
                // the value has been deleted. We do not include it in post diff
                includePair = false;
            } else {
                // see if the storage value has been changed
                includePair =
                    _statePre.originalStorageValue( _address, storageAddress ) != storageValue;
            }

            if ( includePair ) {
                storagePairs[toHex( storageAddress )] = toHex( storageValue );
                // return limited number of storage pairs to prevent DOS attacks
                m_storageValuesReturnedPost++;
                if ( m_storageValuesReturnedPost >= MAX_STORAGE_VALUES_RETURNED )
                    break;
            }
        }

        if ( !storagePairs.empty() )
            value["storage"] = storagePairs;
    }

    if ( !value.empty() )
        _postDiffTrace[toHexPrefixed( _address )] = value;
}


PrestateTracePrinter::PrestateTracePrinter( AlethStandardTrace& standardTrace )
    : TracePrinter( standardTrace, "prestateTrace" ) {}


void PrestateTracePrinter::printAccountPreDiff( Json::Value& _preDiffTrace,
    const HistoricState& _statePre, const HistoricState& _statePost, const Address& _address ) {
    Json::Value value( Json::objectValue );

    // balance diff
    if ( !_statePre.addressInUse( _address ) )
        return;

    auto balance = _statePre.balance( _address );
    auto& code = _statePre.code( _address );
    auto nonce = _statePre.getNonce( _address );


    if ( !_statePost.addressInUse( _address ) || _statePost.balance( _address ) != balance ) {
        value["balance"] = toCompactHexPrefixed( balance );
    }
    if ( !_statePost.addressInUse( _address ) || _statePost.getNonce( _address ) != nonce ) {
        value["nonce"] = ( uint64_t ) nonce;
    }
    if ( !_statePost.addressInUse( _address ) || _statePost.code( _address ) != code ) {
        if ( code != NullBytes ) {
            value["code"] = toHexPrefixed( code );
        }
    }

    if ( m_standardTrace.getAccessedStorageValues().find( _address ) !=
         m_standardTrace.getAccessedStorageValues().end() ) {
        Json::Value storagePairs;

        for ( auto&& it : m_standardTrace.getAccessedStorageValues().at( _address ) ) {
            auto& storageAddress = it.first;
            auto& storageValuePost = it.second;
            bool includePair;
            if ( !_statePost.addressInUse( _address ) ) {
                // contract has been deleted. Include in diff
                includePair = true;
            } else if ( it.second == 0 ) {
                // storage has been deleted. Do not include
                includePair = false;
            } else {
                includePair =
                    _statePre.originalStorageValue( _address, storageAddress ) != storageValuePost;
            }

            if ( includePair ) {
                storagePairs[toHex( it.first )] =
                    toHex( _statePre.originalStorageValue( _address, it.first ) );
                // return limited number of storage pairs to prevent DOS attacks
                m_storageValuesReturnedPre++;
                if ( m_storageValuesReturnedPre >= MAX_STORAGE_VALUES_RETURNED )
                    break;
            }
        }

        if ( !storagePairs.empty() )
            value["storage"] = storagePairs;
    }

    if ( !value.empty() )
        _preDiffTrace[toHexPrefixed( _address )] = value;
}
}  // namespace dev::eth

#endif