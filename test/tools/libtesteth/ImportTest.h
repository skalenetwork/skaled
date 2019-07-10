/*
    Modifications Copyright (C) 2018 SKALE Labs

    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file
 * Helper class for managing data when running state tests
 */

#pragma once
#include <libethashseal/GenesisInfo.h>
#include <libskale/State.h>
#include <test/tools/libtesteth/JsonSpiritHeaders.h>
#include <test/tools/libtestutils/Common.h>

namespace dev {
namespace test {
class ImportTest {
public:
    ImportTest( json_spirit::mObject const& _input, json_spirit::mObject& _output );

    // imports
    void importEnv( json_spirit::mObject const& _o );
    static void importState( json_spirit::mObject const& _o, skale::State& _state );
    static void importState(
        json_spirit::mObject const& _o, skale::State& _state, eth::AccountMaskMap& o_mask );
    static void importTransaction( json_spirit::mObject const& _o, eth::Transaction& o_tr );
    void importTransaction( json_spirit::mObject const& _o );
    static json_spirit::mObject makeAllFieldsHex(
        json_spirit::mObject const& _o, bool _isHeader = false );
    static void parseJsonStrValueIntoSet(
        json_spirit::mValue const& _json, std::set< std::string >& _out );

    enum testType { StateTest, BlockchainTest };
    static std::set< eth::Network > getAllNetworksFromExpectSections(
        json_spirit::mArray const& _expects, testType _testType );


    // check functions
    // check that networks in the vector are allowed
    static void checkAllowedNetwork( std::string const& _network );
    static void checkAllowedNetwork( std::set< std::string > const& _networks );
    static void checkBalance(
        skale::State const& _pre, skale::State const& _post, bigint _miningReward = 0 );

    bytes executeTest( bool _isFilling );
    int exportTest();
    static int compareStates( skale::State const& _stateExpect, skale::State const& _statePost,
        eth::AccountMaskMap const _expectedStateOptions = eth::AccountMaskMap(),
        WhenError _throw = WhenError::Throw );
    bool checkGeneralTestSection( json_spirit::mObject const& _expects,
        std::vector< size_t >& _errorTransactions, std::string const& _network = "" ) const;
    void traceStateDiff();

    skale::State m_statePre;
    skale::State m_statePost;

private:
    using ExecOutput = std::pair< eth::ExecutionResult, eth::TransactionReceipt >;
    std::tuple< skale::State, ExecOutput, skale::ChangeLog > executeTransaction(
        eth::Network const _sealEngineNetwork, eth::EnvInfo const& _env,
        skale::State const& _preState, eth::Transaction const& _tr );

    std::unique_ptr< eth::LastBlockHashesFace const > m_lastBlockHashes;
    std::unique_ptr< eth::EnvInfo > m_envInfo;
    eth::Transaction m_transaction;

    // General State Tests
    struct transactionToExecute {
        transactionToExecute( int d, int g, int v, eth::Transaction const& t )
            : dataInd( d ),
              gasInd( g ),
              valInd( v ),
              transaction( t ),
              postState( 0 ),
              netId( eth::Network::MainNetwork ),
              output( std::make_pair( eth::ExecutionResult(),
                  eth::TransactionReceipt( h256(), u256(), eth::LogEntries() ) ) ) {}
        int dataInd;
        int gasInd;
        int valInd;
        eth::Transaction transaction;
        skale::State postState;
        skale::ChangeLog changeLog;
        eth::Network netId;
        ExecOutput output;
    };
    std::vector< transactionToExecute > m_transactions;
    using StateAndMap = std::pair< skale::State, eth::AccountMaskMap >;
    using TrExpectSection = std::pair< transactionToExecute, StateAndMap >;
    bool checkGeneralTestSectionSearch( json_spirit::mObject const& _expects,
        std::vector< size_t >& _errorTransactions, std::string const& _network = "",
        TrExpectSection* _search = NULL ) const;

    /// Create blockchain test fillers for specified _networks and test information (env, pre, txs)
    /// of Importtest then fill blockchain fillers into tests.
    void makeBlockchainTestFromStateTest( std::set< eth::Network > const& _networks ) const;

    json_spirit::mObject const& m_testInputObject;
    json_spirit::mObject& m_testOutputObject;

    Logger m_logger{createLogger( VerbosityInfo, "state" )};
};

template < class T >
bool inArray( std::vector< T > const& _array, const T& _val ) {
    for ( auto const& obj : _array )
        if ( obj == _val )
            return true;
    return false;
}

}  // namespace test
}  // namespace dev
