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

BOOST_AUTO_TEST_SUITE_END()
