/*
    Modifications Copyright (C) 2018 SKALE Labs

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
/** @file core.cpp
 * @author Dimitry Khokhlov <winsvega@mail.ru>
 * @date 2014
 * CORE test functions.
 */

#include <libdevcore/BMPBN.h>
#include <libdevcore/BMPBN_tests.h>
#include <libdevcore/Common.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/Log.h>
#include <test/tools/libtesteth/TestOutputHelper.h>
#include <boost/test/unit_test.hpp>

#include <skutils/console_colors.h>

using namespace dev::test;

BOOST_FIXTURE_TEST_SUITE( CoreLibTests, TestOutputHelperFixture )

BOOST_AUTO_TEST_CASE( toHex, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    dev::bytes b = dev::fromHex( "f0e1d2c3b4a59687" );
    BOOST_CHECK_EQUAL( dev::toHex( b ), "f0e1d2c3b4a59687" );
    BOOST_CHECK_EQUAL( dev::toHexPrefixed( b ), "0xf0e1d2c3b4a59687" );

    dev::h256 h( "705a1849c02140e7197fbde82987a9eb623f97e32fc479a3cd8e4b3b52dcc4b2" );
    BOOST_CHECK_EQUAL(
        dev::toHex( h ), "705a1849c02140e7197fbde82987a9eb623f97e32fc479a3cd8e4b3b52dcc4b2" );
    BOOST_CHECK_EQUAL( dev::toHexPrefixed( h ),
        "0x705a1849c02140e7197fbde82987a9eb623f97e32fc479a3cd8e4b3b52dcc4b2" );
}

BOOST_AUTO_TEST_CASE(    toCompactHex, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    dev::u256 i( "0x123456789abcdef" );
    BOOST_CHECK_EQUAL( dev::toCompactHex( i ), "0123456789abcdef" );
    BOOST_CHECK_EQUAL( dev::toCompactHexPrefixed( i ), "0x0123456789abcdef" );
}

BOOST_AUTO_TEST_CASE( byteRef, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    cnote << "bytesRef copyTo and toString...";
    dev::bytes originalSequence =
        dev::fromHex( "0102030405060708091011121314151617181920212223242526272829303132" );
    dev::bytesRef out( &originalSequence.at( 0 ), 32 );
    dev::h256 hash32( "1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347" );
    hash32.ref().copyTo( out );

    BOOST_CHECK_MESSAGE(
        out.size() == 32, "Error wrong result size when h256::ref().copyTo(dev::bytesRef out)" );
    BOOST_CHECK_MESSAGE(
        out.toBytes() == originalSequence, "Error when h256::ref().copyTo(dev::bytesRef out)" );
}

BOOST_AUTO_TEST_CASE( isHex, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    BOOST_CHECK( dev::isHex( "0x" ) );
    BOOST_CHECK( dev::isHex( "0xA" ) );
    BOOST_CHECK( dev::isHex( "0xAB" ) );
    BOOST_CHECK( dev::isHex( "0x0AA" ) );
    BOOST_CHECK( !dev::isHex( "0x0Ag" ) );
    BOOST_CHECK( !dev::isHex( "0Ag" ) );
    BOOST_CHECK( !dev::isHex( " " ) );
    BOOST_CHECK( dev::isHex( "aa" ) );
    BOOST_CHECK( dev::isHex( "003" ) );
}

BOOST_AUTO_TEST_CASE( BMPBN ) {
    auto bPrev = cc::_on_;
    cc::_on_ = true;

    bool bIsVerbose = true;

    BOOST_CHECK_MESSAGE( dev::BMPBN::test< dev::bigint >( false, true, bIsVerbose, "dev::bigint" ),
        "BMPBN set of simple checks failed for dev::bigint" );
    BOOST_CHECK_MESSAGE( dev::BMPBN::test< dev::u64 >( true, false, bIsVerbose, "dev::u64" ),
        "BMPBN set of simple checks failed for dev::u64" );

    BOOST_CHECK_MESSAGE( dev::BMPBN::test< dev::u128 >( false, false, bIsVerbose, "dev::u128" ),
        "BMPBN set of simple checks failed for dev::u128" );

    BOOST_CHECK_MESSAGE( dev::BMPBN::test< dev::u160 >( false, false, bIsVerbose, "dev::u160" ),
        "BMPBN set of simple checks failed for dev::u160" );
    BOOST_CHECK_MESSAGE( dev::BMPBN::test< dev::s160 >( true, false, bIsVerbose, "dev::s160" ),
        "BMPBN set of simple checks failed for dev::s160" );

    BOOST_CHECK_MESSAGE( dev::BMPBN::test< dev::u256 >( false, true, bIsVerbose, "dev::u256" ),
        "BMPBN set of simple checks failed for dev::u256" );
    BOOST_CHECK_MESSAGE( dev::BMPBN::test< dev::s256 >( true, true, bIsVerbose, "dev::s256" ),
        "BMPBN set of simple checks failed for dev::s256" );

    BOOST_CHECK_MESSAGE( dev::BMPBN::test< dev::u512 >( false, true, bIsVerbose, "dev::u512" ),
        "BMPBN set of simple checks failed for dev::u512" );
    BOOST_CHECK_MESSAGE( dev::BMPBN::test< dev::s512 >( true, true, bIsVerbose, "dev::s512" ),
        "BMPBN set of simple checks failed for dev::s512" );

    BOOST_CHECK( dev::BMPBN::test_limit_limbs_and_halves< dev::u64 >(
        "9223372036854775807", 64, bIsVerbose ) );
    BOOST_CHECK( dev::BMPBN::test_limit_limbs_and_halves< dev::u128 >(
        "170141183460469231731687303715884105727", 128, bIsVerbose ) );
    BOOST_CHECK( dev::BMPBN::test_limit_limbs_and_halves< dev::u256 >(
        "115792089237316195423570985008687907853269984665640564039457584007913129639935", 256,
        bIsVerbose ) );
    BOOST_CHECK( dev::BMPBN::test_limit_limbs_and_halves< dev::u512 >(
        "134078079299425970995740249982058461274793658205923933777235614437217640300735469768018742"
        "98166903427690031858186486050853753882811946569946433649006084095",
        512, bIsVerbose ) );

    cc::_on_ = bPrev;
}

BOOST_AUTO_TEST_SUITE_END()
