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
 * Stub for generating main boost.test module.
 * Original code taken from boost sources.
 */


#define BOOST_TEST_MODULE EthereumTests
#define BOOST_TEST_NO_MAIN

#include <libdevcore/microprofile.h>
#include <libskale/UnsafeRegion.h>

#include <boost/test/included/unit_test.hpp>

#include <test/tools/jsontests/BlockChainTests.h>
#include <test/tools/jsontests/StateTests.h>
#include <test/tools/jsontests/TransactionTests.h>
#include <test/tools/jsontests/vm.h>
#include <test/tools/libtesteth/TestHelper.h>
#include <clocale>
#include <cstdlib>
#include <iostream>
#include <thread>

using namespace boost::unit_test;

// printer-visitor for --list-tests
struct TestTreeVisitor : test_tree_visitor {
    std::vector< std::string > stack;

    void visit( test_case const& test ) {
        // ignore stack[0] = "Master Test Suite"
        for ( size_t i = 1; i < stack.size(); ++i ) {
            std::cout << stack[i] << "/";
        }

        std::cout << test.p_name << std::endl;
    }

    bool test_suite_start( test_suite const& suite ) {
        stack.push_back( suite.p_name );
        return true;
    }

    void test_suite_finish( test_suite const& /*suite*/ ) { stack.pop_back(); }
};

static std::ostringstream strCout;
std::streambuf* oldCoutStreamBuf;
std::streambuf* oldCerrStreamBuf;

void customTestSuite() {
    // restore output for creating test
    std::cout.rdbuf( oldCoutStreamBuf );
    std::cerr.rdbuf( oldCerrStreamBuf );
    dev::test::Options const& opt = dev::test::Options::get();

    // if generating a random test
    if ( opt.createRandomTest ) {
        if ( !dev::test::createRandomTest() )
            throw framework::internal_error(
                "Create random test error! See std::error for more details." );
    }

    // if running a singletest
    if ( opt.singleTestFile.is_initialized() ) {
        boost::filesystem::path file( opt.singleTestFile.get() );
        if ( opt.rCurrentTestSuite.find( "GeneralStateTests" ) != std::string::npos ) {
            dev::test::StateTestSuite suite;
            suite.runTestWithoutFiller( file );
        } else if ( opt.rCurrentTestSuite.find( "BlockchainTests" ) != std::string::npos ) {
            dev::test::BlockchainTestSuite suite;
            suite.runTestWithoutFiller( file );
        } else if ( opt.rCurrentTestSuite.find( "TransitionTests" ) != std::string::npos ) {
            dev::test::TransitionTestsSuite suite;
            suite.runTestWithoutFiller( file );
        } else if ( opt.rCurrentTestSuite.find( "VMtests" ) != std::string::npos ) {
            dev::test::VmTestSuite suite;
            suite.runTestWithoutFiller( file );
        } else if ( opt.rCurrentTestSuite.find( "TransactionTests" ) != std::string::npos ) {
            dev::test::TransactionTestSuite suite;
            suite.runTestWithoutFiller( file );
        }
    }
}

void travisOut( std::atomic_bool* _stopTravisOut ) {
    int tickCounter = 0;
    while ( !*_stopTravisOut ) {
        std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
        ++tickCounter;
        if ( tickCounter % 10 == 0 )
            std::cout << ".\n" << std::flush;  // Output dot every 10s.
    }
}

/*
The equivalent of setlocale(LC_ALL, “C”) is called before any user code is run.
If the user has an invalid environment setting then it is possible for the call
to set locale to fail, so there are only two possible actions, the first is to
throw a runtime exception and cause the program to quit (default behaviour),
or the second is to modify the environment to something sensible (least
surprising behaviour).

The follow code produces the least surprising behaviour. It will use the user
specified default locale if it is valid, and if not then it will modify the
environment the process is running in to use a sensible default. This also means
that users do not need to install language packs for their OS.
*/
void setCLocale() {
#if __unix__
    if ( !std::setlocale( LC_ALL, "C" ) ) {
        setenv( "LC_ALL", "C", 1 );
    }
#endif
}

// Custom Boost Unit Test Main
int main( int argc, const char* argv[] ) {
    MicroProfileSetEnableAllGroups( true );
    UnsafeRegion::init(".");
    std::srand( time( nullptr ) );

    std::string const dynamicTestSuiteName = "customTestSuite";
    setCLocale();

    // Initialize options
    try {
        dev::test::Options::get( argc, argv );
    } catch ( dev::test::InvalidOption const& e ) {
        std::cerr << *boost::get_error_info< errinfo_comment >( e ) << "\n";
        exit( 1 );
    }

    dev::test::Options const& opt = dev::test::Options::get();
    if ( opt.createRandomTest || opt.singleTestFile.is_initialized() ) {
        bool testSuiteFound = false;
        for ( int i = 0; i < argc; i++ ) {
            // replace test suite to custom tests
            std::string arg = std::string{argv[i]};
            if ( arg == "-t" && i + 1 < argc ) {
                testSuiteFound = true;
                argv[i + 1] = ( char* ) dynamicTestSuiteName.c_str();
                break;
            }
        }

        // BOOST ERROR could not be used here because boost main is not initialized
        if ( !testSuiteFound && opt.createRandomTest ) {
            std::cerr << "createRandomTest requires a test suite to be set -t <TestSuite>\n";
            return -1;
        }
        if ( !testSuiteFound && opt.singleTestFile.is_initialized() ) {
            std::cerr
                << "singletest <file> <testname>  requires a test suite to be set -t <TestSuite>\n";
            return -1;
        }

        // Disable initial output as the random test will output valid json to std
        oldCoutStreamBuf = std::cout.rdbuf();
        oldCerrStreamBuf = std::cerr.rdbuf();
        std::cout.rdbuf( strCout.rdbuf() );
        std::cerr.rdbuf( strCout.rdbuf() );

        // add custom test suite
        test_suite* ts1 = BOOST_TEST_SUITE( "customTestSuite" );
        ts1->add( BOOST_TEST_CASE( &customTestSuite ) );
        framework::master_test_suite().add( ts1 );
    }

    if ( opt.listTests ) {
        TestTreeVisitor visitor;
        traverse_test_tree( framework::master_test_suite(), visitor, true );
        return 0;
    }

    std::cout << "Running tests using path: " << test::getTestPath() << std::endl;
    int result = 0;
    auto fakeInit = []( int, char* [] ) -> boost::unit_test::test_suite* { return nullptr; };
    if ( opt.jsontrace || opt.vmtrace || opt.statediff ) {
        // Do not use travis '.' output thread if debug is defined
        result = unit_test_main( fakeInit, argc, const_cast< char** >( argv ) );
        dev::test::TestOutputHelper::get().printTestExecStats();

        MicroProfileDumpFileImmediately( "profile.html", "profile.csv", nullptr );
        MicroProfileShutdown();

        return result;
    } else {
        // Initialize travis '.' output thread for log activity
        std::atomic_bool stopTravisOut{false};
        std::thread outputThread( travisOut, &stopTravisOut );
        result = unit_test_main( fakeInit, argc, const_cast< char** >( argv ) );
        stopTravisOut = true;
        outputThread.join();
        dev::test::TestOutputHelper::get().printTestExecStats();

        MicroProfileDumpFileImmediately( "profile.html", "profile.csv", nullptr );
        MicroProfileShutdown();

        return result;
    }
}
