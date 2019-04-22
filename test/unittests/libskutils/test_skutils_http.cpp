#include "test_skutils_helper.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE( SkUtils )
BOOST_AUTO_TEST_SUITE( http )

BOOST_AUTO_TEST_CASE( http_server_startup ) {
    skutils::test::test_print_header_name( "SkUtils/http/http_server_startup" );
    skutils::test::test_protocol_server_startup( "http", skutils::test::g_nDefaultPort );
}
BOOST_AUTO_TEST_CASE( http_single_call ) {
    skutils::test::test_print_header_name( "SkUtils/http/http_single_call" );
    skutils::test::test_protocol_single_call( "http", skutils::test::g_nDefaultPort );
}
BOOST_AUTO_TEST_CASE( http_serial_calls ) {
    skutils::test::test_print_header_name( "SkUtils/http/http_serial_calls" );
    skutils::test::test_protocol_serial_calls(
        "http", skutils::test::g_nDefaultPort, skutils::test::g_vecTestClientNamesA );
}
BOOST_AUTO_TEST_CASE( http_parallel_calls ) {
    skutils::test::test_print_header_name( "SkUtils/http/http_parallel_calls" );
    skutils::test::test_protocol_parallel_calls(
        "http", skutils::test::g_nDefaultPort, skutils::test::g_vecTestClientNamesA );
}

BOOST_AUTO_TEST_CASE( https_server_startup ) {
    skutils::test::test_print_header_name( "SkUtils/https/https_server_startup" );
    skutils::test::test_protocol_server_startup( "https", skutils::test::g_nDefaultPort );
}
BOOST_AUTO_TEST_CASE( https_single_call ) {
    skutils::test::test_print_header_name( "SkUtils/https/https_single_call" );
    skutils::test::test_protocol_single_call( "https", skutils::test::g_nDefaultPort );
}
BOOST_AUTO_TEST_CASE( https_serial_calls ) {
    skutils::test::test_print_header_name( "SkUtils/https/https_serial_calls" );
    skutils::test::test_protocol_serial_calls(
        "https", skutils::test::g_nDefaultPort, skutils::test::g_vecTestClientNamesA );
}
BOOST_AUTO_TEST_CASE( https_parallel_calls ) {
    skutils::test::test_print_header_name( "SkUtils/https/https_parallel_calls" );
    skutils::test::test_protocol_parallel_calls(
        "https", skutils::test::g_nDefaultPort, skutils::test::g_vecTestClientNamesA );
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
