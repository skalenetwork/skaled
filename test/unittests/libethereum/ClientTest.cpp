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
#include <test/tools/libtesteth/TestOutputHelper.h>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::test;
using namespace dev::p2p;
namespace fs = boost::filesystem;

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
    TestClientFixture( const std::string& _config = "" ) try {
        ChainParams chainParams;
        if ( _config != "" ) {
            chainParams = chainParams.loadConfig( _config );
        }
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
            shared_ptr< GasPricer >(), NULL, NULL, dir, WithExisting::Kill ) );

        //        m_ethereum.reset(
        //            new eth::Client( chainParams, ( int ) chainParams.networkID, shared_ptr<
        //            GasPricer >(),
        //                dir, dir, WithExisting::Kill, TransactionQueue::Limits{100000, 1024} ) );

        m_ethereum->injectSkaleHost();
        m_ethereum->startWorking();

        m_ethereum->setAuthor( coinbase.address() );

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
    dev::KeyPair coinbase{KeyPair::create()};
};

class TestClientSnapshotsFixture : public TestOutputHelperFixture, public FixtureCommon {
public:
    TestClientSnapshotsFixture( const std::string& _config ) try {
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
        chainParams = chainParams.loadConfig( _config );

        fs::path dir = m_tmpDir.path();

        auto nodesState = contents( dir / fs::path( "network.rlp" ) );

        //        bool testingMode = true;
        //        m_web3.reset( new dev::WebThreeDirect( WebThreeDirect::composeClientVersion( "eth"
        //        ), dir,
        //            dir, chainParams, WithExisting::Kill, {"eth"}, testingMode ) );
        std::shared_ptr< SnapshotManager > mgr;
        mgr.reset( new SnapshotManager( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} ) );

        m_ethereum.reset( new eth::ClientTest( chainParams, ( int ) chainParams.networkID,
            shared_ptr< GasPricer >(), mgr, shared_ptr< InstanceMonitor >(), dir, WithExisting::Kill ) );

        //        m_ethereum.reset(
        //            new eth::Client( chainParams, ( int ) chainParams.networkID, shared_ptr<
        //            GasPricer >(),
        //                dir, dir, WithExisting::Kill, TransactionQueue::Limits{100000, 1024} ) );

        m_ethereum->injectSkaleHost();
        m_ethereum->startWorking();

        m_ethereum->setAuthor( coinbase.address() );

    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "TestClientSnapshotsFixture" )
            << "CRITICAL " << dev::nested_exception_what( ex );
        throw;
    } catch ( ... ) {
        clog( VerbosityError, "TestClientSnapshotsFixture" ) << "CRITICAL unknown error";
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
    dev::KeyPair coinbase{KeyPair::create()};
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

static std::string const c_genesisInfoSkaleTest = std::string() +
                                                  R"E(
{
    "sealEngine": "Ethash",
    "params": {
        "accountStartNonce": "0x00",
        "homesteadForkBlock": "0x00",
        "EIP150ForkBlock": "0x00",
        "EIP158ForkBlock": "0x00",
        "byzantiumForkBlock": "0x00",
        "constantinopleForkBlock": "0x00",
        "constantinopleFixForkBlock": "0x00",
        "networkID" : "12313219",
        "chainID": "0x01",
        "maximumExtraDataSize": "0x20",
        "tieBreakingGas": false,
        "minGasLimit": "0x1388",
        "maxGasLimit": "7fffffffffffffff",
        "gasLimitBoundDivisor": "0x0400",
        "minimumDifficulty": "0x020000",
        "difficultyBoundDivisor": "0x0800",
        "durationLimit": "0x0d",
        "blockReward": "0x4563918244F40000"
    },
    "genesis": {
        "nonce": "0x0000000000000042",
        "difficulty": "0x020000",
        "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
        "author": "0x0000000000000000000000000000000000000000",
        "timestamp": "0x00",
        "parentHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
        "extraData": "0x11bbe8db4e347b4e8c937c1c8370e4b5ed33adb3db69cbdb7a38e1e50b1b82fa",
        "gasLimit": "0x47E7C4"
    },
   "skaleConfig": {
    "nodeInfo": {
      "nodeName": "Node1",
      "nodeID": 1112,
      "bindIP": "127.0.0.1",
      "basePort": 1231,
      "logLevel": "trace",
      "logLevelProposal": "trace"
    },
    "sChain": {
        "schainName": "TestChain",
        "schainID": 1,
        "nodes": [
          { "nodeID": 1112, "ip": "127.0.0.1", "basePort": 1231, "schainIndex" : 1}
        ]
    }
  },
    "accounts": {
        "0000000000000000000000000000000000000001": { "precompiled": { "name": "ecrecover", "linear": { "base": 3000, "word": 0 } } },
        "0000000000000000000000000000000000000002": { "precompiled": { "name": "sha256", "linear": { "base": 60, "word": 12 } } },
        "0000000000000000000000000000000000000003": { "precompiled": { "name": "ripemd160", "linear": { "base": 600, "word": 120 } } },
        "0000000000000000000000000000000000000004": { "precompiled": { "name": "identity", "linear": { "base": 15, "word": 3 } } },
        "0000000000000000000000000000000000000005": { "precompiled": { "name": "modexp", "startingBlock" : "0x2dc6c0" } },
        "0000000000000000000000000000000000000006": { "precompiled": { "name": "alt_bn128_G1_add", "startingBlock" : "0x2dc6c0", "linear": { "base": 500, "word": 0 } } },
        "0000000000000000000000000000000000000007": { "precompiled": { "name": "alt_bn128_G1_mul", "startingBlock" : "0x2dc6c0", "linear": { "base": 40000, "word": 0 } } },
        "0000000000000000000000000000000000000008": { "precompiled": { "name": "alt_bn128_pairing_product", "startingBlock" : "0x2dc6c0" } },
        "0xca4409573a5129a72edf85d6c51e26760fc9c903": { "balance": "100000000000000000000000" },
        "0xD2001300000000000000000000000000000000D2": { "balance": "0", "nonce": "0", "storage": {}, "code":"0x6080604052348015600f57600080fd5b506004361060325760003560e01c8063815b8ab41460375780638273f754146062575b600080fd5b606060048036036020811015604b57600080fd5b8101908080359060200190929190505050606a565b005b60686081565b005b60005a90505b815a82031015607d576070565b5050565b60005a9050609660028281609157fe5b04606a565b5056fea165627a7a72305820f5fb5a65e97cbda96c32b3a2e1497cd6b7989179b5dc29e9875bcbea5a96c4520029"}
    }
}
)E";


BOOST_AUTO_TEST_SUITE( EstimateGas )

BOOST_AUTO_TEST_CASE( constantConsumption ) {
    TestClientFixture fixture( c_genesisInfoSkaleTest );
    ClientTest* testClient = asClientTest( fixture.ethereum() );

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
    TestClientFixture fixture( c_genesisInfoSkaleTest );
    ClientTest* testClient = asClientTest( fixture.ethereum() );

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
    TestClientFixture fixture( c_genesisInfoSkaleTest );
    ClientTest* testClient = asClientTest( fixture.ethereum() );

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
            "bindIP": "127.0.0.1",
            "basePort": 1231
        },
        "sChain": {
            "schainName": "TestChain",
            "schainID": 1,
            "snapshotIntervalMs": 10,
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

BOOST_AUTO_TEST_CASE( ClientSnapshotsTest, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    TestClientSnapshotsFixture fixture( c_skaleConfigString );
    ClientTest* testClient = asClientTest( fixture.ethereum() );

    BOOST_REQUIRE( fs::exists( fs::path( fixture.BTRFS_DIR_PATH ) / "snapshots" / "0" ) );

    BOOST_REQUIRE( testClient->mineBlocks( 1 ) );

    testClient->importTransactionsAsBlock(
        Transactions(), 1000, testClient->latestBlock().info().timestamp() + 86410 );

    BOOST_REQUIRE( fs::exists( fs::path( fixture.BTRFS_DIR_PATH ) / "snapshots" / "1" ) );

    secp256k1_sha256_t ctx;
    secp256k1_sha256_initialize( &ctx );

    dev::h256 empty_str = dev::h256( "" );
    secp256k1_sha256_write( &ctx, empty_str.data(), empty_str.size );

    dev::h256 empty_state_root_hash;
    secp256k1_sha256_finalize( &ctx, empty_state_root_hash.data() );

    BOOST_REQUIRE( testClient->latestBlock().info().stateRoot() == empty_state_root_hash );
}

BOOST_AUTO_TEST_SUITE_END()
