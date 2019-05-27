/*
    Modifications Copyright (C) 2018-2019 SKALE Labs

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
/** @file block.cpp
 * @author Dimitry Khokhlov <dimitry@ethdev.com>
 * @date 2015
 * Block test functions.
 */

#include <libethereum/Block.h>
#include <libethereum/BlockQueue.h>
#include <test/tools/libtesteth/BlockChainHelper.h>
#include <test/tools/libtesteth/JsonSpiritHeaders.h>
#include <test/tools/libtesteth/TestHelper.h>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::test;
using skale::State;

BOOST_FIXTURE_TEST_SUITE( BlockSuite, TestOutputHelperFixture )

BOOST_FIXTURE_TEST_SUITE( GasPricer, TestOutputHelperFixture )

BOOST_AUTO_TEST_CASE( bNormalTransactionInput ) {
    TestBlockChain testBlockchain( TestBlockChain::defaultGenesisBlock( 63000 ) );
    TestBlock const& genesisBlock = testBlockchain.testGenesis();
    State const& genesisState = genesisBlock.state();
    BlockChain const& blockchain = testBlockchain.getInterface();

    TestBlock testBlock;
    TestTransaction transaction1 = TestTransaction::defaultTransaction( 1, 1, 21000 );
    testBlock.addTransaction( transaction1 );
    TestTransaction transaction2 = TestTransaction::defaultTransaction( 2, 1, 21000 );
    testBlock.addTransaction( transaction2 );

    // Normal transaction input
    ZeroGasPricer gp;
    Block block = blockchain.genesisBlock( genesisState );
    block.sync( blockchain );
    TestBlock testBlockT = testBlock;
    block.sync( blockchain, testBlockT.transactionQueue(), gp );
    BOOST_REQUIRE( testBlockT.transactionQueue().topTransactions( 4 ).size() == 2 );
}

BOOST_AUTO_TEST_CASE( bBlockNotSyncedToBlockchain ) {
    TestBlockChain testBlockchain( TestBlockChain::defaultGenesisBlock( 63000 ) );
    TestBlock const& genesisBlock = testBlockchain.testGenesis();
    State const& genesisState = genesisBlock.state();
    BlockChain const& blockchain = testBlockchain.getInterface();

    TestBlock testBlock;
    TestTransaction transaction1 = TestTransaction::defaultTransaction( 1, 1, 21000 );
    testBlock.addTransaction( transaction1 );
    TestTransaction transaction2 = TestTransaction::defaultTransaction( 2, 1, 21000 );
    testBlock.addTransaction( transaction2 );

    // Block not synced to blockchain
    TestBlock testBlockT = testBlock;
    ZeroGasPricer gp;
    Block block = blockchain.genesisBlock( genesisState );
    block.sync( blockchain, testBlockT.transactionQueue(), gp );
    BOOST_REQUIRE( testBlockT.transactionQueue().topTransactions( 4 ).size() == 2 );
}

BOOST_AUTO_TEST_CASE( bExceedBlockGasLimit ) {
    TestBlockChain testBlockchain( TestBlockChain::defaultGenesisBlock( 63000 ) );
    TestBlock const& genesisBlock = testBlockchain.testGenesis();
    State const& genesisState = genesisBlock.state();
    BlockChain const& blockchain = testBlockchain.getInterface();

    TestBlock testBlock;
    TestTransaction transaction1 = TestTransaction::defaultTransaction( 1, 1, 21000 );
    testBlock.addTransaction( transaction1 );
    TestTransaction transaction2 = TestTransaction::defaultTransaction( 2, 1, 21000 );
    testBlock.addTransaction( transaction2 );

    // Transactions valid but exceed block gasLimit - BlockGasLimitReached
    // we include it in block but don't execute
    TestBlock testBlockT = testBlock;
    TestTransaction transaction = TestTransaction::defaultTransaction( 3, 1, 1500000 );
    testBlockT.addTransaction( transaction );

    ZeroGasPricer gp;
    Block block = blockchain.genesisBlock( genesisState );
    block.sync( blockchain );

    u256 balanceBefore = block.state().balance( transaction1.transaction().sender() );

    block.sync( blockchain, testBlockT.transactionQueue(), gp );
    BOOST_REQUIRE( testBlockT.transactionQueue().topTransactions( 4 ).size() == 3 );

    u256 balanceAfter = block.state().balance( transaction1.transaction().sender() );

    BOOST_REQUIRE_EQUAL( balanceBefore - balanceAfter, 21000 * 2 + 200 );  // run only 2
    BOOST_REQUIRE_EQUAL( block.state().getNonce( transaction1.transaction().sender() ),
        u256( 3 ) );  // nonce starts with 1
}

BOOST_AUTO_TEST_CASE( bTemporaryNoGasLeft ) {
    TestBlockChain testBlockchain( TestBlockChain::defaultGenesisBlock( 63000 ) );
    TestBlock const& genesisBlock = testBlockchain.testGenesis();
    State const& genesisState = genesisBlock.state();
    BlockChain const& blockchain = testBlockchain.getInterface();

    TestBlock testBlock;
    TestTransaction transaction1 = TestTransaction::defaultTransaction( 1, 1, 21000 );
    testBlock.addTransaction( transaction1 );
    TestTransaction transaction2 = TestTransaction::defaultTransaction( 2, 1, 21000 );
    testBlock.addTransaction( transaction2 );

    // Temporary no gas left in the block
    TestBlock testBlockT = testBlock;
    TestTransaction transaction = TestTransaction::defaultTransaction( 3, 1, 25000,
        importByteArray( "2384796013245973640576230475239456238475623874502348572634857234592736"
                         "45345234689563486749" ) );
    testBlockT.addTransaction( transaction );

    ZeroGasPricer gp;
    Block block = blockchain.genesisBlock( genesisState );
    block.sync( blockchain );
    block.sync( blockchain, testBlockT.transactionQueue(), gp );
    BOOST_REQUIRE( testBlockT.transactionQueue().topTransactions( 4 ).size() == 3 );
}

BOOST_AUTO_TEST_CASE( bInvalidNonceNoncesAhead ) {
    // Add some amount of gas because block limit decreases
    TestBlockChain testBlockchain( TestBlockChain::defaultGenesisBlock( 63000 + 21000 ) );
    TestBlock const& genesisBlock = testBlockchain.testGenesis();
    State const& genesisState = genesisBlock.state();
    BlockChain const& blockchain = testBlockchain.getInterface();

    TestBlock testBlock;
    TestTransaction transaction1 = TestTransaction::defaultTransaction( 1, 1, 21000 );
    testBlock.addTransaction( transaction1 );
    TestTransaction transaction2 = TestTransaction::defaultTransaction( 2, 1, 21000 );
    testBlock.addTransaction( transaction2 );

    // Invalid nonce - nonces ahead
    TestBlock testBlockT = testBlock;
    TestTransaction transaction = TestTransaction::defaultTransaction( 12, 1, 21000 );
    testBlockT.addTransaction( transaction );

    ZeroGasPricer gp;
    Block block = blockchain.genesisBlock( genesisState );
    block.sync( blockchain );

    u256 balanceBefore = block.state().balance( transaction1.transaction().sender() );

    block.sync( blockchain, testBlockT.transactionQueue(), gp );
    BOOST_REQUIRE( testBlockT.transactionQueue().topTransactions( 4 ).size() == 3 );

    u256 balanceAfter = block.state().balance( transaction1.transaction().sender() );

    BOOST_REQUIRE_EQUAL( balanceBefore - balanceAfter, 21000 * 2 + 200 );  // run only 2
    BOOST_REQUIRE_EQUAL( block.state().getNonce( transaction1.transaction().sender() ),
        u256( 3 ) );  // nonce starts with 1
}

BOOST_AUTO_TEST_CASE( bInvalidNonceNonceTooLow ) {
    TestBlockChain testBlockchain( TestBlockChain::defaultGenesisBlock( 63000 ) );
    TestBlock const& genesisBlock = testBlockchain.testGenesis();
    State const& genesisState = genesisBlock.state();
    BlockChain const& blockchain = testBlockchain.getInterface();

    TestBlock testBlock;
    TestTransaction transaction1 = TestTransaction::defaultTransaction( 1, 1, 21000 );
    testBlock.addTransaction( transaction1 );
    TestTransaction transaction2 = TestTransaction::defaultTransaction( 2, 1, 21000 );
    testBlock.addTransaction( transaction2 );

    // Invalid nonce - nonce too low
    TestBlock testBlockT = testBlock;
    TestTransaction transaction = TestTransaction::defaultTransaction( 0, 1, 21000 );
    testBlockT.addTransaction( transaction );

    ZeroGasPricer gp;
    Block block = blockchain.genesisBlock( genesisState );
    block.sync( blockchain );

    u256 balanceBefore = block.state().balance( transaction1.transaction().sender() );

    block.sync( blockchain, testBlockT.transactionQueue(), gp );
    BOOST_REQUIRE( testBlockT.transactionQueue().topTransactions( 4 ).size() == 3 );

    u256 balanceAfter = block.state().balance( transaction1.transaction().sender() );

    BOOST_REQUIRE_EQUAL( balanceBefore - balanceAfter, 21000 * 2 + 200 );  // run only 2
    BOOST_REQUIRE_EQUAL( block.state().getNonce( transaction1.transaction().sender() ),
        u256( 3 ) );  // nonce starts with 1
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE( bGetReceiptOverflow ) {
    TestBlockChain bc;
    TestBlock const& genesisBlock = bc.testGenesis();
    State const& genesisState = genesisBlock.state();
    BlockChain const& blockchain = bc.getInterface();
    Block block = blockchain.genesisBlock( genesisState );
    BOOST_CHECK_THROW( block.receipt( 123 ), std::out_of_range );
}

BOOST_FIXTURE_TEST_SUITE( ConstantinopleBlockSuite, ConstantinopleTestFixture )

BOOST_AUTO_TEST_CASE( bConstantinopleBlockReward ) {
    TestBlockChain testBlockchain;
    TestBlock testBlock;
    testBlock.mine( testBlockchain );
    testBlockchain.addBlock( testBlock );

    TestBlock const& topBlock = testBlockchain.topBlock();
    BOOST_REQUIRE_EQUAL( topBlock.state().balance( topBlock.beneficiary() ), 2 * ether );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
