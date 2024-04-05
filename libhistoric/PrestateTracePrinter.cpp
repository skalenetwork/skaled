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
    if ( m_trace.getOptions().prestateDiffMode ) {
        printDiffTrace( _jsonTrace, _er, _statePre, _statePost );
    } else {
        printPreStateTrace( _jsonTrace, _statePre, _statePost );
    }
}

void PrestateTracePrinter::printPreStateTrace(
    Json::Value& _jsonTrace, const HistoricState& _statePre, const HistoricState& _statePost ) {
    for ( auto&& item : m_trace.getAccessedAccounts() ) {
        printAllAccessedAccountPreValues( _jsonTrace, _statePre, _statePost, item );
    };

    // geth always prints the balance of block miner balance

    Address minerAddress = m_trace.getBlockAuthor();
    u256 minerBalance = getBalancePre( _statePre, minerAddress );
    _jsonTrace[toHexPrefixed( minerAddress )]["balance"] =
        AlethStandardTrace::toGethCompatibleCompactHexPrefixed( minerBalance );
}


void PrestateTracePrinter::printDiffTrace( Json::Value& _jsonTrace, const ExecutionResult&,
    const HistoricState& _statePre, const HistoricState& _statePost ) {
    STATE_CHECK( _jsonTrace.isObject() )

    Json::Value preDiff( Json::objectValue );
    Json::Value postDiff( Json::objectValue );

    for ( auto&& item : m_trace.getAccessedAccounts() ) {
        printAccountPreDiff( preDiff, _statePre, _statePost, item );
        printAccountPostDiff( postDiff, _statePre, _statePost, item );
    };


    // now deal with miner balance change as a result of transaction
    // geth always prints miner balance change when NOT in call
    if ( !m_trace.isCall() ) {
        printMinerBalanceChange( _statePre, _statePost, preDiff, postDiff );
    }

    // we are done, complete the trace JSON

    _jsonTrace["pre"] = preDiff;
    _jsonTrace["post"] = postDiff;
}

void PrestateTracePrinter::printMinerBalanceChange( const HistoricState& _statePre,
    const HistoricState& _statePost, Json::Value& preDiff, Json::Value& postDiff ) const {
    Address minerAddress = m_trace.getBlockAuthor();
    u256 minerBalancePre = getBalancePre( _statePre, minerAddress );
    u256 minerBalancePost = getBalancePost( _statePost, minerAddress );

    preDiff[toHexPrefixed( minerAddress )]["balance"] =
        AlethStandardTrace::toGethCompatibleCompactHexPrefixed( minerBalancePre );
    postDiff[toHexPrefixed( minerAddress )]["balance"] =
        AlethStandardTrace::toGethCompatibleCompactHexPrefixed( minerBalancePost );
}


// this function returns original values (pre) to result
void PrestateTracePrinter::printAllAccessedAccountPreValues( Json::Value& _jsonTrace,
    const HistoricState& _statePre, const HistoricState& _statePost, const Address& _address ) {
    STATE_CHECK( _jsonTrace.isObject() )

    Json::Value accountPreValues;
    // if this _address did not exist, we do not include it in the diff
    // unless it is contract deploy
    if ( !_statePre.addressInUse( _address ) && !m_trace.isContractCreation() )
        return;

    auto balance = _statePre.balance( _address );

    // take into account that for calls balance is modified in the state before execution
    if ( m_trace.isCall() && _address == m_trace.getFrom() ) {
        balance = m_trace.getOriginalFromBalance();
    }
    printNonce( _statePre, _statePost, _address, accountPreValues );


    accountPreValues["balance"] = AlethStandardTrace::toGethCompatibleCompactHexPrefixed( balance );

    bytes const& code = _statePre.code( _address );
    if ( code != NullBytes ) {
        accountPreValues["code"] = toHexPrefixed( code );
    }

    Json::Value storagePairs;

    auto& accessedStoragedValues = m_trace.getAccessedStorageValues();

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

void PrestateTracePrinter::printNonce( const HistoricState& _statePre,
    const HistoricState& _statePost, const Address& _address,
    Json::Value& accountPreValues ) const {
    // geth does not print nonce for from address in debug_traceCall;


    if ( m_trace.isCall() && _address == m_trace.getFrom() ) {
        return;
    }

    // handle special case of contract creation transaction
    // in this case geth prints nonce = 1 for the contract
    // that has been created
    if ( isNewContract( _statePre, _statePost, _address ) ) {
        accountPreValues["nonce"] = 1;
        return;
    }

    // now handle the generic case

    auto preNonce = ( uint64_t ) _statePre.getNonce( _address );
    auto postNonce = ( uint64_t ) _statePost.getNonce( _address );
    // in calls nonce is always printed by geth
    // find out if the address is a contract. Geth always prints nonce for contracts

    if ( postNonce != preNonce || m_trace.isCall() ||
         isPreExistingContract( _statePre, _address ) ) {
        accountPreValues["nonce"] = preNonce;
    }
}

void PrestateTracePrinter::printAccountPreDiff( Json::Value& _preDiffTrace,
    const HistoricState& _statePre, const HistoricState& _statePost, const Address& _address ) {
    Json::Value diffPre( Json::objectValue );

    // If the account did not exist before the transaction, geth does not print it in pre trace
    // exception is a top level contract  created during the CREATE transaction. Geth always prints
    // it in pre diff
    if ( !_statePre.addressInUse( _address ) &&
         !( m_trace.isContractCreation() && isNewContract( _statePre, _statePost, _address ) ) ) {
        return;
    }

    printPreDiffBalance( _statePre, _statePost, _address, diffPre );

    printPreDiffNonce( _statePre, _statePost, _address, diffPre );

    printPreDiffCode( _statePre, _statePost, _address, diffPre );

    printPreDiffStorage( _statePre, _statePost, _address, diffPre );

    if ( !diffPre.empty() )
        _preDiffTrace[toHexPrefixed( _address )] = diffPre;
}

void PrestateTracePrinter::printPreDiffCode( const HistoricState& _statePre,
    const HistoricState& _statePost, const Address& _address, Json::Value& diffPre ) const {
    auto& code = _statePre.code( _address );

    if ( !_statePost.addressInUse( _address ) || _statePost.code( _address ) != code ||
         _statePre.getNonce( _address ) !=
             _statePost.getNonce( _address )  // geth always prints code here if nonce changed
    ) {
        if ( code != NullBytes ) {
            diffPre["code"] = toHexPrefixed( code );
        }
    }
}

void PrestateTracePrinter::printPreDiffBalance( const HistoricState& _statePre,
    const HistoricState& _statePost, const Address& _address, Json::Value& _diffPre ) const {
    auto balancePre = getBalancePre( _statePre, _address );
    auto balancePost = getBalancePost( _statePost, _address );


    // always print balance of from address in calls
    if ( m_trace.isCall() && _address == m_trace.getFrom() ) {
        _diffPre["balance"] = AlethStandardTrace::toGethCompatibleCompactHexPrefixed( balancePre );
        return;
    }

    // handle the case of a contract creation. Geth always prints balance 0 as pre for new contract
    // geth does always print pre nonce equal 1 for newly created contract
    if ( isNewContract( _statePre, _statePost, _address ) ) {
        _diffPre["balance"] = "0x0";
        return;
    }

    // now handle generic case

    if ( !_statePost.addressInUse( _address ) || balancePost != balancePre ||
         _statePre.getNonce( _address ) !=
             _statePost.getNonce( _address ) ) {  // geth alw prints balance if nonce changed
        _diffPre["balance"] = AlethStandardTrace::toGethCompatibleCompactHexPrefixed( balancePre );
    }
}

u256 PrestateTracePrinter::getBalancePre(
    const HistoricState& _statePre, const Address& _address ) const {
    auto balancePre = _statePre.balance( _address );

    if ( m_trace.isCall() && _address == m_trace.getFrom() ) {
        // take into account that for calls balance is modified in the state before execution
        balancePre = m_trace.getOriginalFromBalance();
    }

    return balancePre;
}

u256 PrestateTracePrinter::getBalancePost(
    const HistoricState& _statePost, const Address& _address ) const {
    auto balancePost = _statePost.balance( _address );
    return balancePost;
}

void PrestateTracePrinter::printPreDiffStorage( const HistoricState& _statePre,
    const HistoricState& _statePost, const Address& _address, Json::Value& _diffPre ) {
    // if its a new contract that did not exist before the transaction,
    // then geth does not print its storage in pre
    if ( isNewContract( _statePre, _statePost, _address ) ) {
        return;
    }

    // now handle generic case

    if ( m_trace.getAccessedStorageValues().find( _address ) !=
         m_trace.getAccessedStorageValues().end() ) {
        Json::Value storagePairs;

        for ( auto&& it : m_trace.getAccessedStorageValues().at( _address ) ) {
            auto& storageAddress = it.first;
            auto storageValuePost = _statePost.storage( _address, storageAddress );
            bool includePair;
            if ( !_statePost.addressInUse( _address ) ) {
                // contract has been deleted. Include in diff
                includePair = true;
            } else if ( storageValuePost == 0 ) {
                // storage has been deleted. Do not include
                includePair = false;
            } else {
                auto storageValuePre = _statePre.originalStorageValue( _address, storageAddress );
                includePair =
                    storageValuePre != storageValuePost &&
                    storageValuePre != 0;  // geth does not print storage in pre if it is zero
            }

            if ( includePair ) {
                storagePairs[toHexPrefixed( it.first )] =
                    toHexPrefixed( _statePre.originalStorageValue( _address, it.first ) );
                // return limited number of storage pairs to prevent DOS attacks
                m_storageValuesReturnedPre++;
                if ( m_storageValuesReturnedPre >= MAX_STORAGE_VALUES_RETURNED )
                    break;
            }
        }

        if ( !storagePairs.empty() )
            _diffPre["storage"] = storagePairs;
    }
}

void PrestateTracePrinter::printPreDiffNonce( const HistoricState& _statePre,
    const HistoricState& _statePost, const Address& _address, Json::Value& _diff ) const {
    // geth does not print pre from nonce in calls
    if ( m_trace.isCall() && _address == m_trace.getFrom() ) {
        return;
    };


    // geth does always print pre nonce equal 1 for newly created contract


    if ( isNewContract( _statePre, _statePost, _address ) ) {
        _diff["nonce"] = 1;
        return;
    }

    // now handle generic case
    auto noncePre = _statePre.getNonce( _address );
    if ( !_statePost.addressInUse( _address ) || _statePost.getNonce( _address ) != noncePre ) {
        _diff["nonce"] = ( uint64_t ) noncePre;
    }
}

void PrestateTracePrinter::printPostDiffNonce( const HistoricState& _statePre,
    const HistoricState& _statePost, const Address& _address, Json::Value& _diff ) const {
    // geth does noty print post diff nonce for newly created contract
    if ( isNewContract( _statePre, _statePost, _address ) ) {
        return;
    }

    // now handle generic case

    auto noncePost = _statePost.getNonce( _address );

    if ( _statePre.getNonce( _address ) != noncePost ) {
        _diff["nonce"] = ( uint64_t ) noncePost;
    }
}

void PrestateTracePrinter::printAccountPostDiff( Json::Value& _postDiffTrace,
    const HistoricState& _statePre, const HistoricState& _statePost, const Address& _address ) {
    Json::Value diffPost( Json::objectValue );


    // if this address does not exist post-transaction we dot include it in the trace
    if ( !_statePost.addressInUse( _address ) )
        return;

    printPostDiffBalance( _statePre, _statePost, _address, diffPost );

    printPostDiffNonce( _statePre, _statePost, _address, diffPost );

    diffPost = printPostDiffCode( _statePre, _statePost, _address, diffPost );

    printPostDiffStorage( _statePre, _statePost, _address, diffPost );

    if ( !diffPost.empty() )
        _postDiffTrace[toHexPrefixed( _address )] = diffPost;
}

Json::Value& PrestateTracePrinter::printPostDiffCode( const HistoricState& _statePre,
    const HistoricState& _statePost, const Address& _address, Json::Value& diffPost ) const {
    auto& codePost = _statePost.code( _address );

    if ( !_statePre.addressInUse( _address ) || _statePre.code( _address ) != codePost ) {
        if ( codePost != NullBytes ) {
            diffPost["code"] = toHexPrefixed( codePost );
        }
    }
    return diffPost;
}

void PrestateTracePrinter::printPostDiffBalance( const HistoricState& _statePre,
    const HistoricState& _statePost, const Address& _address, Json::Value& diffPost ) const {
    auto balancePre = getBalancePre( _statePre, _address );
    auto balancePost = getBalancePost( _statePost, _address );

    // geth does not postbalance of from address in calls
    if ( m_trace.isCall() && _address == m_trace.getFrom() ) {
        return;
    }

    // geth does not print postbalance for a newly created contract
    if ( isNewContract( _statePre, _statePost, _address ) ) {
        return;
    }


    // now handle generic case
    if ( !_statePre.addressInUse( _address ) || balancePre != balancePost ) {
        // if the new address, ot if the value changed, include in post trace
        diffPost["balance"] = AlethStandardTrace::toGethCompatibleCompactHexPrefixed( balancePost );
    }
}

void PrestateTracePrinter::printPostDiffStorage( const HistoricState& _statePre,
    const HistoricState& _statePost, const Address& _address,
    Json::Value& diffPost ) {  // post diffs for storage values
    if ( m_trace.getAccessedStorageValues().find( _address ) !=
         m_trace.getAccessedStorageValues().end() ) {
        Json::Value storagePairs( Json::objectValue );

        // iterate over all accessed storage values
        for ( auto&& it : m_trace.getAccessedStorageValues().at( _address ) ) {
            auto& storageAddress = it.first;
            // take into account the fact that the change could have been reverted
            // so we cannot use the value set by the last SSTORE
            auto storageValue = _statePost.storage( _address, it.first );

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
                storagePairs[toHexPrefixed( storageAddress )] = toHexPrefixed( storageValue );
                // return limited number of storage pairs to prevent DOS attacks
                m_storageValuesReturnedPost++;
                if ( m_storageValuesReturnedPost >= MAX_STORAGE_VALUES_RETURNED )
                    break;
            }
        }

        if ( !storagePairs.empty() )
            diffPost["storage"] = storagePairs;
    }
}


PrestateTracePrinter::PrestateTracePrinter( AlethStandardTrace& standardTrace )
    : TracePrinter( standardTrace, "prestateTrace" ) {}


}  // namespace dev::eth

#endif