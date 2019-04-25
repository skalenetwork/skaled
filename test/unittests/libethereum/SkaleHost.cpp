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
        //dev::eth::simulateMining( *client, 1 );
    }

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

    BOOST_REQUIRE_EQUAL( blockTransactions.size(), 1 );
    BOOST_REQUIRE( blockTransactions[0] == txHash );
}

// Transaction should be EXCLUDED from the block
// Proposer should be penalized
// 1 0 bytes
// 2 Small amount of random bytes
// 3 110 random bytes
// 4 110 bytes of semi-correct RLP
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

    BOOST_REQUIRE_EQUAL( blockTransactions.size(), 0 );
}

// Transaction should be EXCLUDED from the block
// Proposer should be penalized
// zero signature
BOOST_AUTO_TEST_CASE( transactionSigZero ) {
    // TODO find out how to create zero signature
}

// Transaction should be EXCLUDED from the block
// Proposer should be penalized
// corrupted signature
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
    data[43] = 0x7f;  // spoil v

    BOOST_REQUIRE_EQUAL( client->number(), 0 );
    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{data}, utcTime(), 1U ) );
    BOOST_REQUIRE_EQUAL( client->number(), 1 );

    TransactionHashes blockTransactions =
        static_cast< Interface* >( client.get() )->transactionHashes( 1 );

    BOOST_REQUIRE_EQUAL( blockTransactions.size(), 0 );
}

// Transaction should be EXCLUDED from the block
// Proposer should be penalized
// gas < min_gas
BOOST_AUTO_TEST_CASE( transactionGasIncorrect ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value json;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );
    json["gas"] = "19000";

    TransactionSkeleton ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = accountHolder->authenticate( ts );
    Transaction tx( ts, ar.second );

    RLPStream stream;
    tx.streamRLP( stream );

    BOOST_REQUIRE_EQUAL( client->number(), 0 );
    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{stream.out()}, utcTime(), 1U ) );
    BOOST_REQUIRE_EQUAL( client->number(), 1 );

    TransactionHashes blockTransactions =
        static_cast< Interface* >( client.get() )->transactionHashes( 1 );

    BOOST_REQUIRE_EQUAL( blockTransactions.size(), 0 );
}

// Transaction should be COMMITTED to block
// Transaction should be REVERTED during execution
// Sender should be charged for gas consumed
// Proposer should NOT be penalized
// transaction exceedes it's gas limit
BOOST_AUTO_TEST_CASE( transactionGasNotEnough ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    // We change author because coinbase.address() is author address by default
    // and will take all transaction fee after execution so we can't check money spent
    // for senderAddress correctly.
    client->setAuthor( Address( 5 ) );

    // contract test {
    //  function f(uint a) returns(uint d) { return a * 7; }
    // }

    string compiled =
        "6080604052341561000f57600080fd5b60b98061001d6000396000f300"
        "608060405260043610603f576000357c01000000000000000000000000"
        "00000000000000000000000000000000900463ffffffff168063b3de64"
        "8b146044575b600080fd5b3415604e57600080fd5b606a600480360381"
        "019080803590602001909291905050506080565b604051808281526020"
        "0191505060405180910390f35b60006007820290509190505600a16562"
        "7a7a72305820f294e834212334e2978c6dd090355312a3f0f9476b8eb9"
        "8fb480406fc2728a960029";

    Json::Value json;
    int gas = 82000;                   // not enough but will pass size check
    string gasPrice = "100000000000";  // 100b
    json["from"] = toJS( senderAddress );
    json["code"] = compiled;
    json["gas"] = gas;
    json["gasPrice"] = gasPrice;

    TransactionSkeleton ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = accountHolder->authenticate( ts );
    Transaction tx( ts, ar.second );

    RLPStream stream;
    tx.streamRLP( stream );

    h256 txHash = tx.sha3();

    u256 balanceBefore = client->balanceAt( senderAddress );

    BOOST_REQUIRE_EQUAL( client->number(), 0 );
    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{stream.out()}, utcTime(), 1U ) );
    BOOST_REQUIRE_EQUAL( client->number(), 1 );

    TransactionHashes blockTransactions =
        static_cast< Interface* >( client.get() )->transactionHashes( 1 );

    BOOST_REQUIRE_EQUAL( blockTransactions.size(), 1 );
    BOOST_REQUIRE( blockTransactions[0] == txHash );

    u256 balanceAfter = client->balanceAt( senderAddress );
    BOOST_REQUIRE_EQUAL( balanceBefore - balanceAfter, u256( gas ) * u256( gasPrice ) );
}


// Transaction should be COMMITTED to block
// Transaction should be IGNORED during execution
// Proposer should be penalized
// nonce too big
BOOST_AUTO_TEST_CASE( transactionNonceBig ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value json;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );
    json["nonce"] = 1;

    TransactionSkeleton ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = accountHolder->authenticate( ts );
    Transaction tx( ts, ar.second );

    RLPStream stream;
    tx.streamRLP( stream );

    h256 txHash = tx.sha3();

    u256 balanceBefore = client->balanceAt( senderAddress );

    BOOST_REQUIRE_EQUAL( client->number(), 0 );
    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{stream.out()}, utcTime(), 1U ) );
    BOOST_REQUIRE_EQUAL( client->number(), 1 );

    u256 balanceAfter = client->balanceAt( senderAddress );

    BOOST_REQUIRE_EQUAL( balanceBefore, balanceAfter );

    TransactionHashes blockTransactions =
        static_cast< Interface* >( client.get() )->transactionHashes( 1 );

    BOOST_REQUIRE_EQUAL( blockTransactions.size(), 1 );
    BOOST_REQUIRE( blockTransactions[0] == txHash );
}

// Transaction should be COMMITTED to block
// Transaction should be IGNORED during execution
// Proposer should be penalized
// nonce too small
BOOST_AUTO_TEST_CASE( transactionNonceSmall ) {}

// Transaction should be COMMITTED to block
// Transaction should be IGNORED during execution
// Proposer should be penalized
// not enough cache
BOOST_AUTO_TEST_CASE( transactionBalanceBad ) {}

// Transaction should be COMMITTED to block
// Transaction should be IGNORED during execution
// Proposer should be penalized
// transaction goes beyond block gas limit
BOOST_AUTO_TEST_CASE( transactionGasBlockLimitExceeded ) {}

BOOST_AUTO_TEST_SUITE_END()
