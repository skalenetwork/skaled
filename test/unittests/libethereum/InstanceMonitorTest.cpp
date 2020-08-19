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
    explicit InstanceMonitorMock(fs::path const &rotationFlagPath) : InstanceMonitor(rotationFlagPath) {};

    uint64_t getFinishTimestamp() {
        return this->finishTimestamp();
    };

    fs::path getRotationFilePath() {
        return this->rotationFilePath();
    }

    fs::path getRotationFlagPath() {
        return this->m_rotationFlagPath;
    }

    void createFlagFileTest(){
        this->createFlagFile();
    }

    void removeFlagFileTest(){
        this->createFlagFile();
    }
};

class InstanceMonitorTestFixture : public TestOutputHelperFixture {
public:
    fs::path rotationFlagDirPath = "test";

    InstanceMonitorTestFixture() : instanceMonitor(rotationFlagDirPath) {
        std::fstream file;
        fs::create_directory(rotationFlagDirPath);
        rotationFilePath = instanceMonitor.getRotationFilePath();
    }

    InstanceMonitorMock instanceMonitor;
    fs::path rotationFilePath;

    ~InstanceMonitorTestFixture() override {
        if (fs::exists(rotationFilePath)) {
            fs::remove(rotationFilePath);
        }
        if (fs::exists(instanceMonitor.getRotationFlagPath())) {
            fs::remove(instanceMonitor.getRotationFlagPath());
        }
        if (fs::exists(rotationFlagDirPath)) {
            fs::remove(rotationFlagDirPath);
        }
    };
};

BOOST_FIXTURE_TEST_SUITE( InstanceMonitorSuite, InstanceMonitorTestFixture )

BOOST_AUTO_TEST_CASE( test_initRotationParams ) {
    uint64_t ts = 100;
    BOOST_REQUIRE( !fs::exists( instanceMonitor.getRotationFilePath() ) );
    instanceMonitor.initRotationParams(ts);
    BOOST_CHECK_EQUAL(instanceMonitor.getFinishTimestamp(), ts);

    BOOST_REQUIRE( fs::exists( instanceMonitor.getRotationFilePath() ) );


    std::ifstream rotateFile( instanceMonitor.getRotationFilePath().string() );
    auto rotateJson = nlohmann::json::parse( rotateFile );
    BOOST_CHECK_EQUAL(rotateJson["timestamp"].get< uint64_t >(), ts);
}

BOOST_AUTO_TEST_CASE( test_isTimeToRotate_false ) {
    uint64_t currentTime = 100;
    uint64_t finishTime = 200;
    BOOST_REQUIRE( !instanceMonitor.isTimeToRotate( currentTime ) );
    instanceMonitor.initRotationParams(finishTime);
    BOOST_REQUIRE( !instanceMonitor.isTimeToRotate( currentTime ) );
}

BOOST_AUTO_TEST_CASE( test_isTimeToRotate_true ) {
    uint64_t currentTime = 100;

    BOOST_REQUIRE( !instanceMonitor.isTimeToRotate( currentTime ) );

    instanceMonitor.initRotationParams(100);
    BOOST_REQUIRE( instanceMonitor.isTimeToRotate( currentTime ) );

    instanceMonitor.initRotationParams(50);
    BOOST_REQUIRE( instanceMonitor.isTimeToRotate( currentTime ) );
}

BOOST_AUTO_TEST_CASE( test_rotation ) {
    instanceMonitor.initRotationParams(0);
    instanceMonitor.performRotation();

    BOOST_REQUIRE( fs::exists( instanceMonitor.getRotationFlagPath() ) );
    BOOST_REQUIRE( ExitHandler::getSignal() == SIGTERM );
    BOOST_REQUIRE( !( fs::exists( instanceMonitor.getRotationFilePath() ) ) );
}

BOOST_AUTO_TEST_CASE( test_create_remove_flag_file ) {
    instanceMonitor.createFlagFileTest();
    BOOST_REQUIRE( fs::exists( instanceMonitor.getRotationFlagPath() ) );

    instanceMonitor.removeFlagFileTest();
    BOOST_REQUIRE( !( fs::exists( instanceMonitor.getRotationFilePath() ) ) );
}

BOOST_AUTO_TEST_SUITE_END()