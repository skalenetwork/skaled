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
/** @file ParisForkTests.cpp
 * @date 2026
 * Paris fork (EIP-3675 + EIP-4399) unit tests.
 * Tests verify that SealEngine::verify skips the minimumDifficulty check when
 * ParisForkPatch is active, and enforces it when the patch is not active.
 */

#include <boost/test/unit_test.hpp>
#include <libethashseal/GenesisInfo.h>
#include <libethcore/Exceptions.h>
#include <libethcore/SealEngine.h>
#include <libethereum/ChainParams.h>
#include <libethereum/SchainPatch.h>
#include <test/tools/libtesteth/TestOutputHelper.h>

using namespace dev;
using namespace eth;

namespace {

struct PatchableChainParams : public ChainParams {
    using ChainParams::ChainParams;
    void setPatchTimestamp( SchainPatchEnum _patch, time_t _timestamp ) {
        sChain._patchTimestamps[static_cast< size_t >( _patch )] = _timestamp;
    }
};

}  // namespace

BOOST_FIXTURE_TEST_SUITE( ParisForkTests, dev::test::TestOutputHelperFixture )

// EIP-3675: when ParisForkPatch is active, difficulty=0 must pass SealEngine::verify.
BOOST_AUTO_TEST_CASE( parisForkDifficultyZeroPasses ) {
    PatchableChainParams cp( genesisInfo( Network::ConstantinopleTest ) );
#ifndef FAIR
    cp.setPatchTimestamp( SchainPatchEnum::ParisForkPatch, 1 );
#endif
    SchainPatch::init( cp );
    SchainPatch::useLatestBlockTimestamp( 1 );
    std::unique_ptr< SealEngineFace > se( cp.createSealEngine() );

    BlockHeader bi;
    bi.setGasLimit( 0x7fffffffffffffff );
    bi.setGasUsed( 0 );
    bi.setDifficulty( 0 );
    bi.setTimestamp( 1 );

    BOOST_REQUIRE_NO_THROW( se->verify( QuickNonce, bi, BlockHeader{}, bytesConstRef{} ) );

    ChainParams resetCp( genesisInfo( Network::ConstantinopleTest ) );
    SchainPatch::init( resetCp );
    SchainPatch::useLatestBlockTimestamp( 0 );
}

// EIP-3675: without ParisForkPatch, difficulty=0 < minimumDifficulty(131072) must throw.
BOOST_AUTO_TEST_CASE( parisForkDifficultyZeroThrowsPreParis ) {
    ChainParams cp( genesisInfo( Network::ConstantinopleTest ) );
    SchainPatch::init( cp );
    SchainPatch::useLatestBlockTimestamp( 0 );
    std::unique_ptr< SealEngineFace > se( cp.createSealEngine() );

    BlockHeader bi;
    bi.setGasLimit( 0x7fffffffffffffff );
    bi.setGasUsed( 0 );
    bi.setDifficulty( 0 );
    bi.setTimestamp( 1 );

#ifndef FAIR
    BOOST_REQUIRE_THROW(
        se->verify( QuickNonce, bi, BlockHeader{}, bytesConstRef{} ), InvalidDifficulty );
#endif
}

// EIP-3675: post-Paris blocks must have difficulty=0; non-zero difficulty changes the block hash.
BOOST_AUTO_TEST_CASE( parisForkNonZeroDifficultyThrowsPostParis ) {
    PatchableChainParams cp( genesisInfo( Network::ConstantinopleTest ) );
#ifndef FAIR
    cp.setPatchTimestamp( SchainPatchEnum::ParisForkPatch, 1 );
#endif
    SchainPatch::init( cp );
    SchainPatch::useLatestBlockTimestamp( 1 );
    std::unique_ptr< SealEngineFace > se( cp.createSealEngine() );

    BlockHeader parent;
    parent.setGasLimit( 0x7fffffffffffffff );
    parent.setGasUsed( 0 );
    parent.setDifficulty( 0 );
    parent.setTimestamp( 1 );

    BlockHeader bi;
    bi.setParentHash( parent.hash() );
    bi.setNumber( 1 );
    bi.setGasLimit( 0x7fffffffffffffff );
    bi.setGasUsed( 0 );
    bi.setDifficulty( 42 );
    bi.setTimestamp( 2 );

    BOOST_REQUIRE_THROW( se->verify( QuickNonce, bi, parent, bytesConstRef{} ), InvalidDifficulty );

    ChainParams resetCp( genesisInfo( Network::ConstantinopleTest ) );
    SchainPatch::init( resetCp );
    SchainPatch::useLatestBlockTimestamp( 0 );
}

// EIP-4399 reuses the Ethash mixHash position for prevRandao and requires a zero nonce.
BOOST_AUTO_TEST_CASE( parisForkPrevRandaoAndNonceValidation ) {
    PatchableChainParams cp( genesisInfo( Network::ConstantinopleTest ) );
#ifndef FAIR
    cp.setPatchTimestamp( SchainPatchEnum::ParisForkPatch, 1 );
#endif
    SchainPatch::init( cp );
    SchainPatch::useLatestBlockTimestamp( 1 );
    std::unique_ptr< SealEngineFace > se( cp.createSealEngine() );

    BlockHeader parent;
    parent.setGasLimit( 0x7fffffffffffffff );
    parent.setGasUsed( 0 );
    parent.setDifficulty( 0 );
    parent.setTimestamp( 1 );

    BlockHeader bi;
    bi.setParentHash( parent.hash() );
    bi.setNumber( 1 );
    bi.setGasLimit( 0x7fffffffffffffff );
    bi.setGasUsed( 0 );
    bi.setDifficulty( 0 );
    bi.setTimestamp( 2 );
    bi.setSeal( 0, h256( 0 ) );
    bi.setSeal( 1, Nonce( 0 ) );

    BOOST_REQUIRE_NO_THROW( se->verify( QuickNonce, bi, parent, bytesConstRef{} ) );

    bi.setPrevRandao( h256( 1 ) );
    BOOST_REQUIRE_THROW( se->verify( QuickNonce, bi, parent, bytesConstRef{} ), InvalidBlockFormat );

    bi.setPrevRandao( h256( 0 ) );
    bi.setSeal( 1, Nonce( 1 ) );
    BOOST_REQUIRE_THROW( se->verify( QuickNonce, bi, parent, bytesConstRef{} ), InvalidBlockFormat );

    ChainParams resetCp( genesisInfo( Network::ConstantinopleTest ) );
    SchainPatch::init( resetCp );
    SchainPatch::useLatestBlockTimestamp( 0 );
}

BOOST_AUTO_TEST_CASE( parisForkMissingPrevRandaoAndNonceThrows ) {
    PatchableChainParams cp( genesisInfo( Network::ConstantinopleTest ) );
#ifndef FAIR
    cp.setPatchTimestamp( SchainPatchEnum::ParisForkPatch, 1 );
#endif
    SchainPatch::init( cp );
    SchainPatch::useLatestBlockTimestamp( 1 );
    std::unique_ptr< SealEngineFace > se( cp.createSealEngine() );

    BlockHeader parent;
    parent.setGasLimit( 0x7fffffffffffffff );
    parent.setGasUsed( 0 );
    parent.setDifficulty( 0 );
    parent.setTimestamp( 1 );

    BlockHeader bi;
    bi.setParentHash( parent.hash() );
    bi.setNumber( 1 );
    bi.setGasLimit( 0x7fffffffffffffff );
    bi.setGasUsed( 0 );
    bi.setDifficulty( 0 );
    bi.setTimestamp( 2 );

    BOOST_REQUIRE_THROW( se->verify( QuickNonce, bi, parent, bytesConstRef{} ), InvalidBlockFormat );

    ChainParams resetCp( genesisInfo( Network::ConstantinopleTest ) );
    SchainPatch::init( resetCp );
    SchainPatch::useLatestBlockTimestamp( 0 );
}

BOOST_AUTO_TEST_CASE( parisForkPopulateAddsPrevRandaoAndNonce ) {
    PatchableChainParams cp( genesisInfo( Network::ConstantinopleTest ) );
#ifndef FAIR
    cp.setPatchTimestamp( SchainPatchEnum::ParisForkPatch, 1 );
#endif
    SchainPatch::init( cp );
    SchainPatch::useLatestBlockTimestamp( 1 );
    std::unique_ptr< SealEngineFace > se( cp.createSealEngine() );

    BlockHeader parent;
    parent.setNumber( 1 );
    parent.setGasLimit( 0x7fffffffffffffff );
    parent.setGasUsed( 0 );
    parent.setDifficulty( 0 );
    parent.setTimestamp( 1 );
    parent.hash();

    BlockHeader bi;
    se->populateFromParent( bi, parent );
    bi.setTimestamp( 2 );

    BOOST_REQUIRE_EQUAL( bi.difficulty(), 0 );
    BOOST_REQUIRE_EQUAL( bi.sealFieldCount(), 2 );
    BOOST_REQUIRE_EQUAL( bi.prevRandao(), h256( 0 ) );
    BOOST_REQUIRE_EQUAL( bi.seal< Nonce >( 1 ), Nonce( 0 ) );

    ChainParams resetCp( genesisInfo( Network::ConstantinopleTest ) );
    SchainPatch::init( resetCp );
    SchainPatch::useLatestBlockTimestamp( 0 );
}

BOOST_AUTO_TEST_SUITE_END()
