#include "test_skutils_helper.h"
#include <test/tools/libtesteth/TestHelper.h>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE( SkUtils )
BOOST_AUTO_TEST_SUITE( unddos, *boost::unit_test::precondition( dev::test::option_all_tests ) )

static std::string get_test_unddos_settings() {
    std::string test = R"(
{
		"origins": [
			{
				"origin": [
					"11.11.11.11"
				],
				"ban_lengthy": 10 ,
				"ban_peak": 5,
				"max_calls_per_minute": 10,
				"max_calls_per_second": 3,
				"max_ws_conn": 20000
			},
			{
				"origin": [
					"*"
				],
				"ban_lengthy": 30,
				"ban_peak": 10,
				"max_calls_per_minute": 60000,
				"max_calls_per_second": 1000,
				"max_ws_conn": 20000
			}
		],
		"global": {
			"ban_lengthy": 30,
			"ban_peak": 10,
			"max_calls_per_minute": 300000,
			"max_calls_per_second": 5000,
			"max_ws_conn": 20000
		}
	}
)";

    return test;
}

BOOST_AUTO_TEST_CASE( basic_counting ) {
    skutils::unddos::algorithm unddos;
    auto test_settings = get_test_unddos_settings();
    nlohmann::json jsonObj = nlohmann::json::parse( test_settings );
    unddos.load_settings_from_json( jsonObj );
    skutils::unddos::time_tick_mark ttmNow = skutils::unddos::now_tick_mark();
    BOOST_REQUIRE( unddos.register_call_from_origin( "11.11.11.11", ttmNow ) ==
                   skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    BOOST_REQUIRE( unddos.register_call_from_origin( "11.11.11.11", ttmNow ) ==
                   skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    BOOST_REQUIRE( unddos.register_call_from_origin( "11.11.11.11", ttmNow ) ==
                   skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    BOOST_REQUIRE( unddos.register_call_from_origin( "11.11.11.11", ttmNow ) ==
                   skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    BOOST_REQUIRE( unddos.register_call_from_origin( "11.11.11.11", ttmNow ) !=
                   skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    ++ttmNow;
    BOOST_REQUIRE( unddos.register_call_from_origin( "11.11.11.11", ttmNow ) !=
                   skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    ttmNow += 60;
    BOOST_REQUIRE( unddos.register_call_from_origin( "11.11.11.11", ttmNow ) ==
                   skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    ttmNow += 60;
    for ( size_t i = 0; i < 10; ++i ) {
        ++ttmNow;
        BOOST_REQUIRE( unddos.register_call_from_origin( "11.11.11.11", ttmNow ) ==
                       skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
    }
    BOOST_REQUIRE( unddos.register_call_from_origin( "11.11.11.11", ttmNow ) !=
                   skutils::unddos::e_high_load_detection_result_t::ehldr_no_error );
}


BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
