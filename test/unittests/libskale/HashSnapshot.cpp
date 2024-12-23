#include <libconsensus/libBLS/tools/utils.h>
#include <libdevcore/TransientDirectory.h>
#include <libdevcrypto/Hash.h>
#include <libethcore/KeyManager.h>
#include <libp2p/Network.h>

#define private public          // TODO refactor SnapshotManager
    #include <libskale/SnapshotManager.h>
#undef private

#include <libethereum/ClientTest.h>
#include <libskale/SnapshotHashAgent.h>

#include <libweb3jsonrpc/AccountHolder.h>
#include <libweb3jsonrpc/AdminEth.h>
#include <libweb3jsonrpc/Debug.h>
#include <libweb3jsonrpc/Eth.h>
#include <libweb3jsonrpc/Test.h>
#include <libweb3jsonrpc/Web3.h>
#include <skutils/btrfs.h>
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

boost::unit_test::assertion_result option_all_test( boost::unit_test::test_unit_id ) {
    return boost::unit_test::assertion_result( dev::test::Options::get().all ? true : false );
}

namespace dev {
namespace test {
class SnapshotHashAgentTest {
public:
    SnapshotHashAgentTest( ChainParams& _chainParams, const std::string& urlToDownloadSnapshotFrom ) {
        std::vector< libff::alt_bn128_Fr > coeffs( _chainParams.sChain.t );

        for ( auto& elem : coeffs ) {
            elem = libff::alt_bn128_Fr::random_element();

            while ( elem == 0 ) {
                elem = libff::alt_bn128_Fr::random_element();
            }
        }


        blsPrivateKeys_.resize( _chainParams.sChain.nodes.size() );
        for ( size_t i = 0; i < _chainParams.sChain.nodes.size(); ++i ) {
            blsPrivateKeys_[i] = libff::alt_bn128_Fr::zero();

            for ( size_t j = 0; j < _chainParams.sChain.t; ++j ) {
                blsPrivateKeys_[i] =
                    blsPrivateKeys_[i] +
                    coeffs[j] *
                        libff::power( libff::alt_bn128_Fr( std::to_string( i + 1 ).c_str() ), j );
            }
        }

        libBLS::Bls obj =
            libBLS::Bls( _chainParams.sChain.t, _chainParams.sChain.nodes.size() );
        std::vector< size_t > idx( _chainParams.sChain.t );
        for ( size_t i = 0; i < _chainParams.sChain.t; ++i ) {
            idx[i] = i + 1;
        }
        auto lagrange_coeffs = libBLS::ThresholdUtils::LagrangeCoeffs( idx, _chainParams.sChain.t );
        auto keys = obj.KeysRecover( lagrange_coeffs, this->blsPrivateKeys_ );
        keys.second.to_affine_coordinates();
        _chainParams.nodeInfo.commonBLSPublicKeys[0] =
            libBLS::ThresholdUtils::fieldElementToString( keys.second.X.c0 );
        _chainParams.nodeInfo.commonBLSPublicKeys[1] =
            libBLS::ThresholdUtils::fieldElementToString( keys.second.X.c1 );
        _chainParams.nodeInfo.commonBLSPublicKeys[2] =
            libBLS::ThresholdUtils::fieldElementToString( keys.second.Y.c0 );
        _chainParams.nodeInfo.commonBLSPublicKeys[3] =
            libBLS::ThresholdUtils::fieldElementToString( keys.second.Y.c1 );

        this->secret_as_is = keys.first;

        isSnapshotMajorityRequired = !urlToDownloadSnapshotFrom.empty();

        this->hashAgent_.reset( new SnapshotHashAgent( _chainParams, _chainParams.nodeInfo.commonBLSPublicKeys, urlToDownloadSnapshotFrom ) );
    }

    void fillData( const std::vector< dev::h256 >& snapshot_hashes ) {
        this->hashAgent_->hashes_ = snapshot_hashes;

        for ( size_t i = 0; i < this->hashAgent_->n_; ++i ) {
            this->hashAgent_->isReceived_[i] = true;
            this->hashAgent_->public_keys_[i] =
                this->blsPrivateKeys_[i] * libff::alt_bn128_G2::one();
            this->hashAgent_->signatures_[i] = libBLS::Bls::Signing(
                libBLS::ThresholdUtils::HashtoG1( std::make_shared< std::array< uint8_t, 32 > >(
                    this->hashAgent_->hashes_[i].asArray() ) ),
                this->blsPrivateKeys_[i] );
        }
    }

    size_t verifyAllData() const {
        return this->hashAgent_->verifyAllData();
    }

    std::vector< size_t > getNodesToDownloadSnapshotFrom() {
        bool res = this->voteForHash();

        if ( !res ) {
            return {};
        }

        if ( isSnapshotMajorityRequired )
            return this->hashAgent_->nodesToDownloadSnapshotFrom_;

        std::vector< size_t > ret;
        for ( size_t i = 0; i < this->hashAgent_->n_; ++i ) {
            if ( this->hashAgent_->chainParams_.nodeInfo.id ==
                 this->hashAgent_->chainParams_.sChain.nodes[i].id ) {
                continue;
            }

            if ( this->hashAgent_->hashes_[i] == this->hashAgent_->votedHash_.first ) {
                ret.push_back( i );
            }
        }

        return ret;
    }

    std::pair< dev::h256, libff::alt_bn128_G1 > getVotedHash() const {
        return this->hashAgent_->getVotedHash();
    }

    bool voteForHash() { return this->hashAgent_->voteForHash(); }

    void spoilSignature( size_t idx ) {
        this->hashAgent_->signatures_[idx] = libff::alt_bn128_G1::random_element();
    }

    libff::alt_bn128_Fr secret_as_is;

    std::shared_ptr< SnapshotHashAgent > hashAgent_;

    bool isSnapshotMajorityRequired = false;

    std::vector< libff::alt_bn128_Fr > blsPrivateKeys_;
};
}  // namespace test
}  // namespace dev

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
        res = setresgid( 0, 0, 0 );
        if ( res ) {
            cerr << strerror( errno ) << endl;
            assert( false );
        }
#endif
    }
};

// TODO Do not copy&paste from JsonRpcFixture
struct SnapshotHashingFixture : public TestOutputHelperFixture, public FixtureCommon {
    SnapshotHashingFixture() {
        check_sudo();

        dropRoot();

        int rv = system( ( "dd if=/dev/zero of=" + BTRFS_FILE_PATH + " bs=1M count=" + to_string(200) ).c_str() );
        rv = system( ( "mkfs.btrfs " + BTRFS_FILE_PATH ).c_str() );
        rv = system( ( "mkdir " + BTRFS_DIR_PATH ).c_str() );

        gainRoot();
        rv = system( ( "mount -o user_subvol_rm_allowed " + BTRFS_FILE_PATH + " " + BTRFS_DIR_PATH )
                    .c_str() );
        rv = chown( BTRFS_DIR_PATH.c_str(), sudo_uid, sudo_gid );
        ( void )rv;
        dropRoot();

        //        btrfs.subvolume.create( ( BTRFS_DIR_PATH + "/vol1" ).c_str() );
        //        btrfs.subvolume.create( ( BTRFS_DIR_PATH + "/vol2" ).c_str() );
        // system( ( "mkdir " + BTRFS_DIR_PATH + "/snapshots" ).c_str() );

        gainRoot();

        ChainParams chainParams;
        dev::p2p::NetworkPreferences nprefs;
        chainParams.sealEngineName = NoProof::name();
        chainParams.allowFutureBlocks = true;
        chainParams.difficulty = chainParams.minimumDifficulty;
        chainParams.gasLimit = chainParams.maxGasLimit;
        chainParams.byzantiumForkBlock = 0;
        chainParams.externalGasDifficulty = 1;
        chainParams.sChain.contractStorageLimit = 0x1122334455667788UL;
        // add random extra data to randomize genesis hash and get random DB path,
        // so that tests can be run in parallel
        // TODO: better make it use ethemeral in-memory databases
        chainParams.extraData = h256::random().asBytes();

        chainParams.sChain.emptyBlockIntervalMs = 1000;

        //        web3.reset( new WebThreeDirect(
        //            "eth tests", tempDir.path(), "", chainParams, WithExisting::Kill, {"eth"},
        //            true ) );

        mgr.reset( new SnapshotManager( chainParams, boost::filesystem::path( BTRFS_DIR_PATH ) ) );

        boost::filesystem::create_directory(
            boost::filesystem::path( BTRFS_DIR_PATH ) / "filestorage" / "test_dir" );

        std::string tmp_str =
            ( boost::filesystem::path( BTRFS_DIR_PATH ) / "filestorage" / "test_dir._hash" )
                .string();
        std::ofstream directoryHashFile( tmp_str );
        dev::h256 directoryPathHash = dev::sha256(
            ( boost::filesystem::path( BTRFS_DIR_PATH ) / "filestorage" / "test_dir" ).string() );
        directoryHashFile << directoryPathHash;
        directoryHashFile.close();

        std::ofstream newFile( ( boost::filesystem::path( BTRFS_DIR_PATH ) / "filestorage" /
                                 "test_dir" / "test_file.txt" )
                                   .string() );
        newFile << 1111;
        newFile.close();

        std::ofstream newFileHash( ( boost::filesystem::path( BTRFS_DIR_PATH ) / "filestorage" /
                                     "test_dir" / "test_file.txt._hash" )
                                       .string() );
        dev::h256 filePathHash = dev::sha256( ( boost::filesystem::path( BTRFS_DIR_PATH ) /
                                                "filestorage" / "test_dir" / "test_file.txt._hash" )
                                                  .string() );
        dev::h256 fileContentHash = dev::sha256( std::to_string( 1111 ) );

        secp256k1_sha256_t fileData;
        secp256k1_sha256_initialize( &fileData );
        secp256k1_sha256_write( &fileData, filePathHash.data(), filePathHash.size );
        secp256k1_sha256_write( &fileData, fileContentHash.data(), fileContentHash.size );

        dev::h256 fileHash;
        secp256k1_sha256_finalize( &fileData, fileHash.data() );

        newFileHash << fileHash;

        // TODO creation order with dependencies, gasPricer etc..
        auto monitor = make_shared< InstanceMonitor >("test");

        setenv("DATA_DIR", BTRFS_DIR_PATH.c_str(), 1);
        client.reset( new eth::ClientTest( chainParams, ( int ) chainParams.networkID,
            shared_ptr< GasPricer >(), NULL, monitor, boost::filesystem::path( BTRFS_DIR_PATH ),
            WithExisting::Kill ) );

        //        client.reset(
        //            new eth::Client( chainParams, ( int ) chainParams.networkID, shared_ptr<
        //            GasPricer >(),
        //                tempDir.path(), "", WithExisting::Kill, TransactionQueue::Limits{100000,
        //                1024} ) );

        // wait for 1st block to prevent race conditions in UnsafeRegion
        std::promise< void > block_promise;
        auto importHandler = client->setOnBlockImport(
            [&block_promise]( BlockHeader const& ) {
                    block_promise.set_value();
        } );

        client->injectSkaleHost();
        client->startWorking();

        block_promise.get_future().wait();

        client->setAuthor( coinbase.address() );

        using FullServer = ModularServer< rpc::EthFace, /* rpc::NetFace,*/ rpc::Web3Face,
            rpc::AdminEthFace /*, rpc::AdminNetFace*/, rpc::DebugFace, rpc::TestFace >;

        accountHolder.reset( new FixedAccountHolder( [&]() { return client.get(); }, {} ) );
        accountHolder->setAccounts( {coinbase, account2} );

        sessionManager.reset( new rpc::SessionManager() );
        adminSession =
            sessionManager->newSession( rpc::SessionPermissions{{rpc::Privilege::Admin}} );

        auto ethFace = new rpc::Eth( std::string(""), *client, *accountHolder.get() );

        gasPricer = make_shared< eth::TrivialGasPricer >( 1000, 1000 );
        client->setGasPricer(gasPricer);

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

    ~SnapshotHashingFixture() {
        client.reset();
        const char* NC = getenv( "NC" );
        if ( NC )
            return;
        gainRoot();
        [[maybe_unused]] int rv = system( ( "umount " + BTRFS_DIR_PATH ).c_str() );
        assert(rv == 0);
        rv = system( ( "rmdir " + BTRFS_DIR_PATH ).c_str() );
        assert(rv == 0);
        rv = system( ( "rm " + BTRFS_FILE_PATH ).c_str() );
        assert(rv == 0);
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

    TransientDirectory tempDir; // ! should exist before client!
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
    unique_ptr< SnapshotManager > mgr;
};
} //namespace

BOOST_AUTO_TEST_SUITE( SnapshotSigningTestSuite )

BOOST_AUTO_TEST_CASE( PositiveTest ) {
    libff::init_alt_bn128_params();
    ChainParams chainParams;
    chainParams.sChain.t = 3;
    chainParams.sChain.nodes.resize( 4 );
    for ( size_t i = 0; i < chainParams.sChain.nodes.size(); ++i ) {
        chainParams.sChain.nodes[i].id = i;
    }
    chainParams.nodeInfo.id = 3;
    SnapshotHashAgentTest test_agent( chainParams, "" );
    dev::h256 hash = dev::h256::random();
    std::vector< dev::h256 > snapshot_hashes( chainParams.sChain.nodes.size(), hash );
    test_agent.fillData( snapshot_hashes );
    BOOST_REQUIRE( test_agent.verifyAllData() == 3 );
    auto res = test_agent.getNodesToDownloadSnapshotFrom();
    std::vector< size_t > excpected = {0, 1, 2};
    BOOST_REQUIRE( res == excpected );
    BOOST_REQUIRE( test_agent.getVotedHash().first == hash );
    BOOST_REQUIRE( test_agent.getVotedHash().second ==
                   libBLS::Bls::Signing(
                       libBLS::ThresholdUtils::HashtoG1(
                           std::make_shared< std::array< uint8_t, 32 > >( hash.asArray() ) ),
                       test_agent.secret_as_is ) );
}

BOOST_AUTO_TEST_CASE( WrongHash ) {
    libff::init_alt_bn128_params();
    ChainParams chainParams;
    chainParams.sChain.t = 5;
    chainParams.sChain.nodes.resize( 7 );
    for ( size_t i = 0; i < chainParams.sChain.nodes.size(); ++i ) {
        chainParams.sChain.nodes[i].id = i;
    }
    chainParams.nodeInfo.id = 6;
    SnapshotHashAgentTest test_agent( chainParams, "" );
    dev::h256 hash = dev::h256::random();  // `correct` hash
    std::vector< dev::h256 > snapshot_hashes( chainParams.sChain.nodes.size(), hash );
    snapshot_hashes[4] = dev::h256::random();  // hash is different from `correct` hash
    test_agent.fillData( snapshot_hashes );
    BOOST_REQUIRE( test_agent.verifyAllData() == 6 );
    auto res = test_agent.getNodesToDownloadSnapshotFrom();
    std::vector< size_t > excpected = {0, 1, 2, 3, 5};
    BOOST_REQUIRE( res == excpected );
}

BOOST_AUTO_TEST_CASE( NotEnoughVotes ) {
    libff::init_alt_bn128_params();
    ChainParams chainParams;
    chainParams.sChain.t = 3;
    chainParams.sChain.nodes.resize( 4 );
    for ( size_t i = 0; i < chainParams.sChain.nodes.size(); ++i ) {
        chainParams.sChain.nodes[i].id = i;
    }
    chainParams.nodeInfo.id = 3;
    SnapshotHashAgentTest test_agent( chainParams, "" );
    dev::h256 hash = dev::h256::random();
    std::vector< dev::h256 > snapshot_hashes( chainParams.sChain.nodes.size(), hash );
    snapshot_hashes[2] = dev::h256::random();
    test_agent.fillData( snapshot_hashes );
    BOOST_REQUIRE( test_agent.verifyAllData() == 3);
    BOOST_REQUIRE_THROW( test_agent.voteForHash(), NotEnoughVotesException );
}

BOOST_AUTO_TEST_CASE( WrongSignature ) {
    libff::init_alt_bn128_params();
    ChainParams chainParams;
    chainParams.sChain.t = 3;
    chainParams.sChain.nodes.resize( 4 );
    for ( size_t i = 0; i < chainParams.sChain.nodes.size(); ++i ) {
        chainParams.sChain.nodes[i].id = i;
    }
    chainParams.nodeInfo.id = 3;
    SnapshotHashAgentTest test_agent( chainParams, "" );
    dev::h256 hash = dev::h256::random();
    std::vector< dev::h256 > snapshot_hashes( chainParams.sChain.nodes.size(), hash );
    test_agent.fillData( snapshot_hashes );
    test_agent.spoilSignature( 0 );
    BOOST_REQUIRE( test_agent.verifyAllData() == 2 );
}

BOOST_AUTO_TEST_CASE( noSnapshotMajority ) {
    libff::init_alt_bn128_params();
    ChainParams chainParams;
    chainParams.sChain.t = 3;
    chainParams.sChain.nodes.resize( 4 );
    for ( size_t i = 0; i < chainParams.sChain.nodes.size(); ++i ) {
        chainParams.sChain.nodes[i].id = i;
    }
    chainParams.nodeInfo.id = 3;

    chainParams.sChain.nodes[0].ip = "123.45.68.89";
    chainParams.sChain.nodes[1].ip = "123.45.87.89";
    chainParams.sChain.nodes[2].ip = "123.45.77.89";

    chainParams.sChain.nodes[3].ip = "123.45.67.89";
    std::string url = chainParams.sChain.nodes[3].ip + std::string( ":1234" );

    SnapshotHashAgentTest test_agent( chainParams, url );
    dev::h256 hash = dev::h256::random();
    std::vector< dev::h256 > snapshot_hashes( chainParams.sChain.nodes.size(), hash );
    snapshot_hashes[2] = dev::h256::random();
    test_agent.fillData( snapshot_hashes );

    BOOST_REQUIRE_THROW( test_agent.getNodesToDownloadSnapshotFrom(), NotEnoughVotesException );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( HashSnapshotTestSuite, *boost::unit_test::precondition( option_all_test ) )

#define WAIT_FOR_THE_NEXT_BLOCK() { \
    auto bn = client->number(); \
    while ( client->number() == bn ) { \
        usleep( 100 ); \
    } \
}

BOOST_FIXTURE_TEST_CASE( SnapshotHashingTest, SnapshotHashingFixture,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value t;
    t["from"] = toJS( senderAddress );
    t["to"] = toJS( receiver.address() );
    t["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );

    BOOST_REQUIRE( client->getLatestSnapshotBlockNumer() == -1 );

    // Mine to generate a non-zero account balance
    const int blocksToMine = 1;
    dev::eth::simulateMining( *( client ), blocksToMine );

    mgr->doSnapshot( 1 );
    mgr->computeSnapshotHash( 1 );
    BOOST_REQUIRE( mgr->isSnapshotHashPresent( 1 ) );

    BOOST_REQUIRE( client->number() == 1 );
    WAIT_FOR_THE_NEXT_BLOCK();

    mgr->doSnapshot( 2 );
    mgr->computeSnapshotHash( 2 );
    BOOST_REQUIRE( mgr->isSnapshotHashPresent( 2 ) );

    BOOST_REQUIRE( client->number() == 2 );
    WAIT_FOR_THE_NEXT_BLOCK();

    auto hash1 = mgr->getSnapshotHash( 1 );
    auto hash2 = mgr->getSnapshotHash( 2 );

    BOOST_REQUIRE( hash1 != hash2 );

    BOOST_REQUIRE_THROW( mgr->isSnapshotHashPresent( 3 ), SnapshotManager::SnapshotAbsent );

    BOOST_REQUIRE_THROW( mgr->getSnapshotHash( 3 ), SnapshotManager::SnapshotAbsent );

    // TODO check hash absence separately

    BOOST_REQUIRE( client->number() == 3 );
    WAIT_FOR_THE_NEXT_BLOCK();

    mgr->doSnapshot( 3 );

    mgr->computeSnapshotHash( 3, true );

    BOOST_REQUIRE( mgr->isSnapshotHashPresent( 3 ) );

    dev::h256 hash3_dbl = mgr->getSnapshotHash( 3 );

    mgr->computeSnapshotHash( 3 );

    BOOST_REQUIRE( mgr->isSnapshotHashPresent( 3 ) );

    dev::h256 hash3 = mgr->getSnapshotHash( 3 );

    BOOST_REQUIRE( hash3_dbl == hash3 );

    dev::h256 hash = client->hashFromNumber( 3 );
    uint64_t timestampFromBlockchain = client->blockInfo( hash ).timestamp();

    BOOST_REQUIRE_EQUAL( timestampFromBlockchain, mgr->getBlockTimestamp( 3 ) );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( SnapshotPerformanceSuite, *boost::unit_test::disabled() )

BOOST_FIXTURE_TEST_CASE( hashing_speed_db, SnapshotHashingFixture ) {
//    *boost::unit_test::disabled() ) {
    // 21s
    // dev::db::LevelDB db("/home/dimalit/skaled/big_states/1GR_1.4GB/a77d61c4/12041/state");
    // 150s
    dev::db::LevelDB db("/home/dimalit/skale-node-tests/big_states/1/da3e7c49/12041/state");
    auto t1 = std::chrono::high_resolution_clock::now();
    auto hash = db.hashBase();
    auto t2 = std::chrono::high_resolution_clock::now();
    std::cout << "Hash = " << hash << " Time = " << std::chrono::duration<double>(t2-t1).count() << std::endl;
}

BOOST_FIXTURE_TEST_CASE( hashing_speed_fs, SnapshotHashingFixture) {
    secp256k1_sha256_t ctx;
    secp256k1_sha256_initialize( &ctx );

    auto t1 = std::chrono::high_resolution_clock::now();
    // 140s - with 4k and 1b files both
    mgr->proceedFileStorageDirectory("/home/dimalit/skale-node-tests/big_states/10GBF", &ctx, false);
    dev::h256 hash;
    secp256k1_sha256_finalize( &ctx, hash.data() );
    auto t2 = std::chrono::high_resolution_clock::now();
    std::cout << "Hash = " << hash << " Time = " << std::chrono::duration<double>(t2-t1).count() << std::endl;
}


BOOST_AUTO_TEST_SUITE_END()
