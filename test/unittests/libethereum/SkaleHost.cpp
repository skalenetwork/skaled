#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wreturn-type"

#include <libconsensus/node/ConsensusEngine.h>

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
        skaleHost = make_shared< SkaleHost >( *client, tq, &test_stub_factory );
        stub = test_stub_factory.result;

        client->injectSkaleHost( skaleHost );
        client->startWorking();

        // make money
        dev::eth::simulateMining( *client, 1 );
    }

    TransactionQueue tq;

    unique_ptr< Client > client;
    dev::KeyPair coinbase{KeyPair::create()};
    unique_ptr< FixedAccountHolder > accountHolder;
    std::shared_ptr< eth::TrivialGasPricer > gasPricer;

    shared_ptr< SkaleHost > skaleHost;
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
    Transaction tx( ts, ar.second );

    RLPStream stream;
    tx.streamRLP( stream );

    h256 txHash = tx.sha3();

    BOOST_REQUIRE_EQUAL( client->number(), 0 );
    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{stream.out()}, utcTime(), 1U ) );
    BOOST_REQUIRE_EQUAL( client->number(), 1 );

    TransactionHashes blockTransactions =
        static_cast< Interface* >( client.get() )->transactionHashes( 1 );

    BOOST_REQUIRE( blockTransactions.size() == 1 );
    BOOST_REQUIRE( blockTransactions[0] == txHash );
}

BOOST_AUTO_TEST_CASE( transactionRlpBad ) {
    bytes small_tx1 = bytes();
    bytes small_tx2 = jsToBytes( "0x0011223344556677889900" );
    bytes bad_tx1 = jsToBytes(
        "0x0011223344556677889900112233445566778899001122334455667788990011223344556677889900112233"
        "445566778899001122334455667788990011223344556677889900112233445566778899001122334455667788"
        "990011223344556677889900112233445566778899" );
    bytes bad_tx2 = jsToBytes(
        "0xf86c223344556677889900112233445566778899001122334455667788990011223344556677889900112233"
        "445566778899001122334455667788990011223344556677889900112233445566778899001122334455667788"
        "990011223344556677889900112233445566778899" );

    BOOST_REQUIRE_EQUAL( client->number(), 0 );
    BOOST_REQUIRE_NO_THROW( stub->createBlock(
        ConsensusExtFace::transactions_vector{small_tx1, small_tx2, bad_tx1, bad_tx2}, utcTime(),
        1U ) );
    BOOST_REQUIRE_EQUAL( client->number(), 1 );

    TransactionHashes blockTransactions =
        static_cast< Interface* >( client.get() )->transactionHashes( 1 );

    BOOST_REQUIRE( blockTransactions.size() == 0 );
}

BOOST_AUTO_TEST_CASE( transactionSigZero ) {
    // TODO find out how to create zero signature
}

BOOST_AUTO_TEST_CASE( transactionSigBad ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value json;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );

    TransactionSkeleton ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = accountHolder->authenticate( ts );
    Transaction tx( ts, ar.second );

    RLPStream stream;
    tx.streamRLP( stream );
    bytes data = stream.out();

    // TODO try to spoil other fields
    data[43] = 0x7f;      // spoil v

    BOOST_REQUIRE_EQUAL( client->number(), 0 );
    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{data}, utcTime(), 1U ) );
    BOOST_REQUIRE_EQUAL( client->number(), 1 );

    TransactionHashes blockTransactions =
        static_cast< Interface* >( client.get() )->transactionHashes( 1 );

    BOOST_REQUIRE( blockTransactions.size() == 0 );
}

BOOST_AUTO_TEST_CASE( transactionGasSmall ) {}

BOOST_AUTO_TEST_CASE( transactionNonceSmall ) {}

BOOST_AUTO_TEST_CASE( transactionNonceBig ) {}

BOOST_AUTO_TEST_CASE( transactionBalanceBad ) {}

BOOST_AUTO_TEST_CASE( transactionGasBlockLimitExceeded ) {}

BOOST_AUTO_TEST_SUITE_END()
