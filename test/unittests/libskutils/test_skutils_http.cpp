#include <test/tools/libtesteth/TestHelper.h>
#include "test_skutils_helper.h"
#include <boost/test/unit_test.hpp>
#include <test/tools/libtesteth/TestHelper.h>

BOOST_AUTO_TEST_SUITE( SkUtils )
BOOST_AUTO_TEST_SUITE( http, *boost::unit_test::precondition( dev::test::option_all_tests ) )

BOOST_AUTO_TEST_CASE( http_server_startup ) {
    skutils::test::test_print_header_name( "SkUtils/http/http_server_startup" );
    skutils::test::test_protocol_server_startup( "http_async", skutils::test::g_nDefaultPort );
}
BOOST_AUTO_TEST_CASE( http_server_startup_sync ) {
    skutils::test::test_print_header_name( "SkUtils/http/http_server_startup_sync" );
    skutils::test::test_protocol_server_startup( "http_sync", skutils::test::g_nDefaultPort );
}
BOOST_AUTO_TEST_CASE( proxygen_server_startup ) {
    skutils::test::test_print_header_name( "SkUtils/proxygen/proxygen_server_startup" );
    skutils::test::test_protocol_server_startup( "proxygen", skutils::test::g_nDefaultPortProxygen );
}

BOOST_AUTO_TEST_CASE( http_single_call ) {
    skutils::test::test_print_header_name( "SkUtils/http/http_single_call" );
    skutils::test::test_protocol_single_call( "http_async", skutils::test::g_nDefaultPort );
}
BOOST_AUTO_TEST_CASE( http_single_call_sync ) {
    skutils::test::test_print_header_name( "SkUtils/http/http_single_call_sync" );
    skutils::test::test_protocol_single_call( "http_sync", skutils::test::g_nDefaultPort );
}
BOOST_AUTO_TEST_CASE( proxygen_single_call ) {
    skutils::test::test_print_header_name( "SkUtils/proxygen/proxygen_single_call" );
    skutils::test::test_protocol_single_call( "proxygen", skutils::test::g_nDefaultPortProxygen );
}

BOOST_AUTO_TEST_CASE( http_serial_calls ) {
    skutils::test::test_print_header_name( "SkUtils/http/http_serial_calls" );
    skutils::test::test_protocol_serial_calls(
        "http_async", skutils::test::g_nDefaultPort, skutils::test::g_vecTestClientNamesA );
}
BOOST_AUTO_TEST_CASE( http_serial_calls_sync ) {
    skutils::test::test_print_header_name( "SkUtils/http/http_serial_calls_sync" );
    skutils::test::test_protocol_serial_calls(
        "http_sync", skutils::test::g_nDefaultPort, skutils::test::g_vecTestClientNamesA );
}
BOOST_AUTO_TEST_CASE( proxygen_serial_calls ) {
    skutils::test::test_print_header_name( "SkUtils/proxygen/proxygen_serial_calls" );
    skutils::test::test_protocol_serial_calls(
        "proxygen", skutils::test::g_nDefaultPortProxygen, skutils::test::g_vecTestClientNamesA );
}

BOOST_AUTO_TEST_CASE( http_parallel_calls ) {
    skutils::test::test_print_header_name( "SkUtils/http/http_parallel_calls" );
    skutils::test::test_protocol_parallel_calls(
        "http_async", skutils::test::g_nDefaultPort, skutils::test::g_vecTestClientNamesA );
}
BOOST_AUTO_TEST_CASE( http_parallel_calls_sync ) {
    skutils::test::test_print_header_name( "SkUtils/http/http_parallel_calls_sync" );
    skutils::test::test_protocol_parallel_calls(
        "http_sync", skutils::test::g_nDefaultPort, skutils::test::g_vecTestClientNamesA );
}
BOOST_AUTO_TEST_CASE( proxygen_parallel_calls ) {
    skutils::test::test_print_header_name( "SkUtils/proxygen/proxygen_parallel_calls" );
    skutils::test::test_protocol_parallel_calls(
        "proxygen", skutils::test::g_nDefaultPortProxygen, skutils::test::g_vecTestClientNamesA );
}

BOOST_AUTO_TEST_CASE( http_busy_port ) {
    skutils::test::test_print_header_name( "SkUtils/http/http_busy_port" );
    skutils::test::test_protocol_busy_port( "http", skutils::test::g_nDefaultPort );
}
//BOOST_AUTO_TEST_CASE( proxygen_busy_port ) {
//    skutils::test::test_print_header_name( "SkUtils/proxygen/proxygen_busy_port" );
//    skutils::test::test_protocol_busy_port( "proxygen", skutils::test::g_nDefaultPortProxygen );
//}

BOOST_AUTO_TEST_CASE( https_server_startup ) {
    skutils::test::test_print_header_name( "SkUtils/http/https_server_startup" );
    skutils::test::test_protocol_server_startup( "https_async", skutils::test::g_nDefaultPort );
}
BOOST_AUTO_TEST_CASE( https_server_startup_sync ) {
    skutils::test::test_print_header_name( "SkUtils/http/https_server_startup_sync" );
    skutils::test::test_protocol_server_startup( "https_sync", skutils::test::g_nDefaultPort );
}

BOOST_AUTO_TEST_CASE( https_single_call ) {
    skutils::test::test_print_header_name( "SkUtils/http/https_single_call" );
    skutils::test::test_protocol_single_call( "https_async", skutils::test::g_nDefaultPort );
}
BOOST_AUTO_TEST_CASE( https_single_call_sync ) {
    skutils::test::test_print_header_name( "SkUtils/http/https_single_call_sync" );
    skutils::test::test_protocol_single_call( "https_sync", skutils::test::g_nDefaultPort );
}

BOOST_AUTO_TEST_CASE( https_serial_calls ) {
    skutils::test::test_print_header_name( "SkUtils/http/https_serial_calls" );
    skutils::test::test_protocol_serial_calls(
        "https_async", skutils::test::g_nDefaultPort, skutils::test::g_vecTestClientNamesA );
}
BOOST_AUTO_TEST_CASE( https_serial_calls_sync ) {
    skutils::test::test_print_header_name( "SkUtils/http/https_serial_calls_sync" );
    skutils::test::test_protocol_serial_calls(
        "https_sync", skutils::test::g_nDefaultPort, skutils::test::g_vecTestClientNamesA );
}

BOOST_AUTO_TEST_CASE( https_parallel_calls ) {
    skutils::test::test_print_header_name( "SkUtils/http/https_parallel_calls" );
    skutils::test::test_protocol_parallel_calls(
        "https_async", skutils::test::g_nDefaultPort, skutils::test::g_vecTestClientNamesA );
}
BOOST_AUTO_TEST_CASE( https_parallel_calls_sync ) {
    skutils::test::test_print_header_name( "SkUtils/http/https_parallel_calls_sync" );
    skutils::test::test_protocol_parallel_calls(
        "https_sync", skutils::test::g_nDefaultPort, skutils::test::g_vecTestClientNamesA );
}

BOOST_AUTO_TEST_CASE( https_busy_port ) {
    skutils::test::test_print_header_name( "SkUtils/http/https_busy_port" );
    skutils::test::test_protocol_busy_port( "https", skutils::test::g_nDefaultPort );
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
