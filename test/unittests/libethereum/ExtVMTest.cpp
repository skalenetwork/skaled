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

#include <test/tools/libtesteth/BlockChainHelper.h>
#include <test/tools/libtesteth/TestHelper.h>
#include <test/tools/libtestutils/TestLastBlockHashes.h>

#include <libethereum/Block.h>
#include <libethereum/ExtVM.h>

using namespace dev;
using namespace dev::eth;
using namespace dev::test;
using skale::State;

class ExtVMTestFixture : public TestOutputHelperFixture {
public:
    ExtVMTestFixture()
        : networkSelector( eth::Network::ConstantinopleTransitionTest ),
          testBlockchain( TestBlockChain::defaultGenesisBlock() ),
          genesisBlock( testBlockchain.testGenesis() ),
          genesisState( genesisBlock.state() ),
          blockchain( testBlockchain.getInterface() ) {
        TestBlock testBlock;
        // block 1 - before Constantinople
        testBlock.mine( testBlockchain );
        testBlockchain.addBlock( testBlock );

        // block 2 - first Constantinople block
        testBlock.mine( testBlockchain );
        testBlockchain.addBlock( testBlock );
    }

    NetworkSelector networkSelector;
    TestBlockChain testBlockchain;
    TestBlock const& genesisBlock;
    State const& genesisState;
    BlockChain const& blockchain;
};

BOOST_FIXTURE_TEST_SUITE( ExtVmSuite, ExtVMTestFixture )

BOOST_AUTO_TEST_CASE( BlockhashOutOfBoundsRetunsZero ) {
    Block block = blockchain.genesisBlock( genesisState );
    block.sync( blockchain );

    TestLastBlockHashes lastBlockHashes( {} );
    EnvInfo envInfo( block.info(), lastBlockHashes, 0 );
    Address addr( "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b" );
    ExtVM extVM( block.mutableState(), envInfo, *blockchain.sealEngine(), addr, addr, addr, 0, 0,
        {}, {}, {}, 0, false, false );

    BOOST_CHECK_EQUAL( extVM.blockHash( 100 ), h256() );
}

BOOST_AUTO_TEST_CASE( BlockhashBeforeConstantinopleReliesOnLastHashes ) {
    Block block = blockchain.genesisBlock( genesisState );
    block.sync( blockchain );

    h256s lastHashes{h256( "0xaaabbbccc" ), h256( "0xdddeeefff" )};
    TestLastBlockHashes lastBlockHashes( lastHashes );
    EnvInfo envInfo( block.info(), lastBlockHashes, 0 );
    Address addr( "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b" );
    ExtVM extVM( block.mutableState(), envInfo, *blockchain.sealEngine(), addr, addr, addr, 0, 0,
        {}, {}, {}, 0, false, false );
    h256 hash = extVM.blockHash( 1 );
    BOOST_REQUIRE_EQUAL( hash, lastHashes[0] );
}

BOOST_AUTO_TEST_SUITE_END()
