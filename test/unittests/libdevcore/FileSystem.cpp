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
/** @file FileSystem.cpp
 * @date 2025
 * FileSystem test functions.
 */

#include <libdevcore/FileSystem.h>
#include <test/tools/libtesteth/TestOutputHelper.h>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <fstream>

using namespace dev;
using namespace dev::test;
namespace fs = boost::filesystem;

BOOST_FIXTURE_TEST_SUITE( FileSystemTests, TestOutputHelperFixture )

BOOST_AUTO_TEST_CASE(
    isDataDirEmpty_test, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    fs::path tempDir = fs::temp_directory_path() / fs::unique_path();
    fs::path originalDataDir;

    try {
        originalDataDir = getDataDir();
        setDataDir( tempDir );

        BOOST_CHECK( isDataDirEmpty() );

        fs::create_directories( getDataDir() );
        BOOST_CHECK( isDataDirEmpty() );

        fs::path testFile = getDataDir() / "test.txt";
        std::ofstream ofs( testFile.string() );
        ofs << "test content";
        ofs.close();
        BOOST_CHECK( !isDataDirEmpty() );

        fs::remove( testFile );
        fs::path subDir = getDataDir() / "subdir";
        fs::create_directory( subDir );
        BOOST_CHECK( !isDataDirEmpty() );

        fs::remove_all( getDataDir() );
        fs::path notADir = getDataDir();
        std::ofstream ofs2( notADir.string() );
        ofs2 << "not a directory";
        ofs2.close();
        BOOST_CHECK_THROW( isDataDirEmpty(), std::runtime_error );
    } catch ( ... ) {
        if ( fs::exists( tempDir ) ) {
            fs::remove_all( tempDir );
        }
        if ( !originalDataDir.empty() ) {
            setDataDir( originalDataDir );
        }
        throw;
    }

    if ( fs::exists( tempDir ) ) {
        fs::remove_all( tempDir );
    }
    if ( !originalDataDir.empty() ) {
        setDataDir( originalDataDir );
    }
}

BOOST_AUTO_TEST_SUITE_END()
