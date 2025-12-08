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

#include <boost/test/framework.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/test/results_collector.hpp>

#include <test/tools/jsontests/BlockChainTests.h>
#include <test/tools/jsontests/StateTests.h>
#include <test/tools/jsontests/TransactionTests.h>
#include <test/tools/jsontests/vm.h>
#include <test/tools/libtesteth/TestHelper.h>
#include <clocale>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <thread>

using namespace boost::unit_test;

namespace {

class FailureSummaryObserver : public test_observer {
public:
    enum class Status { Failed, Errored, Aborted };

    void setRunLabel( std::string _label ) { mRunLabel = std::move( _label ); }

    void test_unit_finish( test_unit const& unit, unsigned long ) override {
        if ( unit.p_type != boost::unit_test::TUT_CASE )
            return;

        auto const& results = boost::unit_test::results_collector.results( unit.p_id );
        if ( results.passed() || results.skipped() )
            return;

        Entry entry;
        entry.name = unit.full_name();
        entry.status = classify( results );
        entry.details = buildDetails( results );
        mEntries.push_back( std::move( entry ) );
    }

    void test_finish() override { storeAndPrintCurrentRun(); }

    int priority() override { return 6; }

    static void printAggregatedReport() {
        if ( sRunSummaries.empty() )
            return;

        std::cout << "\n=== Aggregated failing test summary (" << sRunSummaries.size()
                  << " run(s)) ===" << std::endl;
        for ( auto const& run : sRunSummaries ) {
            std::cout << "Run: " << run.label << std::endl;
            if ( run.entries.empty() ) {
                std::cout << "  No failed/errored/aborted test cases." << std::endl;
                continue;
            }

            std::cout << "  Failed/errored/aborted test cases (" << run.entries.size()
                      << "):" << std::endl;
            for ( auto const& entry : run.entries ) {
                std::cout << "   - " << entry.name << " [" << statusToString( entry.status )
                          << "]";
                if ( !entry.details.empty() )
                    std::cout << ": " << entry.details;
                std::cout << std::endl;
            }
        }
    }

private:
    struct Entry {
        std::string name;
        Status status;
        std::string details;
    };

    static Status classify( boost::unit_test::test_results const& results ) {
        if ( results.aborted() || results.p_test_cases_aborted > 0 || results.p_test_cases_timed_out > 0 )
            return Status::Aborted;
        if ( results.p_assertions_failed > 0 )
            return Status::Failed;
        return Status::Errored;
    }

    static std::string buildDetails( boost::unit_test::test_results const& results ) {
        std::vector< std::string > parts;
        auto failedAssertions = static_cast< unsigned long >( results.p_assertions_failed );
        if ( failedAssertions > 0 )
            parts.push_back( std::to_string( failedAssertions ) + " assertion(s) failed" );
        auto warningCount = static_cast< unsigned long >( results.p_warnings_failed );
        if ( warningCount > 0 )
            parts.push_back( std::to_string( warningCount ) + " warning(s)" );
        if ( results.p_test_cases_timed_out > 0 )
            parts.push_back( "timed out" );
        if ( results.aborted() )
            parts.push_back( "aborted" );

        std::string summary;
        for ( size_t i = 0; i < parts.size(); ++i ) {
            if ( i > 0 )
                summary += ", ";
            summary += parts[i];
        }
        return summary;
    }

    static const char* statusToString( Status status ) {
        switch ( status ) {
        case Status::Failed:
            return "failed";
        case Status::Errored:
            return "errored";
        case Status::Aborted:
            return "aborted";
        }
        return "unknown";
    }
    struct RunSummary {
        std::string label;
        std::vector< Entry > entries;
    };

    void storeAndPrintCurrentRun() {
        RunSummary summary{mRunLabel.empty() ? std::string( "default-run" ) : mRunLabel, mEntries};
        printSummary( summary );
        sRunSummaries.push_back( std::move( summary ) );
        mEntries.clear();
    }

    static void printSummary( RunSummary const& summary ) {
        std::cout << "\n=== Test execution summary (" << summary.label << ") ===\n";
        if ( summary.entries.empty() ) {
            std::cout << "No failed/errored/aborted test cases detected." << std::endl;
            return;
        }

        std::cout << "Failed/errored/aborted test cases (" << summary.entries.size() << "):" << std::endl;
        for ( auto const& entry : summary.entries ) {
            std::cout << " - " << entry.name << " [" << statusToString( entry.status ) << "]";
            if ( !entry.details.empty() )
                std::cout << ": " << entry.details;
            std::cout << std::endl;
        }
    }
    std::vector< Entry > mEntries;
    std::string mRunLabel;
    static std::vector< RunSummary > sRunSummaries;
};

std::unique_ptr< FailureSummaryObserver > gFailureSummaryObserver = std::make_unique< FailureSummaryObserver >();
std::vector< FailureSummaryObserver::RunSummary > FailureSummaryObserver::sRunSummaries;

}  // namespace

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
    std::string runLabel = opt.rCurrentTestSuite.empty() ? std::string( "full-workflow" ) : opt.rCurrentTestSuite;
    if ( opt.singleTestFile.is_initialized() )
        runLabel += ":" + opt.singleTestFile.get();
    gFailureSummaryObserver->setRunLabel( runLabel );
    framework::register_observer( *gFailureSummaryObserver );
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
        FailureSummaryObserver::printAggregatedReport();
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
        FailureSummaryObserver::printAggregatedReport();
        return result;
    }
}
