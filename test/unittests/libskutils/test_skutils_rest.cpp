#include "test_skutils_helper.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE( SkUtils )
BOOST_AUTO_TEST_SUITE( rest )

BOOST_AUTO_TEST_SUITE( call )

BOOST_AUTO_TEST_CASE( http ) {
    skutils::test::test_print_header_name( "SkUtils/rest/call/http" );
    skutils::test::test_protocol_rest_call( "http", skutils::test::g_nDefaultPort );
}

BOOST_AUTO_TEST_CASE( https ) {
    skutils::test::test_print_header_name( "SkUtils/rest/call/https" );
    skutils::test::test_protocol_rest_call( "https", skutils::test::g_nDefaultPort );
}

BOOST_AUTO_TEST_CASE( ws ) {
    skutils::test::test_print_header_name( "SkUtils/rest/call/ws" );
    skutils::test::test_protocol_rest_call( "ws", skutils::test::g_nDefaultPort );
}

BOOST_AUTO_TEST_CASE( wss ) {
    skutils::test::test_print_header_name( "SkUtils/rest/call/wss" );
    skutils::test::test_protocol_rest_call( "wss", skutils::test::g_nDefaultPort );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( fail )

BOOST_AUTO_TEST_CASE( http ) {
    skutils::test::test_print_header_name( "SkUtils/rest/fail/http" );
    skutils::test::test_protocol_rest_fail( "http", "https", skutils::test::g_nDefaultPort );
}

BOOST_AUTO_TEST_CASE( https ) {
    skutils::test::test_print_header_name( "SkUtils/rest/fail/https" );
    skutils::test::test_protocol_rest_fail( "https", "http", skutils::test::g_nDefaultPort );
}

BOOST_AUTO_TEST_CASE( ws ) {
    skutils::test::test_print_header_name( "SkUtils/rest/fail/ws" );
    skutils::test::test_protocol_rest_fail( "ws", "wss", skutils::test::g_nDefaultPort );
}

BOOST_AUTO_TEST_CASE( wss ) {
    skutils::test::test_print_header_name( "SkUtils/rest/fail/wss" );
    skutils::test::test_protocol_rest_fail( "wss", "ws", skutils::test::g_nDefaultPort );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
