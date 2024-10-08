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

#include <filesystem>

using namespace std;
using namespace std::filesystem;
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

class AddressRangeTestFixture : public TestOutputHelperFixture {
public:
    AddressRangeTestFixture() {
        // get some random addresses and their hashes
        for ( unsigned i = 0; i < addressCount; ++i ) {
            Address addr{i};
            hashToAddress[sha3( addr )] = addr;
        }

        // create accounts in the state
        State writer = state.createStateModifyCopy();
        for ( auto const& hashAndAddr : hashToAddress )
            writer.addBalance( hashAndAddr.second, 100 );
        writer.commit( dev::eth::CommitBehaviour::RemoveEmptyAccounts );

        writer.mutableHistoricState().saveRootForBlock(1);
    }

    TransientDirectory m_tempDirState;
    State state = State( 0, m_tempDirState.path(), h256{}, BaseState::Empty );
    unsigned const addressCount = 10;
    std::map< h256, Address > hashToAddress;
};

BOOST_FIXTURE_TEST_SUITE( StateAddressRangeTests, AddressRangeTestFixture )

BOOST_AUTO_TEST_CASE( LoadAccountCode ) {
    Address addr{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
    State s = state.createStateModifyCopy();
    s.createContract( addr );
    uint8_t codeData[] = {'c', 'o', 'd', 'e'};
    u256 version = 123;
    s.setCode( addr, {std::begin( codeData ), std::end( codeData )}, version );
    s.commit(dev::eth::CommitBehaviour::RemoveEmptyAccounts );

    s.mutableHistoricState().saveRootForBlock(2);
    s.mutableHistoricState().setRootByBlockNumber(2);

    auto& loadedCode = s.code( addr );
    BOOST_CHECK(
        std::equal( std::begin( codeData ), std::end( codeData ), std::begin( loadedCode ) ) );

    auto& loadedHistoricCode = s.mutableHistoricState().code( addr );
    BOOST_CHECK(
        std::equal( std::begin( codeData ), std::end( codeData ), std::begin( loadedHistoricCode ) ) );
}


BOOST_AUTO_TEST_CASE( addressesReturnsAllAddresses ) {

    State sr = state.createStateReadOnlyCopy();
    sr.mutableHistoricState().setRootFromDB();

    std::pair< State::AddressMap, h256 > addressesAndNextKey =
        sr.addresses( h256{}, addressCount * 2 );
    State::AddressMap addresses = addressesAndNextKey.first;

    BOOST_CHECK_EQUAL( addresses.size(), addressCount );
    BOOST_CHECK( addresses == hashToAddress );
    BOOST_CHECK_EQUAL( addressesAndNextKey.second, h256{} );

    std::pair< State::AddressMap, h256 > historicAddressesAndNextKey =
        sr.mutableHistoricState().addresses( h256{}, addressCount * 2 );
    State::AddressMap historicAddresses = historicAddressesAndNextKey.first;

    BOOST_CHECK_EQUAL( historicAddresses.size(), addressCount );
    BOOST_CHECK( historicAddresses == hashToAddress );
    BOOST_CHECK_EQUAL( historicAddressesAndNextKey.second, h256{} );
}

BOOST_AUTO_TEST_CASE( addressesReturnsNoMoreThanRequested ) {
    uint maxResults = 3;
    State sr = state.createStateReadOnlyCopy();
    sr.mutableHistoricState().setRootFromDB();

    // 1 check State
    std::pair< State::AddressMap, h256 > addressesAndNextKey =
        sr.addresses( h256{}, maxResults );
    State::AddressMap& addresses = addressesAndNextKey.first;
    h256& nextKey = addressesAndNextKey.second;

    BOOST_CHECK_EQUAL( addresses.size(), maxResults );
    auto itHashToAddressEnd = std::next( hashToAddress.begin(), maxResults );
    BOOST_CHECK( addresses == State::AddressMap( hashToAddress.begin(), itHashToAddressEnd ) );
    BOOST_CHECK_EQUAL( nextKey, itHashToAddressEnd->first );

    // request next chunk
    std::pair< State::AddressMap, h256 > addressesAndNextKey2 =
        sr.addresses( nextKey, maxResults );
    State::AddressMap& addresses2 = addressesAndNextKey2.first;
    BOOST_CHECK_EQUAL( addresses2.size(), maxResults );
    auto itHashToAddressEnd2 = std::next( itHashToAddressEnd, maxResults );
    BOOST_CHECK( addresses2 == State::AddressMap( itHashToAddressEnd, itHashToAddressEnd2 ) );

    // 2 check historic state
    addressesAndNextKey =
        sr.mutableHistoricState().addresses( h256{}, maxResults );
    addresses = addressesAndNextKey.first;
    h256& historicNextKey = addressesAndNextKey.second;

    BOOST_CHECK_EQUAL( addresses.size(), maxResults );
    itHashToAddressEnd = std::next( hashToAddress.begin(), maxResults );
    BOOST_CHECK( addresses == State::AddressMap( hashToAddress.begin(), itHashToAddressEnd ) );
    BOOST_CHECK_EQUAL( historicNextKey, itHashToAddressEnd->first );

    // request next chunk
    addressesAndNextKey2 =
        sr.mutableHistoricState().addresses( nextKey, maxResults );
    State::AddressMap& historicAddresses2 = addressesAndNextKey2.first;
    BOOST_CHECK_EQUAL( historicAddresses2.size(), maxResults );
    itHashToAddressEnd2 = std::next( itHashToAddressEnd, maxResults );
    BOOST_CHECK( historicAddresses2 == State::AddressMap( itHashToAddressEnd, itHashToAddressEnd2 ) );
}

BOOST_AUTO_TEST_CASE( addressesDoesntReturnDeletedInCache ) {
    State s = state.createStateReadOnlyCopy();

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
    State s = state.createStateReadOnlyCopy();

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

class DbRotationFixture : public TestOutputHelperFixture {
public:
    DbRotationFixture() {
        state.mutableHistoricState().saveRootForBlock( 0 );
    }
    TransientDirectory m_tempDirState;
    State state = State( 0, m_tempDirState.path(), h256{}, BaseState::Empty, 0, 32, 1 );
    Address address1{1}, address2{2};
    size_t countDbPieces(){
        auto di = directory_iterator(m_tempDirState.path() + "/historic_state/00000000/state");
        return std::distance(begin(di), end(di));
    }
};

BOOST_FIXTURE_TEST_SUITE( DbRotationSuite, DbRotationFixture )

// write, then read from historic state
BOOST_AUTO_TEST_CASE( writeAndRead ) {
    State sw = state.createStateModifyCopyAndPassLock();

    sw.incNonce(address1);
    BOOST_CHECK_EQUAL(sw.getNonce(address1), 1);
    sw.incNonce(address1);
    BOOST_CHECK_EQUAL(sw.getNonce(address1), 2);

    sw.commit( dev::eth::CommitBehaviour::RemoveEmptyAccounts, 1001 );

    BOOST_CHECK_EQUAL(sw.getNonce(address1), 2);
    sw.incNonce(address1);
    BOOST_CHECK_EQUAL(sw.getNonce(address1), 3);
}

// make two changes in two blocks and try to access state in each block
BOOST_AUTO_TEST_CASE( twoChanges ) {
    State sw = state.createStateModifyCopyAndPassLock();

    sw.mutableHistoricState().rotateDbsIfNeeded( 1001 );
    sw.incNonce(address1);
    sw.commit( dev::eth::CommitBehaviour::RemoveEmptyAccounts, 1001 );
    sw.mutableHistoricState().saveRootForBlock( 1 );

    sw.mutableHistoricState().rotateDbsIfNeeded( 1002 );
    sw.incNonce( address2 );
    sw.commit( dev::eth::CommitBehaviour::RemoveEmptyAccounts, 1002 );
    sw.mutableHistoricState().saveRootForBlock( 2 );

    // check that in block 0 we have nonce 0/0, block 1 - 1/0, block 2 - 1/1

    sw.mutableHistoricState().setRootByBlockNumber( 0 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address1 ), 0 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address2 ), 0 );

    sw.mutableHistoricState().setRootByBlockNumber( 1 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address1 ), 1 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address2 ), 0 );

    sw.mutableHistoricState().setRootByBlockNumber( 2 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address1 ), 1 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address2 ), 1 );

    // check that rotation happened
    BOOST_CHECK_EQUAL(countDbPieces(), 2);
}

// same, but add empty block between
BOOST_AUTO_TEST_CASE( twoChangesWithInterval ) {
    State sw = state.createStateModifyCopyAndPassLock();

    sw.mutableHistoricState().rotateDbsIfNeeded( 1001 );
    sw.commit( dev::eth::CommitBehaviour::RemoveEmptyAccounts, 1001 );
    sw.mutableHistoricState().saveRootForBlock( 1 );

    sw.mutableHistoricState().rotateDbsIfNeeded( 1002 );
    sw.incNonce(address1);
    sw.commit( dev::eth::CommitBehaviour::RemoveEmptyAccounts, 1002 );
    sw.mutableHistoricState().saveRootForBlock( 2 );

    sw.mutableHistoricState().rotateDbsIfNeeded( 1003 );
    sw.commit( dev::eth::CommitBehaviour::RemoveEmptyAccounts, 1003 );
    sw.mutableHistoricState().saveRootForBlock( 3 );

    sw.mutableHistoricState().rotateDbsIfNeeded( 1004 );
    sw.incNonce( address2 );
    sw.commit( dev::eth::CommitBehaviour::RemoveEmptyAccounts, 1004 );
    sw.mutableHistoricState().saveRootForBlock( 4 );

    sw.mutableHistoricState().rotateDbsIfNeeded( 1005 );
    sw.commit( dev::eth::CommitBehaviour::RemoveEmptyAccounts, 1005 );
    sw.mutableHistoricState().saveRootForBlock( 5 );

    // check that in block 0 and 1 we have nonce 0/0, block 2 and 3 - 1/0, block 4 and 5 - 1/1

    sw.mutableHistoricState().setRootByBlockNumber( 0 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address1 ), 0 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address2 ), 0 );

    sw.mutableHistoricState().setRootByBlockNumber( 1 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address1 ), 0 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address2 ), 0 );

    sw.mutableHistoricState().setRootByBlockNumber( 2 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address1 ), 1 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address2 ), 0 );

    sw.mutableHistoricState().setRootByBlockNumber( 3 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address1 ), 1 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address2 ), 0 );

    sw.mutableHistoricState().setRootByBlockNumber( 4 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address1 ), 1 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address2 ), 1 );

    sw.mutableHistoricState().setRootByBlockNumber( 5 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address1 ), 1 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address2 ), 1 );

    // check that rotation happened
    BOOST_CHECK_EQUAL(countDbPieces(), 5);
}

// change 1 address 2 times in 2 blocks, check it on all blocks
BOOST_AUTO_TEST_CASE( update ) {
    State sw = state.createStateModifyCopyAndPassLock();

    sw.mutableHistoricState().rotateDbsIfNeeded( 1001 );
    sw.incNonce(address1);
    sw.commit( dev::eth::CommitBehaviour::RemoveEmptyAccounts, 1001 );
    sw.mutableHistoricState().saveRootForBlock( 1 );

    sw.mutableHistoricState().rotateDbsIfNeeded( 1002 );
    sw.incNonce( address1 );
    sw.commit( dev::eth::CommitBehaviour::RemoveEmptyAccounts, 1002 );
    sw.mutableHistoricState().saveRootForBlock( 2 );

    // check that in block 0 we have nonce 0/0, block 1 - 1/0, block 2 - 1/1

    sw.mutableHistoricState().setRootByBlockNumber( 0 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address1 ), 0 );

    sw.mutableHistoricState().setRootByBlockNumber( 1 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address1 ), 1 );

    sw.mutableHistoricState().setRootByBlockNumber( 2 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().getNonce( address1 ), 2 );

    // check that rotation happened
    BOOST_CHECK_EQUAL(countDbPieces(), 2);
}

// change storage of 1 address 2 times in 2 blocks, check it on all blocks
// with unrelated blocks in between
BOOST_AUTO_TEST_CASE( updateStorage ) {
    h256 location = h256(123456);

    State sw = state.createStateModifyCopy();

    sw.mutableHistoricState().rotateDbsIfNeeded( 1001 );

    sw.incNonce(address1);
    sw.commit( dev::eth::CommitBehaviour::RemoveEmptyAccounts, 1001 );
    sw.mutableHistoricState().saveRootForBlock( 1 );

    sw.mutableHistoricState().rotateDbsIfNeeded( 1002 );

    sw.setStorage(address1, location, 1);
    sw.commit( dev::eth::CommitBehaviour::RemoveEmptyAccounts, 1002 );
    sw.mutableHistoricState().saveRootForBlock( 2 );

    sw.mutableHistoricState().rotateDbsIfNeeded( 1003 );

    sw.setStorage(address1, location, 2);
    sw.commit( dev::eth::CommitBehaviour::RemoveEmptyAccounts, 1003 );
    sw.mutableHistoricState().saveRootForBlock( 3 );

    state.mutableHistoricState().setRoot(sw.mutableHistoricState().globalRoot());

    // check

    sw.mutableHistoricState().setRootByBlockNumber( 1 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().storage(address1, location), u256(0) );

    sw.mutableHistoricState().setRootByBlockNumber( 2 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().storage(address1, location), u256(1) );

    sw.mutableHistoricState().setRootByBlockNumber( 3 );
    BOOST_CHECK_EQUAL(sw.mutableHistoricState().storage(address1, location), u256(2) );

    // check that rotation happened
    BOOST_CHECK_EQUAL(countDbPieces(), 3);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

}  // namespace test
}  // namespace dev
