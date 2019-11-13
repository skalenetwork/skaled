#include <libskale/SnapshotManager.h>
#include <libweb3jsonrpc/Eth.h>
#include <libethereum/ClientTest.h>
#include <libweb3jsonrpc/AccountHolder.h>
#include <libweb3jsonrpc/Web3.h>
#include <libethcore/KeyManager.h>
#include <libp2p/Network.h>
#include <libdevcore/TransientDirectory.h>
#include <libweb3jsonrpc/AdminEth.h>
#include <libweb3jsonrpc/Debug.h>
#include <libweb3jsonrpc/Test.h>
#include <test/tools/libtesteth/TestHelper.h>
#include <test/tools/libtesteth/TestOutputHelper.h>
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include "../libweb3jsonrpc/WebThreeStubClient.h"

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::test;

namespace {
class TestIpcServer : public jsonrpc::AbstractServerConnector {
public:
    bool StartListening() override { return true; }
    bool StopListening() override { return true; }
    bool SendResponse( std::string const& _response, void* _addInfo = nullptr ) /*override*/ {
        *static_cast< std::string* >( _addInfo ) = _response;
        return true;
    }
};

class TestIpcClient : public jsonrpc::IClientConnector {
public:
    explicit TestIpcClient( TestIpcServer& _server ) : m_server{_server} {}

    void SendRPCMessage( const std::string& _message, std::string& _result ) override {
        m_server.ProcessRequest( _message, _result );
    }

private:
    TestIpcServer& m_server;
};

struct JsonRpcFixture : public TestOutputHelperFixture {
    JsonRpcFixture() {
        dev::p2p::NetworkPreferences nprefs;
        ChainParams chainParams;
        chainParams.sealEngineName = NoProof::name();
        chainParams.allowFutureBlocks = true;
        chainParams.difficulty = chainParams.minimumDifficulty;
        chainParams.gasLimit = chainParams.maxGasLimit;
        chainParams.byzantiumForkBlock = 0;
        chainParams.externalGasDifficulty = 1;
        // add random extra data to randomize genesis hash and get random DB path,
        // so that tests can be run in parallel
        // TODO: better make it use ethemeral in-memory databases
        chainParams.extraData = h256::random().asBytes();
        TransientDirectory tempDir;

        //        web3.reset( new WebThreeDirect(
        //            "eth tests", tempDir.path(), "", chainParams, WithExisting::Kill, {"eth"},
        //            true ) );

        client.reset( new eth::ClientTest( chainParams, ( int ) chainParams.networkID,
            shared_ptr< GasPricer >(), NULL, tempDir.path(), WithExisting::Kill ) );

        //        client.reset(
        //            new eth::Client( chainParams, ( int ) chainParams.networkID, shared_ptr<
        //            GasPricer >(),
        //                tempDir.path(), "", WithExisting::Kill, TransactionQueue::Limits{100000,
        //                1024} ) );

        client->injectSkaleHost();
        client->startWorking();

        client->setAuthor( coinbase.address() );

        using FullServer = ModularServer< rpc::EthFace, /* rpc::NetFace,*/ rpc::Web3Face,
            rpc::AdminEthFace /*, rpc::AdminNetFace*/, rpc::DebugFace, rpc::TestFace >;

        accountHolder.reset( new FixedAccountHolder( [&]() { return client.get(); }, {} ) );
        accountHolder->setAccounts( {coinbase, account2} );

        sessionManager.reset( new rpc::SessionManager() );
        adminSession =
            sessionManager->newSession( rpc::SessionPermissions{{rpc::Privilege::Admin}} );

        auto ethFace = new rpc::Eth( *client, *accountHolder.get() );

        gasPricer = make_shared< eth::TrivialGasPricer >( 0, DefaultGasPrice );

        rpcServer.reset( new FullServer( ethFace /*, new rpc::Net(*web3)*/,
            new rpc::Web3( /*web3->clientVersion()*/ ),  // TODO Add real version?
            new rpc::AdminEth( *client, *gasPricer, keyManager, *sessionManager.get() ),
            /*new rpc::AdminNet(*web3, *sessionManager), */ new rpc::Debug( *client ),
            new rpc::Test( *client ) ) );
        auto ipcServer = new TestIpcServer;
        rpcServer->addConnector( ipcServer );
        ipcServer->StartListening();

        auto client = new TestIpcClient( *ipcServer );
        rpcClient = unique_ptr< WebThreeStubClient >( new WebThreeStubClient( *client ) );
    }

    string sendingRawShouldFail( string const& _t ) {
        try {
            rpcClient->eth_sendRawTransaction( _t );
            BOOST_FAIL( "Exception expected." );
        } catch ( jsonrpc::JsonRpcException const& _e ) {
            return _e.GetMessage();
        }
        return string();
    }

    unique_ptr< Client > client;
    dev::KeyPair coinbase{KeyPair::create()};
    dev::KeyPair account2{KeyPair::create()};
    unique_ptr< FixedAccountHolder > accountHolder;
    unique_ptr< rpc::SessionManager > sessionManager;
    std::shared_ptr< eth::TrivialGasPricer > gasPricer;
    KeyManager keyManager{KeyManager::defaultPath(), SecretStore::defaultPath()};
    unique_ptr< ModularServer<> > rpcServer;
    unique_ptr< WebThreeStubClient > rpcClient;
    std::string adminSession;
};
} // namespace

BOOST_FIXTURE_TEST_SUITE( SnapshotHashingSuite, JsonRpcFixture )

BOOST_AUTO_TEST_CASE( eth_sendRawTransaction_validTransaction ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value t;
    t["from"] = toJS( senderAddress );
    t["to"] = toJS( receiver.address() );
    t["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );

    // Mine to generate a non-zero account balance
    const int blocksToMine = 2;
    const u256 blockReward = 3 * dev::eth::ether;
    cerr << "Reward: " << blockReward << endl;
    cerr << "Balance before: " << client->balanceAt( senderAddress ) << endl;
    dev::eth::simulateMining( *( client ), blocksToMine );
    cerr << "Balance after: " << client->balanceAt( senderAddress ) << endl;
    BOOST_CHECK_EQUAL( blockReward, client->balanceAt( senderAddress ) );

    auto signedTx = rpcClient->eth_signTransaction( t );
    BOOST_REQUIRE( !signedTx["raw"].empty() );

    auto txHash = rpcClient->eth_sendRawTransaction( signedTx["raw"].asString() );
    BOOST_REQUIRE( !txHash.empty() );
}

BOOST_AUTO_TEST_CASE( SnapshotHashing ) {
	SnapshotManager mgr( boost::filesystem::path( "btrfs" ), {"vol1", "vol2"} );

	mgr.doSnapshot( 1 );
	/*fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "vol1" / "d12" );
    fs::remove( fs::path( BTRFS_DIR_PATH ) / "vol2" / "d21" );*/
    mgr.doSnapshot( 2 );

    mgr.computeSnapshotHash( 1 );
    mgr.computeSnapshotHash( 2 );

    BOOST_REQUIRE( mgr.isSnapshotHashPresent( 1 ));
    BOOST_REQUIRE( mgr.isSnapshotHashPresent( 2 ));

    auto hash1 = mgr.getSnapshotHash( 1 );
    auto hash2 = mgr.getSnapshotHash( 2 );

    BOOST_REQUIRE( hash1 != hash2 );
}

BOOST_AUTO_TEST_SUITE_END()