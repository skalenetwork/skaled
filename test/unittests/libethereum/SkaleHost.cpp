#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wreturn-type"

#include <libconsensus/node/ConsensusEngine.h>
#ifdef BITE
#include <libconsensus/libBLS/threshold_encryption/ThresholdEncryption.h>
#endif
#include <libskale/ConsensusGasPricer.h>

#ifdef BITE
#include <libconsensus/libBLS/threshold_encryption/TEPrivateKey.h>
#include <libconsensus/libBLS/threshold_encryption/TEPrivateKeyShare.h>
#include <libconsensus/libBLS/threshold_encryption/TEPublicKeyShare.h>
#include <libconsensus/libBLS/threshold_encryption/TEDecryptSet.h>
#include <test/utils.h>
#include <secp256k1.h>
#include <secp256k1_ecdh.h>
#include <secp256k1_sha256.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <libdevcore/RLP.h>
#endif

#include <test/tools/libtesteth/TestHelper.h>
#include <test/tools/libtesteth/TestOutputHelper.h>

#include <libethereum/ChainParams.h>
#include <libethereum/Client.h>
#include <libethereum/ConsensusStub.h>
#include <libethereum/GasPricer.h>
#include <libp2p/Network.h>
#include <libskale/SkipInvalidTransactionsPatch.h>
#include <libweb3jsonrpc/AccountHolder.h>
#include <libweb3jsonrpc/JsonHelper.h>

#include <json_spirit/JsonSpiritHeaders.h>

#include <libethcore/SealEngine.h>

#include <libdevcore/TransientDirectory.h>

#include <boost/test/data/monomorphic.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

#include <memory>
#include <atomic>
#include <limits>

using namespace dev;
using namespace dev::eth;
using namespace dev::test;
using namespace std;

static size_t rand_port = 1024 + rand() % 64000;

#ifdef FAIR
// We need this mock class to avoid broken consensus after rotation.
// Since test client does not save persistent state, restart triggered by rotation causes SIGABRT.
class MockRotationSkaleHost : public SkaleHost {
public:
    MockRotationSkaleHost( Client& _client, ConsensusFactory* _factory )
        : SkaleHost( _client, _factory ) {}

    void runCommitteeRotationForConsensus() override { ++rotationCallCount; }

    std::atomic< uint32_t > rotationCallCount{0};
};
#endif

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
    void parseFullConfigAndCreateNode(
        const std::string& /*_jsonConfig */, const string& /*_gethURL*/ ) override {}
    void startAll() override {}
    void bootStrapAll() override {}
    void exitGracefully() override { need_exit = true; }
    consensus_engine_status getStatus() const override {
        return need_exit ? CONSENSUS_EXITED : CONSENSUS_ACTIVE;
    }
    void stop() {}

    ConsensusExtFace::Transactions pendingTransactions( size_t _limit ) {
        u256 stateRoot = 0;
        return m_extFace.pendingTransactions( _limit, stateRoot );
    }
    void createBlock( const ConsensusExtFace::Transactions& _approvedTransactions,
#ifdef BITE
        DecryptedTransactions _decryptedTransactions,
#endif
        uint64_t _timeStamp, uint64_t _blockID, u256 _gasPrice = 0, u256 _stateRoot = 0,
#ifdef FAIR
        uint64_t _winningNodeIndex = 1
#else
        uint64_t _winningNodeIndex = -1
#endif
    ) {
        m_extFace.createBlock( _approvedTransactions,
#ifdef BITE
            _decryptedTransactions,
#endif
            _timeStamp, 0, _blockID, _gasPrice, _stateRoot, _winningNodeIndex );
        setPriceForBlockId( _blockID, _gasPrice );
    }

    u256 getPriceForBlockId( uint64_t _blockId ) const override {
        assert( _blockId < block_gas_prices.size() );
        return block_gas_prices.at( _blockId );
    }

    u256 getRandomForBlockId( uint64_t _blockId ) const override { return _blockId; }

#ifdef BITE
    u256 getReencryptionRandomForBlockId( uint64_t _blockId ) const override { return _blockId; }
#endif

    u256 setPriceForBlockId( uint64_t _blockId, u256 _gasPrice ) {
        assert( _blockId <= block_gas_prices.size() );
        if ( _blockId == block_gas_prices.size() )
            block_gas_prices.push_back( _gasPrice );
        else
            block_gas_prices[_blockId] = _gasPrice;
        return 0;
    }

    uint64_t submitOracleRequest( const string& /*_spec*/,
        string&
        /*_receipt*/,
        string& /*error*/ ) override {
        return 0;
    }

    uint64_t checkOracleResult( const string&
        /*_receipt*/,
        string& /*_result */ ) override {
        return 0;
    }

    map< string, uint64_t > getConsensusDbUsage() const override {
        return map< string, uint64_t >();
    };

    virtual ConsensusInterface::SyncInfo getSyncInfo() override {
        return ConsensusInterface::SyncInfo{};
    };

#ifdef FAIR
    void updateLogger() const override {}
#endif
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
    SkaleHostFixture( const std::map< std::string, std::string >& params =
                          std::map< std::string, std::string >(),
        bool mockCommitteeRotation = false ) {
        dev::p2p::NetworkPreferences nprefs;
        libBLS::init();

        chainParams = std::make_shared< ChainParams >();
        chainParams->sealEngineName = NoProof::name();
        chainParams->allowFutureBlocks = true;
        chainParams->difficulty = chainParams->getMinimumDifficulty();
        chainParams->gasLimit = chainParams->getMaxGasLimit();
        chainParams->istanbulForkBlock = 0;
        // add random extra data to randomize genesis hash and get random DB path,
        // so that tests can be run in parallel
        // TODO: better make it use ethemeral in-memory databases
        chainParams->extraData = h256::random().asBytes();
#ifdef FAIR
        chainParams->sChain.nodeGroups = {
            { { GroupNode {
                u256( 0 ),
                u256( 8 ),
                "0xf925c203a30ec6cad5a263db3efab7ed4c1fd74c8688167e10a5a22e15ab5018d8553df0ac54ea"
                "10"
                "5a3d21845e5660bc3d4e7c82e7af1daa3baad393b1521467",
                Address( "0x08151B8F80bfa7dEa760e461412AF24348224edf" )
            } },
                uint64_t( -1 ),
                { "0",
                    "0",
                    "1",
                    "0" } }
        };
#else
        chainParams->sChain.nodeGroups = { { {}, uint64_t( -1 ), { "0", "0", "1", "0" } } };
#endif
        chainParams->nodeInfo.port = chainParams->nodeInfo.port6 = rand_port;
        chainParams->nodeInfo.testSignatures = true;
        chainParams->sChain.nodes[0].port = chainParams->sChain.nodes[0].port6 = rand_port;

        // not 0-timestamp genesis - to test patch
        chainParams->timestamp = std::time( NULL ) - 5;

        if ( params.count( "multiTransactionMode" ) && stoi( params.at( "multiTransactionMode" ) ) )
            chainParams->sChain.multiTransactionMode = true;
        if ( params.count( "skipInvalidTransactionsPatchTimestamp" ) &&
             stoi( params.at( "skipInvalidTransactionsPatchTimestamp" ) ) )
            chainParams->sChain._patchTimestamps[static_cast< size_t >(
                SchainPatchEnum::SkipInvalidTransactionsPatch )] =
                stoi( params.at( "skipInvalidTransactionsPatchTimestamp" ) );

        accountHolder.reset( new FixedAccountHolder( [&]() { return client.get(); }, {} ) );
        accountHolder->setAccounts( { coinbase, account2 } );

        gasPricer = make_shared< eth::TrivialGasPricer >( 0, DefaultGasPrice );
        auto monitor = make_shared< InstanceMonitor >( "test" );

        setenv( "DATA_DIR", tempDir.path().c_str(), 1 );
        client = make_unique< Client >(
            chainParams, chainParams->getNetworkId(), gasPricer, nullptr, monitor, tempDir.path() );
        this->tq = client->debugGetTransactionQueue();
        client->setAuthor( coinbase.address() );

        ConsensusTestStubFactory test_stub_factory;
#ifdef FAIR
        if ( mockCommitteeRotation ) {
            auto mockHost = std::make_shared< MockRotationSkaleHost >( *client, &test_stub_factory );
            skaleHost = mockHost;
            mockRotationHost = mockHost;
        } else
#endif
        {
            skaleHost = make_shared< SkaleHost >( *client, &test_stub_factory );
        }
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

#ifdef FAIR
    void overwriteHistoricNodeGroups( const std::vector< dev::eth::NodeGroup >& _groups ) {
        chainParams->sChain.nodeGroups = _groups;
    }

    void setCurrentGroupStartTimestamps( uint64_t _first, uint64_t _second ) {
        chainParams->sChain.currentGroups[0].startTs = _first;
        chainParams->sChain.currentGroups[1].startTs = _second;
    }

    uint64_t blockTimestamp( dev::eth::BlockNumber _number ) const {
        return client->blockInfo( _number ).timestamp();
    }
#endif

    Transaction tx_from_json( const Json::Value& json ) {
        TransactionSkeleton ts = toTransactionSkeleton( json );
        ts = client->populateTransactionWithDefaults( ts );
        pair< bool, Secret > ar = accountHolder->authenticate( ts );
        return Transaction( ts, ar.second );
    }

    bytes bytes_from_json( const Json::Value& json ) {
        Transaction tx = tx_from_json( json );
        return tx.toBytes();
    }

    void setBlsPublicKey( const std::array< std::string, 4 >& _key ) {
        chainParams->sChain.nodeGroups[0].blsPublicKey = _key;
    }

    void setNodeGroups( const std::vector< dev::eth::NodeGroup >& _groups ) {
        chainParams->sChain.nodeGroups = _groups;
    }

    void setGroupFinishTs( size_t _idx, uint64_t _ts ) {
        BOOST_REQUIRE( _idx < chainParams->sChain.nodeGroups.size() );
        chainParams->sChain.nodeGroups[_idx].finishTs = _ts;
    }

    TransactionQueue* tq;

    TransientDirectory tempDir;  // ! should exist before client!
    std::shared_ptr< ChainParams > chainParams;
    unique_ptr< Client > client;

    dev::KeyPair coinbase{ KeyPair::create() };
    dev::KeyPair account2{ KeyPair::create() };
    unique_ptr< FixedAccountHolder > accountHolder;
    std::shared_ptr< eth::TrivialGasPricer > gasPricer;

    shared_ptr< SkaleHost > skaleHost;
#ifdef FAIR
    std::shared_ptr< MockRotationSkaleHost > mockRotationHost;
#endif
    ConsensusTestStub* stub;
};

#define CHECK_BLOCK_BEGIN auto blockBefore = client->number()

#define REQUIRE_BLOCK_INCREASE( increase )                         \
    {                                                              \
        auto blockAfter = client->number();                        \
        BOOST_REQUIRE_EQUAL( blockAfter - blockBefore, increase ); \
    }

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

#define REQUIRE_NONCE_INCREASE( senderAddress, increase )          \
    {                                                              \
        u256 nonceAfter = client->countAt( senderAddress );        \
        BOOST_REQUIRE_EQUAL( nonceAfter - nonceBefore, increase ); \
    }

#define CHECK_BALANCE_BEGIN( senderAddress ) u256 balanceBefore = client->balanceAt( senderAddress )

#define REQUIRE_BALANCE_DECREASE( senderAddress, decrease )            \
    {                                                                  \
        u256 balanceAfter = client->balanceAt( senderAddress );        \
        BOOST_REQUIRE_EQUAL( balanceBefore - balanceAfter, decrease ); \
    }

#define REQUIRE_BALANCE_DECREASE_GE( senderAddress, decrease )      \
    {                                                               \
        u256 balanceAfter = client->balanceAt( senderAddress );     \
        BOOST_REQUIRE_GE( balanceBefore - balanceAfter, decrease ); \
    }

BOOST_AUTO_TEST_SUITE( SkaleHostSuite )  //, *boost::unit_test::disabled() )

#ifndef FAIR
auto skipInvalidTransactionsVariants = boost::unit_test::data::make( { false, true } );
#else
auto skipInvalidTransactionsVariants = boost::unit_test::data::make( { true } );
#endif

BOOST_DATA_TEST_CASE(
    validTransaction, skipInvalidTransactionsVariants, skipInvalidTransactionsFlag ) {
    SkaleHostFixture fixture(
        std::map< std::string, std::string >( { { "skipInvalidTransactionsPatchTimestamp",
            to_string( int( skipInvalidTransactionsFlag ) ) } } ) );
    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& accountHolder = fixture.accountHolder;
    auto& stub = fixture.stub;

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

    h256 txHash = tx.sha3();

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    ConsensusExtFace::Transactions txns;
    txns.pushBackRegular( tx.toBytes() );
    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( txns,
#ifdef BITE
                           DecryptedTransactions{
#ifdef BITE
                                   std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                   std::make_shared< DecryptedRegularTxsMap >()
                               },
#endif
            utcTime(), 1U ) );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_SIZE( 1, 1 );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash );

    REQUIRE_NONCE_INCREASE( senderAddress, 1 );
    REQUIRE_BALANCE_DECREASE( senderAddress, value + gasPrice * 21000 );
}

// Transaction should be IGNORED or EXCLUDED during execution (depending on
// skipInvalidTransactionsFlag) Proposer should be penalized 1 Small amount of random bytes 2 110
// random bytes 3 110 bytes of semi-correct RLP
BOOST_DATA_TEST_CASE(
    transactionRlpBad, skipInvalidTransactionsVariants, skipInvalidTransactionsFlag
    // , *boost::unit_test::precondition( dev::test::run_not_express )
) {
    SkaleHostFixture fixture(
        std::map< std::string, std::string >( { { "skipInvalidTransactionsPatchTimestamp",
            to_string( int( skipInvalidTransactionsFlag ) ) } } ) );
    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& stub = fixture.stub;

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

    ConsensusExtFace::Transactions blockTxns;
    blockTxns.pushBackRegular( small_tx1 );
    blockTxns.pushBackRegular( small_tx2 );
    blockTxns.pushBackRegular( bad_tx1 );
    blockTxns.pushBackRegular( bad_tx2 );

    BOOST_REQUIRE_NO_THROW( stub->createBlock(
        blockTxns,
#ifdef BITE
                                DecryptedTransactions{
#ifdef BITE
                                        std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                        std::make_shared< DecryptedRegularTxsMap >()
                                    },
#endif
        utcTime(), 1U ) );

    REQUIRE_BLOCK_INCREASE( 1 );

    if ( skipInvalidTransactionsFlag ) {
        REQUIRE_BLOCK_SIZE( 1, 0 );
    } else {
        REQUIRE_BLOCK_SIZE( 1, 3 );
    }

    REQUIRE_NONCE_INCREASE( senderAddress, 0 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 0 );

    // check transaction hashes
    Transactions txns = client->transactions( 1 );
    //    cerr << toJson( txns );

    if ( !skipInvalidTransactionsFlag ) {
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
}

class VrsHackedTransaction : public Transaction {
public:
    void resetSignature() {
        this->m_vrs->r = h256( 0 );
        this->m_vrs->s = h256( 0 );
    }
};

// Transaction should be IGNORED during execution or absent if skipInvalidTransactionsFlag
// Proposer should be penalized
// zero signature
BOOST_DATA_TEST_CASE(
    transactionSigZero, skipInvalidTransactionsVariants, skipInvalidTransactionsFlag
    // , *boost::unit_test::precondition( dev::test::run_not_express )
) {
    SkaleHostFixture fixture(
        std::map< std::string, std::string >( { { "skipInvalidTransactionsPatchTimestamp",
            to_string( int( skipInvalidTransactionsFlag ) ) } } ) );
    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& accountHolder = fixture.accountHolder;
    auto& stub = fixture.stub;

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

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    ConsensusExtFace::Transactions txns;
    txns.pushBackRegular( tx.toBytes() );

    BOOST_REQUIRE_NO_THROW(
            stub->createBlock( txns,
#ifdef BITE
                               DecryptedTransactions{
#ifdef BITE
                                       std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                       std::make_shared< DecryptedRegularTxsMap >()
                                   },
#endif
            utcTime(), 1U ) );

    REQUIRE_BLOCK_INCREASE( 1 );

    if ( skipInvalidTransactionsFlag ) {
        REQUIRE_BLOCK_SIZE( 1, 0 );
    } else {
        REQUIRE_BLOCK_SIZE( 1, 1 );
        h256 txHash = sha3( tx.toBytes() );
        REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash );
    }

    REQUIRE_NONCE_INCREASE( senderAddress, 0 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 0 );
}

// Transaction should be IGNORED during execution or absent if skipInvalidTransactionsFlag
// Proposer should be penalized
// corrupted signature
BOOST_DATA_TEST_CASE(
    transactionSigBad, skipInvalidTransactionsVariants, skipInvalidTransactionsFlag
    // , *boost::unit_test::precondition( dev::test::run_not_express )
) {
    SkaleHostFixture fixture(
        std::map< std::string, std::string >( { { "skipInvalidTransactionsPatchTimestamp",
            to_string( int( skipInvalidTransactionsFlag ) ) } } ) );
    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& accountHolder = fixture.accountHolder;
    auto& stub = fixture.stub;

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

    // spoil txn siganture to make it invalid
    RLPStream txnRlp( 9 );
    txnRlp << tx.nonce();     // nonce
    txnRlp << tx.gasPrice();  // gasPrice
    txnRlp << tx.gas();       // gasLimit
    txnRlp << tx.to();        // to
    txnRlp << tx.value();     // value
    txnRlp << tx.data();      // data

    txnRlp << 30;                // v
    txnRlp << tx.signature().r;  // r
    txnRlp << tx.signature().s;  // s

    auto rlpBytes = txnRlp.out();

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    ConsensusExtFace::Transactions txns;
    txns.pushBackRegular( rlpBytes );

    BOOST_REQUIRE_NO_THROW( stub->createBlock( txns,
#ifdef BITE
                                               DecryptedTransactions{
#ifdef BITE
                                                       std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                                       std::make_shared< DecryptedRegularTxsMap >()
                                                   },
#endif
        utcTime(), 1U ) );

    REQUIRE_BLOCK_INCREASE( 1 );


    if ( skipInvalidTransactionsFlag ) {
        REQUIRE_BLOCK_SIZE( 1, 0 );
    } else {
        REQUIRE_BLOCK_SIZE( 1, 1 );
        h256 txHash = sha3( rlpBytes );
        REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash );
    }

    REQUIRE_NONCE_INCREASE( senderAddress, 0 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 0 );
}

// Transaction should be IGNORED during execution or absent if skipInvalidTransactionsFlag
// Proposer should be penalized
// gas < min_gas
BOOST_DATA_TEST_CASE(
    transactionGasIncorrect, skipInvalidTransactionsVariants, skipInvalidTransactionsFlag
    // , *boost::unit_test::precondition( dev::test::run_not_express )
) {
    SkaleHostFixture fixture(
        std::map< std::string, std::string >( { { "skipInvalidTransactionsPatchTimestamp",
            to_string( int( skipInvalidTransactionsFlag ) ) } } ) );
    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& accountHolder = fixture.accountHolder;
    auto& stub = fixture.stub;

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

    h256 txHash = tx.sha3();

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    ConsensusExtFace::Transactions txns;
    txns.pushBackRegular( tx.toBytes() );

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( txns,
#ifdef BITE
                           DecryptedTransactions{
#ifdef BITE
                                   std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                   std::make_shared< DecryptedRegularTxsMap >()
                               },
#endif
            utcTime(), 1U ) );

    REQUIRE_BLOCK_INCREASE( 1 );

    if ( skipInvalidTransactionsFlag ) {
        REQUIRE_BLOCK_SIZE( 1, 0 );
    } else {
        REQUIRE_BLOCK_SIZE( 1, 1 );
        REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash );
    }

    REQUIRE_NONCE_INCREASE( senderAddress, 0 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 0 );
}

// Transaction should be REVERTED during execution
// Sender should be charged for gas consumed
// Proposer should NOT be penalized
// transaction exceedes it's gas limit
BOOST_DATA_TEST_CASE(
    transactionGasNotEnough, skipInvalidTransactionsVariants, skipInvalidTransactionsFlag
    // , *boost::unit_test::precondition( dev::test::run_not_express )
) {
    SkaleHostFixture fixture(
        std::map< std::string, std::string >( { { "skipInvalidTransactionsPatchTimestamp",
            to_string( int( skipInvalidTransactionsFlag ) ) } } ) );
    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& accountHolder = fixture.accountHolder;
    auto& stub = fixture.stub;

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

    h256 txHash = tx.sha3();

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    ConsensusExtFace::Transactions txns;
    txns.pushBackRegular( tx.toBytes() );

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( txns,
#ifdef BITE
                           DecryptedTransactions{
#ifdef BITE
                                   std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                   std::make_shared< DecryptedRegularTxsMap >()
                               },
#endif
            utcTime(), 1U ) );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_SIZE( 1, 1 );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash );

    REQUIRE_NONCE_INCREASE( senderAddress, 1 );
    REQUIRE_BALANCE_DECREASE( senderAddress, u256( gas ) * u256( gasPrice ) );
}


// Transaction should be IGNORED during execution or absent if skipInvalidTransactionsFlag
// Proposer should be penalized
// nonce too big
BOOST_DATA_TEST_CASE(
    transactionNonceBig, skipInvalidTransactionsVariants, skipInvalidTransactionsFlag ) {
    SkaleHostFixture fixture(
        std::map< std::string, std::string >( { { "skipInvalidTransactionsPatchTimestamp",
            to_string( int( skipInvalidTransactionsFlag ) ) } } ) );
    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& accountHolder = fixture.accountHolder;
    auto& stub = fixture.stub;

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

    h256 txHash = tx.sha3();

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    ConsensusExtFace::Transactions txns;
    txns.pushBackRegular( tx.toBytes() );

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( txns,
#ifdef BITE
                           DecryptedTransactions{
#ifdef BITE
                                   std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                   std::make_shared< DecryptedRegularTxsMap >()
                               },
#endif
            utcTime(), 1U ) );

    REQUIRE_BLOCK_INCREASE( 1 );

    if ( skipInvalidTransactionsFlag ) {
        REQUIRE_BLOCK_SIZE( 1, 0 );
    } else {
        REQUIRE_BLOCK_SIZE( 1, 1 );
        REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash );
    }

    REQUIRE_NONCE_INCREASE( senderAddress, 0 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 0 );
}

// Transaction should be IGNORED during execution or absent if skipInvalidTransactionsFlag
// Proposer should be penalized
// nonce too small
BOOST_DATA_TEST_CASE(
    transactionNonceSmall, skipInvalidTransactionsVariants, skipInvalidTransactionsFlag
    //, *boost::unit_test::precondition( dev::test::run_not_express )
) {
    SkaleHostFixture fixture(
        std::map< std::string, std::string >( { { "skipInvalidTransactionsPatchTimestamp",
            to_string( int( skipInvalidTransactionsFlag ) ) } } ) );
    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& accountHolder = fixture.accountHolder;
    auto& stub = fixture.stub;

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

    ConsensusExtFace::Transactions block1Txns;
    block1Txns.pushBackRegular( tx1.toBytes() );

    // create 1 txns in 1 block
    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( block1Txns,
#ifdef BITE
                           DecryptedTransactions{
#ifdef BITE
                                   std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                   std::make_shared< DecryptedRegularTxsMap >()
                               },
#endif
            utcTime(), 1U ) );

    // now our test txn
    json["value"] = jsToDecimal( toJS( 9000 * dev::eth::szabo ) );
    ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    ar = accountHolder->authenticate( ts );
    Transaction tx2( ts, ar.second );

    h256 txHash = tx2.sha3();

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    ConsensusExtFace::Transactions block2Txns;
    block2Txns.pushBackRegular( tx2.toBytes() );

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( block2Txns,
#ifdef BITE
                           DecryptedTransactions{
#ifdef BITE
                                   std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                   std::make_shared< DecryptedRegularTxsMap >()
                               },
#endif
            utcTime(), 2U ) );

    REQUIRE_BLOCK_INCREASE( 1 );

    if ( skipInvalidTransactionsFlag ) {
        REQUIRE_BLOCK_SIZE( 2, 0 );
    } else {
        REQUIRE_BLOCK_SIZE( 2, 1 );
        REQUIRE_BLOCK_TRANSACTION( 2, 0, txHash );
    }

    REQUIRE_NONCE_INCREASE( senderAddress, 0 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 0 );
}

// Transaction should be IGNORED during execution or absent if skipInvalidTransactionsFlag
// Proposer should be penalized
// not enough cash
BOOST_DATA_TEST_CASE(
    transactionBalanceBad, skipInvalidTransactionsVariants, skipInvalidTransactionsFlag ) {
    SkaleHostFixture fixture(
        std::map< std::string, std::string >( { { "skipInvalidTransactionsPatchTimestamp",
            to_string( int( skipInvalidTransactionsFlag ) ) } } ) );
    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& accountHolder = fixture.accountHolder;
    auto& stub = fixture.stub;

    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

#ifdef FAIR
    // block reward increased for FAIR to 5 ETH
    auto value = 6 * dev::eth::ether + dev::eth::wei;
#else
    auto value = 3 * dev::eth::ether + dev::eth::wei;
#endif

    Json::Value json;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );

    json["value"] = jsToDecimal( toJS( value ) );
    json["nonce"] = 0;

    TransactionSkeleton ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = accountHolder->authenticate( ts );
    Transaction tx( ts, ar.second );

    h256 txHash = tx.sha3();

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    ConsensusExtFace::Transactions block1Txns;
    block1Txns.pushBackRegular( tx.toBytes() );

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( block1Txns,
#ifdef BITE
                           DecryptedTransactions{
#ifdef BITE
                                   std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                   std::make_shared< DecryptedRegularTxsMap >()
                               },
#endif
            utcTime(), 1U ) );

    REQUIRE_BLOCK_INCREASE( 1 );

    if ( skipInvalidTransactionsFlag ) {
        REQUIRE_BLOCK_SIZE( 1, 0 );
    } else {
        REQUIRE_BLOCK_SIZE( 1, 1 );
        REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash );
    }

    REQUIRE_NONCE_INCREASE( senderAddress, 0 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 0 );

    // step 2: check that receipt "moved" to another block after successfull re-execution of the
    // same transaction

    if ( !skipInvalidTransactionsFlag ) {
        LocalisedTransactionReceipt r1 = client->localisedTransactionReceipt( txHash );
        BOOST_REQUIRE_EQUAL( r1.blockNumber(), 1 );
        BOOST_REQUIRE_EQUAL( r1.gasUsed(), 0 );
        LocalisedTransaction lt = client->localisedTransaction( txHash );
        BOOST_REQUIRE_EQUAL( lt.blockNumber(), 1 );
    }

    // make money
    dev::eth::simulateMining( *client, 1, senderAddress );

    ConsensusExtFace::Transactions block2Txns;
    block2Txns.pushBackRegular( tx.toBytes() );

    stub->createBlock( block2Txns,
#ifdef BITE
                       DecryptedTransactions{
#ifdef BITE
                               std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                               std::make_shared< DecryptedRegularTxsMap >()
                           },
#endif
        utcTime(), 2U );

    REQUIRE_BLOCK_SIZE( 2, 1 );
    REQUIRE_BLOCK_TRANSACTION( 2, 0, txHash );
    REQUIRE_NONCE_INCREASE( senderAddress, 1 );
    REQUIRE_BALANCE_DECREASE_GE( senderAddress, 1 );

    LocalisedTransactionReceipt r2 = client->localisedTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( r2.blockNumber(), 2 );
    BOOST_REQUIRE_GE( r2.gasUsed(), 21000 );
    LocalisedTransaction lt = client->localisedTransaction( txHash );
    BOOST_REQUIRE_EQUAL( lt.blockNumber(), 2 );
}

// Transaction should be IGNORED during execution or absent if skipInvalidTransactionsFlag
// Proposer should be penalized
// transaction goes beyond block gas limit
BOOST_DATA_TEST_CASE(
    transactionGasBlockLimitExceeded, skipInvalidTransactionsVariants, skipInvalidTransactionsFlag
    // , *boost::unit_test::precondition( dev::test::run_not_express )
) {
    SkaleHostFixture fixture(
        std::map< std::string, std::string >( { { "skipInvalidTransactionsPatchTimestamp",
            to_string( int( skipInvalidTransactionsFlag ) ) } } ) );
    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& stub = fixture.stub;

    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    // 1 txn with max gas
    Json::Value json;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );
    json["nonce"] = 0;
    json["gasPrice"] = 0;

    Transaction tx1 = fixture.tx_from_json( json );

    h256 txHash1 = tx1.sha3();

    // 2 txn
    json["value"] = jsToDecimal( toJS( 9000 * dev::eth::szabo ) );
    json["nonce"] = 1;
    json["gas"] = jsToDecimal( toJS( client->chainParams().getGasLimit() - 21000 + 1 ) );

    Transaction tx2 = fixture.tx_from_json( json );

    h256 txHash2 = tx2.sha3();

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    ConsensusExtFace::Transactions txns;
    txns.pushBackRegular( tx1.toBytes() );
    txns.pushBackRegular( tx2.toBytes() );

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( txns,
#ifdef BITE
                           DecryptedTransactions{
#ifdef BITE
                                   std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                   std::make_shared< DecryptedRegularTxsMap >()
                               },
#endif
            utcTime(), 1U ) );
    BOOST_REQUIRE_EQUAL( client->number(), 1 );

    REQUIRE_BLOCK_INCREASE( 1 );

    if ( skipInvalidTransactionsFlag ) {
        REQUIRE_BLOCK_SIZE( 1, 1 );

        REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash1 );
    } else {
        REQUIRE_BLOCK_SIZE( 1, 2 );

        REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash1 );
        REQUIRE_BLOCK_TRANSACTION( 1, 1, txHash2 );
    }

    REQUIRE_NONCE_INCREASE( senderAddress, 1 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 10000 * dev::eth::szabo );  // only 1st!
}

// Last transaction should be dropped from block proposal
BOOST_AUTO_TEST_CASE( gasLimitInBlockProposal ) {
    SkaleHostFixture fixture;
    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& skaleHost = fixture.skaleHost;
    auto& stub = fixture.stub;
    auto& account2 = fixture.account2;

    auto receiver = KeyPair::create();


    auto wr_state = client->state().createStateCopyAndClearCaches();
    wr_state.addBalance(
        fixture.account2.address(), client->chainParams().getGasLimit() * 1000 + dev::eth::ether );
    wr_state.commit();
    wr_state.getOriginalDb()->createBlockSnap( 2 );

    // 1 txn with max gas
    Json::Value json;
    json["from"] = toJS( coinbase.address() );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );
    json["nonce"] = 0;
    json["gasPrice"] = 1000;

    Transaction tx1 = fixture.tx_from_json( json );

    // 2 txn
    json["from"] = toJS( account2.address() );
    json["gas"] = jsToDecimal( toJS( client->chainParams().getGasLimit() - 21000 + 1 ) );

    Transaction tx2 = fixture.tx_from_json( json );

    // put already broadcasted txns
    skaleHost->receiveTransaction( toJS( tx1.toBytes() ) );
    skaleHost->receiveTransaction( toJS( tx2.toBytes() ) );

    sleep( 1 );  // allow broadcast thread to move them

    ConsensusExtFace::Transactions proposal = stub->pendingTransactions( 100 );

    BOOST_REQUIRE_EQUAL( proposal.size(), 1 );
    BOOST_REQUIRE( proposal.at( 0 ) == tx1.toBytes() );
}

// positive test for 4 next ones
BOOST_AUTO_TEST_CASE( transactionDropReceive
    //, *boost::unit_test::precondition( dev::test::run_not_express )
) {
    SkaleHostFixture fixture;
    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& skaleHost = fixture.skaleHost;
    auto& stub = fixture.stub;
    auto& tq = fixture.tq;

    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value json;
    u256 value1 = 10000 * dev::eth::szabo;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( value1 ) );
    json["nonce"] = 1;

    // 1st tx
    Transaction tx1 = fixture.tx_from_json( json );
#ifndef FAIR
    tx1.checkOutExternalGas(
        client->chainParams(), client->latestBlock().info().timestamp(), client->number() );
#endif

    // submit it!
    tq->import( tx1 );

    // 2nd tx
    u256 value2 = 20000 * dev::eth::szabo;
    json["value"] = jsToDecimal( toJS( value2 ) );
    json["nonce"] = 0;
    bytes tx2 = fixture.bytes_from_json( json );

    // receive it!
    skaleHost->receiveTransaction( toJS( tx2 ) );

    sleep( 1 );
    BOOST_REQUIRE_EQUAL( tq->knownTransactions().size(), 2 );

    // 3rd transaction to trigger re-verification
    u256 value3 = 30000 * dev::eth::szabo;
    json["value"] = jsToDecimal( toJS( value3 ) );
    json["nonce"] = 0;

    bytes tx3 = fixture.bytes_from_json( json );

    // return it from consensus!
    CHECK_BLOCK_BEGIN;
    CHECK_NONCE_BEGIN( senderAddress );

    ConsensusExtFace::Transactions txns;
    txns.pushBackRegular( tx3 );

    BOOST_REQUIRE_NO_THROW( stub->createBlock( txns,
#ifdef BITE
                                               DecryptedTransactions{
        #ifdef BITE
                                                       std::make_shared< DecryptedCTXTxsMap >(),
        #endif  // BITE
                                                       std::make_shared< DecryptedRegularTxsMap >()
                                                   },
#endif
        utcTime(), 1U ) );
    stub->setPriceForBlockId( 1, 1000 );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_NONCE_INCREASE( senderAddress, 1 );

    // both should be known, but
    BOOST_REQUIRE_EQUAL( tq->knownTransactions().size(), 2 );
    // 2nd should be dropped, 1st kept
    ConsensusExtFace::Transactions pendingTxns = stub->pendingTransactions( 3 );
    BOOST_REQUIRE_EQUAL( pendingTxns.size(), 1 );
}

BOOST_AUTO_TEST_CASE(
    transactionDropQueue, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    SkaleHostFixture fixture;
    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& stub = fixture.stub;
    auto& tq = fixture.tq;

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
    Transaction tx1 = fixture.tx_from_json( json );

#ifndef FAIR
    tx1.checkOutExternalGas(
        client->chainParams(), client->latestBlock().info().timestamp(), client->number() );
#endif

    // submit it!
    tq->import( tx1 );

    sleep( 1 );
    BOOST_REQUIRE_EQUAL( tq->knownTransactions().size(), 1 );

    // 2nd transaction will remove 1
    u256 value2 = 8000 * dev::eth::szabo;
    json["value"] = jsToDecimal( toJS( value2 ) );
    json["nonce"] = 0;

    Transaction tx2 = fixture.tx_from_json( json );

    h256 txHash2 = tx2.sha3();

    // return it from consensus!
    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    ConsensusExtFace::Transactions txns;
    txns.pushBackRegular( tx2.toBytes() );

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( txns,
#ifdef BITE
                           DecryptedTransactions{
#ifdef BITE
                                   std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                   std::make_shared< DecryptedRegularTxsMap >()
                               },
#endif
            utcTime(), 1U ) );
    stub->setPriceForBlockId( 1, 1000 );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash2 );

    REQUIRE_NONCE_INCREASE( senderAddress, 1 );
    REQUIRE_BALANCE_DECREASE( senderAddress, value2 );

    // should not be accessible from queue
    ConsensusExtFace::Transactions pendingTxns = stub->pendingTransactions( 1 );
    BOOST_REQUIRE_EQUAL( pendingTxns.size(), 0 );
}

// TODO Check exact dropping reason!
BOOST_AUTO_TEST_CASE( transactionDropByGasPrice
    // , *boost::unit_test::precondition( dev::test::run_not_express )
) {
    SkaleHostFixture fixture;
    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& stub = fixture.stub;
    auto& tq = fixture.tq;

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
    Transaction tx1 = fixture.tx_from_json( json );

#ifndef FAIR
    tx1.checkOutExternalGas(
        client->chainParams(), client->latestBlock().info().timestamp(), client->number() );
#endif

    // submit it!
    tq->import( tx1 );

    sleep( 1 );
    BOOST_REQUIRE_EQUAL( tq->knownTransactions().size(), 1 );

    // 2nd transaction will remove 1
    u256 value2 = 8000 * dev::eth::szabo;
    json["value"] = jsToDecimal( toJS( value2 ) );
    json["nonce"] = 0;

    Transaction tx2 = fixture.tx_from_json( json );

    h256 txHash2 = tx2.sha3();

    // return it from consensus!
    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    ConsensusExtFace::Transactions txns;
    txns.pushBackRegular( tx2.toBytes() );

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( txns,
#ifdef BITE
                           DecryptedTransactions{
#ifdef BITE
                                   std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                   std::make_shared< DecryptedRegularTxsMap >()
                               },
#endif
            utcTime(), 1U, 1000 ) );
    stub->setPriceForBlockId( 1, 1100 );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash2 );

    REQUIRE_NONCE_INCREASE( senderAddress, 1 );
    REQUIRE_BALANCE_DECREASE( senderAddress, value2 + 21000 * 1000 );

    // should not be accessible from queue
    ConsensusExtFace::Transactions pendingTxns = stub->pendingTransactions( 1 );
    BOOST_REQUIRE_EQUAL( pendingTxns.size(), 0 );
}

// TODO Check exact dropping reason!
BOOST_AUTO_TEST_CASE( transactionDropByGasPriceReceive
    // , *boost::unit_test::precondition( dev::test::run_not_express )
) {
    SkaleHostFixture fixture;
    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& skaleHost = fixture.skaleHost;
    auto& stub = fixture.stub;
    auto& tq = fixture.tq;
    auto& account2 = fixture.account2;

    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    {
        auto wr_state = client->state().createStateCopyAndClearCaches();
        wr_state.addBalance( fixture.account2.address(), 1 * ether );
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
    Transaction tx1 = fixture.tx_from_json( json );

#ifndef FAIR
    tx1.checkOutExternalGas(
        client->chainParams(), client->latestBlock().info().timestamp(), client->number() );
#endif

    // receive it!
    skaleHost->receiveTransaction( toJS( tx1.toBytes() ) );

    sleep( 1 );
    BOOST_REQUIRE_EQUAL( tq->knownTransactions().size(), 1 );

    // 2nd transaction will remove 1
    u256 value2 = 8000 * dev::eth::szabo;
    json["from"] = toJS( account2.address() );
    json["value"] = jsToDecimal( toJS( value2 ) );
    json["nonce"] = 0;

    Transaction tx2 = fixture.tx_from_json( json );

    h256 txHash2 = tx2.sha3();

    // return it from consensus!
    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    ConsensusExtFace::Transactions txns;
    txns.pushBackRegular( tx2.toBytes() );

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( txns,
#ifdef BITE
                           DecryptedTransactions{
#ifdef BITE
                                   std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                   std::make_shared< DecryptedRegularTxsMap >()
                               },
#endif
            utcTime(), 1U, 1000 ) );
    stub->setPriceForBlockId( 1, 1100 );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash2 );

    REQUIRE_NONCE_INCREASE( account2.address(), 1 );
    REQUIRE_BALANCE_DECREASE_GE( account2.address(), value2 );

    // should not be accessible from queue
    ConsensusExtFace::Transactions pendingTxns = stub->pendingTransactions( 1 );
    BOOST_REQUIRE_EQUAL( pendingTxns.size(), 0 );
}

BOOST_AUTO_TEST_CASE( transactionRace
    // , *boost::unit_test::precondition( dev::test::run_not_express )
) {
    SkaleHostFixture fixture;
    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& stub = fixture.stub;

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

    Transaction tx = fixture.tx_from_json( json );

    h256 txHash = tx.sha3();

    // 1 add tx as normal
    client->importTransaction( tx );

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    ConsensusExtFace::Transactions txns;
    txns.pushBackRegular( tx.toBytes() );

    // 2 get it from consensus
    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( txns,
#ifdef BITE
                           DecryptedTransactions{
#ifdef BITE
                                   std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                   std::make_shared< DecryptedRegularTxsMap >()
                               },
#endif
            utcTime(), 1U ) );
    stub->setPriceForBlockId( 1, 1000 );

    REQUIRE_BLOCK_INCREASE( 1 );
    REQUIRE_BLOCK_SIZE( 1, 1 );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, txHash );

    REQUIRE_NONCE_INCREASE( senderAddress, 1 );
    REQUIRE_BALANCE_DECREASE( senderAddress, value + gasPrice * 21000 );

    // 2 should be dropped from q
    ConsensusExtFace::Transactions tx_from_q = stub->pendingTransactions( 1 );
    BOOST_REQUIRE_EQUAL( tx_from_q.size(), 0 );

    // 3 send new tx and see nonce
    json["nonce"] = 1;
    Transaction tx2 = fixture.tx_from_json( json );

    client->importTransaction( tx2 );
}

// test two blocks with overlapping transactions :)
BOOST_AUTO_TEST_CASE( partialCatchUp
    // , *boost::unit_test::precondition( dev::test::run_not_express )
) {
    SkaleHostFixture fixture;
    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& accountHolder = fixture.accountHolder;
    auto& stub = fixture.stub;

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

    ConsensusExtFace::Transactions block1txns;
    block1txns.pushBackRegular( tx1.toBytes() );

    // create 1 txns in 1 block
    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( block1txns,
#ifdef BITE
                           DecryptedTransactions{
#ifdef BITE
                                   std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                   std::make_shared< DecryptedRegularTxsMap >()
                               },
#endif
            utcTime(), 1U ) );

    // now 2 txns
    json["value"] = jsToDecimal( toJS( 9000 * dev::eth::szabo ) );
    ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    ar = accountHolder->authenticate( ts );
    Transaction tx2( ts, ar.second );

#ifndef FAIR
    h256 txHash = tx2.sha3();
#endif

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BALANCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    ConsensusExtFace::Transactions block2Txns;
    block2Txns.pushBackRegular( tx1.toBytes() );
    block2Txns.pushBackRegular( tx2.toBytes() );

    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( block2Txns,
#ifdef BITE
                           DecryptedTransactions{
#ifdef BITE
                                   std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                   std::make_shared< DecryptedRegularTxsMap >()
                               },
#endif
            utcTime(), 2U ) );

    REQUIRE_BLOCK_INCREASE( 1 );
#ifndef FAIR
    REQUIRE_BLOCK_SIZE( 2, 2 );
    REQUIRE_BLOCK_TRANSACTION( 2, 1, txHash );
#else
    REQUIRE_BLOCK_SIZE( 2, 0 );
#endif

    REQUIRE_NONCE_INCREASE( senderAddress, 0 );
    REQUIRE_BALANCE_DECREASE( senderAddress, 0 );
}

BOOST_AUTO_TEST_CASE( getBlockRandom ) {
    SkaleHostFixture fixture;
    auto& skaleHost = fixture.skaleHost;

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "getBlockRandom" );
    auto res = exec( bytesConstRef(), { 1,
                                        1,
#ifdef BITE
                                        { 0 },
                                        dev::h256::random(),
                                        dev::ZeroAddress,
#endif
                                        true } );
    u256 blockRandom = skaleHost->getBlockRandom( 0, false );
    BOOST_REQUIRE( res.first );
    BOOST_REQUIRE( res.second == toBigEndian( static_cast< u256 >( blockRandom ) ) );
}

#ifndef FAIR
BOOST_AUTO_TEST_CASE( getCurrentBLSPublicKey ) {
    SkaleHostFixture fixture;
    auto& skaleHost = fixture.skaleHost;

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "getIMABLSPublicKey" );
    auto res = exec( bytesConstRef(), { 1,
                                        0,
#ifdef BITE
                                        { -1 },
                                        dev::h256::random(),
                                        dev::ZeroAddress,
#endif
                                        true } );
    std::array< std::string, 4 > imaBLSPublicKey = skaleHost->getCurrentBLSPublicKey();
    BOOST_REQUIRE( res.first );
    BOOST_REQUIRE( res.second == toBigEndian( dev::u256( imaBLSPublicKey[0] ) ) +
                                     toBigEndian( dev::u256( imaBLSPublicKey[1] ) ) +
                                     toBigEndian( dev::u256( imaBLSPublicKey[2] ) ) +
                                     toBigEndian( dev::u256( imaBLSPublicKey[3] ) ) );
}
#endif

#ifdef BITE

BOOST_AUTO_TEST_CASE( encryptionRandom_read_only_does_not_advance_counter ) {
    SkaleHostFixture fixture;
    bool isCalledFromTxn = true;
    fixture.skaleHost->resetEncryptionStateForBlock( 1 );

    // Read-only calls always use counter = 0.
    h256 readOnlyRandom1 = fixture.skaleHost->getEncryptionCallRandom( 1, !isCalledFromTxn );
    h256 readOnlyRandom2 = fixture.skaleHost->getEncryptionCallRandom( 1, !isCalledFromTxn );
    BOOST_REQUIRE( readOnlyRandom1 == readOnlyRandom2 );

    // Read-only calls must not advance non-read counter.
    h256 firstNonReadAfterReadOnly = fixture.skaleHost->getEncryptionCallRandom( 1, isCalledFromTxn );
    fixture.skaleHost->resetEncryptionStateForBlock( 1 );
    h256 firstNonReadFresh = fixture.skaleHost->getEncryptionCallRandom( 1, isCalledFromTxn );
    BOOST_REQUIRE( firstNonReadAfterReadOnly == firstNonReadFresh );
}

BOOST_AUTO_TEST_CASE( encryptionRandom_non_read_only_advances_counter ) {
    SkaleHostFixture fixture;
    bool isCalledFromTxn = true;
    fixture.skaleHost->resetEncryptionStateForBlock( 1 );

    h256 random1 = fixture.skaleHost->getEncryptionCallRandom( 1, isCalledFromTxn );
    h256 random2 = fixture.skaleHost->getEncryptionCallRandom( 1, isCalledFromTxn );
    h256 random3 = fixture.skaleHost->getEncryptionCallRandom( 1, isCalledFromTxn );

    BOOST_REQUIRE( random1 != random2 );
    BOOST_REQUIRE( random2 != random3 );

    // Reset must return counter back to zero for the block.
    fixture.skaleHost->resetEncryptionStateForBlock( 1 );
    h256 randomAfterReset = fixture.skaleHost->getEncryptionCallRandom( 1, isCalledFromTxn );
    BOOST_REQUIRE( randomAfterReset == random1 );
}

BOOST_AUTO_TEST_CASE( encryptionRandom_resets_on_commit ) {
    SkaleHostFixture fixture;
    bool isCalledFromTxn = true;
    fixture.skaleHost->resetEncryptionStateForBlock( 1 );

    h256 block1Counter0 = fixture.skaleHost->getEncryptionCallRandom( 1, isCalledFromTxn );
    h256 block1Counter1 = fixture.skaleHost->getEncryptionCallRandom( 1, isCalledFromTxn );
    BOOST_REQUIRE( block1Counter0 != block1Counter1 );

    // Simulate commit/new-block transition.
    fixture.skaleHost->resetEncryptionStateForBlock( 2 );
    h256 block2Counter0 = fixture.skaleHost->getEncryptionCallRandom( 2, isCalledFromTxn );

    // Repeating the same reset should reproduce the first value in the block.
    fixture.skaleHost->resetEncryptionStateForBlock( 2 );
    h256 block2Counter0Again = fixture.skaleHost->getEncryptionCallRandom( 2, isCalledFromTxn );
    BOOST_REQUIRE( block2Counter0 == block2Counter0Again );
    BOOST_REQUIRE( block2Counter0 != block1Counter1 );
}

BOOST_AUTO_TEST_CASE( encryptTE_success ) {
    SkaleHostFixture fixture;

    // TE helper from libBLS
    auto keys = generateKeys(1, 1);

    // set test key
    fixture.setBlsPublicKey(keys.commonPublic.getPublicKeyRaw().toStringArray(libBLS::Base::DEC));

    // Get the executor for encryptTE
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptTE" );

    // Create test input data
    std::string testMessage = "Hello, threshold encryption!";
    bytes dataToEncrypt( testMessage.begin(), testMessage.end() );

    // Create a test SC address (20 bytes)
    dev::Address testScAddress = dev::Address( "0x1234567890123456789012345678901234567890" );

    // Build ABI-encoded input: abi.encode(bytes data)
    // Format: [offset_to_data(32)] [data_length(32)] [data(N)]
    bytes input;

    // Offset to data = 32 (after offset field itself)
    bytes offsetData( 32, 0 );
    offsetData[31] = 32;
    input.insert( input.end(), offsetData.begin(), offsetData.end() );

    // data length
    bytes dataLenBytes( 32, 0 );
    dataLenBytes[31] = static_cast<uint8_t>( dataToEncrypt.size() );
    input.insert( input.end(), dataLenBytes.begin(), dataLenBytes.end() );

    // data (with ABI-compliant padding to 32-byte boundary)
    input.insert( input.end(), dataToEncrypt.begin(), dataToEncrypt.end() );
    size_t paddingNeeded = 32 - ( dataToEncrypt.size() % 32 );
    input.insert( input.end(), paddingNeeded, 0 );

    // Call the precompiled contract
    auto res = exec( bytesConstRef( input.data(), input.size() ),
        PrecompiledCallContext( 1, 0, dev::h256::random(), 0, testScAddress, true ) );

    // Verify success
    BOOST_REQUIRE( res.first );
    BOOST_REQUIRE( !res.second.empty() );

    // Parse output as RLP list [epochId, ciphertextBytes]
    RLP rlp( res.second );
    BOOST_REQUIRE( rlp.isList() );
    BOOST_REQUIRE( rlp.itemCount() == 2 );

    uint64_t epochId = rlp[0].toInt<uint64_t>();
    bytes ciphertextBytes = rlp[1].toBytes();

    BOOST_REQUIRE_EQUAL( epochId, fixture.client->getCurrentEpochId() );

    // Parse ciphertext component
    libBLS::Ciphertext ciphertext = libBLS::Ciphertext::fromBytes( ciphertextBytes, /* validate */ true );

    // Verify exactly 1 key is present
    BOOST_REQUIRE_EQUAL( ciphertext.getKeys().size(), 1 );

    // Validate the TE ciphertext with SC address as AAD
    auto ScBytes = testScAddress.asBytes();
    std::vector< uint8_t > aadBytes{ ScBytes.begin(), ScBytes.end() };
    BOOST_REQUIRE_NO_THROW(
        libBLS::ThresholdEncryption::validateEncryption( ciphertext.getTargetKey(), &aadBytes ) );

    // decrypt & check if decrypted = original
    
    // 1. Create a decryption share from the single private key share
    libBLS::TEDecryptionShare share = libBLS::ThresholdEncryption::partialDecrypt(
        ciphertext.getTargetKey(), keys.secretKeys[0] );
    // 2. Add to decrypt set
    libBLS::TEDecryptSet decryptSet( 1, 1 );  // t=1, n=1
    decryptSet.addDecryptShare( share );
    // 3. Combine shares → AES key
    libBLS::AES256Key aesKey = libBLS::ThresholdEncryption::combineShares( 
        ciphertext.getTargetKey(), decryptSet );
    // 4. Decrypt using AES key
    std::vector< uint8_t > decryptedMessage = 
        libBLS::ThresholdEncryption::decrypt( ciphertext, aesKey );
    // 5. Verify original message matches
    BOOST_REQUIRE( decryptedMessage == dataToEncrypt );    
}

BOOST_AUTO_TEST_CASE( encryptTE_same_data ) {
    SkaleHostFixture fixture;

    // TE helper from libBLS
    auto keys = generateKeys(1, 1);

    // set test key
    fixture.setBlsPublicKey(keys.commonPublic.getPublicKeyRaw().toStringArray(libBLS::Base::DEC));

    // Get the executor for encryptTE
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptTE" );

    // Create test input data
    std::string testMessage = "Deterministic test!";
    bytes dataToEncrypt( testMessage.begin(), testMessage.end() );

    // Build ABI-encoded input: abi.encode(bytes data)
    // Format: [offset_to_data(32)] [data_length(32)] [data(N)]
    bytes input;
    bytes offsetData( 32, 0 );
    offsetData[31] = 32;
    input.insert( input.end(), offsetData.begin(), offsetData.end() );
    bytes dataLenBytes( 32, 0 );
    dataLenBytes[31] = static_cast<uint8_t>( dataToEncrypt.size() );
    input.insert( input.end(), dataLenBytes.begin(), dataLenBytes.end() );
    input.insert( input.end(), dataToEncrypt.begin(), dataToEncrypt.end() );
    size_t paddingNeeded = 32 - ( dataToEncrypt.size() % 32 );
    input.insert( input.end(), paddingNeeded, 0 );

    // READ ONLY -----

    bool isReadOnly = true;
    // Call the precompiled contract twice - read only
    auto res1_ro = exec( bytesConstRef( input.data(), input.size() ),
        PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::Address(), isReadOnly ) );
    auto res2_ro = exec( bytesConstRef( input.data(), input.size() ),
        PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::Address(), isReadOnly ) );

    // Verify success
    BOOST_REQUIRE( res1_ro.first );
    BOOST_REQUIRE( res2_ro.first );
    // Results should be the same -> counter should not increase in read-only calls
    BOOST_REQUIRE( res1_ro.second == res2_ro.second );

    // NOT READ ONLY -----

    isReadOnly = false;
    // Call the precompiled contract twice - not read only
    // simulate block commit -> resets counter before any tx in block is executed
    fixture.skaleHost->resetEncryptionStateForBlock( 1 );
    auto res1 = exec( bytesConstRef( input.data(), input.size() ),
        PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::Address(), isReadOnly ) );
    auto res2 = exec( bytesConstRef( input.data(), input.size() ),
        PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::Address(), isReadOnly ) );

    // Verify success
    BOOST_REQUIRE( res1.first );
    BOOST_REQUIRE( res2.first );

    // Results should differ since encryption counter was increased 
    BOOST_REQUIRE( res1.second != res2.second );

    bytes ciphertextBytes1 = test::parseEpochedCiphertextBytes( res1.second, fixture.client->getCurrentEpochId() );
    bytes ciphertextBytes2 = test::parseEpochedCiphertextBytes( res2.second, fixture.client->getCurrentEpochId() );

    // Build the same public key list as precompiled contract
    std::vector< libBLS::TEPublicKey > publicKeys;
    auto blsPublicKeyArray = fixture.skaleHost->getCurrentBLSPublicKey();
    libBLS::algebra::G2Point publicKeyG2 =
        libBLS::algebra::G2Point::fromString( blsPublicKeyArray, libBLS::Base::DEC );
    publicKeys.emplace_back( publicKeyG2 );

    // Counter starts at 0 and increments per call in a block
    bool isFromTx = true;// we want to check the non-read-only case where counter increments
    bytes expectedCiphertext1 = buildDeterministicCiphertext( fixture.skaleHost->getReencryptionBlockRandom( 1, isFromTx ), 0, publicKeys, dataToEncrypt );
    bytes expectedCiphertext2 = buildDeterministicCiphertext( fixture.skaleHost->getReencryptionBlockRandom( 1, isFromTx ), 1, publicKeys, dataToEncrypt );

    BOOST_REQUIRE( ciphertextBytes1 == expectedCiphertext1 );
    BOOST_REQUIRE( ciphertextBytes2 == expectedCiphertext2 );
}

BOOST_AUTO_TEST_CASE( encryptTE_rotation_soon ) {
    SkaleHostFixture fixture;

    // Generate keys for group 0 and group 1
    auto keys0 = generateKeys(1, 1);
    auto keys1 = generateKeys(1, 1);

    // Set up two groups in chainParams to simulate rotation
    // NodeGroup struct: { nodes, finishTs, blsPublicKey }
    fixture.setNodeGroups({
        // 1000 is a palceholder here - will be substituted in lines below
        { {}, 1000, keys0.commonPublic.getPublicKeyRaw().toStringArray(libBLS::Base::DEC) },
        { {}, uint64_t(-1), keys1.commonPublic.getPublicKeyRaw().toStringArray(libBLS::Base::DEC) }
    });
    
    // We need to manipulate the block timestamp
    // SkaleHostFixture sets genesis timestamp to current time - 5.
    // Let's just adjust the first group's finish timestamp to be close to NOW.
    uint64_t now = std::time(nullptr);
    fixture.setGroupFinishTs(0, now + 10); // rotation in 10 seconds

    // Get the executor for encryptTE
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptTE" );

    // Create test input data
    std::string testMessage = "Rotation test!";
    bytes dataToEncrypt( testMessage.begin(), testMessage.end() );

    // Offset to data = 32 (after offset field itself)
    bytes input;
    bytes offsetData( 32, 0 );
    offsetData[31] = 32;
    input.insert( input.end(), offsetData.begin(), offsetData.end() );

    // data length
    bytes dataLenBytes( 32, 0 );
    dataLenBytes[31] = static_cast<uint8_t>( dataToEncrypt.size() );
    input.insert( input.end(), dataLenBytes.begin(), dataLenBytes.end() );

    // data (with ABI-compliant padding to 32-byte boundary)
    input.insert( input.end(), dataToEncrypt.begin(), dataToEncrypt.end() );
    size_t paddingNeeded = 32 - ( dataToEncrypt.size() % 32 );
    input.insert( input.end(), paddingNeeded, 0 );

    // Call the precompiled contract
    auto res = exec( bytesConstRef( input.data(), input.size() ),
        PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::Address(), true ) );

    // Verify success
    BOOST_REQUIRE( res.first );
    
    // Parse RLP
    RLP rlp( res.second );
    BOOST_REQUIRE( rlp.isList() );
    BOOST_REQUIRE_EQUAL( rlp.itemCount(), 2 );

    // check epoch id
    uint64_t epochId = rlp[0].toInt< uint64_t >();
    BOOST_REQUIRE_EQUAL( epochId, fixture.client->getCurrentEpochId() );

    bytes ciphertextBytes = rlp[1].toBytes();
    libBLS::Ciphertext ciphertext = libBLS::Ciphertext::fromBytes( ciphertextBytes );

    // Verify exactly 2 keys are present because rotation is soon
    BOOST_REQUIRE_EQUAL( ciphertext.getKeys().size(), 2 );

    // SC address used as AAD
    auto ScBytes = dev::Address().asBytes();
    std::vector< uint8_t > aadBytes{ ScBytes.begin(), ScBytes.end() };

    // Validate and decrypt with both keys
    // Key 0 (current)
    {
        libBLS::Ciphertext ctCopy = ciphertext;
        ctCopy.keepKey(0);
        
        // Validate the TE ciphertext with SC address as AAD
        BOOST_REQUIRE_NO_THROW(
            libBLS::ThresholdEncryption::validateEncryption( ctCopy.getTargetKey(), &aadBytes ) );
        
        libBLS::TEDecryptionShare share = libBLS::ThresholdEncryption::partialDecrypt(
            ctCopy.getTargetKey(), keys0.secretKeys[0] );
        libBLS::TEDecryptSet decryptSet( 1, 1 );
        decryptSet.addDecryptShare( share );
        libBLS::AES256Key aesKey = libBLS::ThresholdEncryption::combineShares( 
            ctCopy.getTargetKey(), decryptSet );
        std::vector< uint8_t > decryptedMessage = 
            libBLS::ThresholdEncryption::decrypt( ctCopy, aesKey );
        BOOST_REQUIRE( decryptedMessage == dataToEncrypt );
    }

    // Key 1 (next)
    {
        libBLS::Ciphertext ctCopy = ciphertext;
        ctCopy.keepKey(1);
        
        // Validate the TE ciphertext with SC address as AAD
        BOOST_REQUIRE_NO_THROW(
            libBLS::ThresholdEncryption::validateEncryption( ctCopy.getTargetKey(), &aadBytes ) );
        
        libBLS::TEDecryptionShare share = libBLS::ThresholdEncryption::partialDecrypt(
            ctCopy.getTargetKey(), keys1.secretKeys[0] );
        libBLS::TEDecryptSet decryptSet( 1, 1 );
        decryptSet.addDecryptShare( share );
        libBLS::AES256Key aesKey = libBLS::ThresholdEncryption::combineShares( 
            ctCopy.getTargetKey(), decryptSet );
        std::vector< uint8_t > decryptedMessage = 
            libBLS::ThresholdEncryption::decrypt( ctCopy, aesKey );
        BOOST_REQUIRE( decryptedMessage == dataToEncrypt );
    }
}

BOOST_AUTO_TEST_CASE( encryptTE_inputTooLarge ) {
    SkaleHostFixture fixture;

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptTE" );

    // Create input larger than 64KB
    bytes largeInput( 65 * 1024, 0x42 );  // 65KB of 'B's
    auto res = exec( bytesConstRef( largeInput.data(), largeInput.size() ),
        PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::ZeroAddress, true ) );

    // Verify failure with error code 1 (input too large)
    BOOST_REQUIRE( !res.first );
    BOOST_REQUIRE( res.second == toBigEndian( dev::u256( 1 ) ) );
}

BOOST_AUTO_TEST_CASE( encryptTE_inputTooSmall ) {
    SkaleHostFixture fixture;

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptTE" );

    // Call with input smaller than minimum (64 bytes for ABI format)
    bytes smallInput( 63, 0x42 );
    auto res = exec( bytesConstRef( smallInput.data(), smallInput.size() ),
        PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::ZeroAddress, true ) );

    // Verify failure with error code 2 (input too small)
    BOOST_REQUIRE( !res.first );
    BOOST_REQUIRE( res.second == toBigEndian( dev::u256( 2 ) ) );
}

BOOST_AUTO_TEST_CASE( encryptTE_inputNotAligned ) {
    SkaleHostFixture fixture;

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptTE" );

    // Build input that is not a multiple of 32 bytes (65 bytes)
    bytes input( 65, 0 );

    auto res = exec( bytesConstRef( input.data(), input.size() ), PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::ZeroAddress, true ) );

    // Verify failure with error code 3 (input not 32-byte aligned)
    BOOST_REQUIRE( !res.first );
    BOOST_REQUIRE( res.second == toBigEndian( dev::u256( 3 ) ) );
}

BOOST_AUTO_TEST_CASE( encryptTE_invalidABIEncoding ) {
    SkaleHostFixture fixture;

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptTE" );

    // Build input with wrong data offset (should be 32, we set 64)
    bytes input( 64, 0 );
    input[31] = 64;  // Wrong data offset (should be 32)

    auto res = exec( bytesConstRef( input.data(), input.size() ), PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::ZeroAddress, true ) );

    // Verify failure with error code 5 (invalid data offset)
    BOOST_REQUIRE( !res.first );
    BOOST_REQUIRE( res.second == toBigEndian( dev::u256( 5 ) ) );
}

BOOST_AUTO_TEST_CASE( encryptTE_dataLengthMismatch ) {
    SkaleHostFixture fixture;

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptTE" );

    // Build valid ABI structure but claim more data than available
    bytes input( 96, 0 );
    // offset (0-31): 32
    input[31] = 32;
    // data_length (32-63): claim 100 bytes, but only 32 bytes total remain
    input[63] = 100;
    // actual data (64-95): only 32 bytes of zeros

    auto res = exec( bytesConstRef( input.data(), input.size() ), PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::ZeroAddress, true ) );

    // Verify failure with error code 6 (data length mismatch)
    BOOST_REQUIRE( !res.first );
    BOOST_REQUIRE( res.second == toBigEndian( dev::u256( 6 ) ) );
}

BOOST_AUTO_TEST_CASE( encryptTE_trailingPaddingNotZeros ) {
    SkaleHostFixture fixture;

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptTE" );

    // Build valid ABI structure with 1 byte of data, but non-zero trailing padding
    bytes input( 96, 0 );
    // offset (0-31): 32
    input[31] = 32;
    // data_length (32-63): 1 byte
    input[63] = 1;
    // actual data (64): one byte of data
    input[64] = 0xAB;
    // trailing padding (65-95): should be zeros but we set one to non-zero
    input[95] = 0xFF;

    auto res = exec( bytesConstRef( input.data(), input.size() ), PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::ZeroAddress, true ) );

    // Verify failure with error code 7 (trailing padding not zeros)
    BOOST_REQUIRE( !res.first );
    BOOST_REQUIRE( res.second == toBigEndian( dev::u256( 7 ) ) );
}

BOOST_AUTO_TEST_CASE( encryptTE_counter_reset_on_new_block ) {
    SkaleHostFixture fixture;

    // TE helper from libBLS
    auto keys = generateKeys(1, 1);

    // set test key
    fixture.setBlsPublicKey(keys.commonPublic.getPublicKeyRaw().toStringArray(libBLS::Base::DEC));

    // Get the executor for encryptTE
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptTE" );

    // Create test input data
    std::string testMessage = "Counter reset test!";
    bytes dataToEncrypt( testMessage.begin(), testMessage.end() );

    // Build ABI-encoded input
    bytes input;
    bytes offsetData( 32, 0 );
    offsetData[31] = 32;
    input.insert( input.end(), offsetData.begin(), offsetData.end() );
    bytes dataLenBytes( 32, 0 );
    dataLenBytes[31] = static_cast<uint8_t>( dataToEncrypt.size() );
    input.insert( input.end(), dataLenBytes.begin(), dataLenBytes.end() );
    input.insert( input.end(), dataToEncrypt.begin(), dataToEncrypt.end() );
    size_t paddingNeeded = 32 - ( dataToEncrypt.size() % 32 );
    input.insert( input.end(), paddingNeeded, 0 );

    // Call the precompiled contract in block 1 context (simulating transaction execution)
    // simulate block 1 has been comitted
    fixture.skaleHost->resetEncryptionStateForBlock( 1 );
    auto res1 = exec( bytesConstRef( input.data(), input.size() ),
        PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::Address(), false ) );

    BOOST_REQUIRE( res1.first );

    // Call the precompiled contract in block 2 context (simulating transaction execution)
    fixture.skaleHost->resetEncryptionStateForBlock( 2 );
    auto res2 = exec( bytesConstRef( input.data(), input.size() ),
        PrecompiledCallContext( 2, 0, dev::h256::random(), 0, dev::Address(), false ) );

    BOOST_REQUIRE( res2.first );

    // Results should still be different - blockRandom differs
    BOOST_REQUIRE( res1.second != res2.second );

    // compute the ciphertexts manually using counter 0 for both blocks
    bytes ciphertextBytes1 = test::parseEpochedCiphertextBytes( res1.second, fixture.client->getCurrentEpochId() );
    bytes ciphertextBytes2 = test::parseEpochedCiphertextBytes( res2.second, fixture.client->getCurrentEpochId() );

    // Build the same public key list as precompiled contract
    std::vector< libBLS::TEPublicKey > publicKeys;
    auto blsPublicKeyArray = fixture.skaleHost->getCurrentBLSPublicKey();
    libBLS::algebra::G2Point publicKeyG2 =
        libBLS::algebra::G2Point::fromString( blsPublicKeyArray, libBLS::Base::DEC );
    publicKeys.emplace_back( publicKeyG2 );

    // Counter resets to 0 on each new block
    // Block 1: counter=0, blockRandom for block 1
    bytes expectedCiphertext1 = test::buildDeterministicCiphertext( 
        fixture.skaleHost->getReencryptionBlockRandom( 1, true ), 0, publicKeys, dataToEncrypt );

    // Block 2: counter=0 (reset!), blockRandom for block 2
    bytes expectedCiphertext2 = test::buildDeterministicCiphertext( 
        fixture.skaleHost->getReencryptionBlockRandom( 2, true ), 0, publicKeys, dataToEncrypt );
    // Verify both match expected (proving counter was reset to 0 in block 2)
    BOOST_REQUIRE( ciphertextBytes1 == expectedCiphertext1 );
    BOOST_REQUIRE( ciphertextBytes2 == expectedCiphertext2 );
}

BOOST_AUTO_TEST_CASE( encryptECIES_success ) {
    SkaleHostFixture fixture;

    // Generate a user keypair
    dev::KeyPair userKeys = dev::KeyPair::create();
    dev::Public userPublicKey = userKeys.pub();
    dev::Secret userPrivateKey = userKeys.secret();

    // Extract x and y coordinates from public key (64 bytes total)
    bytes pubKeyX( userPublicKey.data(), userPublicKey.data() + 32 );
    bytes pubKeyY( userPublicKey.data() + 32, userPublicKey.data() + 64 );

    // Create test data to encrypt
    std::string testMessage = "Hello, ECIES encryption!";
    bytes dataToEncrypt( testMessage.begin(), testMessage.end() );

    // Build ABI-encoded input: [offset_to_data(32)] [x(32)] [y(32)] [data_length(32)] [data(N)]
    bytes input;
    // Offset to data = 96 (0x60) = 3 * 32
    input.insert( input.end(), 32, 0 );
    input[31] = 96;
    // x-coordinate (32 bytes)
    input.insert( input.end(), pubKeyX.begin(), pubKeyX.end() );
    // y-coordinate (32 bytes)
    input.insert( input.end(), pubKeyY.begin(), pubKeyY.end() );
    // Data length (32 bytes)
    bytes lenBytes( 32, 0 );
    lenBytes[31] = static_cast<uint8_t>( dataToEncrypt.size() );
    input.insert( input.end(), lenBytes.begin(), lenBytes.end() );
    // Data (with ABI-compliant padding to 32-byte boundary)
    input.insert( input.end(), dataToEncrypt.begin(), dataToEncrypt.end() );
    size_t paddingNeeded = 32 - ( dataToEncrypt.size() % 32 );
    input.insert( input.end(), paddingNeeded, 0 );

    // Get the executor for encryptECIES
    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptECIES" );

    // Call the precompiled contract
    auto res = exec( bytesConstRef( input.data(), input.size() ), PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::ZeroAddress, true ) );

    // Verify success
    BOOST_REQUIRE( res.first );
    BOOST_REQUIRE( !res.second.empty() );

    // Output format: [IV (16 bytes)] [Ephemeral Public Key (33 bytes)] [Ciphertext (N bytes)]
    BOOST_REQUIRE( res.second.size() >= 16 + 33 + 16 );  // IV + pubkey + min ciphertext

    // Parse output
    bytes iv( res.second.begin(), res.second.begin() + 16 );
    bytes ephemeralPubKeyCompressed( res.second.begin() + 16, res.second.begin() + 16 + 33 );

    // Verify ephemeral public key prefix (0x02 or 0x03 for compressed format)
    BOOST_REQUIRE( ephemeralPubKeyCompressed[0] == 0x02 || ephemeralPubKeyCompressed[0] == 0x03 );

    auto decryptedBytes = dev::decryptECIES_CBC( userPrivateKey, &res.second );

    // 7. Verify decrypted matches original
    BOOST_REQUIRE( decryptedBytes == dataToEncrypt );
}

BOOST_AUTO_TEST_CASE( encryptECIES_deterministic ) {
    SkaleHostFixture fixture;

    // Generate a user keypair
    dev::KeyPair userKeys = dev::KeyPair::create();
    dev::Public userPublicKey = userKeys.pub();

    // Extract x and y coordinates from public key
    bytes pubKeyX( userPublicKey.data(), userPublicKey.data() + 32 );
    bytes pubKeyY( userPublicKey.data() + 32, userPublicKey.data() + 64 );

    // Create test data to encrypt
    std::string testMessage = "Deterministic test";
    bytes dataToEncrypt( testMessage.begin(), testMessage.end() );

    // Build ABI-encoded input
    bytes input;
    input.insert( input.end(), 32, 0 );
    input[31] = 96;  // offset
    input.insert( input.end(), pubKeyX.begin(), pubKeyX.end() );
    input.insert( input.end(), pubKeyY.begin(), pubKeyY.end() );
    bytes lenBytes( 32, 0 );
    lenBytes[31] = static_cast<uint8_t>( dataToEncrypt.size() );
    input.insert( input.end(), lenBytes.begin(), lenBytes.end() );
    input.insert( input.end(), dataToEncrypt.begin(), dataToEncrypt.end() );
    size_t paddingNeeded = 32 - ( dataToEncrypt.size() % 32 );
    input.insert( input.end(), paddingNeeded, 0 );

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptECIES" );

    // Call the precompiled contract twice in the same context (same block random)
    // not read only -> will increase counter
    bool isReadOnly = false;
    auto res1 = exec( bytesConstRef( input.data(), input.size() ),
        PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::ZeroAddress, isReadOnly ) );
    auto res2 = exec( bytesConstRef( input.data(), input.size() ),
        PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::ZeroAddress, isReadOnly ) );
    BOOST_REQUIRE( res1.first );
    BOOST_REQUIRE( res2.first );

    // Results should differ since encryption counter was increased
    BOOST_REQUIRE( res1.second != res2.second );

    // Compute deterministic seed: Hash(blockRandom || counter)
    auto buildSeed = [&]( uint64_t counter ) {
        // use block 0, since no commits have been made yet
        bytes blockRandomBytes = toBigEndian( fixture.skaleHost->getBlockRandom( 0, isReadOnly ) );
        bytes counterBytes = toBigEndian( dev::u256( counter ) );
        bytes combined;
        combined.insert( combined.end(), blockRandomBytes.begin(), blockRandomBytes.end() );
        combined.insert( combined.end(), counterBytes.begin(), counterBytes.end() );
        return dev::sha3( combined );
    };

    auto computeCiphertext = [&]( uint64_t counter ) {
        dev::h256 seed = buildSeed( counter );
        return dev::encryptECIES_CBC( userPublicKey, bytesConstRef( &dataToEncrypt ), &seed );
    };

    bytes expectedCiphertext1 = computeCiphertext( 0 );
    bytes expectedCiphertext2 = computeCiphertext( 1 );

    BOOST_REQUIRE( res1.second == expectedCiphertext1 );
    BOOST_REQUIRE( res2.second == expectedCiphertext2 );
}

BOOST_AUTO_TEST_CASE( encryptECIES_inputTooLarge ) {
    SkaleHostFixture fixture;

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptECIES" );

    // Input larger than 64KB
    bytes largeInput( 65 * 1024, 0x42 );
    auto res = exec( bytesConstRef( largeInput.data(), largeInput.size() ), PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::ZeroAddress, true ) );

    // Verify failure with error code 1 (input too large)
    BOOST_REQUIRE( !res.first );
    BOOST_REQUIRE( res.second == toBigEndian( dev::u256( 1 ) ) );
}

BOOST_AUTO_TEST_CASE( encryptECIES_inputTooSmall ) {
    SkaleHostFixture fixture;

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptECIES" );

    // Input smaller than 128 bytes
    bytes smallInput( 64, 0x42 );
    auto res = exec( bytesConstRef( smallInput.data(), smallInput.size() ), PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::ZeroAddress, true ) );

    // Verify failure with error code 2 (input too small)
    BOOST_REQUIRE( !res.first );
    BOOST_REQUIRE( res.second == toBigEndian( dev::u256( 2 ) ) );
}

BOOST_AUTO_TEST_CASE( encryptECIES_inputNotAligned ) {
    SkaleHostFixture fixture;

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptECIES" );

    // Build input that is not a multiple of 32 bytes (129 bytes)
    bytes input( 129, 0 );

    auto res = exec( bytesConstRef( input.data(), input.size() ), PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::ZeroAddress, true ) );

    // Verify failure with error code 3 (input not 32-byte aligned)
    BOOST_REQUIRE( !res.first );
    BOOST_REQUIRE( res.second == toBigEndian( dev::u256( 3 ) ) );
}

BOOST_AUTO_TEST_CASE( encryptECIES_invalidABIOffset ) {
    SkaleHostFixture fixture;

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptECIES" );

    // Build input with wrong data offset (should be 96, we set 64)
    bytes input( 128, 0 );
    input[31] = 64;  // Wrong offset (should be 96)

    auto res = exec( bytesConstRef( input.data(), input.size() ), PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::ZeroAddress, true ) );

    // Verify failure with error code 4 (invalid data offset)
    BOOST_REQUIRE( !res.first );
    BOOST_REQUIRE( res.second == toBigEndian( dev::u256( 4 ) ) );
}

BOOST_AUTO_TEST_CASE( encryptECIES_dataLengthMismatch ) {
    SkaleHostFixture fixture;

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptECIES" );

    // Build input where data_length claims more data than actually present
    bytes input( 128, 0 );
    input[31] = 96;   // Correct offset
    input[127] = 100; // Claim 100 bytes of data, but none actually present

    auto res = exec( bytesConstRef( input.data(), input.size() ), PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::ZeroAddress, true ) );

    // Verify failure with error code 5 (data length mismatch)
    BOOST_REQUIRE( !res.first );
    BOOST_REQUIRE( res.second == toBigEndian( dev::u256( 5 ) ) );
}

BOOST_AUTO_TEST_CASE( encryptECIES_emptyData ) {
    SkaleHostFixture fixture;

    // Generate a valid user keypair
    dev::KeyPair userKeys = dev::KeyPair::create();
    dev::Public userPublicKey = userKeys.pub();
    dev::Secret userPrivateKey = userKeys.secret();

    // Extract x and y coordinates from public key (64 bytes total)
    bytes pubKeyX( userPublicKey.data(), userPublicKey.data() + 32 );
    bytes pubKeyY( userPublicKey.data() + 32, userPublicKey.data() + 64 );

    // Build ABI-encoded input with EMPTY data
    // Format: [offset_to_data(32)] [x(32)] [y(32)] [data_length(32)] [no data]
    bytes input;
    // Offset to data = 96 (0x60) = 3 * 32
    input.insert( input.end(), 32, 0 );
    input[31] = 96;
    // x-coordinate (32 bytes)
    input.insert( input.end(), pubKeyX.begin(), pubKeyX.end() );
    // y-coordinate (32 bytes)
    input.insert( input.end(), pubKeyY.begin(), pubKeyY.end() );
    // Data length = 0 (32 bytes of zeros)
    input.insert( input.end(), 32, 0 );

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptECIES" );

    // Call the precompiled contract
    auto res = exec( bytesConstRef( input.data(), input.size() ), PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::ZeroAddress, true ) );

    // Verify success - empty data encryption should succeed
    BOOST_REQUIRE( res.first );
    BOOST_REQUIRE( !res.second.empty() );

    // Verify we can decrypt back to empty data
    auto decryptedBytes = dev::decryptECIES_CBC( userPrivateKey, &res.second );
    BOOST_REQUIRE( decryptedBytes.empty() );
}

BOOST_AUTO_TEST_CASE( encryptECIES_trailingPaddingNotZeros ) {
    SkaleHostFixture fixture;

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptECIES" );

    // Build valid ABI structure with 1 byte of data, but non-zero trailing padding
    bytes input( 192, 0 );
    // offset (0-31): 96
    input[31] = 96;
    // pubKeyX (32-63): zeros (valid x coordinate check happens later)
    // pubKeyY (64-95): zeros
    // data_length (96-127): 1 byte
    input[127] = 1;
    // actual data (128): one byte of data
    input[128] = 0xAB;
    // trailing padding (129-191): should be zeros but we set one to non-zero
    input[150] = 0xFF;

    auto res = exec( bytesConstRef( input.data(), input.size() ), PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::ZeroAddress, true ) );

    // Verify failure with error code 6 (trailing padding not zeros)
    BOOST_REQUIRE( !res.first );
    BOOST_REQUIRE( res.second == toBigEndian( dev::u256( 6 ) ) );
}

BOOST_AUTO_TEST_CASE( encryptECIES_invalidPublicKey ) {
    SkaleHostFixture fixture;

    // Create invalid public key (not on curve)
    bytes invalidPubKeyX( 32, 0xFF );
    bytes invalidPubKeyY( 32, 0xFF );

    std::string testMessage = "Test";
    bytes dataToEncrypt( testMessage.begin(), testMessage.end() );

    // Build ABI-encoded input
    bytes input;
    input.insert( input.end(), 32, 0 );
    input[31] = 96;  // offset
    input.insert( input.end(), invalidPubKeyX.begin(), invalidPubKeyX.end() );
    input.insert( input.end(), invalidPubKeyY.begin(), invalidPubKeyY.end() );
    bytes lenBytes( 32, 0 );
    lenBytes[31] = static_cast<uint8_t>( dataToEncrypt.size() );
    input.insert( input.end(), lenBytes.begin(), lenBytes.end() );
    input.insert( input.end(), dataToEncrypt.begin(), dataToEncrypt.end() );
    // Add ABI-compliant padding to 32-byte boundary
    size_t paddingNeeded = 32 - ( dataToEncrypt.size() % 32 );
    input.insert( input.end(), paddingNeeded, 0 );

    PrecompiledExecutor exec = PrecompiledRegistrar::executor( "encryptECIES" );
    auto res = exec( bytesConstRef( input.data(), input.size() ), PrecompiledCallContext( 1, 0, dev::h256::random(), 0, dev::ZeroAddress, true ) );

    // Verify failure with error code 7 (invalid public key)
    BOOST_REQUIRE( !res.first );
    BOOST_REQUIRE( res.second == toBigEndian( dev::u256( 7 ) ) );
}
#endif  // BITE

#ifdef BITE
BOOST_AUTO_TEST_CASE( biteTransactions ) {
    SkaleHostFixture fixture;

    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& accountHolder = fixture.accountHolder;
    auto& skaleHost = fixture.skaleHost;
    auto& stub = fixture.stub;

    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value json;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );
    json["nonce"] = 0;

    std::string dataToEncrypt = dev::h256::random().hex();
    json["data"] = std::string( "0x" ) + dataToEncrypt;

    TransactionSkeleton ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = accountHolder->authenticate( ts );
    Transaction txOriginal( ts, ar.second );

    auto messageToEncrypt = libBLS::ThresholdUtils::hexCStringToBytes( dataToEncrypt.c_str() );
    auto publicKey = libBLS::TEPublicKey::random();
    auto ciphertext = libBLS::ThresholdEncryption::encrypt( messageToEncrypt, publicKey );

    json["data"] =
        std::string( "0x" ) + libBLS::ThresholdUtils::bytesToHexString( ciphertext.toBytes() );

    ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    ar = accountHolder->authenticate( ts );
    Transaction txEncrypted( ts, ar.second );

    h256 encryptedTxHash = txEncrypted.sha3();

    shared_ptr< vector< uint8_t > > originalDataBytesPtr =
        std::make_shared< vector< uint8_t > >( messageToEncrypt );

    DecryptedRegularTxFields txFields{ messageToEncrypt, receiver.address().asArray() };
    std::shared_ptr< DecryptedRegularTxsMap > regularTxsMap =
        std::make_shared< DecryptedRegularTxsMap >(
            DecryptedRegularTxsMap{ { 0, std::make_optional( txFields ) } }
        );

    DecryptedTransactions decryptedTxnDataMap(
#ifdef BITE
                std::make_shared< DecryptedCTXTxsMap >(),
#endif
                regularTxsMap
                );

    ConsensusExtFace::Transactions txns;
    txns.pushBackRegular( txEncrypted.toBytes() );

    // simulate new block
    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( txns, decryptedTxnDataMap, utcTime(), 1U ) );

    BOOST_REQUIRE( client->transaction( encryptedTxHash ).toBytes() == txEncrypted.toBytes() );
    BOOST_REQUIRE(
        client->decryptedTransactionData( encryptedTxHash ).data() == txOriginal.data() );
    BOOST_REQUIRE( client->decryptedTransactionData( encryptedTxHash ).to() == txOriginal.to() );
}
#endif

#ifdef FAIR
BOOST_AUTO_TEST_CASE(syncNodeGroupsUpdatesEpochIdWithoutRotation) {
    SkaleHostFixture fixture( {}, true );

    auto& client = fixture.client;
    auto& stub = fixture.stub;

    uint64_t currentTimestamp = static_cast< uint64_t >( utcTime() );

    fixture.overwriteHistoricNodeGroups( {
        {
         {
          GroupNode{ u256( 0 ), u256( 8 ),
            "0xf925c203a30ec6cad5a263db3efab7ed4c1fd74c8688167e10a5a22e15ab5018d8553df0ac54ea"
            "10"
            "5a3d21845e5660bc3d4e7c82e7af1daa3baad393b1521467",
            Address( "0x08151B8F80bfa7dEa760e461412AF24348224edf" )
         }
        },
        currentTimestamp,
        {
            "15959969554621958245201075983340071881770733084910870228938077786643587385029",
            "7970122607051572307517094692346020360016825923464107614135327251488152616550",
            "3371162264373897025322009434717052197952692496405149486989861571246537813591",
            "13678625751515504401110635369790787716744686498431213713911601759809559919693" }
        },
        {
        {
             GroupNode{ u256( 0 ), u256( 8 ),
                 "0xf925c203a30ec6cad5a263db3efab7ed4c1fd74c8688167e10a5a22e15ab5018d8553df0ac54ea"
                 "10"
                 "5a3d21845e5660bc3d4e7c82e7af1daa3baad393b1521467",
                 Address( "0x08151B8F80bfa7dEa760e461412AF24348224edf" )
             }
         },
         std::numeric_limits<uint64_t>::max(), {
                "3842742177969966091367527274107524613106077736353521259727282251005583743182",
                "3497912824016228906558906422247670474553186446469877598411863912329082553081",
                "8173996886448941320370434854289578123609627835954133538412363037981850950343",
                "20979370720689475348670582375026949105497642726992863932315517524004804784155"
        }
        }
    });

    BOOST_REQUIRE_EQUAL( client->getCurrentEpochId(), 0 );

    uint64_t blockTimestamp = currentTimestamp + 1;
    uint64_t blockId = client->number() + 1;

    BOOST_REQUIRE_NO_THROW( stub->createBlock(
        ConsensusExtFace::Transactions{},
#ifdef BITE
                                DecryptedTransactions{
#ifdef BITE
                                        std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                        std::make_shared< DecryptedRegularTxsMap >()
                                    },
#endif
        blockTimestamp, blockId ) );

    BOOST_REQUIRE_EQUAL( client->getCurrentEpochId(), 1 );
}
#endif

struct dummy {};

// Test behavior of MTM if tx with big nonce was already mined as erroneous
BOOST_FIXTURE_TEST_CASE(
    mtmAfterBigNonceMined, dummy, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    SkaleHostFixture fixture(
        std::map< std::string, std::string >( { { "multiTransactionMode", "1" } } ) );

    auto& client = fixture.client;
    auto& coinbase = fixture.coinbase;
    auto& accountHolder = fixture.accountHolder;
    auto& skaleHost = fixture.skaleHost;
    auto& stub = fixture.stub;

    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    // 1 tx nonce = 1
    Json::Value json;
    json["from"] = toJS( senderAddress );
    json["to"] = toJS( receiver.address() );
    json["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );

    // future nonce
    json["nonce"] = 1;

    TransactionSkeleton ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = accountHolder->authenticate( ts );
    Transaction tx1( ts, ar.second );

    h256 tx1Hash = tx1.sha3();

    // it will be put to "future" queue
    skaleHost->receiveTransaction( toJS( tx1.toBytes() ) );
    sleep( 1 );
    ConsensusExtFace::Transactions proposal = stub->pendingTransactions( 100 );
    // and not proposed
    BOOST_REQUIRE_EQUAL( proposal.size(), 0 );

    CHECK_NONCE_BEGIN( senderAddress );
    CHECK_BLOCK_BEGIN;

    ConsensusExtFace::Transactions block1Txns;
    block1Txns.pushBackRegular( tx1.toBytes() );

    // simulate it coming from another node
    BOOST_REQUIRE_NO_THROW(
        stub->createBlock( block1Txns,
#ifdef BITE
                           DecryptedTransactions{
#ifdef BITE
                                   std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                   std::make_shared< DecryptedRegularTxsMap >()
                               },
#endif
            utcTime(), 1U ) );

#ifndef FAIR
    REQUIRE_BLOCK_SIZE( 1, 1 );
    REQUIRE_BLOCK_TRANSACTION( 1, 0, tx1Hash );
#else
    ( void ) tx1Hash;
    REQUIRE_BLOCK_SIZE( 1, 0 );
#endif

    // 2 tx nonce = 0
    json["value"] = jsToDecimal( toJS( 9000 * dev::eth::szabo ) );
    json["nonce"] = 0;
    ts = toTransactionSkeleton( json );
    ts = client->populateTransactionWithDefaults( ts );
    ar = accountHolder->authenticate( ts );
    Transaction tx2( ts, ar.second );

    h256 tx2Hash = tx2.sha3();

    // post it to queue for "realism"
    skaleHost->receiveTransaction( toJS( tx2.toBytes() ) );
    sleep( 1 );
    proposal = stub->pendingTransactions( 100 );
    BOOST_REQUIRE_EQUAL( proposal.size(), 2 );

    ConsensusExtFace::Transactions block2Txns;
    block2Txns.pushBackRegular( proposal.at( 0 ) );

    BOOST_REQUIRE_NO_THROW( stub->createBlock( block2Txns,
#ifdef BITE
                                               DecryptedTransactions{
#ifdef BITE
                                                       std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                                       std::make_shared< DecryptedRegularTxsMap >()
                                                   },
#endif
        utcTime(), 2U ) );

    REQUIRE_BLOCK_INCREASE( 2 );
    REQUIRE_BLOCK_SIZE( 2, 1 );
    REQUIRE_BLOCK_TRANSACTION( 2, 0, tx2Hash );

    REQUIRE_NONCE_INCREASE( senderAddress, 1 );

    // 3 submit nonce = 1 again!
    // it should go to proposal
    BOOST_REQUIRE_THROW( skaleHost->receiveTransaction( toJS( tx1.toBytes() ) ),
        dev::eth::PendingTransactionAlreadyExists );
    sleep( 1 );
    proposal = stub->pendingTransactions( 100 );
    BOOST_REQUIRE_EQUAL( proposal.size(), 1 );

    ConsensusExtFace::Transactions block3Txns;
    block3Txns.pushBackRegular( proposal.at( 0 ) );

    // submit it for sure
    BOOST_REQUIRE_NO_THROW( stub->createBlock( block3Txns,
#ifdef BITE
                                               DecryptedTransactions{
#ifdef BITE
                                                       std::make_shared< DecryptedCTXTxsMap >(),
#endif  // BITE
                                                       std::make_shared< DecryptedRegularTxsMap >()
                                                   },
#endif
        utcTime(), 3U ) );
}

BOOST_AUTO_TEST_SUITE_END()
