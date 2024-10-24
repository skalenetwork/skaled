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
/** @file PrecompiledTest.cpp
 * Preompiled contract implemetations testing.
 */

#include <libdevcore/FileSystem.h>
#include <libdevcore/TransientDirectory.h>
#include <libdevcrypto/Hash.h>
#include <libethereum/Precompiled.h>
#include <libethereum/ChainParams.h>
#include <libethereum/Client.h>
#include <libethereum/ClientTest.h>
#include <libethereum/SkaleHost.h>
#include <libethereum/TransactionQueue.h>
#include <test/tools/libtesteth/TestHelper.h>
#include <boost/test/unit_test.hpp>
#include <libskale/OverlayFS.h>
#include <libethereum/SchainPatch.h>

#include <secp256k1_sha256.h>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::test;
namespace ut = boost::unit_test;

std::string numberToHex( size_t inputNumber ) {
    std::stringstream sstream;
    sstream << std::hex << inputNumber;
    std::string hexNumber = sstream.str();
    hexNumber.insert( hexNumber.begin(), 64 - hexNumber.length(), '0' );
    return hexNumber;
}

std::string stringToHex( std::string inputString ) {
    size_t strLength = ( ( inputString.size() * 2 + 63 ) / 64 ) * 64;
    std::string hexString = toHex( inputString.begin(), inputString.end(), "" );
    hexString.insert( hexString.begin() + hexString.length(), strLength - hexString.length(), '0' );
    return hexString;
}

BOOST_FIXTURE_TEST_SUITE( PrecompiledTests, TestOutputHelperFixture )

BOOST_AUTO_TEST_CASE( modexpFermatTheorem, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000001"
        "0000000000000000000000000000000000000000000000000000000000000020"
        "0000000000000000000000000000000000000000000000000000000000000020"
        "03"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2e"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f" );
    auto res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( res.first );
    bytes expected = fromHex( "0000000000000000000000000000000000000000000000000000000000000001" );
    BOOST_REQUIRE_EQUAL_COLLECTIONS(
        res.second.begin(), res.second.end(), expected.begin(), expected.end() );
}

BOOST_AUTO_TEST_CASE( modexpZeroBase, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000020"
        "0000000000000000000000000000000000000000000000000000000000000020"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2e"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f" );
    auto res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( res.first );
    bytes expected = fromHex( "0000000000000000000000000000000000000000000000000000000000000000" );
    BOOST_REQUIRE_EQUAL_COLLECTIONS(
        res.second.begin(), res.second.end(), expected.begin(), expected.end() );
}

BOOST_AUTO_TEST_CASE( modexpExtraByteIgnored, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000001"
        "0000000000000000000000000000000000000000000000000000000000000002"
        "0000000000000000000000000000000000000000000000000000000000000020"
        "03"
        "ffff"
        "8000000000000000000000000000000000000000000000000000000000000000"
        "07" );
    auto res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( res.first );
    bytes expected = fromHex( "3b01b01ac41f2d6e917c6d6a221ce793802469026d9ab7578fa2e79e4da6aaab" );
    BOOST_REQUIRE_EQUAL_COLLECTIONS(
        res.second.begin(), res.second.end(), expected.begin(), expected.end() );
}

BOOST_AUTO_TEST_CASE( modexpRightPadding, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000001"
        "0000000000000000000000000000000000000000000000000000000000000002"
        "0000000000000000000000000000000000000000000000000000000000000020"
        "03"
        "ffff"
        "80" );
    auto res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( res.first );
    bytes expected = fromHex( "3b01b01ac41f2d6e917c6d6a221ce793802469026d9ab7578fa2e79e4da6aaab" );
    BOOST_REQUIRE_EQUAL_COLLECTIONS(
        res.second.begin(), res.second.end(), expected.begin(), expected.end() );
}

BOOST_AUTO_TEST_CASE( modexpMissingValues ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000001"
        "0000000000000000000000000000000000000000000000000000000000000002"
        "0000000000000000000000000000000000000000000000000000000000000020"
        "03" );
    auto res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( res.first );
    bytes expected = fromHex( "0000000000000000000000000000000000000000000000000000000000000000" );
    BOOST_REQUIRE_EQUAL_COLLECTIONS(
        res.second.begin(), res.second.end(), expected.begin(), expected.end() );
}

BOOST_AUTO_TEST_CASE( modexpEmptyValue, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000001"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000020"
        "03"
        "8000000000000000000000000000000000000000000000000000000000000000" );
    auto res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( res.first );
    bytes expected = fromHex( "0000000000000000000000000000000000000000000000000000000000000001" );
    BOOST_REQUIRE_EQUAL_COLLECTIONS(
        res.second.begin(), res.second.end(), expected.begin(), expected.end() );
}

BOOST_AUTO_TEST_CASE( modexpZeroPowerZero, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000001"
        "0000000000000000000000000000000000000000000000000000000000000001"
        "0000000000000000000000000000000000000000000000000000000000000020"
        "00"
        "00"
        "80" );
    auto res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( res.first );
    bytes expected = fromHex( "0000000000000000000000000000000000000000000000000000000000000001" );
    BOOST_REQUIRE_EQUAL_COLLECTIONS(
        res.second.begin(), res.second.end(), expected.begin(), expected.end() );
}

BOOST_AUTO_TEST_CASE( modexpZeroPowerZeroModZero, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000001"
        "0000000000000000000000000000000000000000000000000000000000000001"
        "0000000000000000000000000000000000000000000000000000000000000020"
        "00"
        "00"
        "00" );
    auto res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( res.first );
    bytes expected = fromHex( "0000000000000000000000000000000000000000000000000000000000000000" );
    BOOST_REQUIRE_EQUAL_COLLECTIONS(
        res.second.begin(), res.second.end(), expected.begin(), expected.end() );
}

BOOST_AUTO_TEST_CASE( modexpModLengthZero, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000001"
        "0000000000000000000000000000000000000000000000000000000000000001"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "01"
        "01" );
    auto res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( res.first );
    BOOST_REQUIRE( res.second.empty() );
}

BOOST_AUTO_TEST_CASE( modexpCostFermatTheorem, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledPricer cost = PrecompiledRegistrar::pricer( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000001"
        "0000000000000000000000000000000000000000000000000000000000000020"
        "0000000000000000000000000000000000000000000000000000000000000020"
        "03"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2e"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f" );
    auto res = cost( ref( in ), {}, {} );

    BOOST_REQUIRE_EQUAL( static_cast< int >( res ), 13056 );
}

BOOST_AUTO_TEST_CASE( modexpCostTooLarge, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledPricer cost = PrecompiledRegistrar::pricer( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000000"
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
        "0000000000000000000000000000000000000000000000000000000000000020"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffd" );
    auto res = cost( ref( in ), {}, {} );

    BOOST_REQUIRE_MESSAGE(
        res ==
            bigint{
                "47428439751604713645494675459558567056699385719046375030561826409641217900517324"},
        "Got: " + toString( res ) );
}

BOOST_AUTO_TEST_CASE( modexpCostEmptyExponent, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledPricer cost = PrecompiledRegistrar::pricer( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000008"  // length of B
        "0000000000000000000000000000000000000000000000000000000000000000"  // length of E
        "0000000000000000000000000000000000000000000000000000000000000010"  // length of M
        "998877665544332211"                                                // B
        ""                                                                  // E
        "998877665544332211998877665544332211"                              // M
        "9978"  // Garbage that should be ignored
    );
    auto res = cost( ref( in ), {}, {} );

    BOOST_REQUIRE_MESSAGE( res == bigint{"12"}, "Got: " + toString( res ) );
}

BOOST_AUTO_TEST_CASE( modexpCostZeroExponent, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledPricer cost = PrecompiledRegistrar::pricer( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000000"  // length of B
        "0000000000000000000000000000000000000000000000000000000000000003"  // length of E
        "000000000000000000000000000000000000000000000000000000000000000a"  // length of M
        ""                                                                  // B
        "000000"                                                            // E
        "112233445566778899aa"                                              // M
    );
    auto res = cost( ref( in ), {}, {} );

    BOOST_REQUIRE_MESSAGE( res == bigint{"5"}, "Got: " + toString( res ) );
}

BOOST_AUTO_TEST_CASE( modexpCostApproximated, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledPricer cost = PrecompiledRegistrar::pricer( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000003"    // length of B
        "0000000000000000000000000000000000000000000000000000000000000021"    // length of E
        "000000000000000000000000000000000000000000000000000000000000000a"    // length of M
        "111111"                                                              // B
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"  // E
        "112233445566778899aa"                                                // M
    );
    auto res = cost( ref( in ), {}, {} );

    BOOST_REQUIRE_MESSAGE( res == bigint{"1315"}, "Got: " + toString( res ) );
}

BOOST_AUTO_TEST_CASE( modexpCostApproximatedPartialByte,
    
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledPricer cost = PrecompiledRegistrar::pricer( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000003"    // length of B
        "0000000000000000000000000000000000000000000000000000000000000021"    // length of E
        "000000000000000000000000000000000000000000000000000000000000000a"    // length of M
        "111111"                                                              // B
        "02ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"  // E
        "112233445566778899aa"                                                // M
    );
    auto res = cost( ref( in ), {}, {} );

    BOOST_REQUIRE_MESSAGE( res == bigint{"1285"}, "Got: " + toString( res ) );
}

BOOST_AUTO_TEST_CASE( modexpCostApproximatedGhost, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledPricer cost = PrecompiledRegistrar::pricer( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000003"    // length of B
        "0000000000000000000000000000000000000000000000000000000000000021"    // length of E
        "000000000000000000000000000000000000000000000000000000000000000a"    // length of M
        "111111"                                                              // B
        "000000000000000000000000000000000000000000000000000000000000000000"  // E
        "112233445566778899aa"                                                // M
    );
    auto res = cost( ref( in ), {}, {} );

    BOOST_REQUIRE_MESSAGE( res == bigint{"40"}, "Got: " + toString( res ) );
}

BOOST_AUTO_TEST_CASE( modexpCostMidRange, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledPricer cost = PrecompiledRegistrar::pricer( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000003"    // length of B
        "0000000000000000000000000000000000000000000000000000000000000021"    // length of E
        "000000000000000000000000000000000000000000000000000000000000004a"    // length of M = 74
        "111111"                                                              // B
        "000000000000000000000000000000000000000000000000000000000000000000"  // E
        "112233445566778899aa"                                                // M
    );
    auto res = cost( ref( in ), {}, {} );

    BOOST_REQUIRE_MESSAGE(
        res == ( ( 74 * 74 / 4 + 96 * 74 - 3072 ) * 8 ) / 20, "Got: " + toString( res ) );
}

BOOST_AUTO_TEST_CASE( modexpCostHighRange, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledPricer cost = PrecompiledRegistrar::pricer( "modexp" );

    bytes in = fromHex(
        "0000000000000000000000000000000000000000000000000000000000000003"    // length of B
        "0000000000000000000000000000000000000000000000000000000000000021"    // length of E
        "0000000000000000000000000000000000000000000000000000000000000401"    // length of M = 1025
        "111111"                                                              // B
        "000000000000000000000000000000000000000000000000000000000000000000"  // E
        "112233445566778899aa"                                                // M
    );
    auto res = cost( ref( in ), {}, {} );

    BOOST_REQUIRE_MESSAGE(
        res == ( ( 1025 * 1025 / 16 + 480 * 1025 - 199680 ) * 8 ) / 20, "Got: " + toString( res ) );
}

/// @defgroup PrecompiledTests Test cases for precompiled contracts.
///
/// These test cases are used for testing and benchmarking precompiled contracts.
/// They are ported from go-ethereum, so formatting is not perfect.
/// https://github.com/ethereum/go-ethereum/blob/master/core/vm/contracts_test.go.
/// @{

struct PrecompiledTest {
    const char* input;
    const char* expected;
    const char* name;
};

constexpr PrecompiledTest ecrecoverTests[] = {
    {"38d18acb67d25c8bb9942764b62f18e17054f66a817bd4295423adf9ed98873e00000000000000000000000000000"
     "0000000000000000000000000000000001b38d18acb67d25c8bb9942764b62f18e17054f66a817bd4295423adf9ed"
     "98873e789d1dd423d25f0772d2748d60f7e4b81bb14d086eba8e8e8efb6dcff8a4ae02",
        "000000000000000000000000ceaccac640adf55b2028469bd36ba501f28b699d", ""}};

constexpr PrecompiledTest modexpTests[] = {
    {
        "0000000000000000000000000000000000000000000000000000000000000001"
        "0000000000000000000000000000000000000000000000000000000000000020"
        "0000000000000000000000000000000000000000000000000000000000000020"
        "03"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2e"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
        "0000000000000000000000000000000000000000000000000000000000000001",
        "eip_example1",
    },
    {
        "0000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000020"
        "0000000000000000000000000000000000000000000000000000000000000020"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2e"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "eip_example2",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000004000000000000000000000000000"
        "000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000"
        "000000000040e09ad9675465c53a109fac66a445c91b292d2bb2c5268addb30cd82f80fcb0033ff97c80a5fc6f"
        "39193ae969c6ede6710a6b7ac27078a06d90ef1c72e5c85fb502fc9e1f6beb81516545975218075ec2af118cd8"
        "798df6e08a147c60fd6095ac2bb02c2908cf4dd7c81f11c289e4bce98f3553768f392a80ce22bf5c4f4a248c6"
        "b",
        "60008f1614cc01dcfb6bfb09c625cf90b47d4468db81b5f8b7a39d42f332eab9b2da8f2d95311648a8f243f4bb"
        "13cfb3d8f7f2a3c014122ebb3ed41b02783adc",
        "nagydani-1-square",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000004000000000000000000000000000"
        "000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000"
        "000000000040e09ad9675465c53a109fac66a445c91b292d2bb2c5268addb30cd82f80fcb0033ff97c80a5fc6f"
        "39193ae969c6ede6710a6b7ac27078a06d90ef1c72e5c85fb503fc9e1f6beb81516545975218075ec2af118cd8"
        "798df6e08a147c60fd6095ac2bb02c2908cf4dd7c81f11c289e4bce98f3553768f392a80ce22bf5c4f4a248c6"
        "b",
        "4834a46ba565db27903b1c720c9d593e84e4cbd6ad2e64b31885d944f68cd801f92225a8961c952ddf2797fa47"
        "01b330c85c4b363798100b921a1a22a46a7fec",
        "nagydani-1-qube",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000004000000000000000000000000000"
        "000000000000000000000000000000000000030000000000000000000000000000000000000000000000000000"
        "000000000040e09ad9675465c53a109fac66a445c91b292d2bb2c5268addb30cd82f80fcb0033ff97c80a5fc6f"
        "39193ae969c6ede6710a6b7ac27078a06d90ef1c72e5c85fb5010001fc9e1f6beb81516545975218075ec2af11"
        "8cd8798df6e08a147c60fd6095ac2bb02c2908cf4dd7c81f11c289e4bce98f3553768f392a80ce22bf5c4f4a24"
        "8c6b",
        "c36d804180c35d4426b57b50c5bfcca5c01856d104564cd513b461d3c8b8409128a5573e416d0ebe38f5f73676"
        "6d9dc27143e4da981dfa4d67f7dc474cbee6d2",
        "nagydani-1-pow0x10001",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000008000000000000000000000000000"
        "000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000"
        "000000000080cad7d991a00047dd54d3399b6b0b937c718abddef7917c75b6681f40cc15e2be0003657d8d4c34"
        "167b2f0bbbca0ccaa407c2a6a07d50f1517a8f22979ce12a81dcaf707cc0cebfc0ce2ee84ee7f77c38b9281b98"
        "22a8d3de62784c089c9b18dcb9a2a5eecbede90ea788a862a9ddd9d609c2c52972d63e289e28f6a590ffbf5102"
        "e6d893b80aeed5e6e9ce9afa8a5d5675c93a32ac05554cb20e9951b2c140e3ef4e433068cf0fb73bc9f33af185"
        "3f64aa27a0028cbf570d7ac9048eae5dc7b28c87c31e5810f1e7fa2cda6adf9f1076dbc1ec1238560071e7efc4"
        "e9565c49be9e7656951985860a558a754594115830bcdb421f741408346dd5997bb01c287087",
        "981dd99c3b113fae3e3eaa9435c0dc96779a23c12a53d1084b4f67b0b053a27560f627b873e3f16ad78f28c94f"
        "14b6392def26e4d8896c5e3c984e50fa0b3aa44f1da78b913187c6128baa9340b1e9c9a0fd02cb78885e72576d"
        "a4a8f7e5a113e173a7a2889fde9d407bd9f06eb05bc8fc7b4229377a32941a02bf4edcc06d70",
        "nagydani-2-square",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000008000000000000000000000000000"
        "000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000"
        "000000000080cad7d991a00047dd54d3399b6b0b937c718abddef7917c75b6681f40cc15e2be0003657d8d4c34"
        "167b2f0bbbca0ccaa407c2a6a07d50f1517a8f22979ce12a81dcaf707cc0cebfc0ce2ee84ee7f77c38b9281b98"
        "22a8d3de62784c089c9b18dcb9a2a5eecbede90ea788a862a9ddd9d609c2c52972d63e289e28f6a590ffbf5103"
        "e6d893b80aeed5e6e9ce9afa8a5d5675c93a32ac05554cb20e9951b2c140e3ef4e433068cf0fb73bc9f33af185"
        "3f64aa27a0028cbf570d7ac9048eae5dc7b28c87c31e5810f1e7fa2cda6adf9f1076dbc1ec1238560071e7efc4"
        "e9565c49be9e7656951985860a558a754594115830bcdb421f741408346dd5997bb01c287087",
        "d89ceb68c32da4f6364978d62aaa40d7b09b59ec61eb3c0159c87ec3a91037f7dc6967594e530a69d049b64adf"
        "a39c8fa208ea970cfe4b7bcd359d345744405afe1cbf761647e32b3184c7fbe87cee8c6c7ff3b378faba6c68b8"
        "3b6889cb40f1603ee68c56b4c03d48c595c826c041112dc941878f8c5be828154afd4a16311f",
        "nagydani-2-qube",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000008000000000000000000000000000"
        "000000000000000000000000000000000000030000000000000000000000000000000000000000000000000000"
        "000000000080cad7d991a00047dd54d3399b6b0b937c718abddef7917c75b6681f40cc15e2be0003657d8d4c34"
        "167b2f0bbbca0ccaa407c2a6a07d50f1517a8f22979ce12a81dcaf707cc0cebfc0ce2ee84ee7f77c38b9281b98"
        "22a8d3de62784c089c9b18dcb9a2a5eecbede90ea788a862a9ddd9d609c2c52972d63e289e28f6a590ffbf5101"
        "0001e6d893b80aeed5e6e9ce9afa8a5d5675c93a32ac05554cb20e9951b2c140e3ef4e433068cf0fb73bc9f33a"
        "f1853f64aa27a0028cbf570d7ac9048eae5dc7b28c87c31e5810f1e7fa2cda6adf9f1076dbc1ec1238560071e7"
        "efc4e9565c49be9e7656951985860a558a754594115830bcdb421f741408346dd5997bb01c287087",
        "ad85e8ef13fd1dd46eae44af8b91ad1ccae5b7a1c92944f92a19f21b0b658139e0cabe9c1f679507c2de354bf2"
        "c91ebd965d1e633978a830d517d2f6f8dd5fd58065d58559de7e2334a878f8ec6992d9b9e77430d4764e863d77"
        "c0f87beede8f2f7f2ab2e7222f85cc9d98b8467f4bb72e87ef2882423ebdb6daf02dddac6db2",
        "nagydani-2-pow0x10001",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000010000000000000000000000000000"
        "000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000"
        "000000000100c9130579f243e12451760976261416413742bd7c91d39ae087f46794062b8c239f2a74abf39186"
        "05a0e046a7890e049475ba7fbb78f5de6490bd22a710cc04d30088179a919d86c2da62cf37f59d8f258d2310d9"
        "4c24891be2d7eeafaa32a8cb4b0cfe5f475ed778f45907dc8916a73f03635f233f7a77a00a3ec9ca6761a5bbd5"
        "58a2318ecd0caa1c5016691523e7e1fa267dd35e70c66e84380bdcf7c0582f540174e572c41f81e93da0b757df"
        "f0b0fe23eb03aa19af0bdec3afb474216febaacb8d0381e631802683182b0fe72c28392539850650b70509f549"
        "80241dc175191a35d967288b532a7a8223ce2440d010615f70df269501944d4ec16fe4a3cb02d7a85909174757"
        "835187cb52e71934e6c07ef43b4c46fc30bbcd0bc72913068267c54a4aabebb493922492820babdeb7dc9b1558"
        "fcf7bd82c37c82d3147e455b623ab0efa752fe0b3a67ca6e4d126639e645a0bf417568adbb2a6a4eef62fa1fa2"
        "9b2a5a43bebea1f82193a7dd98eb483d09bb595af1fa9c97c7f41f5649d976aee3e5e59e2329b43b13bea228d4"
        "a93f16ba139ccb511de521ffe747aa2eca664f7c9e33da59075cc335afcd2bf3ae09765f01ab5a7c3e3938ec16"
        "8b74724b5074247d200d9970382f683d6059b94dbc336603d1dfee714e4b447ac2fa1d99ecb4961da2854e0379"
        "5ed758220312d101e1e3d87d5313a6d052aebde75110363d",
        "affc7507ea6d84751ec6b3f0d7b99dbcc263f33330e450d1b3ff0bc3d0874320bf4edd57debd58730698815795"
        "8cb3cfd369cc0c9c198706f635c9e0f15d047df5cb44d03e2727f26b083c4ad8485080e1293f171c1ed52aef59"
        "93a5815c35108e848c951cf1e334490b4a539a139e57b68f44fee583306f5b85ffa57206b3ee5660458858534e"
        "5386b9584af3c7f67806e84c189d695e5eb96e1272d06ec2df5dc5fabc6e94b793718c60c36be0a4d031fc84cd"
        "658aa72294b2e16fc240aef70cb9e591248e38bd49c5a554d1afa01f38dab72733092f7555334bbef6c8c43011"
        "9840492380aa95fa025dcf699f0a39669d812b0c6946b6091e6e235337b6f8",
        "nagydani-3-square",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000010000000000000000000000000000"
        "000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000"
        "000000000100c9130579f243e12451760976261416413742bd7c91d39ae087f46794062b8c239f2a74abf39186"
        "05a0e046a7890e049475ba7fbb78f5de6490bd22a710cc04d30088179a919d86c2da62cf37f59d8f258d2310d9"
        "4c24891be2d7eeafaa32a8cb4b0cfe5f475ed778f45907dc8916a73f03635f233f7a77a00a3ec9ca6761a5bbd5"
        "58a2318ecd0caa1c5016691523e7e1fa267dd35e70c66e84380bdcf7c0582f540174e572c41f81e93da0b757df"
        "f0b0fe23eb03aa19af0bdec3afb474216febaacb8d0381e631802683182b0fe72c28392539850650b70509f549"
        "80241dc175191a35d967288b532a7a8223ce2440d010615f70df269501944d4ec16fe4a3cb03d7a85909174757"
        "835187cb52e71934e6c07ef43b4c46fc30bbcd0bc72913068267c54a4aabebb493922492820babdeb7dc9b1558"
        "fcf7bd82c37c82d3147e455b623ab0efa752fe0b3a67ca6e4d126639e645a0bf417568adbb2a6a4eef62fa1fa2"
        "9b2a5a43bebea1f82193a7dd98eb483d09bb595af1fa9c97c7f41f5649d976aee3e5e59e2329b43b13bea228d4"
        "a93f16ba139ccb511de521ffe747aa2eca664f7c9e33da59075cc335afcd2bf3ae09765f01ab5a7c3e3938ec16"
        "8b74724b5074247d200d9970382f683d6059b94dbc336603d1dfee714e4b447ac2fa1d99ecb4961da2854e0379"
        "5ed758220312d101e1e3d87d5313a6d052aebde75110363d",
        "1b280ecd6a6bf906b806d527c2a831e23b238f89da48449003a88ac3ac7150d6a5e9e6b3be4054c7da11dd1e47"
        "0ec29a606f5115801b5bf53bc1900271d7c3ff3cd5ed790d1c219a9800437a689f2388ba1a11d68f6a8e5b74e9"
        "a3b1fac6ee85fc6afbac599f93c391f5dc82a759e3c6c0ab45ce3f5d25d9b0c1bf94cf701ea6466fc9a478dacc"
        "5754e593172b5111eeba88557048bceae401337cd4c1182ad9f700852bc8c99933a193f0b94cf1aedbefc48be3"
        "bc93ef5cb276d7c2d5462ac8bb0c8fe8923a1db2afe1c6b90d59c534994a6a633f0ead1d638fdc293486bb634f"
        "f2c8ec9e7297c04241a61c37e3ae95b11d53343d4ba2b4cc33d2cfa7eb705e",
        "nagydani-3-qube",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000010000000000000000000000000000"
        "000000000000000000000000000000000000030000000000000000000000000000000000000000000000000000"
        "000000000100c9130579f243e12451760976261416413742bd7c91d39ae087f46794062b8c239f2a74abf39186"
        "05a0e046a7890e049475ba7fbb78f5de6490bd22a710cc04d30088179a919d86c2da62cf37f59d8f258d2310d9"
        "4c24891be2d7eeafaa32a8cb4b0cfe5f475ed778f45907dc8916a73f03635f233f7a77a00a3ec9ca6761a5bbd5"
        "58a2318ecd0caa1c5016691523e7e1fa267dd35e70c66e84380bdcf7c0582f540174e572c41f81e93da0b757df"
        "f0b0fe23eb03aa19af0bdec3afb474216febaacb8d0381e631802683182b0fe72c28392539850650b70509f549"
        "80241dc175191a35d967288b532a7a8223ce2440d010615f70df269501944d4ec16fe4a3cb010001d7a8590917"
        "4757835187cb52e71934e6c07ef43b4c46fc30bbcd0bc72913068267c54a4aabebb493922492820babdeb7dc9b"
        "1558fcf7bd82c37c82d3147e455b623ab0efa752fe0b3a67ca6e4d126639e645a0bf417568adbb2a6a4eef62fa"
        "1fa29b2a5a43bebea1f82193a7dd98eb483d09bb595af1fa9c97c7f41f5649d976aee3e5e59e2329b43b13bea2"
        "28d4a93f16ba139ccb511de521ffe747aa2eca664f7c9e33da59075cc335afcd2bf3ae09765f01ab5a7c3e3938"
        "ec168b74724b5074247d200d9970382f683d6059b94dbc336603d1dfee714e4b447ac2fa1d99ecb4961da2854e"
        "03795ed758220312d101e1e3d87d5313a6d052aebde75110363d",
        "37843d7c67920b5f177372fa56e2a09117df585f81df8b300fba245b1175f488c99476019857198ed459ed8d97"
        "99c377330e49f4180c4bf8e8f66240c64f65ede93d601f957b95b83efdee1e1bfde74169ff77002eaf078c7181"
        "5a9220c80b2e3b3ff22c2f358111d816ebf83c2999026b6de50bfc711ff68705d2f40b753424aefc9f70f08d90"
        "8b5a20276ad613b4ab4309a3ea72f0c17ea9df6b3367d44fb3acab11c333909e02e81ea2ed404a712d3ea96bba"
        "87461720e2d98723e7acd0520ac1a5212dbedcd8dc0c1abf61d4719e319ff4758a774790b8d463cdfe131d1b2d"
        "cfee52d002694e98e720cb6ae7ccea353bc503269ba35f0f63bf8d7b672a76",
        "nagydani-3-pow0x10001",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000020000000000000000000000000000"
        "000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000"
        "000000000200db34d0e438249c0ed685c949cc28776a05094e1c48691dc3f2dca5fc3356d2a0663bd376e47128"
        "39917eb9a19c670407e2c377a2de385a3ff3b52104f7f1f4e0c7bf7717fb913896693dc5edbb65b760ef1b00e4"
        "2e9d8f9af17352385e1cd742c9b006c0f669995cb0bb21d28c0aced2892267637b6470d8cee0ab27fc5d42658f"
        "6e88240c31d6774aa60a7ebd25cd48b56d0da11209f1928e61005c6eb709f3e8e0aaf8d9b10f7d7e296d772264"
        "dc76897ccdddadc91efa91c1903b7232a9e4c3b941917b99a3bc0c26497dedc897c25750af60237aa67934a26a"
        "2bc491db3dcc677491944bc1f51d3e5d76b8d846a62db03dedd61ff508f91a56d71028125035c3a44cbb041497"
        "c83bf3e4ae2a9613a401cc721c547a2afa3b16a2969933d3626ed6d8a7428648f74122fd3f2a02a20758f7f693"
        "892c8fd798b39abac01d18506c45e71432639e9f9505719ee822f62ccbf47f6850f096ff77b5afaf4be7d77202"
        "5791717dbe5abf9b3f40cff7d7aab6f67e38f62faf510747276e20a42127e7500c444f9ed92baf65ade9e83684"
        "5e39c4316d9dce5f8e2c8083e2c0acbb95296e05e51aab13b6b8f53f06c9c4276e12b0671133218cc3ea907da3"
        "bd9a367096d9202128d14846cc2e20d56fc8473ecb07cecbfb8086919f3971926e7045b853d85a69d026195c70"
        "f9f7a823536e2a8f4b3e12e94d9b53a934353451094b8102df3143a0057457d75e8c708b6337a6f5a4fd1a0672"
        "7acf9fb93e2993c62f3378b37d56c85e7b1e00f0145ebf8e4095bd723166293c60b6ac1252291ef65823c9e040"
        "ddad14969b3b340a4ef714db093a587c37766d68b8d6b5016e741587e7e6bf7e763b44f0247e64bae30f994d24"
        "8bfd20541a333e5b225ef6a61199e301738b1e688f70ec1d7fb892c183c95dc543c3e12adf8a5e8b9ca9d04f94"
        "45cced3ab256f29e998e69efaa633a7b60e1db5a867924ccab0a171d9d6e1098dfa15acde9553de599eaa56490"
        "c8f411e4985111f3d40bddfc5e301edb01547b01a886550a61158f7e2033c59707789bf7c854181d0c2e2a42a9"
        "3cf09209747d7082e147eb8544de25c3eb14f2e35559ea0c0f5877f2f3fc92132c0ae9da4e45b2f6c866a224ea"
        "6d1f28c05320e287750fbc647368d41116e528014cc1852e5531d53e4af938374daba6cee4baa821ed07117253"
        "bb3601ddd00d59a3d7fb2ef1f5a2fbba7c429f0cf9a5b3462410fd833a69118f8be9c559b1000cc608fd877fb4"
        "3f8e65c2d1302622b944462579056874b387208d90623fcdaf93920ca7a9e4ba64ea208758222ad868501cc2c3"
        "45e2d3a5ea2a17e5069248138c8a79c0251185d29ee73e5afab5354769142d2bf0cb6712727aa6bf84a6245fcd"
        "ae66e4938d84d1b9dd09a884818622080ff5f98942fb20acd7e0c916c2d5ea7ce6f7e173315384518f",
        "8a5aea5f50dcc03dc7a7a272b5aeebc040554dbc1ffe36753c4fc75f7ed5f6c2cc0de3a922bf96c78bf0643a73"
        "025ad21f45a4a5cadd717612c511ab2bff1190fe5f1ae05ba9f8fe3624de1de2a817da6072ddcdb933b5021681"
        "1dbe6a9ca79d3a3c6b3a476b079fd0d05f04fb154e2dd3e5cb83b148a006f2bcbf0042efb2ae7b916ea81b27aa"
        "c25c3bf9a8b6d35440062ad8eae34a83f3ffa2cc7b40346b62174a4422584f72f95316f6b2bee9ff232ba97393"
        "01c97c99a9ded26c45d72676eb856ad6ecc81d36a6de36d7f9dafafee11baa43a4b0d5e4ecffa7b9b7dcefd58c"
        "397dd373e6db4acd2b2c02717712e6289bed7c813b670c4a0c6735aa7f3b0f1ce556eae9fcc94b501b2c8781ba"
        "50a8c6220e8246371c3c7359fe4ef9da786ca7d98256754ca4e496be0a9174bedbecb384bdf470779186d6a833"
        "f068d2838a88d90ef3ad48ff963b67c39cc5a3ee123baf7bf3125f64e77af7f30e105d72c4b9b5b237ed251e4c"
        "122c6d8c1405e736299c3afd6db16a28c6a9cfa68241e53de4cd388271fe534a6a9b0dbea6171d170db1b89858"
        "468885d08fecbd54c8e471c3e25d48e97ba450b96d0d87e00ac732aaa0d3ce4309c1064bd8a4c0808a97e0143e"
        "43a24cfa847635125cd41c13e0574487963e9d725c01375db99c31da67b4cf65eff555f0c0ac416c727ff8d438"
        "ad7c42030551d68c2e7adda0abb1ca7c10",
        "nagydani-4-square",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000020000000000000000000000000000"
        "000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000"
        "000000000200db34d0e438249c0ed685c949cc28776a05094e1c48691dc3f2dca5fc3356d2a0663bd376e47128"
        "39917eb9a19c670407e2c377a2de385a3ff3b52104f7f1f4e0c7bf7717fb913896693dc5edbb65b760ef1b00e4"
        "2e9d8f9af17352385e1cd742c9b006c0f669995cb0bb21d28c0aced2892267637b6470d8cee0ab27fc5d42658f"
        "6e88240c31d6774aa60a7ebd25cd48b56d0da11209f1928e61005c6eb709f3e8e0aaf8d9b10f7d7e296d772264"
        "dc76897ccdddadc91efa91c1903b7232a9e4c3b941917b99a3bc0c26497dedc897c25750af60237aa67934a26a"
        "2bc491db3dcc677491944bc1f51d3e5d76b8d846a62db03dedd61ff508f91a56d71028125035c3a44cbb041497"
        "c83bf3e4ae2a9613a401cc721c547a2afa3b16a2969933d3626ed6d8a7428648f74122fd3f2a02a20758f7f693"
        "892c8fd798b39abac01d18506c45e71432639e9f9505719ee822f62ccbf47f6850f096ff77b5afaf4be7d77202"
        "5791717dbe5abf9b3f40cff7d7aab6f67e38f62faf510747276e20a42127e7500c444f9ed92baf65ade9e83684"
        "5e39c4316d9dce5f8e2c8083e2c0acbb95296e05e51aab13b6b8f53f06c9c4276e12b0671133218cc3ea907da3"
        "bd9a367096d9202128d14846cc2e20d56fc8473ecb07cecbfb8086919f3971926e7045b853d85a69d026195c70"
        "f9f7a823536e2a8f4b3e12e94d9b53a934353451094b8103df3143a0057457d75e8c708b6337a6f5a4fd1a0672"
        "7acf9fb93e2993c62f3378b37d56c85e7b1e00f0145ebf8e4095bd723166293c60b6ac1252291ef65823c9e040"
        "ddad14969b3b340a4ef714db093a587c37766d68b8d6b5016e741587e7e6bf7e763b44f0247e64bae30f994d24"
        "8bfd20541a333e5b225ef6a61199e301738b1e688f70ec1d7fb892c183c95dc543c3e12adf8a5e8b9ca9d04f94"
        "45cced3ab256f29e998e69efaa633a7b60e1db5a867924ccab0a171d9d6e1098dfa15acde9553de599eaa56490"
        "c8f411e4985111f3d40bddfc5e301edb01547b01a886550a61158f7e2033c59707789bf7c854181d0c2e2a42a9"
        "3cf09209747d7082e147eb8544de25c3eb14f2e35559ea0c0f5877f2f3fc92132c0ae9da4e45b2f6c866a224ea"
        "6d1f28c05320e287750fbc647368d41116e528014cc1852e5531d53e4af938374daba6cee4baa821ed07117253"
        "bb3601ddd00d59a3d7fb2ef1f5a2fbba7c429f0cf9a5b3462410fd833a69118f8be9c559b1000cc608fd877fb4"
        "3f8e65c2d1302622b944462579056874b387208d90623fcdaf93920ca7a9e4ba64ea208758222ad868501cc2c3"
        "45e2d3a5ea2a17e5069248138c8a79c0251185d29ee73e5afab5354769142d2bf0cb6712727aa6bf84a6245fcd"
        "ae66e4938d84d1b9dd09a884818622080ff5f98942fb20acd7e0c916c2d5ea7ce6f7e173315384518f",
        "5a2664252aba2d6e19d9600da582cdd1f09d7a890ac48e6b8da15ae7c6ff1856fc67a841ac2314d283ffa3ca81"
        "a0ecf7c27d89ef91a5a893297928f5da0245c99645676b481b7e20a566ee6a4f2481942bee191deec5544600bb"
        "2441fd0fb19e2ee7d801ad8911c6b7750affec367a4b29a22942c0f5f4744a4e77a8b654da2a82571037099e9c"
        "6d930794efe5cdca73c7b6c0844e386bdca8ea01b3d7807146bb81365e2cdc6475f8c23e0ff84463126189dc97"
        "89f72bbce2e3d2d114d728a272f1345122de23df54c922ec7a16e5c2a8f84da8871482bd258c20a7c09bbcd64c"
        "7a96a51029bbfe848736a6ba7bf9d931a9b7de0bcaf3635034d4958b20ae9ab3a95a147b0421dd5f7ebff46c97"
        "1010ebfc4adbbe0ad94d5498c853e7142c450d8c71de4b2f84edbf8acd2e16d00c8115b150b1c30e553dbb8263"
        "5e781379fe2a56360420ff7e9f70cc64c00aba7e26ed13c7c19622865ae07248daced36416080f35f8cc157a85"
        "7ed70ea4f347f17d1bee80fa038abd6e39b1ba06b97264388b21364f7c56e192d4b62d9b161405f32ab1e2594e"
        "86243e56fcf2cb30d21adef15b9940f91af681da24328c883d892670c6aa47940867a81830a82b82716895db81"
        "0df1b834640abefb7db2092dd92912cb9a735175bc447be40a503cf22dfe565b4ed7a3293ca0dfd63a507430b3"
        "23ee248ec82e843b673c97ad730728cebc",
        "nagydani-4-qube",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000020000000000000000000000000000"
        "000000000000000000000000000000000000030000000000000000000000000000000000000000000000000000"
        "000000000200db34d0e438249c0ed685c949cc28776a05094e1c48691dc3f2dca5fc3356d2a0663bd376e47128"
        "39917eb9a19c670407e2c377a2de385a3ff3b52104f7f1f4e0c7bf7717fb913896693dc5edbb65b760ef1b00e4"
        "2e9d8f9af17352385e1cd742c9b006c0f669995cb0bb21d28c0aced2892267637b6470d8cee0ab27fc5d42658f"
        "6e88240c31d6774aa60a7ebd25cd48b56d0da11209f1928e61005c6eb709f3e8e0aaf8d9b10f7d7e296d772264"
        "dc76897ccdddadc91efa91c1903b7232a9e4c3b941917b99a3bc0c26497dedc897c25750af60237aa67934a26a"
        "2bc491db3dcc677491944bc1f51d3e5d76b8d846a62db03dedd61ff508f91a56d71028125035c3a44cbb041497"
        "c83bf3e4ae2a9613a401cc721c547a2afa3b16a2969933d3626ed6d8a7428648f74122fd3f2a02a20758f7f693"
        "892c8fd798b39abac01d18506c45e71432639e9f9505719ee822f62ccbf47f6850f096ff77b5afaf4be7d77202"
        "5791717dbe5abf9b3f40cff7d7aab6f67e38f62faf510747276e20a42127e7500c444f9ed92baf65ade9e83684"
        "5e39c4316d9dce5f8e2c8083e2c0acbb95296e05e51aab13b6b8f53f06c9c4276e12b0671133218cc3ea907da3"
        "bd9a367096d9202128d14846cc2e20d56fc8473ecb07cecbfb8086919f3971926e7045b853d85a69d026195c70"
        "f9f7a823536e2a8f4b3e12e94d9b53a934353451094b81010001df3143a0057457d75e8c708b6337a6f5a4fd1a"
        "06727acf9fb93e2993c62f3378b37d56c85e7b1e00f0145ebf8e4095bd723166293c60b6ac1252291ef65823c9"
        "e040ddad14969b3b340a4ef714db093a587c37766d68b8d6b5016e741587e7e6bf7e763b44f0247e64bae30f99"
        "4d248bfd20541a333e5b225ef6a61199e301738b1e688f70ec1d7fb892c183c95dc543c3e12adf8a5e8b9ca9d0"
        "4f9445cced3ab256f29e998e69efaa633a7b60e1db5a867924ccab0a171d9d6e1098dfa15acde9553de599eaa5"
        "6490c8f411e4985111f3d40bddfc5e301edb01547b01a886550a61158f7e2033c59707789bf7c854181d0c2e2a"
        "42a93cf09209747d7082e147eb8544de25c3eb14f2e35559ea0c0f5877f2f3fc92132c0ae9da4e45b2f6c866a2"
        "24ea6d1f28c05320e287750fbc647368d41116e528014cc1852e5531d53e4af938374daba6cee4baa821ed0711"
        "7253bb3601ddd00d59a3d7fb2ef1f5a2fbba7c429f0cf9a5b3462410fd833a69118f8be9c559b1000cc608fd87"
        "7fb43f8e65c2d1302622b944462579056874b387208d90623fcdaf93920ca7a9e4ba64ea208758222ad868501c"
        "c2c345e2d3a5ea2a17e5069248138c8a79c0251185d29ee73e5afab5354769142d2bf0cb6712727aa6bf84a624"
        "5fcdae66e4938d84d1b9dd09a884818622080ff5f98942fb20acd7e0c916c2d5ea7ce6f7e173315384518f",
        "bed8b970c4a34849fc6926b08e40e20b21c15ed68d18f228904878d4370b56322d0da5789da0318768a374758e"
        "6375bfe4641fca5285ec7171828922160f48f5ca7efbfee4d5148612c38ad683ae4e3c3a053d2b7c098cf2b34f"
        "2cb19146eadd53c86b2d7ccf3d83b2c370bfb840913ee3879b1057a6b4e07e110b6bcd5e958bc71a14798c91d5"
        "18cc70abee264b0d25a4110962a764b364ac0b0dd1ee8abc8426d775ec0f22b7e47b32576afaf1b5a48f64573e"
        "d1c5c29f50ab412188d9685307323d990802b81dacc06c6e05a1e901830ba9fcc67688dc29c5e27bde0a6e845c"
        "a925f5454b6fb3747edfaa2a5820838fb759eadf57f7cb5cec57fc213ddd8a4298fa079c3c0f472b07fb15aa6a"
        "7f0a3780bd296ff6a62e58ef443870b02260bd4fd2bbc98255674b8e1f1f9f8d33c7170b0ebbea4523b695911a"
        "bbf26e41885344823bd0587115fdd83b721a4e8457a31c9a84b3d3520a07e0e35df7f48e5a9d534d0ec7feef1f"
        "f74de6a11e7f93eab95175b6ce22c68d78a642ad642837897ec11349205d8593ac19300207572c38d29ca5dfa0"
        "3bc14cdbc32153c80e5cc3e739403d34c75915e49beb43094cc6dcafb3665b305ddec9286934ae66ec6b777ca5"
        "28728c851318eb0f207b39f1caaf96db6eeead6b55ed08f451939314577d42bcc9f97c0b52d0234f88fd07e4c1"
        "d7780fdebc025cfffcb572cb27a8c33963",
        "nagydani-4-pow0x10001",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000040000000000000000000000000000"
        "000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000"
        "000000000400c5a1611f8be90071a43db23cc2fe01871cc4c0e8ab5743f6378e4fef77f7f6db0095c0727e2022"
        "5beb665645403453e325ad5f9aeb9ba99bf3c148f63f9c07cf4fe8847ad5242d6b7d4499f93bd47056ddab8f7d"
        "ee878fc2314f344dbee2a7c41a5d3db91eff372c730c2fdd3a141a4b61999e36d549b9870cf2f4e632c4d5df5f"
        "024f81c028000073a0ed8847cfb0593d36a47142f578f05ccbe28c0c06aeb1b1da027794c48db880278f79ba78"
        "ae64eedfea3c07d10e0562668d839749dc95f40467d15cf65b9cfc52c7c4bcef1cda3596dd52631aac942f146c"
        "7cebd46065131699ce8385b0db1874336747ee020a5698a3d1a1082665721e769567f579830f9d259cec1a8368"
        "45109c21cf6b25da572512bf3c42fd4b96e43895589042ab60dd41f497db96aec102087fe784165bb45f942859"
        "268fd2ff6c012d9d00c02ba83eace047cc5f7b2c392c2955c58a49f0338d6fc58749c9db2155522ac17914ec21"
        "6ad87f12e0ee95574613942fa615898c4d9e8a3be68cd6afa4e7a003dedbdf8edfee31162b174f965b20ae752a"
        "d89c967b3068b6f722c16b354456ba8e280f987c08e0a52d40a2e8f3a59b94d590aeef01879eb7a90b3ee7d772"
        "c839c85519cbeaddc0c193ec4874a463b53fcaea3271d80ebfb39b33489365fc039ae549a17a9ff898eea2f4cb"
        "27b8dbee4c17b998438575b2b8d107e4a0d66ba7fca85b41a58a8d51f191a35c856dfbe8aef2b00048a694bbcc"
        "ff832d23c8ca7a7ff0b6c0b3011d00b97c86c0628444d267c951d9e4fb8f83e154b8f74fb51aa16535e498235c"
        "5597dac9606ed0be3173a3836baa4e7d756ffe1e2879b415d3846bccd538c05b847785699aefde3e305decb600"
        "cd8fb0e7d8de5efc26971a6ad4e6d7a2d91474f1023a0ac4b78dc937da0ce607a45974d2cac1c33a2631ff7fe6"
        "144a3b2e5cf98b531a9627dea92c1dc82204d09db0439b6a11dd64b484e1263aa45fd9539b6020b55e3baece39"
        "86a8bffc1003406348f5c61265099ed43a766ee4f93f5f9c5abbc32a0fd3ac2b35b87f9ec26037d88275bd7dd0"
        "a54474995ee34ed3727f3f97c48db544b1980193a4b76a8a3ddab3591ce527f16d91882e67f0103b5cda53f7da"
        "54d489fc4ac08b6ab358a5a04aa9daa16219d50bd672a7cb804ed769d218807544e5993f1c27427104b349906a"
        "0b654df0bf69328afd3013fbe430155339c39f236df5557bf92f1ded7ff609a8502f49064ec3d1dbfb6c15d3a4"
        "c11a4f8acd12278cbf68acd5709463d12e3338a6eddb8c112f199645e23154a8e60879d2a654e3ed9296aa28f1"
        "34168619691cd2c6b9e2eba4438381676173fc63c2588a3c5910dc149cf3760f0aa9fa9c3f5faa9162b0bf1aac"
        "9dd32b706a60ef53cbdb394b6b40222b5bc80eea82ba8958386672564cae3794f977871ab62337cf02e3004920"
        "1ec12937e7ce79d0f55d9c810e20acf52212aca1d3888949e0e4830aad88d804161230eb89d4d329cc83570fe2"
        "57217d2119134048dd2ed167646975fc7d77136919a049ea74cf08ddd2b896890bb24a0ba18094a22baa351bf2"
        "9ad96c66bbb1a598f2ca391749620e62d61c3561a7d3653ccc8892c7b99baaf76bf836e2991cb06d6bc0514568"
        "ff0d1ec8bb4b3d6984f5eaefb17d3ea2893722375d3ddb8e389a8eef7d7d198f8e687d6a513983df906099f9a2"
        "d23f4f9dec6f8ef2f11fc0a21fac45353b94e00486f5e17d386af42502d09db33cf0cf28310e049c07e88682ae"
        "eb00cb833c5174266e62407a57583f1f88b304b7c6e0c84bbe1c0fd423072d37a5bd0aacf764229e5c7cd02473"
        "460ba3645cd8e8ae144065bf02d0dd238593d8e230354f67e0b2f23012c23274f80e3ee31e35e2606a4a3f31d9"
        "4ab755e6d163cff52cbb36b6d0cc67ffc512aeed1dce4d7a0d70ce82f2baba12e8d514dc92a056f994adfb17b5"
        "b9712bd5186f27a2fda1f7039c5df2c8587fdc62f5627580c13234b55be4df3056050e2d1ef3218f0dd66cb052"
        "65fe1acfb0989d8213f2c19d1735a7cf3fa65d88dad5af52dc2bba22b7abf46c3bc77b5091baab9e8f0ddc4d5e"
        "581037de91a9f8dcbc69309be29cc815cf19a20a7585b8b3073edf51fc9baeb3e509b97fa4ecfd621e0fd57bd6"
        "1cac1b895c03248ff12bdbc57509250df3517e8a3fe1d776836b34ab352b973d932ef708b14f7418f9eceb1d87"
        "667e61e3e758649cb083f01b133d37ab2f5afa96d6c84bcacf4efc3851ad308c1e7d9113624fce29fab460ab9d"
        "2a48d92cdb281103a5250ad44cb2ff6e67ac670c02fdafb3e0f1353953d6d7d5646ca1568dea55275a050ec501"
        "b7c6250444f7219f1ba7521ba3b93d089727ca5f3bbe0d6c1300b423377004954c5628fdb65770b18ced5c9b23"
        "a4a5a6d6ef25fe01b4ce278de0bcc4ed86e28a0a68818ffa40970128cf2c38740e80037984428c1bd5113f40ff"
        "47512ee6f4e4d8f9b8e8e1b3040d2928d003bd1c1329dc885302fbce9fa81c23b4dc49c7c82d29b52957847898"
        "676c89aa5d32b5b0e1c0d5a2b79a19d67562f407f19425687971a957375879d90c5f57c857136c17106c9ab1b9"
        "9d80e69c8c954ed386493368884b55c939b8d64d26f643e800c56f90c01079d7c534e3b2b7ae352cefd3016da5"
        "5f6a85eb803b85e2304915fd2001f77c74e28746293c46e4f5f0fd49cf988aafd0026b8e7a3bab2da5cdce1ea2"
        "6c2e29ec03f4807fac432662b2d6c060be1c7be0e5489de69d0a6e03a4b9117f9244b34a0f1ecba89884f781c6"
        "320412413a00c4980287409a2a78c2cd7e65cecebbe4ec1c28cac4dd95f6998e78fc6f1392384331c9436aa10e"
        "10e2bf8ad2c4eafbcf276aa7bae64b74428911b3269c749338b0fc5075ad",
        "d61fe4e3f32ac260915b5b03b78a86d11bfc41d973fce5b0cc59035cf8289a8a2e3878ea15fa46565b0d806e2f"
        "85b53873ea20ed653869b688adf83f3ef444535bf91598ff7e80f334fb782539b92f39f55310cc4b35349ab7b2"
        "78346eda9bc37c0d8acd3557fae38197f412f8d9e57ce6a76b7205c23564cab06e5615be7c6f05c3d05ec690cb"
        "a91da5e89d55b152ff8dd2157dc5458190025cf94b1ad98f7cbe64e9482faba95e6b33844afc640892872b44a9"
        "932096508f4a782a4805323808f23e54b6ff9b841dbfa87db3505ae4f687972c18ea0f0d0af89d36c1c2a5b145"
        "60c153c3fee406f5cf15cfd1c0bb45d767426d465f2f14c158495069d0c5955a00150707862ecaae30624ebacd"
        "d8ac33e4e6aab3ff90b6ba445a84689386b9e945d01823a65874444316e83767290fcff630d2477f49d5d8ffdd"
        "200e08ee1274270f86ed14c687895f6caf5ce528bd970c20d2408a9ba66216324c6a011ac4999098362dbd98a0"
        "38129a2d40c8da6ab88318aa3046cb660327cc44236d9e5d2163bd0959062195c51ed93d0088b6f92051fc9905"
        "0ece2538749165976233697ab4b610385366e5ce0b02ad6b61c168ecfbedcdf74278a38de340fd7a5fead8e588"
        "e294795f9b011e2e60377a89e25c90e145397cdeabc60fd32444a6b7642a611a83c464d8b8976666351b4865c3"
        "7b02e6dc21dbcdf5f930341707b618cc0f03c3122646b3385c9df9f2ec730eec9d49e7dfc9153b6e6289da8c4f"
        "0ebea9ccc1b751948e3bb7171c9e4d57423b0eeeb79095c030cb52677b3f7e0b45c30f645391f3f9c957afa549"
        "c4e0b2465b03c67993cd200b1af01035962edbc4c9e89b31c82ac121987d6529dafdeef67a132dc04b6dc68e77"
        "f22862040b75e2ceb9ff16da0fca534e6db7bd12fa7b7f51b6c08c1e23dfcdb7acbd2da0b51c87ffbced065a61"
        "2e9b1c8bba9b7e2d8d7a2f04fcc4aaf355b60d764879a76b5e16762d5f2f55d585d0c8e82df6940960cddfb72c"
        "91dfa71f6b4e1c6ca25dfc39a878e998a663c04fe29d5e83b9586d047b4d7ff70a9f0d44f127e7d741685ca75f"
        "11629128d916a0ffef4be586a30c4b70389cc746e84ebf177c01ee8a4511cfbb9d1ecf7f7b33c7dd8177896e10"
        "bbc82f838dcd6db7ac67de62bf46b6a640fb580c5d1d2708f3862e3d2b645d0d18e49ef088053e3a220adc0e03"
        "3c2afcfe61c90e32151152eb3caaf746c5e377d541cafc6cbb0cc0fa48b5caf1728f2e1957f5addfc234f1a9d8"
        "9e40d49356c9172d0561a695fce6dab1d412321bbf407f63766ffd7b6b3d79bcfa07991c5a9709849c1008689e"
        "3b47c50d613980bec239fb64185249d055b30375ccb4354d71fe4d05648fbf6c80634dfc3575f2f24abb714c1e"
        "4c95e8896763bf4316e954c7ad19e5780ab7a040ca6fb9271f90a8b22ae738daf6cb",
        "nagydani-5-square",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000040000000000000000000000000000"
        "000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000"
        "000000000400c5a1611f8be90071a43db23cc2fe01871cc4c0e8ab5743f6378e4fef77f7f6db0095c0727e2022"
        "5beb665645403453e325ad5f9aeb9ba99bf3c148f63f9c07cf4fe8847ad5242d6b7d4499f93bd47056ddab8f7d"
        "ee878fc2314f344dbee2a7c41a5d3db91eff372c730c2fdd3a141a4b61999e36d549b9870cf2f4e632c4d5df5f"
        "024f81c028000073a0ed8847cfb0593d36a47142f578f05ccbe28c0c06aeb1b1da027794c48db880278f79ba78"
        "ae64eedfea3c07d10e0562668d839749dc95f40467d15cf65b9cfc52c7c4bcef1cda3596dd52631aac942f146c"
        "7cebd46065131699ce8385b0db1874336747ee020a5698a3d1a1082665721e769567f579830f9d259cec1a8368"
        "45109c21cf6b25da572512bf3c42fd4b96e43895589042ab60dd41f497db96aec102087fe784165bb45f942859"
        "268fd2ff6c012d9d00c02ba83eace047cc5f7b2c392c2955c58a49f0338d6fc58749c9db2155522ac17914ec21"
        "6ad87f12e0ee95574613942fa615898c4d9e8a3be68cd6afa4e7a003dedbdf8edfee31162b174f965b20ae752a"
        "d89c967b3068b6f722c16b354456ba8e280f987c08e0a52d40a2e8f3a59b94d590aeef01879eb7a90b3ee7d772"
        "c839c85519cbeaddc0c193ec4874a463b53fcaea3271d80ebfb39b33489365fc039ae549a17a9ff898eea2f4cb"
        "27b8dbee4c17b998438575b2b8d107e4a0d66ba7fca85b41a58a8d51f191a35c856dfbe8aef2b00048a694bbcc"
        "ff832d23c8ca7a7ff0b6c0b3011d00b97c86c0628444d267c951d9e4fb8f83e154b8f74fb51aa16535e498235c"
        "5597dac9606ed0be3173a3836baa4e7d756ffe1e2879b415d3846bccd538c05b847785699aefde3e305decb600"
        "cd8fb0e7d8de5efc26971a6ad4e6d7a2d91474f1023a0ac4b78dc937da0ce607a45974d2cac1c33a2631ff7fe6"
        "144a3b2e5cf98b531a9627dea92c1dc82204d09db0439b6a11dd64b484e1263aa45fd9539b6020b55e3baece39"
        "86a8bffc1003406348f5c61265099ed43a766ee4f93f5f9c5abbc32a0fd3ac2b35b87f9ec26037d88275bd7dd0"
        "a54474995ee34ed3727f3f97c48db544b1980193a4b76a8a3ddab3591ce527f16d91882e67f0103b5cda53f7da"
        "54d489fc4ac08b6ab358a5a04aa9daa16219d50bd672a7cb804ed769d218807544e5993f1c27427104b349906a"
        "0b654df0bf69328afd3013fbe430155339c39f236df5557bf92f1ded7ff609a8502f49064ec3d1dbfb6c15d3a4"
        "c11a4f8acd12278cbf68acd5709463d12e3338a6eddb8c112f199645e23154a8e60879d2a654e3ed9296aa28f1"
        "34168619691cd2c6b9e2eba4438381676173fc63c2588a3c5910dc149cf3760f0aa9fa9c3f5faa9162b0bf1aac"
        "9dd32b706a60ef53cbdb394b6b40222b5bc80eea82ba8958386672564cae3794f977871ab62337cf03e3004920"
        "1ec12937e7ce79d0f55d9c810e20acf52212aca1d3888949e0e4830aad88d804161230eb89d4d329cc83570fe2"
        "57217d2119134048dd2ed167646975fc7d77136919a049ea74cf08ddd2b896890bb24a0ba18094a22baa351bf2"
        "9ad96c66bbb1a598f2ca391749620e62d61c3561a7d3653ccc8892c7b99baaf76bf836e2991cb06d6bc0514568"
        "ff0d1ec8bb4b3d6984f5eaefb17d3ea2893722375d3ddb8e389a8eef7d7d198f8e687d6a513983df906099f9a2"
        "d23f4f9dec6f8ef2f11fc0a21fac45353b94e00486f5e17d386af42502d09db33cf0cf28310e049c07e88682ae"
        "eb00cb833c5174266e62407a57583f1f88b304b7c6e0c84bbe1c0fd423072d37a5bd0aacf764229e5c7cd02473"
        "460ba3645cd8e8ae144065bf02d0dd238593d8e230354f67e0b2f23012c23274f80e3ee31e35e2606a4a3f31d9"
        "4ab755e6d163cff52cbb36b6d0cc67ffc512aeed1dce4d7a0d70ce82f2baba12e8d514dc92a056f994adfb17b5"
        "b9712bd5186f27a2fda1f7039c5df2c8587fdc62f5627580c13234b55be4df3056050e2d1ef3218f0dd66cb052"
        "65fe1acfb0989d8213f2c19d1735a7cf3fa65d88dad5af52dc2bba22b7abf46c3bc77b5091baab9e8f0ddc4d5e"
        "581037de91a9f8dcbc69309be29cc815cf19a20a7585b8b3073edf51fc9baeb3e509b97fa4ecfd621e0fd57bd6"
        "1cac1b895c03248ff12bdbc57509250df3517e8a3fe1d776836b34ab352b973d932ef708b14f7418f9eceb1d87"
        "667e61e3e758649cb083f01b133d37ab2f5afa96d6c84bcacf4efc3851ad308c1e7d9113624fce29fab460ab9d"
        "2a48d92cdb281103a5250ad44cb2ff6e67ac670c02fdafb3e0f1353953d6d7d5646ca1568dea55275a050ec501"
        "b7c6250444f7219f1ba7521ba3b93d089727ca5f3bbe0d6c1300b423377004954c5628fdb65770b18ced5c9b23"
        "a4a5a6d6ef25fe01b4ce278de0bcc4ed86e28a0a68818ffa40970128cf2c38740e80037984428c1bd5113f40ff"
        "47512ee6f4e4d8f9b8e8e1b3040d2928d003bd1c1329dc885302fbce9fa81c23b4dc49c7c82d29b52957847898"
        "676c89aa5d32b5b0e1c0d5a2b79a19d67562f407f19425687971a957375879d90c5f57c857136c17106c9ab1b9"
        "9d80e69c8c954ed386493368884b55c939b8d64d26f643e800c56f90c01079d7c534e3b2b7ae352cefd3016da5"
        "5f6a85eb803b85e2304915fd2001f77c74e28746293c46e4f5f0fd49cf988aafd0026b8e7a3bab2da5cdce1ea2"
        "6c2e29ec03f4807fac432662b2d6c060be1c7be0e5489de69d0a6e03a4b9117f9244b34a0f1ecba89884f781c6"
        "320412413a00c4980287409a2a78c2cd7e65cecebbe4ec1c28cac4dd95f6998e78fc6f1392384331c9436aa10e"
        "10e2bf8ad2c4eafbcf276aa7bae64b74428911b3269c749338b0fc5075ad",
        "5f9c70ec884926a89461056ad20ac4c30155e817f807e4d3f5bb743d789c83386762435c3627773fa77da51444"
        "51f2a8aad8adba88e0b669f5377c5e9bad70e45c86fe952b613f015a9953b8a5de5eaee4566acf98d41e327d93"
        "a35bd5cef4607d025e58951167957df4ff9b1627649d3943805472e5e293d3efb687cfd1e503faafeb2840a3e3"
        "b3f85d016051a58e1c9498aab72e63b748d834b31eb05d85dcde65e27834e266b85c75cc4ec0135135e0601cb9"
        "3eeeb6e0010c8ceb65c4c319623c5e573a2c8c9fbbf7df68a930beb412d3f4dfd146175484f45d7afaa0d2e606"
        "84af9b34730f7c8438465ad3e1d0c3237336722f2aa51095bd5759f4b8ab4dda111b684aa3dac62a761722e7ae"
        "43495b7709933512c81c4e3c9133a51f7ce9f2b51fcec064f65779666960b4e45df3900f54311f5613e8012dd1"
        "b8efd359eda31a778264c72aa8bb419d862734d769076bce2810011989a45374e5c5d8729fec21427f0bf397ea"
        "cbb4220f603cf463a4b0c94efd858ffd9768cd60d6ce68d755e0fbad007ce5c2223d70c7018345a102e4ab3c60"
        "a13a9e7794303156d4c2063e919f2153c13961fb324c80b240742f47773a7a8e25b3e3fb19b00ce839346c6eb3"
        "c732fbc6b888df0b1fe0a3d07b053a2e9402c267b2d62f794d8a2840526e3ade15ce2264496ccd7519571dfde4"
        "7f7a4bb16292241c20b2be59f3f8fb4f6383f232d838c5a22d8c95b6834d9d2ca493f5a505ebe8899503b0e8f9"
        "b19e6e2dd81c1628b80016d02097e0134de51054c4e7674824d4d758760fc52377d2cad145e259aa2ffaf54139"
        "e1a66b1e0c1c191e32ac59474c6b526f5b3ba07d3e5ec286eddf531fcd5292869be58c9f22ef91026159f7cf9d"
        "05ef66b4299f4da48cc1635bf2243051d342d378a22c83390553e873713c0454ce5f3234397111ac3fe3207b86"
        "f0ed9fc025c81903e1748103692074f83824fda6341be4f95ff00b0a9a208c267e12fa01825054cc0513629bf3"
        "dbb56dc5b90d4316f87654a8be18227978ea0a8a522760cad620d0d14fd38920fb7321314062914275a5f99f67"
        "7145a6979b156bd82ecd36f23f8e1273cc2759ecc0b2c69d94dad5211d1bed939dd87ed9e07b91d49713a6e16a"
        "de0a98aea789f04994e318e4ff2c8a188cd8d43aeb52c6daa3bc29b4af50ea82a247c5cd67b573b34cbadcc0a3"
        "76d3bbd530d50367b42705d870f2e27a8197ef46070528bfe408360faa2ebb8bf76e9f388572842bcb119f4d84"
        "ee34ae31f5cc594f23705a49197b181fb78ed1ec99499c690f843a4d0cf2e226d118e9372271054fbabdcc5c92"
        "ae9fefaef0589cd0e722eaf30c1703ec4289c7fd81beaa8a455ccee5298e31e2080c10c366a6fcf56f7d13582a"
        "d0bcad037c612b710fc595b70fbefaaca23623b60c6c39b11beb8e5843b6b3dac60f",
        "nagydani-5-qube",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000040000000000000000000000000000"
        "000000000000000000000000000000000000030000000000000000000000000000000000000000000000000000"
        "000000000400c5a1611f8be90071a43db23cc2fe01871cc4c0e8ab5743f6378e4fef77f7f6db0095c0727e2022"
        "5beb665645403453e325ad5f9aeb9ba99bf3c148f63f9c07cf4fe8847ad5242d6b7d4499f93bd47056ddab8f7d"
        "ee878fc2314f344dbee2a7c41a5d3db91eff372c730c2fdd3a141a4b61999e36d549b9870cf2f4e632c4d5df5f"
        "024f81c028000073a0ed8847cfb0593d36a47142f578f05ccbe28c0c06aeb1b1da027794c48db880278f79ba78"
        "ae64eedfea3c07d10e0562668d839749dc95f40467d15cf65b9cfc52c7c4bcef1cda3596dd52631aac942f146c"
        "7cebd46065131699ce8385b0db1874336747ee020a5698a3d1a1082665721e769567f579830f9d259cec1a8368"
        "45109c21cf6b25da572512bf3c42fd4b96e43895589042ab60dd41f497db96aec102087fe784165bb45f942859"
        "268fd2ff6c012d9d00c02ba83eace047cc5f7b2c392c2955c58a49f0338d6fc58749c9db2155522ac17914ec21"
        "6ad87f12e0ee95574613942fa615898c4d9e8a3be68cd6afa4e7a003dedbdf8edfee31162b174f965b20ae752a"
        "d89c967b3068b6f722c16b354456ba8e280f987c08e0a52d40a2e8f3a59b94d590aeef01879eb7a90b3ee7d772"
        "c839c85519cbeaddc0c193ec4874a463b53fcaea3271d80ebfb39b33489365fc039ae549a17a9ff898eea2f4cb"
        "27b8dbee4c17b998438575b2b8d107e4a0d66ba7fca85b41a58a8d51f191a35c856dfbe8aef2b00048a694bbcc"
        "ff832d23c8ca7a7ff0b6c0b3011d00b97c86c0628444d267c951d9e4fb8f83e154b8f74fb51aa16535e498235c"
        "5597dac9606ed0be3173a3836baa4e7d756ffe1e2879b415d3846bccd538c05b847785699aefde3e305decb600"
        "cd8fb0e7d8de5efc26971a6ad4e6d7a2d91474f1023a0ac4b78dc937da0ce607a45974d2cac1c33a2631ff7fe6"
        "144a3b2e5cf98b531a9627dea92c1dc82204d09db0439b6a11dd64b484e1263aa45fd9539b6020b55e3baece39"
        "86a8bffc1003406348f5c61265099ed43a766ee4f93f5f9c5abbc32a0fd3ac2b35b87f9ec26037d88275bd7dd0"
        "a54474995ee34ed3727f3f97c48db544b1980193a4b76a8a3ddab3591ce527f16d91882e67f0103b5cda53f7da"
        "54d489fc4ac08b6ab358a5a04aa9daa16219d50bd672a7cb804ed769d218807544e5993f1c27427104b349906a"
        "0b654df0bf69328afd3013fbe430155339c39f236df5557bf92f1ded7ff609a8502f49064ec3d1dbfb6c15d3a4"
        "c11a4f8acd12278cbf68acd5709463d12e3338a6eddb8c112f199645e23154a8e60879d2a654e3ed9296aa28f1"
        "34168619691cd2c6b9e2eba4438381676173fc63c2588a3c5910dc149cf3760f0aa9fa9c3f5faa9162b0bf1aac"
        "9dd32b706a60ef53cbdb394b6b40222b5bc80eea82ba8958386672564cae3794f977871ab62337cf010001e300"
        "49201ec12937e7ce79d0f55d9c810e20acf52212aca1d3888949e0e4830aad88d804161230eb89d4d329cc8357"
        "0fe257217d2119134048dd2ed167646975fc7d77136919a049ea74cf08ddd2b896890bb24a0ba18094a22baa35"
        "1bf29ad96c66bbb1a598f2ca391749620e62d61c3561a7d3653ccc8892c7b99baaf76bf836e2991cb06d6bc051"
        "4568ff0d1ec8bb4b3d6984f5eaefb17d3ea2893722375d3ddb8e389a8eef7d7d198f8e687d6a513983df906099"
        "f9a2d23f4f9dec6f8ef2f11fc0a21fac45353b94e00486f5e17d386af42502d09db33cf0cf28310e049c07e886"
        "82aeeb00cb833c5174266e62407a57583f1f88b304b7c6e0c84bbe1c0fd423072d37a5bd0aacf764229e5c7cd0"
        "2473460ba3645cd8e8ae144065bf02d0dd238593d8e230354f67e0b2f23012c23274f80e3ee31e35e2606a4a3f"
        "31d94ab755e6d163cff52cbb36b6d0cc67ffc512aeed1dce4d7a0d70ce82f2baba12e8d514dc92a056f994adfb"
        "17b5b9712bd5186f27a2fda1f7039c5df2c8587fdc62f5627580c13234b55be4df3056050e2d1ef3218f0dd66c"
        "b05265fe1acfb0989d8213f2c19d1735a7cf3fa65d88dad5af52dc2bba22b7abf46c3bc77b5091baab9e8f0ddc"
        "4d5e581037de91a9f8dcbc69309be29cc815cf19a20a7585b8b3073edf51fc9baeb3e509b97fa4ecfd621e0fd5"
        "7bd61cac1b895c03248ff12bdbc57509250df3517e8a3fe1d776836b34ab352b973d932ef708b14f7418f9eceb"
        "1d87667e61e3e758649cb083f01b133d37ab2f5afa96d6c84bcacf4efc3851ad308c1e7d9113624fce29fab460"
        "ab9d2a48d92cdb281103a5250ad44cb2ff6e67ac670c02fdafb3e0f1353953d6d7d5646ca1568dea55275a050e"
        "c501b7c6250444f7219f1ba7521ba3b93d089727ca5f3bbe0d6c1300b423377004954c5628fdb65770b18ced5c"
        "9b23a4a5a6d6ef25fe01b4ce278de0bcc4ed86e28a0a68818ffa40970128cf2c38740e80037984428c1bd5113f"
        "40ff47512ee6f4e4d8f9b8e8e1b3040d2928d003bd1c1329dc885302fbce9fa81c23b4dc49c7c82d29b5295784"
        "7898676c89aa5d32b5b0e1c0d5a2b79a19d67562f407f19425687971a957375879d90c5f57c857136c17106c9a"
        "b1b99d80e69c8c954ed386493368884b55c939b8d64d26f643e800c56f90c01079d7c534e3b2b7ae352cefd301"
        "6da55f6a85eb803b85e2304915fd2001f77c74e28746293c46e4f5f0fd49cf988aafd0026b8e7a3bab2da5cdce"
        "1ea26c2e29ec03f4807fac432662b2d6c060be1c7be0e5489de69d0a6e03a4b9117f9244b34a0f1ecba89884f7"
        "81c6320412413a00c4980287409a2a78c2cd7e65cecebbe4ec1c28cac4dd95f6998e78fc6f1392384331c9436a"
        "a10e10e2bf8ad2c4eafbcf276aa7bae64b74428911b3269c749338b0fc5075ad",
        "5a0eb2bdf0ac1cae8e586689fa16cd4b07dfdedaec8a110ea1fdb059dd5253231b6132987598dfc6e11f867804"
        "28982d50cf68f67ae452622c3b336b537ef3298ca645e8f89ee39a26758206a5a3f6409afc709582f95274b57b"
        "71fae5c6b74619ae6f089a5393c5b79235d9caf699d23d88fb873f78379690ad8405e34c19f5257d596580c7a6"
        "a7206a3712825afe630c76b31cdb4a23e7f0632e10f14f4e282c81a66451a26f8df2a352b5b9f607a7198449d1"
        "b926e27036810368e691a74b91c61afa73d9d3b99453e7c8b50fd4f09c039a2f2feb5c419206694c31b92df1d9"
        "586140cb3417b38d0c503c7b508cc2ed12e813a1c795e9829eb39ee78eeaf360a169b491a1d4e419574e712402"
        "de9d48d54c1ae5e03739b7156615e8267e1fb0a897f067afd11fb33f6e24182d7aaaaa18fe5bc1982f20d6b871"
        "e5a398f0f6f718181d31ec225cfa9a0a70124ed9a70031bdf0c1c7829f708b6e17d50419ef361cf77d99c85f44"
        "607186c8d683106b8bd38a49b5d0fb503b397a83388c5678dcfcc737499d84512690701ed621a6f0172aecf037"
        "184ddf0f2453e4053024018e5ab2e30d6d5363b56e8b41509317c99042f517247474ab3abc848e00a07f69c254"
        "f46f2a05cf6ed84e5cc906a518fdcfdf2c61ce731f24c5264f1a25fc04934dc28aec112134dd523f70115074ca"
        "34e3807aa4cb925147f3a0ce152d323bd8c675ace446d0fd1ae30c4b57f0eb2c23884bc18f0964c0114796c5b6"
        "d080c3d89175665fbf63a6381a6a9da39ad070b645c8bb1779506da14439a9f5b5d481954764ea114fac688930"
        "bc68534d403cff4210673b6a6ff7ae416b7cd41404c3d3f282fcd193b86d0f54d0006c2a503b40d5c3930da980"
        "565b8f9630e9493a79d1c03e74e5f93ac8e4dc1a901ec5e3b3e57049124c7b72ea345aa359e782285d9e6a5c14"
        "4a378111dd02c40855ff9c2be9b48425cb0b2fd62dc8678fd151121cf26a65e917d65d8e0dacfae108eb5508b6"
        "01fb8ffa370be1f9a8b749a2d12eeab81f41079de87e2d777994fa4d28188c579ad327f9957fb7bdecec5c6808"
        "44dd43cb57cf87aeb763c003e65011f73f8c63442df39a92b946a6bd968a1c1e4d5fa7d88476a68bd8e20e5b70"
        "a99259c7d3f85fb1b65cd2e93972e6264e74ebf289b8b6979b9b68a85cd5b360c1987f87235c3c845d62489e33"
        "acf85d53fa3561fe3a3aee18924588d9c6eba4edb7a4d106b31173e42929f6f0c48c80ce6a72d54eca7c0fe870"
        "068b7a7c89c63cdda593f5b32d3cb4ea8a32c39f00ab449155757172d66763ed9527019d6de6c9f2416aa6203f"
        "4d11c9ebee1e1d3845099e55504446448027212616167eb36035726daa7698b075286f5379cd3e93cb3e0cf4f9"
        "cb8d017facbb5550ed32d5ec5400ae57e47e2bf78d1eaeff9480cc765ceff39db500",
        "nagydani-5-pow0x10001",
    }};

constexpr PrecompiledTest bn256AddTests[] = {
    {"18b18acfb4c2c30276db5411368e7185b311dd124691610c5d3b74034e093dc9063c909c4720840cb5134cb9f59fa"
     "749755796819658d32efc0d288198f3726607c2b7f58a84bd6145f00c9c2bc0bb1a187f20ff2c92963a88019e7c6a"
     "014eed06614e20c147e940f2d70da3f74c9a17df361706a4485c742bd6788478fa17d7",
        "2243525c5efd4b9c3d3c45ac0ca3fe4dd85e830a4ce6b65fa1eeaee202839703301d1d33be6da8e509df21cc35"
        "964723180eed7532537db9ae5e7d48f195c915",
        "chfast1"},
    {
        "2243525c5efd4b9c3d3c45ac0ca3fe4dd85e830a4ce6b65fa1eeaee202839703301d1d33be6da8e509df21cc35"
        "964723180eed7532537db9ae5e7d48f195c91518b18acfb4c2c30276db5411368e7185b311dd124691610c5d3b"
        "74034e093dc9063c909c4720840cb5134cb9f59fa749755796819658d32efc0d288198f37266",
        "2bd3e6d0f3b142924f5ca7b49ce5b9d54c4703d7ae5648e61d02268b1a0a9fb721611ce0a6af85915e2f1d7030"
        "0909ce2e49dfad4a4619c8390cae66cefdb204",
        "chfast2",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000000000",
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "00000000000000000000000000000000000000",
        "cdetrio1",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "00000000000000000000000000000000000000",
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "00000000000000000000000000000000000000",
        "cdetrio2",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000",
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "00000000000000000000000000000000000000",
        "cdetrio3",
    },
    {
        "",
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "00000000000000000000000000000000000000",
        "cdetrio4",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000",
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "00000000000000000000000000000000000000",
        "cdetrio5",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000010000000000000000000000000000000000000000000000000000000000000002",
        "000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000"
        "00000000000000000000000000000000000002",
        "cdetrio6",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000001000000000000000000000000000000000000000000000000000000000000000200000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000",
        "000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000"
        "00000000000000000000000000000000000002",
        "cdetrio7",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000"
        "00000000000000000000000000000000000002",
        "000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000"
        "00000000000000000000000000000000000002",
        "cdetrio8",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000"
        "000000000000000000000000000000000000020000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000000000",
        "000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000"
        "00000000000000000000000000000000000002",
        "cdetrio9",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000"
        "000000000000000000000000000000000000020000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000",
        "000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000"
        "00000000000000000000000000000000000002",
        "cdetrio10",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000"
        "000000000000000000000000000000000000020000000000000000000000000000000000000000000000000000"
        "0000000000010000000000000000000000000000000000000000000000000000000000000002",
        "030644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd315ed738c0e0a7c92e7845f96b2"
        "ae9c0a68a6a449e3538fc7ff3ebf7a5a18a2c4",
        "cdetrio11",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000"
        "000000000000000000000000000000000000020000000000000000000000000000000000000000000000000000"
        "000000000001000000000000000000000000000000000000000000000000000000000000000200000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000",
        "030644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd315ed738c0e0a7c92e7845f96b2"
        "ae9c0a68a6a449e3538fc7ff3ebf7a5a18a2c4",
        "cdetrio12",
    },
    {
        "17c139df0efee0f766bc0204762b774362e4ded88953a39ce849a8a7fa163fa901e0559bacb160664764a357af"
        "8a9fe70baa9258e0b959273ffc5718c6d4cc7c039730ea8dff1254c0fee9c0ea777d29a9c710b7e616683f194f"
        "18c43b43b869073a5ffcc6fc7a28c30723d6e58ce577356982d65b833a5a5c15bf9024b43d98",
        "15bf2bb17880144b5d1cd2b1f46eff9d617bffd1ca57c37fb5a49bd84e53cf66049c797f9ce0d17083deb32b5e"
        "36f2ea2a212ee036598dd7624c168993d1355f",
        "cdetrio13",
    },
    {
        "17c139df0efee0f766bc0204762b774362e4ded88953a39ce849a8a7fa163fa901e0559bacb160664764a357af"
        "8a9fe70baa9258e0b959273ffc5718c6d4cc7c17c139df0efee0f766bc0204762b774362e4ded88953a39ce849"
        "a8a7fa163fa92e83f8d734803fc370eba25ed1f6b8768bd6d83887b87165fc2434fe11a830cb00000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000",
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "00000000000000000000000000000000000000",
        "cdetrio14",
    }};

constexpr PrecompiledTest bn256ScalarMulTests[] = {
    {
        "2bd3e6d0f3b142924f5ca7b49ce5b9d54c4703d7ae5648e61d02268b1a0a9fb721611ce0a6af85915e2f1d7030"
        "0909ce2e49dfad4a4619c8390cae66cefdb2040000000000000000000000000000000000000000000000001113"
        "8ce750fa15c2",
        "070a8d6a982153cae4be29d434e8faef8a47b274a053f5a4ee2a6c9c13c31e5c031b8ce914eba3a9ffb989f9cd"
        "d5b0f01943074bf4f0f315690ec3cec6981afc",
        "chfast1",
    },
    {
        "070a8d6a982153cae4be29d434e8faef8a47b274a053f5a4ee2a6c9c13c31e5c031b8ce914eba3a9ffb989f9cd"
        "d5b0f01943074bf4f0f315690ec3cec6981afc30644e72e131a029b85045b68181585d97816a916871ca8d3c20"
        "8c16d87cfd46",
        "025a6f4181d2b4ea8b724290ffb40156eb0adb514c688556eb79cdea0752c2bb2eff3f31dea215f1eb86023a13"
        "3a996eb6300b44da664d64251d05381bb8a02e",
        "chfast2",
    },
    {
        "025a6f4181d2b4ea8b724290ffb40156eb0adb514c688556eb79cdea0752c2bb2eff3f31dea215f1eb86023a13"
        "3a996eb6300b44da664d64251d05381bb8a02e183227397098d014dc2822db40c0ac2ecbc0b548b438e5469e10"
        "460b6c3e7ea3",
        "14789d0d4a730b354403b5fac948113739e276c23e0258d8596ee72f9cd9d3230af18a63153e0ec25ff9f2951d"
        "d3fa90ed0197bfef6e2a1a62b5095b9d2b4a27",
        "chfast3",
    },
    {
        "1a87b0584ce92f4593d161480614f2989035225609f08058ccfa3d0f940febe31a2f3c951f6dadcc7ee9007dff"
        "81504b0fcd6d7cf59996efdc33d92bf7f9f8f6ffffffffffffffffffffffffffffffffffffffffffffffffffff"
        "ffffffffffff",
        "2cde5879ba6f13c0b5aa4ef627f159a3347df9722efce88a9afbb20b763b4c411aa7e43076f6aee272755a7f9b"
        "84832e71559ba0d2e0b17d5f9f01755e5b0d11",
        "cdetrio1",
    },
    {
        "1a87b0584ce92f4593d161480614f2989035225609f08058ccfa3d0f940febe31a2f3c951f6dadcc7ee9007dff"
        "81504b0fcd6d7cf59996efdc33d92bf7f9f8f630644e72e131a029b85045b68181585d2833e84879b9709143e1"
        "f593f0000000",
        "1a87b0584ce92f4593d161480614f2989035225609f08058ccfa3d0f940febe3163511ddc1c3f25d3967453882"
        "00081287b3fd1472d8339d5fecb2eae0830451",
        "cdetrio2",
    },
    {
        "1a87b0584ce92f4593d161480614f2989035225609f08058ccfa3d0f940febe31a2f3c951f6dadcc7ee9007dff"
        "81504b0fcd6d7cf59996efdc33d92bf7f9f8f60000000000000000000000000000000100000000000000000000"
        "000000000000",
        "1051acb0700ec6d42a88215852d582efbaef31529b6fcbc3277b5c1b300f5cf0135b2394bb45ab04b8bd7611bd"
        "2dfe1de6a4e6e2ccea1ea1955f577cd66af85b",
        "cdetrio3",
    },
    {
        "1a87b0584ce92f4593d161480614f2989035225609f08058ccfa3d0f940febe31a2f3c951f6dadcc7ee9007dff"
        "81504b0fcd6d7cf59996efdc33d92bf7f9f8f60000000000000000000000000000000000000000000000000000"
        "000000000009",
        "1dbad7d39dbc56379f78fac1bca147dc8e66de1b9d183c7b167351bfe0aeab742cd757d51289cd8dbd0acf9e67"
        "3ad67d0f0a89f912af47ed1be53664f5692575",
        "cdetrio4",
    },
    {
        "1a87b0584ce92f4593d161480614f2989035225609f08058ccfa3d0f940febe31a2f3c951f6dadcc7ee9007dff"
        "81504b0fcd6d7cf59996efdc33d92bf7f9f8f60000000000000000000000000000000000000000000000000000"
        "000000000001",
        "1a87b0584ce92f4593d161480614f2989035225609f08058ccfa3d0f940febe31a2f3c951f6dadcc7ee9007dff"
        "81504b0fcd6d7cf59996efdc33d92bf7f9f8f6",
        "cdetrio5",
    },
    {
        "17c139df0efee0f766bc0204762b774362e4ded88953a39ce849a8a7fa163fa901e0559bacb160664764a357af"
        "8a9fe70baa9258e0b959273ffc5718c6d4cc7cffffffffffffffffffffffffffffffffffffffffffffffffffff"
        "ffffffffffff",
        "29e587aadd7c06722aabba753017c093f70ba7eb1f1c0104ec0564e7e3e21f6022b1143f6a41008e7755c71c3d"
        "00b6b915d386de21783ef590486d8afa8453b1",
        "cdetrio6",
    },
    {
        "17c139df0efee0f766bc0204762b774362e4ded88953a39ce849a8a7fa163fa901e0559bacb160664764a357af"
        "8a9fe70baa9258e0b959273ffc5718c6d4cc7c30644e72e131a029b85045b68181585d2833e84879b9709143e1"
        "f593f0000000",
        "17c139df0efee0f766bc0204762b774362e4ded88953a39ce849a8a7fa163fa92e83f8d734803fc370eba25ed1"
        "f6b8768bd6d83887b87165fc2434fe11a830cb",
        "cdetrio7",
    },
    {
        "17c139df0efee0f766bc0204762b774362e4ded88953a39ce849a8a7fa163fa901e0559bacb160664764a357af"
        "8a9fe70baa9258e0b959273ffc5718c6d4cc7c0000000000000000000000000000000100000000000000000000"
        "000000000000",
        "221a3577763877920d0d14a91cd59b9479f83b87a653bb41f82a3f6f120cea7c2752c7f64cdd7f0e494bff7b60"
        "419f242210f2026ed2ec70f89f78a4c56a1f15",
        "cdetrio8",
    },
    {
        "17c139df0efee0f766bc0204762b774362e4ded88953a39ce849a8a7fa163fa901e0559bacb160664764a357af"
        "8a9fe70baa9258e0b959273ffc5718c6d4cc7c0000000000000000000000000000000000000000000000000000"
        "000000000009",
        "228e687a379ba154554040f8821f4e41ee2be287c201aa9c3bc02c9dd12f1e691e0fd6ee672d04cfd924ed8fdc"
        "7ba5f2d06c53c1edc30f65f2af5a5b97f0a76a",
        "cdetrio9",
    },
    {
        "17c139df0efee0f766bc0204762b774362e4ded88953a39ce849a8a7fa163fa901e0559bacb160664764a357af"
        "8a9fe70baa9258e0b959273ffc5718c6d4cc7c0000000000000000000000000000000000000000000000000000"
        "000000000001",
        "17c139df0efee0f766bc0204762b774362e4ded88953a39ce849a8a7fa163fa901e0559bacb160664764a357af"
        "8a9fe70baa9258e0b959273ffc5718c6d4cc7c",
        "cdetrio10",
    },
    {
        "039730ea8dff1254c0fee9c0ea777d29a9c710b7e616683f194f18c43b43b869073a5ffcc6fc7a28c30723d6e5"
        "8ce577356982d65b833a5a5c15bf9024b43d98ffffffffffffffffffffffffffffffffffffffffffffffffffff"
        "ffffffffffff",
        "00a1a234d08efaa2616607e31eca1980128b00b415c845ff25bba3afcb81dc00242077290ed33906aeb8e42fd9"
        "8c41bcb9057ba03421af3f2d08cfc441186024",
        "cdetrio11",
    },
    {
        "039730ea8dff1254c0fee9c0ea777d29a9c710b7e616683f194f18c43b43b869073a5ffcc6fc7a28c30723d6e5"
        "8ce577356982d65b833a5a5c15bf9024b43d9830644e72e131a029b85045b68181585d2833e84879b9709143e1"
        "f593f0000000",
        "039730ea8dff1254c0fee9c0ea777d29a9c710b7e616683f194f18c43b43b8692929ee761a352600f54921df9b"
        "f472e66217e7bb0cee9032e00acc86b3c8bfaf",
        "cdetrio12",
    },
    {
        "039730ea8dff1254c0fee9c0ea777d29a9c710b7e616683f194f18c43b43b869073a5ffcc6fc7a28c30723d6e5"
        "8ce577356982d65b833a5a5c15bf9024b43d980000000000000000000000000000000100000000000000000000"
        "000000000000",
        "1071b63011e8c222c5a771dfa03c2e11aac9666dd097f2c620852c3951a4376a2f46fe2f73e1cf310a168d56ba"
        "a5575a8319389d7bfa6b29ee2d908305791434",
        "cdetrio13",
    },
    {
        "039730ea8dff1254c0fee9c0ea777d29a9c710b7e616683f194f18c43b43b869073a5ffcc6fc7a28c30723d6e5"
        "8ce577356982d65b833a5a5c15bf9024b43d980000000000000000000000000000000000000000000000000000"
        "000000000009",
        "19f75b9dd68c080a688774a6213f131e3052bd353a304a189d7a2ee367e3c2582612f545fb9fc89fde80fd81c6"
        "8fc7dcb27fea5fc124eeda69433cf5c46d2d7f",
        "cdetrio14",
    },
    {
        "039730ea8dff1254c0fee9c0ea777d29a9c710b7e616683f194f18c43b43b869073a5ffcc6fc7a28c30723d6e5"
        "8ce577356982d65b833a5a5c15bf9024b43d980000000000000000000000000000000000000000000000000000"
        "000000000001",
        "039730ea8dff1254c0fee9c0ea777d29a9c710b7e616683f194f18c43b43b869073a5ffcc6fc7a28c30723d6e5"
        "8ce577356982d65b833a5a5c15bf9024b43d98",
        "cdetrio15",
    }};

// bn256PairingTests are the test and benchmark data for the bn256 pairing check
// precompiled contract.
constexpr PrecompiledTest bn256PairingTests[] = {
    {
        "1c76476f4def4bb94541d57ebba1193381ffa7aa76ada664dd31c16024c43f593034dd2920f673e204fee2811c"
        "678745fc819b55d3e9d294e45c9b03a76aef41209dd15ebff5d46c4bd888e51a93cf99a7329636c63514396b4a"
        "452003a35bf704bf11ca01483bfa8b34b43561848d28905960114c8ac04049af4b6315a416782bb8324af6cfc9"
        "3537a2ad1a445cfd0ca2a71acd7ac41fadbf933c2a51be344d120a2a4cf30c1bf9845f20c6fe39e07ea2cce61f"
        "0c9bb048165fe5e4de877550111e129f1cf1097710d41c4ac70fcdfa5ba2023c6ff1cbeac322de49d1b6df7c20"
        "32c61a830e3c17286de9462bf242fca2883585b93870a73853face6a6bf411198e9393920d483a7260bfb731fb"
        "5d25f1aa493335a9e71297e485b7aef312c21800deef121f1e76426a00665e5c4479674322d4f75edadd46debd"
        "5cd992f6ed090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b12c85ea5db8c6deb"
        "4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa",
        "0000000000000000000000000000000000000000000000000000000000000001",
        "jeff1",
    },
    {
        "2eca0c7238bf16e83e7a1e6c5d49540685ff51380f309842a98561558019fc0203d3260361bb8451de5ff5ecd1"
        "7f010ff22f5c31cdf184e9020b06fa5997db841213d2149b006137fcfb23036606f848d638d576a120ca981b5b"
        "1a5f9300b3ee2276cf730cf493cd95d64677bbb75fc42db72513a4c1e387b476d056f80aa75f21ee6226d31426"
        "322afcda621464d0611d226783262e21bb3bc86b537e986237096df1f82dff337dd5972e32a8ad43e28a78a96a"
        "823ef1cd4debe12b6552ea5f06967a1237ebfeca9aaae0d6d0bab8e28c198c5a339ef8a2407e31cdac516db922"
        "160fa257a5fd5b280642ff47b65eca77e626cb685c84fa6d3b6882a283ddd1198e9393920d483a7260bfb731fb"
        "5d25f1aa493335a9e71297e485b7aef312c21800deef121f1e76426a00665e5c4479674322d4f75edadd46debd"
        "5cd992f6ed090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b12c85ea5db8c6deb"
        "4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa",
        "0000000000000000000000000000000000000000000000000000000000000001",
        "jeff2",
    },
    {
        "0f25929bcb43d5a57391564615c9e70a992b10eafa4db109709649cf48c50dd216da2f5cb6be7a0aa72c440c53"
        "c9bbdfec6c36c7d515536431b3a865468acbba2e89718ad33c8bed92e210e81d1853435399a271913a6520736a"
        "4729cf0d51eb01a9e2ffa2e92599b68e44de5bcf354fa2642bd4f26b259daa6f7ce3ed57aeb314a9a87b789a58"
        "af499b314e13c3d65bede56c07ea2d418d6874857b70763713178fb49a2d6cd347dc58973ff49613a20757d0fc"
        "c22079f9abd10c3baee245901b9e027bd5cfc2cb5db82d4dc9677ac795ec500ecd47deee3b5da006d6d049b811"
        "d7511c78158de484232fc68daf8a45cf217d1c2fae693ff5871e8752d73b21198e9393920d483a7260bfb731fb"
        "5d25f1aa493335a9e71297e485b7aef312c21800deef121f1e76426a00665e5c4479674322d4f75edadd46debd"
        "5cd992f6ed090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b12c85ea5db8c6deb"
        "4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa",
        "0000000000000000000000000000000000000000000000000000000000000001",
        "jeff3",
    },
    {
        "2f2ea0b3da1e8ef11914acf8b2e1b32d99df51f5f4f206fc6b947eae860eddb6068134ddb33dc888ef446b648d"
        "72338684d678d2eb2371c61a50734d78da4b7225f83c8b6ab9de74e7da488ef02645c5a16a6652c3c71a15dc37"
        "fe3a5dcb7cb122acdedd6308e3bb230d226d16a105295f523a8a02bfc5e8bd2da135ac4c245d065bbad92e7c4e"
        "31bf3757f1fe7362a63fbfee50e7dc68da116e67d600d9bf6806d302580dc0661002994e7cd3a7f224e7ddc278"
        "02777486bf80f40e4ca3cfdb186bac5188a98c45e6016873d107f5cd131f3a3e339d0375e58bd6219347b00812"
        "2ae2b09e539e152ec5364e7e2204b03d11d3caa038bfc7cd499f8176aacbee1f39e4e4afc4bc74790a4a028aff"
        "2c3d2538731fb755edefd8cb48d6ea589b5e283f150794b6736f670d6a1033f9b46c6f5204f50813eb85c8dc4b"
        "59db1c5d39140d97ee4d2b36d99bc49974d18ecca3e7ad51011956051b464d9e27d46cc25e0764bb98575bd466"
        "d32db7b15f582b2d5c452b36aa394b789366e5e3ca5aabd415794ab061441e51d01e94640b7e3084a07e02c78c"
        "f3103c542bc5b298669f211b88da1679b0b64a63b7e0e7bfe52aae524f73a55be7fe70c7e9bfc94b4cf0da1213"
        "d2149b006137fcfb23036606f848d638d576a120ca981b5b1a5f9300b3ee2276cf730cf493cd95d64677bbb75f"
        "c42db72513a4c1e387b476d056f80aa75f21ee6226d31426322afcda621464d0611d226783262e21bb3bc86b53"
        "7e986237096df1f82dff337dd5972e32a8ad43e28a78a96a823ef1cd4debe12b6552ea5f",
        "0000000000000000000000000000000000000000000000000000000000000001",
        "jeff4",
    },
    {
        "20a754d2071d4d53903e3b31a7e98ad6882d58aec240ef981fdf0a9d22c5926a29c853fcea789887315916bbeb"
        "89ca37edb355b4f980c9a12a94f30deeed30211213d2149b006137fcfb23036606f848d638d576a120ca981b5b"
        "1a5f9300b3ee2276cf730cf493cd95d64677bbb75fc42db72513a4c1e387b476d056f80aa75f21ee6226d31426"
        "322afcda621464d0611d226783262e21bb3bc86b537e986237096df1f82dff337dd5972e32a8ad43e28a78a96a"
        "823ef1cd4debe12b6552ea5f1abb4a25eb9379ae96c84fff9f0540abcfc0a0d11aeda02d4f37e4baf74cb0c110"
        "73b3ff2cdbb38755f8691ea59e9606696b3ff278acfc098fa8226470d03869217cee0a9ad79a4493b5253e2e4e"
        "3a39fc2df38419f230d341f60cb064a0ac290a3d76f140db8418ba512272381446eb73958670f00cf46f1d9e64"
        "cba057b53c26f64a8ec70387a13e41430ed3ee4a7db2059cc5fc13c067194bcc0cb49a98552fd72bd9edb65734"
        "6127da132e5b82ab908f5816c826acb499e22f2412d1a2d70f25929bcb43d5a57391564615c9e70a992b10eafa"
        "4db109709649cf48c50dd2198a1f162a73261f112401aa2db79c7dab1533c9935c77290a6ce3b191f2318d198e"
        "9393920d483a7260bfb731fb5d25f1aa493335a9e71297e485b7aef312c21800deef121f1e76426a00665e5c44"
        "79674322d4f75edadd46debd5cd992f6ed090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadc"
        "d122975b12c85ea5db8c6deb4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa",
        "0000000000000000000000000000000000000000000000000000000000000001",
        "jeff5",
    },
    {
        "1c76476f4def4bb94541d57ebba1193381ffa7aa76ada664dd31c16024c43f593034dd2920f673e204fee2811c"
        "678745fc819b55d3e9d294e45c9b03a76aef41209dd15ebff5d46c4bd888e51a93cf99a7329636c63514396b4a"
        "452003a35bf704bf11ca01483bfa8b34b43561848d28905960114c8ac04049af4b6315a416782bb8324af6cfc9"
        "3537a2ad1a445cfd0ca2a71acd7ac41fadbf933c2a51be344d120a2a4cf30c1bf9845f20c6fe39e07ea2cce61f"
        "0c9bb048165fe5e4de877550111e129f1cf1097710d41c4ac70fcdfa5ba2023c6ff1cbeac322de49d1b6df7c10"
        "3188585e2364128fe25c70558f1560f4f9350baf3959e603cc91486e110936198e9393920d483a7260bfb731fb"
        "5d25f1aa493335a9e71297e485b7aef312c21800deef121f1e76426a00665e5c4479674322d4f75edadd46debd"
        "5cd992f6ed090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b12c85ea5db8c6deb"
        "4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "jeff6",
    },
    {
        // ecpairing_empty_data_insufficient_gas
        "",
        "0000000000000000000000000000000000000000000000000000000000000001",
        "empty_data",
    },
    {
        // ecpairing_one_point_insufficient_gas
        "000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000"
        "00000000000000000000000000000000000002198e9393920d483a7260bfb731fb5d25f1aa493335a9e71297e4"
        "85b7aef312c21800deef121f1e76426a00665e5c4479674322d4f75edadd46debd5cd992f6ed090689d0585ff0"
        "75ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b12c85ea5db8c6deb4aab71808dcb408fe3d1e769"
        "0c43d37b4ce6cc0166fa7daa",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "one_point",
    },
    {
        // ecpairing_two_point_match_2
        "000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000"
        "00000000000000000000000000000000000002198e9393920d483a7260bfb731fb5d25f1aa493335a9e71297e4"
        "85b7aef312c21800deef121f1e76426a00665e5c4479674322d4f75edadd46debd5cd992f6ed090689d0585ff0"
        "75ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b12c85ea5db8c6deb4aab71808dcb408fe3d1e769"
        "0c43d37b4ce6cc0166fa7daa000000000000000000000000000000000000000000000000000000000000000100"
        "00000000000000000000000000000000000000000000000000000000000002198e9393920d483a7260bfb731fb"
        "5d25f1aa493335a9e71297e485b7aef312c21800deef121f1e76426a00665e5c4479674322d4f75edadd46debd"
        "5cd992f6ed275dc4a288d1afb3cbb1ac09187524c7db36395df7be3b99e673b13a075a65ec1d9befcd05a5323e"
        "6da4d435f3b617cdb3af83285c2df711ef39c01571827f9d",
        "0000000000000000000000000000000000000000000000000000000000000001",
        "two_point_match_2",
    },
    {
        // ecpairing_two_point_match_3
        "000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000"
        "00000000000000000000000000000000000002203e205db4f19b37b60121b83a7333706db86431c6d835849957"
        "ed8c3928ad7927dc7234fd11d3e8c36c59277c3e6f149d5cd3cfa9a62aee49f8130962b4b3b9195e8aa5b78274"
        "63722b8c153931579d3505566b4edf48d498e185f0509de15204bb53b8977e5f92a0bc372742c4830944a59b4f"
        "e6b1c0466e2a6dad122b5d2e030644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd31a"
        "76dae6d3272396d0cbe61fced2bc532edac647851e3ac53ce1cc9c7e645a83198e9393920d483a7260bfb731fb"
        "5d25f1aa493335a9e71297e485b7aef312c21800deef121f1e76426a00665e5c4479674322d4f75edadd46debd"
        "5cd992f6ed090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b12c85ea5db8c6deb"
        "4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa",
        "0000000000000000000000000000000000000000000000000000000000000001",
        "two_point_match_3",
    },
    {
        // ecpairing_two_point_match_4
        "105456a333e6d636854f987ea7bb713dfd0ae8371a72aea313ae0c32c0bf10160cf031d41b41557f3e7e3ba0c5"
        "1bebe5da8e6ecd855ec50fc87efcdeac168bcc0476be093a6d2b4bbf907172049874af11e1b6267606e00804d3"
        "ff0037ec57fd3010c68cb50161b7d1d96bb71edfec9880171954e56871abf3d93cc94d745fa114c059d74e5b6c"
        "4ec14ae5864ebe23a71781d86c29fb8fb6cce94f70d3de7a2101b33461f39d9e887dbb100f170a2345dde3c07e"
        "256d1dfa2b657ba5cd030427000000000000000000000000000000000000000000000000000000000000000100"
        "000000000000000000000000000000000000000000000000000000000000021a2c3013d2ea92e13c800cde68ef"
        "56a294b883f6ac35d25f587c09b1b3c635f7290158a80cd3d66530f74dc94c94adb88f5cdb481acca997b6e600"
        "71f08a115f2f997f3dbd66a7afe07fe7862ce239edba9e05c5afff7f8a1259c9733b2dfbb929d1691530ca701b"
        "4a106054688728c9972c8512e9789e9567aae23e302ccd75",
        "0000000000000000000000000000000000000000000000000000000000000001",
        "two_point_match_4",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000"
        "00000000000000000000000000000000000002198e9393920d483a7260bfb731fb5d25f1aa493335a9e71297e4"
        "85b7aef312c21800deef121f1e76426a00665e5c4479674322d4f75edadd46debd5cd992f6ed090689d0585ff0"
        "75ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b12c85ea5db8c6deb4aab71808dcb408fe3d1e769"
        "0c43d37b4ce6cc0166fa7daa000000000000000000000000000000000000000000000000000000000000000100"
        "00000000000000000000000000000000000000000000000000000000000002198e9393920d483a7260bfb731fb"
        "5d25f1aa493335a9e71297e485b7aef312c21800deef121f1e76426a00665e5c4479674322d4f75edadd46debd"
        "5cd992f6ed275dc4a288d1afb3cbb1ac09187524c7db36395df7be3b99e673b13a075a65ec1d9befcd05a5323e"
        "6da4d435f3b617cdb3af83285c2df711ef39c01571827f9d000000000000000000000000000000000000000000"
        "00000000000000000000010000000000000000000000000000000000000000000000000000000000000002198e"
        "9393920d483a7260bfb731fb5d25f1aa493335a9e71297e485b7aef312c21800deef121f1e76426a00665e5c44"
        "79674322d4f75edadd46debd5cd992f6ed090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadc"
        "d122975b12c85ea5db8c6deb4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa000000000000000000"
        "000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000"
        "00000000000000000002198e9393920d483a7260bfb731fb5d25f1aa493335a9e71297e485b7aef312c21800de"
        "ef121f1e76426a00665e5c4479674322d4f75edadd46debd5cd992f6ed275dc4a288d1afb3cbb1ac09187524c7"
        "db36395df7be3b99e673b13a075a65ec1d9befcd05a5323e6da4d435f3b617cdb3af83285c2df711ef39c01571"
        "827f9d000000000000000000000000000000000000000000000000000000000000000100000000000000000000"
        "00000000000000000000000000000000000000000002198e9393920d483a7260bfb731fb5d25f1aa493335a9e7"
        "1297e485b7aef312c21800deef121f1e76426a00665e5c4479674322d4f75edadd46debd5cd992f6ed090689d0"
        "585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b12c85ea5db8c6deb4aab71808dcb408fe3"
        "d1e7690c43d37b4ce6cc0166fa7daa000000000000000000000000000000000000000000000000000000000000"
        "00010000000000000000000000000000000000000000000000000000000000000002198e9393920d483a7260bf"
        "b731fb5d25f1aa493335a9e71297e485b7aef312c21800deef121f1e76426a00665e5c4479674322d4f75edadd"
        "46debd5cd992f6ed275dc4a288d1afb3cbb1ac09187524c7db36395df7be3b99e673b13a075a65ec1d9befcd05"
        "a5323e6da4d435f3b617cdb3af83285c2df711ef39c01571827f9d000000000000000000000000000000000000"
        "000000000000000000000000000100000000000000000000000000000000000000000000000000000000000000"
        "02198e9393920d483a7260bfb731fb5d25f1aa493335a9e71297e485b7aef312c21800deef121f1e76426a0066"
        "5e5c4479674322d4f75edadd46debd5cd992f6ed090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355"
        "acdadcd122975b12c85ea5db8c6deb4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa000000000000"
        "000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000"
        "00000000000000000000000002198e9393920d483a7260bfb731fb5d25f1aa493335a9e71297e485b7aef312c2"
        "1800deef121f1e76426a00665e5c4479674322d4f75edadd46debd5cd992f6ed275dc4a288d1afb3cbb1ac0918"
        "7524c7db36395df7be3b99e673b13a075a65ec1d9befcd05a5323e6da4d435f3b617cdb3af83285c2df711ef39"
        "c01571827f9d000000000000000000000000000000000000000000000000000000000000000100000000000000"
        "00000000000000000000000000000000000000000000000002198e9393920d483a7260bfb731fb5d25f1aa4933"
        "35a9e71297e485b7aef312c21800deef121f1e76426a00665e5c4479674322d4f75edadd46debd5cd992f6ed09"
        "0689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b12c85ea5db8c6deb4aab71808dcb"
        "408fe3d1e7690c43d37b4ce6cc0166fa7daa000000000000000000000000000000000000000000000000000000"
        "00000000010000000000000000000000000000000000000000000000000000000000000002198e9393920d483a"
        "7260bfb731fb5d25f1aa493335a9e71297e485b7aef312c21800deef121f1e76426a00665e5c4479674322d4f7"
        "5edadd46debd5cd992f6ed275dc4a288d1afb3cbb1ac09187524c7db36395df7be3b99e673b13a075a65ec1d9b"
        "efcd05a5323e6da4d435f3b617cdb3af83285c2df711ef39c01571827f9d",
        "0000000000000000000000000000000000000000000000000000000000000001",
        "ten_point_match_1",
    },
    {
        "000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000"
        "00000000000000000000000000000000000002203e205db4f19b37b60121b83a7333706db86431c6d835849957"
        "ed8c3928ad7927dc7234fd11d3e8c36c59277c3e6f149d5cd3cfa9a62aee49f8130962b4b3b9195e8aa5b78274"
        "63722b8c153931579d3505566b4edf48d498e185f0509de15204bb53b8977e5f92a0bc372742c4830944a59b4f"
        "e6b1c0466e2a6dad122b5d2e030644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd31a"
        "76dae6d3272396d0cbe61fced2bc532edac647851e3ac53ce1cc9c7e645a83198e9393920d483a7260bfb731fb"
        "5d25f1aa493335a9e71297e485b7aef312c21800deef121f1e76426a00665e5c4479674322d4f75edadd46debd"
        "5cd992f6ed090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b12c85ea5db8c6deb"
        "4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa000000000000000000000000000000000000000000"
        "00000000000000000000010000000000000000000000000000000000000000000000000000000000000002203e"
        "205db4f19b37b60121b83a7333706db86431c6d835849957ed8c3928ad7927dc7234fd11d3e8c36c59277c3e6f"
        "149d5cd3cfa9a62aee49f8130962b4b3b9195e8aa5b7827463722b8c153931579d3505566b4edf48d498e185f0"
        "509de15204bb53b8977e5f92a0bc372742c4830944a59b4fe6b1c0466e2a6dad122b5d2e030644e72e131a029b"
        "85045b68181585d97816a916871ca8d3c208c16d87cfd31a76dae6d3272396d0cbe61fced2bc532edac647851e"
        "3ac53ce1cc9c7e645a83198e9393920d483a7260bfb731fb5d25f1aa493335a9e71297e485b7aef312c21800de"
        "ef121f1e76426a00665e5c4479674322d4f75edadd46debd5cd992f6ed090689d0585ff075ec9e99ad690c3395"
        "bc4b313370b38ef355acdadcd122975b12c85ea5db8c6deb4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166"
        "fa7daa000000000000000000000000000000000000000000000000000000000000000100000000000000000000"
        "00000000000000000000000000000000000000000002203e205db4f19b37b60121b83a7333706db86431c6d835"
        "849957ed8c3928ad7927dc7234fd11d3e8c36c59277c3e6f149d5cd3cfa9a62aee49f8130962b4b3b9195e8aa5"
        "b7827463722b8c153931579d3505566b4edf48d498e185f0509de15204bb53b8977e5f92a0bc372742c4830944"
        "a59b4fe6b1c0466e2a6dad122b5d2e030644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87"
        "cfd31a76dae6d3272396d0cbe61fced2bc532edac647851e3ac53ce1cc9c7e645a83198e9393920d483a7260bf"
        "b731fb5d25f1aa493335a9e71297e485b7aef312c21800deef121f1e76426a00665e5c4479674322d4f75edadd"
        "46debd5cd992f6ed090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b12c85ea5db"
        "8c6deb4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa000000000000000000000000000000000000"
        "000000000000000000000000000100000000000000000000000000000000000000000000000000000000000000"
        "02203e205db4f19b37b60121b83a7333706db86431c6d835849957ed8c3928ad7927dc7234fd11d3e8c36c5927"
        "7c3e6f149d5cd3cfa9a62aee49f8130962b4b3b9195e8aa5b7827463722b8c153931579d3505566b4edf48d498"
        "e185f0509de15204bb53b8977e5f92a0bc372742c4830944a59b4fe6b1c0466e2a6dad122b5d2e030644e72e13"
        "1a029b85045b68181585d97816a916871ca8d3c208c16d87cfd31a76dae6d3272396d0cbe61fced2bc532edac6"
        "47851e3ac53ce1cc9c7e645a83198e9393920d483a7260bfb731fb5d25f1aa493335a9e71297e485b7aef312c2"
        "1800deef121f1e76426a00665e5c4479674322d4f75edadd46debd5cd992f6ed090689d0585ff075ec9e99ad69"
        "0c3395bc4b313370b38ef355acdadcd122975b12c85ea5db8c6deb4aab71808dcb408fe3d1e7690c43d37b4ce6"
        "cc0166fa7daa000000000000000000000000000000000000000000000000000000000000000100000000000000"
        "00000000000000000000000000000000000000000000000002203e205db4f19b37b60121b83a7333706db86431"
        "c6d835849957ed8c3928ad7927dc7234fd11d3e8c36c59277c3e6f149d5cd3cfa9a62aee49f8130962b4b3b919"
        "5e8aa5b7827463722b8c153931579d3505566b4edf48d498e185f0509de15204bb53b8977e5f92a0bc372742c4"
        "830944a59b4fe6b1c0466e2a6dad122b5d2e030644e72e131a029b85045b68181585d97816a916871ca8d3c208"
        "c16d87cfd31a76dae6d3272396d0cbe61fced2bc532edac647851e3ac53ce1cc9c7e645a83198e9393920d483a"
        "7260bfb731fb5d25f1aa493335a9e71297e485b7aef312c21800deef121f1e76426a00665e5c4479674322d4f7"
        "5edadd46debd5cd992f6ed090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b12c8"
        "5ea5db8c6deb4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa",
        "0000000000000000000000000000000000000000000000000000000000000001",
        "ten_point_match_2",
    },
    {
        // ecpairing_two_point_match_4
        "105456a333e6d636854f987ea7bb713dfd0ae8371a72aea313ae0c32c0bf10160cf031d41b41557f3e7e3ba0c5"
        "1bebe5da8e6ecd855ec50fc87efcdeac168bcc0476be093a6d2b4bbf907172049874af11e1b6267606e00804d3"
        "ff0037ec57fd3010c68cb50161b7d1d96bb71edfec9880171954e56871abf3d93cc94d745fa114c059d74e5b6c"
        "4ec14ae5864ebe23a71781d86c29fb8fb6cce94f70d3de7a2101b33461f39d9e887dbb100f170a2345dde3c07e"
        "256d1dfa2b657ba5cd030427000000000000000000000000000000000000000000000000000000000000000100"
        "000000000000000000000000000000000000000000000000000000000000021a2c3013d2ea92e13c800cde68ef"
        "56a294b883f6ac35d25f587c09b1b3c635f7290158a80cd3d66530f74dc94c94adb88f5cdb481acca997b6e600"
        "71f08a115f2f997f3dbd66a7afe07fe7862ce239edba9e05c5afff7f8a1259c9733b2dfbb929d1691530ca701b"
        "4a106054688728c9972c8512e9789e9567aae23e302ccd75",
        "0000000000000000000000000000000000000000000000000000000000000001",
        "ten_point_match_3",
    },
};

namespace {
void benchmarkPrecompiled( char const name[], vector_ref< const PrecompiledTest > tests, int n ) {
    if ( !Options::get().all ) {
        std::cout << "Skipping benchmark test because --all option is not specified.\n";
        return;
    }

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( name );
    Timer timer;

    for ( auto&& test : tests ) {
        bytes input = fromHex( test.input );
        bytesConstRef inputRef = &input;

        auto res = exec( inputRef );
        BOOST_REQUIRE_MESSAGE( res.first, test.name );
        BOOST_REQUIRE_EQUAL( toHex( res.second ), test.expected );

        timer.restart();
        for ( int i = 0; i < n; ++i )
            exec( inputRef );
        auto d = timer.duration() / n;

        auto t = std::chrono::duration_cast< std::chrono::nanoseconds >( d ).count();
        std::cout << ut::framework::current_test_case().p_name << "/" << test.name << ": " << t
                  << " ns\n";
    }
}
}  // namespace

/// @}

BOOST_AUTO_TEST_CASE( bench_ecrecover,
    *ut::label( "bench" ) * boost::unit_test::precondition( dev::test::run_not_express ) ) {
    vector_ref< const PrecompiledTest > tests{
        ecrecoverTests, sizeof( ecrecoverTests ) / sizeof( ecrecoverTests[0] )};
    benchmarkPrecompiled( "ecrecover", tests, 100000 );
}

BOOST_AUTO_TEST_CASE( bench_modexp,
    *ut::label( "bench" ) * boost::unit_test::precondition( dev::test::run_not_express ) ) {
    vector_ref< const PrecompiledTest > tests{
        modexpTests, sizeof( modexpTests ) / sizeof( modexpTests[0] )};
    benchmarkPrecompiled( "modexp", tests, 10000 );
}

BOOST_AUTO_TEST_CASE( bench_bn256Add,
    *ut::label( "bench" ) * boost::unit_test::precondition( dev::test::run_not_express ) ) {
    vector_ref< const PrecompiledTest > tests{
        bn256AddTests, sizeof( bn256AddTests ) / sizeof( bn256AddTests[0] )};
    benchmarkPrecompiled( "alt_bn128_G1_add", tests, 1000000 );
}

BOOST_AUTO_TEST_CASE( bench_bn256ScalarMul,
    *ut::label( "bench" ) * boost::unit_test::precondition( dev::test::run_not_express ) ) {
    vector_ref< const PrecompiledTest > tests{
        bn256ScalarMulTests, sizeof( bn256ScalarMulTests ) / sizeof( bn256ScalarMulTests[0] )};
    benchmarkPrecompiled( "alt_bn128_G1_mul", tests, 10000 );
}

BOOST_AUTO_TEST_CASE( bench_bn256Pairing,
    *ut::label( "bench" ) * boost::unit_test::precondition( dev::test::run_not_express ) ) {
    vector_ref< const PrecompiledTest > tests{
        bn256PairingTests, sizeof( bn256PairingTests ) / sizeof( bn256PairingTests[0] )};
    benchmarkPrecompiled( "alt_bn128_pairing_product", tests, 1000 );
}

BOOST_AUTO_TEST_CASE( ecaddCostBeforeIstanbul, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledPricer cost = PrecompiledRegistrar::pricer( "alt_bn128_G1_add" );

    ChainParams chainParams{genesisInfo( eth::Network::IstanbulTransitionTest )};

    auto res = cost( {}, chainParams, 1 );

    BOOST_REQUIRE_EQUAL( static_cast< int >( res ), 500 );
}

BOOST_AUTO_TEST_CASE( ecaddCostIstanbul, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledPricer cost = PrecompiledRegistrar::pricer( "alt_bn128_G1_add" );

    ChainParams chainParams{genesisInfo( eth::Network::IstanbulTransitionTest )};

    auto res = cost( {}, chainParams, 2 );

    BOOST_REQUIRE_EQUAL( static_cast< int >( res ), 150 );
}

BOOST_AUTO_TEST_CASE( ecmulBeforeIstanbul, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledPricer cost = PrecompiledRegistrar::pricer( "alt_bn128_G1_mul" );

    ChainParams chainParams{genesisInfo( eth::Network::IstanbulTransitionTest )};

    auto res = cost( {}, chainParams, 1 );

    BOOST_REQUIRE_EQUAL( static_cast< int >( res ), 40000 );
}

BOOST_AUTO_TEST_CASE( ecmulCostIstanbul, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    PrecompiledPricer cost = PrecompiledRegistrar::pricer( "alt_bn128_G1_mul" );

    ChainParams chainParams{genesisInfo( eth::Network::IstanbulTransitionTest )};

    auto res = cost( {}, chainParams, 2 );

    BOOST_REQUIRE_EQUAL( static_cast< int >( res ), 6000 );
}

BOOST_AUTO_TEST_CASE( ecpairingCost ) {
    PrecompiledPricer cost = PrecompiledRegistrar::pricer( "alt_bn128_pairing_product" );

    ChainParams chainParams{genesisInfo( eth::Network::IstanbulTransitionTest )};

    bytes in{fromHex(
        "0x1c76476f4def4bb94541d57ebba1193381ffa7aa76ada664dd31c16024c43f593034dd2920f673e204fee281"
        "1c678745fc819b55d3e9d294e45c9b03a76aef41209dd15ebff5d46c4bd888e51a93cf99a7329636c63514396b"
        "4a452003a35bf704bf11ca01483bfa8b34b43561848d28905960114c8ac04049af4b6315a416782bb8324af6cf"
        "c93537a2ad1a445cfd0ca2a71acd7ac41fadbf933c2a51be344d120a2a4cf30c1bf9845f20c6fe39e07ea2cce6"
        "1f0c9bb048165fe5e4de877550111e129f1cf1097710d41c4ac70fcdfa5ba2023c6ff1cbeac322de49d1b6df7c"
        "2032c61a830e3c17286de9462bf242fca2883585b93870a73853face6a6bf411198e9393920d483a7260bfb731"
        "fb5d25f1aa493335a9e71297e485b7aef312c21800deef121f1e76426a00665e5c4479674322d4f75edadd46de"
        "bd5cd992f6ed090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b12c85ea5db8c6d"
        "eb4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa" )};

    auto costBeforeIstanbul = cost( ref( in ), chainParams, 1 );
    BOOST_CHECK_EQUAL( static_cast< int >( costBeforeIstanbul ), in.size() / 192 * 80000 + 100000 );

    auto costIstanbul = cost( ref( in ), chainParams, 2 );
    BOOST_CHECK_EQUAL( static_cast< int >( costIstanbul ), in.size() / 192 * 34000 + 45000 );
}

static std::string const genesisInfoSkaleConfigTest = std::string() +
                                                  R"E(
{
    "sealEngine": "Ethash",
    "params": {
        "accountStartNonce": "0x00",
        "homesteadForkBlock": "0x00",
        "EIP150ForkBlock": "0x00",
        "EIP158ForkBlock": "0x00",
        "byzantiumForkBlock": "0x00",
        "constantinopleForkBlock": "0x00",
        "constantinopleFixForkBlock": "0x00",
        "networkID" : "12313219",
        "chainID": "0x01",
        "maximumExtraDataSize": "0x20",
        "tieBreakingGas": false,
        "minGasLimit": "0x1388",
        "maxGasLimit": "7fffffffffffffff",
        "gasLimitBoundDivisor": "0x0400",
        "minimumDifficulty": "0x020000",
        "difficultyBoundDivisor": "0x0800",
        "durationLimit": "0x0d",
        "blockReward": "0x4563918244F40000"
    },
    "genesis": {
        "nonce": "0x0000000000000042",
        "difficulty": "0x020000",
        "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
        "author": "0x0000000000000000000000000000000000000000",
        "timestamp": "0x00",
        "parentHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
        "extraData": "0x11bbe8db4e347b4e8c937c1c8370e4b5ed33adb3db69cbdb7a38e1e50b1b82fa",
        "gasLimit": "0x47E7C4"
    },
   "skaleConfig": {
    "nodeInfo": {
      "nodeName": "Node1",
      "nodeID": 1112,
      "bindIP": "127.0.0.1",
      "basePort": 1234,
      "logLevel": "trace",
      "logLevelProposal": "trace",
      "testSignatures": true,
      "wallets": {
        "ima": {
            "n": 1
        }
      }
    },
    "sChain": {
        "schainName": "TestChain",
        "schainID": 1,
        "contractStorageLimit": 32000,
        "precompiledConfigPatchTimestamp": 1,
        "emptyBlockIntervalMs": -1,
        "nodeGroups": {
            "1": {
                "nodes": {
                    "30": [
                        13,
                        30,
                        "0x6180cde2cbbcc6b6a17efec4503a7d4316f8612f411ee171587089f770335f484003ad236c534b9afa82befc1f69533723abdb6ec2601e582b72dcfd7919338b"
                    ]
                },
                "finish_ts": null,
                "bls_public_key": {
                    "blsPublicKey0": "10860211539819517237363395256510340030868592687836950245163587507107792195621",
                    "blsPublicKey1": "2419969454136313127863904023626922181546178935031521540751337209075607503568",
                    "blsPublicKey2": "3399776985251727272800732947224655319335094876742988846345707000254666193993",
                    "blsPublicKey3": "16982202412630419037827505223148517434545454619191931299977913428346639096984"
                }
            },
            "0": {
                "nodes": {
                    "26": [
                        3,
                        26,
                        "0x3a581d62b12232dade30c3710215a271984841657449d1f474295a13737b778266f57e298f123ae80cbab7cc35ead1b62a387556f94b326d5c65d4a7aa2abcba"
                    ]
                },
                "finish_ts": 4294967290,
                "bls_public_key": {
                    "blsPublicKey0": "12457351342169393659284905310882617316356538373005664536506840512800919345414",
                    "blsPublicKey1": "11573096151310346982175966190385407867176668720531590318594794283907348596326",
                    "blsPublicKey2": "13929944172721019694880576097738949215943314024940461401664534665129747139387",
                    "blsPublicKey3": "7375214420811287025501422512322868338311819657776589198925786170409964211914"
                }
            }
        },
        "nodes": [
          { "nodeID": 1112, "ip": "127.0.0.1", "basePort": 1234, "schainIndex" : 1, "publicKey": "0xfa", "owner": "0x21abd6db4e347b4e8c937c1c8370e4b5ed3f0dd3db69cbdb7a38e1e50b1b82fc"}
        ]
    }
  },
    "accounts": {
        "0000000000000000000000000000000000000001": { "precompiled": { "name": "ecrecover", "linear": { "base": 3000, "word": 0 } } },
        "0000000000000000000000000000000000000002": { "precompiled": { "name": "sha256", "linear": { "base": 60, "word": 12 } } },
        "0000000000000000000000000000000000000003": { "precompiled": { "name": "ripemd160", "linear": { "base": 600, "word": 120 } } },
        "0000000000000000000000000000000000000004": { "precompiled": { "name": "identity", "linear": { "base": 15, "word": 3 } } },
        "0000000000000000000000000000000000000005": { "precompiled": { "name": "modexp", "startingBlock" : "0x2dc6c0" } },
        "0000000000000000000000000000000000000006": { "precompiled": { "name": "alt_bn128_G1_add", "startingBlock" : "0x2dc6c0", "linear": { "base": 500, "word": 0 } } },
        "0000000000000000000000000000000000000007": { "precompiled": { "name": "alt_bn128_G1_mul", "startingBlock" : "0x2dc6c0", "linear": { "base": 40000, "word": 0 } } },
        "0000000000000000000000000000000000000008": { "precompiled": { "name": "alt_bn128_pairing_product", "startingBlock" : "0x2dc6c0" } },
        "0xca4409573a5129a72edf85d6c51e26760fc9c903": { "balance": "100000000000000000000000" },
        "0xD2001300000000000000000000000000000000D2": { "balance": "0", "nonce": "0", "storage": {}, "code":"0x6080604052348015600f57600080fd5b506004361060325760003560e01c8063815b8ab41460375780638273f754146062575b600080fd5b606060048036036020811015604b57600080fd5b8101908080359060200190929190505050606a565b005b60686081565b005b60005a90505b815a82031015607d576070565b5050565b60005a9050609660028281609157fe5b04606a565b5056fea165627a7a72305820f5fb5a65e97cbda96c32b3a2e1497cd6b7989179b5dc29e9875bcbea5a96c4520029"},
        "0xD2001300000000000000000000000000000000D4": { "balance": "0", "nonce": "0", "storage": {}, "code":"0x608060405234801561001057600080fd5b506004361061004c5760003560e01c80632098776714610051578063b8bd717f1461007f578063d37165fa146100ad578063fdde8d66146100db575b600080fd5b61007d6004803603602081101561006757600080fd5b8101908080359060200190929190505050610109565b005b6100ab6004803603602081101561009557600080fd5b8101908080359060200190929190505050610136565b005b6100d9600480360360208110156100c357600080fd5b8101908080359060200190929190505050610170565b005b610107600480360360208110156100f157600080fd5b8101908080359060200190929190505050610191565b005b60005a90505b815a8203101561011e5761010f565b600080fd5b815a8203101561013257610123565b5050565b60005a90505b815a8203101561014b5761013c565b600060011461015957600080fd5b5a90505b815a8203101561016c5761015d565b5050565b60005a9050600081830390505b805a8303101561018c5761017d565b505050565b60005a90505b815a820310156101a657610197565b60016101b157600080fd5b5a90505b815a820310156101c4576101b5565b505056fea264697066735822122089b72532621e7d1849e444ee6efaad4fb8771258e6f79755083dce434e5ac94c64736f6c63430006000033"},
        "0xd40B3c51D0ECED279b1697DbdF45d4D19b872164": { "balance": "0", "nonce": "0", "storage": {}, "code":"0x6080604052348015600f57600080fd5b506004361060325760003560e01c80636057361d146037578063b05784b8146062575b600080fd5b606060048036036020811015604b57600080fd5b8101908080359060200190929190505050607e565b005b60686088565b6040518082815260200191505060405180910390f35b8060008190555050565b6000805490509056fea2646970667358221220e5ff9593bfa9540a34cad5ecbe137dcafcfe1f93e3c4832610438d6f0ece37db64736f6c63430006060033"},
        "0xD2001300000000000000000000000000000000D3": { "balance": "0", "nonce": "0", "storage": {}, "code":"0x608060405234801561001057600080fd5b50600436106100365760003560e01c8063ee919d501461003b578063f0fdf83414610069575b600080fd5b6100676004803603602081101561005157600080fd5b81019080803590602001909291905050506100af565b005b6100956004803603602081101561007f57600080fd5b8101908080359060200190929190505050610108565b604051808215151515815260200191505060405180910390f35b600160008083815260200190815260200160002060006101000a81548160ff021916908315150217905550600080600083815260200190815260200160002060006101000a81548160ff02191690831515021790555050565b60006020528060005260406000206000915054906101000a900460ff168156fea2646970667358221220cf479cb746c4b897c88be4ad8e2612a14e27478f91928c49619c98da374a3bf864736f6c63430006000033"},
        "0xD40b89C063a23eb85d739f6fA9B14341838eeB2b": { "balance": "0", "nonce": "0", "storage": {"0x101e368776582e57ab3d116ffe2517c0a585cd5b23174b01e275c2d8329c3d83": "0x0000000000000000000000000000000000000000000000000000000000000001"}, "code":"0x608060405234801561001057600080fd5b506004361061004c5760003560e01c80634df7e3d014610051578063d82cf7901461006f578063ee919d501461009d578063f0fdf834146100cb575b600080fd5b610059610111565b6040518082815260200191505060405180910390f35b61009b6004803603602081101561008557600080fd5b8101908080359060200190929190505050610117565b005b6100c9600480360360208110156100b357600080fd5b810190808035906020019092919050505061017d565b005b6100f7600480360360208110156100e157600080fd5b81019080803590602001909291905050506101ab565b604051808215151515815260200191505060405180910390f35b60015481565b60008082815260200190815260200160002060009054906101000a900460ff16151560011515141561017a57600080600083815260200190815260200160002060006101000a81548160ff02191690831515021790555060018054016001819055505b50565b600160008083815260200190815260200160002060006101000a81548160ff02191690831515021790555050565b60006020528060005260406000206000915054906101000a900460ff168156fea264697066735822122000af6f9a0d5c9b8b642648557291c9eb0f9732d60094cf75e14bb192abd97bcc64736f6c63430006000033"}
    }
}
)E";

BOOST_AUTO_TEST_CASE( getConfigVariable ) {
    ChainParams chainParams;
    chainParams = chainParams.loadConfig( genesisInfoSkaleConfigTest );
    chainParams.sealEngineName = NoProof::name();
    chainParams.allowFutureBlocks = true;

    dev::eth::g_configAccesssor.reset( new skutils::json_config_file_accessor( "../../test/unittests/libethereum/PrecompiledConfig.json" ) );

    std::unique_ptr<dev::eth::Client> client;
    dev::TransientDirectory m_tmpDir;
    auto monitor = make_shared< InstanceMonitor >("test");
    setenv("DATA_DIR", m_tmpDir.path().c_str(), 1);
    client.reset( new eth::ClientTest( chainParams, ( int ) chainParams.networkID,
        shared_ptr< GasPricer >(), nullptr, monitor, m_tmpDir.path(), dev::WithExisting::Kill ) );

    client->injectSkaleHost();
    client->startWorking();

    client->setAuthor( Address("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF") );

    ClientTest* testClient = asClientTest( client.get() );

    testClient->mineBlocks( 1 );
    testClient->importTransactionsAsBlock( dev::eth::Transactions(), 1000, 4294967294 );
    dev::eth::g_skaleHost = testClient->skaleHost();

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "getConfigVariableUint256" );
    std::string input = stringToHex( "skaleConfig.sChain.nodes.0.id" );
    input = input.substr(0, 58); // remove 0s in the end

    bytes in = fromHex( numberToHex( 29 ) + input );
    auto res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( res.first );
    BOOST_REQUIRE( dev::fromBigEndian<dev::u256>( res.second ) == 30 );

    input = stringToHex( "skaleConfig.sChain.nodes.0.schainIndex" );
    input = input.substr(0, 76); // remove 0s in the end
    in = fromHex( numberToHex( 38 ) + input );
    res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( res.first );
    BOOST_REQUIRE( dev::fromBigEndian<dev::u256>( res.second ) == 13 );

    input = stringToHex( "skaleConfig.sChain.nodes.0.publicKey" );
    input = input.substr(0, 72); // remove 0s in the end
    in = fromHex( numberToHex( 36 ) + input );
    res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( !res.first );

    input = stringToHex( "skaleConfig.sChain.nodes.0.unknownField" );
    input = input.substr(0, 78); // remove 0s in the end
    in = fromHex( numberToHex( 39 ) + input );
    res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( !res.first );

    input = stringToHex( "skaleConfig.nodeInfo.wallets.ima.n" );
    input = input.substr(0, 68); // remove 0s in the end
    in = fromHex( numberToHex( 34 ) + input );
    res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( res.first );
    BOOST_REQUIRE( dev::fromBigEndian<dev::u256>( res.second ) == 1 );

    input = stringToHex( "skaleConfig.nodeInfo.wallets.ima.t" );
    input = input.substr(0, 68); // remove 0s in the end
    in = fromHex( numberToHex( 34 ) + input );
    res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( !res.first );

    exec = PrecompiledRegistrar::executor( "getConfigVariableString" );

    input = stringToHex( "skaleConfig.sChain.nodes.0.publicKey" );
    input = input.substr(0, 72); // remove 0s in the end
    in = fromHex( numberToHex( 36 ) + input );
    res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( res.first );
    BOOST_REQUIRE( res.second == fromHex("0x6180cde2cbbcc6b6a17efec4503a7d4316f8612f411ee171587089f770335f484003ad236c534b9afa82befc1f69533723abdb6ec2601e582b72dcfd7919338b") );

    input = stringToHex( "skaleConfig.sChain.nodes.0.id" );
    input = input.substr(0, 58); // remove 0s in the end

    in = fromHex( numberToHex( 29 ) + input );
    res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( !res.first );

    input = stringToHex( "skaleConfig.sChain.nodes.0.schainIndex" );
    input = input.substr(0, 76); // remove 0s in the end
    in = fromHex( numberToHex( 38 ) + input );
    res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( !res.first );

    input = stringToHex( "skaleConfig.sChain.nodes.0.unknownField" );
    input = input.substr(0, 78); // remove 0s in the end
    in = fromHex( numberToHex( 39 ) + input );
    res = exec( bytesConstRef( in.data(), in.size() ) );

    BOOST_REQUIRE( !res.first );
}

struct FilestorageFixture : public TestOutputHelperFixture {
    FilestorageFixture() {
        ownerAddress = Address( "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" );
        fileName = "test_file";
        fileSize = 100;
        pathToFile = dev::getDataDir() / "filestorage" / ownerAddress.hex() / fileName;
        boost::filesystem::path pathToTestFile = dev::getDataDir() / "test";
        boost::filesystem::ofstream of( pathToTestFile );

        hexAddress = ownerAddress.hex();
        hexAddress.insert( hexAddress.begin(), 64 - hexAddress.length(), '0' );

        fstream file;
        file.open( pathToFile.string(), ios::out );
        file.seekp( static_cast< long >( fileSize ) - 10 );
        file.write( "a b", 3 );
        file.seekp( static_cast< long >( fileSize ) - 1 );
        file.write( "0", 1 );

        m_overlayFS = std::make_shared< skale::OverlayFS >( true );
    }

    ~FilestorageFixture() override {
        std::string pathToHashFile = pathToFile.string() + "._hash";
        remove( pathToFile.c_str() );
        remove( pathToHashFile.c_str() );
    }

    Address ownerAddress;
    std::string hexAddress;
    std::string fileName;
    std::size_t fileSize;
    boost::filesystem::path pathToFile;
    std::shared_ptr< skale::OverlayFS > m_overlayFS;
};

BOOST_FIXTURE_TEST_SUITE( FilestoragePrecompiledTests, FilestorageFixture )

BOOST_AUTO_TEST_CASE( createFile ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "createFile" );

    std::string fileName = "test_file_createFile";
    auto path = dev::getDataDir() / "filestorage" / Address( ownerAddress ).hex() / fileName;

    bytes in = fromHex( hexAddress + numberToHex( fileName.length() ) + stringToHex( fileName ) +
                        numberToHex( fileSize ) );
    auto res = exec( bytesConstRef( in.data(), in.size() ), m_overlayFS.get() );

    BOOST_REQUIRE( res.first );

    m_overlayFS->commit();
    BOOST_REQUIRE( boost::filesystem::exists( path ) );
    BOOST_REQUIRE( boost::filesystem::file_size( path ) == fileSize );
    remove( path.c_str() );
}

BOOST_AUTO_TEST_CASE( fileWithHashExtension ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "createFile" );

    std::string fileName = "createFile._hash";
    auto path = dev::getDataDir() / "filestorage" / Address( ownerAddress ).hex() / fileName;

    bytes in = fromHex( hexAddress + numberToHex( fileName.length() ) + stringToHex( fileName ) +
            numberToHex( fileSize ) );
    auto res = exec( bytesConstRef( in.data(), in.size() ), m_overlayFS.get() );
    BOOST_REQUIRE( res.first == false);

    m_overlayFS->commit();
    BOOST_REQUIRE( !boost::filesystem::exists( path ) );
}

BOOST_AUTO_TEST_CASE( uploadChunk ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "uploadChunk" );

    std::string data = "random_data";
    bytes in = fromHex( hexAddress + numberToHex( fileName.length() ) + stringToHex( fileName ) +
                        numberToHex( 0 ) + numberToHex( data.length() ) + stringToHex( data ) );
    auto res = exec( bytesConstRef( in.data(), in.size() ), m_overlayFS.get() );
    BOOST_REQUIRE( res.first );

    m_overlayFS->commit();
    std::ifstream ifs( pathToFile.string() );
    std::string content;
    std::copy_n( std::istreambuf_iterator< char >( ifs.rdbuf() ), data.length(),
        std::back_inserter( content ) );
    BOOST_REQUIRE( data == content );
}

BOOST_AUTO_TEST_CASE( readChunk ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "readChunk" );

    bytes in = fromHex( hexAddress + numberToHex( fileName.length() ) + stringToHex( fileName ) +
                        numberToHex( 0 ) + numberToHex( fileSize ) );
    auto res = exec( bytesConstRef( in.data(), in.size() ), m_overlayFS.get() );
    BOOST_REQUIRE( res.first );

    std::ifstream file( pathToFile.c_str(), std::ios_base::binary );
    std::vector< unsigned char > buffer;
    buffer.resize( fileSize );
    file.read( reinterpret_cast< char* >( &buffer[0] ), fileSize );
    BOOST_REQUIRE( buffer.size() == fileSize );
    BOOST_REQUIRE( res.second == buffer );
}

BOOST_AUTO_TEST_CASE( readMaliciousChunk ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "readChunk" );

    fileName = "../../test";
    bytes in = fromHex( hexAddress + numberToHex( fileName.length() ) + stringToHex( fileName ) +
                        numberToHex( 0 ) + numberToHex( fileSize ) );
    auto res = exec( bytesConstRef( in.data(), in.size() ), m_overlayFS.get() );
    BOOST_REQUIRE( res.first == false);
}

BOOST_AUTO_TEST_CASE( getFileSize ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "getFileSize" );

    bytes in = fromHex( hexAddress + numberToHex( fileName.length() ) + stringToHex( fileName ) );
    auto res = exec( bytesConstRef( in.data(), in.size() ), m_overlayFS.get() );
    BOOST_REQUIRE( res.first );
    BOOST_REQUIRE( res.second == toBigEndian( static_cast< u256 >( fileSize ) ) );
}

BOOST_AUTO_TEST_CASE( getMaliciousFileSize ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "getFileSize" );

    fileName = "../../test";

    bytes in = fromHex( hexAddress + numberToHex( fileName.length() ) + stringToHex( fileName ) );
    auto res = exec( bytesConstRef( in.data(), in.size() ), m_overlayFS.get() );
    BOOST_REQUIRE( !res.first );
}

BOOST_AUTO_TEST_CASE( deleteFile ) {
    PrecompiledExecutor execCreate = PrecompiledRegistrar::executor( "createFile" );
    bytes inCreate = fromHex( hexAddress + numberToHex( fileName.length() ) + stringToHex( fileName ) +
                            numberToHex( fileSize ) );
    execCreate( bytesConstRef( inCreate.data(), inCreate.size() ), m_overlayFS.get() );
    m_overlayFS->commit();

    PrecompiledExecutor execHash = PrecompiledRegistrar::executor( "calculateFileHash" );
    bytes inHash = fromHex( hexAddress + numberToHex( fileName.length() ) + stringToHex( fileName ) +
                        numberToHex( fileSize ) );
    execHash( bytesConstRef( inHash.data(), inHash.size() ), m_overlayFS.get() );
    m_overlayFS->commit();

    BOOST_REQUIRE( boost::filesystem::exists( pathToFile.string() + "._hash" ) );
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "deleteFile" );

    bytes in = fromHex( hexAddress + numberToHex( fileName.length() ) + stringToHex( fileName ) );
    auto res = exec( bytesConstRef( in.data(), in.size() ), m_overlayFS.get() );
    BOOST_REQUIRE( res.first );

    m_overlayFS->commit();
    BOOST_REQUIRE( !boost::filesystem::exists( pathToFile ) );
    BOOST_REQUIRE( !boost::filesystem::exists( pathToFile.string() + "._hash" ) );
}

BOOST_AUTO_TEST_CASE( createDirectory ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "createDirectory" );

    std::string dirName = "test_dir";
    boost::filesystem::path pathToDir =
        dev::getDataDir() / "filestorage" / ownerAddress.hex() / dirName;

    bytes in = fromHex( hexAddress + numberToHex( dirName.length() ) + stringToHex( dirName ) );
    auto res = exec( bytesConstRef( in.data(), in.size() ), m_overlayFS.get() );
    BOOST_REQUIRE( res.first );

    m_overlayFS->commit();
    BOOST_REQUIRE( boost::filesystem::exists( pathToDir ) );
    remove( pathToDir.c_str() );
}

BOOST_AUTO_TEST_CASE( deleteDirectory ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "deleteDirectory" );

    std::string dirName = "test_dir";
    boost::filesystem::path pathToDir =
        dev::getDataDir() / "filestorage" / ownerAddress.hex() / dirName;
    boost::filesystem::create_directories( pathToDir );

    bytes in = fromHex( hexAddress + numberToHex( dirName.length() ) + stringToHex( dirName ) );
    auto res = exec( bytesConstRef( in.data(), in.size() ), m_overlayFS.get() );

    BOOST_REQUIRE( res.first );

    m_overlayFS->commit();
    BOOST_REQUIRE( !boost::filesystem::exists( pathToDir ) );
}

BOOST_AUTO_TEST_CASE( calculateFileHash ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "calculateFileHash" );

    std::string fileHashName = pathToFile.string() + "._hash";

    std::ofstream fileHash( fileHashName );
    std::string relativePath = pathToFile.string().substr( pathToFile.string().find( "filestorage" ) );
    dev::h256 hash = dev::sha256( relativePath );
    fileHash << hash;

    fileHash.close();

    bytes in = fromHex( hexAddress + numberToHex( fileName.length() ) + stringToHex( fileName ) +
                        numberToHex( fileSize ) );
    auto res = exec( bytesConstRef( in.data(), in.size() ), m_overlayFS.get() );

    BOOST_REQUIRE( res.first );

    m_overlayFS->commit();
    BOOST_REQUIRE( boost::filesystem::exists( fileHashName ) );

    std::ifstream resultFile( fileHashName );
    dev::h256 calculatedHash;
    resultFile >> calculatedHash;

    std::ifstream originFile( pathToFile.string() );
    originFile.seekg( 0, std::ios::end );
    size_t fileContentSize = originFile.tellg();
    std::string content( fileContentSize, ' ' );
    originFile.seekg( 0 );
    originFile.read( &content[0], fileContentSize );

    BOOST_REQUIRE( content.size() == fileSize );
    BOOST_REQUIRE( originFile.gcount() == int( fileSize ) );

    dev::h256 fileContentHash = dev::sha256( content );

    secp256k1_sha256_t ctx;
    secp256k1_sha256_initialize( &ctx );
    secp256k1_sha256_write( &ctx, hash.data(), hash.size );
    secp256k1_sha256_write( &ctx, fileContentHash.data(), fileContentHash.size );

    dev::h256 commonFileHash;
    secp256k1_sha256_finalize( &ctx, commonFileHash.data() );

    BOOST_REQUIRE( calculatedHash == commonFileHash );
    BOOST_REQUIRE( boost::filesystem::exists( fileHashName ) );

    remove( ( pathToFile.parent_path() / fileHashName ).c_str() );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
