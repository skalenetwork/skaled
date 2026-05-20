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
/** @file BlockHeaderRLPTest.cpp
 * @date 2026
 * Tests for London fork block header RLP field ordering.
 */

#include <libethcore/BlockHeader.h>
#include <libethcore/ChainOperationParams.h>
#include <libethereum/SchainPatch.h>
#include <libethereum/SchainPatchEnum.h>
#include <test/tools/libtesteth/TestHelper.h>
#include <boost/test/unit_test.hpp>

using namespace dev;
using namespace dev::eth;
using namespace dev::test;

namespace {

struct PatchableChainParams : public ChainOperationParams {
    void setPatchTimestamp( SchainPatchEnum _patch, time_t _timestamp ) {
        sChain._patchTimestamps[static_cast< size_t >( _patch )] = _timestamp;
    }
};

struct SchainPatchGuard {
    ~SchainPatchGuard() {
        PatchableChainParams cp;
        SchainPatch::init( cp );
    }
};

BlockHeader makeHeader( time_t _timestamp, u256 _baseFee ) {
    BlockHeader header;
    header.setParentHash( h256( 1 ) );
    header.setAuthor( Address( 2 ) );
    header.setNumber( 42 );
    header.setGasLimit( 1000000 );
    header.setGasUsed( 21000 );
    header.setTimestamp( _timestamp );
    header.setBaseFeePerGas( _baseFee );
    return header;
}

void enableLondonAtTimestampOne() {
    PatchableChainParams cp;
    cp.setPatchTimestamp( SchainPatchEnum::LondonForkPatch, 1 );
    SchainPatch::init( cp );
}

}  // namespace

BOOST_FIXTURE_TEST_SUITE( BlockHeaderRLPTests, TestOutputHelperFixture )

#ifndef FAIR
// Under FAIR, LondonForkPatch is unconditionally pre-enabled (see preEnabledForFAIR), so a
// "pre-London" header configuration cannot be produced in this build.
BOOST_AUTO_TEST_CASE( preLondonHeaderHasNoBaseFee ) {
    SchainPatchGuard guard;
    PatchableChainParams cp;
    SchainPatch::init( cp );

    BlockHeader header = makeHeader( 100, 7 );

    RLPStream stream;
    header.streamRLP( stream, WithSeal );

    BOOST_REQUIRE_EQUAL( RLP( stream.out() ).itemCount(), unsigned{ BlockHeader::BasicFields } );
}
#endif

BOOST_AUTO_TEST_CASE( londonHeaderBaseFeeIsLastWithoutSealFields ) {
    SchainPatchGuard guard;
    enableLondonAtTimestampOne();

    const u256 expectedBaseFee = 5;
    BlockHeader header = makeHeader( 100, expectedBaseFee );

    RLPStream stream;
    header.streamRLP( stream, WithSeal );
    RLP rlp( stream.out() );

    BOOST_REQUIRE_EQUAL( rlp.itemCount(), unsigned{ BlockHeader::BasicFields + 1 } );
    BOOST_REQUIRE_EQUAL( rlp[BlockHeader::BasicFields].toInt< u256 >(), expectedBaseFee );
}

BOOST_AUTO_TEST_CASE( londonHeaderWithSealFieldsOrdersBaseFeeAfterSeal ) {
    SchainPatchGuard guard;
    enableLondonAtTimestampOne();

    const h256 expectedMixHash( 0xdead );
    const Nonce expectedNonce( 0x1234 );
    const u256 expectedBaseFee = 42;

    BlockHeader header = makeHeader( 100, expectedBaseFee );
    header.setSeal( 0, expectedMixHash );
    header.setSeal( 1, expectedNonce );

    RLPStream stream;
    header.streamRLP( stream, WithSeal );
    RLP rlp( stream.out() );

    BOOST_REQUIRE_EQUAL( rlp.itemCount(), 16u );
    BOOST_REQUIRE_EQUAL( rlp[13].toHash< h256 >( RLP::VeryStrict ), expectedMixHash );
    BOOST_REQUIRE_EQUAL( rlp[14].toHash< Nonce >( RLP::VeryStrict ), expectedNonce );
    BOOST_REQUIRE_EQUAL( rlp[15].toInt< u256 >(), expectedBaseFee );
}

BOOST_AUTO_TEST_CASE( londonHeaderWithSealFieldsRoundTrips ) {
    SchainPatchGuard guard;
    enableLondonAtTimestampOne();

    const h256 expectedMixHash( 0xbeef );
    const Nonce expectedNonce( 0x5678 );
    const u256 expectedBaseFee = 777;

    BlockHeader original = makeHeader( 50, expectedBaseFee );
    original.setSeal( 0, expectedMixHash );
    original.setSeal( 1, expectedNonce );
    const h256 originalHash = original.hash( WithSeal );

    RLPStream stream;
    original.streamRLP( stream, WithSeal );

    BlockHeader restored( stream.out(), HeaderData );
    BOOST_REQUIRE_EQUAL( restored.baseFeePerGas(), expectedBaseFee );
    BOOST_REQUIRE_EQUAL( restored.seal< h256 >( 0 ), expectedMixHash );
    BOOST_REQUIRE_EQUAL( restored.seal< Nonce >( 1 ), expectedNonce );
    BOOST_REQUIRE_EQUAL( restored.hash( WithSeal ), originalHash );
}

BOOST_AUTO_TEST_CASE( londonHeaderWithoutSealStillIncludesBaseFee ) {
    SchainPatchGuard guard;
    enableLondonAtTimestampOne();

    const u256 expectedBaseFee = 3;
    BlockHeader header = makeHeader( 100, expectedBaseFee );
    header.setSeal( 0, h256( 0xaaaa ) );
    header.setSeal( 1, Nonce( 0xbbbb ) );

    RLPStream stream;
    header.streamRLP( stream, WithoutSeal );
    RLP rlp( stream.out() );

    BOOST_REQUIRE_EQUAL( rlp.itemCount(), unsigned{ BlockHeader::BasicFields + 1 } );
    BOOST_REQUIRE_EQUAL( rlp[BlockHeader::BasicFields].toInt< u256 >(), expectedBaseFee );
}

// P1#2 (NoProof shape accepted): SKALE's NoProof seal engine has 0 seal fields, so the normal
// SKALE London header is 14 items (13 basic + 1 baseFee). populate() must accept it and read
// the trailing field as baseFeePerGas.
BOOST_AUTO_TEST_CASE( nonGenesisLondonHeaderNoProofShapeAccepted ) {
    SchainPatchGuard guard;
    enableLondonAtTimestampOne();

    const u256 expectedBaseFee = 7;
    RLPStream stream;
    stream.appendList( BlockHeader::BasicFields + 1 );  // 13 basic + 1 baseFee (no seal)
    stream << h256( 1 )                  // parentHash
           << EmptyListSHA3              // sha3Uncles
           << Address( 2 )               // author
           << EmptyTrie                  // stateRoot
           << EmptyTrie                  // transactionsRoot
           << EmptyTrie                  // receiptsRoot
           << LogBloom()                 // logBloom
           << u256( 0 )                  // difficulty
           << u256( 42 )                 // number (non-genesis)
           << u256( 1000000 )            // gasLimit
           << u256( 21000 )              // gasUsed
           << u256( 100 )                // timestamp (London-active)
           << bytes()                    // extraData
           << expectedBaseFee;           // baseFeePerGas (NoProof: no seal precedes it)

    BlockHeader restored( stream.out(), HeaderData );
    BOOST_REQUIRE_EQUAL( restored.baseFeePerGas(), expectedBaseFee );
    BOOST_REQUIRE_EQUAL( restored.seal< h256 >( 0 ), h256() );  // no seal fields
}

// P1#2 (sealed variant): a non-genesis London header with seal fields but no trailing baseFee
// (15 items = 13 basic + 2 seal) must be rejected. Without the strict count check the parser
// would have silently consumed the nonce as baseFeePerGas.
BOOST_AUTO_TEST_CASE( nonGenesisLondonHeaderSealedMissingBaseFeeIsRejected ) {
    SchainPatchGuard guard;
    enableLondonAtTimestampOne();

    RLPStream stream;
    stream.appendList( BlockHeader::BasicFields + 2 );  // 13 basic + 2 seal, NO baseFee
    stream << h256( 1 )                  // parentHash
           << EmptyListSHA3              // sha3Uncles
           << Address( 2 )               // author
           << EmptyTrie                  // stateRoot
           << EmptyTrie                  // transactionsRoot
           << EmptyTrie                  // receiptsRoot
           << LogBloom()                 // logBloom
           << u256( 0 )                  // difficulty
           << u256( 42 )                 // number (non-genesis)
           << u256( 1000000 )            // gasLimit
           << u256( 21000 )              // gasUsed
           << u256( 100 )                // timestamp (London-active)
           << bytes();                   // extraData
    // Two "seal" fields with no baseFee tail — last field below would be misread as baseFee.
    stream.append( h256( 0xdead ) );
    stream.append( Nonce( 0xbeef ) );

    BOOST_REQUIRE_THROW( BlockHeader( stream.out(), HeaderData ), Exception );
}

// P1#2: a non-genesis London header written WITHOUT baseFeePerGas must be rejected when parsed,
// not silently treated as baseFee=0. This guards against malformed/stripped headers leaking into
// import paths.
BOOST_AUTO_TEST_CASE( nonGenesisLondonHeaderMissingBaseFeeIsRejected ) {
    SchainPatchGuard guard;
    enableLondonAtTimestampOne();

    // Manually build a non-genesis (number=42) header RLP with the 13 basic fields and NO
    // trailing baseFeePerGas. Build the RLP by hand using the same field order as
    // BlockHeader::streamRLPFields so populate() reaches the strict baseFee check.
    RLPStream stream;
    stream.appendList( BlockHeader::BasicFields );
    stream << h256( 1 )                  // parentHash
           << EmptyListSHA3              // sha3Uncles
           << Address( 2 )               // author
           << EmptyTrie                  // stateRoot
           << EmptyTrie                  // transactionsRoot
           << EmptyTrie                  // receiptsRoot
           << LogBloom()                 // logBloom
           << u256( 0 )                  // difficulty
           << u256( 42 )                 // number (non-genesis)
           << u256( 1000000 )            // gasLimit
           << u256( 21000 )              // gasUsed
           << u256( 100 )                // timestamp (London-active)
           << bytes();                   // extraData

    BOOST_REQUIRE_THROW( BlockHeader( stream.out(), HeaderData ), Exception );
}

// P1#3: genesis (block 0) must NOT carry baseFeePerGas in its RLP, even if its timestamp falls
// inside the London-active range. Parser must accept the 13-field form for genesis.
BOOST_AUTO_TEST_CASE( genesisHeaderHasNoBaseFeeUnderLondon ) {
    SchainPatchGuard guard;
    enableLondonAtTimestampOne();

    BlockHeader genesis;
    genesis.setParentHash( h256() );
    genesis.setAuthor( Address() );
    genesis.setNumber( 0 );
    genesis.setGasLimit( 1000000 );
    genesis.setGasUsed( 0 );
    genesis.setTimestamp( 100 );  // London-active per enableLondonAtTimestampOne()
    genesis.setBaseFeePerGas( 0 );

    RLPStream stream;
    genesis.streamRLP( stream, WithoutSeal );

    // Exactly 13 fields — no baseFeePerGas tail for genesis.
    BOOST_REQUIRE_EQUAL( RLP( stream.out() ).itemCount(), unsigned{ BlockHeader::BasicFields } );

    // Parser must accept the genesis-shaped RLP and not throw.
    BOOST_REQUIRE_NO_THROW( BlockHeader( stream.out(), HeaderData ) );
    BlockHeader restored( stream.out(), HeaderData );
    BOOST_REQUIRE_EQUAL( restored.number(), 0 );
    BOOST_REQUIRE_EQUAL( restored.baseFeePerGas(), u256( 0 ) );
}

#ifndef FAIR
// P1#3 (round-trip): genesis hash with London-active timestamp must be stable: the no-baseFee
// serialized form is what gets hashed and what populate() reads back.
// Skipped under FAIR — LondonForkPatch is preEnabled there, so the "pre-London" branch in this
// test cannot be observed in that build.
BOOST_AUTO_TEST_CASE( genesisHashIsStableUnderLondon ) {
    SchainPatchGuard guard;
    enableLondonAtTimestampOne();

    BlockHeader genesis;
    genesis.setParentHash( h256() );
    genesis.setAuthor( Address() );
    genesis.setNumber( 0 );
    genesis.setGasLimit( 1000000 );
    genesis.setGasUsed( 0 );
    genesis.setTimestamp( 100 );
    genesis.setBaseFeePerGas( 0 );

    h256 hashLondon = genesis.hash( WithoutSeal );

    // Now do the same thing pre-London — hash must match. Genesis must not depend on London
    // activation for its on-chain identity.
    {
        SchainPatchGuard innerGuard;
        PatchableChainParams cp;
        SchainPatch::init( cp );
        BlockHeader preLondon;
        preLondon.setParentHash( h256() );
        preLondon.setAuthor( Address() );
        preLondon.setNumber( 0 );
        preLondon.setGasLimit( 1000000 );
        preLondon.setGasUsed( 0 );
        preLondon.setTimestamp( 100 );
        preLondon.setBaseFeePerGas( 0 );
        BOOST_REQUIRE_EQUAL( preLondon.hash( WithoutSeal ), hashLondon );
    }
    // Re-enable London for subsequent tests in this suite.
    enableLondonAtTimestampOne();
}
#endif  // !FAIR

BOOST_AUTO_TEST_SUITE_END()
