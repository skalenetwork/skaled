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
/** @file transactionqueue.cpp
 * @author Christoph Jentzsch <cj@ethdev.com>
 * @date 2015
 * TransactionQueue test functions.
 */

#include <libethereum/TransactionQueue.h>
#include <test/tools/libtesteth/BlockChainHelper.h>
#include <test/tools/libtesteth/TestHelper.h>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::test;

namespace {

ImportResult importCurrent( TransactionQueue& _queue, Transaction const& _transaction ) {
    return _queue.import( _transaction, IfDropped::Ignore, false, _transaction.nonce() );
}

ImportResult importCurrent( TransactionQueue& _queue, bytes const& _transactionRLP ) {
    Transaction transaction( _transactionRLP, CheckTransaction::Everything );
    return _queue.import( _transactionRLP, IfDropped::Ignore, false, transaction.nonce() );
}

}

BOOST_FIXTURE_TEST_SUITE( TransactionQueueSuite, TestOutputHelperFixture )

BOOST_AUTO_TEST_CASE( TransactionEIP86 ) {
    dev::eth::TransactionQueue txq;
    Address to = Address( "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b" );
    RLPStream streamRLP;
    streamRLP.appendList( 9 );
    streamRLP << 0 << 10 * szabo << 25000;
    streamRLP << to << 0 << bytes() << 0 << 0 << 0;
    Transaction tx0( streamRLP.out(), CheckTransaction::Everything );
    ImportResult result = importCurrent( txq, tx0 );
    BOOST_CHECK( result == ImportResult::ZeroSignature );
    BOOST_CHECK( txq.knownTransactions().size() == 0 );
}

BOOST_AUTO_TEST_CASE( tqMaxNonce ) {
    dev::eth::TransactionQueue txq;

    // from a94f5374fce5edbc8e2a8697c15331677e6ebf0b
    const u256 gasCost = 10 * szabo;
    const u256 gas = 25000;
    Address dest = Address( "0x095e7baea6a6c7c4c2dfeb977efac326af552d87" );
    Address to = Address( "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b" );
    Secret sec = Secret( "0x45a915e4d060149eb4365960e6a7a45f334393093061116b197e3240065ff2d8" );
    Transaction tx0( 0, gasCost, gas, dest, bytes(), 0, sec );
    Transaction tx0_1( 1, gasCost, gas, dest, bytes(), 0, sec );
    Transaction tx1( 0, gasCost, gas, dest, bytes(), 1, sec );
    Transaction tx2( 0, gasCost, gas, dest, bytes(), 2, sec );
    Transaction tx9( 0, gasCost, gas, dest, bytes(), 9, sec );

    BOOST_REQUIRE( importCurrent( txq, tx0 ) == ImportResult::Success );
    BOOST_CHECK( 1 == txq.maxNonce( to ) );
    BOOST_REQUIRE( importCurrent( txq, tx0 ) == ImportResult::AlreadyKnown );
    BOOST_CHECK( 1 == txq.maxNonce( to ) );
    BOOST_REQUIRE( importCurrent( txq, tx0_1 ) == ImportResult::SameNonceAlreadyInQueue );
    BOOST_CHECK( 1 == txq.maxNonce( to ) );
    BOOST_REQUIRE( importCurrent( txq, tx1 ) == ImportResult::Success );
    BOOST_CHECK( 2 == txq.maxNonce( to ) );
    BOOST_REQUIRE( txq.import( tx9, IfDropped::Ignore, true, 0 ) == ImportResult::Success );
    BOOST_CHECK( 10 == txq.maxNonce( to ) );
    BOOST_REQUIRE( txq.import( tx2, IfDropped::Ignore, false, 0 ) == ImportResult::Success );
    BOOST_CHECK( 10 == txq.maxNonce( to ) );
}

BOOST_AUTO_TEST_CASE( tqPriority ) {
    dev::eth::TransactionQueue txq;

    const u256 gasCostCheap = 10 * szabo;
    const u256 gasCostMed = 20 * szabo;
    const u256 gasCostHigh = 30 * szabo;
    const u256 gas = 25000;
    Address dest = Address( "0x095e7baea6a6c7c4c2dfeb977efac326af552d87" );
    Secret sender1 = Secret( "0x3333333333333333333333333333333333333333333333333333333333333333" );
    Secret sender2 = Secret( "0x4444444444444444444444444444444444444444444444444444444444444444" );
    Transaction tx0( 0, gasCostCheap, gas, dest, bytes(), 0, sender1 );
    Transaction tx0_1( 1, gasCostMed, gas, dest, bytes(), 0, sender1 );
    Transaction tx1( 0, gasCostCheap, gas, dest, bytes(), 1, sender1 );
    Transaction tx2( 0, gasCostHigh, gas, dest, bytes(), 0, sender2 );
    Transaction tx3( 0, gasCostCheap - 1, gas, dest, bytes(), 1, sender2 );
    Transaction tx4( 0, gasCostMed, gas, dest, bytes(), 2, sender1 );
    Transaction tx5( 0, gasCostHigh, gas, dest, bytes(), 2, sender2 );

    importCurrent( txq, tx0 );
    BOOST_CHECK( Transactions{tx0} == txq.topTransactions( 256 ) );
    importCurrent( txq, tx0 );
    BOOST_CHECK( Transactions{tx0} == txq.topTransactions( 256 ) );
    importCurrent( txq, tx0_1 );
    BOOST_CHECK( Transactions{tx0} == txq.topTransactions( 256 ) );  // no replacement any more!
    importCurrent( txq, tx1 );
    BOOST_CHECK( ( Transactions{tx0, tx1} ) == txq.topTransactions( 256 ) );
    importCurrent( txq, tx2 );
    BOOST_CHECK( ( Transactions{tx2, tx0, tx1} ) == txq.topTransactions( 256 ) );
    importCurrent( txq, tx3 );
    BOOST_CHECK( ( Transactions{tx2, tx0, tx1, tx3} ) == txq.topTransactions( 256 ) );
    importCurrent( txq, tx4 );
    BOOST_CHECK( ( Transactions{tx2, tx0, tx1, tx3, tx4} ) == txq.topTransactions( 256 ) );
    importCurrent( txq, tx5 );
    BOOST_CHECK( ( Transactions{tx2, tx0, tx1, tx3, tx5, tx4} ) == txq.topTransactions( 256 ) );

    txq.drop( tx0.sha3() );
    // prev BOOST_CHECK( ( Transactions{tx2, tx1, tx3, tx5, tx4} ) == txq.topTransactions( 256 ) );
    // now tx4 has nonce increase 1, and goes lower then tx5 and tx3
    BOOST_CHECK( ( Transactions{ tx2, tx1, tx4, tx3, tx5 } ) == txq.topTransactions( 256 ) );
    txq.drop( tx1.sha3() );
    BOOST_CHECK( ( Transactions{ tx2, tx4, tx3, tx5 } ) == txq.topTransactions( 256 ) );
    txq.drop( tx5.sha3() );
    BOOST_CHECK( ( Transactions{ tx2, tx4, tx3 } ) == txq.topTransactions( 256 ) );

    Transaction tx6( 0, gasCostMed, gas, dest, bytes(), 20, sender1 );
    importCurrent( txq, tx6 );
    BOOST_CHECK( ( Transactions{tx2, tx4, tx3, tx6} ) == txq.topTransactions( 256 ) );

    Transaction tx7( 0, gasCostHigh, gas, dest, bytes(), 2, sender2 );
    importCurrent( txq, tx7 );
    // deterministic signature: hash of tx5 and tx7 will be same
    BOOST_CHECK( ( Transactions{ tx2, tx4, tx3, tx6 } ) == txq.topTransactions( 256 ) );
}

BOOST_AUTO_TEST_CASE( tqNonceChange ) {
    dev::eth::TransactionQueue txq;

    const u256 gasCost = 10 * szabo;
    const u256 gas = 25000;
    Address dest = Address( "0x095e7baea6a6c7c4c2dfeb977efac326af552d87" );
    Secret sender1 = Secret( "0x3333333333333333333333333333333333333333333333333333333333333333" );
    Secret sender2 = Secret( "0x4444444444444444444444444444444444444444444444444444444444444444" );
    Transaction tx10( 0, gasCost, gas, dest, bytes(), 0, sender1 );
    Transaction tx11( 0, gasCost, gas, dest, bytes(), 1, sender1 );
    Transaction tx12( 0, gasCost, gas, dest, bytes(), 2, sender1 );
    Transaction tx13( 0, gasCost, gas, dest, bytes(), 3, sender1 );

    Transaction tx20( 0, gasCost, gas, dest, bytes(), 0, sender2 );
    Transaction tx21( 0, gasCost, gas, dest, bytes(), 1, sender2 );
    Transaction tx22( 0, gasCost, gas, dest, bytes(), 2, sender2 );
    Transaction tx23( 0, gasCost, gas, dest, bytes(), 3, sender2 );

    // 1 insert 0,1,2 for both senders
    importCurrent( txq, tx20 ); // h = 0
    importCurrent( txq, tx21 ); // h = 1
    importCurrent( txq, tx22 ); // h = 2
    importCurrent( txq, tx10 ); // h = 0
    importCurrent( txq, tx11 ); // h = 1
    importCurrent( txq, tx12 ); // h = 2
    importCurrent( txq, tx13 ); // h = 3

    // 2 increase nonce for account 2
    txq.dropGood( tx20 );
    txq.dropGood( tx21 );

    // 3 insert tx with height = 3-2=1
    importCurrent( txq, tx23 ); // h = 1 => goes with tx11

    Transactions top6 = txq.topTransactions( 6 );
    for ( auto tx : top6 ) {
        std::cout << tx.from() << " " << tx.nonce() << std::endl;
    }
    // expected BAD result       [tx10], [tx11, tx23], [tx12, tx22], [tx13] !!!
    // prev without sort BOOST_REQUIRE( ( Transactions{tx10, tx11, tx22, tx23, tx12, tx13 } ) ==
    // top6 ); with sort:
    BOOST_REQUIRE( ( Transactions{ tx10, tx22, tx11, tx23, tx12, tx13 } ) == top6 );
}

BOOST_AUTO_TEST_CASE( tqFuture ) {
    dev::eth::TransactionQueue txq;
    const u256 gasCostMed = 20 * szabo;
    const u256 gas = 25000;
    Address dest = Address( "0x095e7baea6a6c7c4c2dfeb977efac326af552d87" );
    Secret sender = Secret( "0x3333333333333333333333333333333333333333333333333333333333333333" );
    Transaction tx0( 0, gasCostMed, gas, dest, bytes(), 0, sender );
    Transaction tx1( 0, gasCostMed, gas, dest, bytes(), 1, sender );
    Transaction tx2( 0, gasCostMed, gas, dest, bytes(), 2, sender );
    Transaction tx3( 0, gasCostMed, gas, dest, bytes(), 3, sender );
    Transaction tx4( 0, gasCostMed, gas, dest, bytes(), 4, sender );

    importCurrent( txq, tx0 );
    importCurrent( txq, tx1 );
    importCurrent( txq, tx2 );
    importCurrent( txq, tx3 );
    importCurrent( txq, tx4 );
    BOOST_CHECK( ( Transactions{tx0, tx1, tx2, tx3, tx4} ) == txq.topTransactions( 256 ) );

    txq.setFuture( tx2.sha3() );
    BOOST_CHECK( ( Transactions{ tx0, tx1 } ) == txq.topTransactions( 256 ) );

    // TODO disabled it temporarily!!
    //    Transaction tx2_2( 1, gasCostMed, gas, dest, bytes(), 2, sender );
    //    importCurrent( txq, tx2_2 );
    //    BOOST_CHECK( ( Transactions{tx0, tx1, tx2_2, tx3, tx4} ) == txq.topTransactions( 256 ) );
    //    // no replacement
}


BOOST_AUTO_TEST_CASE( tqLimits ) {
    dev::eth::TransactionQueue txq( 3, 3 );
    const u256 gasCostMed = 20 * szabo;
    const u256 gas = 25000;
    Address dest = Address( "0x095e7baea6a6c7c4c2dfeb977efac326af552d87" );
    Secret sender = Secret( "0x3333333333333333333333333333333333333333333333333333333333333333" );
    Secret sender2 = Secret( "0x4444444444444444444444444444444444444444444444444444444444444444" );
    Transaction tx0( 0, gasCostMed, gas, dest, bytes(), 0, sender );
    Transaction tx1( 0, gasCostMed, gas, dest, bytes(), 1, sender );
    Transaction tx2( 0, gasCostMed, gas, dest, bytes(), 2, sender );
    Transaction tx3( 0, gasCostMed, gas, dest, bytes(), 3, sender );
    Transaction tx4( 0, gasCostMed, gas, dest, bytes(), 4, sender );
    Transaction tx5( 0, gasCostMed + 1, gas, dest, bytes(), 0, sender2 );

    BOOST_REQUIRE( importCurrent( txq, tx0 ) == ImportResult::Success );
    BOOST_REQUIRE( importCurrent( txq, tx1 ) == ImportResult::Success );
    BOOST_REQUIRE( importCurrent( txq, tx2 ) == ImportResult::Success );
    BOOST_REQUIRE( importCurrent( txq, tx3 ) == ImportResult::QueueIsFull );
    BOOST_REQUIRE( importCurrent( txq, tx4 ) == ImportResult::QueueIsFull );
    BOOST_REQUIRE( importCurrent( txq, tx5 ) == ImportResult::QueueIsFull );
    BOOST_CHECK( ( Transactions{tx0, tx1, tx2} ) == txq.topTransactions( 256 ) );
}

BOOST_AUTO_TEST_CASE( tqCurrentFullDoesNotFallBackToFuture ) {
    dev::eth::TransactionQueue tq( 1, 3 );

    const u256 gasCostMed = 20 * szabo;
    const u256 gas = 25000;
    Address dest = Address( "0x095e7baea6a6c7c4c2dfeb977efac326af552d87" );
    Secret sender1 = Secret( "0x3333333333333333333333333333333333333333333333333333333333333333" );
    Secret sender2 = Secret( "0x4444444444444444444444444444444444444444444444444444444444444444" );
    Transaction tx0( 0, gasCostMed, gas, dest, bytes(), 0, sender1 );
    Transaction tx1( 0, gasCostMed, gas, dest, bytes(), 1, sender1 );
    Transaction tx2( 0, gasCostMed, gas, dest, bytes(), 2, sender1 );
    Transaction otherCurrent( 0, gasCostMed, gas, dest, bytes(), 0, sender2 );

    BOOST_REQUIRE( tq.import( tx0, IfDropped::Ignore, true, 0 ) == ImportResult::Success );
    BOOST_REQUIRE(
        tq.import( otherCurrent, IfDropped::Ignore, true, 0 ) == ImportResult::QueueIsFull );
    BOOST_REQUIRE( tq.import( tx1, IfDropped::Ignore, true, 0 ) == ImportResult::QueueIsFull );
    BOOST_REQUIRE( tq.status().current == 1 );
    BOOST_REQUIRE( tq.status().future == 0 );

    BOOST_REQUIRE( tq.import( tx2, IfDropped::Ignore, true, 0 ) == ImportResult::Success );
    BOOST_REQUIRE( tq.status().current == 1 );
    BOOST_REQUIRE( tq.status().future == 1 );
    BOOST_CHECK( ( Transactions{tx0} ) == tq.topTransactions( 256 ) );
}

BOOST_AUTO_TEST_CASE( tqCurrentGapCanBeFilled ) {
    dev::eth::TransactionQueue tq;

    const u256 gasCostMed = 20 * szabo;
    const u256 gas = 25000;
    Address dest = Address( "0x095e7baea6a6c7c4c2dfeb977efac326af552d87" );
    Secret sender = Secret( "0x3333333333333333333333333333333333333333333333333333333333333333" );
    Transaction tx0( 0, gasCostMed, gas, dest, bytes(), 0, sender );
    Transaction tx1( 0, gasCostMed, gas, dest, bytes(), 1, sender );
    Transaction tx2( 0, gasCostMed, gas, dest, bytes(), 2, sender );
    Transaction tx3( 0, gasCostMed, gas, dest, bytes(), 3, sender );

    BOOST_REQUIRE( tq.import( tx0, IfDropped::Ignore, false, 0 ) == ImportResult::Success );
    BOOST_REQUIRE( tq.import( tx1, IfDropped::Ignore, false, 0 ) == ImportResult::Success );
    BOOST_REQUIRE( tq.import( tx3, IfDropped::Ignore, false, 3 ) == ImportResult::Success );

    BOOST_REQUIRE( tq.import( tx2, IfDropped::Ignore, false, 0 ) == ImportResult::Success );
    BOOST_REQUIRE( tq.status().current == 4 );
    BOOST_REQUIRE( tq.status().future == 0 );
    BOOST_CHECK( ( Transactions{tx0, tx1, tx2, tx3} ) == tq.topTransactions( 256 ) );
}

BOOST_AUTO_TEST_CASE( tqBlockedFuturePromotionRetriesWhenCapacityFrees ) {
    dev::eth::TransactionQueue tq( 1, 3 );

    const u256 gasCostMed = 20 * szabo;
    const u256 gas = 25000;
    Address dest = Address( "0x095e7baea6a6c7c4c2dfeb977efac326af552d87" );
    Secret sender1 = Secret( "0x3333333333333333333333333333333333333333333333333333333333333333" );
    Secret sender2 = Secret( "0x4444444444444444444444444444444444444444444444444444444444444444" );
    Transaction a0( 0, gasCostMed, gas, dest, bytes(), 0, sender1 );
    Transaction a1( 0, gasCostMed, gas, dest, bytes(), 1, sender1 );
    Transaction b0( 0, gasCostMed, gas, dest, bytes(), 0, sender2 );

    BOOST_REQUIRE( tq.import( b0, IfDropped::Ignore, true, 0 ) == ImportResult::Success );
    BOOST_REQUIRE( tq.import( a1, IfDropped::Ignore, true, 0 ) == ImportResult::Success );

    tq.dropGood( a0 );
    BOOST_REQUIRE( tq.status().current == 1 );
    BOOST_REQUIRE( tq.status().future == 1 );
    BOOST_CHECK( ( Transactions{b0} ) == tq.topTransactions( 256 ) );

    tq.drop( b0.sha3() );
    BOOST_REQUIRE( tq.status().current == 1 );
    BOOST_REQUIRE( tq.status().future == 0 );
    BOOST_CHECK( ( Transactions{a1} ) == tq.topTransactions( 256 ) );
}

BOOST_AUTO_TEST_CASE( tqBlockedFuturePromotionInvalidatesWhenPredecessorDrops ) {
    dev::eth::TransactionQueue tq( 1, 3 );

    const u256 gasCostMed = 20 * szabo;
    const u256 gas = 25000;
    Address dest = Address( "0x095e7baea6a6c7c4c2dfeb977efac326af552d87" );
    Secret sender = Secret( "0x3333333333333333333333333333333333333333333333333333333333333333" );
    Transaction tx0( 0, gasCostMed, gas, dest, bytes(), 0, sender );
    Transaction tx1( 0, gasCostMed, gas, dest, bytes(), 1, sender );

    BOOST_REQUIRE( tq.import( tx1, IfDropped::Ignore, true, 0 ) == ImportResult::Success );
    BOOST_REQUIRE( tq.import( tx0, IfDropped::Ignore, false, 0 ) == ImportResult::Success );
    BOOST_REQUIRE( tq.status().current == 1 );
    BOOST_REQUIRE( tq.status().future == 1 );

    tq.drop( tx0.sha3() );
    BOOST_REQUIRE( tq.status().current == 0 );
    BOOST_REQUIRE( tq.status().future == 1 );
    BOOST_CHECK( tq.topTransactions( 256 ).empty() );
}

BOOST_AUTO_TEST_CASE( tqDropGoodRemovesBlockedFutureTransaction ) {
    dev::eth::TransactionQueue tq( 1, 3 );

    const u256 gasCostMed = 20 * szabo;
    const u256 gas = 25000;
    Address dest = Address( "0x095e7baea6a6c7c4c2dfeb977efac326af552d87" );
    Secret sender1 = Secret( "0x3333333333333333333333333333333333333333333333333333333333333333" );
    Secret sender2 = Secret( "0x4444444444444444444444444444444444444444444444444444444444444444" );
    Transaction a1( 0, gasCostMed, gas, dest, bytes(), 1, sender1 );
    Transaction b0( 0, gasCostMed, gas, dest, bytes(), 0, sender2 );

    BOOST_REQUIRE( tq.import( b0, IfDropped::Ignore, true, 0 ) == ImportResult::Success );
    BOOST_REQUIRE( tq.import( a1, IfDropped::Ignore, true, 0 ) == ImportResult::Success );
    BOOST_REQUIRE( tq.import( a1, IfDropped::Ignore, false, 1 ) == ImportResult::QueueIsFull );

    tq.dropGood( a1 );
    BOOST_REQUIRE( tq.status().current == 1 );
    BOOST_REQUIRE( tq.status().future == 0 );
    BOOST_CHECK( ( Transactions{b0} ) == tq.topTransactions( 256 ) );

    tq.drop( b0.sha3() );
    BOOST_REQUIRE( tq.status().current == 0 );
    BOOST_REQUIRE( tq.status().future == 0 );
    BOOST_CHECK( tq.topTransactions( 256 ).empty() );
}

BOOST_AUTO_TEST_CASE( tqImport ) {
    TestTransaction testTransaction = TestTransaction::defaultTransaction();
    TransactionQueue tq;
    h256Hash known = tq.knownTransactions();
    BOOST_REQUIRE( known.size() == 0 );
    
    ImportResult ir = importCurrent( tq, testTransaction.transaction().toBytes() );
    BOOST_REQUIRE( ir == ImportResult::Success );
    known = tq.knownTransactions();
    BOOST_REQUIRE( known.size() == 1 );
    
    ir = importCurrent( tq, testTransaction.transaction().toBytes() );
    BOOST_REQUIRE( ir == ImportResult::AlreadyKnown );

    bytes rlp = testTransaction.transaction().toBytes();
    rlp.at( 0 ) = 03;
    ir = tq.import( rlp, IfDropped::Ignore, false, 0 );
    BOOST_REQUIRE( ir == ImportResult::Malformed );

    known = tq.knownTransactions();
    BOOST_REQUIRE( known.size() == 1 );

    TestTransaction testTransaction2 = TestTransaction::defaultTransaction( 1, 2 );
    TestTransaction testTransaction3 = TestTransaction::defaultTransaction( 1, 1 );
    TestTransaction testTransaction4 = TestTransaction::defaultTransaction( 1, 4 );
    
    ir = importCurrent( tq, testTransaction2.transaction().toBytes() );
    BOOST_REQUIRE( ir == ImportResult::SameNonceAlreadyInQueue );
    
    ir = importCurrent( tq, testTransaction3.transaction().toBytes() );
    BOOST_REQUIRE( ir == ImportResult::AlreadyKnown );
    
    ir = importCurrent( tq, testTransaction4.transaction().toBytes() );
    known = tq.knownTransactions();
    BOOST_REQUIRE( known.size() == 1 );
    Transactions ts = tq.topTransactions( 4 );
    BOOST_REQUIRE( ts.size() == 1 );
    //    BOOST_REQUIRE( Transaction( ts.at( 0 ) ).gasPrice() == 4 );   now we don't overbit gas
    //    price!

    tq.setFuture( Transaction( ts.at( 0 ) ).sha3() );
    Address from = Transaction( ts.at( 0 ) ).from();
    ts = tq.topTransactions( 4 );
    BOOST_REQUIRE( ts.size() == 0 );
    BOOST_REQUIRE( tq.waiting( from ) == 1 );
}

BOOST_AUTO_TEST_CASE( tqImportFuture ) {
    TransactionQueue tq;
    h256Hash known = tq.knownTransactions();
    BOOST_REQUIRE( known.size() == 0 );
    TransactionQueue::Status status = tq.status();
    BOOST_REQUIRE( status.future == 0 );

    TestTransaction tx1 = TestTransaction::defaultTransaction( 4 );
    Address sender = tx1.transaction().sender();
    u256 maxNonce = tq.maxNonce( sender );
    BOOST_REQUIRE( maxNonce == 0 );
    u256 waiting = tq.waiting( sender );
    BOOST_REQUIRE( waiting == 0 );
    
    ImportResult ir1 = tq.import( tx1.transaction().toBytes(), IfDropped::Ignore, true, 0 );
    BOOST_REQUIRE( ir1 == ImportResult::Success );
    known = tq.knownTransactions();
    BOOST_REQUIRE( known.size() == 1 );
    status = tq.status();
    BOOST_REQUIRE( status.future == 1 );
    maxNonce = tq.maxNonce( sender );
    BOOST_REQUIRE( maxNonce == 5 );
    waiting = tq.waiting( sender );
    BOOST_REQUIRE( waiting == 1 );

    // HACK it's now allowed to repeat future transaction
    ir1 = tq.import( tx1.transaction().toBytes(), IfDropped::Ignore, true, 0 );
    BOOST_REQUIRE( ir1 == ImportResult::Success );
    known = tq.knownTransactions();
    BOOST_REQUIRE( known.size() == 1 );
    status = tq.status();
    BOOST_REQUIRE( status.future == 1 );
    maxNonce = tq.maxNonce( sender );
    BOOST_REQUIRE( maxNonce == 5 );
    waiting = tq.waiting( sender );
    BOOST_REQUIRE( waiting == 1 );

    bytes rlp = tx1.transaction().toBytes();
    rlp.at( 0 ) = 03;
    ir1 = tq.import( rlp, IfDropped::Ignore, true, 0 );
    BOOST_REQUIRE( ir1 == ImportResult::Malformed );

    TestTransaction tx2 = TestTransaction::defaultTransaction(2);
    ImportResult ir2 = tq.import( tx2.transaction().toBytes(), IfDropped::Ignore, true, 0 );
    BOOST_REQUIRE( ir2 == ImportResult::Success );
    known = tq.knownTransactions();
    BOOST_REQUIRE( known.size() == 2 );
    maxNonce = tq.maxNonce( sender );
    BOOST_REQUIRE( maxNonce == 5 );
    waiting = tq.waiting( sender );
    BOOST_REQUIRE( waiting == 2 );
    BOOST_CHECK( ( Transactions{} ) == tq.topTransactions( 256 ) );

    TestTransaction tx3 = TestTransaction::defaultTransaction(1);
    ImportResult ir3 = tq.import( tx3.transaction().toBytes(), IfDropped::Ignore, true, 0 );
    BOOST_REQUIRE( ir3 == ImportResult::Success );
    known = tq.knownTransactions();
    BOOST_REQUIRE( known.size() == 3 );
    maxNonce = tq.maxNonce( sender );
    BOOST_REQUIRE( maxNonce == 5 );
    waiting = tq.waiting( sender );
    BOOST_REQUIRE( waiting == 3 );
    BOOST_CHECK( ( Transactions{} ) == tq.topTransactions( 256 ) );

    TestTransaction tx4 = TestTransaction::defaultTransaction(0);
    ImportResult ir4 = importCurrent( tq, tx4.transaction().toBytes() );
    BOOST_REQUIRE( ir4 == ImportResult::Success );
    known = tq.knownTransactions();
    BOOST_REQUIRE( known.size() == 4 );
    maxNonce = tq.maxNonce( sender );
    BOOST_REQUIRE( maxNonce == 5 );
    waiting = tq.waiting( sender );
    BOOST_REQUIRE( waiting == 4 );
    status = tq.status();
    BOOST_REQUIRE( status.future == 1 );
    BOOST_REQUIRE( status.current == 3 );
    BOOST_CHECK( ( Transactions{ tx4.transaction(), tx3.transaction(), tx2.transaction() } ) ==
                 tq.topTransactions( 256 ) );

    TestTransaction tx5 = TestTransaction::defaultTransaction(3);
    ImportResult ir5 = importCurrent( tq, tx5.transaction().toBytes() );
    BOOST_REQUIRE( ir5 == ImportResult::Success );
    known = tq.knownTransactions();
    BOOST_REQUIRE( known.size() == 5 );
    maxNonce = tq.maxNonce( sender );
    BOOST_REQUIRE( maxNonce == 5 );
    waiting = tq.waiting( sender );
    BOOST_REQUIRE( waiting == 5 );
    status = tq.status();
    BOOST_REQUIRE( status.future == 0 );
    BOOST_REQUIRE( status.current == 5 );
    BOOST_CHECK( ( Transactions{ tx4.transaction(), tx3.transaction(), tx2.transaction(),
                     tx5.transaction(), tx1.transaction() } ) == tq.topTransactions( 256 ) );

    const u256 gasCostMed = 20 * szabo;
    const u256 gas = 25000;
    Address dest = Address( "0x095e7baea6a6c7c4c2dfeb977efac326af552d87" );
    Secret sender2 = Secret( "0x4444444444444444444444444444444444444444444444444444444444444444" );
    Transaction tx0( 0, gasCostMed, gas, dest, bytes(), 4, sender2 );
    ImportResult ir0 = tq.import( tx0, IfDropped::Ignore, true, 0 );
    BOOST_REQUIRE( ir0 == ImportResult::Success );
    waiting = tq.waiting( dev::toAddress( sender2 ) );
    BOOST_REQUIRE( waiting == 1 );
    status = tq.status();
    BOOST_REQUIRE( status.future == 1 );
}

BOOST_AUTO_TEST_CASE( dropFromFutureToCurrent ) {
    TransactionQueue tq;

    TestTransaction tx1 = TestTransaction::defaultTransaction( 1 );

    // put transaction to future
    ImportResult ir1 = tq.import( tx1.transaction().toBytes(), IfDropped::Ignore, true, 0 );
    BOOST_REQUIRE( ir1 == ImportResult::Success );
    TransactionQueue::Status status = tq.status();
    BOOST_REQUIRE( status.current == 0 && status.future == 1 );

    // Re-importing while it is still future should not remove it from FTQ.
    ir1 = tq.import( tx1.transaction().toBytes(), IfDropped::Ignore, false, 0 );
    BOOST_REQUIRE( ir1 == ImportResult::Success );
    status = tq.status();
    BOOST_REQUIRE( status.current == 0 && status.future == 1 );

    // push it to current and see it fall back from future to current
    ir1 = tq.import( tx1.transaction().toBytes(), IfDropped::Ignore, false, 1 );
    BOOST_REQUIRE( ir1 == ImportResult::Success );
    status = tq.status();
    BOOST_REQUIRE( status.current == 1 && status.future == 0 );
}

BOOST_AUTO_TEST_CASE( tqImportFutureNonceWithFutureTxsDisabled ) {
    TransactionQueue tq;
    TestTransaction tx1 = TestTransaction::defaultTransaction( 1 );

    ImportResult ir = tq.import( tx1.transaction().toBytes(), IfDropped::Ignore, false, 0 );
    BOOST_REQUIRE( ir == ImportResult::InvalidNonce );

    BOOST_REQUIRE( tq.waiting( tx1.transaction().sender() ) == 0 );
    BOOST_REQUIRE( tq.knownTransactions().empty() );

    TransactionQueue::Status status = tq.status();
    BOOST_REQUIRE( status.current == 0 );
    BOOST_REQUIRE( status.future == 0 );
}

BOOST_AUTO_TEST_CASE( tqImportFutureLimits ) {
    dev::eth::TransactionQueue tq( 1024, 2 );
    TestTransaction tx1 = TestTransaction::defaultTransaction(3);
    tq.import( tx1.transaction().toBytes(), IfDropped::Ignore, true, 0 );

    TestTransaction tx2 = TestTransaction::defaultTransaction(2);
    tq.import( tx2.transaction().toBytes(), IfDropped::Ignore, true, 0 );

    auto waiting = tq.waiting( tx1.transaction().sender() );
    BOOST_REQUIRE( waiting == 2 );
    auto known = tq.knownTransactions();
    BOOST_REQUIRE( known.size() == 2 );

    TestTransaction tx3 = TestTransaction::defaultTransaction(1);
    ImportResult ir = tq.import( tx3.transaction().toBytes(), IfDropped::Ignore, true, 0 );
    BOOST_REQUIRE( ir == ImportResult::QueueIsFull );

    waiting = tq.waiting( tx1.transaction().sender() );
    BOOST_REQUIRE( waiting == 2 );
    known = tq.knownTransactions();
    BOOST_REQUIRE( known.size() == 2 );
    BOOST_CHECK( ( h256Hash{ tx1.transaction().sha3(), tx2.transaction().sha3() } ) == known );
}

BOOST_AUTO_TEST_CASE( tqImportFutureLimits2 ) {
    dev::eth::TransactionQueue tq( 1024, 2 );

    TestTransaction tx1 = TestTransaction::defaultTransaction(3);
    tq.import( tx1.transaction().toBytes(), IfDropped::Ignore, true, 0 );

    auto waiting = tq.waiting( tx1.transaction().sender() );
    BOOST_REQUIRE( waiting == 1 );
    auto known = tq.knownTransactions();
    BOOST_REQUIRE( known.size() == 1 );
    auto status = tq.status();
    BOOST_REQUIRE( status.future == 1 );

    const u256 gasCostMed = 20 * szabo;
    const u256 gas = 25000;
    Address dest = Address( "0x095e7baea6a6c7c4c2dfeb977efac326af552d87" );
    Secret sender2 = Secret( "0x1111111111111111111111111111111111111111111111111111111111111111" );
    Transaction tx0( 0, gasCostMed, gas, dest, bytes(), 4, sender2 );
    ImportResult ir0 = tq.import( tx0, IfDropped::Ignore, true, 0 );
    BOOST_REQUIRE( ir0 == ImportResult::Success );

    waiting = tq.waiting( tx1.transaction().sender() );
    BOOST_REQUIRE( waiting == 1 );
    waiting = tq.waiting(toAddress(sender2));
    BOOST_REQUIRE( waiting == 1 );
    known = tq.knownTransactions();
    BOOST_REQUIRE( known.size() == 2 ); 
    status = tq.status();
    BOOST_REQUIRE( status.future == 2 );  

    TestTransaction tx2 = TestTransaction::defaultTransaction(2);
    ImportResult ir2 = tq.import( tx2.transaction().toBytes(), IfDropped::Ignore, true, 0 );
    BOOST_REQUIRE( ir2 == ImportResult::QueueIsFull );

    waiting = tq.waiting(tx1.transaction().sender());
    BOOST_REQUIRE( waiting == 1 );
    waiting = tq.waiting(toAddress(sender2));
    BOOST_REQUIRE( waiting == 1 );
    known = tq.knownTransactions();
    BOOST_REQUIRE( known.size() == 2 );
    status = tq.status();
    BOOST_REQUIRE( status.future == 2 );  
 
    BOOST_CHECK( ( h256Hash{ tx0.sha3(), tx1.transaction().sha3() } ) == known );
}

BOOST_AUTO_TEST_CASE( tqDrop ) {
    TransactionQueue tq;
    TestTransaction testTransaction = TestTransaction::defaultTransaction();
    tq.dropGood( testTransaction.transaction() );
    importCurrent( tq, testTransaction.transaction().toBytes() );
    BOOST_REQUIRE( tq.topTransactions( 4 ).size() == 1 );
    tq.dropGood( testTransaction.transaction() );
    BOOST_REQUIRE( tq.topTransactions( 4 ).size() == 0 );
}

BOOST_AUTO_TEST_CASE( tqLimit ) {
    TransactionQueue tq( 5, 3 );
    Address from;
    for ( size_t i = 1; i < 7; i++ ) {
        TestTransaction testTransaction = TestTransaction::defaultTransaction( i );
        ImportResult res = importCurrent( tq, testTransaction.transaction() );
        from = testTransaction.transaction().from();
        if ( i <= 5 )
            BOOST_REQUIRE( res == ImportResult::Success );
        else
            BOOST_REQUIRE( res == ImportResult::QueueIsFull );
    }

    // 5 are imported and the 6th is rejected
    BOOST_REQUIRE( tq.waiting( from ) == 5 );

    Transactions topTr = tq.topTransactions( 10 );
    BOOST_REQUIRE( topTr.size() == 5 );

    for ( int i = topTr.size() - 1; i >= 0; i-- )
        tq.setFuture( topTr.at( i ).sha3() );

    topTr = tq.topTransactions( 10 );
    BOOST_REQUIRE( topTr.size() == 0 );

    TestTransaction testTransaction = TestTransaction::defaultTransaction( 7 );
    BOOST_REQUIRE( tq.waiting( from ) == 3 );

    // Drop out of bound feauture
    ImportResult res = importCurrent( tq, testTransaction.transaction() );
    BOOST_REQUIRE( res == ImportResult::Success );

    // future list size is now 3  + 1 imported transaction
    BOOST_REQUIRE( tq.waiting( testTransaction.transaction().from() ) == 4 );

    topTr = tq.topTransactions( 10 );
    BOOST_REQUIRE( topTr.size() == 1 );  // 1 imported transaction
}

BOOST_AUTO_TEST_CASE( tqLimitBytes ) {
    TransactionQueue tq( 100, 100, 250, 250 );

    unsigned maxTxCount =
        250 / TestTransaction::defaultTransaction( 1 ).transaction().toBytes().size();

    TestTransaction testTransaction = TestTransaction::defaultTransaction( 2 );
    ImportResult res = tq.import( testTransaction.transaction(), IfDropped::Ignore, true, 0 );
    BOOST_REQUIRE( res == ImportResult::Success );

    testTransaction = TestTransaction::defaultTransaction( 3 );
    res = tq.import( testTransaction.transaction(), IfDropped::Ignore, true, 0 );
    BOOST_REQUIRE( res == ImportResult::Success );

    BOOST_REQUIRE( tq.status().current == 0 );

    BOOST_REQUIRE( tq.status().future == maxTxCount );

    testTransaction = TestTransaction::defaultTransaction( 4 );
    res = tq.import( testTransaction.transaction(), IfDropped::Ignore, true, 0 );
    BOOST_REQUIRE( res == ImportResult::QueueIsFull );

    BOOST_REQUIRE( tq.status().current == 0 );

    BOOST_REQUIRE( tq.status().future == maxTxCount );

    for ( size_t i = 1; i < 10; i++ ) {
        if ( i == 2 || i == 3 )
            continue;
        testTransaction = TestTransaction::defaultTransaction( i );
        res = importCurrent( tq, testTransaction.transaction() );
        BOOST_REQUIRE( res == ImportResult::Success || res == ImportResult::QueueIsFull );
    }

    BOOST_REQUIRE( tq.status().current == maxTxCount );
    BOOST_REQUIRE( tq.status().future <= maxTxCount );
}

BOOST_AUTO_TEST_CASE( tqEqueue ) {
    TransactionQueue tq;
    TestTransaction testTransaction = TestTransaction::defaultTransaction();

    bytes payloadToDecode = testTransaction.transaction().toBytes();

    RLPStream rlpStream( 2 );
    rlpStream.appendRaw( payloadToDecode );
    rlpStream.appendRaw( payloadToDecode );

    RLP tRlp( rlpStream.out() );

    // check that Transaction could be recreated from the RLP
    Transaction tRlpTransaction( payloadToDecode, CheckTransaction::Cheap );
    BOOST_REQUIRE( tRlpTransaction.data() == testTransaction.transaction().data() );

    // SKALE as we don't have tx verifier threads - we won't check tx import
    //    // try to import transactions
    //    string hashStr =
    //        "010203040506070809101112131415161718192021222324252627282930313201020304050607080910111213"
    //        "14151617181920212223242526272829303132";
    //    tq.enqueue( tRlp, h512( hashStr ) );
    //    tq.enqueue( tRlp, h512( hashStr ) );
    //    std::this_thread::sleep_for( std::chrono::seconds( 1 ) );

    //    // at least 1 transaction should be imported through RLP
    //    Transactions topTr = tq.topTransactions( 10 );
    //    BOOST_REQUIRE( topTr.size() == 1 );
}

BOOST_AUTO_TEST_SUITE_END()
