#include "test_skutils_helper.h"
#include <boost/test/unit_test.hpp>
#include <test/tools/libtesteth/TestHelper.h>

BOOST_AUTO_TEST_SUITE( SkUtils )
BOOST_AUTO_TEST_SUITE( unddos, *boost::unit_test::precondition( dev::test::option_all_tests ) )

static skutils::unddos::settings compose_test_unddos_settings() {
    skutils::unddos::settings settings;
    //
    skutils::unddos::origin_dos_limits oe1;
    oe1.m_originWildcards.push_back("11.11.11.11" );
    oe1.m_defaultMaxCallsPerSec = 3;
    oe1.m_defaultMaxCallsPerMin = 10;
    oe1.m_maxWSConn = 2;
    oe1.m_banPerSecDuration = skutils::unddos::duration(5 );
    oe1.m_banPerMinDuration = skutils::unddos::duration(10 );
    skutils::unddos::origin_dos_limits oe2;
    oe2.load_unlim_for_localhost_only();
    return settings;
}

BOOST_AUTO_TEST_CASE( basic_counting ) {
    skutils::unddos::algorithm unddos;
    unddos.set_settings( compose_test_unddos_settings() );
    skutils::unddos::time_tick_mark ttmNow = skutils::unddos::now_tick_mark();
    BOOST_REQUIRE( unddos.register_call_from_origin( "11.11.11.11", ttmNow ) == skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    BOOST_REQUIRE( unddos.register_call_from_origin( "11.11.11.11", ttmNow ) == skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    BOOST_REQUIRE( unddos.register_call_from_origin( "11.11.11.11", ttmNow ) == skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    BOOST_REQUIRE( unddos.register_call_from_origin( "11.11.11.11", ttmNow ) != skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    ++ ttmNow;
    BOOST_REQUIRE( unddos.register_call_from_origin( "11.11.11.11", ttmNow ) != skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    ttmNow += 60;
    BOOST_REQUIRE( unddos.register_call_from_origin( "11.11.11.11", ttmNow ) == skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    ttmNow += 60;
    for( size_t i = 0; i < 10; ++ i ) {
        ++ ttmNow;
        BOOST_REQUIRE( unddos.register_call_from_origin( "11.11.11.11", ttmNow ) == skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    }
    BOOST_REQUIRE( unddos.register_call_from_origin( "11.11.11.11", ttmNow ) != skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
}

BOOST_AUTO_TEST_CASE( ws_conn_counting ) {
    skutils::unddos::algorithm unddos;
    unddos.set_settings( compose_test_unddos_settings() );
    BOOST_REQUIRE( ! unddos.unregister_ws_conn_for_origin( "11.11.11.11" ) );
    BOOST_REQUIRE( unddos.register_ws_conn_for_origin( "11.11.11.11" ) == skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    BOOST_REQUIRE( unddos.register_ws_conn_for_origin( "11.11.11.11" ) == skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    BOOST_REQUIRE( unddos.register_ws_conn_for_origin( "11.11.11.11" ) != skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    BOOST_REQUIRE( unddos.unregister_ws_conn_for_origin( "11.11.11.11" ) );
    BOOST_REQUIRE( unddos.unregister_ws_conn_for_origin( "11.11.11.11" ) );
    BOOST_REQUIRE( unddos.register_ws_conn_for_origin( "11.11.11.11" ) == skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()

