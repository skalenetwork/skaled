#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wreturn-type"

#include <mapreduce_consensus/node/ConsensusEngine.h>

#include <test/tools/libtesteth/TestHelper.h>
#include <test/tools/libtesteth/TestOutputHelper.h>

#include <libethereum/ChainParams.h>
#include <libethereum/Client.h>
#include <libethereum/ConsensusStub.h>
#include <libethereum/GasPricer.h>
#include <libp2p/Network.h>
#include <libweb3jsonrpc/AccountHolder.h>
#include <libweb3jsonrpc/JsonHelper.h>

#include <libethcore/SealEngine.h>

#include <libdevcore/TransientDirectory.h>

#include <boost/test/unit_test.hpp>

#include <memory>

using namespace dev;
using namespace dev::eth;
using namespace dev::test;
using namespace std;

namespace {

class ConsensusTestStub : public ConsensusInterface {
private:
    ConsensusExtFace& m_extFace;

public:
    ConsensusTestStub( ConsensusExtFace& _extFace ) : m_extFace( _extFace ) {}
    ~ConsensusTestStub() override {}
    void parseFullConfigAndCreateNode( const std::string& _jsonConfig ) override {}
    void startAll() override {}
    void bootStrapAll() override {}
    void exitGracefully() override {}
    void stop() {}

    ConsensusExtFace::transactions_vector pendingTransactions( size_t _limit ) {
        return m_extFace.pendingTransactions( _limit );
    }
    void createBlock( const ConsensusExtFace::transactions_vector& _approvedTransactions,
        uint64_t _timeStamp, uint64_t _blockID ) {
        m_extFace.createBlock( _approvedTransactions, _timeStamp, _blockID );
    }
};

class ConsensusTestStubFactory : public ConsensusFactory {
public:
    virtual unique_ptr< ConsensusInterface > create( ConsensusExtFace& _extFace ) const override {
        result = new ConsensusTestStub( _extFace );
        return unique_ptr< ConsensusInterface >( result );
    }

    mutable ConsensusTestStub* result;
};

struct SkaleHostFixture : public TestOutputHelperFixture {
    SkaleHostFixture() {
        dev::p2p::NetworkPreferences nprefs;
        ChainParams chainParams;
        chainParams.sealEngineName = NoProof::name();
        chainParams.allowFutureBlocks = true;
        chainParams.difficulty = chainParams.minimumDifficulty;
        chainParams.gasLimit = chainParams.maxGasLimit;
        chainParams.byzantiumForkBlock = 0;
        // add random extra data to randomize genesis hash and get random DB path,
        // so that tests can be run in parallel
        // TODO: better make it use ethemeral in-memory databases
        chainParams.extraData = h256::random().asBytes();
        TransientDirectory tempDir;

        accountHolder.reset( new FixedAccountHolder( [&]() { return client.get(); }, {} ) );
        accountHolder->setAccounts( {coinbase} );

        gasPricer = make_shared< eth::TrivialGasPricer >( 0, DefaultGasPrice );

        client =
            make_unique< Client >( chainParams, chainParams.networkID, gasPricer, tempDir.path() );
        client->setAuthor( coinbase.address() );

        ConsensusTestStubFactory test_stub_factory;

        skaleHost = make_unique< SkaleHost >( *client, tq, &test_stub_factory );
        stub = test_stub_factory.result;

        // make money
        dev::eth::simulateMining( *client, 1 );
    }

    unique_ptr< Client > client;
    dev::KeyPair coinbase{KeyPair::create()};
    unique_ptr< FixedAccountHolder > accountHolder;
    std::shared_ptr< eth::TrivialGasPricer > gasPricer;

    unique_ptr< SkaleHost > skaleHost;
    TransactionQueue tq;
    ConsensusTestStub* stub;
};

}  // namespace

BOOST_FIXTURE_TEST_SUITE( SkaleHostSuite, SkaleHostFixture )

BOOST_AUTO_TEST_CASE( validTransaction ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value json;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );

    TransactionSkeleton ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = accountHolder->authenticate( ts );
    Transaction t( ts, ar.second );
    RLPStream s;
    t.streamRLP( s );
    Json::Value signedTx = toJson( t, s.out() );

    BOOST_REQUIRE( !signedTx["raw"].empty() );

//    auto txHash = rpcClient->eth_sendRawTransaction( signedTx["raw"].asString() );
//    BOOST_REQUIRE( !txHash.empty() );
}

BOOST_AUTO_TEST_SUITE_END()
