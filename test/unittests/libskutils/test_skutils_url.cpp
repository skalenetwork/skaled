#include <skutils/url.h>
#include <test/tools/libtesteth/TestHelper.h>
#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

BOOST_AUTO_TEST_SUITE( SkUtils )
BOOST_AUTO_TEST_SUITE( url_address_classification )

static void checkLocalPrivate( const std::vector< std::string >& addresses, bool expected ) {
    for ( const auto& address : addresses )
        BOOST_CHECK_MESSAGE( skutils::is_local_private_network_address( address ) == expected,
            "is_local_private_network_address(\"" << address << "\") must be "
                                                  << ( expected ? "true" : "false" ) );
}

BOOST_AUTO_TEST_CASE( private_ipv4_ranges_are_local ) {
    checkLocalPrivate(
        { "10.0.0.0", "10.255.255.255", "127.0.0.1", "127.255.255.255", "172.16.0.0", "172.20.1.1",
            "172.31.255.255", "192.168.0.0", "192.168.1.1", "192.168.255.255" },
        true );
}

BOOST_AUTO_TEST_CASE( public_ipv4_ranges_are_not_local ) {
    // boundaries of 172.16.0.0/12 and 192.168.0.0/16, and public ranges inside 172/8 and 192/8
    checkLocalPrivate( { "9.255.255.255", "11.0.0.0", "8.8.8.8", "172.15.255.255", "172.32.0.0",
                           "172.64.0.1", "172.104.1.1", "192.0.2.1", "192.99.1.1",
                           "192.167.255.255", "192.169.0.0", "192.241.1.1" },
        false );
}

BOOST_AUTO_TEST_CASE( private_ipv6_ranges_are_local ) {
    checkLocalPrivate( { "::1", "fc00::1", "fd12:3456::1", "fe80::1", "febf::1" }, true );
}

BOOST_AUTO_TEST_CASE( public_ipv6_ranges_are_not_local ) {
    checkLocalPrivate( { "2a01:4f8::1", "2001:db8::1", "2001:4860:4860::8888", "2600::1", "fb00::1",
                           "fec0::1", "ff02::1", "::", "::ffff:8.8.8.8" },
        false );
}

BOOST_AUTO_TEST_CASE( malformed_addresses_are_not_local ) {
    checkLocalPrivate(
        { "", "localhost", "10", "10.0.0", "10.0.0.0.1", "127.0.0.1:80", "[::1]", "10.0.0.256",
            "fc00::1::2", "not-an-ip", "010.0.0.1", " 10.0.0.1", "10.0.0.1 ", "fe80::1%lo" },
        false );
}

BOOST_AUTO_TEST_CASE( ipv4_mapped_ipv6_follows_ipv4_ranges ) {
    checkLocalPrivate(
        { "::ffff:10.0.0.1", "::ffff:127.0.0.1", "::ffff:172.16.0.1", "::ffff:192.168.1.1" },
        true );
    checkLocalPrivate( { "::ffff:8.8.8.8", "::ffff:172.104.1.1", "::ffff:192.99.1.1" }, false );
}

static void checkLoopback( const std::vector< std::string >& addresses, bool expected ) {
    for ( const auto& address : addresses )
        BOOST_CHECK_MESSAGE( skutils::is_loopback_address( address ) == expected,
            "is_loopback_address(\"" << address << "\") must be "
                                     << ( expected ? "true" : "false" ) );
}

BOOST_AUTO_TEST_CASE( loopback_addresses ) {
    checkLoopback( { "127.0.0.1", "127.0.0.2", "127.1.2.3", "127.255.255.255", "::1",
                       "0:0:0:0:0:0:0:1", "::ffff:127.0.0.1", "::ffff:127.1.2.3" },
        true );
}

BOOST_AUTO_TEST_CASE( non_loopback_addresses ) {
    // private and link-local ranges are not loopback either
    checkLoopback(
        { "126.255.255.255", "128.0.0.1", "0.0.0.0", "10.0.0.1", "172.16.0.1", "172.17.0.1",
            "192.168.1.1", "8.8.8.8", "::", "::2", "::0:1:1", "fe80::1", "fc00::1", "fd00::1",
            "2a01:4f8::1", "::ffff:8.8.8.8", "::ffff:10.0.0.1", "::127.0.0.1", "64:ff9b::7f00:1" },
        false );
}

BOOST_AUTO_TEST_CASE( malformed_addresses_are_not_loopback ) {
    checkLoopback( { "", "localhost", "127", "127.1", "127.0.0", "0177.0.0.1", "0x7f.0.0.1",
                       "127.0.0.1:80", "[::1]", "[::1]:80", "::1%lo", " 127.0.0.1", "127.0.0.1 ",
                       "127.0.0.1.attacker.example", "2130706433" },
        false );
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
