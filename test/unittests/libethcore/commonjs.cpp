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
/** @file commonjs.cpp
 * @author Marek Kotewicz <marek@ethdev.com>
 * @date 2014
 */

#include <libdevcore/Log.h>
#include <libethcore/CommonJS.h>
#include <test/tools/libtesteth/TestOutputHelper.h>
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::test;

BOOST_FIXTURE_TEST_SUITE( commonjs, TestOutputHelperFixture )

BOOST_AUTO_TEST_CASE( jsToPublic, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    KeyPair kp = KeyPair::create();
    string s = toJS( kp.pub() );
    Public pub = dev::jsToPublic( s );
    BOOST_CHECK_EQUAL( kp.pub(), pub );
}

BOOST_AUTO_TEST_CASE( jsToAddress, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    KeyPair kp = KeyPair::create();
    string s = toJS( kp.address() );
    Address address = dev::jsToAddress( s );
    BOOST_CHECK_EQUAL( kp.address(), address );
}

BOOST_AUTO_TEST_CASE( jsToSecret, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    KeyPair kp = KeyPair::create();
    string s = toJS( kp.secret().makeInsecure() );
    Secret secret = dev::jsToSecret( s );
    BOOST_CHECK_EQUAL( kp.secret().makeInsecure(), secret.makeInsecure() );
}

BOOST_AUTO_TEST_SUITE_END()
