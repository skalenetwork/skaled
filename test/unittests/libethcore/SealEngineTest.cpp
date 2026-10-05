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
/** @file SealEngineTest.cpp
 * SealEngineFace class testing.
 */

#include <libethashseal/Ethash.h>
#include <libethcore/Exceptions.h>
#include <libethereum/SchainPatch.h>
#include <libethereum/SchainPatchEnum.h>
#include <test/tools/libtesteth/TestHelper.h>
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::test;

namespace {
// Unsigned transaction is what eth_call RPC method uses for execution
class UnsignedTransactionFixture : public TestOutputHelperFixture {
public:
    UnsignedTransactionFixture() {
        params.setExperimentalForkBlock( u256( 0x1000 ) );

        ethash.setChainParams( params );

        header.clear();
        header.setGasLimit( 22000 );
    }

    ChainOperationParams params;
    Ethash ethash;
    BlockHeader header;
    Transaction tx{ 0, 0, 21000, Address( "a94f5374fce5edbc8e2a8697c15331677e6ebf0b" ), bytes(),
        0 };
};

struct PatchableChainParams : public ChainOperationParams {
    void setPatchTimestamp( SchainPatchEnum _patch, time_t _timestamp ) {
        sChain._patchTimestamps[static_cast< size_t >( _patch )] = _timestamp;
    }
};

// London active from timestamp 1 with a base fee far above the gas prices used in the tests.
// Restores the default patch state on exit.
class LondonTransactionFixture : public UnsignedTransactionFixture {
public:
    LondonTransactionFixture() {
        PatchableChainParams cp;
        cp.setPatchTimestamp( SchainPatchEnum::LondonForkPatch, 1 );
        SchainPatch::init( cp );

        header.setNumber( 1 );
        header.setBaseFeePerGas( 1000000 );
    }

    ~LondonTransactionFixture() { SchainPatch::init( PatchableChainParams() ); }
};
}  // namespace

BOOST_FIXTURE_TEST_SUITE( SealEngineTests, TestOutputHelperFixture )
BOOST_FIXTURE_TEST_SUITE( UnsignedTransactionTests, UnsignedTransactionFixture )

BOOST_AUTO_TEST_CASE( UnsignedTransactionIsValidBeforeExperimental,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    // Unsigned transaction (having empty optional for signature)  should be valid otherwise
    // eth_call would fail All transactions coming from the network or RPC methods like
    // sendRawTransaction have their optional<Signature> initialized,
    // so these tests don't cover them

    header.setNumber( 1 );

    SealEngineFace::verifyTransaction( params, ImportRequirements::TransactionSignatures, tx, 1,
        header, 0 );  // check that it doesn't throw
}

BOOST_AUTO_TEST_CASE( UnsignedTransactionIsValidInExperimental ) {
    header.setNumber( 0x1010 );

    SealEngineFace::verifyTransaction( params, ImportRequirements::TransactionSignatures, tx, 1,
        header, 0 );  // check that it doesn't throw
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( LondonBaseFeeTests, LondonTransactionFixture )

// eth_call / eth_estimateGas results must not depend on the gasPrice the caller passes
BOOST_AUTO_TEST_CASE( UnsignedTransactionIgnoresBaseFeeUnderLondon ) {
    BOOST_REQUIRE( tx.gasPrice() < header.baseFeePerGas() );

    SealEngineFace::verifyTransaction( params, ImportRequirements::Everything, tx, 1, header,
        0 );  // check that it doesn't throw
}

BOOST_AUTO_TEST_CASE( SignedTransactionBelowBaseFeeRejectedUnderLondon ) {
    Transaction signedTx( 0, 1, 21000, Address( "a94f5374fce5edbc8e2a8697c15331677e6ebf0b" ),
        bytes(), 0, KeyPair::create().secret() );

    BOOST_CHECK_THROW( SealEngineFace::verifyTransaction(
                           params, ImportRequirements::Everything, signedTx, 1, header, 0 ),
        InvalidTransactionFormat );

    // the same transaction is valid once it pays the base fee
    header.setBaseFeePerGas( signedTx.gasPrice() );
    SealEngineFace::verifyTransaction(
        params, ImportRequirements::Everything, signedTx, 1, header, 0 );
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
