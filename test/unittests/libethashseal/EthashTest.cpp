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
/** @file
 * Ethash class testing.
 */

#include <libethashseal/Ethash.h>

#include <test/tools/libtesteth/TestOutputHelper.h>

#include <ethash/ethash.hpp>

#include <boost/test/unit_test.hpp>

using namespace dev;
using namespace dev::eth;
using namespace dev::test;

BOOST_FIXTURE_TEST_SUITE( EthashTests, TestOutputHelperFixture )

// FIXME: Add a helper function here, because the test cases are almost identical.
// TODO: Add tests for Homestead difficulty change.

BOOST_AUTO_TEST_CASE( calculateDifficultyByzantiumWithoutUncles ) {
    ChainOperationParams params;
    params.homesteadForkBlock = 0;
    params.byzantiumForkBlock = u256( 0x1000 );

    Ethash ethash;
    ethash.setChainParams( params );

    BlockHeader parentHeader;
    parentHeader.clear();
    parentHeader.setNumber( 0x2000 );
    parentHeader.setTimestamp( 100 );
    parentHeader.setDifficulty( 1000000 );

    BlockHeader header;
    header.clear();
    header.setNumber( 0x2001 );
    header.setTimestamp( 130 );

    BOOST_CHECK_EQUAL( ethash.calculateDifficulty( header, parentHeader ), 999024 );
}

BOOST_AUTO_TEST_CASE( calculateDifficultyByzantiumWithUncles ) {
    ChainOperationParams params;
    params.homesteadForkBlock = 0;
    params.byzantiumForkBlock = u256( 0x1000 );

    Ethash ethash;
    ethash.setChainParams( params );

    BlockHeader parentHeader;
    parentHeader.clear();
    parentHeader.setNumber( 0x2000 );
    parentHeader.setTimestamp( 100 );
    parentHeader.setDifficulty( 1000000 );
    parentHeader.setSha3Uncles(
        h256( "0x949d991d685738352398dff73219ab19c62c06e6f8ce899fbae755d5127ed1ef" ) );

    BlockHeader header;
    header.clear();
    header.setNumber( 0x2001 );
    header.setTimestamp( 130 );

    BOOST_CHECK_EQUAL( ethash.calculateDifficulty( header, parentHeader ), 999512 );
}

BOOST_AUTO_TEST_CASE( calculateDifficultyByzantiumMaxAdjustment ) {
    ChainOperationParams params;
    params.homesteadForkBlock = 0;
    params.byzantiumForkBlock = u256( 0x1000 );

    Ethash ethash;
    ethash.setChainParams( params );

    BlockHeader parentHeader;
    parentHeader.clear();
    parentHeader.setNumber( 0x2000 );
    parentHeader.setTimestamp( 100 );
    parentHeader.setDifficulty( 1000000 );

    BlockHeader header;
    header.clear();
    header.setNumber( 0x2001 );
    header.setTimestamp( 1100 );

    BOOST_CHECK_EQUAL( ethash.calculateDifficulty( header, parentHeader ), 951688 );
}

BOOST_AUTO_TEST_CASE( epochSeed ) {
    BlockHeader header;
    header.setNumber( 0 );
    h256 seed = Ethash::seedHash( header );
    BOOST_CHECK_EQUAL( seed, h256{} );

    header.setNumber( 30000 );
    seed = Ethash::seedHash( header );
    BOOST_CHECK_EQUAL(
        seed, h256{"290decd9548b62a8d60345a988386fc84ba6bc95484008f6362f93160ef3e563"} );

    header.setNumber( 2048 * 30000 );
    seed = Ethash::seedHash( header );
    BOOST_CHECK_EQUAL(
        seed, h256{"20a7678ca7b50829183baac2e1e3c43fa3c4bcbc171b11cf5a9f30bebd172920"} );
}

namespace {
struct EthashTestCase {
    char const* nonce;
    char const* mixHash;
    char const* header;
    char const* seed;
    char const* result;
    int cacheSize;
    int fullSize;
    char const* headerHash;
    char const* cacheHash;
};

EthashTestCase ethashTestCases[] = {
    {
        "4242424242424242",
        "58f759ede17a706c93f13030328bcea40c1d1341fb26f2facd21ceb0dae57017",
        "f901f3a00000000000000000000000000000000000000000000000000000000000000000a01dcc4de8dec75d7a"
        "ab85b567b6ccd41ad312451b948a7413f0a142fd40d49347940000000000000000000000000000000000000000"
        "a09178d0f23c965d81f0834a4c72c6253ce6830f4022b1359aaebfc1ecba442d4ea056e81f171bcc55a6ff8345"
        "e692c0f86e5b48e01b996cadc001622fb5e363b421a056e81f171bcc55a6ff8345e692c0f86e5b48e01b996cad"
        "c001622fb5e363b421b90100000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000008302"
        "000080830f4240808080a058f759ede17a706c93f13030328bcea40c1d1341fb26f2facd21ceb0dae570178842"
        "42424242424242",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "dd47fd2d98db51078356852d7c4014e6a5d6c387c35f40e2875b74a256ed7906",
        16776896,
        1073739904,
        "2a8de2adf89af77358250bf908bf04ba94a6e8c3ba87775564a41d269a05e4ce",
        "35ded12eecf2ce2e8da2e15c06d463aae9b84cb2530a00b932e4bbc484cde353",
    },
    {
        "307692cf71b12f6d",
        "e55d02c555a7969361cf74a9ec6211d8c14e4517930a00442f171bdb1698d175",
        "f901f7a01bef91439a3e070a6586851c11e6fd79bbbea074b2b836727b8e75c7d4a6b698a01dcc4de8dec75d7a"
        "ab85b567b6ccd41ad312451b948a7413f0a142fd40d4934794ea3cb5f94fa2ddd52ec6dd6eb75cf824f4058ca1"
        "a00c6e51346be0670ce63ac5f05324e27d20b180146269c5aab844d09a2b108c64a056e81f171bcc55a6ff8345"
        "e692c0f86e5b48e01b996cadc001622fb5e363b421a056e81f171bcc55a6ff8345e692c0f86e5b48e01b996cad"
        "c001622fb5e363b421b90100000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000008302"
        "004002832fefd880845511ed2a80a0e55d02c555a7969361cf74a9ec6211d8c14e4517930a00442f171bdb1698"
        "d17588307692cf71b12f6d",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "ab9b13423cface72cbec8424221651bc2e384ef0f7a560e038fc68c8d8684829",
        16776896,
        1073739904,
        "100cbec5e5ef82991290d0d93d758f19082e71f234cf479192a8b94df6da6bfe",
        "35ded12eecf2ce2e8da2e15c06d463aae9b84cb2530a00b932e4bbc484cde353",
    },
};
}  // namespace

BOOST_AUTO_TEST_CASE( ethashEvalHeader ) {
    // FIXME: Drop this test as ethash library has this test cases in its test suite.

    for ( auto& t : ethashTestCases ) {
        BlockHeader header{fromHex( t.header ), HeaderData};
        h256 headerHash{t.headerHash};
        eth::Nonce nonce{t.nonce};
        BOOST_REQUIRE_EQUAL( headerHash, header.hash( WithoutSeal ) );
        BOOST_REQUIRE_EQUAL( nonce, Ethash::nonce( header ) );

        ethash::result result = ethash::hash(
            ethash::get_global_epoch_context( ethash::get_epoch_number( header.number() ) ),
            ethash::hash256_from_bytes( header.hash( WithoutSeal ).data() ),
            ( uint64_t )( u64 ) nonce );

        h256 mix{result.mix_hash.bytes, h256::ConstructFromPointer};
        h256 final{result.final_hash.bytes, h256::ConstructFromPointer};

        BOOST_REQUIRE_EQUAL( final, h256{t.result} );
        BOOST_REQUIRE_EQUAL( mix, Ethash::mixHash( header ) );
    }
}

BOOST_AUTO_TEST_SUITE_END()
