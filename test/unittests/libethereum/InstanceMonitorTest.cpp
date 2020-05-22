#include <libethereum/InstanceMonitor.h>
#include <test/tools/libtesteth/TestHelper.h>

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

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
    InstanceMonitorTestFixture() : instanceMonitor( configPath ) {
        std::fstream file;
        file.open( configPath.string(), std::ios::out );
        rotationFilePath = instanceMonitor.getRotationFilePath();
    }

    InstanceMonitorMock instanceMonitor;
    fs::path configPath = "testConfig.json";
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
    BOOST_REQUIRE(!fs::exists(instanceMonitor.getRotationFilePath()));
    instanceMonitor.initRotationParams(ts, isExit);
    BOOST_CHECK_EQUAL(instanceMonitor.getIsExit(), isExit);
    BOOST_CHECK_EQUAL(instanceMonitor.getFinishTimestamp(), ts);
    BOOST_REQUIRE(fs::exists(instanceMonitor.getRotationFilePath()));
}

BOOST_AUTO_TEST_SUITE_END()