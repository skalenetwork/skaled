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
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::test;
using namespace dev::p2p;
namespace fs = boost::filesystem;

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

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
