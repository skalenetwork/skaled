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
/**
 * @file main.cpp
 * @author Gav Wood <i@gavwood.com>
 * @author Tasha Carl <tasha@carl.pro> - I here by place all my contributions in this file under MIT
 * licence, as specified by http://opensource.org/licenses/MIT.
 * @date 2014
 * Ethereum client.
 */

#include <signal.h>
#include <fstream>
#include <iostream>
#include <thread>

#include <stdint.h>

#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/options_description.hpp>

#include <json_spirit/JsonSpiritHeaders.h>

#include <libdevcore/FileSystem.h>
#include <libdevcore/LevelDB.h>
#include <libdevcore/LoggingProgramOptions.h>
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
#include <libweb3jsonrpc/AdminEth.h>
#include <libweb3jsonrpc/Debug.h>
#include <libweb3jsonrpc/Eth.h>
#include <libweb3jsonrpc/IpcServer.h>
#include <libweb3jsonrpc/ModularServer.h>
#include <libweb3jsonrpc/Net.h>
#include <libweb3jsonrpc/Personal.h>
#include <libweb3jsonrpc/Skale.h>
#include <libweb3jsonrpc/SkaleStats.h>
#include <libweb3jsonrpc/Test.h>
#include <libweb3jsonrpc/Web3.h>

#include <jsonrpccpp/server/connectors/httpserver.h>

#include <libp2p/Network.h>

#include <libskale/SnapshotHashAgent.h>
#include <libskale/httpserveroverride.h>

#include "../libdevcore/microprofile.h"

#include "MinerAux.h"

#include <libweb3jsonrpc/Skale.h>
#include <skale/buildinfo.h>

#include <boost/algorithm/string/replace.hpp>

#include <stdlib.h>
#include <time.h>

#include <skutils/console_colors.h>
#include <skutils/rest_call.h>
#include <skutils/task_performance.h>
#include <skutils/utils.h>

using namespace std;
using namespace dev;
using namespace dev::p2p;
using namespace dev::eth;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

#ifndef ETH_MINIUPNPC
#define ETH_MINIUPNPC 0
#endif

namespace dev {
namespace db {
extern unsigned c_maxOpenLeveldbFiles;
}
}  // namespace dev

namespace {
std::atomic< bool > g_silence = {false};
unsigned const c_lineWidth = 160;

static void version() {
    const auto* buildinfo = skale_get_buildinfo();
    std::string pv = buildinfo->project_version, ver, commit;
    auto pos = pv.find( "+" );
    if ( pos != std::string::npos ) {
        commit = pv.c_str() + pos;
        boost::replace_all( commit, "+commit.", "" );
        ver = pv.substr( 0, pos );
    } else
        ver = pv;
    cout << "Skaled............................" << ver << "\n";
    if ( !commit.empty() ) {
        cout << "Commit............................" << commit << "\n";
    }
    cout << "Skale network protocol version...." << dev::eth::c_protocolVersion << "\n";
    cout << "Client database version..........." << dev::eth::c_databaseVersion << "\n";
    cout << "Build............................." << buildinfo->system_name << "/"
         << buildinfo->build_type << "\n";
}

static std::string clientVersion() {
    const auto* buildinfo = skale_get_buildinfo();
    return std::string( "skaled/" ) + buildinfo->project_version + "/" + buildinfo->system_name +
           "/" + buildinfo->compiler_id + buildinfo->compiler_version + "/" + buildinfo->build_type;
}

/*
The equivalent of setlocale(LC_ALL, “C”) is called before any user code is run.
If the user has an invalid environment setting then it is possible for the call
to set locale to fail, so there are only two possible actions, the first is to
throw a runtime exception and cause the program to quit (default behaviour),
or the second is to modify the environment to something sensible (least
surprising behaviour).

The follow code produces the least surprising behaviour. It will use the user
specified default locale if it is valid, and if not then it will modify the
environment the process is running in to use a sensible default. This also means
that users do not need to install language packs for their OS.
*/
void setCLocale() {
#if __unix__
    if ( !std::setlocale( LC_ALL, "C" ) ) {
        setenv( "LC_ALL", "C", 1 );
    }
#endif
}

void importPresale( KeyManager& _km, string const& _file, function< string() > _pass ) {
    KeyPair k = _km.presaleSecret( contentsString( _file ), [&]( bool ) { return _pass(); } );
    _km.import( k.secret(), "Presale wallet" + _file + " (insecure)" );
}

enum class NodeMode { PeerServer, Full };

enum class OperationMode { Node, Import, ImportSnapshot, Export };

enum class Format { Binary, Hex, Human };

void stopSealingAfterXBlocks( eth::Client* _c, unsigned _start, unsigned& io_mining ) {
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

void removeEmptyOptions( po::parsed_options& parsed ) {
    const set< string > filteredOptions = {"http-port", "https-port", "ws-port", "wss-port",
        "http-port6", "https-port6", "ws-port6", "wss-port6", "ws-log", "ssl-key", "ssl-cert",
        "acceptors"};
    const set< string > emptyValues = {"NULL", "null", "None"};

    parsed.options.erase( remove_if( parsed.options.begin(), parsed.options.end(),
                              [&filteredOptions, &emptyValues]( const auto& option ) -> bool {
                                  return filteredOptions.count( option.string_key ) &&
                                         emptyValues.count( option.value.front() );
                              } ),
        parsed.options.end() );
}

unsigned getLatestSnapshotBlockNumber( const std::string& strURLWeb3 ) {
    skutils::rest::client cli;
    if ( !cli.open( strURLWeb3 ) ) {
        throw std::runtime_error( "REST failed to connect to server" );
    }

    nlohmann::json joIn = nlohmann::json::object();
    joIn["jsonrpc"] = "2.0";
    joIn["method"] = "skale_getLatestSnapshotBlockNumber";
    joIn["params"] = nlohmann::json::object();
    skutils::rest::data_t d = cli.call( joIn );
    if ( d.empty() ) {
        throw std::runtime_error( "cannot get blockNumber to download snapshot" );
    }
    nlohmann::json joAnswer = nlohmann::json::parse( d.s_ );
    unsigned block_number = dev::eth::jsToBlockNumber( joAnswer["result"].get< std::string >() );

    return block_number;
}

void downloadSnapshot( unsigned block_number, std::shared_ptr< SnapshotManager >& snapshotManager,
    const std::string& strURLWeb3, const ChainParams& chainParams ) {
    fs::path saveTo;
    try {
        std::cout << cc::normal( "Will download snapshot from " ) << cc::u( strURLWeb3 )
                  << std::endl;

        try {
            bool isBinaryDownload = true;
            std::string strErrorDescription;
            saveTo = snapshotManager->getDiffPath( block_number );
            bool bOK = dev::rpc::snapshot::download( strURLWeb3, block_number, saveTo,
                [&]( size_t idxChunck, size_t cntChunks ) -> bool {
                    std::cout << cc::normal( "... download progress ... " )
                              << cc::size10( idxChunck ) << cc::normal( " of " )
                              << cc::size10( cntChunks ) << "\r";
                    return true;  // continue download
                },
                isBinaryDownload, &strErrorDescription );
            std::cout << "                                                  \r";  // clear
                                                                                  // progress
                                                                                  // line
            if ( !bOK ) {
                if ( strErrorDescription.empty() )
                    strErrorDescription = "download failed, connection problem during download";
                throw std::runtime_error( strErrorDescription );
            }
        } catch ( ... ) {
            std::throw_with_nested(
                std::runtime_error( cc::error( "Exception while downloading snapshot" ) ) );
        }
        std::cout << cc::success( "Snapshot download success for block " )
                  << cc::u( to_string( block_number ) ) << std::endl;
        try {
            snapshotManager->importDiff( block_number );
        } catch ( ... ) {
            std::throw_with_nested( std::runtime_error(
                cc::fatal( "FATAL:" ) + " " +
                cc::error( "Exception while importing downloaded snapshot: " ) ) );
        }

        /// HACK refactor this piece of code! ///
        vector< string > prefixes{"prices_", "blocks_"};
        for ( const string& prefix : prefixes ) {
            fs::path db_path;
            for ( auto& f :
                fs::directory_iterator( getDataDir() / "snapshots" / to_string( block_number ) ) ) {
                if ( f.path().string().find( prefix ) != string::npos ) {
                    db_path = f.path();
                    break;
                }  // if
            }
            if ( db_path.empty() ) {
                clog( VerbosityError, "downloadSnapshot" )
                    << cc::fatal( "Snapshot downloaded without " + prefix + " db" ) << std::endl;
                return;
            }

            fs::rename( db_path,
                db_path.parent_path() / ( prefix + chainParams.nodeInfo.id.str() + ".db" ) );
        }
        //// HACK END ////

        snapshotManager->restoreSnapshot( block_number );
        std::cout << cc::success( "Snapshot restore success for block " )
                  << cc::u( to_string( block_number ) ) << std::endl;

    } catch ( ... ) {
        std::throw_with_nested(
            std::runtime_error( cc::fatal( "FATAL:" ) + " " +
                                cc::error( "Exception while processing downloaded snapshot: " ) ) );
    }
    if ( !saveTo.empty() )
        fs::remove( saveTo );
}

}  // namespace

static const std::list< std::pair< std::string, std::string > >
get_machine_ip_addresses_4() {  // first-interface name, second-address
    static const std::list< std::pair< std::string, std::string > > listIfaceInfos4 =
        skutils::network::get_machine_ip_addresses( true, false );  // IPv4
    return listIfaceInfos4;
}
static const std::list< std::pair< std::string, std::string > >
get_machine_ip_addresses_6() {  // first-interface name, second-address
    static const std::list< std::pair< std::string, std::string > > listIfaceInfos6 =
        skutils::network::get_machine_ip_addresses( false, true );  // IPv6
    return listIfaceInfos6;
}

static std::unique_ptr< Client > g_client;
unique_ptr< ModularServer<> > g_jsonrpcIpcServer;

int main( int argc, char** argv ) try {
    cc::_on_ = false;
    cc::_max_value_size_ = 2048;
    MicroProfileSetEnableAllGroups( true );
    BlockHeader::useTimestampHack = false;

    srand( time( nullptr ) );

    setCLocale();

    skutils::signal::init_common_signal_handling( []( int nSignalNo ) -> void {
        if ( nSignalNo == SIGPIPE )
            return;
        bool stopWasRaisedBefore = skutils::signal::g_bStop;
        if ( !stopWasRaisedBefore ) {
            if ( g_jsonrpcIpcServer.get() ) {
                g_jsonrpcIpcServer->StopListening();
                g_jsonrpcIpcServer.reset( nullptr );
            }
            if ( g_client ) {
                g_client->stopWorking();
            }
        }
        skutils::signal::g_bStop = true;
        std::string strMessagePrefix = stopWasRaisedBefore ?
                                           cc::error( "\nStop flag was already raised on. " ) +
                                               cc::fatal( "WILL FORCE TERMINATE." ) +
                                               cc::error( " Caught (second) signal. " ) :
                                           cc::error( "\nCaught (first) signal. " );
        std::cerr << strMessagePrefix << cc::error( skutils::signal::signal2str( nSignalNo ) )
                  << "\n";
        std::cerr.flush();
        std::cout << "\n" << skutils::signal::generate_stack_trace() << "\n\n";
        dev::ExitHandler::exitHandler( nSignalNo );
        if ( stopWasRaisedBefore )
            _exit( 13 );
    } );


    // Init secp256k1 context by calling one of the functions.
    toPublic( {} );

    // Init defaults
    Defaults::get();
    Ethash::init();
    NoProof::init();

    /// Operating mode.
    OperationMode mode = OperationMode::Node;

    /// File name for import/export.
    string filename;
    bool safeImport = false;

    /// Hashes/numbers for export range.
    string exportFrom = "1";
    string exportTo = "latest";
    Format exportFormat = Format::Binary;

    /// General params for Node operation
    NodeMode nodeMode = NodeMode::Full;

    bool is_ipc = false;
    int nExplicitPortHTTP4 = -1;
    int nExplicitPortHTTP6 = -1;
    int nExplicitPortHTTPS4 = -1;
    int nExplicitPortHTTPS6 = -1;
    int nExplicitPortWS4 = -1;
    int nExplicitPortWS6 = -1;
    int nExplicitPortWSS4 = -1;
    int nExplicitPortWSS6 = -1;
    bool bTraceJsonRpcCalls = false;
    bool bEnabledDebugBehaviorAPIs = false;

    const std::list< std::pair< std::string, std::string > >& listIfaceInfos4 =
        get_machine_ip_addresses_4();  // IPv4
    const std::list< std::pair< std::string, std::string > >& listIfaceInfos6 =
        get_machine_ip_addresses_6();  // IPv6

    string strJsonAdminSessionKey;
    ChainParams chainParams;
    string privateChain;

    bool upnp = true;
    WithExisting withExisting = WithExisting::Trust;

    /// Networking params.
    string listenIP;
    unsigned short listenPort = 30303;
    string publicIP;
    string remoteHost;
    static const unsigned NoNetworkID = static_cast< unsigned int >( -1 );
    unsigned networkID = NoNetworkID;

    /// Mining params
    strings presaleImports;

    /// Transaction params
    bool alwaysConfirm = true;

    /// Wallet password stuff
    string masterPassword;
    bool masterSet = false;

    strings passwordsToNote;
    Secrets toImport;

    MinerCLI m( MinerCLI::OperationMode::None );

    fs::path configPath;
    string configJSON;

    po::options_description clientDefaultMode( "CLIENT MODE (default)", c_lineWidth );
    auto addClientOption = clientDefaultMode.add_options();
    addClientOption( "web3-shutdown",
        "Enable programmatic shutdown via \"skale_shutdownInstance\" web3 methd call" );
    addClientOption( "ssl-key", po::value< std::string >()->value_name( "<path>" ),
        "Specifies path to SSL key file" );
    addClientOption( "ssl-cert", po::value< std::string >()->value_name( "<path>" ),
        "Specifies path to SSL certificate file file" );

    /// skale
    addClientOption( "aa", po::value< string >()->value_name( "<yes/no/always>" ),
        "Auto-auth; automatic answer to all authentication questions" );

    addClientOption( "skale", "Use the Skale net" );

    addClientOption( "config", po::value< string >()->value_name( "<file>" ),
        "Configure specialised blockchain using given JSON information\n" );
    addClientOption( "ipc", "Enable IPC server (default: on)" );
    addClientOption( "ipcpath", po::value< string >()->value_name( "<path>" ),
        "Set .ipc socket path (default: data directory)" );
    addClientOption( "no-ipc", "Disable IPC server" );

    addClientOption( "http-port", po::value< string >()->value_name( "<port>" ),
        "Run web3 HTTP(IPv4) server(s) on specified port(and next set of ports if --acceptors > "
        "1)" );
    addClientOption( "https-port", po::value< string >()->value_name( "<port>" ),
        "Run web3 HTTPS(IPv4) server(s) on specified port(and next set of ports if --acceptors > "
        "1)" );
    addClientOption( "ws-port", po::value< string >()->value_name( "<port>" ),
        "Run web3 WS(IPv4) server on specified port(and next set of ports if --acceptors > 1)" );
    addClientOption( "wss-port", po::value< string >()->value_name( "<port>" ),
        "Run web3 WSS(IPv4) server(s) on specified port(and next set of ports if --acceptors > "
        "1)" );

    addClientOption( "http-port6", po::value< string >()->value_name( "<port>" ),
        "Run web3 HTTP(IPv6) server(s) on specified port(and next set of ports if --acceptors > "
        "1)" );
    addClientOption( "https-port6", po::value< string >()->value_name( "<port>" ),
        "Run web3 HTTPS(IPv6) server(s) on specified port(and next set of ports if --acceptors > "
        "1)" );
    addClientOption( "ws-port6", po::value< string >()->value_name( "<port>" ),
        "Run web3 WS(IPv6) server on specified port(and next set of ports if --acceptors > 1)" );
    addClientOption( "wss-port6", po::value< string >()->value_name( "<port>" ),
        "Run web3 WSS(IPv6) server(s) on specified port(and next set of ports if --acceptors > "
        "1)" );

    std::string strPerformanceWarningDurationOptionDescription =
        "Specifies time margin in floating point format, in seconds, for displaying performance "
        "warning messages in log output if JSON RPC call processing exeeds it, default is " +
        std::to_string(
            SkaleServerOverride::g_lfDefaultExecutionDurationMaxForPerformanceWarning ) +
        " seconds";
    addClientOption( "performance-warning-duration",
        po::value< double >()->value_name( "<seconds>" ),
        strPerformanceWarningDurationOptionDescription.c_str() );

    addClientOption( "performance-timeline-enable",
        "Enable performance timeline tracker and corresponding JSON RPC APIs" );
    addClientOption( "performance-timeline-disable",
        "Disabled performance timeline tracker and corresponding JSON RPC APIs" );
    addClientOption( "performance-timeline-max-items",
        po::value< size_t >()->value_name( "<number>" ),
        "Specifies max number of items performance timeline tracker can save" );

    std::string str_ws_mode_description =
        "Run web3 WS and/or WSS server(s) using specified mode(" +
        skutils::ws::nlws::list_srvmodes_as_str() + "); default mode is " +
        skutils::ws::nlws::srvmode2str( skutils::ws::nlws::g_default_srvmode );
    addClientOption(
        "ws-mode", po::value< string >()->value_name( "<mode>" ), str_ws_mode_description.c_str() );
    addClientOption( "ws-log", po::value< string >()->value_name( "<mode>" ),
        "Web socket debug logging mode(none, basic detailed; default is none)" );
    addClientOption( "max-connections", po::value< size_t >()->value_name( "<count>" ),
        "Max number of RPC connections(such as web3) summary for all protocols(0 is default and "
        "means unlimited)" );
    addClientOption( "max-http-queues", po::value< size_t >()->value_name( "<count>" ),
        "Max number of handler queues for HTTP/S connections per endpoint server" );
    addClientOption(
        "async-http-transfer-mode", "Use asynchronous HTTP(S) query handling, default mode" );
    addClientOption( "sync-http-transfer-mode", "Use synchronous HTTP(S) query handling" );

    addClientOption( "acceptors", po::value< size_t >()->value_name( "<count>" ),
        "Number of parallel RPC connection(such as web3) acceptor threads per protocol(1 is "
        "default and "
        "minimal)" );
    addClientOption( "web3-trace", "Log HTTP/HTTPS/WS/WSS requests and responses" );
    addClientOption( "enable-debug-behavior-apis",
        "Enables debug set of JSON RPC APIs which are changing app behavior" );

    addClientOption( "max-batch", po::value< size_t >()->value_name( "<count>" ),
        "Maximum count of requests in JSON RPC batch request array" );

    addClientOption( "admin", po::value< string >()->value_name( "<password>" ),
        "Specify admin session key for JSON-RPC (default: auto-generated and printed at "
        "start-up)" );
    addClientOption( "kill,K", "Kill the blockchain first" );
    addClientOption( "rebuild,R", "Rebuild the blockchain from the existing database" );
    addClientOption( "rescue", "Attempt to rescue a corrupt database\n" );
    addClientOption( "import-presale", po::value< string >()->value_name( "<file>" ),
        "Import a pre-sale key; you'll need to specify the password to this key" );
    addClientOption( "import-secret,s", po::value< string >()->value_name( "<secret>" ),
        "Import a secret key into the key store" );
    addClientOption( "import-session-secret,S", po::value< string >()->value_name( "<secret>" ),
        "Import a secret session into the key store" );
    addClientOption( "master", po::value< string >()->value_name( "<password>" ),
        "Give the master password for the key store; use --master \"\" to show a prompt" );
    addClientOption( "password", po::value< string >()->value_name( "<password>" ),
        "Give a password for a private key\n" );

    po::options_description clientTransacting( "CLIENT TRANSACTING", c_lineWidth );
    auto addTransactingOption = clientTransacting.add_options();
    addTransactingOption( "unsafe-transactions",
        "Allow all transactions to proceed without verification; EXTREMELY UNSAFE\n" );

    po::options_description clientNetworking( "CLIENT NETWORKING", c_lineWidth );
    auto addNetworkingOption = clientNetworking.add_options();
    addNetworkingOption( "public-ip", po::value< string >()->value_name( "<ip>" ),
        "Force advertised public IP to the given IP (default: auto)" );
    addNetworkingOption( "remote,r", po::value< string >()->value_name( "<host>(:<port>)" ),
        "Connect to the given remote host (default: none)" );
    addNetworkingOption( "network-id", po::value< unsigned >()->value_name( "<n>" ),
        "Only connect to other hosts with this network id" );
#if ETH_MINIUPNPC
    addNetworkingOption(
        "upnp", po::value< string >()->value_name( "<on/off>" ), "Use UPnP for NAT (default: on)" );
#endif

    // skale - snapshot download command
    addClientOption( "download-snapshot", po::value< string >()->value_name( "<url>" ),
        "Download snapshot from other skaled node specified by web3/json-rpc url" );
    // addClientOption( "download-target", po::value< string >()->value_name( "<port>" ),
    //    "Path of file to save downloaded snapshot to" );
    addClientOption( "public-key",
        po::value< std::string >()->value_name( "<libff::alt_bn128_G2>" ),
        "Collects old common public key from chain to verify snapshot before starts from it" );
    addClientOption( "start-timestamp", po::value< time_t >()->value_name( "<seconds>" ),
        "Start at specified timestamp (since epoch) - usually after downloading a snapshot" );

    LoggingOptions loggingOptions;
    po::options_description loggingProgramOptions(
        createLoggingProgramOptions( c_lineWidth, loggingOptions ) );

    po::options_description generalOptions( "GENERAL OPTIONS", c_lineWidth );
    auto addGeneralOption = generalOptions.add_options();
    addGeneralOption( "db-path,d", po::value< string >()->value_name( "<path>" ),
        ( "Load database from path (default: " + getDataDir().string() + ")" ).c_str() );
    addGeneralOption( "bls-key-file", po::value< string >()->value_name( "<file>" ),
        "Load BLS keys from file (default: none)" );
    addGeneralOption( "colors", "Use ANSI colorized output and logging" );
    addGeneralOption( "log-value-size-limit",
        po::value< size_t >()->value_name( "<size in bytes>" ),
        "Log value size limit(zero means unlimited)" );
    addGeneralOption( "no-colors", "Use output and logging without colors" );
    addGeneralOption( "version,V", "Show the version and exit" );
    addGeneralOption( "help,h", "Show this help message and exit\n" );

    po::options_description vmOptions = vmProgramOptions( c_lineWidth );


    po::options_description allowedOptions( "Allowed options" );
    allowedOptions.add( clientDefaultMode )
        .add( clientTransacting )
        .add( clientNetworking )
        .add( vmOptions )
        .add( loggingProgramOptions )
        .add( generalOptions );

    po::variables_map vm;
    vector< string > unrecognisedOptions;
    try {
        po::parsed_options parsed = po::command_line_parser( argc, argv )
                                        .options( allowedOptions )
                                        .allow_unregistered()
                                        .run();
        unrecognisedOptions = collect_unrecognized( parsed.options, po::include_positional );
        removeEmptyOptions( parsed );
        po::store( parsed, vm );
        po::notify( vm );
    } catch ( po::error const& e ) {
        cerr << e.what();
        return EX_USAGE;
    }
    for ( size_t i = 0; i < unrecognisedOptions.size(); ++i )
        if ( !m.interpretOption( i, unrecognisedOptions ) ) {
            cerr << "Invalid argument: " << unrecognisedOptions[i] << "\n";
            return EX_USAGE;
        }

    if ( vm.count( "no-colors" ) )
        cc::_on_ = false;
    if ( vm.count( "colors" ) )
        cc::_on_ = true;
    if ( vm.count( "version" ) ) {
        version();
        return 0;
    }
    if ( vm.count( "help" ) ) {
        cout << "NAME:\n"
             << "   skaled " << Version << '\n'
             << "USAGE:\n"
             << "   skaled [options]\n\n";
        cout << clientDefaultMode << clientTransacting << clientNetworking;
        cout << vmOptions << loggingProgramOptions << generalOptions;
        return 0;
    }
    if ( vm.count( "log-value-size-limit" ) ) {
        int n = vm["log-value-size-limit"].as< size_t >();
        cc::_max_value_size_ = ( n > 0 ) ? n : std::string::npos;
    }

    cout << std::endl << "skaled " << Version << std::endl << std::endl;

    pid_t this_process_pid = getpid();
    std::cout << cc::debug( "This process " ) << cc::info( "PID" ) << cc::debug( "=" )
              << cc::size10( size_t( this_process_pid ) ) << std::endl;

    setupLogging( loggingOptions );

    skutils::dispatch::default_domain( skutils::tools::cpu_count() * 2 );
    // skutils::dispatch::default_domain( 48 );

    if ( vm.count( "import-snapshot" ) ) {
        mode = OperationMode::ImportSnapshot;
        filename = vm["import-snapshot"].as< string >();
    }

    bool chainConfigIsSet = false, chainConfigParsed = false;
    static nlohmann::json joConfig;

    if ( vm.count( "import-presale" ) )
        presaleImports.push_back( vm["import-presale"].as< string >() );
    if ( vm.count( "admin" ) )
        strJsonAdminSessionKey = vm["admin"].as< string >();

    if ( vm.count( "skale" ) ) {
        chainParams = ChainParams( genesisInfo( eth::Network::Skale ) );
        chainConfigIsSet = true;
    }

    if ( vm.count( "config" ) ) {
        try {
            configPath = vm["config"].as< string >();
            if ( !fs::is_regular_file( configPath.string() ) )
                throw "Bad config file path";
            configJSON = contentsString( configPath.string() );
            if ( configJSON.empty() )
                throw "Config file probably not found";
            chainParams = chainParams.loadConfig( configJSON, configPath );
            chainConfigIsSet = true;
            // TODO avoid double-parse!!
            joConfig = nlohmann::json::parse( configJSON );
            chainConfigParsed = true;
            dev::eth::g_configAccesssor.reset(
                new skutils::json_config_file_accessor( configPath.string() ) );
        } catch ( const char* str ) {
            cerr << "Error: " << str << ": " << configPath << "\n";
            return EX_USAGE;
        } catch ( const json_spirit::Error_position& err ) {
            cerr << "error in parsing config json:\n";
            cerr << configJSON << endl;
            cerr << err.reason_ << " line " << err.line_ << endl;
            return EX_CONFIG;
        } catch ( const std::exception& ex ) {
            cerr << "provided configuration is incorrect\n";
            cerr << configJSON << endl;
            cerr << nested_exception_what( ex ) << endl;
            return EX_CONFIG;
        } catch ( ... ) {
            cerr << "provided configuration is incorrect\n";
            // cerr << "sample: \n" << genesisInfo(eth::Network::MainNetworkTest) << "\n";
            cerr << configJSON << endl;
            return EX_CONFIG;
        }
    }

    if ( !chainConfigIsSet )
        // default to skale if not already set with `--config`
        chainParams = ChainParams( genesisInfo( eth::Network::Skale ) );

    // First, get "ipc" true/false from config.json
    // Second, get it from command line parameter (higher priority source)
    if ( chainConfigParsed ) {
        is_ipc = false;
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "ipc" ) )
                is_ipc = joConfig["skaleConfig"]["nodeInfo"]["ipc"].get< bool >();
        } catch ( ... ) {
        }
    }
    if ( vm.count( "ipc" ) )
        is_ipc = true;
    if ( vm.count( "no-ipc" ) )
        is_ipc = false;
    clog( VerbosityInfo, "main" ) << cc::notice( "IPC server" ) + cc::debug( " is: " )
                                  << ( is_ipc ? cc::success( "on" ) : cc::error( "off" ) );

    // First, get "httpRpcPort", "httpsRpcPort", "wsRpcPort" and "wssRpcPort" from config.json
    // Second, get them from command line parameters (higher priority source)
    if ( chainConfigParsed ) {
        nExplicitPortHTTP4 = -1;
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "httpRpcPort" ) )
                nExplicitPortHTTP4 =
                    joConfig["skaleConfig"]["nodeInfo"]["httpRpcPort"].get< int >();
        } catch ( ... ) {
        }

        if ( !( 0 <= nExplicitPortHTTP4 && nExplicitPortHTTP4 <= 65535 ) )
            nExplicitPortHTTP4 = -1;
        else
            clog( VerbosityInfo, "main" )
                << cc::debug( "Got " )
                << cc::notice( "HTTP/4 port" ) + cc::debug( " from configuration JSON: " )
                << cc::num10( nExplicitPortHTTP4 );
        //
        nExplicitPortHTTP6 = -1;
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "httpRpcPort6" ) )
                nExplicitPortHTTP6 =
                    joConfig["skaleConfig"]["nodeInfo"]["httpRpcPort6"].get< int >();
        } catch ( ... ) {
        }

        if ( !( 0 <= nExplicitPortHTTP6 && nExplicitPortHTTP6 <= 65535 ) )
            nExplicitPortHTTP6 = nExplicitPortHTTP4;
        if ( !( 0 <= nExplicitPortHTTP6 && nExplicitPortHTTP6 <= 65535 ) )
            nExplicitPortHTTP6 = -1;
        else
            clog( VerbosityInfo, "main" )
                << cc::debug( "Got " )
                << cc::notice( "HTTP/6 port" ) + cc::debug( " from configuration JSON: " )
                << cc::num10( nExplicitPortHTTP6 );
        //
        //
        nExplicitPortHTTPS4 = -1;
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "httpsRpcPort" ) )
                nExplicitPortHTTPS4 =
                    joConfig["skaleConfig"]["nodeInfo"]["httpsRpcPort"].get< int >();
        } catch ( ... ) {
        }

        if ( !( 0 <= nExplicitPortHTTPS4 && nExplicitPortHTTPS4 <= 65535 ) )
            nExplicitPortHTTPS4 = -1;
        else
            clog( VerbosityInfo, "main" )
                << cc::debug( "Got " )
                << cc::notice( "HTTPS/4 port" ) + cc::debug( " from configuration JSON: " )
                << cc::num10( nExplicitPortHTTPS4 );
        //
        nExplicitPortHTTPS6 = -1;
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "httpsRpcPort6" ) )
                nExplicitPortHTTPS6 =
                    joConfig["skaleConfig"]["nodeInfo"]["httpsRpcPort6"].get< int >();
        } catch ( ... ) {
        }

        if ( !( 0 <= nExplicitPortHTTPS6 && nExplicitPortHTTPS6 <= 65535 ) )
            nExplicitPortHTTPS6 = nExplicitPortHTTPS4;
        if ( !( 0 <= nExplicitPortHTTPS6 && nExplicitPortHTTPS6 <= 65535 ) )
            nExplicitPortHTTPS6 = -1;
        else
            clog( VerbosityInfo, "main" )
                << cc::debug( "Got " )
                << cc::notice( "HTTPS/6 port" ) + cc::debug( " from configuration JSON: " )
                << cc::num10( nExplicitPortHTTPS6 );
        //
        //
        nExplicitPortWS4 = -1;
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "wsRpcPort" ) )
                nExplicitPortWS4 = joConfig["skaleConfig"]["nodeInfo"]["wsRpcPort"].get< int >();
        } catch ( ... ) {
        }
        if ( !( 0 <= nExplicitPortWS4 && nExplicitPortWS4 <= 65535 ) )
            nExplicitPortWS4 = -1;
        else
            clog( VerbosityInfo, "main" )
                << cc::debug( "Got " )
                << cc::notice( "WS/4 port" ) + cc::debug( " from configuration JSON: " )
                << cc::num10( nExplicitPortWS4 );
        //
        nExplicitPortWS6 = -1;
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "wsRpcPort6" ) )
                nExplicitPortWS6 = joConfig["skaleConfig"]["nodeInfo"]["wsRpcPort6"].get< int >();
        } catch ( ... ) {
        }
        if ( !( 0 <= nExplicitPortWS6 && nExplicitPortWS6 <= 65535 ) )
            nExplicitPortWS6 = nExplicitPortWS4;
        if ( !( 0 <= nExplicitPortWS6 && nExplicitPortWS6 <= 65535 ) )
            nExplicitPortWS6 = -1;
        else
            clog( VerbosityInfo, "main" )
                << cc::debug( "Got " )
                << cc::notice( "WS/6 port" ) + cc::debug( " from configuration JSON: " )
                << cc::num10( nExplicitPortWS6 );
        //
        //
        nExplicitPortWSS4 = -1;
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "wssRpcPort" ) )
                nExplicitPortWSS4 = joConfig["skaleConfig"]["nodeInfo"]["wssRpcPort"].get< int >();
        } catch ( ... ) {
        }
        if ( !( 0 <= nExplicitPortWSS4 && nExplicitPortWSS4 <= 65535 ) )
            nExplicitPortWSS4 = -1;
        else
            clog( VerbosityInfo, "main" )
                << cc::debug( "Got " )
                << cc::notice( "WSS/4 port" ) + cc::debug( " from configuration JSON: " )
                << cc::num10( nExplicitPortWSS4 );
        //
        nExplicitPortWSS6 = -1;
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "wssRpcPort6" ) )
                nExplicitPortWSS6 = joConfig["skaleConfig"]["nodeInfo"]["wssRpcPort6"].get< int >();
        } catch ( ... ) {
        }
        if ( !( 0 <= nExplicitPortWSS6 && nExplicitPortWSS6 <= 65535 ) )
            nExplicitPortWSS6 = nExplicitPortWSS4;
        if ( !( 0 <= nExplicitPortWSS6 && nExplicitPortWSS6 <= 65535 ) )
            nExplicitPortWSS6 = -1;
        else
            clog( VerbosityInfo, "main" )
                << cc::debug( "Got " )
                << cc::notice( "WSS/6 port" ) + cc::debug( " from configuration JSON: " )
                << cc::num10( nExplicitPortWSS6 );
    }  // if ( chainConfigParsed )
    if ( vm.count( "http-port" ) ) {
        std::string strPort = vm["http-port"].as< string >();
        if ( !strPort.empty() ) {
            nExplicitPortHTTP4 = atoi( strPort.c_str() );
            if ( !( 0 <= nExplicitPortHTTP4 && nExplicitPortHTTP4 <= 65535 ) )
                nExplicitPortHTTP4 = -1;
            else
                clog( VerbosityInfo, "main" )
                    << cc::debug( "Got " )
                    << cc::notice( "HTTP/4 port" ) + cc::debug( " from command line: " )
                    << cc::num10( nExplicitPortHTTP4 );
        }
    }
    if ( vm.count( "http-port6" ) ) {
        std::string strPort = vm["http-port6"].as< string >();
        if ( !strPort.empty() ) {
            nExplicitPortHTTP6 = atoi( strPort.c_str() );
            if ( !( 0 <= nExplicitPortHTTP6 && nExplicitPortHTTP6 <= 65535 ) )
                nExplicitPortHTTP6 = -1;
            else
                clog( VerbosityInfo, "main" )
                    << cc::debug( "Got " )
                    << cc::notice( "HTTP/6 port" ) + cc::debug( " from command line: " )
                    << cc::num10( nExplicitPortHTTP6 );
        }
    }
    if ( vm.count( "https-port" ) ) {
        std::string strPort = vm["https-port"].as< string >();
        if ( !strPort.empty() ) {
            nExplicitPortHTTPS4 = atoi( strPort.c_str() );
            if ( !( 0 <= nExplicitPortHTTPS4 && nExplicitPortHTTPS4 <= 65535 ) )
                nExplicitPortHTTPS4 = -1;
            else
                clog( VerbosityInfo, "main" )
                    << cc::debug( "Got " )
                    << cc::notice( "HTTPS/4 port" ) + cc::debug( " from command line: " )
                    << cc::num10( nExplicitPortHTTPS4 );
        }
    }
    if ( vm.count( "https-port6" ) ) {
        std::string strPort = vm["https-port6"].as< string >();
        if ( !strPort.empty() ) {
            nExplicitPortHTTPS6 = atoi( strPort.c_str() );
            if ( !( 0 <= nExplicitPortHTTPS6 && nExplicitPortHTTPS6 <= 65535 ) )
                nExplicitPortHTTPS6 = -1;
            else
                clog( VerbosityInfo, "main" )
                    << cc::debug( "Got " )
                    << cc::notice( "HTTPS/6 port" ) + cc::debug( " from command line: " )
                    << cc::num10( nExplicitPortHTTPS6 );
        }
    }
    if ( vm.count( "ws-port" ) ) {
        std::string strPort = vm["ws-port"].as< string >();
        if ( !strPort.empty() ) {
            nExplicitPortWS4 = atoi( strPort.c_str() );
            if ( !( 0 <= nExplicitPortWS4 && nExplicitPortWS4 <= 65535 ) )
                nExplicitPortWS4 = -1;
            else
                clog( VerbosityInfo, "main" )
                    << cc::debug( "Got " )
                    << cc::notice( "WS/4 port" ) + cc::debug( " from command line: " )
                    << cc::num10( nExplicitPortWS4 );
        }
    }
    if ( vm.count( "ws-port6" ) ) {
        std::string strPort = vm["ws-port6"].as< string >();
        if ( !strPort.empty() ) {
            nExplicitPortWS6 = atoi( strPort.c_str() );
            if ( !( 0 <= nExplicitPortWS6 && nExplicitPortWS6 <= 65535 ) )
                nExplicitPortWS6 = -1;
            else
                clog( VerbosityInfo, "main" )
                    << cc::debug( "Got " )
                    << cc::notice( "WS/6 port" ) + cc::debug( " from command line: " )
                    << cc::num10( nExplicitPortWS6 );
        }
    }
    if ( vm.count( "wss-port" ) ) {
        std::string strPort = vm["wss-port"].as< string >();
        if ( !strPort.empty() ) {
            nExplicitPortWSS4 = atoi( strPort.c_str() );
            if ( !( 0 <= nExplicitPortWSS4 && nExplicitPortWSS4 <= 65535 ) )
                nExplicitPortWSS4 = -1;
            else
                clog( VerbosityInfo, "main" )
                    << cc::debug( "Got " )
                    << cc::notice( "WSS/4 port" ) + cc::debug( " from command line: " )
                    << cc::num10( nExplicitPortWSS4 );
        }
    }
    if ( vm.count( "wss-port6" ) ) {
        std::string strPort = vm["wss-port6"].as< string >();
        if ( !strPort.empty() ) {
            nExplicitPortWSS6 = atoi( strPort.c_str() );
            if ( !( 0 <= nExplicitPortWSS6 && nExplicitPortWSS6 <= 65535 ) )
                nExplicitPortWSS6 = -1;
            else
                clog( VerbosityInfo, "main" )
                    << cc::debug( "Got " )
                    << cc::notice( "WSS/6 port" ) + cc::debug( " from command line: " )
                    << cc::num10( nExplicitPortWSS6 );
        }
    }

    // First, get "web3-trace" from config.json
    // Second, get it from command line parameter (higher priority source)
    if ( chainConfigParsed ) {
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "web3-trace" ) )
                bTraceJsonRpcCalls =
                    joConfig["skaleConfig"]["nodeInfo"]["web3-trace"].get< bool >();
        } catch ( ... ) {
        }
    }
    if ( vm.count( "web3-trace" ) )
        bTraceJsonRpcCalls = true;
    clog( VerbosityInfo, "main" ) << cc::info( "JSON RPC" )
                                  << cc::debug( " trace logging mode is " )
                                  << cc::flag_ed( bTraceJsonRpcCalls );

    // First, get "enable-debug-behavior-apis" from config.json
    // Second, get it from command line parameter (higher priority source)
    if ( chainConfigParsed ) {
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "enable-debug-behavior-apis" ) )
                bEnabledDebugBehaviorAPIs =
                    joConfig["skaleConfig"]["nodeInfo"]["enable-debug-behavior-apis"].get< bool >();
        } catch ( ... ) {
        }
    }
    if ( vm.count( "enable-debug-behavior-apis" ) )
        bEnabledDebugBehaviorAPIs = true;
    clog( VerbosityInfo, "main" ) << cc::warn( "Important notce: " ) << cc::debug( "Programmatic " )
                                  << cc::info( "enable-debug-behavior-apis" )
                                  << cc::debug( " mode is " )
                                  << cc::flag_ed( bEnabledDebugBehaviorAPIs );

    // First, get "unsafe-transactions" from config.json
    // Second, get it from command line parameter (higher priority source)
    if ( chainConfigParsed ) {
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "unsafe-transactions" ) )
                alwaysConfirm =
                    !joConfig["skaleConfig"]["nodeInfo"]["unsafe-transactions"].get< bool >();
        } catch ( ... ) {
        }
    }
    if ( vm.count( "unsafe-transactions" ) )
        alwaysConfirm = false;
    clog( VerbosityInfo, "main" ) << cc::warn( "Important notce: " ) << cc::debug( "Programmatic " )
                                  << cc::info( "unsafe-transactions" ) << cc::debug( " mode is " )
                                  << cc::flag_ed( !alwaysConfirm );

    // First, get "web3-shutdown" from config.json
    // Second, get it from command line parameter (higher priority source)
    bool bEnabledShutdownViaWeb3 = false;
    if ( chainConfigParsed ) {
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "web3-shutdown" ) )
                bEnabledShutdownViaWeb3 =
                    joConfig["skaleConfig"]["nodeInfo"]["web3-shutdown"].get< bool >();
        } catch ( ... ) {
        }
    }
    if ( vm.count( "web3-shutdown" ) )
        bEnabledShutdownViaWeb3 = true;
    clog( VerbosityInfo, "main" ) << cc::warn( "Important notce: " ) << cc::debug( "Programmatic " )
                                  << cc::info( "web3-shutdown" ) << cc::debug( " mode is " )
                                  << cc::flag_ed( bEnabledShutdownViaWeb3 );

    // First, get "ipcpath" from config.json
    // Second, get it from command line parameter (higher priority source)
    std::string strPathIPC;
    if ( chainConfigParsed ) {
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "ipcpath" ) )
                strPathIPC = joConfig["skaleConfig"]["nodeInfo"]["ipcpath"].get< std::string >();
        } catch ( ... ) {
        }
    }
    clog( VerbosityInfo, "main" ) << cc::notice( "IPC path" ) + cc::debug( " is: " )
                                  << cc::p( strPathIPC );
    if ( vm.count( "ipcpath" ) )
        strPathIPC = vm["ipcpath"].as< std::string >();
    if ( !strPathIPC.empty() )
        setIpcPath( strPathIPC );

    // First, get "db-path"" from config.json
    // Second, get it from command line parameter (higher priority source)
    std::string strPathDB;
    if ( chainConfigParsed ) {
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "db-path" ) )
                strPathDB = joConfig["skaleConfig"]["nodeInfo"]["db-path"].get< std::string >();
        } catch ( ... ) {
        }
    }
    if ( vm.count( "db-path" ) )
        strPathDB = vm["db-path"].as< std::string >();
    clog( VerbosityInfo, "main" ) << cc::notice( "DB path" ) + cc::debug( " is: " )
                                  << cc::p( strPathDB );
    if ( !strPathDB.empty() )
        setDataDir( strPathDB );

    ///////////////// CACHE PARAMS ///////////////
    extern chrono::system_clock::duration c_collectionDuration;
    extern unsigned c_collectionQueueSize;
    extern unsigned c_maxCacheSize;
    extern unsigned c_minCacheSize;

    unsigned c_transactionQueueSize = 100000;

    if ( chainConfigParsed ) {
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "minCacheSize" ) )
                c_minCacheSize =
                    joConfig["skaleConfig"]["nodeInfo"]["minCacheSize"].get< unsigned >();
        } catch ( ... ) {
        }

        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "maxCacheSize" ) )
                c_maxCacheSize =
                    joConfig["skaleConfig"]["nodeInfo"]["maxCacheSize"].get< unsigned >();
        } catch ( ... ) {
        }

        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "collectionQueueSize" ) )
                c_collectionQueueSize =
                    joConfig["skaleConfig"]["nodeInfo"]["collectionQueueSize"].get< unsigned >();
        } catch ( ... ) {
        }

        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "collectionDuration" ) )
                c_collectionDuration = chrono::seconds(
                    joConfig["skaleConfig"]["nodeInfo"]["collectionDuration"].get< unsigned >() );
        } catch ( ... ) {
        }

        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "transactionQueueSize" ) )
                c_transactionQueueSize =
                    joConfig["skaleConfig"]["nodeInfo"]["transactionQueueSize"].get< unsigned >();
        } catch ( ... ) {
        }

        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "maxOpenLeveldbFiles" ) )
                dev::db::c_maxOpenLeveldbFiles =
                    joConfig["skaleConfig"]["nodeInfo"]["maxOpenLeveldbFiles"].get< unsigned >();
        } catch ( ... ) {
        }
    }
    ////////////// END CACHE PARAMS ////////////

    if ( vm.count( "public-ip" ) ) {
        publicIP = vm["public-ip"].as< string >();
    }
    if ( vm.count( "remote" ) ) {
        string host = vm["remote"].as< string >();
        string::size_type found = host.find_first_of( ':' );
        if ( found != std::string::npos ) {
            remoteHost = host.substr( 0, found );
        } else
            remoteHost = host;
    }
    if ( vm.count( "import" ) ) {
        mode = OperationMode::Import;
        filename = vm["import"].as< string >();
    }
    if ( vm.count( "export" ) ) {
        mode = OperationMode::Export;
        filename = vm["export"].as< string >();
    }
    if ( vm.count( "password" ) )
        passwordsToNote.push_back( vm["password"].as< string >() );
    if ( vm.count( "master" ) ) {
        masterPassword = vm["master"].as< string >();
        masterSet = true;
    }
    if ( vm.count( "dont-check" ) )
        safeImport = true;
    if ( vm.count( "format" ) ) {
        string m = vm["format"].as< string >();
        if ( m == "binary" )
            exportFormat = Format::Binary;
        else if ( m == "hex" )
            exportFormat = Format::Hex;
        else if ( m == "human" )
            exportFormat = Format::Human;
        else {
            cerr << "Bad "
                 << "--format"
                 << " option: " << m << "\n";
            return EX_USAGE;
        }
    }
    if ( vm.count( "to" ) )
        exportTo = vm["to"].as< string >();
    if ( vm.count( "from" ) )
        exportFrom = vm["from"].as< string >();
    if ( vm.count( "only" ) )
        exportTo = exportFrom = vm["only"].as< string >();
#if ETH_MINIUPNPC
    if ( vm.count( "upnp" ) ) {
        string m = vm["upnp"].as< string >();
        if ( isTrue( m ) )
            upnp = true;
        else if ( isFalse( m ) )
            upnp = false;
        else {
            cerr << "Bad "
                 << "--upnp"
                 << " option: " << m << "\n";
            return EX_USAGE;
        }
    }
#endif
    if ( vm.count( "network-id" ) )
        try {
            networkID = vm["network-id"].as< unsigned >();
        } catch ( ... ) {
            cerr << "Bad "
                 << "--network-id"
                 << " option: " << vm["network-id"].as< string >() << "\n";
            return EX_USAGE;
        }
    if ( vm.count( "kill" ) )
        withExisting = WithExisting::Kill;
    if ( vm.count( "rebuild" ) )
        withExisting = WithExisting::Verify;
    if ( vm.count( "rescue" ) )
        withExisting = WithExisting::Rescue;
    if ( ( vm.count( "import-secret" ) ) ) {
        Secret s( fromHex( vm["import-secret"].as< string >() ) );
        toImport.emplace_back( s );
    }
    if ( vm.count( "import-session-secret" ) ) {
        Secret s( fromHex( vm["import-session-secret"].as< string >() ) );
        toImport.emplace_back( s );
    }

    std::shared_ptr< SnapshotManager > snapshotManager;
    if ( chainParams.sChain.snapshotIntervalSec > 0 || vm.count( "download-snapshot" ) )
        snapshotManager.reset( new SnapshotManager(
            getDataDir(), {BlockChain::getChainDirName( chainParams ), "filestorage",
                              "prices_" + chainParams.nodeInfo.id.str() + ".db",
                              "blocks_" + chainParams.nodeInfo.id.str() + ".db"} ) );

    bool isStartedFromSnapshot = false;
    if ( vm.count( "download-snapshot" ) ) {
        isStartedFromSnapshot = true;
        std::string commonPublicKey = "";
        if ( !vm.count( "public-key" ) ) {
            throw std::runtime_error(
                cc::error( "Missing --public-key option - cannot download snapshot" ) );
        } else {
            commonPublicKey = vm["public-key"].as< std::string >();
        }

        bool successfullDownload = false;

        for ( size_t idx = 0; idx < chainParams.sChain.nodes.size(); ++idx )
            try {
                if ( chainParams.nodeInfo.id == chainParams.sChain.nodes[idx].id )
                    continue;

                std::string blockNumber_url =
                    std::string( "http://" ) + std::string( chainParams.sChain.nodes[idx].ip ) +
                    std::string( ":" ) +
                    ( chainParams.sChain.nodes[idx].port + 3 ).convert_to< std::string >();

                unsigned blockNumber = getLatestSnapshotBlockNumber( blockNumber_url );
                clog( VerbosityInfo, "main" )
                    << cc::notice( "Latest Snapshot Block Number" ) + cc::debug( " is: " )
                    << cc::p( std::to_string( blockNumber ) ) << " (from " << blockNumber_url
                    << ")";

                SnapshotHashAgent snapshotHashAgent( chainParams, commonPublicKey );

                libff::init_alt_bn128_params();
                std::pair< dev::h256, libff::alt_bn128_G1 > voted_hash;
                std::vector< std::string > list_urls_to_download;
                try {
                    list_urls_to_download =
                        snapshotHashAgent.getNodesToDownloadSnapshotFrom( blockNumber );
                    clog( VerbosityInfo, "main" )
                        << cc::notice( "Got urls to download snapshot from " )
                        << cc::p( std::to_string( list_urls_to_download.size() ) )
                        << cc::notice( " nodes " );
                    voted_hash = snapshotHashAgent.getVotedHash();
                } catch ( std::exception& ex ) {
                    std::throw_with_nested( std::runtime_error( cc::error(
                        "Exception while collecting snapshot hash from other skaleds " ) ) );
                }

                bool present = false;
                dev::h256 calculated_hash;

                try {
                    present = snapshotManager->isSnapshotHashPresent( blockNumber );
                    // if there is snapshot but no hash!
                    if ( !present )
                        snapshotManager->removeSnapshot( blockNumber );
                } catch ( const std::exception& ex ) {
                    // usually snapshot absent exception
                    clog( VerbosityInfo, "main" ) << dev::nested_exception_what( ex );
                }

                if ( present ) {
                    clog( VerbosityInfo, "main" )
                        << "Snapshot for block " << blockNumber << " already present locally";

                    calculated_hash = snapshotManager->getSnapshotHash( blockNumber );

                    if ( calculated_hash == voted_hash.first )
                        successfullDownload = true;
                    else
                        snapshotManager->removeSnapshot( blockNumber );
                }

                size_t n_found = list_urls_to_download.size();

                if ( n_found == 0 )
                    continue;
                size_t shift = rand() % n_found;

                for ( size_t cnt = 0; cnt < n_found && !successfullDownload; ++cnt )
                    try {
                        size_t i = ( shift + cnt ) % n_found;

                        std::string urlToDownloadSnapshot;
                        urlToDownloadSnapshot = list_urls_to_download[i];

                        downloadSnapshot(
                            blockNumber, snapshotManager, urlToDownloadSnapshot, chainParams );

                        try {
                            if ( !present )
                                snapshotManager->computeSnapshotHash( blockNumber, true );
                        } catch ( const std::exception& ) {
                            std::throw_with_nested( std::runtime_error(
                                cc::fatal( "FATAL:" ) + " " +
                                cc::error( "Exception while computing snapshot hash " ) ) );
                        }

                        dev::h256 calculated_hash = snapshotManager->getSnapshotHash( blockNumber );

                        if ( calculated_hash == voted_hash.first )
                            successfullDownload = true;
                        else
                            snapshotManager->removeSnapshot( blockNumber );
                    } catch ( const std::exception& ex ) {
                        // just retry
                        clog( VerbosityWarning, "main" ) << dev::nested_exception_what( ex );
                    }

                if ( successfullDownload )
                    break;

            } catch ( std::exception& ex ) {
                clog( VerbosityWarning, "main" )
                    << cc::warn( "Exception while getLatestSnapshotBlockNumber: " )
                    << cc::warn( dev::nested_exception_what( ex ) );
            }  // for blockNumber_url

        if ( !successfullDownload ) {
            throw std::runtime_error( "FATAL: tried to download snapshot from everywhere!" );
        }
    }  // if --download-snapshot

    // it was needed for snapshot downloading
    if ( chainParams.sChain.snapshotIntervalSec <= 0 ) {
        snapshotManager = nullptr;
    }

    time_t startTimestamp = 0;
    if ( vm.count( "start-timestamp" ) ) {
        startTimestamp = vm["start-timestamp"].as< time_t >();
    }

    if ( time( NULL ) < startTimestamp ) {
        std::cout << "\nWill start at localtime " << ctime( &startTimestamp ) << std::endl;
        do
            sleep( 1 );
        while ( time( NULL ) < startTimestamp );
    }

    if ( loggingOptions.verbosity > 0 )
        cout << cc::attention( "skaled, a C++ Skale client" ) << "\n";

    m.execute();

    fs::path secretsPath = SecretStore::defaultPath();
    KeyManager keyManager( KeyManager::defaultPath(), secretsPath );
    for ( auto const& s : passwordsToNote )
        keyManager.notePassword( s );

    string logbuf;
    std::string additional;

    auto getPassword = [&]( string const& prompt ) {
        bool s = g_silence;
        g_silence = true;
        cout << "\n";
        string ret = dev::getPassword( prompt );
        g_silence = s;
        return ret;
    };
    auto getResponse = [&]( string const& prompt, unordered_set< string > const& acceptable ) {
        bool s = g_silence;
        g_silence = true;
        cout << "\n";
        string ret;
        while ( true ) {
            cout << prompt;
            getline( cin, ret );
            if ( acceptable.count( ret ) )
                break;
            cout << "Invalid response: " << ret << "\n";
        }
        g_silence = s;
        return ret;
    };
    auto getAccountPassword = [&]( Address const& a ) {
        return getPassword( "Enter password for address " + keyManager.accountName( a ) + " (" +
                            a.abridged() + "; hint:" + keyManager.passwordHint( a ) + "): " );
    };

    auto netPrefs = publicIP.empty() ? NetworkPreferences( listenIP, listenPort, upnp ) :
                                       NetworkPreferences( publicIP, listenIP, listenPort, upnp );
    netPrefs.discovery = false;
    netPrefs.pin = false;

    auto nodesState = contents( getDataDir() / fs::path( "network.rlp" ) );
    auto caps = set< string >{"eth"};

    ExitHandler exitHandler;

    signal( SIGTERM, &ExitHandler::exitHandler );
    signal( SIGINT, &ExitHandler::exitHandler );

    //    dev::WebThreeDirect web3( WebThreeDirect::composeClientVersion( "skaled" ), getDataDir(),
    //    "",
    //        chainParams, withExisting, nodeMode == NodeMode::Full ? caps : set< string >(), false
    //        );

    std::shared_ptr< GasPricer > gasPricer;

    auto rotationFlagDirPath = configPath.parent_path();
    auto instanceMonitor = make_shared< InstanceMonitor >( rotationFlagDirPath );
    SkaleDebugInterface debugInterface;

    if ( getDataDir().size() )
        Defaults::setDBPath( getDataDir() );
    if ( nodeMode == NodeMode::Full && caps.count( "eth" ) ) {
        Ethash::init();
        NoProof::init();

        if ( chainParams.sealEngineName == Ethash::name() ) {
            g_client.reset( new eth::EthashClient( chainParams, ( int ) chainParams.networkID,
                shared_ptr< GasPricer >(), snapshotManager, instanceMonitor, getDataDir(),
                withExisting, TransactionQueue::Limits{c_transactionQueueSize, 1024},
                isStartedFromSnapshot ) );
        } else if ( chainParams.sealEngineName == NoProof::name() ) {
            g_client.reset( new eth::Client( chainParams, ( int ) chainParams.networkID,
                shared_ptr< GasPricer >(), snapshotManager, instanceMonitor, getDataDir(),
                withExisting, TransactionQueue::Limits{c_transactionQueueSize, 1024},
                isStartedFromSnapshot ) );
        } else
            BOOST_THROW_EXCEPTION( ChainParamsInvalid() << errinfo_comment(
                                       "Unknown seal engine: " + chainParams.sealEngineName ) );

        // XXX nested lambdas and strlen hacks..
        auto client_debug_handler = g_client->getDebugHandler();
        debugInterface.add_handler( [client_debug_handler]( const std::string& arg ) -> string {
            if ( arg.find( "Client " ) == 0 )
                return client_debug_handler( arg.substr( 7 ) );
            else
                return "";
        } );
        g_client->setAuthor( chainParams.sChain.owner );

        DefaultConsensusFactory cons_fact( *g_client );
        setenv( "DATA_DIR", getDataDir().c_str(), 0 );

        std::shared_ptr< SkaleHost > skaleHost =
            std::make_shared< SkaleHost >( *g_client, &cons_fact );

        // XXX nested lambdas and strlen hacks..
        auto skaleHost_debug_handler = skaleHost->getDebugHandler();
        debugInterface.add_handler( [skaleHost_debug_handler]( const std::string& arg ) -> string {
            if ( arg.find( "SkaleHost " ) == 0 )
                return skaleHost_debug_handler( arg.substr( 10 ) );
            else
                return "";
        } );

        gasPricer = std::make_shared< ConsensusGasPricer >( *skaleHost );

        g_client->setGasPricer( gasPricer );
        g_client->injectSkaleHost( skaleHost );

        const auto* buildinfo = skale_get_buildinfo();
        g_client->setExtraData(
            rlpList( 0, string{buildinfo->project_version}.substr( 0, 5 ) + "++" +
                            string{buildinfo->git_commit_hash}.substr( 0, 4 ) +
                            string{buildinfo->build_type}.substr( 0, 1 ) +
                            string{buildinfo->system_name}.substr( 0, 5 ) +
                            string{buildinfo->compiler_id}.substr( 0, 3 ) ) );

        // this must be last! (or client will be mining blocks before this!)
        g_client->startWorking();
    }

    auto toNumber = [&]( string const& s ) -> unsigned {
        if ( s == "latest" )
            return g_client->number();
        if ( s.size() == 64 || ( s.size() == 66 && s.substr( 0, 2 ) == "0x" ) )
            return g_client->blockChain().number( h256( s ) );
        try {
            return static_cast< unsigned int >( stoul( s ) );
        } catch ( ... ) {
            cerr << "Bad block number/hash option: " << s << "\n";
            return static_cast< unsigned int >( -1 );
        }
    };

    if ( mode == OperationMode::Export ) {
        ofstream fout( filename, std::ofstream::binary );
        ostream& out = ( filename.empty() || filename == "--" ) ? cout : fout;

        unsigned last = toNumber( exportTo );
        for ( unsigned i = toNumber( exportFrom ); i <= last; ++i ) {
            bytes block = g_client->blockChain().block( g_client->blockChain().numberHash( i ) );
            switch ( exportFormat ) {
            case Format::Binary:
                out.write( reinterpret_cast< char const* >( block.data() ),
                    std::streamsize( block.size() ) );
                break;
            case Format::Hex:
                out << toHex( block ) << "\n";
                break;
            case Format::Human:
                out << RLP( block ) << "\n";
                break;
            }
        }
        return 0;
    }

    if ( mode == OperationMode::Import ) {
        std::thread th( [&]() {
            dev::setThreadName( "import" );

            ifstream fin( filename, std::ifstream::binary );
            istream& in = ( filename.empty() || filename == "--" ) ? cin : fin;
            unsigned alreadyHave = 0;
            unsigned good = 0;
            unsigned futureTime = 0;
            unsigned unknownParent = 0;
            unsigned bad = 0;
            chrono::steady_clock::time_point t = chrono::steady_clock::now();
            double last = 0;
            unsigned lastImported = 0;
            unsigned imported = 0;

            unsigned block_no = static_cast< unsigned int >( -1 );
            cout << "Skipping " << g_client->syncStatus().currentBlockNumber + 1 << " blocks.\n";
            MICROPROFILE_ENTERI( "main", "bunch 10s", MP_LIGHTGRAY );
            while ( in.peek() != -1 && ( !exitHandler.shouldExit() ) ) {
                bytes block( 8 );
                {
                    if ( block_no >= g_client->number() ) {
                        MICROPROFILE_ENTERI( "main", "in.read", -1 );
                    }
                    in.read( reinterpret_cast< char* >( block.data() ),
                        std::streamsize( block.size() ) );
                    block.resize( RLP( block, RLP::LaissezFaire ).actualSize() );
                    if ( block.size() >= 8 ) {
                        in.read( reinterpret_cast< char* >( block.data() + 8 ),
                            std::streamsize( block.size() ) - 8 );
                        if ( block_no >= g_client->number() ) {
                            MICROPROFILE_LEAVE();
                        }
                    } else {
                        throw std::runtime_error( "Buffer error" );
                    }
                }
                block_no++;

                if ( block_no <= g_client->number() )
                    continue;

                switch ( g_client->queueBlock( block, safeImport ) ) {
                case ImportResult::Success:
                    good++;
                    break;
                case ImportResult::AlreadyKnown:
                    alreadyHave++;
                    break;
                case ImportResult::UnknownParent:
                    unknownParent++;
                    break;
                case ImportResult::FutureTimeUnknown:
                    unknownParent++;
                    futureTime++;
                    break;
                case ImportResult::FutureTimeKnown:
                    futureTime++;
                    break;
                default:
                    bad++;
                    break;
                }

                // sync chain with queue
                tuple< ImportRoute, bool, unsigned > r = g_client->syncQueue( 10 );
                imported += get< 2 >( r );

                double e =
                    chrono::duration_cast< chrono::milliseconds >( chrono::steady_clock::now() - t )
                        .count() /
                    1000.0;
                if ( static_cast< unsigned int >( e ) >= last + 10 ) {
                    MICROPROFILE_LEAVE();
                    auto i = imported - lastImported;
                    auto d = e - last;
                    cout << i << " more imported at " << i / d << " blocks/s. " << imported
                         << " imported in " << e << " seconds at "
                         << ( round( imported * 10 / e ) / 10 ) << " blocks/s (#"
                         << g_client->number() << ")"
                         << "\n";
                    fprintf( g_client->performance_fd, "%d\t%.2lf\n", g_client->number(), i / d );
                    last = static_cast< unsigned >( e );
                    lastImported = imported;
                    MICROPROFILE_ENTERI( "main", "bunch 10s", MP_LIGHTGRAY );
                }
            }  // while
            MICROPROFILE_LEAVE();

            bool moreToImport = true;
            while ( moreToImport ) {
                {
                    MICROPROFILE_SCOPEI( "main", "sleep 1 sec", MP_DIMGREY );
                    this_thread::sleep_for( chrono::seconds( 1 ) );
                }
                tie( ignore, moreToImport, ignore ) = g_client->syncQueue( 100000 );
            }
            double e =
                chrono::duration_cast< chrono::milliseconds >( chrono::steady_clock::now() - t )
                    .count() /
                1000.0;
            cout << imported << " imported in " << e << " seconds at "
                 << ( round( imported * 10 / e ) / 10 ) << " blocks/s (#" << g_client->number()
                 << ")\n";
        } );  // thread
        th.join();
        return 0;
    }

    try {
        if ( keyManager.exists() ) {
            if ( !keyManager.load( masterPassword ) && masterSet ) {
                while ( true ) {
                    masterPassword = getPassword( "Please enter your MASTER password: " );
                    if ( keyManager.load( masterPassword ) )
                        break;
                    cout << "The password you entered is incorrect. If you have forgotten your "
                            "password, and you wish to start afresh, manually remove the file: "
                         << ( getDataDir( "ethereum" ) / fs::path( "keys.info" ) ).string() << "\n";
                }
            }
        } else {
            if ( masterSet )
                keyManager.create( masterPassword );
            else
                keyManager.create( std::string() );
        }
    } catch ( ... ) {
        cerr << "Error initializing key manager: "
             << boost::current_exception_diagnostic_information() << "\n";
        return 1;
    }

    for ( auto const& presale : presaleImports )
        importPresale( keyManager, presale,
            [&]() { return getPassword( "Enter your wallet password for " + presale + ": " ); } );

    for ( auto const& s : toImport ) {
        keyManager.import( s, "Imported key (UNSAFE)" );
    }

    if ( mode == OperationMode::ImportSnapshot ) {
        try {
            auto stateImporter = g_client->createStateImporter();
            auto blockChainImporter = g_client->createBlockChainImporter();
            SnapshotImporter importer( *stateImporter, *blockChainImporter );

            auto snapshotStorage( createSnapshotStorage( filename ) );
            importer.import( *snapshotStorage, g_client->blockChain().genesisHash() );
            // continue with regular sync from the snapshot block
        } catch ( ... ) {
            cerr << "Error during importing the snapshot: "
                 << boost::current_exception_diagnostic_information() << endl;
            return EX_DATAERR;
        }
    }

    if ( nodeMode == NodeMode::Full ) {
        g_client->setSealer( m.minerType() );
        if ( networkID != NoNetworkID )
            g_client->setNetworkId( networkID );
    }

    cout << "Mining Beneficiary: " << g_client->author() << endl;

    unique_ptr< rpc::SessionManager > sessionManager;
    unique_ptr< SimpleAccountHolder > accountHolder;

    AddressHash allowedDestinations;

    std::string autoAuthAnswer;

    // First, get "aa" from config.json
    // Second, get it from command line parameter (higher priority source)
    std::string strAA;
    if ( chainConfigParsed ) {
        try {
            strAA = joConfig["skaleConfig"]["nodeInfo"]["aa"].get< std::string >();
        } catch ( ... ) {
            strAA.clear();
        }
    }
    if ( vm.count( "aa" ) )
        strAA = vm["aa"].as< string >();
    if ( !strAA.empty() ) {
        if ( strAA == "yes" || strAA == "no" || strAA == "always" )
            autoAuthAnswer = strAA;
        else {
            cerr << "Bad "
                 << "--aa"
                 << " option: " << strAA << "\n";
            return EX_USAGE;
        }
        clog( VerbosityInfo, "main" )
            << cc::info( "Auto-answer" ) << cc::debug( " mode is set to: " ) << cc::info( strAA );
    }

    std::function< bool( TransactionSkeleton const&, bool ) > authenticator;

    if ( autoAuthAnswer == "yes" || autoAuthAnswer == "always" )
        authenticator = [&]( TransactionSkeleton const& _t, bool ) -> bool {
            if ( autoAuthAnswer == "always" )
                allowedDestinations.insert( _t.to );
            return true;
        };
    else if ( autoAuthAnswer == "no" )
        authenticator = []( TransactionSkeleton const&, bool ) -> bool { return false; };
    else
        authenticator = [&]( TransactionSkeleton const& _t, bool isProxy ) -> bool {
            // "unlockAccount" functionality is done in the AccountHolder.
            if ( !alwaysConfirm || allowedDestinations.count( _t.to ) )
                return true;

            string r = getResponse(
                _t.userReadable( isProxy,
                    [&]( TransactionSkeleton const& _t ) -> pair< bool, string > {
                        h256 contractCodeHash = g_client->postState().codeHash( _t.to );
                        if ( contractCodeHash == EmptySHA3 )
                            return std::make_pair( false, std::string() );
                        // TODO: actually figure out the natspec. we'll need the
                        // natspec database here though.
                        return std::make_pair( true, std::string() );
                    },
                    [&]( Address const& _a ) { return _a.hex(); } ) +
                    "\nEnter yes/no/always (always to this address): ",
                {"yes", "n", "N", "no", "NO", "always"} );
            if ( r == "always" )
                allowedDestinations.insert( _t.to );
            return r == "yes" || r == "always";
        };
    if ( chainParams.nodeInfo.ip.empty() ) {
        clog( VerbosityWarning, "main" )
            << cc::info( "IPv4" )
            << cc::warn( " bind address is not set, will not start RPC on this protocol" );
        nExplicitPortHTTP4 = nExplicitPortHTTPS4 = nExplicitPortWS4 = nExplicitPortWSS4 = -1;
    }
    if ( chainParams.nodeInfo.ip6.empty() ) {
        clog( VerbosityWarning, "main" )
            << cc::info( "IPv6" )
            << cc::warn( " bind address is not set, will not start RPC on this protocol" );
        nExplicitPortHTTP6 = nExplicitPortHTTPS6 = nExplicitPortWS6 = nExplicitPortWSS6 = -1;
    }
    if ( is_ipc || nExplicitPortHTTP4 > 0 || nExplicitPortHTTPS4 > 0 || nExplicitPortWS4 > 0 ||
         nExplicitPortWSS4 > 0 || nExplicitPortHTTP6 > 0 || nExplicitPortHTTPS6 > 0 ||
         nExplicitPortWS6 > 0 || nExplicitPortWSS6 > 0 ) {
        using FullServer = ModularServer< rpc::EthFace,
            rpc::SkaleFace,   /// skale
            rpc::SkaleStats,  /// skaleStats
            rpc::NetFace, rpc::Web3Face, rpc::PersonalFace,
            rpc::AdminEthFace,  // SKALE rpc::AdminNetFace,
            rpc::DebugFace, rpc::TestFace >;

        sessionManager.reset( new rpc::SessionManager() );
        accountHolder.reset( new SimpleAccountHolder(
            [&]() { return g_client.get(); }, getAccountPassword, keyManager, authenticator ) );

        auto ethFace = new rpc::Eth( *g_client, *accountHolder.get() );
        /// skale
        auto skaleFace = new rpc::Skale( *g_client );
        /// skaleStatsFace
        auto skaleStatsFace = new rpc::SkaleStats( configPath.string(), *g_client );

        std::string argv_string;
        {
            ostringstream ss;
            for ( int i = 1; i < argc; ++i )
                ss << argv[i] << " ";
            argv_string = ss.str();
        }

        g_jsonrpcIpcServer.reset( new FullServer( ethFace,
            skaleFace,       /// skale
            skaleStatsFace,  /// skaleStats
            new rpc::Net( chainParams ), new rpc::Web3( clientVersion() ),
            new rpc::Personal( keyManager, *accountHolder, *g_client ),
            new rpc::AdminEth( *g_client, *gasPricer.get(), keyManager, *sessionManager.get() ),
            bEnabledDebugBehaviorAPIs ? new rpc::Debug( *g_client, &debugInterface, argv_string ) :
                                        nullptr,
            nullptr ) );

        if ( is_ipc ) {
            try {
                auto ipcConnector = new IpcServer( "geth" );
                g_jsonrpcIpcServer->addConnector( ipcConnector );
                if ( !ipcConnector->StartListening() ) {
                    clog( VerbosityError, "main" )
                        << "Cannot start listening for RPC requests on ipc port: "
                        << strerror( errno );
                    return EX_IOERR;
                }  // error
            } catch ( const std::exception& ex ) {
                clog( VerbosityError, "main" )
                    << "Cannot start listening for RPC requests on ipc port: " << ex.what();
                return EX_IOERR;
            }  // catch
        }      // if ( is_ipc )

        if ( nExplicitPortHTTP4 >= 65536 ) {
            clog( VerbosityError, "main" )
                << cc::fatal( "FATAL:" ) << cc::error( " Please specify valid value " )
                << cc::warn( "--http-port" ) << cc::error( "=" ) << cc::warn( "number" );
            return EX_USAGE;
        }
        if ( nExplicitPortHTTP6 >= 65536 ) {
            clog( VerbosityError, "main" )
                << cc::fatal( "FATAL:" ) << cc::error( " Please specify valid value " )
                << cc::warn( "--http-port6" ) << cc::error( "=" ) << cc::warn( "number" );
            return EX_USAGE;
        }
        if ( nExplicitPortHTTPS4 >= 65536 ) {
            clog( VerbosityError, "main" )
                << cc::fatal( "FATAL:" ) << cc::error( " Please specify valid value " )
                << cc::warn( "--https-port" ) << cc::error( "=" ) << cc::warn( "number" );
            return EX_USAGE;
        }
        if ( nExplicitPortHTTPS6 >= 65536 ) {
            clog( VerbosityError, "main" )
                << cc::fatal( "FATAL:" ) << cc::error( " Please specify valid value " )
                << cc::warn( "--https-port6" ) << cc::error( "=" ) << cc::warn( "number" );
            return EX_USAGE;
        }
        if ( nExplicitPortWS4 >= 65536 ) {
            clog( VerbosityError, "main" )
                << cc::fatal( "FATAL:" ) << cc::error( " Please specify valid value " )
                << cc::warn( "--ws-port" ) << cc::error( "=" ) << cc::warn( "number" );
            return EX_USAGE;
        }
        if ( nExplicitPortWS6 >= 65536 ) {
            clog( VerbosityError, "main" )
                << cc::fatal( "FATAL:" ) << cc::error( " Please specify valid value " )
                << cc::warn( "--ws-port6" ) << cc::error( "=" ) << cc::warn( "number" );
            return EX_USAGE;
        }
        if ( nExplicitPortWSS4 >= 65536 ) {
            clog( VerbosityError, "main" )
                << cc::fatal( "FATAL:" ) << cc::error( " Please specify valid value " )
                << cc::warn( "--wss-port" ) << cc::error( "=" ) << cc::warn( "number" );
            return EX_USAGE;
        }
        if ( nExplicitPortWSS6 >= 65536 ) {
            clog( VerbosityError, "main" )
                << cc::fatal( "FATAL:" ) << cc::error( " Please specify valid value " )
                << cc::warn( "--wss-port6" ) << cc::error( "=" ) << cc::warn( "number" );
            return EX_USAGE;
        }

        if ( nExplicitPortHTTP4 > 0 || nExplicitPortHTTPS4 > 0 || nExplicitPortWS4 > 0 ||
             nExplicitPortWSS4 > 0 || nExplicitPortHTTP6 > 0 || nExplicitPortHTTPS6 > 0 ||
             nExplicitPortWS6 > 0 || nExplicitPortWSS6 > 0 ) {
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::attention( "RPC params" ) << cc::debug( ":" );
            //
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::info( "HTTP/4 port" )
                << cc::debug( ".............................. " )
                << ( ( nExplicitPortHTTP4 >= 0 ) ? cc::num10( nExplicitPortHTTP4 ) :
                                                   cc::error( "off" ) );
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::info( "HTTP/6 port" )
                << cc::debug( ".............................. " )
                << ( ( nExplicitPortHTTP6 >= 0 ) ? cc::num10( nExplicitPortHTTP6 ) :
                                                   cc::error( "off" ) );
            //
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::info( "HTTPS/4 port" )
                << cc::debug( "............................. " )
                << ( ( nExplicitPortHTTPS4 >= 0 ) ? cc::num10( nExplicitPortHTTPS4 ) :
                                                    cc::error( "off" ) );
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::info( "HTTPS/6 port" )
                << cc::debug( "............................. " )
                << ( ( nExplicitPortHTTPS6 >= 0 ) ? cc::num10( nExplicitPortHTTPS6 ) :
                                                    cc::error( "off" ) );
            //
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::info( "WS/4 port" )
                << cc::debug( "................................ " )
                << ( ( nExplicitPortWS4 >= 0 ) ? cc::num10( nExplicitPortWS4 ) :
                                                 cc::error( "off" ) );
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::info( "WS/6 port" )
                << cc::debug( "................................ " )
                << ( ( nExplicitPortWS6 >= 0 ) ? cc::num10( nExplicitPortWS6 ) :
                                                 cc::error( "off" ) );
            //
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::info( "WSS/4 port" )
                << cc::debug( "............................... " )
                << ( ( nExplicitPortWSS4 >= 0 ) ? cc::num10( nExplicitPortWSS4 ) :
                                                  cc::error( "off" ) );
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::info( "WSS/6 port" )
                << cc::debug( "............................... " )
                << ( ( nExplicitPortWSS6 >= 0 ) ? cc::num10( nExplicitPortWSS6 ) :
                                                  cc::error( "off" ) );
            //
            std::string strPathSslKey, strPathSslCert;
            bool bHaveSSL = false;
            if ( ( nExplicitPortHTTPS4 > 0 || nExplicitPortWSS4 > 0 || nExplicitPortHTTPS6 > 0 ||
                     nExplicitPortWSS6 > 0 ) &&
                 vm.count( "ssl-key" ) > 0 && vm.count( "ssl-cert" ) > 0 ) {
                strPathSslKey = vm["ssl-key"].as< std::string >();
                strPathSslCert = vm["ssl-cert"].as< std::string >();
                if ( ( !strPathSslKey.empty() ) && ( !strPathSslCert.empty() ) )
                    bHaveSSL = true;
            }

            double lfExecutionDurationMaxForPerformanceWarning = SkaleServerOverride::
                g_lfDefaultExecutionDurationMaxForPerformanceWarning;  // in seconds, default 1
                                                                       // second
            if ( vm.count( "performance-warning-duration" ) > 0 ) {
                lfExecutionDurationMaxForPerformanceWarning = vm["ssl-key"].as< double >();
                if ( lfExecutionDurationMaxForPerformanceWarning < 0.0 )
                    lfExecutionDurationMaxForPerformanceWarning = 0.0;
            }

            skutils::task::performance::tracker_ptr pTracker =
                skutils::task::performance::get_default_tracker();
            if ( vm.count( "performance-timeline-enable" ) > 0 )
                pTracker->set_enabled( true );
            if ( vm.count( "performance-timeline-disable" ) > 0 )
                pTracker->set_enabled( false );
            if ( vm.count( "performance-timeline-max-items" ) > 0 ) {
                size_t maxItemCount = vm["performance-timeline-max-items"].as< size_t >();
                pTracker->set_safe_max_item_count( maxItemCount );
            }
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::info( "Performance timeline tracker" )
                << cc::debug( "............. " )
                << ( pTracker->is_enabled() ? cc::size10( pTracker->get_safe_max_item_count() ) :
                                              cc::error( "off" ) );

            if ( !bHaveSSL )
                nExplicitPortHTTPS4 = nExplicitPortWSS4 = nExplicitPortHTTPS6 = nExplicitPortWSS6 =
                    -1;
            if ( bHaveSSL ) {
                clog( VerbosityInfo, "main" )
                    << cc::debug( "...." ) << cc::info( "SSL key is" )
                    << cc::debug( "............................... " ) << cc::p( strPathSslKey );
                clog( VerbosityInfo, "main" )
                    << cc::debug( "...." ) + cc::info( "SSL certificate is" )
                    << cc::debug( "....................... " ) << cc::p( strPathSslCert );
            }
            //
            //
            size_t maxConnections = 0,
                   max_http_handler_queues = __SKUTILS_HTTP_DEFAULT_MAX_PARALLEL_QUEUES_COUNT__,
                   cntServers = 1, cntInBatch = 128;
            bool is_async_http_transfer_mode = true;

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
            if ( vm.count( "max-connections" ) )
                maxConnections = vm["max-connections"].as< size_t >();
            //
            // First, get "max-http-queues" true/false from config.json
            // Second, get it from command line parameter (higher priority source)
            if ( chainConfigParsed ) {
                try {
                    max_http_handler_queues =
                        joConfig["skaleConfig"]["nodeInfo"]["max-http-queues"].get< size_t >();
                } catch ( ... ) {
                    max_http_handler_queues = __SKUTILS_HTTP_DEFAULT_MAX_PARALLEL_QUEUES_COUNT__;
                }
            }
            if ( vm.count( "max-http-queues" ) )
                max_http_handler_queues = vm["max-http-queues"].as< size_t >();

            // First, get "max-http-queues" true/false from config.json
            // Second, get it from command line parameter (higher priority source)
            if ( chainConfigParsed ) {
                try {
                    is_async_http_transfer_mode =
                        joConfig["skaleConfig"]["nodeInfo"]["async-http-transfer-mode"]
                            .get< bool >();
                } catch ( ... ) {
                    is_async_http_transfer_mode = true;
                }
            }
            if ( vm.count( "async-http-transfer-mode" ) )
                is_async_http_transfer_mode = true;
            if ( vm.count( "sync-http-transfer-mode" ) )
                is_async_http_transfer_mode = false;

            // First, get "acceptors" true/false from config.json
            // Second, get it from command line parameter (higher priority source)
            if ( chainConfigParsed ) {
                try {
                    cntServers = joConfig["skaleConfig"]["nodeInfo"]["acceptors"].get< size_t >();
                } catch ( ... ) {
                    cntServers = 1;
                }
            }
            if ( vm.count( "acceptors" ) )
                cntServers = vm["acceptors"].as< size_t >();
            if ( cntServers < 1 )
                cntServers = 1;

            // First, get "acceptors" true/false from config.json
            // Second, get it from command line parameter (higher priority source)
            if ( chainConfigParsed ) {
                try {
                    cntInBatch = joConfig["skaleConfig"]["nodeInfo"]["max-batch"].get< size_t >();
                } catch ( ... ) {
                    cntInBatch = 128;
                }
            }
            if ( vm.count( "max-batch" ) )
                cntInBatch = vm["max-batch"].as< size_t >();
            if ( cntInBatch < 1 )
                cntInBatch = 1;

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
            if ( vm.count( "ws-mode" ) ) {
                std::string s = vm["ws-mode"].as< std::string >();
                skutils::ws::nlws::g_default_srvmode = skutils::ws::nlws::str2srvmode( s );
            }

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
            if ( vm.count( "ws-log" ) ) {
                std::string s = vm["ws-log"].as< std::string >();
                skutils::ws::g_eWSLL = skutils::ws::str2wsll( s );
            }

            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) + cc::info( "WS mode" )
                << cc::debug( ".................................. " )
                << skutils::ws::nlws::srvmode2str( skutils::ws::nlws::g_default_srvmode );
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) + cc::info( "WS logging" )
                << cc::debug( "............................... " )
                << cc::info( skutils::ws::wsll2str( skutils::ws::g_eWSLL ) );
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) + cc::info( "Max RPC connections" )
                << cc::debug( "...................... " )
                << ( ( maxConnections > 0 ) ? cc::size10( maxConnections ) :
                                              cc::error( "disabled" ) );
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) + cc::info( "Max HTTP queues" )
                << cc::debug( ".......................... " )
                << ( ( max_http_handler_queues > 0 ) ? cc::size10( max_http_handler_queues ) :
                                                       cc::notice( "default" ) );
            clog( VerbosityInfo, "main" ) << cc::debug( "...." ) + cc::info( "Asynchronous HTTP" )
                                          << cc::debug( "........................ " )
                                          << cc::yn( is_async_http_transfer_mode );
            //
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) + cc::info( "Max count in batch JSON RPC request" )
                << cc::debug( "...... " ) << cc::size10( cntInBatch );
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) + cc::info( "Parallel RPC connection acceptors" )
                << cc::debug( "........ " ) << cc::size10( cntServers );
            SkaleServerOverride::fn_binary_snapshot_download_t fn_binary_snapshot_download =
                [=]( const nlohmann::json& joRequest ) -> std::vector< uint8_t > {
                return skaleFace->impl_skale_downloadSnapshotFragmentBinary( joRequest );
            };
            //
            auto skale_server_connector = new SkaleServerOverride( chainParams,
                fn_binary_snapshot_download, cntServers, g_client.get(), chainParams.nodeInfo.ip,
                nExplicitPortHTTP4, chainParams.nodeInfo.ip6, nExplicitPortHTTP6,
                chainParams.nodeInfo.ip, nExplicitPortHTTPS4, chainParams.nodeInfo.ip6,
                nExplicitPortHTTPS6, chainParams.nodeInfo.ip, nExplicitPortWS4,
                chainParams.nodeInfo.ip6, nExplicitPortWS6, chainParams.nodeInfo.ip,
                nExplicitPortWSS4, chainParams.nodeInfo.ip6, nExplicitPortWSS6, strPathSslKey,
                strPathSslCert, lfExecutionDurationMaxForPerformanceWarning );
            //
            // unddos
            if ( joConfig.count( "unddos" ) > 0 ) {
                nlohmann::json joUnDdosSettings = joConfig["unddos"];
                skale_server_connector->unddos_.load_settings_from_json( joUnDdosSettings );
            } else
                skale_server_connector->unddos_.get_settings();  // auto-init
            //
            clog( VerbosityInfo, "main" )
                << cc::attention( "UN-DDOS" ) + cc::debug( " is using configuration" )
                << cc::j( skale_server_connector->unddos_.get_settings_json() );
            skale_server_connector->max_http_handler_queues_ = max_http_handler_queues;
            skale_server_connector->is_async_http_transfer_mode_ = is_async_http_transfer_mode;
            skale_server_connector->maxCountInBatchJsonRpcRequest_ = cntInBatch;
            //
            skaleStatsFace->setProvider( skale_server_connector );
            skale_server_connector->setConsumer( skaleStatsFace );
            //
            skale_server_connector->m_bTraceCalls = bTraceJsonRpcCalls;
            skale_server_connector->max_connection_set( maxConnections );
            g_jsonrpcIpcServer->addConnector( skale_server_connector );
            if ( !skale_server_connector->StartListening() ) {  // TODO Will it delete itself?
                return EX_IOERR;
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
            static const std::chrono::milliseconds g_waitAttempt = std::chrono::milliseconds( 100 );
            if ( nExplicitPortHTTP4 > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatHTTP4 < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !exitHandler.shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityInfo, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "HTTP/4" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatHTTP4 = skale_server_connector->getServerPortStatusHTTP( 4 );
                }
            }
            if ( nExplicitPortHTTP6 > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatHTTP6 < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !exitHandler.shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityInfo, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "HTTP/6" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatHTTP6 = skale_server_connector->getServerPortStatusHTTP( 6 );
                }
            }
            if ( nExplicitPortHTTPS4 > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatHTTPS4 < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !exitHandler.shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityInfo, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "HTTPS/4" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatHTTPS4 = skale_server_connector->getServerPortStatusHTTPS( 4 );
                }
            }
            if ( nExplicitPortHTTPS6 > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatHTTPS6 < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !exitHandler.shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityInfo, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "HTTPS/6" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatHTTPS6 = skale_server_connector->getServerPortStatusHTTPS( 6 );
                }
            }
            if ( nExplicitPortWS4 > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatWS4 < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !exitHandler.shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityInfo, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "WS/4" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatWS4 = skale_server_connector->getServerPortStatusWS( 4 );
                }
            }
            if ( nExplicitPortWS6 > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatWS6 < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !exitHandler.shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityInfo, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "WS/6" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatWS6 = skale_server_connector->getServerPortStatusWS( 6 );
                }
            }
            if ( nExplicitPortWSS4 > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatWSS4 < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !exitHandler.shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityInfo, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "WSS/4" )
                            << cc::debug( " start... " );
                    nStatWSS4 = skale_server_connector->getServerPortStatusWSS( 4 );
                }
            }
            if ( nExplicitPortWSS6 > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatWSS6 < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !exitHandler.shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityInfo, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "WSS/6" )
                            << cc::debug( " start... " );
                    nStatWSS6 = skale_server_connector->getServerPortStatusWSS( 6 );
                }
            }
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::attention( "RPC status" ) << cc::debug( ":" );
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::info( "HTTP/4" )
                << cc::debug( "................................. " )
                << ( ( nStatHTTP4 >= 0 ) ?
                           ( ( nExplicitPortHTTP4 > 0 ) ? cc::num10( nStatHTTP4 ) :
                                                          cc::warn( "still starting..." ) ) :
                           cc::error( "off" ) );
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::info( "HTTP/6" )
                << cc::debug( "................................. " )
                << ( ( nStatHTTP6 >= 0 ) ?
                           ( ( nExplicitPortHTTP6 > 0 ) ? cc::num10( nStatHTTP6 ) :
                                                          cc::warn( "still starting..." ) ) :
                           cc::error( "off" ) );
            //
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::info( "HTTPS/4" )
                << cc::debug( "................................ " )
                << ( ( nStatHTTPS4 >= 0 ) ?
                           ( ( nExplicitPortHTTPS4 > 0 ) ? cc::num10( nStatHTTPS4 ) :
                                                           cc::warn( "still starting..." ) ) :
                           cc::error( "off" ) );
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::info( "HTTPS/6" )
                << cc::debug( "................................ " )
                << ( ( nStatHTTPS6 >= 0 ) ?
                           ( ( nExplicitPortHTTPS6 > 0 ) ? cc::num10( nStatHTTPS6 ) :
                                                           cc::warn( "still starting..." ) ) :
                           cc::error( "off" ) );
            //
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::info( "WS/4" )
                << cc::debug( "................................... " )
                << ( ( nStatWS4 >= 0 ) ?
                           ( ( nExplicitPortWS4 > 0 ) ? cc::num10( nStatWS4 ) :
                                                        cc::warn( "still starting..." ) ) :
                           cc::error( "off" ) );
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::info( "WS/6" )
                << cc::debug( "................................... " )
                << ( ( nStatWS6 >= 0 ) ?
                           ( ( nExplicitPortWS6 > 0 ) ? cc::num10( nStatWS6 ) :
                                                        cc::warn( "still starting..." ) ) :
                           cc::error( "off" ) );
            //
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::info( "WSS/4" )
                << cc::debug( ".................................. " )
                << ( ( nStatWSS4 >= 0 ) ?
                           ( ( nExplicitPortWSS4 > 0 ) ? cc::num10( nStatWSS4 ) :
                                                         cc::warn( "still starting..." ) ) :
                           cc::error( "off" ) );
            clog( VerbosityInfo, "main" )
                << cc::debug( "...." ) << cc::info( "WSS/6" )
                << cc::debug( ".................................. " )
                << ( ( nStatWSS6 >= 0 ) ?
                           ( ( nExplicitPortWSS6 > 0 ) ? cc::num10( nStatWSS6 ) :
                                                         cc::warn( "still starting..." ) ) :
                           cc::error( "off" ) );
        }  // if ( nExplicitPortHTTP > 0 || nExplicitPortHTTPS > 0 || nExplicitPortWS > 0 ||
           // nExplicitPortWSS > 0 )

        if ( strJsonAdminSessionKey.empty() )
            strJsonAdminSessionKey =
                sessionManager->newSession( rpc::SessionPermissions{{rpc::Privilege::Admin}} );
        else
            sessionManager->addSession(
                strJsonAdminSessionKey, rpc::SessionPermissions{{rpc::Privilege::Admin}} );

        clog( VerbosityInfo, "main" )
            << cc::bright( "JSONRPC Admin Session Key: " ) << cc::sunny( strJsonAdminSessionKey );
    }  // if ( is_ipc || nExplicitPortHTTP > 0 || nExplicitPortHTTPS > 0  || nExplicitPortWS > 0 ||
       // nExplicitPortWSS > 0 )

    if ( bEnabledShutdownViaWeb3 ) {
        clog( VerbosityInfo, "main" ) << cc::debug( "Enabling programmatic shutdown via Web3..." );
        dev::rpc::Skale::enableWeb3Shutdown( true );
        dev::rpc::Skale::onShutdownInvoke(
            []() { ExitHandler::exitHandler( SIGABRT, ExitHandler::ec_web3_request ); } );
        clog( VerbosityInfo, "main" )
            << cc::debug( "Done, programmatic shutdown via Web3 is enabled" );
    } else {
        clog( VerbosityInfo, "main" ) << cc::debug( "Disabling programmatic shutdown via Web3..." );
        dev::rpc::Skale::enableWeb3Shutdown( false );
        clog( VerbosityInfo, "main" )
            << cc::debug( "Done, programmatic shutdown via Web3 is disabled" );
    }

    dev::setThreadName( "main" );

    if ( g_client ) {
        unsigned int n = g_client->blockChain().details().number;
        unsigned int mining = 0;
        while ( !exitHandler.shouldExit() )
            stopSealingAfterXBlocks( g_client.get(), n, mining );
    } else {
        while ( !exitHandler.shouldExit() )
            this_thread::sleep_for( chrono::milliseconds( 1000 ) );
    }
    if ( g_jsonrpcIpcServer.get() ) {
        g_jsonrpcIpcServer->StopListening();
        g_jsonrpcIpcServer.reset( nullptr );
    }
    if ( g_client ) {
        g_client->stopWorking();
        g_client.reset( nullptr );
    }

    std::cerr << localeconv()->decimal_point << std::endl;

    std::string basename = "profile" + chainParams.nodeInfo.id.str();
    MicroProfileDumpFileImmediately(
        ( basename + ".html" ).c_str(), ( basename + ".csv" ).c_str(), nullptr );
    MicroProfileShutdown();

    //    clog( VerbosityInfo, "main" ) << cc::debug( "Stopping task dispatcher..." );
    //    skutils::dispatch::shutdown();
    //    clog( VerbosityInfo, "main" ) << cc::debug( "Done, task dispatcher stopped" );
    ExitHandler::exit_code_t ec = ExitHandler::requestedExitCode();
    if ( ec == ExitHandler::ec_success ) {
        int sig_no = ExitHandler::getSignal();
        if ( sig_no != SIGINT && sig_no != SIGTERM )
            ec = ExitHandler::ec_failure;
    }
    if ( ec != ExitHandler::ec_success ) {
        std::cerr << cc::error( "Exiting main with code " ) << cc::num10( int( ec ) )
                  << cc::error( "...\n" );
        std::cerr.flush();
    }
    return int( ec );
} catch ( const Client::CreationException& ex ) {
    clog( VerbosityError, "main" ) << dev::nested_exception_what( ex );
    // TODO close microprofile!!
    g_client.reset( nullptr );
    return int( ExitHandler::ec_failure );
} catch ( const SkaleHost::CreationException& ex ) {
    clog( VerbosityError, "main" ) << dev::nested_exception_what( ex );
    // TODO close microprofile!!
    g_client.reset( nullptr );
    return int( ExitHandler::ec_failure );
} catch ( const std::exception& ex ) {
    clog( VerbosityError, "main" ) << "CRITICAL " << dev::nested_exception_what( ex );
    clog( VerbosityError, "main" ) << "\n"
                                   << skutils::signal::generate_stack_trace() << "\n"
                                   << std::endl;
    g_client.reset( nullptr );
    return int( ExitHandler::ec_failure );
} catch ( ... ) {
    clog( VerbosityError, "main" ) << "CRITICAL unknown error";
    clog( VerbosityError, "main" ) << "\n"
                                   << skutils::signal::generate_stack_trace() << "\n"
                                   << std::endl;
    g_client.reset( nullptr );
    return int( ExitHandler::ec_failure );
}
