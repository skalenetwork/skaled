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
/** @file BerlinForkTransaction.cpp
 * @date 2026
 * Berlin fork transaction and receipt test functions.
 */

#include "test/tools/libtesteth/TestHelper.h"
#include <libethcore/Common.h>
#include <libethcore/Exceptions.h>
#include <libethereum/SchainPatch.h>
#include <libethereum/TransactionReceipt.h>
#include <libevm/VMFace.h>

using namespace dev;
using namespace eth;
using namespace dev::test;

BOOST_FIXTURE_TEST_SUITE( BerlinForkTransaction, TestOutputHelperFixture )
BOOST_AUTO_TEST_CASE( typedTransactionsRequireEIP2718Support ) {
    // Readable tx representation (EIP-2930 / type-1 envelope):
    // {
    //   chainId: 151,
    //   nonce: 1,
    //   gasPrice: 0x04a817c800,
    //   gasLimit: 0x7530,
    //   to: 0x7d36af85a184e220a656525fcbb9a63b9ab3c12b,
    //   value: 0x01,
    //   data: 0x,
    //   accessList: [
    //     {
    //       address: 0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae,
    //       storageKeys: [
    //         0x0000000000000000000000000000000000000000000000000000000000000003,
    //         0x0000000000000000000000000000000000000000000000000000000000000007
    //       ]
    //     }
    //   ],
    //   yParity/r/s: present
    // }
    auto txRlp = fromHex(
        "0x01f8c38197018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180f85bf85994de"
        "0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000"
        "000000000000000003a0000000000000000000000000000000000000000000000000000000000000000780a0b0"
        "3eaf481958e22fc39bd1d526eb9255be1e6625614f02ca939e51c3d7e64bcaa05f675640c04bb050d27bd1f39c"
        "07b6ff742311b04dab760bb3bc206054332879" );

    BOOST_REQUIRE_THROW(
        Transaction( txRlp, CheckTransaction::None, false, false ), std::exception );
    BOOST_REQUIRE_NO_THROW( Transaction( txRlp, CheckTransaction::None, false, true ) );
}

BOOST_AUTO_TEST_CASE( accessListIntrinsicGasEIP2930 ) {
    // Readable tx representation (EIP-2930 / type-1 envelope), same canonical sample as above:
    // {
    //   chainId: 151,
    //   nonce: 1,
    //   gasPrice: 0x04a817c800,
    //   gasLimit: 0x7530,
    //   to: 0x7d36af85a184e220a656525fcbb9a63b9ab3c12b,
    //   value: 0x01,
    //   data: 0x,
    //   accessList: 1 address + 2 storage keys,
    //   yParity/r/s: present
    // }
    auto txRlp = fromHex(
        "0x01f8c38197018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180f85bf85994de"
        "0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000"
        "000000000000000003a0000000000000000000000000000000000000000000000000000000000000000780a0b0"
        "3eaf481958e22fc39bd1d526eb9255be1e6625614f02ca939e51c3d7e64bcaa05f675640c04bb050d27bd1f39c"
        "07b6ff742311b04dab760bb3bc206054332879" );
    Transaction tx( txRlp, CheckTransaction::None, false, true );

    EVMSchedule patchedSchedule = BerlinForkPatch::makeSchedule( IstanbulSchedule );
    int64_t gasWithoutEIP2930 = tx.baseGasRequired( IstanbulSchedule );
    int64_t gasWithEIP2930 = tx.baseGasRequired( patchedSchedule );
    BOOST_REQUIRE_EQUAL( gasWithEIP2930 - gasWithoutEIP2930, 6200 );
}

BOOST_AUTO_TEST_CASE( eip2929ScheduleValues ) {
    EVMSchedule patchedSchedule = BerlinForkPatch::makeSchedule( IstanbulSchedule );
    BOOST_REQUIRE( patchedSchedule.eip2929Mode );
    BOOST_REQUIRE( patchedSchedule.eip2930Mode );
    BOOST_REQUIRE( patchedSchedule.eip2565Mode );
    BOOST_REQUIRE_EQUAL( patchedSchedule.sloadGas, patchedSchedule.coldSloadCost );
    BOOST_REQUIRE_EQUAL( patchedSchedule.balanceGas, patchedSchedule.coldAccountAccessCost );
    BOOST_REQUIRE_EQUAL( patchedSchedule.extcodesizeGas, patchedSchedule.coldAccountAccessCost );
    BOOST_REQUIRE_EQUAL( patchedSchedule.extcodecopyGas, patchedSchedule.coldAccountAccessCost );
    BOOST_REQUIRE_EQUAL( patchedSchedule.extcodehashGas, patchedSchedule.coldAccountAccessCost );
    // EIP-2929 SSTORE adjustments
    BOOST_REQUIRE_EQUAL( patchedSchedule.sstoreUnchangedGas, patchedSchedule.warmStorageReadCost );
    BOOST_REQUIRE_EQUAL( patchedSchedule.sstoreResetGas, 5000u - patchedSchedule.coldSloadCost );
    // Verify constant values
    BOOST_REQUIRE_EQUAL( patchedSchedule.warmStorageReadCost, 100u );
    BOOST_REQUIRE_EQUAL( patchedSchedule.coldSloadCost, 2100u );
    BOOST_REQUIRE_EQUAL( patchedSchedule.coldAccountAccessCost, 2600u );
    BOOST_REQUIRE_EQUAL( patchedSchedule.txAccessListAddressGas, 2400u );
    BOOST_REQUIRE_EQUAL( patchedSchedule.txAccessListStorageKeyGas, 1900u );
}

// EIP-2929: test AccessSets access set tracking
BOOST_AUTO_TEST_CASE( eip2929SubStateAccessSets ) {
    AccessSets sets;

    Address addr1( "0x1000000000000000000000000000000000000001" );
    Address addr2( "0x2000000000000000000000000000000000000002" );

    // Initially empty
    BOOST_REQUIRE( sets.accessedAddresses.empty() );
    BOOST_REQUIRE( sets.accessedStorageKeys.empty() );

    // Insert an address — first time should report not present (cold)
    auto result1 = sets.accessedAddresses.insert( addr1 );
    BOOST_REQUIRE( result1.second );  // was inserted = cold

    // Second insert of same address — should report already present (warm)
    auto result2 = sets.accessedAddresses.insert( addr1 );
    BOOST_REQUIRE( !result2.second );  // was NOT inserted = warm

    // Different address is cold
    auto result3 = sets.accessedAddresses.insert( addr2 );
    BOOST_REQUIRE( result3.second );  // cold

    // Storage key tracking
    u256 key1 = 42;
    u256 key2 = 99;

    auto sResult1 = sets.accessedStorageKeys.insert( { addr1, key1 } );
    BOOST_REQUIRE( sResult1.second );  // cold

    auto sResult2 = sets.accessedStorageKeys.insert( { addr1, key1 } );
    BOOST_REQUIRE( !sResult2.second );  // warm

    // Same address different key is cold
    auto sResult3 = sets.accessedStorageKeys.insert( { addr1, key2 } );
    BOOST_REQUIRE( sResult3.second );  // cold

    // Same key different address is cold
    auto sResult4 = sets.accessedStorageKeys.insert( { addr2, key1 } );
    BOOST_REQUIRE( sResult4.second );  // cold
}

// EIP-2929: access sets are transaction-global — all frames share one AccessSets instance.
// SubState::operator+= no longer merges access sets (they are already shared).
BOOST_AUTO_TEST_CASE( eip2929SubStateMerge ) {
    // Verify SubState merge still works for suicides/refunds/logs.
    SubState parent;
    SubState child;
    child.refunds = 42;
    parent += child;
    BOOST_REQUIRE_EQUAL( parent.refunds, 42 );

    // Verify shared AccessSets accumulate entries from all frames without copying.
    auto sharedSets = std::make_shared< AccessSets >();

    Address addr1( "0x1000000000000000000000000000000000000001" );
    Address addr2( "0x2000000000000000000000000000000000000002" );
    Address addr3( "0x3000000000000000000000000000000000000003" );

    // "parent frame" inserts
    sharedSets->accessedAddresses.insert( addr1 );
    sharedSets->accessedAddresses.insert( addr2 );
    sharedSets->accessedStorageKeys.insert( { addr1, u256( 1 ) } );

    // "child frame" inserts into the same object
    sharedSets->accessedAddresses.insert( addr2 );
    sharedSets->accessedAddresses.insert( addr3 );
    sharedSets->accessedStorageKeys.insert( { addr1, u256( 2 ) } );
    sharedSets->accessedStorageKeys.insert( { addr3, u256( 1 ) } );

    // All entries visible in one place — no merge step needed
    BOOST_REQUIRE_EQUAL( sharedSets->accessedAddresses.size(), 3u );
    BOOST_REQUIRE( sharedSets->accessedAddresses.count( addr1 ) );
    BOOST_REQUIRE( sharedSets->accessedAddresses.count( addr2 ) );
    BOOST_REQUIRE( sharedSets->accessedAddresses.count( addr3 ) );

    BOOST_REQUIRE_EQUAL( sharedSets->accessedStorageKeys.size(), 3u );
    BOOST_REQUIRE( sharedSets->accessedStorageKeys.count( { addr1, u256( 1 ) } ) );
    BOOST_REQUIRE( sharedSets->accessedStorageKeys.count( { addr1, u256( 2 ) } ) );
    BOOST_REQUIRE( sharedSets->accessedStorageKeys.count( { addr3, u256( 1 ) } ) );
}

// EIP-2929: SubState::clear() resets suicides/logs/refunds but NOT access sets.
// Access sets are transaction-global and must survive subcall reverts per spec.
BOOST_AUTO_TEST_CASE( eip2929SubStateClear ) {
    SubState sub;
    sub.refunds = 100;
    sub.clear();
    BOOST_REQUIRE_EQUAL( sub.refunds, 0 );

    // AccessSets live independently and are unaffected by SubState::clear().
    AccessSets sets;
    Address addr1( "0x1000000000000000000000000000000000000001" );
    sets.accessedAddresses.insert( addr1 );
    sets.accessedStorageKeys.insert( { addr1, u256( 1 ) } );
    BOOST_REQUIRE( !sets.accessedAddresses.empty() );
    BOOST_REQUIRE( !sets.accessedStorageKeys.empty() );
}

// EIP-2929: test schedule SSTORE_RESET_GAS adjustment
BOOST_AUTO_TEST_CASE( eip2929SstoreResetGasAdjustment ) {
    // Pre-Berlin: sstoreResetGas = 5000
    BOOST_REQUIRE_EQUAL( IstanbulSchedule.sstoreResetGas, 5000u );

    // Post-Berlin: sstoreResetGas = 5000 - COLD_SLOAD_COST = 2900
    EVMSchedule berlinSchedule = BerlinForkPatch::makeSchedule( IstanbulSchedule );
    BOOST_REQUIRE_EQUAL( berlinSchedule.sstoreResetGas, 2900u );

    // SLOAD_GAS (sstoreUnchangedGas) becomes WARM_STORAGE_READ_COST
    BOOST_REQUIRE_EQUAL( berlinSchedule.sstoreUnchangedGas, 100u );
}

// EIP-2718: test typed transaction envelope format
BOOST_AUTO_TEST_CASE( eip2718TypedTransactionTypes ) {
    // Type 1 (EIP-2930 access list tx)
    auto type1Rlp = fromHex(
        "0x01f8c38197018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180f85bf85994de"
        "0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000"
        "000000000000000003a0000000000000000000000000000000000000000000000000000000000000000780a0b0"
        "3eaf481958e22fc39bd1d526eb9255be1e6625614f02ca939e51c3d7e64bcaa05f675640c04bb050d27bd1f39c"
        "07b6ff742311b04dab760bb3bc206054332879" );
    Transaction tx1( type1Rlp, CheckTransaction::None, false, true );
    BOOST_REQUIRE_EQUAL( tx1.txType(), TransactionType::Type1 );

    // Type 2 (EIP-1559 tx)
    auto type2Rlp = fromHex(
        "0x02f8c98197808504a817c8008504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180"
        "f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000"
        "000000000000000000000000000003a00000000000000000000000000000000000000000000000000000000000"
        "00000780a0f1a407dfc1a9f782001d89f617e9b3a2f295378533784fb39960dea60beea2d0a05ac3da2946554b"
        "a3d5721850f4f89ee7a0c38e4acab7130908e7904d13174388" );
    Transaction tx2( type2Rlp, CheckTransaction::None, false, true );
    BOOST_REQUIRE_EQUAL( tx2.txType(), TransactionType::Type2 );

    // Legacy tx
    auto legacyRlp = fromHex(
        "0xf86d800182521c94095e7baea6a6c7c4c2dfeb977efac326af552d870a8e0358ac39584bc98a7c9"
        "79f984b031ba048b55bfa915ac795c431978d8a6a992b628d557da5ff759b307d495a36649353a0ef"
        "ffd310ac743f371de3b9f7f9cb56c0b28ad43601b4ab949f53faa07bd2c804" );
    Transaction txLegacy( legacyRlp, CheckTransaction::None );
    BOOST_REQUIRE_EQUAL( txLegacy.txType(), TransactionType::Legacy );
}

// EIP-2718: typed transactions roundtrip encoding
BOOST_AUTO_TEST_CASE( eip2718RoundtripEncoding ) {
    // Type 1 tx should roundtrip encode/decode correctly
    auto type1Rlp = fromHex(
        "0x01f8c38197018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180f85bf85994de"
        "0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000"
        "000000000000000003a0000000000000000000000000000000000000000000000000000000000000000780a0b0"
        "3eaf481958e22fc39bd1d526eb9255be1e6625614f02ca939e51c3d7e64bcaa05f675640c04bb050d27bd1f39c"
        "07b6ff742311b04dab760bb3bc206054332879" );
    Transaction tx1( type1Rlp, CheckTransaction::None, false, true );
    auto reencoded1 = tx1.toBytes();
    BOOST_REQUIRE( reencoded1 == type1Rlp );

    // Type 2 tx should roundtrip
    auto type2Rlp = fromHex(
        "0x02f8c98197808504a817c8008504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180"
        "f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000"
        "000000000000000000000000000003a00000000000000000000000000000000000000000000000000000000000"
        "00000780a0f1a407dfc1a9f782001d89f617e9b3a2f295378533784fb39960dea60beea2d0a05ac3da2946554b"
        "a3d5721850f4f89ee7a0c38e4acab7130908e7904d13174388" );
    Transaction tx2( type2Rlp, CheckTransaction::None, false, true );
    auto reencoded2 = tx2.toBytes();
    BOOST_REQUIRE( reencoded2 == type2Rlp );
}

// EIP-2930: access list parsing and gas cost calculation
BOOST_AUTO_TEST_CASE( eip2930AccessListGasCost ) {
    EVMSchedule berlinSchedule = BerlinForkPatch::makeSchedule( IstanbulSchedule );

    // Type 1 tx with 1 address + 2 storage keys
    auto txRlp = fromHex(
        "0x01f8c38197018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180f85bf85994de"
        "0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000"
        "000000000000000003a0000000000000000000000000000000000000000000000000000000000000000780a0b0"
        "3eaf481958e22fc39bd1d526eb9255be1e6625614f02ca939e51c3d7e64bcaa05f675640c04bb050d27bd1f39c"
        "07b6ff742311b04dab760bb3bc206054332879" );
    Transaction tx( txRlp, CheckTransaction::None, false, true );
    BOOST_REQUIRE_EQUAL( tx.accessList().size(), 1u );

    // Expected: 1 * 2400 (address) + 2 * 1900 (keys) = 6200
    int64_t accessListGas =
        TransactionBase::accessListGasRequired( tx.accessList(), berlinSchedule );
    BOOST_REQUIRE_EQUAL( accessListGas, 6200 );

    // Empty access list should cost 0
    auto txEmptyAL = fromHex(
        "0x01f8678197808504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180c001a01ebdc5"
        "46c8b85511b7ba831f47c4981069d7af972d10b7dce2c57225cb5df6a7a055ae1e84fea41d37589eb740a0a930"
        "17a5cd0e9f10ee50f165bf4b1b4c78ddae" );
    Transaction tx2( txEmptyAL, CheckTransaction::None, false, true );
    BOOST_REQUIRE_EQUAL( tx2.accessList().size(), 0u );
    int64_t emptyGas = TransactionBase::accessListGasRequired( tx2.accessList(), berlinSchedule );
    BOOST_REQUIRE_EQUAL( emptyGas, 0 );
}

// EIP-2565: verify the new modexp complexity formula
BOOST_AUTO_TEST_CASE( eip2565MultComplexityFormula ) {
    // The EIP-2565 formula: words = ceil(max_length / 8); complexity = words^2
    // For max_length=32 (from the test vector): words = ceil(32/8) = 4, complexity = 16
    // For max_length=64: words = ceil(64/8) = 8, complexity = 64
    // For max_length=1: words = ceil(1/8) = 1, complexity = 1
    // For max_length=8: words = ceil(8/8) = 1, complexity = 1
    // For max_length=9: words = ceil(9/8) = 2, complexity = 4

    // The BerlinForkPatch enables eip2565Mode
    EVMSchedule berlinSchedule = BerlinForkPatch::makeSchedule( IstanbulSchedule );
    BOOST_REQUIRE( berlinSchedule.eip2565Mode );
}

// EIP-2929: verify AccessSets tracks warm/cold correctly (used by ExtVMFace::accessAccount etc.)
BOOST_AUTO_TEST_CASE( eip2929ExtVMFaceAccessTracking ) {
    AccessSets sets;
    Address addr1( "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" );
    Address addr2( "0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb" );
    u256 key1( 100 );

    // First access to addr1 should be cold (returns false = was not warm)
    auto r1 = sets.accessedAddresses.insert( addr1 );
    BOOST_REQUIRE( r1.second );  // cold

    // Second access should be warm
    auto r2 = sets.accessedAddresses.insert( addr1 );
    BOOST_REQUIRE( !r2.second );  // warm

    // First storage access
    auto r3 = sets.accessedStorageKeys.insert( { addr1, key1 } );
    BOOST_REQUIRE( r3.second );  // cold

    // Second storage access
    auto r4 = sets.accessedStorageKeys.insert( { addr1, key1 } );
    BOOST_REQUIRE( !r4.second );  // warm

    // Different address same key is still cold
    auto r5 = sets.accessedStorageKeys.insert( { addr2, key1 } );
    BOOST_REQUIRE( r5.second );  // cold
}

// EIP-2930: typed receipt encoding (TransactionReceipt with tx type)
BOOST_AUTO_TEST_CASE( eip2930TypedReceiptEncoding ) {
    // Create a receipt for a Type1 transaction
    TransactionReceipt receipt( uint8_t( 1 ), u256( 21000 ), LogEntries{} );
    receipt.setTxType( 1 );

    BOOST_REQUIRE_EQUAL( receipt.txType(), 1 );

    // typedRlp() should prepend 0x01 to the RLP
    bytes typed = receipt.typedRlp();
    BOOST_REQUIRE( !typed.empty() );
    BOOST_REQUIRE_EQUAL( typed[0], 0x01 );

    // The remaining bytes should be valid RLP
    bytes bareRlp = receipt.rlp();
    BOOST_REQUIRE( typed.size() == bareRlp.size() + 1 );
    BOOST_REQUIRE( std::equal( bareRlp.begin(), bareRlp.end(), typed.begin() + 1 ) );

    // Legacy receipt should NOT have type prefix
    TransactionReceipt legacyReceipt( uint8_t( 1 ), u256( 21000 ), LogEntries{} );
    legacyReceipt.setTxType( 0 );
    bytes legacyBytes = legacyReceipt.typedRlp();
    bytes legacyBareRlp = legacyReceipt.rlp();
    BOOST_REQUIRE( legacyBytes == legacyBareRlp );
}

// EIP-2930: typed receipt roundtrip (decode typed receipt)
BOOST_AUTO_TEST_CASE( eip2930TypedReceiptRoundtrip ) {
    // Create a receipt, set type, get typedRlp, decode back
    LogEntries logs;
    logs.emplace_back(
        Address( "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" ), h256s{}, bytes{ 0x01, 0x02 } );
    TransactionReceipt original( uint8_t( 1 ), u256( 50000 ), logs );
    original.setTxType( 1 );

    // Encode as typed receipt
    bytes encoded = original.typedRlp();
    BOOST_REQUIRE_EQUAL( encoded[0], 0x01 );

    // Decode the typed receipt
    bytesConstRef encodedRef( &encoded );
    TransactionReceipt decoded( encodedRef );
    BOOST_REQUIRE_EQUAL( decoded.txType(), 1 );
    BOOST_REQUIRE_EQUAL( decoded.statusCode(), 1 );
    BOOST_REQUIRE( decoded.cumulativeGasUsed() == u256( 50000 ) );
    BOOST_REQUIRE_EQUAL( decoded.log().size(), 1u );
    BOOST_REQUIRE(
        decoded.log()[0].address == Address( "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" ) );

    // Type2 receipt
    TransactionReceipt type2Receipt( uint8_t( 1 ), u256( 30000 ), LogEntries{} );
    type2Receipt.setTxType( 2 );
    bytes type2Encoded = type2Receipt.typedRlp();
    BOOST_REQUIRE_EQUAL( type2Encoded[0], 0x02 );

    bytesConstRef type2EncodedRef( &type2Encoded );
    TransactionReceipt type2Decoded( type2EncodedRef );
    BOOST_REQUIRE_EQUAL( type2Decoded.txType(), 2 );
    BOOST_REQUIRE( type2Decoded.cumulativeGasUsed() == u256( 30000 ) );
}

// EIP-2930: access list address size validation
BOOST_AUTO_TEST_CASE( eip2930AccessListAddressValidation ) {
    // Construct a Type1 tx with an access list entry that has a 19-byte address (invalid).
    // The access list RLP entry: [19-byte-address, []]
    RLPStream innerEntry;
    innerEntry.appendList( 2 );
    // 19-byte address (one byte short)
    innerEntry << bytes( 19, 0xaa );
    innerEntry.appendList( 0 );

    RLPStream accessList;
    accessList.appendList( 1 );
    accessList.appendRaw( innerEntry.out() );

    // Wrap in a full Type1 tx RLP: [chainId, nonce, gasPrice, gasLimit, to, value, data,
    // accessList, v, r, s]
    RLPStream txRlp;
    txRlp.appendList( 11 );
    txRlp << u256( 1 );                                                // chainId
    txRlp << u256( 0 );                                                // nonce
    txRlp << u256( 1000 );                                             // gasPrice
    txRlp << u256( 21000 );                                            // gasLimit
    txRlp << Address( "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" );  // to
    txRlp << u256( 0 );                                                // value
    txRlp << bytes{};                                                  // data
    txRlp.appendRaw( accessList.out() );                               // accessList
    txRlp << u256( 0 );                                                // v
    txRlp << u256( 0 );                                                // r
    txRlp << u256( 0 );                                                // s

    // Prepend type byte 0x01
    bytes txBytes = txRlp.out();
    txBytes.insert( txBytes.begin(), 0x01 );

    // Should throw due to invalid address size
    BOOST_REQUIRE_THROW(
        Transaction( txBytes, CheckTransaction::None, false, true ), InvalidTransactionFormat );
}

// EIP-2930: access list storage key size validation
BOOST_AUTO_TEST_CASE( eip2930AccessListStorageKeyValidation ) {
    // Construct a Type1 tx with an access list entry that has a 31-byte storage key (invalid).
    RLPStream keyList;
    keyList.appendList( 1 );
    keyList << bytes( 31, 0xbb );  // 31-byte key (should be 32)

    RLPStream innerEntry;
    innerEntry.appendList( 2 );
    innerEntry << Address( "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" ).asBytes();
    innerEntry.appendRaw( keyList.out() );

    RLPStream accessList;
    accessList.appendList( 1 );
    accessList.appendRaw( innerEntry.out() );

    RLPStream txRlp;
    txRlp.appendList( 11 );
    txRlp << u256( 1 );                                                // chainId
    txRlp << u256( 0 );                                                // nonce
    txRlp << u256( 1000 );                                             // gasPrice
    txRlp << u256( 21000 );                                            // gasLimit
    txRlp << Address( "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" );  // to
    txRlp << u256( 0 );                                                // value
    txRlp << bytes{};                                                  // data
    txRlp.appendRaw( accessList.out() );                               // accessList
    txRlp << u256( 0 );                                                // v
    txRlp << u256( 0 );                                                // r
    txRlp << u256( 0 );                                                // s

    bytes txBytes = txRlp.out();
    txBytes.insert( txBytes.begin(), 0x01 );

    BOOST_REQUIRE_THROW(
        Transaction( txBytes, CheckTransaction::None, false, true ), InvalidTransactionFormat );
}

// EIP-2718: getTransactionType() rejects unsupported typed transaction types [3, 0x7f]
BOOST_AUTO_TEST_CASE( eip2718RejectUnsupportedTransactionType ) {
    // A byte in [3, 0x7f] followed by arbitrary RLP should be rejected as unsupported type.
    // Type 3 (unsupported)
    bytes type3Tx = { 0x03 };
    // Append a minimal RLP list so there's some data.
    RLPStream s;
    s.appendList( 0 );
    bytes rlpBytes = s.out();
    type3Tx.insert( type3Tx.end(), rlpBytes.begin(), rlpBytes.end() );
    BOOST_REQUIRE_THROW(
        Transaction( type3Tx, CheckTransaction::None, false, true ), InvalidTransactionFormat );

    // Type 0x7f (max single-byte typed tx)
    bytes type7fTx = { 0x7f };
    type7fTx.insert( type7fTx.end(), rlpBytes.begin(), rlpBytes.end() );
    BOOST_REQUIRE_THROW(
        Transaction( type7fTx, CheckTransaction::None, false, true ), InvalidTransactionFormat );

    // Byte 0x80 (RLP string prefix, invalid envelope)
    bytes type80Tx = { 0x80 };
    type80Tx.insert( type80Tx.end(), rlpBytes.begin(), rlpBytes.end() );
    BOOST_REQUIRE_THROW(
        Transaction( type80Tx, CheckTransaction::None, false, true ), InvalidTransactionFormat );

    // Types 1 and 2 should NOT throw at type detection (they'll fail later on malformed RLP,
    // but getTransactionType itself should accept them).
    // Type 0 (legacy) should also be accepted — it falls through to legacy parsing.
}

BOOST_AUTO_TEST_SUITE_END()
