#include <test/tools/libtesteth/TestHelper.h>
#include "test_skutils_helper.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE( SkUtils )
BOOST_AUTO_TEST_SUITE( ws, *boost::unit_test::precondition( dev::test::option_all_tests ) )

BOOST_AUTO_TEST_CASE( ws_server_startup ) {
    skutils::test::test_print_header_name( "SkUtils/ws/ws_server_startup" );
    skutils::test::test_protocol_server_startup( "ws", skutils::test::g_nDefaultPort );
}

BOOST_AUTO_TEST_CASE( ws_single_call ) {
    skutils::test::test_print_header_name( "SkUtils/ws/ws_single_call" );
    skutils::test::test_protocol_single_call( "ws", skutils::test::g_nDefaultPort );
}

BOOST_AUTO_TEST_CASE( ws_serial_calls ) {
    skutils::test::test_print_header_name( "SkUtils/ws/ws_serial_calls" );
    skutils::test::test_protocol_serial_calls(
        "ws", skutils::test::g_nDefaultPort, skutils::test::g_vecTestClientNamesA );
}

BOOST_AUTO_TEST_CASE( ws_parallel_calls ) {
    skutils::test::test_print_header_name( "SkUtils/ws/ws_parallel_calls" );
    skutils::test::test_protocol_parallel_calls(
        "ws", skutils::test::g_nDefaultPort, skutils::test::g_vecTestClientNamesA );
}

BOOST_AUTO_TEST_CASE( ws_busy_port ) {
    skutils::test::test_print_header_name( "SkUtils/ws/ws_busy_port" );
    skutils::test::test_protocol_busy_port( "ws", skutils::test::g_nDefaultPort );
}

BOOST_AUTO_TEST_CASE( wss_server_startup ) {
    skutils::test::test_print_header_name( "SkUtils/ws/wss_server_startup" );
    skutils::test::test_protocol_server_startup( "wss", skutils::test::g_nDefaultPort );
}

BOOST_AUTO_TEST_CASE( wss_single_call ) {
    skutils::test::test_print_header_name( "SkUtils/ws/wss_single_call" );
    skutils::test::test_protocol_single_call( "wss", skutils::test::g_nDefaultPort );
}

BOOST_AUTO_TEST_CASE( wss_serial_calls ) {
    skutils::test::test_print_header_name( "SkUtils/ws/wss_serial_calls" );
    skutils::test::test_protocol_serial_calls(
        "wss", skutils::test::g_nDefaultPort, skutils::test::g_vecTestClientNamesA );
}

BOOST_AUTO_TEST_CASE( wss_parallel_calls ) {
    skutils::test::test_print_header_name( "SkUtils/ws/wss_parallel_calls" );
    skutils::test::test_protocol_parallel_calls(
        "wss", skutils::test::g_nDefaultPort, skutils::test::g_vecTestClientNamesB );
}

// TO-FIX: l_sergiy: fix SSL shutdown in client code, nevertheless this is not needed in skaled

BOOST_AUTO_TEST_CASE( wss_busy_port ) {
    skutils::test::test_print_header_name( "SkUtils/ws/wss_busy_port" );
    skutils::test::test_protocol_busy_port( "wss", skutils::test::g_nDefaultPort );
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
