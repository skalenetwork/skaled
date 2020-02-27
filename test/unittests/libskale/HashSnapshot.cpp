#include <libconsensus/libBLS/bls/BLSutils.h>
#include <libdevcore/TransientDirectory.h>
#include <libdevcrypto/Hash.h>
#include <libethcore/KeyManager.h>
#include <libethereum/ClientTest.h>
#include <libp2p/Network.h>
#include <libskale/SnapshotHashAgent.h>
#include <libskale/SnapshotManager.h>
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
    SnapshotHashAgentTest( std::shared_ptr< SnapshotHashAgent > hashAgent )
        : hashAgent_( hashAgent ) {
        std::vector< libff::alt_bn128_Fr > coeffs( this->hashAgent_->chain_params_.sChain.t );

        for ( auto& elem : coeffs ) {
            elem = libff::alt_bn128_Fr::random_element();

            while ( elem == 0 ) {
                elem = libff::alt_bn128_Fr::random_element();
            }
        }


        insecureBlsPrivateKeys_.resize( this->hashAgent_->chain_params_.sChain.nodes.size() );
        for ( size_t i = 0; i < this->hashAgent_->chain_params_.sChain.nodes.size(); ++i ) {
            insecureBlsPrivateKeys_[i] = libff::alt_bn128_Fr::zero();

            for ( size_t j = 0; j < this->hashAgent_->chain_params_.sChain.t; ++j ) {
                insecureBlsPrivateKeys_[i] =
                    insecureBlsPrivateKeys_[i] +
                    coeffs[j] *
                        libff::power( libff::alt_bn128_Fr( std::to_string( i + 1 ).c_str() ), j );
            }
        }
    }

    void fillData( const std::vector< dev::h256 >& snapshot_hashes ) {
        this->hashAgent_->hashes_ = snapshot_hashes;

        for ( size_t i = 0; i < this->hashAgent_->n_; ++i ) {
            this->hashAgent_->public_keys_[i] =
                this->insecureBlsPrivateKeys_[i] * libff::alt_bn128_G2::one();
            this->hashAgent_->signatures_[i] = signatures::Bls::Signing(
                signatures::Bls::HashtoG1( std::make_shared< std::array< uint8_t, 32 > >(
                    this->hashAgent_->hashes_[i].asArray() ) ),
                this->insecureBlsPrivateKeys_[i] );
        }

        signatures::Bls obj =
            signatures::Bls( this->hashAgent_->chain_params_.sChain.t, this->hashAgent_->n_ );
        std::vector< size_t > idx( this->hashAgent_->chain_params_.sChain.t );
        for ( size_t i = 0; i < this->hashAgent_->chain_params_.sChain.t; ++i ) {
            idx[i] = i + 1;
        }
        auto lagrange_coeffs = obj.LagrangeCoeffs( idx );
        auto keys = obj.KeysRecover( lagrange_coeffs, this->insecureBlsPrivateKeys_ );
        keys.second.to_affine_coordinates();
        this->hashAgent_->chain_params_.nodeInfo.insecureCommonBLSPublicKeys[0] =
            BLSutils::ConvertToString( keys.second.X.c0 );
        this->hashAgent_->chain_params_.nodeInfo.insecureCommonBLSPublicKeys[1] =
            BLSutils::ConvertToString( keys.second.X.c1 );
        this->hashAgent_->chain_params_.nodeInfo.insecureCommonBLSPublicKeys[2] =
            BLSutils::ConvertToString( keys.second.Y.c0 );
        this->hashAgent_->chain_params_.nodeInfo.insecureCommonBLSPublicKeys[3] =
            BLSutils::ConvertToString( keys.second.Y.c1 );

        this->insecure_secret = keys.first;
    }

    bool verifyAllData() const {
        bool result = false;
        this->hashAgent_->verifyAllData( result );
        return result;
    }

    std::vector< size_t > getNodesToDownloadSnapshotFrom() {
        bool res = this->voteForHash();

        if ( !res ) {
            return {};
        }

        std::vector< size_t > ret;
        for ( size_t i = 0; i < this->hashAgent_->n_; ++i ) {
            if ( this->hashAgent_->chain_params_.nodeInfo.id ==
                 this->hashAgent_->chain_params_.sChain.nodes[i].id ) {
                continue;
            }

            if ( this->hashAgent_->hashes_[i] == this->hashAgent_->voted_hash_.first ) {
                ret.push_back( i );
            }
        }

        return ret;
    }

    std::pair< dev::h256, libff::alt_bn128_G1 > getVotedHash() const {
        return this->hashAgent_->getVotedHash();
    }

    bool voteForHash() { return this->hashAgent_->voteForHash( this->hashAgent_->voted_hash_ ); }

    void spoilSignature( size_t idx ) {
        this->hashAgent_->signatures_[idx] = libff::alt_bn128_G1::random_element();
    }

    libff::alt_bn128_Fr insecure_secret;

    std::shared_ptr< SnapshotHashAgent > hashAgent_;

    std::vector< libff::alt_bn128_Fr > insecureBlsPrivateKeys_;
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
        setresgid( 0, 0, 0 );
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
        dev::p2p::NetworkPreferences nprefs;
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

        //        web3.reset( new WebThreeDirect(
        //            "eth tests", tempDir.path(), "", chainParams, WithExisting::Kill, {"eth"},
        //            true ) );

        mgr.reset( new SnapshotManager( boost::filesystem::path( BTRFS_DIR_PATH ),
            {BlockChain::getChainDirName( chainParams ), "filestorage"} ) );

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

        client.reset( new eth::ClientTest( chainParams, ( int ) chainParams.networkID,
            shared_ptr< GasPricer >(), NULL, boost::filesystem::path( BTRFS_DIR_PATH ),
            WithExisting::Kill ) );

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

    ~SnapshotHashingFixture() {
        client.reset();
        const char* NC = getenv( "NC" );
        if ( NC )
            return;
        gainRoot();
        system( ( "umount " + BTRFS_DIR_PATH ).c_str() );
        system( ( "rmdir " + BTRFS_DIR_PATH ).c_str() );
        system( ( "rm " + BTRFS_FILE_PATH ).c_str() );
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
    unique_ptr< SnapshotManager > mgr;

    TransientDirectory tempDir;
};
}  // namespace

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
    std::shared_ptr< SnapshotHashAgent > agent;
    agent.reset( new SnapshotHashAgent( chainParams ) );
    SnapshotHashAgentTest test_agent( agent );
    dev::h256 hash = dev::h256::random();
    std::vector< dev::h256 > snapshot_hashes( chainParams.sChain.nodes.size(), hash );
    test_agent.fillData( snapshot_hashes );
    BOOST_REQUIRE( test_agent.verifyAllData() );
    auto res = test_agent.getNodesToDownloadSnapshotFrom();
    std::vector< size_t > excpected = {0, 1, 2};
    BOOST_REQUIRE( res == excpected );
    BOOST_REQUIRE( test_agent.getVotedHash().first == hash );
    BOOST_REQUIRE( test_agent.getVotedHash().second ==
                   signatures::Bls::Signing(
                       signatures::Bls::HashtoG1(
                           std::make_shared< std::array< uint8_t, 32 > >( hash.asArray() ) ),
                       test_agent.insecure_secret ) );
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
    std::shared_ptr< SnapshotHashAgent > agent;
    agent.reset( new SnapshotHashAgent( chainParams ) );
    SnapshotHashAgentTest test_agent( agent );
    dev::h256 hash = dev::h256::random();  // `correct` hash
    std::vector< dev::h256 > snapshot_hashes( chainParams.sChain.nodes.size(), hash );
    snapshot_hashes[4] = dev::h256::random();  // hash is different from `correct` hash
    test_agent.fillData( snapshot_hashes );
    BOOST_REQUIRE( test_agent.verifyAllData() );
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
    std::shared_ptr< SnapshotHashAgent > agent;
    agent.reset( new SnapshotHashAgent( chainParams ) );
    SnapshotHashAgentTest test_agent( agent );
    dev::h256 hash = dev::h256::random();
    std::vector< dev::h256 > snapshot_hashes( chainParams.sChain.nodes.size(), hash );
    snapshot_hashes[2] = dev::h256::random();
    test_agent.fillData( snapshot_hashes );
    BOOST_REQUIRE( test_agent.verifyAllData() );
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
    std::shared_ptr< SnapshotHashAgent > agent;
    agent.reset( new SnapshotHashAgent( chainParams ) );
    SnapshotHashAgentTest test_agent( agent );
    dev::h256 hash = dev::h256::random();
    std::vector< dev::h256 > snapshot_hashes( chainParams.sChain.nodes.size(), hash );
    test_agent.fillData( snapshot_hashes );
    test_agent.spoilSignature( 0 );
    BOOST_REQUIRE_THROW( test_agent.verifyAllData(), IsNotVerified );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( HashSnapshotTestSuite, *boost::unit_test::precondition( option_all_test ) )

BOOST_FIXTURE_TEST_CASE( SnapshotHashingTest, SnapshotHashingFixture ) {
    auto senderAddress = coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value t;
    t["from"] = toJS( senderAddress );
    t["to"] = toJS( receiver.address() );
    t["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );

    // Mine to generate a non-zero account balance
    const int blocksToMine = 1;
    dev::eth::simulateMining( *( client ), blocksToMine );

    mgr->doSnapshot( 1 );
    mgr->computeSnapshotHash( 1 );

    dev::eth::simulateMining( *( client ), blocksToMine );
    mgr->doSnapshot( 2 );

    mgr->computeSnapshotHash( 2 );

    BOOST_REQUIRE( mgr->isSnapshotHashPresent( 1 ) );
    BOOST_REQUIRE( mgr->isSnapshotHashPresent( 2 ) );

    auto hash1 = mgr->getSnapshotHash( 1 );
    auto hash2 = mgr->getSnapshotHash( 2 );

    BOOST_REQUIRE( hash1 != hash2 );

    mgr->doSnapshot( 3 );

    mgr->computeSnapshotHash( 3 );

    BOOST_REQUIRE( mgr->isSnapshotHashPresent( 3 ) );

    auto hash2_dbl = mgr->getSnapshotHash( 3 );

    BOOST_REQUIRE( hash2 == hash2_dbl );

    BOOST_REQUIRE_THROW( !mgr->isSnapshotHashPresent( 4 ), SnapshotManager::SnapshotAbsent );

    BOOST_REQUIRE_THROW( mgr->getSnapshotHash( 4 ), SnapshotManager::SnapshotAbsent );

    // TODO check hash absence separately
}

BOOST_FIXTURE_TEST_CASE( SnapshotHashingFileStorageTest, SnapshotHashingFixture ) {
    mgr->doSnapshot( 4 );

    mgr->computeSnapshotHash( 4, true );

    BOOST_REQUIRE( mgr->isSnapshotHashPresent( 4 ) );

    dev::h256 hash4_dbl = mgr->getSnapshotHash( 4 );

    mgr->computeSnapshotHash( 4 );

    BOOST_REQUIRE( mgr->isSnapshotHashPresent( 4 ) );

    dev::h256 hash4 = mgr->getSnapshotHash( 4 );

    BOOST_REQUIRE( hash4_dbl == hash4 );
}

BOOST_AUTO_TEST_SUITE_END()
