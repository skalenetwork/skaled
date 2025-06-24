/*
    Modifications Copyright (C) 2018-2019 SKALE Labs

    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
 */
/** @file
 * Base functions for all test suites
 */

#include <test/tools/libtesteth/JsonSpiritHeaders.h>
#include <test/tools/libtesteth/Stats.h>
#include <test/tools/libtesteth/TestHelper.h>
#include <test/tools/libtesteth/TestSuite.h>
#include <boost/algorithm/string.hpp>
#include <string>
using namespace std;
using namespace dev;
namespace fs = boost::filesystem;

// Helper functions for test proccessing
namespace {
struct TestFileData {
    json_spirit::mValue data;
    h256 hash;
};

void removeComments( json_spirit::mValue& _obj ) {
    if ( _obj.type() == json_spirit::obj_type ) {
        list< string > removeList;
        for ( auto& i : _obj.get_obj() ) {
            if ( i.first.substr( 0, 2 ) == "//" ) {
                removeList.push_back( i.first );
                continue;
            }

            removeComments( i.second );
        }
        for ( auto& i : removeList )
            _obj.get_obj().erase( _obj.get_obj().find( i ) );
    } else if ( _obj.type() == json_spirit::array_type ) {
        for ( auto& i : _obj.get_array() )
            removeComments( i );
    }
}

TestFileData readTestFile( fs::path const& _testFileName ) {
    TestFileData testData;
    string const s = dev::contentsString( _testFileName );
    BOOST_REQUIRE_MESSAGE( s.length() > 0, "Contents of " + _testFileName.string() + " is empty." );

    if ( _testFileName.extension() == ".json" )
        json_spirit::read_string( s, testData.data );
    else if ( _testFileName.extension() == ".yml" )
        testData.data = test::parseYamlToJson( s );
    else
        BOOST_ERROR( "Unknow test format!" + test::TestOutputHelper::get().testFile().string() );

    string srcString = json_spirit::write_string( testData.data, false );
    if ( test::Options::get().showhash )
        std::cout << "'" << srcString << "'" << std::endl;
    testData.hash = sha3( srcString );
    return testData;
}

void addClientInfo(
    json_spirit::mValue& _v, fs::path const& _testSource, h256 const& _testSourceHash ) {
    for ( auto& i : _v.get_obj() ) {
        json_spirit::mObject& o = i.second.get_obj();
        json_spirit::mObject clientinfo;

        string comment;
        if ( o.count( "_info" ) ) {
            json_spirit::mObject& existingInfo = o["_info"].get_obj();
            if ( existingInfo.count( "comment" ) )
                comment = existingInfo["comment"].get_str();
        }

        clientinfo["filledwith"] = test::prepareVersionString();
        clientinfo["lllcversion"] = test::prepareLLLCVersionString();
        clientinfo["source"] = _testSource.string();
        clientinfo["sourceHash"] = toString( _testSourceHash );
        clientinfo["comment"] = comment;
        o["_info"] = clientinfo;
    }
}

void checkFillerHash( fs::path const& _compiledTest, fs::path const& _sourceTest ) {
    json_spirit::mValue filledTest;
    string const s = dev::contentsString( _compiledTest );
    BOOST_REQUIRE_MESSAGE( s.length() > 0, "Contents of " + _compiledTest.string() + " is empty." );
    json_spirit::read_string( s, filledTest );

    TestFileData fillerData = readTestFile( _sourceTest );
    for ( auto& i : filledTest.get_obj() ) {
        BOOST_REQUIRE_MESSAGE( i.second.type() == json_spirit::obj_type,
            i.first + " should contain an object under a test name." );
        json_spirit::mObject const& obj = i.second.get_obj();
        BOOST_REQUIRE_MESSAGE(
            obj.count( "_info" ) > 0, "_info section not set! " + _compiledTest.string() );
        json_spirit::mObject const& info = obj.at( "_info" ).get_obj();
        BOOST_REQUIRE_MESSAGE( info.count( "sourceHash" ) > 0,
            "sourceHash not found in " + _compiledTest.string() + " in " + i.first );
        h256 const sourceHash = h256( info.at( "sourceHash" ).get_str() );
        BOOST_CHECK_MESSAGE( sourceHash == fillerData.hash,
            "Test " + _compiledTest.string() + " in " + i.first +
                " is outdated. Filler hash is different! ( '" + sourceHash.hex().substr( 0, 4 ) +
                "' != '" + fillerData.hash.hex().substr( 0, 4 ) + "') " );
    }
}

}  // namespace

namespace dev {
namespace test {

// Postifx names for the test files (appended to the end of test file names)
string const c_fillerPostf = "Filler";
string const c_copierPostf = "Copier";

void TestSuite::runTestWithoutFiller( boost::filesystem::path const& _file ) const {
    // Allow to execute a custom test .json file on any test suite
    auto& testOutput = test::TestOutputHelper::get();
    testOutput.initTest( 1 );
    executeFile( _file );
    testOutput.finishTest();
}

bool TestSuite::isFileSuffixedBy(const std::string& _file, const std::string& _suffix) const {
    return _file.size() > _suffix.size() && _file.substr(0, _suffix.size()) == _suffix;
}

void TestSuite::runAllTestsInFolder( string const& _testFolder ) const {
    // check that destination folder test files has according Filler file in src folder
    string const filter = test::Options::get().singleTestName.empty() ?
                              string() :
                              test::Options::get().singleTestName;
    vector< fs::path > const compiledFiles =
        test::getFiles( getFullPath( _testFolder ), {".json", ".yml"}, filter );

    std::vector<std::string> expectedFilePrefixes = {
        c_fillerPostf + ".json",
        c_fillerPostf + ".yml",
        c_copierPostf + ".json"
    };

    for (auto const& file : compiledFiles) {
        std::vector<fs::path> existingFiles;
        for (const auto& prefix : expectedFilePrefixes) {
            fs::path expectedFile = stripExtensionAndAppendPrefix(_testFolder, file, prefix);
            if (fs::exists(expectedFile)) {
                existingFiles.push_back(expectedFile);
            }
        }

        // Ensure at least one expected file exists
        BOOST_REQUIRE_MESSAGE(existingFiles.size() > 0,
            "The compiled test folder contains a test file (" + file.filename().string() +
            ") that does not have a corresponding Filler.json, Filler.yml, or Copier.json file in the source folder.");

        // Ensure not all three files exist simultaneously
        BOOST_REQUIRE_MESSAGE(existingFiles.size() != expectedFilePrefixes.size(),
            "Source test file must be one of Filler.json, Filler.yml, or Copier.json, but not all three: " +
            file.filename().string());

        // Check filled tests created from actual fillers
        if (!Options::get().filltests) {
            for (auto const& expectedFile : existingFiles) {
                checkFillerHash(file, expectedFile);
            }
        }
    }

    // run all tests
    vector< fs::path > files = test::getFiles( getFullPathFiller( _testFolder ),
        {".json", ".yml"}, filter.empty() ? filter : filter + "Filler" );

    // Filter out test files based on MIRAGE flag for files with the same prefix
    if ( filter.empty() ) {
        
        const std::string DEFAULT = "D";
        const std::string MIRAGE_PREFIX = "MIRAGE_";
        
        // Group files by their name excluding any suffixes
        // file name -> suffix -> file path
        std::unordered_map<std::string, std::unordered_map<std::string, fs::path>> filesByPrefix;
        for (auto const& file : files) {
            std::string fileName = file.stem().string();
            std::string fileNameWithoutPrefix;
            std::string prefix;
            
            if (isFileSuffixedBy(fileName, MIRAGE_PREFIX)) {
                prefix = MIRAGE_PREFIX;
                fileNameWithoutPrefix = fileName.substr(MIRAGE_PREFIX.size());
            }
            else { 
                prefix = DEFAULT;
                fileNameWithoutPrefix = fileName;
            }
            filesByPrefix[fileNameWithoutPrefix][prefix] = file;
        }

        std::vector<fs::path> filteredFiles;
        for (auto const& entry : filesByPrefix) {
            const auto& prefixMap = entry.second;
            const bool hasFiles = prefixMap.size();
            if (hasFiles) {

#ifdef MIRAGE
                const bool hasMirage = prefixMap.find(MIRAGE_PREFIX) != prefixMap.end();
                if (hasMirage) {
                    filteredFiles.push_back(prefixMap.at(MIRAGE_PREFIX));
                } else {
                    filteredFiles.push_back(prefixMap.at(DEFAULT));
                }
#else
                filteredFiles.push_back(prefixMap.at(DEFAULT));
#endif

            }
        }
        files = filteredFiles;
        std::cout << "Filtered test files: " << files << std::endl;
    }

    auto& testOutput = test::TestOutputHelper::get();
    testOutput.initTest( files.size() );
    for ( auto const& file : files ) {
        testOutput.showProgress();
        testOutput.setCurrentTestFile( file );
        executeTest( _testFolder, file );
    }
    testOutput.finishTest();
}

fs::path TestSuite::getFullPathFiller( string const& _testFolder ) const {
    return test::getTestPath() / "src" / suiteFillerFolder() / _testFolder;
}

fs::path TestSuite::getFullPath( string const& _testFolder ) const {
    return test::getTestPath() / suiteFolder() / _testFolder;
}

fs::path TestSuite::stripExtensionAndAppendPrefix( const std::string& _folder, fs::path const& _file, std::string const& _prefix) const {
    return getFullPathFiller( _folder ) / fs::path(_file.stem().string() + _prefix);
}


void TestSuite::executeTest( string const& _testFolder, fs::path const& _testFileName ) const {
    fs::path const boostRelativeTestPath = fs::relative( _testFileName, getTestPath() );
    string testname = _testFileName.stem().string();
    bool isCopySource = false;
    if ( testname.rfind( c_fillerPostf ) != string::npos )
        testname = testname.substr( 0, testname.rfind( "Filler" ) );
    else if ( testname.rfind( c_copierPostf ) != string::npos ) {
        testname = testname.substr( 0, testname.rfind( c_copierPostf ) );
        isCopySource = true;
    } else
        BOOST_REQUIRE_MESSAGE(
            false, "Incorrect file suffix in the filler folder! " + _testFileName.string() );

    // Filename of the test that would be generated
    std::cout << "test target name: " << testname << std::endl;
    std::cout << "test source name: " << _testFileName.string() << std::endl;

    fs::path boostTestPath = getFullPath( _testFolder ) / fs::path( testname + ".json" );

#ifdef MIRAGE // append MIRAGE_ suffix to all filled tests
    // If the original testname does not start with "MIRAGE_", generate as "MIRAGE_testname.json"
    const std::string MIRAGE_PREFIX = "MIRAGE_";
    const bool hasMiragePrefix = testname.size() > MIRAGE_PREFIX.size() &&
        testname.substr(0, MIRAGE_PREFIX.size()) == MIRAGE_PREFIX;
    if (!hasMiragePrefix) {
        boostTestPath = getFullPath( _testFolder ) / fs::path( MIRAGE_PREFIX + testname + ".json" );
    }
#endif

    std::cout << "test output name: " << boostTestPath.string() << std::endl;

    // TODO: An old unmaintained way to gather execution stats needs review.
    if ( Options::get().stats )
        Listener::registerListener( Stats::get() );

    if ( Options::get().filltests ) {
        if ( isCopySource ) {
            clog << "Copying " << _testFileName.string() << "\n";
            clog << " TO " << boostTestPath.string() << "\n";
            assert( _testFileName.string() != boostTestPath.string() );
            TestOutputHelper::get().showProgress();
            dev::test::copyFile( _testFileName, boostTestPath );
            BOOST_REQUIRE_MESSAGE( boost::filesystem::exists( boostTestPath.string() ),
                "Error when copying the test file!" );

            // Update _info and build information of the copied test
            json_spirit::mValue v;
            string const s = asString( dev::contents( boostTestPath ) );
            json_spirit::read_string( s, v );
            addClientInfo( v, boostRelativeTestPath, readTestFile( _testFileName ).hash );
            writeFile( boostTestPath, asBytes( json_spirit::write_string( v, true ) ) );
        } else {
            if ( !Options::get().singleTest )
                cnote << "Populating tests...";

            std::cout << "test name: " << testname << std::endl;
            TestFileData fillerData = readTestFile( _testFileName );
            removeComments( fillerData.data );
            json_spirit::mValue output = doTests( fillerData.data, true );
            addClientInfo( output, boostRelativeTestPath, fillerData.hash );
            writeFile( boostTestPath, asBytes( json_spirit::write_string( output, true ) ) );
        }
    }

    // Test is generated. Now run it and check that there should be no errors
    if ( Options::get().verbosity > 1 )
        std::cout << "TEST " << testname << ":\n";

    Listener::notifySuiteStarted( testname );  // Outdated logging
    executeFile( boostTestPath );
}

void TestSuite::executeFile( boost::filesystem::path const& _file ) const {
    json_spirit::mValue v;
    string const s = asString( dev::contents( _file ) );
    BOOST_REQUIRE_MESSAGE(
        s.length() > 0, "Contents of " << _file.string()
                                       << " is empty. Have you cloned the 'tests' repo branch "
                                          "develop and set ETHEREUM_TEST_PATH to its path?" );
    json_spirit::read_string( s, v );
    doTests( v, false );
}

}  // namespace test
}  // namespace dev
