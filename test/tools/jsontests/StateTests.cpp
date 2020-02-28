/*
    Modifications Copyright (C) 2018 SKALE Labs

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
/** @file StateTests.cpp
 * @author Dimitry Khokhlov <dimitry@ethereum.org>
 * @date 2016
 * General State Tests parser.
 */

#include <boost/filesystem/operations.hpp>
#include <boost/test/unit_test.hpp>

#include <json_spirit/JsonSpiritHeaders.h>
#include <libdevcore/CommonIO.h>
#include <libethereum/BlockChain.h>
#include <libethereum/Defaults.h>
#include <libethereum/ExtVM.h>
#include <libskale/State.h>
#include <test/tools/jsontests/StateTests.h>
#include <test/tools/libtesteth/TestHelper.h>
#include <test/tools/libtesteth/TestSuite.h>

using namespace std;
using namespace json_spirit;
using namespace dev;
using namespace dev::eth;
namespace fs = boost::filesystem;

namespace dev {
namespace test {

json_spirit::mValue StateTestSuite::doTests(
    json_spirit::mValue const& _input, bool _fillin ) const {
    BOOST_REQUIRE_MESSAGE(
        _input.type() == obj_type, TestOutputHelper::get().get().testFile().string() +
                                       " A GeneralStateTest file should contain an object." );
    BOOST_REQUIRE_MESSAGE( !_fillin || _input.get_obj().size() == 1,
        TestOutputHelper::get().testFile().string() +
            " A GeneralStateTest filler should contain only one test." );
    json_spirit::mValue v = json_spirit::mObject();

    for ( auto& i : _input.get_obj() ) {
        string const testname = i.first;
        BOOST_REQUIRE_MESSAGE(
            i.second.type() == obj_type, TestOutputHelper::get().testFile().string() +
                                             " should contain an object under a test name." );
        json_spirit::mObject const& inputTest = i.second.get_obj();
        v.get_obj()[testname] = json_spirit::mObject();
        json_spirit::mObject& outputTest = v.get_obj()[testname].get_obj();

        if ( _fillin && !TestOutputHelper::get().testFile().empty() )
            BOOST_REQUIRE_MESSAGE(
                testname + "Filler" == TestOutputHelper::get().testFile().stem().string(),
                TestOutputHelper::get().testFile().string() +
                    " contains a test with a different name '" + testname + "'" );

        if ( !TestOutputHelper::get().checkTest( testname ) )
            continue;

        BOOST_REQUIRE_MESSAGE( inputTest.count( "env" ) > 0, testname + " env not set!" );
        BOOST_REQUIRE_MESSAGE( inputTest.count( "pre" ) > 0, testname + " pre not set!" );
        BOOST_REQUIRE_MESSAGE(
            inputTest.count( "transaction" ) > 0, testname + " transaction not set!" );

        ImportTest importer( inputTest, outputTest );
        Listener::ExecTimeGuard guard{i.first};
        importer.executeTest( _fillin );

        if ( _fillin ) {
#if ETH_FATDB
            if ( inputTest.count( "_info" ) )
                outputTest["_info"] = inputTest.at( "_info" );

            if ( importer.exportTest() )
                cerr << testname << endl;
#else
            BOOST_THROW_EXCEPTION(
                Exception() << errinfo_comment(
                    testname + " You can not fill tests when FATDB is switched off" ) );
#endif
        } else {
            BOOST_REQUIRE_MESSAGE( inputTest.count( "post" ) > 0, testname + " post not set!" );
            BOOST_REQUIRE_MESSAGE( inputTest.at( "post" ).type() == obj_type,
                testname + " post field is not an object." );

            // check post hashes against cpp client on all networks
            bool foundResults = false;
            mObject post = inputTest.at( "post" ).get_obj();
            vector< size_t > wrongTransactionsIndexes;
            for ( mObject::const_iterator i = post.begin(); i != post.end(); ++i ) {
                BOOST_REQUIRE_MESSAGE( i->second.type() == array_type,
                    testname + " post field should contain an array for each network." );
                for ( auto const& exp : i->second.get_array() ) {
                    BOOST_REQUIRE_MESSAGE( exp.type() == obj_type,
                        " post field should contain an array of objects for each network." );
                    if ( !Options::get().singleTestNet.empty() &&
                         i->first != Options::get().singleTestNet )
                        continue;
                    if ( test::isDisabledNetwork( test::stringToNetId( i->first ) ) )
                        continue;
                    if ( importer.checkGeneralTestSection(
                             exp.get_obj(), wrongTransactionsIndexes, i->first ) )
                        foundResults = true;
                }
            }

            if ( !foundResults ) {
                Options const& opt = Options::get();
                BOOST_ERROR( "Transaction not found! (Network: " +
                             ( opt.singleTestNet.empty() ? "Any" : opt.singleTestNet ) +
                             ", dataInd: " + toString( opt.trDataIndex ) +
                             ", gasInd: " + toString( opt.trGasIndex ) +
                             ", valInd: " + toString( opt.trValueIndex ) + ")" );
            }

            if ( Options::get().statediff )
                importer.traceStateDiff();
        }
    }
    return v;
}

fs::path StateTestSuite::suiteFolder() const {
    return "GeneralStateTests";
}

fs::path StateTestSuite::suiteFillerFolder() const {
    return "GeneralStateTestsFiller";
}

}  // namespace test
}  // namespace dev

class GeneralTestFixture {
public:
    GeneralTestFixture() {
        test::StateTestSuite suite;
        string casename = boost::unit_test::framework::current_test_case().p_name;
        if ( casename == "stQuadraticComplexityTest" && !test::Options::get().all ) {
            std::cout << "Skipping " << casename << " because --all option is not specified.\n";
            return;
        }
        suite.runAllTestsInFolder( casename );
    }
};

BOOST_FIXTURE_TEST_SUITE( GeneralStateTests, GeneralTestFixture )

// Frontier Tests
BOOST_AUTO_TEST_CASE( stCallCodes, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
}
BOOST_AUTO_TEST_CASE(
    stCallCreateCallCodeTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stExample, *boost::unit_test::precondition( dev::test::run_not_express ) *
                   boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stInitCodeTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE( stLogTests, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stMemoryTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stPreCompiledContracts, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stPreCompiledContracts2, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE( stRandom, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE( stRandom2, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stRecursiveCreate, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stRefundTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stSolidityTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stSpecialTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stSystemOperationsTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stTransactionTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stTransitionTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stWalletTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}

// Homestead Tests
BOOST_AUTO_TEST_CASE( stCallDelegateCodesCallCodeHomestead,
    *boost::unit_test::precondition( dev::test::run_not_express ) *
        boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE( stCallDelegateCodesHomestead,
    *boost::unit_test::precondition( dev::test::run_not_express ) *
        boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stHomesteadSpecific, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stDelegatecallTestHomestead, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}

// EIP150 Tests
BOOST_AUTO_TEST_CASE(
    stChangedEIP150, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stEIP150singleCodeGasPrices, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stMemExpandingEIP150Calls, *boost::unit_test::precondition( dev::test::run_not_express ) *
                                   boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stEIP150Specific, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}

// EIP158 Tests
BOOST_AUTO_TEST_CASE(
    stEIP158Specific, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stNonZeroCallsTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stZeroCallsTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stZeroCallsRevert, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stCodeSizeLimit, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stCreateTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stRevertTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}

// Metropolis Tests
BOOST_AUTO_TEST_CASE(
    stStackTests, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stStaticCall, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stReturnDataTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stZeroKnowledge, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stZeroKnowledge2, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE( stCodeCopyTest ) {}
BOOST_AUTO_TEST_CASE( stBugs, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}

// Constantinople Tests
BOOST_AUTO_TEST_CASE( stShift, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}

// Stress Tests
BOOST_AUTO_TEST_CASE(
    stAttackTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stMemoryStressTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stQuadraticComplexityTest, *boost::unit_test::precondition( dev::test::run_not_express ) *
                                   boost::unit_test::precondition( dev::test::run_not_express ) ) {}

// Invalid Opcode Tests
BOOST_AUTO_TEST_CASE( stBadOpcode, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
}

// New Tests
BOOST_AUTO_TEST_CASE(
    stArgsZeroOneBalance, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_CASE(
    stEWASMTests, *boost::unit_test::precondition( dev::test::run_not_express ) ) {}
BOOST_AUTO_TEST_SUITE_END()
