/*
    Modifications Copyright (C) 2018-2019 SKALE Labs

    This file is part of skaled.

    skaled is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skaled is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skaled.  If not, see <http://www.gnu.org/licenses/>.
 */
/**
 * @author Sergiy <l_sergiy@skalelabs.com>
 * @date 2019
 */

#include "RunChain.h"
#include <boost/test/unit_test.hpp>

#include <fstream>
#include <iostream>
#include <thread>

#include <stdint.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/options_description.hpp>

#include <json_spirit/JsonSpiritHeaders.h>

#include <libdevcore/FileSystem.h>
#include <libdevcore/LoggingProgramOptions.h>
#include <libethashseal/Ethash.h>
#include <libethashseal/EthashClient.h>
#include <libethashseal/GenesisInfo.h>
#include <libethcore/KeyManager.h>
#include <libethereum/ClientTest.h>
#include <libethereum/Defaults.h>
#include <libethereum/SnapshotImporter.h>
#include <libethereum/SnapshotStorage.h>
#include <libevm/VMFactory.h>

#include <libskale/ConsensusGasPricer.h>

#include <libdevcrypto/LibSnark.h>

#include <libweb3jsonrpc/AccountHolder.h>
//#include <libweb3jsonrpc/AdminEth.h>
#include <libweb3jsonrpc/Debug.h>
#include <libweb3jsonrpc/Eth.h>
#include <libweb3jsonrpc/IpcServer.h>
#include <libweb3jsonrpc/ModularServer.h>
#include <libweb3jsonrpc/Net.h>

//#include <libweb3jsonrpc/Personal.h>

#include <libweb3jsonrpc/Skale.h>
#include <libweb3jsonrpc/SkaleStats.h>
#include <libweb3jsonrpc/Test.h>
#include <libweb3jsonrpc/Web3.h>

#include <jsonrpccpp/server/connectors/httpserver.h>

#include <libp2p/Network.h>

#include <libskale/httpserveroverride.h>

#include <libweb3jsonrpc/Skale.h>
#include <skale/buildinfo.h>

#include <boost/algorithm/string/replace.hpp>

#include <stdlib.h>
#include <time.h>

#include "../../skaled/MinerAux.h"

#include <skutils/console_colors.h>
#include <skutils/rest_call.h>
#include <skutils/url.h>
#include <skutils/utils.h>

using namespace std;
using namespace dev;
using namespace dev::p2p;
using namespace dev::eth;
// namespace po = boost::program_options;
namespace fs = boost::filesystem;

#ifndef ETH_MINIUPNPC
#define ETH_MINIUPNPC 0
#endif

namespace test {

run_chain::run_chain() {}

run_chain::~run_chain() {}

static void setCLocale() {
#if __unix__
    if ( !std::setlocale( LC_ALL, "C" ) ) {
        setenv( "LC_ALL", "C", 1 );
    }
#endif
}

static void stopSealingAfterXBlocks( eth::Client* _c, unsigned _start, unsigned& io_mining ) {
    try {
        if ( io_mining != ~0U && io_mining && asEthashClient( _c )->isMining() &&
             _c->blockChain().details().number - _start == io_mining ) {
            _c->stopSealing();
            io_mining = ~0U;
        }
    } catch ( InvalidSealEngine& ) {
    }

    this_thread::sleep_for( chrono::milliseconds( 100 ) );
}

std::string run_chain::clientVersion() const {
    return "test_1.0";
}

bool run_chain::shouldExit() const {
    return false;
}

bool run_chain::init() {
    try {
        cc::_on_ = true;
        cc::_max_value_size_ = std::string::npos;  // 2048

        MicroProfileSetEnableAllGroups( true );
        BlockHeader::useTimestampHack = false;

        setCLocale();

        // Init secp256k1 context by calling one of the functions.
        toPublic( {} );

        // Init defaults
        Defaults::get();
        Ethash::init();
        NoProof::init();

        string jsonAdmin;
        ChainParams chainParams;
        string privateChain;

        bool upnp = true;
        WithExisting withExisting = WithExisting::Trust;

        static const unsigned NoNetworkID = static_cast< unsigned int >( -1 );
        unsigned networkID = NoNetworkID;

        /// Mining params
        // strings presaleImports;

        /// Transaction params
        bool alwaysConfirm = false;

        MinerCLI m( MinerCLI::OperationMode::None );

        skutils::dispatch::default_domain( skutils::tools::cpu_count() * 2 );

        string configJSON;
        bool chainConfigIsSet = false, chainConfigParsed = false;
        static nlohmann::json joConfig;
        //
        //
        //
        //
        //
        //
        //
        //
        //    try {
        //        configPath = vm["config"].as< string >();
        //        configJSON = contentsString( configPath.string() );
        //        if ( configJSON.empty() )
        //            throw "Config file probably not found";
        //        joConfig = nlohmann::json::parse( configJSON );
        //        chainConfigParsed = true;
        //    } catch ( ... ) {
        //        cerr << "Bad --config option: " << vm["config"].as< string >() << "\n";
        //        return false;
        //    }
        //
        //
        //
        //
        //
        //
        //
        //
        BOOST_REQUIRE( chainConfigParsed );
        BOOST_REQUIRE( chainConfigIsSet );

        // Fget "httpRpcPort", "httpsRpcPort", "wsRpcPort" and "wssRpcPort" from config.json
        try {
            nExplicitPortHTTP4 = joConfig["skaleConfig"]["nodeInfo"]["httpRpcPort"].get< int >();
        } catch ( ... ) {
            nExplicitPortHTTP4 = -1;
        }
        if ( !( 0 <= nExplicitPortHTTP4 && nExplicitPortHTTP4 <= 65535 ) )
            nExplicitPortHTTP4 = -1;
        else
            std::cout << cc::debug( "Got " )
                      << cc::notice( "HTTP/4 port" ) + cc::debug( " from configuration JSON: " )
                      << cc::num10( nExplicitPortHTTP4 ) << "\n";
        //
        try {
            nExplicitPortHTTP6 = joConfig["skaleConfig"]["nodeInfo"]["httpRpcPort6"].get< int >();
        } catch ( ... ) {
            nExplicitPortHTTP6 = -1;
        }
        if ( !( 0 <= nExplicitPortHTTP6 && nExplicitPortHTTP6 <= 65535 ) )
            nExplicitPortHTTP6 = nExplicitPortHTTP4;
        if ( !( 0 <= nExplicitPortHTTP6 && nExplicitPortHTTP6 <= 65535 ) )
            nExplicitPortHTTP6 = -1;
        else
            std::cout << cc::debug( "Got " )
                      << cc::notice( "HTTP/6 port" ) + cc::debug( " from configuration JSON: " )
                      << cc::num10( nExplicitPortHTTP6 ) << "\n";
        //
        //
        try {
            nExplicitPortHTTPS4 = joConfig["skaleConfig"]["nodeInfo"]["httpsRpcPort"].get< int >();
        } catch ( ... ) {
            nExplicitPortHTTPS4 = -1;
        }
        if ( !( 0 <= nExplicitPortHTTPS4 && nExplicitPortHTTPS4 <= 65535 ) )
            nExplicitPortHTTPS4 = -1;
        else
            std::cout << cc::debug( "Got " )
                      << cc::notice( "HTTPS/4 port" ) + cc::debug( " from configuration JSON: " )
                      << cc::num10( nExplicitPortHTTPS4 ) << "\n";
        //
        try {
            nExplicitPortHTTPS6 = joConfig["skaleConfig"]["nodeInfo"]["httpsRpcPort6"].get< int >();
        } catch ( ... ) {
            nExplicitPortHTTPS6 = -1;
        }
        if ( !( 0 <= nExplicitPortHTTPS6 && nExplicitPortHTTPS6 <= 65535 ) )
            nExplicitPortHTTPS6 = nExplicitPortHTTPS4;
        if ( !( 0 <= nExplicitPortHTTPS6 && nExplicitPortHTTPS6 <= 65535 ) )
            nExplicitPortHTTPS6 = -1;
        else
            std::cout << cc::debug( "Got " )
                      << cc::notice( "HTTPS/6 port" ) + cc::debug( " from configuration JSON: " )
                      << cc::num10( nExplicitPortHTTPS6 ) << "\n";
        //
        //
        try {
            nExplicitPortWS4 = joConfig["skaleConfig"]["nodeInfo"]["wsRpcPort"].get< int >();
        } catch ( ... ) {
            nExplicitPortWS4 = -1;
        }
        if ( !( 0 <= nExplicitPortWS4 && nExplicitPortWS4 <= 65535 ) )
            nExplicitPortWS4 = -1;
        else
            std::cout << cc::debug( "Got " )
                      << cc::notice( "WS/4 port" ) + cc::debug( " from configuration JSON: " )
                      << cc::num10( nExplicitPortWS4 ) << "\n";
        //
        try {
            nExplicitPortWS6 = joConfig["skaleConfig"]["nodeInfo"]["wsRpcPort6"].get< int >();
        } catch ( ... ) {
            nExplicitPortWS6 = -1;
        }
        if ( !( 0 <= nExplicitPortWS6 && nExplicitPortWS6 <= 65535 ) )
            nExplicitPortWS6 = nExplicitPortWS4;
        if ( !( 0 <= nExplicitPortWS6 && nExplicitPortWS6 <= 65535 ) )
            nExplicitPortWS6 = -1;
        else
            std::cout << cc::debug( "Got " )
                      << cc::notice( "WS/6 port" ) + cc::debug( " from configuration JSON: " )
                      << cc::num10( nExplicitPortWS6 ) << "\n";
        //
        //
        try {
            nExplicitPortWSS4 = joConfig["skaleConfig"]["nodeInfo"]["wssRpcPort"].get< int >();
        } catch ( ... ) {
            nExplicitPortWSS4 = -1;
        }
        if ( !( 0 <= nExplicitPortWSS4 && nExplicitPortWSS4 <= 65535 ) )
            nExplicitPortWSS4 = -1;
        else
            std::cout << cc::debug( "Got " )
                      << cc::notice( "WSS/4 port" ) + cc::debug( " from configuration JSON: " )
                      << cc::num10( nExplicitPortWSS4 ) << "\n";
        //
        try {
            nExplicitPortWSS6 = joConfig["skaleConfig"]["nodeInfo"]["wssRpcPort6"].get< int >();
        } catch ( ... ) {
            nExplicitPortWSS6 = -1;
        }
        if ( !( 0 <= nExplicitPortWSS6 && nExplicitPortWSS6 <= 65535 ) )
            nExplicitPortWSS6 = nExplicitPortWSS4;
        if ( !( 0 <= nExplicitPortWSS6 && nExplicitPortWSS6 <= 65535 ) )
            nExplicitPortWSS6 = -1;
        else
            std::cout << cc::debug( "Got " )
                      << cc::notice( "WSS/6 port" ) + cc::debug( " from configuration JSON: " )
                      << cc::num10( nExplicitPortWSS6 ) << "\n";

        // get "web3-trace" from config.json
        try {
            bTraceJsonRpcCalls = joConfig["skaleConfig"]["nodeInfo"]["web3-trace"].get< bool >();
        } catch ( ... ) {
        }
        std::cout << cc::info( "JSON RPC" ) << cc::debug( " trace logging mode is " )
                  << cc::flag_ed( bTraceJsonRpcCalls ) << "\n";

        // get "enable-debug-behavior-apis" from config.json
        try {
            rpc::Debug::g_bEnabledDebugBehaviorAPIs =
                joConfig["skaleConfig"]["nodeInfo"]["enable-debug-behavior-apis"].get< bool >();
        } catch ( ... ) {
        }
        std::cout << cc::warn( "Important notce: " ) << cc::debug( "Programmatic " )
                  << cc::info( "enable-debug-behavior-apis" ) << cc::debug( " mode is " )
                  << cc::flag_ed( rpc::Debug::g_bEnabledDebugBehaviorAPIs ) << "\n";

        // get "unsafe-transactions" from config.json
        try {
            alwaysConfirm =
                joConfig["skaleConfig"]["nodeInfo"]["unsafe-transactions"].get< bool >() ? false :
                                                                                           true;
        } catch ( ... ) {
        }
        std::cout << cc::warn( "Important notce: " ) << cc::debug( "Programmatic " )
                  << cc::info( "unsafe-transactions" ) << cc::debug( " mode is " )
                  << cc::flag_ed( !alwaysConfirm ) << "\n";

        // get "web3-shutdown" from config.json
        bool bEnabledShutdownViaWeb3 = false;
        try {
            bEnabledShutdownViaWeb3 =
                joConfig["skaleConfig"]["nodeInfo"]["web3-shutdown"].get< bool >();
        } catch ( ... ) {
        }

        std::cout << cc::warn( "Important notce: " ) << cc::debug( "Programmatic " )
                  << cc::info( "web3-shutdown" ) << cc::debug( " mode is " )
                  << cc::flag_ed( bEnabledShutdownViaWeb3 ) << "\n";

        // get "ipcpath" from config.json
        // Second, get it from command line parameter (higher priority source)
        std::string strPathIPC;
        try {
            strPathIPC = joConfig["skaleConfig"]["nodeInfo"]["ipcpath"].get< std::string >();
        } catch ( ... ) {
            strPathIPC.clear();
        }
        std::cout << cc::notice( "IPC path" ) + cc::debug( " is: " ) << cc::p( strPathIPC ) << "\n";
        if ( !strPathIPC.empty() )
            setIpcPath( strPathIPC );

        // get "db-path"" from config.json
        // Second, get it from command line parameter (higher priority source)
        std::string strPathDB;
        try {
            strPathDB = joConfig["skaleConfig"]["nodeInfo"]["db-path"].get< std::string >();
        } catch ( ... ) {
            strPathDB.clear();
        }
        std::cout << cc::notice( "DB path" ) + cc::debug( " is: " ) << cc::p( strPathDB ) << "\n";
        if ( !strPathDB.empty() )
            setDataDir( strPathDB );

        // if ( vm.count( "bls-key-file" ) && vm["bls-key-file"].as< string >() != "NULL" ) {
        //    try {
        //        fs::path blsFile = vm["bls-key-file"].as< string >();
        //        blsJson = contentsString( blsFile.string() );
        //        if ( blsJson.empty() )
        //            throw "BLS key file probably not found";
        //    } catch ( ... ) {
        //        cerr << "Bad --bls-key-file option: " << vm["bls-key-file"].as< string >() <<
        //        "\n"; return false;
        //    }
        //}
        // if ( vm.count( "public-ip" ) ) {
        //    publicIP = vm["public-ip"].as< string >();
        //}
        // if ( vm.count( "remote" ) ) {
        //    string host = vm["remote"].as< string >();
        //    string::size_type found = host.find_first_of( ':' );
        //    if ( found != std::string::npos ) {
        //        remoteHost = host.substr( 0, found );
        //    } else
        //        remoteHost = host;
        //}
        // if ( vm.count( "password" ) )
        //    passwordsToNote.push_back( vm["password"].as< string >() );
        // if ( vm.count( "master" ) ) {
        //    masterPassword = vm["master"].as< string >();
        //    masterSet = true;
        //}
        // if ( vm.count( "dont-check" ) )
        //    safeImport = true;
        // if ( vm.count( "format" ) ) {
        //    string m = vm["format"].as< string >();
        //    if ( m == "binary" )
        //        exportFormat = Format::Binary;
        //    else if ( m == "hex" )
        //        exportFormat = Format::Hex;
        //    else if ( m == "human" )
        //        exportFormat = Format::Human;
        //    else {
        //        cerr << "Bad "
        //             << "--format"
        //             << " option: " << m << "\n";
        //        return false;
        //    }
        //}
        // if ( vm.count( "network-id" ) )
        //    try {
        //        networkID = vm["network-id"].as< unsigned >();
        //    } catch ( ... ) {
        //        cerr << "Bad "
        //             << "--network-id"
        //             << " option: " << vm["network-id"].as< string >() << "\n";
        //        return false;
        //    }
        // if ( vm.count( "kill" ) )
        //    withExisting = WithExisting::Kill;
        // if ( vm.count( "rebuild" ) )
        //    withExisting = WithExisting::Verify;
        // if ( vm.count( "rescue" ) )
        //    withExisting = WithExisting::Rescue;
        // if ( ( vm.count( "import-secret" ) ) ) {
        //    Secret s( fromHex( vm["import-secret"].as< string >() ) );
        //    toImport.emplace_back( s );
        //}
        // if ( vm.count( "import-session-secret" ) ) {
        //    Secret s( fromHex( vm["import-session-secret"].as< string >() ) );
        //    toImport.emplace_back( s );
        //}
        // if ( vm.count( "skale" ) ) {
        //    chainParams = ChainParams( genesisInfo( eth::Network::Skale ) );
        //    chainConfigIsSet = true;
        //}

        if ( !configJSON.empty() ) {
            try {
                fs::path configPath;
                chainParams = chainParams.loadConfig( configJSON, configPath );
                chainConfigIsSet = true;
            } catch ( const json_spirit::Error_position& err ) {
                cerr << "error in parsing config json:\n";
                cerr << err.reason_ << " line " << err.line_ << endl;
                cerr << configJSON << endl;
            } catch ( const std::exception& ex ) {
                cerr << "provided configuration is not well formatted\n";
                cerr << configJSON << endl;
                cerr << ex.what() << endl;
                return false;
            } catch ( ... ) {
                cerr << "provided configuration is not well formatted\n";
                // cerr << "sample: \n" << genesisInfo(eth::Network::MainNetworkTest) << "\n";
                cerr << configJSON << endl;
                return false;
            }
        }

        string blsPrivateKey;
        string blsPublicKey1;
        string blsPublicKey2;
        string blsPublicKey3;
        string blsPublicKey4;
        // if ( !blsJson.empty() ) {
        //    try {
        //        using namespace json_spirit;

        //        mValue val;
        //        json_spirit::read_string_or_throw( blsJson, val );
        //        mObject obj = val.get_obj();

        //        string blsPrivateKey = obj["secret_key"].get_str();

        //        mArray pub = obj["common_public"].get_array();

        //        string blsPublicKey1 = pub[0].get_str();
        //        string blsPublicKey2 = pub[1].get_str();
        //        string blsPublicKey3 = pub[2].get_str();
        //        string blsPublicKey4 = pub[3].get_str();

        //    } catch ( const json_spirit::Error_position& err ) {
        //        cerr << "error in parsing BLS keyfile:\n";
        //        cerr << err.reason_ << " line " << err.line_ << endl;
        //        cerr << blsJson << endl;
        //    } catch ( ... ) {
        //        cerr << "BLS keyfile is not well formatted\n";
        //        cerr << blsJson << endl;
        //        return false;
        //    }
        //}

        LoggingOptions loggingOptions;
        setupLogging( loggingOptions );

        if ( !chainConfigIsSet )
            // default to skale if not already set with `--config`
            chainParams = ChainParams( genesisInfo( eth::Network::Skale ) );

        std::shared_ptr< SnapshotManager > snapshotManager;
        // if ( chainParams.nodeInfo.snapshotInterval > 0 || vm.count( "download-snapshot" ) )
        //    snapshotManager.reset( new SnapshotManager(
        //        getDataDir(), {BlockChain::getChainDirName( chainParams ), "filestorage",
        //                          "prices_" + chainParams.nodeInfo.id.str() + ".db"} ) );
        //// it was needed for snapshot downloading
        // if ( chainParams.nodeInfo.snapshotInterval <= 0 ) {
        snapshotManager = nullptr;
        //}

        if ( loggingOptions.verbosity > 0 )
            cout << cc::attention( "skaled, a C++ Skale client" ) << "\n";

        m.execute();

        // fs::path secretsPath = SecretStore::defaultPath();
        // KeyManager keyManager( KeyManager::defaultPath(), secretsPath );
        // for ( auto const& s : passwordsToNote )
        //    keyManager.notePassword( s );

        auto nodesState = contents( getDataDir() / fs::path( "network.rlp" ) );
        auto caps = set< string >{"eth"};

        std::unique_ptr< Client > client;
        std::shared_ptr< GasPricer > gasPricer;

        if ( getDataDir().size() )
            Defaults::setDBPath( getDataDir() );
        if ( nodeMode == NodeMode::Full && caps.count( "eth" ) ) {
            Ethash::init();
            NoProof::init();

            if ( chainParams.sealEngineName == Ethash::name() ) {
                client.reset( new eth::EthashClient( chainParams, ( int ) chainParams.networkID,
                    shared_ptr< GasPricer >(), snapshotManager, getDataDir(), withExisting,
                    TransactionQueue::Limits{100000, 1024} ) );
            } else if ( chainParams.sealEngineName == NoProof::name() ) {
                client.reset( new eth::Client( chainParams, ( int ) chainParams.networkID,
                    shared_ptr< GasPricer >(), snapshotManager, getDataDir(), withExisting,
                    TransactionQueue::Limits{100000, 1024} ) );
            } else
                BOOST_THROW_EXCEPTION( ChainParamsInvalid() << errinfo_comment(
                                           "Unknown seal engine: " + chainParams.sealEngineName ) );

            client->setAuthor( chainParams.sChain.owner );

            DefaultConsensusFactory cons_fact( *client, blsPrivateKey, blsPublicKey1, blsPublicKey2,
                blsPublicKey3, blsPublicKey4 );
            std::shared_ptr< SkaleHost > skaleHost =
                std::make_shared< SkaleHost >( *client, &cons_fact );
            gasPricer = std::make_shared< ConsensusGasPricer >( *skaleHost );

            client->setGasPricer( gasPricer );
            client->injectSkaleHost( skaleHost );
            client->startWorking();

            const auto* buildinfo = skale_get_buildinfo();
            client->setExtraData(
                rlpList( 0, string{buildinfo->project_version}.substr( 0, 5 ) + "++" +
                                string{buildinfo->git_commit_hash}.substr( 0, 4 ) +
                                string{buildinfo->build_type}.substr( 0, 1 ) +
                                string{buildinfo->system_name}.substr( 0, 5 ) +
                                string{buildinfo->compiler_id}.substr( 0, 3 ) ) );
        }

        auto toNumber = [&]( string const& s ) -> unsigned {
            if ( s == "latest" )
                return client->number();
            if ( s.size() == 64 || ( s.size() == 66 && s.substr( 0, 2 ) == "0x" ) )
                return client->blockChain().number( h256( s ) );
            try {
                return static_cast< unsigned int >( stoul( s ) );
            } catch ( ... ) {
                cerr << "Bad block number/hash option: " << s << "\n";
                return static_cast< unsigned int >( -1 );
            }
        };

        cout << "skaled " << Version << "\n";

        if ( nodeMode == NodeMode::Full ) {
            client->setSealer( m.minerType() );
            if ( networkID != NoNetworkID )
                client->setNetworkId( networkID );
        }

        cout << "Mining Beneficiary: " << client->author() << endl;

        unique_ptr< ModularServer<> > jsonrpcIpcServer;
        unique_ptr< rpc::SessionManager > sessionManager;
        unique_ptr< AccountHolder > accountHolder;

        AddressHash allowedDestinations;

        std::function< bool( TransactionSkeleton const&, bool ) > authenticator;

        authenticator = [&]( TransactionSkeleton const& _t, bool ) -> bool {
            allowedDestinations.insert( _t.to );
            return true;
        };
        if ( chainParams.nodeInfo.ip.empty() ) {
            std::cout << cc::info( "IPv4" )
                      << cc::warn( " bind address is not set, will not start RPC on this protocol" )
                      << "\n";
            nExplicitPortHTTP4 = nExplicitPortHTTPS4 = nExplicitPortWS4 = nExplicitPortWSS4 = -1;
        }
        if ( chainParams.nodeInfo.ip6.empty() ) {
            std::cout << cc::info( "IPv6" )
                      << cc::warn( " bind address is not set, will not start RPC on this protocol" )
                      << "\n";
            nExplicitPortHTTP6 = nExplicitPortHTTPS6 = nExplicitPortWS6 = nExplicitPortWSS6 = -1;
        }
        if ( is_ipc || nExplicitPortHTTP4 > 0 || nExplicitPortHTTPS4 > 0 || nExplicitPortWS4 > 0 ||
             nExplicitPortWSS4 > 0 || nExplicitPortHTTP6 > 0 || nExplicitPortHTTPS6 > 0 ||
             nExplicitPortWS6 > 0 || nExplicitPortWSS6 > 0 ) {
            using FullServer = ModularServer< rpc::EthFace,
                rpc::SkaleFace,               /// skale
                rpc::SkaleStats,              /// skaleStats
                rpc::NetFace, rpc::Web3Face,  // rpc::PersonalFace,
                // rpc::AdminEthFace,            // SKALE rpc::AdminNetFace,
                rpc::DebugFace, rpc::TestFace >;

            sessionManager.reset( new rpc::SessionManager() );
            // accountHolder.reset( new SimpleAccountHolder(
            //    [&]() { return client.get(); }, getAccountPassword, keyManager, authenticator ) );
            accountHolder.reset( new NoAccountHolder );

            auto ethFace = new rpc::Eth( *client, *accountHolder.get() );
            /// skale
            auto skaleFace = new rpc::Skale( *client );
            /// skaleStatsFace
            auto skaleStatsFace = new rpc::SkaleStats( joConfig, *client );

            jsonrpcIpcServer.reset( new FullServer( ethFace,
                skaleFace,       /// skale
                skaleStatsFace,  /// skaleStats
                new rpc::Net(), new rpc::Web3( clientVersion() ),
                // new rpc::Personal( keyManager, *accountHolder, *client ),
                // new rpc::AdminEth( *client, *gasPricer.get(), keyManager, *sessionManager.get()
                // ),
                new rpc::Debug( *client ), nullptr ) );

            if ( is_ipc ) {
                try {
                    auto ipcConnector = new IpcServer( "geth" );
                    jsonrpcIpcServer->addConnector( ipcConnector );
                    if ( !ipcConnector->StartListening() ) {
                        std::cout << "Cannot start listening for RPC requests on ipc port: "
                                  << strerror( errno ) << "\n";
                        return false;
                    }  // error
                } catch ( const std::exception& ex ) {
                    std::cout << "Cannot start listening for RPC requests on ipc port: "
                              << ex.what() << "\n";
                    return false;
                }  // catch
            }      // if ( is_ipc )

            if ( nExplicitPortHTTP4 >= 65536 ) {
                std::cout << cc::fatal( "FATAL:" ) << cc::error( " Please specify valid value " )
                          << cc::warn( "--http-port" ) << cc::error( "=" ) << cc::warn( "number" )
                          << "\n";
                return false;
            }
            if ( nExplicitPortHTTP6 >= 65536 ) {
                std::cout << cc::fatal( "FATAL:" ) << cc::error( " Please specify valid value " )
                          << cc::warn( "--http-port6" ) << cc::error( "=" ) << cc::warn( "number" )
                          << "\n";
                return false;
            }
            if ( nExplicitPortHTTPS4 >= 65536 ) {
                std::cout << cc::fatal( "FATAL:" ) << cc::error( " Please specify valid value " )
                          << cc::warn( "--https-port" ) << cc::error( "=" ) << cc::warn( "number" )
                          << "\n";
                return false;
            }
            if ( nExplicitPortHTTPS6 >= 65536 ) {
                std::cout << cc::fatal( "FATAL:" ) << cc::error( " Please specify valid value " )
                          << cc::warn( "--https-port6" ) << cc::error( "=" ) << cc::warn( "number" )
                          << "\n";
                return false;
            }
            if ( nExplicitPortWS4 >= 65536 ) {
                std::cout << cc::fatal( "FATAL:" ) << cc::error( " Please specify valid value " )
                          << cc::warn( "--ws-port" ) << cc::error( "=" ) << cc::warn( "number" )
                          << "\n";
                return false;
            }
            if ( nExplicitPortWS6 >= 65536 ) {
                std::cout << cc::fatal( "FATAL:" ) << cc::error( " Please specify valid value " )
                          << cc::warn( "--ws-port6" ) << cc::error( "=" ) << cc::warn( "number" )
                          << "\n";
                return false;
            }
            if ( nExplicitPortWSS4 >= 65536 ) {
                std::cout << cc::fatal( "FATAL:" ) << cc::error( " Please specify valid value " )
                          << cc::warn( "--wss-port" ) << cc::error( "=" ) << cc::warn( "number" )
                          << "\n";
                return false;
            }
            if ( nExplicitPortWSS6 >= 65536 ) {
                std::cout << cc::fatal( "FATAL:" ) << cc::error( " Please specify valid value " )
                          << cc::warn( "--wss-port6" ) << cc::error( "=" ) << cc::warn( "number" )
                          << "\n";
                return false;
            }

            if ( nExplicitPortHTTP4 > 0 || nExplicitPortHTTPS4 > 0 || nExplicitPortWS4 > 0 ||
                 nExplicitPortWSS4 > 0 || nExplicitPortHTTP6 > 0 || nExplicitPortHTTPS6 > 0 ||
                 nExplicitPortWS6 > 0 || nExplicitPortWSS6 > 0 ) {
                std::cout << cc::debug( "...." ) << cc::attention( "RPC params" )
                          << cc::debug( ":" ) << "\n";
                //
                std::cout << cc::debug( "...." ) << cc::info( "HTTP/4 port" )
                          << cc::debug( ".............................. " )
                          << ( ( nExplicitPortHTTP4 >= 0 ) ? cc::num10( nExplicitPortHTTP4 ) :
                                                             cc::error( "off" ) )
                          << "\n";
                std::cout << cc::debug( "...." ) << cc::info( "HTTP/6 port" )
                          << cc::debug( ".............................. " )
                          << ( ( nExplicitPortHTTP6 >= 0 ) ? cc::num10( nExplicitPortHTTP6 ) :
                                                             cc::error( "off" ) )
                          << "\n";
                //
                std::cout << cc::debug( "...." ) << cc::info( "HTTPS/4 port" )
                          << cc::debug( "............................. " )
                          << ( ( nExplicitPortHTTPS4 >= 0 ) ? cc::num10( nExplicitPortHTTPS4 ) :
                                                              cc::error( "off" ) )
                          << "\n";
                std::cout << cc::debug( "...." ) << cc::info( "HTTPS/6 port" )
                          << cc::debug( "............................. " )
                          << ( ( nExplicitPortHTTPS6 >= 0 ) ? cc::num10( nExplicitPortHTTPS6 ) :
                                                              cc::error( "off" ) )
                          << "\n";
                //
                std::cout << cc::debug( "...." ) << cc::info( "WS/4 port" )
                          << cc::debug( "................................ " )
                          << ( ( nExplicitPortWS4 >= 0 ) ? cc::num10( nExplicitPortWS4 ) :
                                                           cc::error( "off" ) );
                std::cout << cc::debug( "...." ) << cc::info( "WS/6 port" )
                          << cc::debug( "................................ " )
                          << ( ( nExplicitPortWS6 >= 0 ) ? cc::num10( nExplicitPortWS6 ) :
                                                           cc::error( "off" ) );
                //
                std::cout << cc::debug( "...." ) << cc::info( "WSS/4 port" )
                          << cc::debug( "............................... " )
                          << ( ( nExplicitPortWSS4 >= 0 ) ? cc::num10( nExplicitPortWSS4 ) :
                                                            cc::error( "off" ) )
                          << "\n";
                std::cout << cc::debug( "...." ) << cc::info( "WSS/6 port" )
                          << cc::debug( "............................... " )
                          << ( ( nExplicitPortWSS6 >= 0 ) ? cc::num10( nExplicitPortWSS6 ) :
                                                            cc::error( "off" ) )
                          << "\n";
                //
                std::string strPathSslKey, strPathSslCert;
                bool bHaveSSL = false;
                // if ( ( nExplicitPortHTTPS4 > 0 || nExplicitPortWSS4 > 0 ||
                //         nExplicitPortHTTPS6 > 0 || nExplicitPortWSS6 > 0 ) &&
                //     vm.count( "ssl-key" ) > 0 && vm.count( "ssl-cert" ) > 0 ) {
                //    strPathSslKey = vm["ssl-key"].as< std::string >();
                //    strPathSslCert = vm["ssl-cert"].as< std::string >();
                //    if ( ( !strPathSslKey.empty() ) && ( !strPathSslCert.empty() ) )
                //        bHaveSSL = true;
                //}
                if ( !bHaveSSL )
                    nExplicitPortHTTPS4 = nExplicitPortWSS4 = nExplicitPortHTTPS6 =
                        nExplicitPortWSS6 = -1;
                if ( bHaveSSL ) {
                    std::cout << cc::debug( "...." ) << cc::info( "SSL key is" )
                              << cc::debug( "............................... " )
                              << cc::p( strPathSslKey ) << "\n";
                    std::cout << cc::debug( "...." ) + cc::info( "SSL certificate is" )
                              << cc::debug( "....................... " ) << cc::p( strPathSslCert )
                              << "\n";
                }
                //
                //
                size_t maxConnections = 0, cntServers = 1;

                // First, get "max-connections" true/false from config.json
                // Second, get it from command line parameter (higher priority source)
                if ( chainConfigParsed ) {
                    try {
                        maxConnections =
                            joConfig["skaleConfig"]["nodeInfo"]["max-connections"].get< size_t >();
                    } catch ( ... ) {
                        maxConnections = 0;
                    }
                }
                // if ( vm.count( "max-connections" ) )
                //    maxConnections = vm["max-connections"].as< size_t >();

                // First, get "acceptors" true/false from config.json
                // Second, get it from command line parameter (higher priority source)
                if ( chainConfigParsed ) {
                    try {
                        cntServers =
                            joConfig["skaleConfig"]["nodeInfo"]["acceptors"].get< size_t >();
                    } catch ( ... ) {
                        cntServers = 1;
                    }
                }
                // if ( vm.count( "acceptors" ) )
                //    cntServers = vm["acceptors"].as< size_t >();
                if ( cntServers < 1 )
                    cntServers = 1;

                // First, get "ws-mode" true/false from config.json
                // Second, get it from command line parameter (higher priority source)
                if ( chainConfigParsed ) {
                    try {
                        std::string s =
                            joConfig["skaleConfig"]["nodeInfo"]["ws-mode"].get< std::string >();
                        skutils::ws::nlws::g_default_srvmode = skutils::ws::nlws::str2srvmode( s );
                    } catch ( ... ) {
                    }
                }
                // if ( vm.count( "ws-mode" ) ) {
                //    std::string s = vm["ws-mode"].as< std::string >();
                //    skutils::ws::nlws::g_default_srvmode = skutils::ws::nlws::str2srvmode( s );
                //}

                // First, get "ws-log" true/false from config.json
                // Second, get it from command line parameter (higher priority source)
                if ( chainConfigParsed ) {
                    try {
                        std::string s =
                            joConfig["skaleConfig"]["nodeInfo"]["ws-log"].get< std::string >();
                        skutils::ws::g_eWSLL = skutils::ws::str2wsll( s );
                    } catch ( ... ) {
                    }
                }
                // if ( vm.count( "ws-log" ) ) {
                //    std::string s = vm["ws-log"].as< std::string >();
                //    skutils::ws::g_eWSLL = skutils::ws::str2wsll( s );
                //}

                std::cout << cc::debug( "...." ) + cc::info( "WS mode" )
                          << cc::debug( ".................................. " )
                          << skutils::ws::nlws::srvmode2str( skutils::ws::nlws::g_default_srvmode )
                          << "\n";
                std::cout << cc::debug( "...." ) + cc::info( "WS logging" )
                          << cc::debug( "............................... " )
                          << cc::info( skutils::ws::wsll2str( skutils::ws::g_eWSLL ) ) << "\n";
                std::cout << cc::debug( "...." ) + cc::info( "Max RPC connections" )
                          << cc::debug( "...................... " )
                          << ( ( maxConnections > 0 ) ? cc::size10( maxConnections ) :
                                                        cc::error( "disabled" ) )
                          << "\n";
                std::cout << cc::debug( "...." ) + cc::info( "Parallel RPC connection acceptors" )
                          << cc::debug( "........ " ) << cc::size10( cntServers ) << "\n";
                SkaleServerOverride::fn_binary_snapshot_download_t fn_binary_snapshot_download =
                    [=]( const nlohmann::json& joRequest ) -> std::vector< uint8_t > {
                    return skaleFace->impl_skale_downloadSnapshotFragmentBinary( joRequest );
                };
                auto skale_server_connector = new SkaleServerOverride( chainParams,
                    fn_binary_snapshot_download, cntServers, client.get(), chainParams.nodeInfo.ip,
                    nExplicitPortHTTP4, chainParams.nodeInfo.ip6, nExplicitPortHTTP6,
                    chainParams.nodeInfo.ip, nExplicitPortHTTPS4, chainParams.nodeInfo.ip6,
                    nExplicitPortHTTPS6, chainParams.nodeInfo.ip, nExplicitPortWS4,
                    chainParams.nodeInfo.ip6, nExplicitPortWS6, chainParams.nodeInfo.ip,
                    nExplicitPortWSS4, chainParams.nodeInfo.ip6, nExplicitPortWSS6, strPathSslKey,
                    strPathSslCert );
                //
                skaleStatsFace->setProvider( skale_server_connector );
                skale_server_connector->setConsumer( skaleStatsFace );
                //
                skale_server_connector->m_bTraceCalls = bTraceJsonRpcCalls;
                skale_server_connector->max_connection_set( maxConnections );
                jsonrpcIpcServer->addConnector( skale_server_connector );
                if ( !skale_server_connector->StartListening() ) {  // TODO Will it delete itself?
                    return false;
                }
                int nStatHTTP4 = skale_server_connector->getServerPortStatusHTTP( 4 );
                int nStatHTTP6 = skale_server_connector->getServerPortStatusHTTP( 6 );
                int nStatHTTPS4 = skale_server_connector->getServerPortStatusHTTPS( 4 );
                int nStatHTTPS6 = skale_server_connector->getServerPortStatusHTTPS( 6 );
                int nStatWS4 = skale_server_connector->getServerPortStatusWS( 4 );
                int nStatWS6 = skale_server_connector->getServerPortStatusWS( 6 );
                int nStatWSS4 = skale_server_connector->getServerPortStatusWSS( 4 );
                int nStatWSS6 = skale_server_connector->getServerPortStatusWSS( 6 );
                static const size_t g_cntWaitAttempts = 30;
                static const std::chrono::milliseconds g_waitAttempt =
                    std::chrono::milliseconds( 100 );
                if ( nExplicitPortHTTP4 > 0 ) {
                    for ( size_t idxWaitAttempt = 0;
                          nStatHTTP4 < 0 && idxWaitAttempt < g_cntWaitAttempts && ( !shouldExit() );
                          ++idxWaitAttempt ) {
                        if ( idxWaitAttempt == 0 )
                            std::cout << cc::debug( "Waiting for " ) + cc::info( "HTTP/4" )
                                      << cc::debug( " start... " ) << "\n";
                        std::this_thread::sleep_for( g_waitAttempt );
                        nStatHTTP4 = skale_server_connector->getServerPortStatusHTTP( 4 );
                    }
                }
                if ( nExplicitPortHTTP6 > 0 ) {
                    for ( size_t idxWaitAttempt = 0;
                          nStatHTTP6 < 0 && idxWaitAttempt < g_cntWaitAttempts && ( !shouldExit() );
                          ++idxWaitAttempt ) {
                        if ( idxWaitAttempt == 0 )
                            std::cout << cc::debug( "Waiting for " ) + cc::info( "HTTP/6" )
                                      << cc::debug( " start... " ) << "\n";
                        std::this_thread::sleep_for( g_waitAttempt );
                        nStatHTTP6 = skale_server_connector->getServerPortStatusHTTP( 6 );
                    }
                }
                if ( nExplicitPortHTTPS4 > 0 ) {
                    for ( size_t idxWaitAttempt = 0;
                          nStatHTTPS4 < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                          ( !shouldExit() );
                          ++idxWaitAttempt ) {
                        if ( idxWaitAttempt == 0 )
                            std::cout << cc::debug( "Waiting for " ) + cc::info( "HTTPS/4" )
                                      << cc::debug( " start... " ) << "\n";
                        std::this_thread::sleep_for( g_waitAttempt );
                        nStatHTTPS4 = skale_server_connector->getServerPortStatusHTTPS( 4 );
                    }
                }
                if ( nExplicitPortHTTPS6 > 0 ) {
                    for ( size_t idxWaitAttempt = 0;
                          nStatHTTPS6 < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                          ( !shouldExit() );
                          ++idxWaitAttempt ) {
                        if ( idxWaitAttempt == 0 )
                            std::cout << cc::debug( "Waiting for " ) + cc::info( "HTTPS/6" )
                                      << cc::debug( " start... " ) << "\n";
                        std::this_thread::sleep_for( g_waitAttempt );
                        nStatHTTPS6 = skale_server_connector->getServerPortStatusHTTPS( 6 );
                    }
                }
                if ( nExplicitPortWS4 > 0 ) {
                    for ( size_t idxWaitAttempt = 0;
                          nStatWS4 < 0 && idxWaitAttempt < g_cntWaitAttempts && ( !shouldExit() );
                          ++idxWaitAttempt ) {
                        if ( idxWaitAttempt == 0 )
                            std::cout << cc::debug( "Waiting for " ) + cc::info( "WS/4" )
                                      << cc::debug( " start... " ) << "\n";
                        std::this_thread::sleep_for( g_waitAttempt );
                        nStatWS4 = skale_server_connector->getServerPortStatusWS( 4 );
                    }
                }
                if ( nExplicitPortWS6 > 0 ) {
                    for ( size_t idxWaitAttempt = 0;
                          nStatWS6 < 0 && idxWaitAttempt < g_cntWaitAttempts && ( !shouldExit() );
                          ++idxWaitAttempt ) {
                        if ( idxWaitAttempt == 0 )
                            std::cout << cc::debug( "Waiting for " ) + cc::info( "WS/6" )
                                      << cc::debug( " start... " ) << "\n";
                        std::this_thread::sleep_for( g_waitAttempt );
                        nStatWS6 = skale_server_connector->getServerPortStatusWS( 6 );
                    }
                }
                if ( nExplicitPortWSS4 > 0 ) {
                    for ( size_t idxWaitAttempt = 0;
                          nStatWSS4 < 0 && idxWaitAttempt < g_cntWaitAttempts && ( !shouldExit() );
                          ++idxWaitAttempt ) {
                        if ( idxWaitAttempt == 0 )
                            std::cout << cc::debug( "Waiting for " ) + cc::info( "WSS/4" )
                                      << cc::debug( " start... " ) << "\n";
                        nStatWSS4 = skale_server_connector->getServerPortStatusWSS( 4 );
                    }
                }
                if ( nExplicitPortWSS6 > 0 ) {
                    for ( size_t idxWaitAttempt = 0;
                          nStatWSS6 < 0 && idxWaitAttempt < g_cntWaitAttempts && ( !shouldExit() );
                          ++idxWaitAttempt ) {
                        if ( idxWaitAttempt == 0 )
                            std::cout << cc::debug( "Waiting for " ) + cc::info( "WSS/6" )
                                      << cc::debug( " start... " ) << "\n";
                        nStatWSS6 = skale_server_connector->getServerPortStatusWSS( 6 );
                    }
                }
                std::cout << cc::debug( "...." ) << cc::attention( "RPC status" )
                          << cc::debug( ":" ) << "\n";
                std::cout << cc::debug( "...." ) << cc::info( "HTTP/4" )
                          << cc::debug( "................................. " )
                          << ( ( nStatHTTP4 >= 0 ) ? ( ( nExplicitPortHTTP4 > 0 ) ?
                                                             cc::num10( nStatHTTP4 ) :
                                                             cc::warn( "still starting..." ) ) :
                                                     cc::error( "off" ) );
                std::cout << cc::debug( "...." ) << cc::info( "HTTP/6" )
                          << cc::debug( "................................. " )
                          << ( ( nStatHTTP6 >= 0 ) ? ( ( nExplicitPortHTTP6 > 0 ) ?
                                                             cc::num10( nStatHTTP6 ) :
                                                             cc::warn( "still starting..." ) ) :
                                                     cc::error( "off" ) )
                          << "\n";
                //
                std::cout << cc::debug( "...." ) << cc::info( "HTTPS/4" )
                          << cc::debug( "................................ " )
                          << ( ( nStatHTTPS4 >= 0 ) ? ( ( nExplicitPortHTTPS4 > 0 ) ?
                                                              cc::num10( nStatHTTPS4 ) :
                                                              cc::warn( "still starting..." ) ) :
                                                      cc::error( "off" ) )
                          << "\n";
                std::cout << cc::debug( "...." ) << cc::info( "HTTPS/6" )
                          << cc::debug( "................................ " )
                          << ( ( nStatHTTPS6 >= 0 ) ? ( ( nExplicitPortHTTPS6 > 0 ) ?
                                                              cc::num10( nStatHTTPS6 ) :
                                                              cc::warn( "still starting..." ) ) :
                                                      cc::error( "off" ) )
                          << "\n";
                //
                std::cout << cc::debug( "...." ) << cc::info( "WS/4" )
                          << cc::debug( "................................... " )
                          << ( ( nStatWS4 >= 0 ) ? ( ( nExplicitPortWS4 > 0 ) ?
                                                           cc::num10( nStatWS4 ) :
                                                           cc::warn( "still starting..." ) ) :
                                                   cc::error( "off" ) );
                std::cout << cc::debug( "...." ) << cc::info( "WS/6" )
                          << cc::debug( "................................... " )
                          << ( ( nStatWS6 >= 0 ) ? ( ( nExplicitPortWS6 > 0 ) ?
                                                           cc::num10( nStatWS6 ) :
                                                           cc::warn( "still starting..." ) ) :
                                                   cc::error( "off" ) )
                          << "\n";
                //
                std::cout << cc::debug( "...." ) << cc::info( "WSS/4" )
                          << cc::debug( ".................................. " )
                          << ( ( nStatWSS4 >= 0 ) ? ( ( nExplicitPortWSS4 > 0 ) ?
                                                            cc::num10( nStatWSS4 ) :
                                                            cc::warn( "still starting..." ) ) :
                                                    cc::error( "off" ) )
                          << "\n";
                std::cout << cc::debug( "...." ) << cc::info( "WSS/6" )
                          << cc::debug( ".................................. " )
                          << ( ( nStatWSS6 >= 0 ) ? ( ( nExplicitPortWSS6 > 0 ) ?
                                                            cc::num10( nStatWSS6 ) :
                                                            cc::warn( "still starting..." ) ) :
                                                    cc::error( "off" ) )
                          << "\n";
            }  // if ( nExplicitPortHTTP > 0 || nExplicitPortHTTPS > 0 || nExplicitPortWS > 0 ||
            // nExplicitPortWSS > 0 )

            if ( jsonAdmin.empty() )
                jsonAdmin =
                    sessionManager->newSession( rpc::SessionPermissions{{rpc::Privilege::Admin}} );
            else
                sessionManager->addSession(
                    jsonAdmin, rpc::SessionPermissions{{rpc::Privilege::Admin}} );

            cout << "JSONRPC Admin Session Key: " << jsonAdmin << "\n";
        }  // if ( is_ipc || nExplicitPortHTTP > 0 || nExplicitPortHTTPS > 0  || nExplicitPortWS > 0
           // ||
        // nExplicitPortWSS > 0 )

        if ( bEnabledShutdownViaWeb3 ) {
            std::cout << "Enabling programmatic shutdown via Web3...\n";
            dev::rpc::Skale::enableWeb3Shutdown( true );
            dev::rpc::Skale::onShutdownInvoke( []() { ExitHandler::exitHandler( 0 ); } );
            cout << "Done, programmatic shutdown via Web3 is enabled\n";
        } else {
            std::cout << "Disabling programmatic shutdown via Web3...\n";
            dev::rpc::Skale::enableWeb3Shutdown( false );
            std::cout << "Done, programmatic shutdown via Web3 is disabled\n";
        }


        if ( client ) {
            unsigned int n = client->blockChain().details().number;
            unsigned int mining = 0;

            while ( !shouldExit() )
                stopSealingAfterXBlocks( client.get(), n, mining );
        } else
            while ( !shouldExit() )
                this_thread::sleep_for( chrono::milliseconds( 1000 ) );

        if ( jsonrpcIpcServer.get() )
            jsonrpcIpcServer->StopListening();

        std::cerr << localeconv()->decimal_point << std::endl;

        std::string basename = "profile" + chainParams.nodeInfo.id.str();
        MicroProfileDumpFileImmediately(
            ( basename + ".html" ).c_str(), ( basename + ".csv" ).c_str(), nullptr );
        MicroProfileShutdown();

        BOOST_REQUIRE( true );
        return true;
    } catch ( const Client::CreationException& ex ) {
        std::cout << dev::nested_exception_what( ex ) << "\n";
        // TODO close microprofile!!
    } catch ( const SkaleHost::CreationException& ex ) {
        std::cout << dev::nested_exception_what( ex ) << "\n";
        // TODO close microprofile!!
    } catch ( const std::exception& ex ) {
        std::cout << "CRITICAL " << dev::nested_exception_what( ex ) << "\n";
    } catch ( ... ) {
        std::cout << "CRITICAL unknown error"
                  << "\n";
    }
    BOOST_REQUIRE( false );
    return false;
}

};  // namespace test
