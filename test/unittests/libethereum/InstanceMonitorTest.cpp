#include <libethereum/InstanceMonitor.h>
#include <test/tools/libtesteth/TestHelper.h>

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <json.hpp>

using namespace dev;
using namespace dev::eth;
using namespace dev::test;

namespace fs = boost::filesystem;

class InstanceMonitorMock: public InstanceMonitor {
public:
    explicit InstanceMonitorMock(fs::path const &config) : InstanceMonitor(config) {};

    uint64_t getFinishTimestamp() {
        return this->finishTimestamp();
    };

    bool getIsExit() {
        return this->isExit();
    };

    fs::path getRotationFilePath() {
        return this->rotationFilePath();
    }
};

class InstanceMonitorTestFixture : public TestOutputHelperFixture {
public:
    fs::path configPath = "testConfig.json";

    InstanceMonitorTestFixture() : instanceMonitor( configPath ) {
        std::fstream file;
        file.open( configPath.string(), std::ios::out );
        rotationFilePath = instanceMonitor.getRotationFilePath();
    }

    InstanceMonitorMock instanceMonitor;
    fs::path rotationFilePath;

    ~InstanceMonitorTestFixture() override {
        if (fs::exists(configPath)) {
            fs::remove(configPath);
        }
        if (fs::exists(rotationFilePath)) {
            fs::remove(rotationFilePath);
        }
    };
};

BOOST_FIXTURE_TEST_SUITE( InstanceMonitorSuite, InstanceMonitorTestFixture )

BOOST_AUTO_TEST_CASE( test_initRotationParams ) {
    uint64_t ts = 100;
    bool isExit = false;
    BOOST_REQUIRE( !fs::exists( instanceMonitor.getRotationFilePath() ) );
    instanceMonitor.initRotationParams(ts, isExit);
    BOOST_CHECK_EQUAL(instanceMonitor.getIsExit(), isExit);
    BOOST_CHECK_EQUAL(instanceMonitor.getFinishTimestamp(), ts);

    BOOST_REQUIRE( fs::exists( instanceMonitor.getRotationFilePath() ) );


    std::ifstream rotateFile( instanceMonitor.getRotationFilePath().string() );
    auto rotateJson = nlohmann::json::parse( rotateFile );
    BOOST_CHECK_EQUAL(rotateJson["isExit"].get< bool >(), isExit);
    BOOST_CHECK_EQUAL(rotateJson["timestamp"].get< uint64_t >(), ts);
}

BOOST_AUTO_TEST_CASE( test_isTimeToRotate_false ) {
    uint64_t currentTime = 100;
    uint64_t finishTime = 200;
    BOOST_REQUIRE( !instanceMonitor.isTimeToRotate( currentTime ) );
    instanceMonitor.initRotationParams(finishTime, false);
    BOOST_REQUIRE( !instanceMonitor.isTimeToRotate( currentTime ) );
}

BOOST_AUTO_TEST_CASE( test_isTimeToRotate_true ) {
    uint64_t currentTime = 100;

    BOOST_REQUIRE( !instanceMonitor.isTimeToRotate( currentTime ) );

    instanceMonitor.initRotationParams(100, false);
    BOOST_REQUIRE( instanceMonitor.isTimeToRotate( currentTime ) );

    instanceMonitor.initRotationParams(50, false);
    BOOST_REQUIRE( instanceMonitor.isTimeToRotate( currentTime ) );
}

BOOST_AUTO_TEST_CASE( test_exiting ) {
    instanceMonitor.initRotationParams(0, true);
    instanceMonitor.performRotation();
    BOOST_REQUIRE( ExitHandler::getSignal() == SIGTERM );
    BOOST_REQUIRE( !( fs::exists( instanceMonitor.getRotationFilePath() ) ) );
}

BOOST_AUTO_TEST_CASE( test_restarting ) {
    fs::path newConfigPath = configPath;
    newConfigPath += ".tmp";
    std::ofstream newConfig;
    newConfig.open(newConfigPath.string());
    newConfig << 1;
    newConfig.close();
    instanceMonitor.initRotationParams(0, false);
    instanceMonitor.performRotation();
    BOOST_REQUIRE( ExitHandler::getSignal() == SIGTERM );
    BOOST_REQUIRE( !( fs::exists( instanceMonitor.getRotationFilePath() ) ) );
    BOOST_REQUIRE( !( fs::exists( newConfigPath) ) );
    std::ifstream config(configPath.string() );
    int val;
    config >> val;
    BOOST_REQUIRE(val == 1 );
}

BOOST_AUTO_TEST_SUITE_END()