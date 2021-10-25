#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wreturn-type"

#include <libconsensus/node/ConsensusEngine.h>
#include <libskale/ConsensusGasPricer.h>

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

class ConsensusTestStub : public ConsensusInterface {
private:
    ConsensusExtFace& m_extFace;
    std::vector< u256 > block_gas_prices;
    bool need_exit = false;

public:
    ConsensusTestStub( ConsensusExtFace& _extFace ) : m_extFace( _extFace ) {
        block_gas_prices.push_back( 1000 );
    }
    ~ConsensusTestStub() override {}
    void parseFullConfigAndCreateNode( const std::string& _jsonConfig ) override {}
    void startAll() override {}
    void bootStrapAll() override {}
    void exitGracefully() override { need_exit = true; }
    consensus_engine_status getStatus() const override {
        return need_exit? CONSENSUS_EXITED : CONSENSUS_ACTIVE;
    }
    void stop() {}

    ConsensusExtFace::transactions_vector pendingTransactions( size_t _limit ) {
        u256 stateRoot = 0;
        return m_extFace.pendingTransactions( _limit, stateRoot );
    }
    void createBlock( const ConsensusExtFace::transactions_vector& _approvedTransactions,
        uint64_t _timeStamp, uint64_t _blockID, u256 _gasPrice = 0, u256 _stateRoot = 0, uint64_t _winningNodeIndex = -1 ) {
        m_extFace.createBlock(
            _approvedTransactions, _timeStamp, 0, _blockID, _gasPrice, _stateRoot, _winningNodeIndex );
        setPriceForBlockId( _blockID, _gasPrice );
    }

    u256 getPriceForBlockId( uint64_t _blockId ) const override {
        assert( _blockId < block_gas_prices.size() );
        return block_gas_prices.at( _blockId );
    }

    u256 getRandomForBlockId( uint64_t _blockId ) const override {
        return 0;
    }

    u256 setPriceForBlockId( uint64_t _blockId, u256 _gasPrice ) {
        assert( _blockId <= block_gas_prices.size() );
        if ( _blockId == block_gas_prices.size() )
            block_gas_prices.push_back( _gasPrice );
        else
            block_gas_prices[_blockId] = _gasPrice;
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

// TODO Do not copy&paste from JsonRpcFixture
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

        accountHolder.reset( new FixedAccountHolder( [&]() { return client.get(); }, {} ) );
        accountHolder->setAccounts( {coinbase, account2} );

        gasPricer = make_shared< eth::TrivialGasPricer >( 0, DefaultGasPrice );
        auto monitor = make_shared< InstanceMonitor >("test");
        client = make_unique< Client >(
            chainParams, chainParams.networkID, gasPricer, nullptr, monitor, tempDir.path() );
        this->tq = client->debugGetTransactionQueue();
        client->setAuthor( coinbase.address() );

        ConsensusTestStubFactory test_stub_factory;
        skaleHost = make_shared< SkaleHost >( *client, &test_stub_factory );
        stub = test_stub_factory.result;

        client->injectSkaleHost( skaleHost );
        client->setGasPricer( make_shared< ConsensusGasPricer >( *skaleHost ) );
        client->startWorking();

        // make money
        dev::eth::simulateMining( *client, 1 );

        // We change author because coinbase.address() is author address by default
        // and will take all transaction fee after execution so we can't check money spent
        // for senderAddress correctly.
        client->setAuthor( Address( 5 ) );
        dev::eth::g_skaleHost = skaleHost;
    }

    Transaction tx_from_json( const Json::Value& json ) {
        TransactionSkeleton ts = toTransactionSkeleton( json );
        ts = client->populateTransactionWithDefaults( ts );
        pair< bool, Secret > ar = accountHolder->authenticate( ts );
        return Transaction( ts, ar.second );
    }

    bytes bytes_from_json( const Json::Value& json ) {
        Transaction tx = tx_from_json( json );
        RLPStream stream;
        tx.streamRLP( stream );
        return stream.out();
    }

    TransactionQueue* tq;

    TransientDirectory tempDir; // ! should exist before client!
    unique_ptr< Client > client;

    dev::KeyPair coinbase{KeyPair::create()};
    dev::KeyPair account2{KeyPair::create()};
    unique_ptr< FixedAccountHolder > accountHolder;
    std::shared_ptr< eth::TrivialGasPricer > gasPricer;

    shared_ptr< SkaleHost > skaleHost;
    ConsensusTestStub* stub;
};

#define CHECK_BLOCK_BEGIN auto blockBefore = client->number()

#define REQUIRE_BLOCK_INCREASE( increase ) \
    auto blockAfter = client->number();    \
    BOOST_REQUIRE_EQUAL( blockAfter - blockBefore, increase )

#define REQUIRE_BLOCK_SIZE( number, s )                                             \
    {                                                                               \
        TransactionHashes blockTransactions =                                       \
            static_cast< Interface* >( client.get() )->transactionHashes( number ); \
        BOOST_REQUIRE_EQUAL( blockTransactions.size(), s );                         \
    }

#define REQUIRE_BLOCK_TRANSACTION( blockNumber, txNumber, txHash )                       \
    {                                                                                    \
        TransactionHashes blockTransactions =                                            \
            static_cast< Interface* >( client.get() )->transactionHashes( blockNumber ); \
        BOOST_REQUIRE_EQUAL( blockTransactions[txNumber], txHash );                      \
    }

#define CHECK_NONCE_BEGIN( senderAddress ) u256 nonceBefore = client->countAt( senderAddress )

#define REQUIRE_NONCE_INCREASE( senderAddress, increase ) \
    u256 nonceAfter = client->countAt( senderAddress );   \
    BOOST_REQUIRE_EQUAL( nonceAfter - nonceBefore, increase )

#define CHECK_BALANCE_BEGIN( senderAddress ) u256 balanceBefore = client->balanceAt( senderAddress )

#define REQUIRE_BALANCE_DECREASE( senderAddress, decrease ) \
    u256 balanceAfter = client->balanceAt( senderAddress ); \
    BOOST_REQUIRE_EQUAL( balanceBefore - balanceAfter, decrease )

#define REQUIRE_BALANCE_DECREASE_GE( senderAddress, decrease ) \
    u256 balanceAfter = client->balanceAt( senderAddress );    \
    BOOST_REQUIRE_GE( balanceBefore - balanceAfter, decrease )

BOOST_FIXTURE_TEST_SUITE( SkaleHostSuite, SkaleHostFixture )  //, *boost::unit_test::disabled() )

BOOST_AUTO_TEST_CASE( validTransaction ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value json;
    u256 gasPrice = 100 * dev::eth::shannon;  // 100b
    u256 value = 10000 * dev::eth::szabo;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( value ) );
    json["gasPrice"] = jsToDecimal( toJS( gasPrice ) );

    TransactionSkeleton ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = accountHolder->authenticate( ts );
    Transaction tx( ts, ar.second );

    RLPStream stream;
    tx.streamRLP( stream );

    h256 txHash = tx.sha3();

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{stream.out()}, utcTime(), 1U ) );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_SIZE( 1, 1 );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash );

    REQUIRE_NONCE_INCREASE( senderAddress, 1 );
    REQUIRE_BALANCE_DECREASE( senderAddress, value + gasPrice * 21000 );
}

// Transaction should be IGNORED during execution
// Proposer should be penalized
// 1 Small amount of random bytes
// 2 110 random bytes
// 3 110 bytes of semi-correct RLP
BOOST_AUTO_TEST_CASE( transactionRlpBad ) {
    auto senderAddress = coinbase.address();

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

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    BOOST_REQUIRE_NO_THROW( stub->createBlock(
        ConsensusExtFace::transactions_vector{small_tx1, small_tx2, bad_tx1, bad_tx2}, utcTime(),
        1U ) );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_SIZE( 1, 3 );

    REQUIRE_NONCE_INCREASE( senderAddress, 0 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 0 );

    // check transaction hashes
    Transactions txns = client->transactions( 1 );
    //    cerr << toJson( txns );

    REQUIRE_BLOCK_TRANSACTION( 1, 0, txns[0].sha3() );
    REQUIRE_BLOCK_TRANSACTION( 1, 1, txns[1].sha3() );
    REQUIRE_BLOCK_TRANSACTION( 1, 2, txns[2].sha3() );

    // check also receipts and locations
    size_t i = 0;
    for ( const Transaction& tx : txns ) {
        Transaction tx2 = client->transaction( tx.sha3() );
        LocalisedTransaction lt = client->localisedTransaction( tx.sha3() );
        LocalisedTransactionReceipt lr = client->localisedTransactionReceipt( tx.sha3() );

        BOOST_REQUIRE_EQUAL( tx2, tx );

        BOOST_REQUIRE_EQUAL( lt, tx );
        BOOST_REQUIRE_EQUAL( lt.blockNumber(), 1 );
        BOOST_REQUIRE_EQUAL( lt.blockHash(), client->hashFromNumber( 1 ) );
        BOOST_REQUIRE_EQUAL( lt.transactionIndex(), i );

        BOOST_REQUIRE_EQUAL( lr.hash(), tx.sha3() );
        BOOST_REQUIRE_EQUAL( lr.blockNumber(), lt.blockNumber() );
        BOOST_REQUIRE_EQUAL( lr.blockHash(), lt.blockHash() );
        BOOST_REQUIRE_EQUAL( lr.transactionIndex(), i );

        ++i;
    }  // for
}

class VrsHackedTransaction : public Transaction {
public:
    void resetSignature() {
        this->m_vrs->r = h256( 0 );
        this->m_vrs->s = h256( 0 );
    }
};

// Transaction should be IGNORED during execution
// Proposer should be penalized
// zero signature
BOOST_AUTO_TEST_CASE( transactionSigZero ) {
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

    // kill signature
    // HACK
    VrsHackedTransaction* hacked_tx = reinterpret_cast< VrsHackedTransaction* >( &tx );
    hacked_tx->resetSignature();

    RLPStream stream;
    tx.streamRLP( stream, WithSignature );

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{stream.out()}, utcTime(), 1U ) );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_SIZE( 1, 1 );

    h256 txHash = sha3( stream.out() );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash );

    REQUIRE_NONCE_INCREASE( senderAddress, 0 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 0 );
}

// Transaction should be IGNORED during execution
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

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{data}, utcTime(), 1U ) );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_SIZE( 1, 1 );

    h256 txHash = sha3( data );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash );

    REQUIRE_NONCE_INCREASE( senderAddress, 0 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 0 );
}

// Transaction should be IGNORED during execution
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

    h256 txHash = tx.sha3();

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{stream.out()}, utcTime(), 1U ) );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_SIZE( 1, 1 );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash );

    REQUIRE_NONCE_INCREASE( senderAddress, 0 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 0 );
}

// Transaction should be REVERTED during execution
// Sender should be charged for gas consumed
// Proposer should NOT be penalized
// transaction exceedes it's gas limit
BOOST_AUTO_TEST_CASE( transactionGasNotEnough ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

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

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{stream.out()}, utcTime(), 1U ) );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_SIZE( 1, 1 );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash );

    REQUIRE_NONCE_INCREASE( senderAddress, 1 );
    REQUIRE_BALANCE_DECREASE( senderAddress, u256( gas ) * u256( gasPrice ) );
}


// Transaction should be IGNORED during execution
// Proposer should be penalized
// nonce too big
BOOST_AUTO_TEST_CASE( transactionNonceBig, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
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

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{stream.out()}, utcTime(), 1U ) );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_SIZE( 1, 1 );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash );

    REQUIRE_NONCE_INCREASE( senderAddress, 0 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 0 );
}

// Transaction should be IGNORED during execution
// Proposer should be penalized
// nonce too small
BOOST_AUTO_TEST_CASE( transactionNonceSmall ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value json;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );
    json["nonce"] = 0;

    TransactionSkeleton ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = accountHolder->authenticate( ts );
    Transaction tx1( ts, ar.second );

    RLPStream stream1;
    tx1.streamRLP( stream1 );

    // create 1 txns in 1 block
    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{stream1.out()}, utcTime(), 1U ) );

    // now our test txn
    json["value"] = jsToDecimal( toJS( 9000 * dev::eth::szabo ) );
    ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    ar = accountHolder->authenticate( ts );
    Transaction tx2( ts, ar.second );

    RLPStream stream2;
    tx2.streamRLP( stream2 );

    h256 txHash = tx2.sha3();

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{stream2.out()}, utcTime(), 2U ) );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_SIZE( 2, 1 );
    REQUIRE_BLOCK_TRANSACTION( 2, 0, txHash );

    REQUIRE_NONCE_INCREASE( senderAddress, 0 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 0 );
}

// Transaction should be IGNORED during execution
// Proposer should be penalized
// not enough cash
BOOST_AUTO_TEST_CASE( transactionBalanceBad, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value json;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( 3 * dev::eth::ether + dev::eth::wei ) );
    json["nonce"] = 0;

    TransactionSkeleton ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = accountHolder->authenticate( ts );
    Transaction tx( ts, ar.second );

    RLPStream stream;
    tx.streamRLP( stream );

    h256 txHash = tx.sha3();

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{stream.out()}, utcTime(), 1U ) );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_SIZE( 1, 1 );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash );

    REQUIRE_NONCE_INCREASE( senderAddress, 0 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 0 );
}

// Transaction should be IGNORED during execution
// Proposer should be penalized
// transaction goes beyond block gas limit
BOOST_AUTO_TEST_CASE( transactionGasBlockLimitExceeded ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    // 1 txn with max gas
    Json::Value json;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );
    json["nonce"] = 0;
    json["gasPrice"] = 0;

    Transaction tx1 = tx_from_json( json );

    RLPStream stream1;
    tx1.streamRLP( stream1 );

    h256 txHash1 = tx1.sha3();

    // 2 txn
    json["value"] = jsToDecimal( toJS( 9000 * dev::eth::szabo ) );
    json["nonce"] = 1;
    json["gas"] = jsToDecimal( toJS( client->chainParams().gasLimit - 21000 + 1 ) );

    Transaction tx2 = tx_from_json( json );

    RLPStream stream2;
    tx2.streamRLP( stream2 );

    h256 txHash2 = tx2.sha3();

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    BOOST_REQUIRE_NO_THROW( stub->createBlock(
        ConsensusExtFace::transactions_vector{stream1.out(), stream2.out()}, utcTime(), 1U ) );
    BOOST_REQUIRE_EQUAL( client->number(), 1 );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_SIZE( 1, 2 );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash1 );
    REQUIRE_BLOCK_TRANSACTION( 1, 1, txHash2 );

    REQUIRE_NONCE_INCREASE( senderAddress, 1 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 10000 * dev::eth::szabo );  // only 1st!
}

// positive test for 4 next ones
BOOST_AUTO_TEST_CASE( transactionDropReceive ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value json;
    u256 value1 = 10000 * dev::eth::szabo;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( value1 ) );
    json["nonce"] = 1;

    // 1st tx
    Transaction tx1 = tx_from_json( json );
    tx1.checkOutExternalGas( client->chainParams().difficulty );

    // submit it!
    tq->import( tx1 );

    // 2nd tx
    u256 value2 = 20000 * dev::eth::szabo;
    json["value"] = jsToDecimal( toJS( value2 ) );
    json["nonce"] = 0;
    bytes tx2 = bytes_from_json( json );

    // receive it!
    skaleHost->receiveTransaction( toJS( tx2 ) );

    sleep( 1 );
    BOOST_REQUIRE_EQUAL( tq->knownTransactions().size(), 2 );

    // 3rd transaction to trigger re-verification
    u256 value3 = 30000 * dev::eth::szabo;
    json["value"] = jsToDecimal( toJS( value3 ) );
    json["nonce"] = 0;

    bytes tx3 = bytes_from_json( json );

    // return it from consensus!
    CHECK_BLOCK_BEGIN;
    CHECK_NONCE_BEGIN( senderAddress );

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{tx3}, utcTime(), 1U ) );
    stub->setPriceForBlockId( 1, 1000 );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_NONCE_INCREASE( senderAddress, 1 );

    // both should be known, but
    BOOST_REQUIRE_EQUAL( tq->knownTransactions().size(), 2 );
    // 2nd should be dropped, 1st kept
    ConsensusExtFace::transactions_vector txns = stub->pendingTransactions( 3 );
    BOOST_REQUIRE_EQUAL( txns.size(), 1 );
}

BOOST_AUTO_TEST_CASE( transactionDropQueue, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value json;
    u256 value1 = 10000 * dev::eth::szabo;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( value1 ) );
    json["gasPrice"] = jsToDecimal( "0x0" );
    json["nonce"] = 1;

    // 1st tx
    Transaction tx1 = tx_from_json( json );
    tx1.checkOutExternalGas( client->chainParams().difficulty );

    // submit it!
    tq->import( tx1 );

    sleep( 1 );
    BOOST_REQUIRE_EQUAL( tq->knownTransactions().size(), 1 );

    // 2nd transaction will remove 1
    u256 value2 = 8000 * dev::eth::szabo;
    json["value"] = jsToDecimal( toJS( value2 ) );
    json["nonce"] = 0;

    Transaction tx2 = tx_from_json( json );

    RLPStream stream2;
    tx2.streamRLP( stream2 );

    h256 txHash2 = tx2.sha3();

    // return it from consensus!
    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{stream2.out()}, utcTime(), 1U ) );
    stub->setPriceForBlockId( 1, 1000 );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash2 );

    REQUIRE_NONCE_INCREASE( senderAddress, 1 );
    REQUIRE_BALANCE_DECREASE( senderAddress, value2 );

    // should not be accessible from queue
    ConsensusExtFace::transactions_vector txns = stub->pendingTransactions( 1 );
    BOOST_REQUIRE_EQUAL( txns.size(), 0 );
}

// TODO Check exact dropping reason!
BOOST_AUTO_TEST_CASE( transactionDropByGasPrice, 
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value json;
    u256 value1 = 10000 * dev::eth::szabo;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( value1 ) );
    json["gasPrice"] = "1000";
    json["nonce"] = 1;

    // 1st tx
    Transaction tx1 = tx_from_json( json );
    tx1.checkOutExternalGas( client->chainParams().difficulty );

    // submit it!
    tq->import( tx1 );

    sleep( 1 );
    BOOST_REQUIRE_EQUAL( tq->knownTransactions().size(), 1 );

    // 2nd transaction will remove 1
    u256 value2 = 8000 * dev::eth::szabo;
    json["value"] = jsToDecimal( toJS( value2 ) );
    json["nonce"] = 0;

    Transaction tx2 = tx_from_json( json );

    RLPStream stream2;
    tx2.streamRLP( stream2 );

    h256 txHash2 = tx2.sha3();

    // return it from consensus!
    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    BOOST_REQUIRE_NO_THROW( stub->createBlock(
        ConsensusExtFace::transactions_vector{stream2.out()}, utcTime(), 1U, 1000 ) );
    stub->setPriceForBlockId( 1, 1100 );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash2 );

    REQUIRE_NONCE_INCREASE( senderAddress, 1 );
    REQUIRE_BALANCE_DECREASE( senderAddress, value2 + 21000 * 1000 );

    // should not be accessible from queue
    ConsensusExtFace::transactions_vector txns = stub->pendingTransactions( 1 );
    BOOST_REQUIRE_EQUAL( txns.size(), 0 );
}

// TODO Check exact dropping reason!
BOOST_AUTO_TEST_CASE( transactionDropByGasPriceReceive ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    {
        auto wr_state = client->state().startWrite();
        wr_state.addBalance( account2.address(), 1 * ether );
        wr_state.commit();
    }

    Json::Value json;
    u256 value1 = 10000 * dev::eth::szabo;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( value1 ) );
    json["nonce"] = 0;
    json["gasPrice"] = "1000";

    // 1st tx
    Transaction tx1 = tx_from_json( json );
    tx1.checkOutExternalGas( client->chainParams().difficulty );

    RLPStream stream1;
    tx1.streamRLP( stream1 );

    // receive it!
    skaleHost->receiveTransaction( toJS( stream1.out() ) );

    sleep( 1 );
    BOOST_REQUIRE_EQUAL( tq->knownTransactions().size(), 1 );

    // 2nd transaction will remove 1
    u256 value2 = 8000 * dev::eth::szabo;
    json["from"] = toJS( account2.address() );
    json["value"] = jsToDecimal( toJS( value2 ) );
    json["nonce"] = 0;

    Transaction tx2 = tx_from_json( json );

    RLPStream stream2;
    tx2.streamRLP( stream2 );

    h256 txHash2 = tx2.sha3();

    // return it from consensus!
    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    BOOST_REQUIRE_NO_THROW( stub->createBlock(
        ConsensusExtFace::transactions_vector{stream2.out()}, utcTime(), 1U, 1000 ) );
    stub->setPriceForBlockId( 1, 1100 );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash2 );

    REQUIRE_NONCE_INCREASE( account2.address(), 1 );
    REQUIRE_BALANCE_DECREASE_GE( account2.address(), value2 );

    // should not be accessible from queue
    ConsensusExtFace::transactions_vector txns = stub->pendingTransactions( 1 );
    BOOST_REQUIRE_EQUAL( txns.size(), 0 );
}

BOOST_AUTO_TEST_CASE( transactionRace ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value json;
    u256 gasPrice = 100 * dev::eth::shannon;  // 100b
    u256 value = 10000 * dev::eth::szabo;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( value ) );
    json["gasPrice"] = jsToDecimal( toJS( gasPrice ) );
    json["nonce"] = 0;

    Transaction tx = tx_from_json( json );

    RLPStream stream;
    tx.streamRLP( stream );

    h256 txHash = tx.sha3();

    // 1 add tx as normal
    client->importTransaction( tx );

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    // 2 get it from consensus
    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{stream.out()}, utcTime(), 1U ) );
    stub->setPriceForBlockId( 1, 1000 );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_SIZE( 1, 1 );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash );

    REQUIRE_NONCE_INCREASE( senderAddress, 1 );
    REQUIRE_BALANCE_DECREASE( senderAddress, value + gasPrice * 21000 );

    // 2 should be dropped from q
    ConsensusExtFace::transactions_vector tx_from_q = stub->pendingTransactions( 1 );
    BOOST_REQUIRE_EQUAL( tx_from_q.size(), 0 );

    // 3 send new tx and see nonce
    json["nonce"] = 1;
    Transaction tx2 = tx_from_json( json );

    client->importTransaction( tx2 );
}

// test two blocks with overlapping transactions :)
BOOST_AUTO_TEST_CASE( partialCatchUp ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value json;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );
    json["nonce"] = 0;

    TransactionSkeleton ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = accountHolder->authenticate( ts );
    Transaction tx1( ts, ar.second );

    RLPStream stream1;
    tx1.streamRLP( stream1 );

    // create 1 txns in 1 block
    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{stream1.out()}, utcTime(), 1U ) );

    // now 2 txns
    json["value"] = jsToDecimal( toJS( 9000 * dev::eth::szabo ) );
    ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    ar = accountHolder->authenticate( ts );
    Transaction tx2( ts, ar.second );

    RLPStream stream2;
    tx2.streamRLP( stream2 );

    h256 txHash = tx2.sha3();

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( ConsensusExtFace::transactions_vector{stream1.out(), stream2.out()}, utcTime(), 2U ) );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_SIZE( 2, 2 );
    REQUIRE_BLOCK_TRANSACTION( 2, 1, txHash );

    REQUIRE_NONCE_INCREASE( senderAddress, 0 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 0 );
}

BOOST_AUTO_TEST_CASE( getBlockRandom ) {
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "getBlockRandom" );
    auto res = exec( bytesConstRef() );
    u256 blockRandom = skaleHost->getBlockRandom();
    BOOST_REQUIRE( res.first );
    BOOST_REQUIRE( res.second == toBigEndian( static_cast< u256 >( blockRandom ) ) );
}

BOOST_AUTO_TEST_SUITE_END()
