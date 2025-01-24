/*
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
/** @file testHelperTest.cpp
 * Unit tests for TestHelper functions.
 */

#include <test/tools/libtesteth/TestHelper.h>
#include <test/tools/libtesteth/TestOutputHelper.h>
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace dev;
using namespace dev::test;

BOOST_FIXTURE_TEST_SUITE( TestHelperSuite, TestOutputHelperFixture )

BOOST_AUTO_TEST_SUITE( TranslateNetworks )
BOOST_AUTO_TEST_CASE( translateNetworks_gteIstanbul,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    set< string > networks = {">=Istanbul"};
    networks = test::translateNetworks( networks );
    BOOST_CHECK( contains( networks, "Istanbul" ) );
}

BOOST_AUTO_TEST_CASE( translateNetworks_gtConstantinople,
    
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    set< string > networks = {">Constantinople"};
    networks = test::translateNetworks( networks );
    BOOST_CHECK( !contains( networks, "Constantinople" ) );
    BOOST_CHECK( contains( networks, "ConstantinopleFix" ) );
}

BOOST_AUTO_TEST_CASE( translateNetworks_gtHomestead,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    set< string > networks = {"Frontier", ">Homestead"};
    networks = test::translateNetworks( networks );
    BOOST_REQUIRE( networks.count( "Frontier" ) > 0 );
    BOOST_REQUIRE( networks.count( "Homestead" ) == 0 );
    for ( auto const& net : test::getNetworks() ) {
        if ( net != eth::Network::FrontierTest && net != eth::Network::HomesteadTest )
            BOOST_REQUIRE( networks.count( test::netIdToString( net ) ) > 0 );
    }
}

BOOST_AUTO_TEST_CASE( translateNetworks_geHomestead,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    set< string > networks = {"Frontier", ">=Homestead"};
    networks = test::translateNetworks( networks );
    for ( auto const& net : test::getNetworks() )
        BOOST_REQUIRE( networks.count( test::netIdToString( net ) ) > 0 );
}

BOOST_AUTO_TEST_CASE( translateNetworks_ltHomestead,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    set< string > networks = {"<Homestead"};
    networks = test::translateNetworks( networks );
    BOOST_REQUIRE( networks.count( "Frontier" ) > 0 );
    for ( auto const& net : test::getNetworks() ) {
        if ( net != eth::Network::FrontierTest )
            BOOST_REQUIRE( networks.count( test::netIdToString( net ) ) == 0 );
    }
}

BOOST_AUTO_TEST_CASE( translateNetworks_ltTest,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    set< string > networks = {"<=EIP150", "<EIP158"};
    networks = test::translateNetworks( networks );
    BOOST_REQUIRE( networks.count( "Frontier" ) > 0 );
    BOOST_REQUIRE( networks.count( "Homestead" ) > 0 );
    BOOST_REQUIRE( networks.count( "EIP150" ) > 0 );
    BOOST_REQUIRE( networks.count( "EIP158" ) == 0 );
    BOOST_REQUIRE( networks.count( "Byzantium" ) == 0 );
}

BOOST_AUTO_TEST_CASE( translateNetworks_leHomestead,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    set< string > networks = {"<=Homestead"};
    networks = test::translateNetworks( networks );
    BOOST_REQUIRE( networks.count( "Frontier" ) > 0 );
    BOOST_REQUIRE( networks.count( "Homestead" ) > 0 );
    for ( auto const& net : test::getNetworks() ) {
        if ( net != eth::Network::FrontierTest && net != eth::Network::HomesteadTest )
            BOOST_REQUIRE( networks.count( test::netIdToString( net ) ) == 0 );
    }
}

BOOST_AUTO_TEST_CASE( translateNetworks_leFrontier,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    set< string > networks = {"<=Frontier"};
    networks = test::translateNetworks( networks );
    BOOST_REQUIRE( networks.count( "Frontier" ) > 0 );
    for ( auto const& net : test::getNetworks() ) {
        if ( net != eth::Network::FrontierTest )
            BOOST_REQUIRE( networks.count( test::netIdToString( net ) ) == 0 );
    }
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( TestHelper )
BOOST_AUTO_TEST_CASE( levenshteinDistance_similar,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    char const* word1 = "someword";
    char const* word2 = "soemword";
    size_t distance = test::levenshteinDistance( word1, strlen( word1 ), word2, strlen( word2 ) );
    BOOST_CHECK_EQUAL( distance, 2 );
}

BOOST_AUTO_TEST_CASE( levenshteinDistance_similar2,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    char const* word1 = "sOmeWord";
    char const* word2 = "someword";
    size_t distance = test::levenshteinDistance( word1, strlen( word1 ), word2, strlen( word2 ) );
    BOOST_CHECK_EQUAL( distance, 2 );
}

BOOST_AUTO_TEST_CASE( levenshteinDistance_similar3,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    char const* word1 = "sOmeWoRd";
    char const* word2 = "someword";
    size_t distance = test::levenshteinDistance( word1, strlen( word1 ), word2, strlen( word2 ) );
    BOOST_CHECK_EQUAL( distance, 3 );
}

BOOST_AUTO_TEST_CASE( levenshteinDistance_similar4,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    char const* word1 = "sOmeWoRd";
    char const* word2 = "soemword";
    size_t distance = test::levenshteinDistance( word1, strlen( word1 ), word2, strlen( word2 ) );
    BOOST_CHECK_EQUAL( distance, 5 );
}

BOOST_AUTO_TEST_CASE( levenshteinDistance_AgtB,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    char const* word1 = "someword";
    char const* word2 = "other";
    size_t distance = test::levenshteinDistance( word1, strlen( word1 ), word2, strlen( word2 ) );
    BOOST_CHECK_EQUAL( distance, 4 );
}

BOOST_AUTO_TEST_CASE( levenshteinDistance_AgtB2,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    char const* word1 = "some long sentence here";
    char const* word2 = "other shorter phrase";
    size_t distance = test::levenshteinDistance( word1, strlen( word1 ), word2, strlen( word2 ) );
    BOOST_CHECK_EQUAL( distance, 14 );
}

BOOST_AUTO_TEST_CASE( levenshteinDistance_BgtA,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    char const* word1 = "other";
    char const* word2 = "someword";
    size_t distance = test::levenshteinDistance( word1, strlen( word1 ), word2, strlen( word2 ) );
    BOOST_CHECK_EQUAL( distance, 4 );
}

BOOST_AUTO_TEST_CASE( levenshteinDistance_BgtA2,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    char const* word1 = "other shorter phrase";
    char const* word2 = "some long sentence here";
    size_t distance = test::levenshteinDistance( word1, strlen( word1 ), word2, strlen( word2 ) );
    BOOST_CHECK_EQUAL( distance, 14 );
}

BOOST_AUTO_TEST_CASE( levenshteinDistance_different,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    char const* word1 = "abdefg";
    char const* word2 = "hijklmn";
    size_t distance = test::levenshteinDistance( word1, strlen( word1 ), word2, strlen( word2 ) );
    BOOST_CHECK_EQUAL( distance, 6 );
}

BOOST_AUTO_TEST_CASE( getTestSuggestions,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    vector< string > const testList = {
        "test1", "test2", "BlockSuite", "BlockSuite/TestCase", "GeneralBlockchainTests"};
    auto list = test::testSuggestions( testList, "blocksuit" );
    BOOST_CHECK( test::inArray( list, string( "BlockSuite" ) ) );
}

BOOST_AUTO_TEST_CASE( getTestSuggestions2,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    vector< string > const testList = {"test1", "test2", "BlockSuite", "BlockSuite/TestCase",
        "GeneralBlockchainTests", "GeneralStateTests/stExample", "BCGeneralStateTests/stExample"};

    auto list = test::testSuggestions( testList, "GeneralStateTests/stExample2" );
    BOOST_CHECK( test::inArray( list, string( "GeneralStateTests/stExample" ) ) );
    BOOST_CHECK( test::inArray( list, string( "BCGeneralStateTests/stExample" ) ) );
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
