/*
    Copyright (C) 2026-present, SKALE Labs

    This file is part of skaled.

    skaled is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skaled is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skaled.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file DecryptedTxFieldsPatchTest.cpp
 * Tests for libethereum/patches/DecryptedTxFieldsPatch: both behaviours and the
 * timestamp window that selects between them.
 */

#ifdef BITE

#include <libethashseal/GenesisInfo.h>
#include <libethereum/ChainParams.h>
#include <libethereum/SchainPatch.h>
#include <libethereum/patches/DecryptedTxFieldsPatch.h>
#include <test/tools/libtesteth/TestOutputHelper.h>
#include <boost/test/unit_test.hpp>

#include <memory>
#include <string>

using namespace dev;
using namespace dev::eth;
using namespace dev::eth::patches;

namespace {

// Builds a block layout from a string, one character per transaction:
//   'P' plain transaction (no map entry)
//   'B' BITE transaction with decrypted fields
//   'F' BITE transaction whose decryption failed (map entry without a value)
// The transactions start at block index _firstTxIndex (the CTX prefix size).
DecryptedRegularTxsMap makeMap( const std::string& _layout, size_t _firstTxIndex = 0 ) {
    DecryptedRegularTxsMap map;
    for ( size_t k = 0; k < _layout.size(); ++k ) {
        if ( _layout[k] == 'B' )
            map.emplace( _firstTxIndex + k, DecryptedRegularTxFields{ { uint8_t( k ) }, {} } );
        else if ( _layout[k] == 'F' )
            map.emplace( _firstTxIndex + k, std::nullopt );
    }
    return map;
}

// Looks up each transaction in [_firstTxIndex, _endTxIndex) the way SkaleHost does and
// returns one character per transaction: '1' if it gets decrypted fields, '0' if not.
// Also checks that fields, when present, are the ones stored under the transaction's own index.
std::string applied( const std::shared_ptr< const DecryptedRegularTxsMap >& _fields,
    const DecryptedRegularTxsMap& _map, size_t _firstTxIndex, size_t _endTxIndex ) {
    BOOST_REQUIRE( _fields );
    std::string result;
    for ( size_t i = _firstTxIndex; i < _endTxIndex; ++i ) {
        auto entry = _fields->find( i );
        bool hasFields = entry != _fields->end() && entry->second.has_value();
        if ( hasFields ) {
            BOOST_REQUIRE( entry->second->data == _map.at( i ).value().data );
        }
        result += hasFields ? '1' : '0';
    }
    return result;
}

std::string current( const std::string& _layout, size_t _firstTxIndex = 0 ) {
    auto map =
        std::make_shared< const DecryptedRegularTxsMap >( makeMap( _layout, _firstTxIndex ) );
    return applied( decryptedFields( map ), *map, _firstTxIndex, _firstTxIndex + _layout.size() );
}

std::string legacy( const std::string& _layout, size_t _firstTxIndex = 0 ) {
    auto map = makeMap( _layout, _firstTxIndex );
    size_t endTxIndex = _firstTxIndex + _layout.size();
    return applied(
        legacyDecryptedFields( map, _firstTxIndex, endTxIndex ), map, _firstTxIndex, endTxIndex );
}

struct PatchTimestampsFixture : public dev::test::TestOutputHelperFixture {
    void setWindow( time_t _legacyStart, time_t _fix ) {
        ChainParams cp( genesisInfo( Network::ConstantinopleTest ) );
        cp.setPatchTimestamps(
            { { SchainPatchEnum::DecryptedTxFieldsLegacyStartPatch, _legacyStart },
                { SchainPatchEnum::DecryptedTxFieldsPatch, _fix } } );
        SchainPatch::init( cp );
    }
    ~PatchTimestampsFixture() {
        ChainParams cp( genesisInfo( Network::ConstantinopleTest ) );
        SchainPatch::init( cp );
    }
};

}  // namespace

BOOST_FIXTURE_TEST_SUITE( DecryptedTxFieldsPatchTests, PatchTimestampsFixture )

BOOST_AUTO_TEST_CASE( currentBehaviourUsesOwnIndex ) {
    BOOST_REQUIRE_EQUAL( current( "" ), "" );
    BOOST_REQUIRE_EQUAL( current( "P" ), "0" );
    BOOST_REQUIRE_EQUAL( current( "B" ), "1" );
    BOOST_REQUIRE_EQUAL( current( "PB" ), "01" );
    BOOST_REQUIRE_EQUAL( current( "BP" ), "10" );
    BOOST_REQUIRE_EQUAL( current( "BB" ), "11" );
    BOOST_REQUIRE_EQUAL( current( "PBB" ), "011" );
    BOOST_REQUIRE_EQUAL( current( "BBPB" ), "1101" );
    BOOST_REQUIRE_EQUAL( current( "PPBPB" ), "00101" );
    // a failed decryption gives no fields and does not affect the other transactions
    BOOST_REQUIRE_EQUAL( current( "FB" ), "01" );
    BOOST_REQUIRE_EQUAL( current( "PFB" ), "001" );
    // with a CTX prefix the map keys are block indexes
    BOOST_REQUIRE_EQUAL( current( "PB", 3 ), "01" );
    BOOST_REQUIRE_EQUAL( current( "BPB", 3 ), "101" );
    // the map from consensus is used as it is, without a copy
    auto map = std::make_shared< const DecryptedRegularTxsMap >( makeMap( "PB" ) );
    BOOST_REQUIRE( decryptedFields( map ) == map );
}

BOOST_AUTO_TEST_CASE( legacyBehaviourIsKept ) {
    BOOST_REQUIRE_EQUAL( legacy( "" ), "" );
    BOOST_REQUIRE_EQUAL( legacy( "P" ), "0" );
    BOOST_REQUIRE_EQUAL( legacy( "B" ), "1" );
    BOOST_REQUIRE_EQUAL( legacy( "BP" ), "10" );
    BOOST_REQUIRE_EQUAL( legacy( "BB" ), "11" );
    BOOST_REQUIRE_EQUAL( legacy( "BBP" ), "110" );
    BOOST_REQUIRE_EQUAL( legacy( "PB" ), "00" );
    BOOST_REQUIRE_EQUAL( legacy( "PBB" ), "000" );
    BOOST_REQUIRE_EQUAL( legacy( "BBPB" ), "1100" );
    BOOST_REQUIRE_EQUAL( legacy( "PPBPB" ), "00000" );
    BOOST_REQUIRE_EQUAL( legacy( "BPBB" ), "1000" );
    BOOST_REQUIRE_EQUAL( legacy( "FB" ), "01" );
    BOOST_REQUIRE_EQUAL( legacy( "PFB" ), "000" );
    BOOST_REQUIRE_EQUAL( legacy( "PB", 3 ), "00" );
    BOOST_REQUIRE_EQUAL( legacy( "BPB", 3 ), "100" );
}

// The loop of releases up to 5.3.x, kept here as the reference for the legacy behaviour.
static std::string referenceLegacyLoop(
    const DecryptedRegularTxsMap& _map, size_t _firstTxIndex, size_t _txCount ) {
    std::string result;
    auto regularTxnsIterator = _map.begin();
    for ( size_t i = _firstTxIndex; i < _firstTxIndex + _txCount; ++i ) {
        bool fieldsSet = false;
        if ( regularTxnsIterator != _map.end() && regularTxnsIterator->first == i ) {
            std::optional< DecryptedRegularTxFields > txFields = regularTxnsIterator->second;
            if ( txFields.has_value() )
                fieldsSet = true;
        }
        result += fieldsSet ? '1' : '0';
        if ( regularTxnsIterator != _map.end() )
            ++regularTxnsIterator;
    }
    return result;
}

// every layout of up to 7 transactions, with and without a CTX prefix and stray map keys
BOOST_AUTO_TEST_CASE( legacyBehaviourMatchesReferenceLoop ) {
    const std::string kinds = "PBF";
    for ( size_t length = 0; length <= 7; ++length ) {
        size_t layoutsCount = 1;
        for ( size_t k = 0; k < length; ++k )
            layoutsCount *= kinds.size();
        for ( size_t n = 0; n < layoutsCount; ++n ) {
            std::string layout;
            for ( size_t k = 0, rest = n; k < length; ++k, rest /= kinds.size() )
                layout += kinds[rest % kinds.size()];
            for ( size_t firstTxIndex : { size_t( 0 ), size_t( 1 ), size_t( 3 ) } ) {
                for ( bool strayKeys : { false, true } ) {
                    auto map = makeMap( layout, firstTxIndex );
                    if ( strayKeys ) {
                        if ( firstTxIndex > 0 )
                            map.emplace( 0, DecryptedRegularTxFields{ { 7 }, {} } );
                        map.emplace(
                            firstTxIndex + length + 2, DecryptedRegularTxFields{ { 8 }, {} } );
                    }
                    size_t endTxIndex = firstTxIndex + length;
                    BOOST_REQUIRE_EQUAL(
                        applied( legacyDecryptedFields( map, firstTxIndex, endTxIndex ), map,
                            firstTxIndex, endTxIndex ),
                        referenceLegacyLoop( map, firstTxIndex, length ) );
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE( entriesOutsideTheTransactionRangeAreIgnored ) {
    // keys 0 and 9 do not belong to the two transactions at block indexes 3 and 4
    auto map = std::make_shared< DecryptedRegularTxsMap >();
    map->emplace( 0, DecryptedRegularTxFields{ { 1 }, {} } );
    map->emplace( 4, DecryptedRegularTxFields{ { 2 }, {} } );
    map->emplace( 9, DecryptedRegularTxFields{ { 3 }, {} } );

    BOOST_REQUIRE_EQUAL( applied( decryptedFields( map ), *map, 3, 5 ), "01" );
    BOOST_REQUIRE_EQUAL( applied( legacyDecryptedFields( *map, 3, 5 ), *map, 3, 5 ), "01" );
}

BOOST_AUTO_TEST_CASE( nothingSetMeansLegacyFromTheFirstBlock ) {
    setWindow( 0, 0 );
    BOOST_REQUIRE( useLegacyDecryptedFields( 0 ) );
    BOOST_REQUIRE( useLegacyDecryptedFields( 1 ) );
    BOOST_REQUIRE( useLegacyDecryptedFields( 2000000000 ) );
}

BOOST_AUTO_TEST_CASE( onlyFixSetMeansLegacyUntilTheFix ) {
    setWindow( 0, 200 );
    BOOST_REQUIRE( useLegacyDecryptedFields( 0 ) );
    BOOST_REQUIRE( useLegacyDecryptedFields( 199 ) );
    BOOST_REQUIRE( !useLegacyDecryptedFields( 200 ) );
    BOOST_REQUIRE( !useLegacyDecryptedFields( 201 ) );
}

BOOST_AUTO_TEST_CASE( windowBoundaries ) {
    setWindow( 100, 200 );
    BOOST_REQUIRE( !useLegacyDecryptedFields( 0 ) );
    BOOST_REQUIRE( !useLegacyDecryptedFields( 99 ) );
    BOOST_REQUIRE( useLegacyDecryptedFields( 100 ) );
    BOOST_REQUIRE( useLegacyDecryptedFields( 199 ) );
    BOOST_REQUIRE( !useLegacyDecryptedFields( 200 ) );
    BOOST_REQUIRE( !useLegacyDecryptedFields( 2000000000 ) );
}

BOOST_AUTO_TEST_CASE( onlyStartSetMeansLegacyNeverEnds ) {
    setWindow( 100, 0 );
    BOOST_REQUIRE( !useLegacyDecryptedFields( 99 ) );
    BOOST_REQUIRE( useLegacyDecryptedFields( 100 ) );
    BOOST_REQUIRE( useLegacyDecryptedFields( 2000000000 ) );
}

BOOST_AUTO_TEST_CASE( emptyWindowMeansCurrentBehaviourEverywhere ) {
    // start == fix, and start after fix: the legacy behaviour is never selected
    setWindow( 200, 200 );
    BOOST_REQUIRE( !useLegacyDecryptedFields( 199 ) );
    BOOST_REQUIRE( !useLegacyDecryptedFields( 200 ) );
    setWindow( 300, 200 );
    BOOST_REQUIRE( !useLegacyDecryptedFields( 100 ) );
    BOOST_REQUIRE( !useLegacyDecryptedFields( 250 ) );
    BOOST_REQUIRE( !useLegacyDecryptedFields( 300 ) );
}

BOOST_AUTO_TEST_CASE( selectorFollowsTheWindow ) {
    setWindow( 100, 200 );
    auto map = std::make_shared< const DecryptedRegularTxsMap >( makeMap( "PB" ) );
    BOOST_REQUIRE_EQUAL( applied( selectDecryptedFields( map, 0, 2, 99 ), *map, 0, 2 ), "01" );
    BOOST_REQUIRE_EQUAL( applied( selectDecryptedFields( map, 0, 2, 100 ), *map, 0, 2 ), "00" );
    BOOST_REQUIRE_EQUAL( applied( selectDecryptedFields( map, 0, 2, 199 ), *map, 0, 2 ), "00" );
    BOOST_REQUIRE_EQUAL( applied( selectDecryptedFields( map, 0, 2, 200 ), *map, 0, 2 ), "01" );
}

BOOST_AUTO_TEST_SUITE_END()

#endif  // BITE
