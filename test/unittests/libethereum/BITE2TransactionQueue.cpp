/*
    Modifications Copyright (C) 2019 SKALE Labs

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

#include <libethereum/BITE2TransactionQueue.h>
#include <libethereum/Transaction.h>
#include <libconsensus/bite/Constants.h>
#include <test/tools/libtesteth/TestHelper.h>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::test;

#ifdef BITE2

BOOST_FIXTURE_TEST_SUITE( BITE2TransactionQueueSuite, TestOutputHelperFixture )

BOOST_AUTO_TEST_CASE( addCommitClear ) {
    BITE2TransactionQueue queue;
    
    Secret sec = Secret( "0x45a915e4d060149eb4365960e6a7a45f334393093061116b197e3240065ff2d8" );
    Transaction tx1( 0, 100, 21000, Address(), bytes(), 10, sec );
    Transaction tx2( 1, 100, 21000, Address(), bytes(), 10, sec );
    
    queue.addTemp( std::move( tx1 ) );
    queue.addTemp( std::move( tx2 ) );

    BOOST_REQUIRE_EQUAL( queue.debug_pendingBITE2Transactions().size(), 2 );
    
    queue.commitTemp();
    BOOST_REQUIRE_EQUAL( queue.pendingBITE2Transactions().size(), 2 );

    Transaction tx3( 2, 100, 21000, Address(), bytes(), 10, sec );
    queue.addTemp( std::move( tx3 ) );
    BOOST_REQUIRE_EQUAL( queue.pendingBITE2Transactions().size(), 3 );

    queue.clearTemp();
    BOOST_REQUIRE_EQUAL( queue.pendingBITE2Transactions().size(), 2 );
    
    queue.clear();
    BOOST_REQUIRE_EQUAL( queue.pendingBITE2Transactions().size(), 0 );
}

BOOST_AUTO_TEST_CASE( dropGood ) {
    BITE2TransactionQueue queue;
    Secret sec = Secret( "0x45a915e4d060149eb4365960e6a7a45f334393093061116b197e3240065ff2d8" );

    bytes ctxData;
    ctxData.insert( ctxData.end(), std::begin(BITE2_FUNCTION_SELECTOR_AS_BYTE_ARRAY), std::end(BITE2_FUNCTION_SELECTOR_AS_BYTE_ARRAY ) );

    Transaction txCtx( 0, 100, 21000, Address(), ctxData, 0, sec );
    txCtx.checkIfCTXAndSet( ctxData );
    BOOST_REQUIRE( txCtx.isCTX() );

    Transaction txNormal( 0, 100, 21000, Address(), bytes(), 0, sec );
    txNormal.checkIfCTXAndSet( bytes() );
    BOOST_REQUIRE( !txNormal.isCTX() );

    queue.addTemp( Transaction( txCtx ) );
    queue.commitTemp();
    
    queue.finalize();

    BOOST_REQUIRE( queue.dropGood( txCtx ) );
    
    queue.clear();
    queue.addTemp( Transaction( txCtx ) );
    queue.addTemp( Transaction( txCtx ) );
    queue.commitTemp();
    queue.finalize();

    BOOST_REQUIRE( queue.dropGood( txCtx ) );
    BOOST_REQUIRE( queue.dropGood( txCtx ) );

    queue.clear();
    queue.addTemp( Transaction( txNormal ) );
    queue.commitTemp();
    queue.finalize();

    BOOST_REQUIRE( !queue.dropGood( txNormal ) );
}

BOOST_AUTO_TEST_SUITE_END()

#endif
