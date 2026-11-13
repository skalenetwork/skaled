#include <skutils/url.h>
#include <test/tools/libtesteth/TestHelper.h>
#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

BOOST_AUTO_TEST_SUITE( SkUtils )
BOOST_AUTO_TEST_SUITE( url_address_classification )

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

BOOST_AUTO_TEST_CASE( proxied_client_addresses ) {
    // a loopback peer is the proxy on this host, so its X-Real-IP names the client
    BOOST_CHECK_EQUAL(
        skutils::proxied_client_address( "127.0.0.1", "203.0.113.7" ), "203.0.113.7" );
    BOOST_CHECK_EQUAL( skutils::proxied_client_address( "::1", " 2001:db8::1 " ), "2001:db8::1" );
    // any other peer is taken at its own address, whatever it sends
    BOOST_CHECK_EQUAL(
        skutils::proxied_client_address( "198.51.100.2", "203.0.113.7" ), "198.51.100.2" );
    for ( const char* real_ip :
        { "", "localhost", "203.0.113", "203.0.113.7:80", "[2001:db8::1]" } )
        BOOST_CHECK_EQUAL( skutils::proxied_client_address( "127.0.0.1", real_ip ), "127.0.0.1" );
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
