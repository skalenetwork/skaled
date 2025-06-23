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
/** @file
 * Fixture class for boost output when running testeth
 */

#pragma once
#include <test/tools/libtesteth/JsonSpiritHeaders.h>
#include <test/tools/libtestutils/Common.h>

namespace dev {
namespace test {

/**
 * @class TestOutputHelper
 * @brief A singleton class that provides utilities for managing and displaying test execution progress and statistics.
 *
 * This class is designed to assist in tracking the progress of test execution, displaying the percentage of completed tests,
 * and managing test-related metadata such as test names and file paths. It also provides functionality to measure and store
 * execution times for tests.
 *
 * @note This class cannot be copied or assigned due to the deleted copy constructor and assignment operator.
 */
class TestOutputHelper {
public:
    /**
     * @brief Get the singleton instance of TestOutputHelper.
     * @return Reference to the singleton instance.
     */
    static TestOutputHelper& get() {
        static TestOutputHelper instance;
        return instance;
    }

    // Prevent copying and assignment of the singleton instance.
    TestOutputHelper( TestOutputHelper const& ) = delete;
    void operator=( TestOutputHelper const& ) = delete;

    /**
     * @brief Initialize the test output helper for a new test run.
     * @param _maxTests The maximum number of tests to be executed. Default is 1.
     */
    void initTest( size_t _maxTests = 1 );

    /**
     * @brief Display the progress of test execution.
     *
     * This function calculates and displays the percentage of tests completed based on the current
     * test number and the maximum number of tests.
     */
    void showProgress();

    /**
     * @brief Finish the current test and print execution statistics.
     *
     * This function finalizes the current test execution, prints the execution statistics,
     * and resets the timer for the next test.
     */
    void finishTest();

    /**
     * @brief Check if the current test matches the specified test name.
     * @param _testName The name of the test to check against the current test.
     * @return False if `singleTest` is set and the current test name does not match `_testName`, otherwise true.
     * Sets the current test name to `_testName` when returning true.
     */
    bool shouldRunTest( std::string const& _testName );


    void setCurrentTestFile( boost::filesystem::path const& _name ) {
        m_currentTestFileName = _name;
    }
    void setCurrentTestName( std::string const& _name ) { m_currentTestName = _name; }
    std::string const& testName() { return m_currentTestName; }
    std::string const& caseName() { return m_currentTestCaseName; }
    boost::filesystem::path const& testFile() { return m_currentTestFileName; }
    void printTestExecStats();

private:
    TestOutputHelper() {}
    Timer m_timer;
    size_t m_currTest;
    size_t m_maxTests;
    std::string m_currentTestName;
    std::string m_currentTestCaseName;
    boost::filesystem::path m_currentTestFileName;
    typedef std::pair< double, std::string > execTimeName;
    std::vector< execTimeName > m_execTimeResults;
};

class TestOutputHelperFixture {
public:
    TestOutputHelperFixture();
    virtual ~TestOutputHelperFixture();
};


}  // namespace test
}  // namespace dev
