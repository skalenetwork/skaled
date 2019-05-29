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
/// @file
/// State unit tests.

#include <libdevcore/TransientDirectory.h>
#include <libethcore/BasicAuthority.h>
#include <libethereum/Block.h>
#include <libethereum/BlockChain.h>
#include <libethereum/Defaults.h>
#include <test/tools/libtesteth/TestHelper.h>

using namespace std;
using namespace dev;
using namespace dev::eth;
using skale::BaseState;
using skale::State;

namespace dev {
namespace test {
BOOST_FIXTURE_TEST_SUITE( StateUnitTests, TestOutputHelperFixture )

BOOST_AUTO_TEST_CASE( Basic ) {
    Block s( Block::Null );
}

BOOST_AUTO_TEST_CASE( LoadAccountCode ) {
    Address addr{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
    State state( 0 );
    State s = state.startWrite();
    s.createContract( addr );
    uint8_t codeData[] = {'c', 'o', 'd', 'e'};
    s.setCode( addr, {std::begin( codeData ), std::end( codeData )} );
    s.commit( State::CommitBehaviour::RemoveEmptyAccounts );

    auto& loadedCode = s.code( addr );
    BOOST_CHECK(
        std::equal( std::begin( codeData ), std::end( codeData ), std::begin( loadedCode ) ) );
}

class AddressRangeTestFixture : public TestOutputHelperFixture {
public:
    AddressRangeTestFixture() {
        // get some random addresses and their hashes
        for ( unsigned i = 0; i < addressCount; ++i ) {
            Address addr{i};
            hashToAddress[sha3( addr )] = addr;
        }

        // create accounts in the state
        State writer = state.startWrite();
        for ( auto const& hashAndAddr : hashToAddress )
            writer.addBalance( hashAndAddr.second, 100 );
        writer.commit( State::CommitBehaviour::RemoveEmptyAccounts );
    }

    TransientDirectory m_tempDirState;
    State state = State( 0, m_tempDirState.path(), h256{}, BaseState::Empty );
    unsigned const addressCount = 10;
    std::map< h256, Address > hashToAddress;
};

BOOST_FIXTURE_TEST_SUITE( StateAddressRangeTests, AddressRangeTestFixture )


BOOST_AUTO_TEST_CASE( addressesReturnsAllAddresses ) {
    std::pair< State::AddressMap, h256 > addressesAndNextKey =
        state.startRead().addresses( h256{}, addressCount * 2 );
    State::AddressMap addresses = addressesAndNextKey.first;

    BOOST_CHECK_EQUAL( addresses.size(), addressCount );
    BOOST_CHECK( addresses == hashToAddress );
    BOOST_CHECK_EQUAL( addressesAndNextKey.second, h256{} );
}

BOOST_AUTO_TEST_CASE( addressesReturnsNoMoreThanRequested ) {
    int maxResults = 3;
    std::pair< State::AddressMap, h256 > addressesAndNextKey =
        state.startRead().addresses( h256{}, maxResults );
    State::AddressMap& addresses = addressesAndNextKey.first;
    h256& nextKey = addressesAndNextKey.second;

    BOOST_CHECK_EQUAL( addresses.size(), maxResults );
    auto itHashToAddressEnd = std::next( hashToAddress.begin(), maxResults );
    BOOST_CHECK( addresses == State::AddressMap( hashToAddress.begin(), itHashToAddressEnd ) );
    BOOST_CHECK_EQUAL( nextKey, itHashToAddressEnd->first );

    // request next chunk
    std::pair< State::AddressMap, h256 > addressesAndNextKey2 =
        state.startRead().addresses( nextKey, maxResults );
    State::AddressMap& addresses2 = addressesAndNextKey2.first;
    BOOST_CHECK_EQUAL( addresses2.size(), maxResults );
    auto itHashToAddressEnd2 = std::next( itHashToAddressEnd, maxResults );
    BOOST_CHECK( addresses2 == State::AddressMap( itHashToAddressEnd, itHashToAddressEnd2 ) );
}

BOOST_AUTO_TEST_CASE( addressesDoesntReturnDeletedInCache ) {
    State s = state.startRead();

    // delete some accounts
    unsigned deleteCount = 3;
    auto it = hashToAddress.begin();
    for ( unsigned i = 0; i < deleteCount; ++i, ++it )
        s.kill( it->second );
    // don't commmit

    std::pair< State::AddressMap, h256 > addressesAndNextKey =
        s.addresses( h256{}, addressCount * 2 );
    State::AddressMap& addresses = addressesAndNextKey.first;
    BOOST_CHECK_EQUAL( addresses.size(), addressCount - deleteCount );
    BOOST_CHECK( addresses == State::AddressMap( it, hashToAddress.end() ) );
}

BOOST_AUTO_TEST_CASE( addressesReturnsCreatedInCache ) {
    State s = state.startRead();

    // create some accounts
    unsigned createCount = 3;
    std::map< h256, Address > newHashToAddress;
    for ( unsigned i = addressCount; i < addressCount + createCount; ++i ) {
        Address addr{i};
        newHashToAddress[sha3( addr )] = addr;
    }

    // create accounts in the state
    for ( auto const& hashAndAddr : newHashToAddress )
        s.addBalance( hashAndAddr.second, 100 );
    // don't commmit

    std::pair< State::AddressMap, h256 > addressesAndNextKey =
        s.addresses( newHashToAddress.begin()->first, addressCount + createCount );
    State::AddressMap& addresses = addressesAndNextKey.first;
    for ( auto const& hashAndAddr : newHashToAddress )
        BOOST_CHECK( addresses.find( hashAndAddr.first ) != addresses.end() );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

}  // namespace test
}  // namespace dev
