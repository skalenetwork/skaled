#include <libdevcore/StatusAndControl.h>
#include <test/tools/libtesteth/TestHelper.h>

#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <json.hpp>

using namespace dev;
using namespace dev::eth;
using namespace dev::test;

namespace fs = boost::filesystem;

class StatusAndControlTestFixture : public TestOutputHelperFixture {
public:
    fs::path statusFolderPath = "test-status";
    std::string statusFileName = "skaled.status";

    StatusAndControlTestFixture() {
        fs::create_directory( statusFolderPath );

        statusFilePath = statusFolderPath / "skaled.status";
    }

    fs::path statusFilePath;

    ~StatusAndControlTestFixture() override {
        return;
        if ( fs::exists( statusFilePath ) ) {
            fs::remove( statusFilePath );
        }
        if ( fs::exists( statusFolderPath / "skaled.status" ) ) {
            fs::remove( statusFolderPath / "skaled.status" );
        }
        if ( fs::exists( statusFolderPath ) ) {
            fs::remove( statusFolderPath );
        }
    };
};

BOOST_FIXTURE_TEST_SUITE( StatusAndControlSuite, StatusAndControlTestFixture )


nlohmann::json::object_t readJson( const fs::path& path ) {
    std::ifstream File( path );
    std::stringstream Buffer;
    Buffer << File.rdbuf();
    File.close();
    return nlohmann::json::parse( Buffer.str() );
}


BOOST_AUTO_TEST_CASE( test_status_file_creation ) {
    auto Status = std::make_shared< StatusAndControlFile >( statusFolderPath, statusFileName );

    // Set and verify exit states
    Status->setExitState( StatusAndControl::ExitState::ClearDataDir, true );
    Status->setExitState( StatusAndControl::ExitState::StartAgain, true );
    Status->setExitState( StatusAndControl::ExitState::StartFromSnapshot, true );
    Status->setExitState( StatusAndControl::ExitState::ExitTimeReached, true );

    // Set and verify subsystem running states
    Status->setSubsystemRunning( StatusAndControl::Subsystem::SnapshotDownloader, true );
    Status->setSubsystemRunning( StatusAndControl::Subsystem::WaitingForTimestamp, true );
    Status->setSubsystemRunning( StatusAndControl::Subsystem::Blockchain, true );
    Status->setSubsystemRunning( StatusAndControl::Subsystem::Rpc, true );

    // Verify all fields in JSON
    auto StatusJson = readJson( statusFilePath );

    // Verify subsystemRunning
    BOOST_REQUIRE( StatusJson["subsystemRunning"]["SnapshotDownloader"] == true );
    BOOST_REQUIRE( StatusJson["subsystemRunning"]["WaitingForTimestamp"] == true );
    BOOST_REQUIRE( StatusJson["subsystemRunning"]["Blockchain"] == true );
    BOOST_REQUIRE( StatusJson["subsystemRunning"]["Rpc"] == true );

    // Verify exitState
    BOOST_REQUIRE( StatusJson["exitState"]["ClearDataDir"] == true );
    BOOST_REQUIRE( StatusJson["exitState"]["StartAgain"] == true );
    BOOST_REQUIRE( StatusJson["exitState"]["StartFromSnapshot"] == true );
    BOOST_REQUIRE( StatusJson["exitState"]["ExitTimeReached"] == true );
}

BOOST_AUTO_TEST_SUITE_END()
