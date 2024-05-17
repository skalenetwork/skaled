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
/** @file Transaction.cpp
 * @author Dmitrii Khokhlov <winsvega@mail.ru>
 * @date 2015
 * Transaaction test functions.
 */

#include "test/tools/libtesteth/TestHelper.h"
#include <libethcore/Common.h>
#include <libethcore/Exceptions.h>
#include <libevm/VMFace.h>
#include <test/tools/libtesteth/BlockChainHelper.h>
using namespace dev;
using namespace eth;
using namespace dev::test;

BOOST_FIXTURE_TEST_SUITE( libethereum, TestOutputHelperFixture )

BOOST_AUTO_TEST_CASE( TransactionGasRequired, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    // Transaction data is 0358ac39584bc98a7c979f984b03, 14 bytes
    Transaction tr(
        fromHex( "0xf86d800182521c94095e7baea6a6c7c4c2dfeb977efac326af552d870a8e0358ac39584bc98a7c9"
                 "79f984b031ba048b55bfa915ac795c431978d8a6a992b628d557da5ff759b307d495a36649353a0ef"
                 "ffd310ac743f371de3b9f7f9cb56c0b28ad43601b4ab949f53faa07bd2c804" ),
        CheckTransaction::None );
    BOOST_CHECK_EQUAL( tr.baseGasRequired( HomesteadSchedule ), 14 * 68 + 21000 );
    BOOST_CHECK_EQUAL( tr.baseGasRequired( IstanbulSchedule ), 14 * 16 + 21000 );

    tr = Transaction (
            fromHex( "0x01f8d18197808504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b018"
                     "e0358ac39584bc98a7c979f984b03f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697b"
                     "aef842a00000000000000000000000000000000000000000000000000000000000000003a0000"
                     "000000000000000000000000000000000000000000000000000000000000780a08ae3a721ee02"
                     "cf52d85ecec934c6f46ea3e96d6355eb8ccde261e1e419885761a0234565f6d227d8eba0937b0"
                     "f03cb25f83aeb24c13b7a39a9ef6e80c1ea272a3c" ),
            CheckTransaction::None, false, true );
    BOOST_CHECK_EQUAL( tr.baseGasRequired( HomesteadSchedule ), 14 * 68 + 21000 );
    BOOST_CHECK_EQUAL( tr.baseGasRequired( IstanbulSchedule ), 14 * 16 + 21000 );

    tr = Transaction (
            fromHex( "0x02f8d78197808504a817c8008504a817c800827530947d36af85a184e220a656525fcbb9a63"
                     "b9ab3c12b018e0358ac39584bc98a7c979f984b03f85bf85994de0b295669a9fd93d5f28d9ec8"
                     "5e40f4cb697baef842a0000000000000000000000000000000000000000000000000000000000"
                     "0000003a0000000000000000000000000000000000000000000000000000000000000000780a0"
                     "23927f0e208494bd1fd8876597899d72025167fed902e9c1c417ddd8639bb7b4a02a63ea48f7e"
                     "94df3a40c4a840ba98da02f13817acb5fe137d40f632e6c8ed367" ),
            CheckTransaction::None, false, true );
    BOOST_CHECK_EQUAL( tr.baseGasRequired( HomesteadSchedule ), 14 * 68 + 21000 );
    BOOST_CHECK_EQUAL( tr.baseGasRequired( IstanbulSchedule ), 14 * 16 + 21000 );
}

BOOST_AUTO_TEST_CASE( TransactionWithEmptyRecepient ) {
    // recipient RLP is 0x80 (empty array)
    auto txRlp = fromHex(
        "0xf84c8014830493e080808026a02f23977c68f851bbec8619510a4acdd34805270d97f5714b003efe7274914c"
        "a2a05874022b26e0d88807bdcc59438f86f5a82e24afefad5b6a67ae853896fe2b37" );
    Transaction tx( txRlp, CheckTransaction::None );  // shouldn't throw

    // recipient RLP is 0xc0 (empty list)
    txRlp = fromHex(
        "0xf84c8014830493e0c0808026a02f23977c68f851bbec8619510a4acdd34805270d97f5714b003efe7274914c"
        "a2a05874022b26e0d88807bdcc59438f86f5a82e24afefad5b6a67ae853896fe2b37" );
    BOOST_REQUIRE_THROW( Transaction( txRlp, CheckTransaction::None ), InvalidTransactionFormat );

    txRlp = fromHex(
        "0x01f8bd8197808504a817c80082753080018e0358ac39584bc98a7c979f984b03f85bf85994de0b295669a9fd"
        "93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000000000000000"
        "000003a0000000000000000000000000000000000000000000000000000000000000000780a08d795591e0eb53"
        "fb374a804ba3f73cf291069549d62316219811c3f7fb8cfad0a07e9d0bd7fabc8f74475624c912b5334dc49224"
        "b1dede6c802d52a35254bfc457" );
    tx = Transaction( txRlp, CheckTransaction::None, false, true );  // shouldn't throw

    // recipient RLP is 0xc0 (empty list)
    txRlp = fromHex(
        "0x01f8bd8197808504a817c800827530c0018e0358ac39584bc98a7c979f984b03f85bf85994de0b295669a9fd"
        "93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000000000000000"
        "000003a0000000000000000000000000000000000000000000000000000000000000000780a08d795591e0eb53"
        "fb374a804ba3f73cf291069549d62316219811c3f7fb8cfad0a07e9d0bd7fabc8f74475624c912b5334dc49224"
        "b1dede6c802d52a35254bfc457" );
    BOOST_REQUIRE_THROW( Transaction( txRlp, CheckTransaction::None, false, true ), InvalidTransactionFormat );

    txRlp = fromHex(
        "0x02f8c38197808504a817c8008504a817c80082753080018e0358ac39584bc98a7c979f984b03f85bf85994de"
        "0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000"
        "000000000000000003a0000000000000000000000000000000000000000000000000000000000000000780a0c8"
        "029a8b702d54c79ef18b557e755a1bfd8a4afcfcf31813790df34a6f740a95a00ceb8fdf611b4c9ff8d007d2a5"
        "44bc4bfae0e97a03e32b1c8b8208c82cebcafb" );
    tx = Transaction( txRlp, CheckTransaction::None, false, true );  // shouldn't throw

    // recipient RLP is 0xc0 (empty list)
    txRlp = fromHex(
        "0x02f8c38197808504a817c8008504a817c800827530c0018e0358ac39584bc98a7c979f984b03f85bf85994de"
        "0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000"
        "000000000000000003a0000000000000000000000000000000000000000000000000000000000000000780a0c8"
        "029a8b702d54c79ef18b557e755a1bfd8a4afcfcf31813790df34a6f740a95a00ceb8fdf611b4c9ff8d007d2a5"
        "44bc4bfae0e97a03e32b1c8b8208c82cebcafb" );
    BOOST_REQUIRE_THROW( Transaction( txRlp, CheckTransaction::None, false, true ), InvalidTransactionFormat );
}

BOOST_AUTO_TEST_CASE( TransactionNotReplayProtected, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    auto txRlp = fromHex(
        "0xf86d800182521c94095e7baea6a6c7c4c2dfeb977efac326af552d870a8e0358ac39584bc98a7c979f984b03"
        "1ba048b55bfa915ac795c431978d8a6a992b628d557da5ff759b307d495a36649353a0efffd310ac743f371de3"
        "b9f7f9cb56c0b28ad43601b4ab949f53faa07bd2c804" );
    Transaction tx( txRlp, CheckTransaction::None );
    tx.checkChainId( 1234, true );  // any chain ID is accepted for not replay protected tx

    BOOST_REQUIRE( tx.toBytes() == txRlp );

    txRlp = fromHex(
            "0x01f8ce8504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b018e0358ac3958"
            "4bc98a7c979f984b03f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a0000000000000"
            "0000000000000000000000000000000000000000000000000003a000000000000000000000000000000000"
            "0000000000000000000000000000000701a0a3b1de6f2958e1e34db86438bba310637f2e799fe9768a143a"
            "d87e47c33d1e6ca00e04ef9fe6bb01176c5a4c5bf4a070662478a320eaaff2895d17451c8d61d472" );
    BOOST_REQUIRE_THROW( Transaction( txRlp, CheckTransaction::None, false, true ), dev::BadCast );

    txRlp = fromHex(
            "0x02f8d5808504a817c8008504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b"
            "018e0358ac39584bc98a7c979f984b03f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842"
            "a00000000000000000000000000000000000000000000000000000000000000003a0000000000000000000"
            "000000000000000000000000000000000000000000000780a023927f0e208494bd1fd8876597899d720251"
            "67fed902e9c1c417ddd8639bb7b4a02a63ea48f7e94df3a40c4a840ba98da02f13817acb5fe137d40f632e"
            "6c8ed367" );
    BOOST_REQUIRE_THROW( Transaction( txRlp, CheckTransaction::None, false, true ), dev::BadCast );
}

BOOST_AUTO_TEST_CASE( TransactionChainIDMax64Bit, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    // recoveryID = 0, v = 36893488147419103265
    auto txRlp1 = fromHex(
        "0xf86e808698852840a46f82d6d894095e7baea6a6c7c4c2dfeb977efac326af552d8780808902000000000000"
        "0021a098ff921201554726367d2be8c804a7ff89ccf285ebc57dff8ae4c44b9c19ac4aa01887321be575c8095f"
        "789dd4c743dfe42c1820f9231f98a962b210e3ac2452a3" );
    Transaction tx1{txRlp1, CheckTransaction::None};
    tx1.checkChainId( std::numeric_limits< uint64_t >::max(), false );

    // recoveryID = 1, v = 36893488147419103266
    auto txRlp2 = fromHex(
        "0xf86e808698852840a46f82d6d894095e7baea6a6c7c4c2dfeb977efac326af552d8780808902000000000000"
        "0022a098ff921201554726367d2be8c804a7ff89ccf285ebc57dff8ae4c44b9c19ac4aa01887321be575c8095f"
        "789dd4c743dfe42c1820f9231f98a962b210e3ac2452a3" );
    Transaction tx2{txRlp2, CheckTransaction::None};
    tx2.checkChainId( std::numeric_limits< uint64_t >::max(), false );

    txRlp1 = fromHex(
        "0x01f8d888ffffffffffffffff808504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b01"
        "8e0358ac39584bc98a7c979f984b03f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000"
        "000000000000000000000000000000000000000000000000000000000003a00000000000000000000000000000"
        "00000000000000000000000000000000000701a0e236de02b843139aebfce593d680c06ce79cfd2f2e7f9dcac9"
        "fe23b38060591aa0734952245446ad42e47ec996c9a7b02973cbc8dd944c9622714416b2bef122f4" );
    tx1 = Transaction{txRlp1, CheckTransaction::None, false, true};
    tx1.checkChainId( std::numeric_limits< uint64_t >::max(), false );

    txRlp1 = fromHex(
        "0x02f8de88ffffffffffffffff808504a817c8008504a817c800827530947d36af85a184e220a656525fcbb9a6"
        "3b9ab3c12b018e0358ac39584bc98a7c979f984b03f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697b"
        "aef842a00000000000000000000000000000000000000000000000000000000000000003a00000000000000000"
        "00000000000000000000000000000000000000000000000780a0b62465e633b565f2f3632125b452d8df66d4f6"
        "b48b58f59da6201234e3f9ce75a0467f18ca2b64f3642cb37e7d5470bbac5fbc62c66b23a0ff955b994803fcf3"
        "74" );
    tx1 = Transaction{txRlp1, CheckTransaction::None, false, true};
    tx1.checkChainId( std::numeric_limits< uint64_t >::max(), false );
}

BOOST_AUTO_TEST_CASE( TransactionChainIDBiggerThan64Bit ) {
    // recoveryID = 0, v = 184467440737095516439
    auto txRlp1 = fromHex(
        "0xf86a03018255f094b94f5374fce5edbc8e2a8697c15331677e6ebf0b0a825544890a0000000000000117a098"
        "ff921201554726367d2be8c804a7ff89ccf285ebc57dff8ae4c44b9c19ac4aa08887321be575c8095f789dd4c7"
        "43dfe42c1820f9231f98a962b210e3ac2452a3" );
    BOOST_REQUIRE_THROW( Transaction( txRlp1, CheckTransaction::None ), InvalidSignature );

    // recoveryID = 1, v = 184467440737095516440
    auto txRlp2 = fromHex(
        "0xf86a03018255f094b94f5374fce5edbc8e2a8697c15331677e6ebf0b0a825544890a0000000000000118a098"
        "ff921201554726367d2be8c804a7ff89ccf285ebc57dff8ae4c44b9c19ac4aa08887321be575c8095f789dd4c7"
        "43dfe42c1820f9231f98a962b210e3ac2452a3" );
    BOOST_REQUIRE_THROW( Transaction( txRlp2, CheckTransaction::None ), InvalidSignature );

    txRlp1 = fromHex(
        "0x01f8d9890a0000000000000117808504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b"
        "018e0358ac39584bc98a7c979f984b03f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a000"
        "00000000000000000000000000000000000000000000000000000000000003a000000000000000000000000000"
        "0000000000000000000000000000000000000780a0e108b83ed5e1b701b249970e61d9ae409eb6870af96f1a9d"
        "8827f497375ae5c8a0795eb0b4f36fe712af5e6a8447802c9eb0913a2add86174552bf2e4b0e183feb" );
    RLPStream rlpStream;
    auto tx = Transaction( txRlp1, CheckTransaction::None, false, true );
    auto txBytes = tx.toBytes(IncludeSignature::WithSignature);
    BOOST_REQUIRE( txBytes != txRlp1 );

    txRlp1 = fromHex(
        "0x02f8df890a0000000000000117808504a817c8008504a817c800827530947d36af85a184e220a656525fcbb9"
        "a63b9ab3c12b018e0358ac39584bc98a7c979f984b03f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb69"
        "7baef842a00000000000000000000000000000000000000000000000000000000000000003a000000000000000"
        "0000000000000000000000000000000000000000000000000780a0912e3aad5af05008d3a282d2a76dc975d234"
        "4eb34e2500c924a58ccfdc9dbeb4a04afcffcb5d1897df030d45a7eeb3ceb7c7e6fe368fc47865156b4899de32"
        "01c7" );
    tx = Transaction( txRlp1, CheckTransaction::None, false, true );
    txBytes = tx.toBytes(IncludeSignature::WithSignature);
    BOOST_REQUIRE( txBytes != txRlp1 );
}

BOOST_AUTO_TEST_CASE( TransactionReplayProtected ) {
    auto txRlp = fromHex(
        "0xf86c098504a817c800825208943535353535353535353535353535353535353535880de0b6b3a76400008025"
        "a028ef61340bd939bc2195fe537567866003e1a15d3c71ff63e1590620aa636276a067cbe9d8997f761aecb703"
        "304b3800ccf555c9f3dc64214b297fb1966a3b6d83" );
    Transaction tx( txRlp, CheckTransaction::None );
    tx.checkChainId( 1, false );
    BOOST_REQUIRE_THROW( tx.checkChainId( 123, false ), InvalidSignature );

    auto txBytes = tx.toBytes(IncludeSignature::WithSignature);
    BOOST_REQUIRE( txBytes == txRlp );

    txRlp = fromHex(
        "0x01f8c38197018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180f85bf85994de"
        "0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000"
        "000000000000000003a0000000000000000000000000000000000000000000000000000000000000000780a0b0"
        "3eaf481958e22fc39bd1d526eb9255be1e6625614f02ca939e51c3d7e64bcaa05f675640c04bb050d27bd1f39c"
        "07b6ff742311b04dab760bb3bc206054332879" );
    tx = Transaction( txRlp, CheckTransaction::None, false, true );
    tx.checkChainId( 151, false );
    BOOST_REQUIRE_THROW( tx.checkChainId( 123, false ), InvalidSignature );
    
    BOOST_REQUIRE( tx.toBytes() == txRlp );

    txRlp = fromHex(
        "0x02f8c98197808504a817c8008504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180"
        "f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000"
        "000000000000000000000000000003a00000000000000000000000000000000000000000000000000000000000"
        "00000780a0f1a407dfc1a9f782001d89f617e9b3a2f295378533784fb39960dea60beea2d0a05ac3da2946554b"
        "a3d5721850f4f89ee7a0c38e4acab7130908e7904d13174388" );
    tx = Transaction( txRlp, CheckTransaction::None, false, true );
    tx.checkChainId( 151, false );
    BOOST_REQUIRE_THROW( tx.checkChainId( 123, false ), InvalidSignature );
    
    BOOST_REQUIRE( tx.toBytes() == txRlp );
}

BOOST_AUTO_TEST_CASE( accessList ) {
    // [ { 'address': HexBytes( "0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae" ),
    // 'storageKeys': ( "0x0000000000000000000000000000000000000000000000000000000000000003", "0x0000000000000000000000000000000000000000000000000000000000000007" ) } ]
    auto txRlp = fromHex(
        "0x01f8c38197018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180f85bf85994de"
        "0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000"
        "000000000000000003a0000000000000000000000000000000000000000000000000000000000000000780a0b0"
        "3eaf481958e22fc39bd1d526eb9255be1e6625614f02ca939e51c3d7e64bcaa05f675640c04bb050d27bd1f39c"
        "07b6ff742311b04dab760bb3bc206054332879" );
    Transaction tx;
    BOOST_REQUIRE_NO_THROW( tx = Transaction( txRlp, CheckTransaction::None, false, true ) );
    BOOST_REQUIRE( tx.accessList().size() == 1 );

    // empty accessList
    txRlp = fromHex(
        "0x01f8678197808504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180c001a01ebdc5"
        "46c8b85511b7ba831f47c4981069d7af972d10b7dce2c57225cb5df6a7a055ae1e84fea41d37589eb740a0a930"
        "17a5cd0e9f10ee50f165bf4b1b4c78ddae" );
    BOOST_REQUIRE_NO_THROW( tx = Transaction( txRlp, CheckTransaction::None, false, true ) );
    BOOST_REQUIRE( tx.accessList().size() == 0 );

    // no accessList
    txRlp = fromHex(
        "0x01f8678197808504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180c080a025fffe"
        "aafed61a15aefd1be5ccbd19e3fe07d0088b06ab6ad960d0f6c382d8cea02e255bf1a7de0a75ccec6d00bcc367"
        "1af06ca9641fc02024a9d6b28f9b01307b" );
    BOOST_REQUIRE_NO_THROW( tx = Transaction( txRlp, CheckTransaction::None, false, true ) );
    BOOST_REQUIRE( tx.accessList().size() == 0 );

    // change empty accessList 0xc0 to empty array 0x80
    txRlp = fromHex(
        "0x01f8678197808504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b01808001a01ebdc5"
        "46c8b85511b7ba831f47c4981069d7af972d10b7dce2c57225cb5df6a7a055ae1e84fea41d37589eb740a0a930"
        "17a5cd0e9f10ee50f165bf4b1b4c78ddae" );
    BOOST_REQUIRE_THROW( Transaction( txRlp, CheckTransaction::None, false, true ), InvalidTransactionFormat );
}

BOOST_AUTO_TEST_CASE( ExecutionResultOutput, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    std::stringstream buffer;
    ExecutionResult exRes;

    exRes.gasUsed = u256( "12345" );
    exRes.newAddress = Address( "a94f5374fce5edbc8e2a8697c15331677e6ebf0b" );
    exRes.output = fromHex( "001122334455" );

    buffer << exRes;
    BOOST_CHECK_MESSAGE(
        buffer.str() == "{12345, a94f5374fce5edbc8e2a8697c15331677e6ebf0b, 001122334455}",
        "Error ExecutionResultOutput" );
}

BOOST_AUTO_TEST_CASE( transactionExceptionOutput ) {
    std::stringstream buffer;
    buffer << TransactionException::BadInstruction;
    BOOST_CHECK_MESSAGE(
        buffer.str() == "BadInstruction", "Error output TransactionException::BadInstruction" );
    buffer.str( std::string() );

    buffer << TransactionException::None;
    BOOST_CHECK_MESSAGE( buffer.str() == "None", "Error output TransactionException::None" );
    buffer.str( std::string() );

    buffer << TransactionException::BadRLP;
    BOOST_CHECK_MESSAGE( buffer.str() == "BadRLP", "Error output TransactionException::BadRLP" );
    buffer.str( std::string() );

    buffer << TransactionException::InvalidFormat;
    BOOST_CHECK_MESSAGE(
        buffer.str() == "InvalidFormat", "Error output TransactionException::InvalidFormat" );
    buffer.str( std::string() );

    buffer << TransactionException::OutOfGasIntrinsic;
    BOOST_CHECK_MESSAGE( buffer.str() == "OutOfGasIntrinsic",
        "Error output TransactionException::OutOfGasIntrinsic" );
    buffer.str( std::string() );

    buffer << TransactionException::InvalidSignature;
    BOOST_CHECK_MESSAGE(
        buffer.str() == "InvalidSignature", "Error output TransactionException::InvalidSignature" );
    buffer.str( std::string() );

    buffer << TransactionException::InvalidNonce;
    BOOST_CHECK_MESSAGE(
        buffer.str() == "InvalidNonce", "Error output TransactionException::InvalidNonce" );
    buffer.str( std::string() );

    buffer << TransactionException::NotEnoughCash;
    BOOST_CHECK_MESSAGE(
        buffer.str() == "NotEnoughCash", "Error output TransactionException::NotEnoughCash" );
    buffer.str( std::string() );

    buffer << TransactionException::OutOfGasBase;
    BOOST_CHECK_MESSAGE(
        buffer.str() == "OutOfGasBase", "Error output TransactionException::OutOfGasBase" );
    buffer.str( std::string() );

    buffer << TransactionException::BlockGasLimitReached;
    BOOST_CHECK_MESSAGE( buffer.str() == "BlockGasLimitReached",
        "Error output TransactionException::BlockGasLimitReached" );
    buffer.str( std::string() );

    buffer << TransactionException::BadInstruction;
    BOOST_CHECK_MESSAGE(
        buffer.str() == "BadInstruction", "Error output TransactionException::BadInstruction" );
    buffer.str( std::string() );

    buffer << TransactionException::BadJumpDestination;
    BOOST_CHECK_MESSAGE( buffer.str() == "BadJumpDestination",
        "Error output TransactionException::BadJumpDestination" );
    buffer.str( std::string() );

    buffer << TransactionException::OutOfGas;
    BOOST_CHECK_MESSAGE(
        buffer.str() == "OutOfGas", "Error output TransactionException::OutOfGas" );
    buffer.str( std::string() );

    buffer << TransactionException::OutOfStack;
    BOOST_CHECK_MESSAGE(
        buffer.str() == "OutOfStack", "Error output TransactionException::OutOfStack" );
    buffer.str( std::string() );

    buffer << TransactionException::StackUnderflow;
    BOOST_CHECK_MESSAGE(
        buffer.str() == "StackUnderflow", "Error output TransactionException::StackUnderflow" );
    buffer.str( std::string() );

    buffer << TransactionException::InvalidContractDeployer;
    BOOST_CHECK_MESSAGE( buffer.str() == "InvalidContractDeployer",
        "Error output TransactionException::InvalidContractDeployer" );
    buffer.str( std::string() );

    buffer << TransactionException( -1 );
    BOOST_CHECK_MESSAGE(
        buffer.str() == "Unknown", "Error output TransactionException::StackUnderflow" );
    buffer.str( std::string() );
}

BOOST_AUTO_TEST_CASE( toTransactionExceptionConvert ) {
    RLPException rlpEx;  // toTransactionException(*(dynamic_cast<Exception*>
    BOOST_CHECK_MESSAGE( toTransactionException( rlpEx ) == TransactionException::BadRLP,
        "RLPException !=> TransactionException" );
    OutOfGasIntrinsic oogEx;
    BOOST_CHECK_MESSAGE( toTransactionException( oogEx ) == TransactionException::OutOfGasIntrinsic,
        "OutOfGasIntrinsic !=> TransactionException" );
    InvalidSignature sigEx;
    BOOST_CHECK_MESSAGE( toTransactionException( sigEx ) == TransactionException::InvalidSignature,
        "InvalidSignature !=> TransactionException" );
    OutOfGasBase oogbEx;
    BOOST_CHECK_MESSAGE( toTransactionException( oogbEx ) == TransactionException::OutOfGasBase,
        "OutOfGasBase !=> TransactionException" );
    InvalidNonce nonceEx;
    BOOST_CHECK_MESSAGE( toTransactionException( nonceEx ) == TransactionException::InvalidNonce,
        "InvalidNonce !=> TransactionException" );
    NotEnoughCash cashEx;
    BOOST_CHECK_MESSAGE( toTransactionException( cashEx ) == TransactionException::NotEnoughCash,
        "NotEnoughCash !=> TransactionException" );
    BlockGasLimitReached blGasEx;
    BOOST_CHECK_MESSAGE(
        toTransactionException( blGasEx ) == TransactionException::BlockGasLimitReached,
        "BlockGasLimitReached !=> TransactionException" );
    BadInstruction badInsEx;
    BOOST_CHECK_MESSAGE( toTransactionException( badInsEx ) == TransactionException::BadInstruction,
        "BadInstruction !=> TransactionException" );
    BadJumpDestination badJumpEx;
    BOOST_CHECK_MESSAGE(
        toTransactionException( badJumpEx ) == TransactionException::BadJumpDestination,
        "BadJumpDestination !=> TransactionException" );
    OutOfGas oogEx2;
    BOOST_CHECK_MESSAGE( toTransactionException( oogEx2 ) == TransactionException::OutOfGas,
        "OutOfGas !=> TransactionException" );
    OutOfStack oosEx;
    BOOST_CHECK_MESSAGE( toTransactionException( oosEx ) == TransactionException::OutOfStack,
        "OutOfStack !=> TransactionException" );
    StackUnderflow stackEx;
    BOOST_CHECK_MESSAGE( toTransactionException( stackEx ) == TransactionException::StackUnderflow,
        "StackUnderflow !=> TransactionException" );
    InvalidContractDeployer originEx;
    BOOST_CHECK_MESSAGE(
        toTransactionException( originEx ) == TransactionException::InvalidContractDeployer,
        "InvalidContractDeployer !=> TransactionException" );
    Exception notEx;
    BOOST_CHECK_MESSAGE( toTransactionException( notEx ) == TransactionException::Unknown,
        "Unexpected should be TransactionException::Unknown" );
}

BOOST_AUTO_TEST_CASE( GettingSenderForUnsignedTransactionThrows,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    Transaction tx(
        0, 0, 10000, Address( "a94f5374fce5edbc8e2a8697c15331677e6ebf0b" ), bytes(), 0 );
    BOOST_CHECK( !tx.hasSignature() );

    BOOST_REQUIRE_THROW( tx.sender(), TransactionIsUnsigned );
}

BOOST_AUTO_TEST_CASE( GettingSignatureForUnsignedTransactionThrows,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    Transaction tx(
        0, 0, 10000, Address( "a94f5374fce5edbc8e2a8697c15331677e6ebf0b" ), bytes(), 0 );
    BOOST_REQUIRE_THROW( tx.signature(), TransactionIsUnsigned );
}

BOOST_AUTO_TEST_CASE( StreamRLPWithSignatureForUnsignedTransactionThrows ) {
    Transaction tx(
        0, 0, 10000, Address( "a94f5374fce5edbc8e2a8697c15331677e6ebf0b" ), bytes(), 0 );
    BOOST_REQUIRE_THROW(
        tx.toBytes( IncludeSignature::WithSignature ), TransactionIsUnsigned );
}

BOOST_AUTO_TEST_CASE( CheckLowSForUnsignedTransactionThrows,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    Transaction tx(
        0, 0, 10000, Address( "a94f5374fce5edbc8e2a8697c15331677e6ebf0b" ), bytes(), 0 );
    BOOST_REQUIRE_THROW( tx.checkLowS(), TransactionIsUnsigned );
}

BOOST_AUTO_TEST_SUITE_END()
