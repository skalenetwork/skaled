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
/** @file Common.h
 * @author Marek Kotewicz <marek@ethdev.com>
 * @date 2015
 */

#pragma once

#include <json/json.h>
#include <libdevcore/Log.h>
#include <string>

#include <test/tools/libtesteth/Options.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wnonnull-compare"

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

#pragma GCC diagnostic pop

namespace dev {
namespace test {

boost::filesystem::path getTestPath();
int randomNumber();
Json::Value loadJsonFromFile( boost::filesystem::path const& _path );
boost::filesystem::path toTestFilePath( std::string const& _filename );
boost::filesystem::path getRandomPath();

boost::unit_test::assertion_result option_all_tests( boost::unit_test::test_unit_id );
boost::unit_test::assertion_result run_not_express( boost::unit_test::test_unit_id );

}  // namespace test

}  // namespace dev
