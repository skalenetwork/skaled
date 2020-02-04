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
/** @date 2018
 */

#include <libdevcore/CommonJS.h>
#include <libdevcore/TransientDirectory.h>
#include <libethashseal/GenesisInfo.h>
#include <libethereum/ChainParams.h>
#include <libethereum/ClientTest.h>
#include <libp2p/Network.h>
#include <test/tools/libtesteth/Options.h>
#include <test/tools/libtesteth/TestOutputHelper.h>
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::test;
using namespace dev::p2p;
namespace fs = boost::filesystem;

boost::unit_test::assertion_result option_all_tests( boost::unit_test::test_unit_id ) {
    return boost::unit_test::assertion_result( dev::test::Options::get().all ? true : false );
}

struct FixtureCommon {
    const string BTRFS_FILE_PATH = "btrfs.file";
    const string BTRFS_DIR_PATH = "btrfs";
    uid_t sudo_uid;
    gid_t sudo_gid;

    void check_sudo() {
#if ( !defined __APPLE__ )
        char* id_str = getenv( "SUDO_UID" );
        if ( id_str == NULL ) {
            cerr << "Please run under sudo" << endl;
            exit( -1 );
        }

        sscanf( id_str, "%d", &sudo_uid );

        //    uid_t ru, eu, su;
        //    getresuid( &ru, &eu, &su );
        //    cerr << ru << " " << eu << " " << su << endl;

        if ( geteuid() != 0 ) {
            cerr << "Need to be root" << endl;
            exit( -1 );
        }

        id_str = getenv( "SUDO_GID" );
        sscanf( id_str, "%d", &sudo_gid );

        gid_t rgid, egid, sgid;
        getresgid( &rgid, &egid, &sgid );
        cerr << "GIDS: " << rgid << " " << egid << " " << sgid << endl;
#endif
    }

    void dropRoot() {
#if ( !defined __APPLE__ )
        int res = setresgid( sudo_gid, sudo_gid, 0 );
        cerr << "setresgid " << sudo_gid << " " << res << endl;
        if ( res < 0 )
            cerr << strerror( errno ) << endl;
        res = setresuid( sudo_uid, sudo_uid, 0 );
        cerr << "setresuid " << sudo_uid << " " << res << endl;
        if ( res < 0 )
            cerr << strerror( errno ) << endl;
#endif
    }

    void gainRoot() {
#if ( !defined __APPLE__ )
        int res = setresuid( 0, 0, 0 );
        if ( res ) {
            cerr << strerror( errno ) << endl;
            assert( false );
        }
        setresgid( 0, 0, 0 );
        if ( res ) {
            cerr << strerror( errno ) << endl;
            assert( false );
        }
#endif
    }
};

class TestClientFixture : public TestOutputHelperFixture {
public:
    TestClientFixture() try {
        ChainParams chainParams;
        chainParams.sealEngineName = NoProof::name();
        chainParams.allowFutureBlocks = true;

        fs::path dir = m_tmpDir.path();

        string listenIP = "127.0.0.1";
        unsigned short listenPort = 30303;
        auto netPrefs = NetworkPreferences( listenIP, listenPort, false );
        netPrefs.discovery = false;
        netPrefs.pin = false;

        auto nodesState = contents( dir / fs::path( "network.rlp" ) );

        //        bool testingMode = true;
        //        m_web3.reset( new dev::WebThreeDirect( WebThreeDirect::composeClientVersion( "eth"
        //        ), dir,
        //            dir, chainParams, WithExisting::Kill, {"eth"}, testingMode ) );

        m_ethereum.reset( new eth::ClientTest( chainParams, ( int ) chainParams.networkID,
            shared_ptr< GasPricer >(), NULL, dir, WithExisting::Kill ) );

        //        m_ethereum.reset(
        //            new eth::Client( chainParams, ( int ) chainParams.networkID, shared_ptr<
        //            GasPricer >(),
        //                dir, dir, WithExisting::Kill, TransactionQueue::Limits{100000, 1024} ) );

        m_ethereum->injectSkaleHost();
        m_ethereum->startWorking();

    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "TestClientFixture" )
            << "CRITICAL " << dev::nested_exception_what( ex );
        throw;
    } catch ( ... ) {
        clog( VerbosityError, "TestClientFixture" ) << "CRITICAL unknown error";
        throw;
    }

    dev::eth::Client* ethereum() { return m_ethereum.get(); }

private:
    std::unique_ptr< dev::eth::Client > m_ethereum;
    TransientDirectory m_tmpDir;
};

class TestClientSnapshotsFixture : public TestOutputHelperFixture, public FixtureCommon {
public:
    TestClientSnapshotsFixture() try {
        check_sudo();

        dropRoot();

        system( ( "dd if=/dev/zero of=" + BTRFS_FILE_PATH + " bs=1M count=200" ).c_str() );
        system( ( "mkfs.btrfs " + BTRFS_FILE_PATH ).c_str() );
        system( ( "mkdir " + BTRFS_DIR_PATH ).c_str() );

        gainRoot();
        system( ( "mount -o user_subvol_rm_allowed " + BTRFS_FILE_PATH + " " + BTRFS_DIR_PATH )
                    .c_str() );
        chown( BTRFS_DIR_PATH.c_str(), sudo_uid, sudo_gid );
        dropRoot();

        //        btrfs.subvolume.create( ( BTRFS_DIR_PATH + "/vol1" ).c_str() );
        //        btrfs.subvolume.create( ( BTRFS_DIR_PATH + "/vol2" ).c_str() );
        // system( ( "mkdir " + BTRFS_DIR_PATH + "/snapshots" ).c_str() );

        gainRoot();

        ChainParams chainParams;
        chainParams.sealEngineName = NoProof::name();
        chainParams.allowFutureBlocks = true;
        chainParams.nodeInfo.snapshotIntervalMs = 10;

        fs::path dir = m_tmpDir.path();

        string listenIP = "127.0.0.1";
        unsigned short listenPort = 30303;
        auto netPrefs = NetworkPreferences( listenIP, listenPort, false );
        netPrefs.discovery = false;
        netPrefs.pin = false;

        auto nodesState = contents( dir / fs::path( "network.rlp" ) );

        //        bool testingMode = true;
        //        m_web3.reset( new dev::WebThreeDirect( WebThreeDirect::composeClientVersion( "eth"
        //        ), dir,
        //            dir, chainParams, WithExisting::Kill, {"eth"}, testingMode ) );
        std::shared_ptr< SnapshotManager > mgr;
        mgr.reset( new SnapshotManager( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} ) );

        m_ethereum.reset( new eth::ClientTest( chainParams, ( int ) chainParams.networkID,
            shared_ptr< GasPricer >(), mgr, dir, WithExisting::Kill ) );

        //        m_ethereum.reset(
        //            new eth::Client( chainParams, ( int ) chainParams.networkID, shared_ptr<
        //            GasPricer >(),
        //                dir, dir, WithExisting::Kill, TransactionQueue::Limits{100000, 1024} ) );

        m_ethereum->injectSkaleHost();
        m_ethereum->startWorking();

    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "TestClientFixture" )
            << "CRITICAL " << dev::nested_exception_what( ex );
        throw;
    } catch ( ... ) {
        clog( VerbosityError, "TestClientFixture" ) << "CRITICAL unknown error";
        throw;
    }

    dev::eth::Client* ethereum() { return m_ethereum.get(); }

    ~TestClientSnapshotsFixture() {
        const char* NC = getenv( "NC" );
        if ( NC )
            return;
        gainRoot();
        system( ( "umount " + BTRFS_DIR_PATH ).c_str() );
        system( ( "rmdir " + BTRFS_DIR_PATH ).c_str() );
        system( ( "rm " + BTRFS_FILE_PATH ).c_str() );
    }

private:
    std::unique_ptr< dev::eth::Client > m_ethereum;
    TransientDirectory m_tmpDir;
};

// genesis config string from solidity
static std::string const c_configString = R"(
{
    "sealEngine": "NoProof",
    "params": {
        "accountStartNonce": "0x00",
        "maximumExtraDataSize": "0x1000000",
        "blockReward": "0x",
        "allowFutureBlocks": true,
        "homesteadForkBlock": "0x00",
        "EIP150ForkBlock": "0x00",
        "EIP158ForkBlock": "0x00"
    },
    "genesis": {
        "nonce": "0x0000000000000042",
        "author": "0000000000000010000000000000000000000000",
        "timestamp": "0x00",
        "extraData": "0x",
        "gasLimit": "0x1000000000000",
        "difficulty": "0x020000",
        "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000"
    },
    "accounts": {
        "0000000000000000000000000000000000000001": { "wei": "1", "precompiled": { "name": "ecrecover", "linear": { "base": 3000, "word": 0 } } },
        "0000000000000000000000000000000000000002": { "wei": "1", "precompiled": { "name": "sha256", "linear": { "base": 60, "word": 12 } } },
        "0000000000000000000000000000000000000003": { "wei": "1", "precompiled": { "name": "ripemd160", "linear": { "base": 600, "word": 120 } } },
        "0000000000000000000000000000000000000004": { "wei": "1", "precompiled": { "name": "identity", "linear": { "base": 15, "word": 3 } } },
        "0000000000000000000000000000000000000005": { "wei": "1", "precompiled": { "name": "modexp" } },
        "0000000000000000000000000000000000000006": { "wei": "1", "precompiled": { "name": "alt_bn128_G1_add", "linear": { "base": 500, "word": 0 } } },
        "0000000000000000000000000000000000000007": { "wei": "1", "precompiled": { "name": "alt_bn128_G1_mul", "linear": { "base": 40000, "word": 0 } } },
        "0000000000000000000000000000000000000008": { "wei": "1", "precompiled": { "name": "alt_bn128_pairing_product" } }
    }
}
)";


BOOST_FIXTURE_TEST_SUITE( ClientTestSuite, TestClientFixture )

BOOST_AUTO_TEST_CASE( ClientTest_setChainParamsAuthor ) {
    ClientTest* testClient = asClientTest( ethereum() );
    BOOST_CHECK_EQUAL(
        testClient->author(), Address( "0000000000000000000000000000000000000000" ) );
    testClient->setChainParams( c_configString );
    BOOST_CHECK_EQUAL(
        testClient->author(), Address( "0000000000000010000000000000000000000000" ) );
}

BOOST_AUTO_TEST_SUITE( EstimateGas )

BOOST_AUTO_TEST_CASE( constantConsumption ) {
    ClientTest* testClient = asClientTest( ethereum() );
    testClient->setChainParams( genesisInfo( dev::eth::Network::SkaleTest ) );

    //    This contract is predeployed on SKALE test network
    //    on address 0xD2001300000000000000000000000000000000D2

    //    pragma solidity ^0.5.3;


    //    contract GasEstimate {
    //        function spendHalfOfGas() external view {
    //            uint initialGas = gasleft();
    //            spendGas(initialGas / 2);
    //        }

    //        function spendGas(uint amount) public view {
    //            uint initialGas = gasleft();
    //            while (initialGas - gasleft() < amount) {}
    //        }
    //    }

    Address from( "0xca4409573a5129a72edf85d6c51e26760fc9c903" );
    Address contractAddress( "0xD2001300000000000000000000000000000000D2" );

    // data to call method spendGas(50000)
    bytes data =
        jsToBytes( "0x815b8ab4000000000000000000000000000000000000000000000000000000000000c350" );

    u256 estimate = testClient
                        ->estimateGas( from, 0, contractAddress, data, 10000000, 1000000,
                            GasEstimationCallback() )
                        .first;

    BOOST_CHECK_EQUAL( estimate, u256( 71800 ) );
}

BOOST_AUTO_TEST_CASE( linearConsumption ) {
    ClientTest* testClient = asClientTest( ethereum() );
    testClient->setChainParams( genesisInfo( dev::eth::Network::SkaleTest ) );

    //    This contract is predeployed on SKALE test network
    //    on address 0xD2001300000000000000000000000000000000D2

    //    pragma solidity ^0.5.3;


    //    contract GasEstimate {
    //        function spendHalfOfGas() external view {
    //            uint initialGas = gasleft();
    //            spendGas(initialGas / 2);
    //        }

    //        function spendGas(uint amount) public view {
    //            uint initialGas = gasleft();
    //            while (initialGas - gasleft() < amount) {}
    //        }
    //    }

    Address from( "0xca4409573a5129a72edf85d6c51e26760fc9c903" );
    Address contractAddress( "0xD2001300000000000000000000000000000000D2" );

    // data to call method spendHalfOfGas()
    bytes data = jsToBytes( "0x8273f754" );

    u256 estimate = testClient
                        ->estimateGas( from, 0, contractAddress, data, 10000000, 1000000,
                            GasEstimationCallback() )
                        .first;

    BOOST_CHECK_EQUAL( estimate, u256( 21694 ) );
}

BOOST_AUTO_TEST_CASE( exceedsGasLimit ) {
    ClientTest* testClient = asClientTest( ethereum() );
    testClient->setChainParams( genesisInfo( dev::eth::Network::SkaleTest ) );

    //    This contract is predeployed on SKALE test network
    //    on address 0xD2001300000000000000000000000000000000D2

    //    pragma solidity ^0.5.3;


    //    contract GasEstimate {
    //        function spendHalfOfGas() external view {
    //            uint initialGas = gasleft();
    //            spendGas(initialGas / 2);
    //        }

    //        function spendGas(uint amount) public view {
    //            uint initialGas = gasleft();
    //            while (initialGas - gasleft() < amount) {}
    //        }
    //    }

    Address from( "0xca4409573a5129a72edf85d6c51e26760fc9c903" );
    Address contractAddress( "0xD2001300000000000000000000000000000000D2" );

    // data to call method spendGas(50000)
    bytes data =
        jsToBytes( "0x815b8ab4000000000000000000000000000000000000000000000000000000000000c350" );

    int64_t maxGas = 50000;
    u256 estimate = testClient
                        ->estimateGas( from, 0, contractAddress, data, maxGas, 1000000,
                            GasEstimationCallback() )
                        .first;

    BOOST_CHECK_EQUAL( estimate, u256( maxGas ) );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

static std::string const c_skaleConfigString = R"(
{
    "sealEngine": "NoProof",
    "params": {
        "accountStartNonce": "0x00",
        "maximumExtraDataSize": "0x1000000",
        "blockReward": "0x",
        "allowFutureBlocks": true,
        "homesteadForkBlock": "0x00",
        "EIP150ForkBlock": "0x00",
        "EIP158ForkBlock": "0x00"
    },
    "genesis": {
        "nonce": "0x0000000000000042",
        "author": "0000000000000010000000000000000000000000",
        "timestamp": "0x00",
        "extraData": "0x",
        "gasLimit": "0x1000000000000",
        "difficulty": "0x020000",
        "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000"
    },
    "skaleConfig": {
        "nodeInfo": {
            "nodeName": "TestNode",
            "nodeID": 1112,
            "snapshotIntervalMs": 10
        },
        "sChain": {
            "schainName": "TestChain",
            "schainID": 1,
            "nodes": [
              { "nodeID": 1112, "ip": "127.0.0.1", "basePort": 1231, "ip6": "::1", "basePort6": 1231, "schainIndex" : 1}
            ]
        }
    },
    "accounts": {
        "0000000000000000000000000000000000000001": { "wei": "1", "precompiled": { "name": "ecrecover", "linear": { "base": 3000, "word": 0 } } },
        "0000000000000000000000000000000000000002": { "wei": "1", "precompiled": { "name": "sha256", "linear": { "base": 60, "word": 12 } } },
        "0000000000000000000000000000000000000003": { "wei": "1", "precompiled": { "name": "ripemd160", "linear": { "base": 600, "word": 120 } } },
        "0000000000000000000000000000000000000004": { "wei": "1", "precompiled": { "name": "identity", "linear": { "base": 15, "word": 3 } } },
        "0000000000000000000000000000000000000005": { "wei": "1", "precompiled": { "name": "modexp" } },
        "0000000000000000000000000000000000000006": { "wei": "1", "precompiled": { "name": "alt_bn128_G1_add", "linear": { "base": 500, "word": 0 } } },
        "0000000000000000000000000000000000000007": { "wei": "1", "precompiled": { "name": "alt_bn128_G1_mul", "linear": { "base": 40000, "word": 0 } } },
        "0000000000000000000000000000000000000008": { "wei": "1", "precompiled": { "name": "alt_bn128_pairing_product" } }
    }
}
)";

BOOST_AUTO_TEST_SUITE( ClientSnapshotsSuite, *boost::unit_test::precondition( option_all_tests ) )

BOOST_FIXTURE_TEST_CASE( ClientSnapshotsTest, TestClientSnapshotsFixture ) {
    ClientTest* testClient = asClientTest( ethereum() );
    testClient->setChainParams( c_skaleConfigString );

    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "0" ) );

    testClient->mineBlocks( 1 );

    testClient->importTransactionsAsBlock(
        Transactions(), 1000, testClient->latestBlock().info().timestamp() + 86410 );

    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" ) );

    secp256k1_sha256_t ctx;
    secp256k1_sha256_initialize( &ctx );

    dev::h256 empty_str = dev::h256( "" );
    secp256k1_sha256_write( &ctx, empty_str.data(), empty_str.size );

    dev::h256 empty_state_root_hash;
    secp256k1_sha256_finalize( &ctx, empty_state_root_hash.data() );

    BOOST_REQUIRE( testClient->latestBlock().info().stateRoot() == empty_state_root_hash );
}

BOOST_AUTO_TEST_SUITE_END()
