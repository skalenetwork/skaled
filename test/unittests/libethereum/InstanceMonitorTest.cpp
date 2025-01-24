#include <libethereum/InstanceMonitor.h>
#include <test/tools/libtesteth/TestHelper.h>

#include <libdevcore/StatusAndControl.h>

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <json.hpp>

using namespace dev;
using namespace dev::eth;
using namespace dev::test;

namespace fs = boost::filesystem;

class InstanceMonitorMock: public InstanceMonitor {
public:
    explicit InstanceMonitorMock(fs::path const &rotationFlagFilePath, std::shared_ptr<StatusAndControl> statusAndControl) : InstanceMonitor(rotationFlagFilePath, statusAndControl) {};

    fs::path getRotationInfoFilePath() {
        return this->rotationInfoFilePath();
    }

    void createFlagFileTest(){
        this->reportExitTimeReached( true );
    }

    void removeFlagFileTest(){
        this->reportExitTimeReached( false );
    }

    uint64_t getRotationTimestamp() const {
        return this->rotationTimestamp();
    }
};

class InstanceMonitorTestFixture : public TestOutputHelperFixture {
public:
    fs::path rotationFlagDirPath = "test";

    InstanceMonitorTestFixture() {
        fs::create_directory(rotationFlagDirPath);

        statusAndControlFile = std::make_shared<StatusAndControlFile>(rotationFlagDirPath);
        instanceMonitor = new InstanceMonitorMock(rotationFlagDirPath, statusAndControlFile);

        rotationFilePath = instanceMonitor->getRotationInfoFilePath();
    }

    InstanceMonitorMock* instanceMonitor;
    std::shared_ptr<StatusAndControl> statusAndControlFile;
    fs::path rotationFilePath;

    ~InstanceMonitorTestFixture() override {
        if (fs::exists(rotationFilePath)) {
            fs::remove(rotationFilePath);
        }
        if (fs::exists(rotationFlagDirPath/"skaled.status")) {
            fs::remove(rotationFlagDirPath/"skaled.status");
        }
        if (fs::exists(rotationFlagDirPath)) {
            fs::remove(rotationFlagDirPath);
        }
    };
};

BOOST_FIXTURE_TEST_SUITE( InstanceMonitorSuite, InstanceMonitorTestFixture )

BOOST_AUTO_TEST_CASE( test_initRotationParams ) {
    uint64_t ts = 100;
    BOOST_REQUIRE( !fs::exists(instanceMonitor->getRotationInfoFilePath() ) );
    instanceMonitor->initRotationParams(ts);
    BOOST_CHECK_EQUAL(instanceMonitor->getRotationTimestamp(), ts);

    BOOST_REQUIRE( fs::exists(instanceMonitor->getRotationInfoFilePath() ) );


    std::ifstream rotateFile(instanceMonitor->getRotationInfoFilePath().string() );
    auto rotateJson = nlohmann::json::parse( rotateFile );
    BOOST_CHECK_EQUAL(rotateJson["timestamp"].get< uint64_t >(), ts);
}


BOOST_AUTO_TEST_CASE( test_isTimeToRotate_invalid_file ) {
    uint64_t currentTime = 100;
    std::ofstream rotationInfoFile(instanceMonitor->getRotationInfoFilePath().string() );
    rotationInfoFile << "Broken file";
    BOOST_REQUIRE( !instanceMonitor->isTimeToRotate( currentTime ) );
}


BOOST_AUTO_TEST_CASE( test_isTimeToRotate_false ) {
    uint64_t currentTime = 100;
    uint64_t finishTime = 200;
    BOOST_REQUIRE( !instanceMonitor->isTimeToRotate( currentTime ) );
    instanceMonitor->initRotationParams(finishTime);
    BOOST_REQUIRE( !instanceMonitor->isTimeToRotate( currentTime ) );
}

BOOST_AUTO_TEST_CASE( test_isTimeToRotate_true ) {
    uint64_t currentTime = 100;

    BOOST_REQUIRE( !instanceMonitor->isTimeToRotate( currentTime ) );

    instanceMonitor->initRotationParams(100);
    BOOST_REQUIRE( instanceMonitor->isTimeToRotate( currentTime ) );

    instanceMonitor->initRotationParams(50);
    BOOST_REQUIRE( instanceMonitor->isTimeToRotate( currentTime ) );

    currentTime = 49;
    BOOST_REQUIRE( !instanceMonitor->isTimeToRotate( currentTime ) );
}

BOOST_AUTO_TEST_CASE( test_rotation ) {
    instanceMonitor->initRotationParams(0);
        instanceMonitor->prepareRotation();

    BOOST_REQUIRE( statusAndControlFile->getExitState(StatusAndControl::ExitTimeReached) );
    BOOST_REQUIRE( !( fs::exists(instanceMonitor->getRotationInfoFilePath() ) ) );
}

BOOST_AUTO_TEST_CASE( test_create_remove_flag_file ) {
    instanceMonitor->createFlagFileTest();
    BOOST_REQUIRE( statusAndControlFile->getExitState(StatusAndControl::ExitTimeReached) );

    instanceMonitor->removeFlagFileTest();
    BOOST_REQUIRE( !( fs::exists(instanceMonitor->getRotationInfoFilePath() ) ) );
    BOOST_REQUIRE( !statusAndControlFile->getExitState(StatusAndControl::ExitTimeReached) );
}

BOOST_AUTO_TEST_SUITE_END()
