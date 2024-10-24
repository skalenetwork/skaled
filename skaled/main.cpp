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

#include <libdevcore/DBFactory.h>
#include <libdevcore/FileSystem.h>
#include <libdevcore/LevelDB.h>
#include <libdevcore/LoggingProgramOptions.h>
#include <libdevcore/SharedSpace.h>
#include <libdevcore/StatusAndControl.h>
#include <libethashseal/EthashClient.h>
#include <libethashseal/GenesisInfo.h>
#include <libethcore/KeyManager.h>
#include <libethereum/ClientTest.h>
#include <libethereum/Defaults.h>
#include <libethereum/SnapshotStorage.h>
#include <libevm/VMFactory.h>

#include <libskale/ConsensusGasPricer.h>
#include <libskale/SnapshotManager.h>
#include <libskale/UnsafeRegion.h>

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
#include <libweb3jsonrpc/SkalePerformanceTracker.h>
#include <libweb3jsonrpc/SkaleStats.h>
#include <libweb3jsonrpc/Test.h>
#include <libweb3jsonrpc/Tracing.h>
#include <libweb3jsonrpc/Web3.h>
#include <libweb3jsonrpc/rapidjson_handlers.h>

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
#include <skutils/url.h>
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
std::atomic< bool > g_silence = { false };
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
    std::cout << cc::info( "Skaled" ) << cc::debug( "............................" )
              << cc::attention( ver ) << "\n";
    if ( !commit.empty() )
        cout << cc::info( "Commit" ) << cc::debug( "............................" )
             << cc::attention( commit ) << "\n";
    std::cout << cc::info( "Skale network protocol version" ) << cc::debug( "...." )
              << cc::num10( dev::eth::c_protocolVersion ) << cc::debug( "." )
              << cc::num10( c_minorProtocolVersion ) << "\n";
    std::cout << cc::info( "Client database version" ) << cc::debug( "..........." )
              << cc::num10( dev::eth::c_databaseVersion ) << "\n";
    std::cout << cc::info( "Build" ) << cc::debug( "............................." )
              << cc::attention( buildinfo->system_name ) << cc::debug( "/" )
              << cc::attention( buildinfo->build_type ) << "\n";
    std::cout.flush();
}

static std::string clientVersion() {
    const auto* buildinfo = skale_get_buildinfo();
    return std::string( "skaled/" ) + buildinfo->project_version + "/" + buildinfo->system_name +
           "/" + buildinfo->compiler_id + buildinfo->compiler_version + "/" + buildinfo->build_type;
}

static std::string clientVersionColorized() {
    const auto* buildinfo = skale_get_buildinfo();
    return cc::info( "skaled" ) + cc::debug( "/" ) + cc::attention( buildinfo->project_version ) +
           cc::debug( "/" ) + cc::attention( buildinfo->system_name ) + cc::debug( "/" ) +
           cc::attention( buildinfo->compiler_id ) + cc::notice( buildinfo->compiler_version ) +
           cc::debug( "/" ) + cc::attention( buildinfo->build_type );
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
    const set< string > filteredOptions = { "http-port", "https-port", "ws-port", "wss-port",
        "http-port6", "https-port6", "ws-port6", "wss-port6", "info-http-port", "info-https-port",
        "info-ws-port", "info-wss-port", "info-http-port6", "info-https-port6", "info-ws-port6",
        "info-wss-port6", "ws-log", "ssl-key", "ssl-cert", "ssl-ca", "acceptors",
        "info-acceptors" };
    const set< string > emptyValues = { "NULL", "null", "None" };

    parsed.options.erase( remove_if( parsed.options.begin(), parsed.options.end(),
                              [&filteredOptions, &emptyValues]( const auto& option ) -> bool {
                                  return filteredOptions.count( option.string_key ) &&
                                         emptyValues.count( option.value.front() );
                              } ),
        parsed.options.end() );
}

unsigned getLatestSnapshotBlockNumber( const std::string& strURLWeb3 ) {
    skutils::rest::client cli( skutils::rest::g_nClientConnectionTimeoutMS );
    if ( !cli.open( strURLWeb3 ) ) {
        throw std::runtime_error( "REST failed to connect to server" );
    }

    nlohmann::json joIn = nlohmann::json::object();
    joIn["jsonrpc"] = "2.0";
    joIn["method"] = "skale_getLatestSnapshotBlockNumber";
    joIn["params"] = nlohmann::json::object();
    skutils::rest::data_t d = cli.call( joIn );
    if ( !d.err_s_.empty() )
        throw std::runtime_error( "cannot get blockNumber to download snapshot: " + d.err_s_ );
    if ( d.empty() )
        throw std::runtime_error( "cannot get blockNumber to download snapshot" );
    nlohmann::json joAnswer = nlohmann::json::parse( d.s_ );
    unsigned block_number = dev::eth::jsToBlockNumber( joAnswer["result"].get< std::string >() );

    return block_number;
}

void downloadSnapshot( unsigned block_number, std::shared_ptr< SnapshotManager >& snapshotManager,
    const std::string& strURLWeb3, const ChainParams& chainParams ) {
    fs::path saveTo;
    try {
        clog( VerbosityInfo, "downloadSnapshot" ) << "Will download snapshot from " << strURLWeb3;

        try {
            bool isBinaryDownload = true;
            std::string strErrorDescription;
            saveTo = snapshotManager->getDiffPath( block_number );
            bool bOK = dev::rpc::snapshot::download(
                strURLWeb3, block_number, saveTo,
                [&]( size_t idxChunck, size_t cntChunks ) -> bool {
                    clog( VerbosityInfo, "downloadSnapshot" )
                        << "... download progress ... " << idxChunck << " of " << cntChunks << "\r";
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
            // remove partially downloaded snapshot
            boost::filesystem::remove( saveTo );
            std::throw_with_nested( std::runtime_error( "Exception while downloading snapshot" ) );
        }
        clog( VerbosityInfo, "downloadSnapshot" )
            << "Snapshot download success for block " << to_string( block_number );
        try {
            snapshotManager->importDiff( block_number );
        } catch ( ... ) {
            std::throw_with_nested(
                std::runtime_error( "FATAL: Exception while importing downloaded snapshot: " ) );
        }

        /// HACK refactor this piece of code! ///
        vector< string > prefixes{ "prices_", "blocks_" };
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
                    << "Snapshot downloaded without " + prefix + " db";
                return;
            }

            fs::rename( db_path,
                db_path.parent_path() / ( prefix + chainParams.nodeInfo.id.str() + ".db" ) );
        }
        //// HACK END ////

    } catch ( ... ) {
        std::throw_with_nested(
            std::runtime_error( "FATAL: Exception while processing downloaded snapshot: " ) );
    }
    if ( !saveTo.empty() )
        fs::remove( saveTo );
}

std::array< std::string, 4 > getBLSPublicKeyToVerifySnapshot( const ChainParams& chainParams ) {
    std::array< std::string, 4 > arrayCommonPublicKey;
    bool isRotationtrigger = true;
    if ( chainParams.sChain.nodeGroups.size() > 1 ) {
        if ( ( uint64_t ) time( NULL ) >=
             chainParams.sChain.nodeGroups[chainParams.sChain.nodeGroups.size() - 2].finishTs ) {
            isRotationtrigger = false;
        }
    } else {
        isRotationtrigger = false;
    }
    if ( isRotationtrigger ) {
        arrayCommonPublicKey =
            chainParams.sChain.nodeGroups[chainParams.sChain.nodeGroups.size() - 2].blsPublicKey;
    } else {
        arrayCommonPublicKey = chainParams.sChain.nodeGroups.back().blsPublicKey;
    }

    return arrayCommonPublicKey;
}

unsigned getBlockToDownladSnapshot( const std::string& nodeUrl ) {
    clog( VerbosityInfo, "getBlockToDownladSnapshot" )
        << "Asking node " << ' ' << nodeUrl << " for latest snapshot block number.";

    unsigned blockNumber = getLatestSnapshotBlockNumber( nodeUrl );
    clog( VerbosityInfo, "getBlockToDownladSnapshot" )
        << std::string( "Latest Snapshot Block Number is: " ) << blockNumber << " (from " << nodeUrl
        << ")";

    return blockNumber;
}

std::pair< std::vector< std::string >, std::pair< dev::h256, libff::alt_bn128_G1 > >
voteForSnapshotHash(
    std::unique_ptr< SnapshotHashAgent >& snapshotHashAgent, unsigned blockNumber ) {
    std::pair< dev::h256, libff::alt_bn128_G1 > votedHash;
    std::vector< std::string > listUrlsToDownload;
    try {
        listUrlsToDownload = snapshotHashAgent->getNodesToDownloadSnapshotFrom( blockNumber );
        clog( VerbosityInfo, "voteForSnapshotHash" )
            << "Got urls to download snapshot from " << listUrlsToDownload.size() << " nodes ";

        if ( listUrlsToDownload.size() == 0 )
            return { listUrlsToDownload, votedHash };

        votedHash = snapshotHashAgent->getVotedHash();

        return { listUrlsToDownload, votedHash };
    } catch ( std::exception& ex ) {
        std::throw_with_nested( std::runtime_error(
            cc::error( "Exception while collecting snapshot hash from other skaleds " ) ) );
    }
}

bool checkLocalSnapshot( std::shared_ptr< SnapshotManager >& snapshotManager, unsigned blockNumber,
    const dev::h256& votedHash ) {
    try {
        if ( snapshotManager->isSnapshotHashPresent( blockNumber ) ) {
            clog( VerbosityInfo, "checkLocalSnapshot" )
                << "Snapshot for block " << blockNumber << " already present locally";

            dev::h256 calculated_hash = snapshotManager->getSnapshotHash( blockNumber );

            if ( calculated_hash == votedHash ) {
                clog( VerbosityInfo, "checkLocalSnapshot" )
                    << std::string( "Will delete all snapshots except " )
                    << std::to_string( blockNumber );
                snapshotManager->cleanupButKeepSnapshot( blockNumber );
                snapshotManager->restoreSnapshot( blockNumber );
                clog( VerbosityInfo, "checkLocalSnapshot" )
                    << "Snapshot restore success for block " << std::to_string( blockNumber );
                return true;
            } else {
                clog( VerbosityWarning, "checkLocalSnapshot" )
                    << "Snapshot is present locally but its hash is different";
            }
        }  // if present
    } catch ( const std::exception& ex ) {
        // usually snapshot absent exception
        clog( VerbosityInfo, "checkLocalSnapshot" ) << dev::nested_exception_what( ex );
    }

    return false;
}

bool tryDownloadSnapshot( std::shared_ptr< SnapshotManager >& snapshotManager,
    const ChainParams& chainParams, const std::vector< std::string >& listUrlsToDownload,
    const std::pair< dev::h256, libff::alt_bn128_G1 >& votedHash, unsigned blockNumber,
    bool isRegularSnapshot ) {
    clog( VerbosityInfo, "tryDownloadSnapshot" )
        << "Will cleanup data dir and snapshots dir if needed";
    if ( isRegularSnapshot )
        snapshotManager->cleanup();

    bool successfullDownload = false;

    size_t n_found = listUrlsToDownload.size();

    size_t shift = rand() % n_found;

    for ( size_t cnt = 0; cnt < n_found && !successfullDownload; ++cnt )
        try {
            size_t i = ( shift + cnt ) % n_found;

            std::string urlToDownloadSnapshot;
            urlToDownloadSnapshot = listUrlsToDownload[i];

            downloadSnapshot( blockNumber, snapshotManager, urlToDownloadSnapshot, chainParams );

            try {
                snapshotManager->computeSnapshotHash( blockNumber, true );
            } catch ( const std::exception& ) {
                std::throw_with_nested( std::runtime_error(
                    std::string( "FATAL:" ) +
                    std::string( " Exception while computing snapshot hash " ) ) );
            }

            dev::h256 calculated_hash = snapshotManager->getSnapshotHash( blockNumber );

            if ( calculated_hash == votedHash.first ) {
                successfullDownload = true;
                if ( isRegularSnapshot ) {
                    snapshotManager->restoreSnapshot( blockNumber );
                    clog( VerbosityInfo, "tryDownloadSnapshot" )
                        << "Snapshot restore success for block " << to_string( blockNumber );
                }
                return successfullDownload;
            } else {
                clog( VerbosityWarning, "tryDownloadSnapshot" )
                    << "Downloaded snapshot with incorrect hash! Incoming hash "
                    << votedHash.first.hex() << " is not equal to calculated hash "
                    << calculated_hash.hex() << " Will try again";
                if ( isRegularSnapshot )
                    snapshotManager->cleanup();
                else
                    snapshotManager->removeSnapshot( 0 );
            }
        } catch ( const std::exception& ex ) {
            // just retry
            clog( VerbosityWarning, "tryDownloadSnapshot" ) << dev::nested_exception_what( ex );
        }  // for download url
    return false;
}

bool downloadSnapshotFromUrl( std::shared_ptr< SnapshotManager >& snapshotManager,
    const ChainParams& chainParams, const std::array< std::string, 4 >& arrayCommonPublicKey,
    const std::string& urlToDownloadSnapshotFrom, bool isRegularSnapshot,
    bool forceDownload = false ) {
    unsigned blockNumber = 0;
    if ( isRegularSnapshot )
        blockNumber = getBlockToDownladSnapshot( urlToDownloadSnapshotFrom );

    std::unique_ptr< SnapshotHashAgent > snapshotHashAgent;
    if ( forceDownload )
        snapshotHashAgent.reset(
            new SnapshotHashAgent( chainParams, arrayCommonPublicKey, urlToDownloadSnapshotFrom ) );
    else
        snapshotHashAgent.reset( new SnapshotHashAgent( chainParams, arrayCommonPublicKey ) );

    libff::init_alt_bn128_params();
    std::pair< dev::h256, libff::alt_bn128_G1 > votedHash;
    std::vector< std::string > listUrlsToDownload;
    std::tie( listUrlsToDownload, votedHash ) =
        voteForSnapshotHash( snapshotHashAgent, blockNumber );

    if ( listUrlsToDownload.empty() ) {
        if ( !isRegularSnapshot )
            return true;
        clog( VerbosityWarning, "downloadAndProccessSnapshot" )
            << "No nodes to download from - will skip " << urlToDownloadSnapshotFrom;
        return false;
    }

    bool successfullDownload = checkLocalSnapshot( snapshotManager, blockNumber, votedHash.first );
    if ( successfullDownload )
        return successfullDownload;

    successfullDownload = tryDownloadSnapshot( snapshotManager, chainParams, listUrlsToDownload,
        votedHash, blockNumber, isRegularSnapshot );

    return successfullDownload;
}

void downloadAndProccessSnapshot( std::shared_ptr< SnapshotManager >& snapshotManager,
    const ChainParams& chainParams, const std::string& urlToDownloadSnapshotFrom,
    bool isRegularSnapshot ) {
    std::array< std::string, 4 > arrayCommonPublicKey =
        getBLSPublicKeyToVerifySnapshot( chainParams );

    bool successfullDownload = false;

    if ( !urlToDownloadSnapshotFrom.empty() )
        successfullDownload = downloadSnapshotFromUrl( snapshotManager, chainParams,
            arrayCommonPublicKey, urlToDownloadSnapshotFrom, isRegularSnapshot, true );
    else {
        for ( size_t idx = 0; idx < chainParams.sChain.nodes.size() && !successfullDownload; ++idx )
            try {
                if ( chainParams.nodeInfo.id == chainParams.sChain.nodes.at( idx ).id )
                    continue;

                std::string nodeUrl =
                    std::string( "http://" ) +
                    std::string( chainParams.sChain.nodes.at( idx ).ip ) + std::string( ":" ) +
                    ( chainParams.sChain.nodes.at( idx ).port + 3 ).convert_to< std::string >();

                successfullDownload = downloadSnapshotFromUrl( snapshotManager, chainParams,
                    arrayCommonPublicKey, nodeUrl, isRegularSnapshot );
            } catch ( std::exception& ex ) {
                clog( VerbosityWarning, "downloadAndProccessSnapshot" )
                    << "Exception while trying to set up snapshot: "
                    << dev::nested_exception_what( ex );
            }  // for blockNumber_url
    }

    if ( !successfullDownload ) {
        throw std::runtime_error( "FATAL: tried to download snapshot from everywhere!" );
    }
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
    dev::setThreadName( "main" );
    BlockHeader::useTimestampHack = false;
    srand( time( nullptr ) );
    setCLocale();
    skutils::signal::init_common_signal_handling( ExitHandler::exitHandler );
    bool isExposeAllDebugInfo = false;

    // Init secp256k1 context by calling one of the functions.
    toPublic( {} );

    // Init defaults
    Defaults::get();
    Ethash::init();
    NoProof::init();

    /// General params for Node operation
    NodeMode nodeMode = NodeMode::Full;

    bool is_ipc = false;
    int nExplicitPortHTTP4std = -1;
    int nExplicitPortHTTP4nfo = -1;
    int nExplicitPortHTTP6std = -1;
    int nExplicitPortHTTP6nfo = -1;
    int nExplicitPortHTTPS4std = -1;
    int nExplicitPortHTTPS4nfo = -1;
    int nExplicitPortHTTPS6std = -1;
    int nExplicitPortHTTPS6nfo = -1;
    int nExplicitPortWS4std = -1;
    int nExplicitPortWS4nfo = -1;
    int nExplicitPortWS6std = -1;
    int nExplicitPortWS6nfo = -1;
    int nExplicitPortWSS4std = -1;
    int nExplicitPortWSS4nfo = -1;
    int nExplicitPortWSS6std = -1;
    int nExplicitPortWSS6nfo = -1;
    bool bTraceJsonRpcCalls = false;
    bool bTraceJsonRpcSpecialCalls = false;
    bool bEnabledAPIs_personal = false;
    bool bEnabledAPIs_admin = false;
    bool bEnabledAPIs_debug = false;
    bool bEnabledAPIs_performanceTracker = false;

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
    addClientOption( "test-enable-crash-at", po::value< std::string >()->value_name( "<id>" ),
        "For testing purpuses, deliberately crash on specified named point" );
    addClientOption( "ssl-key", po::value< std::string >()->value_name( "<path>" ),
        "Specifies path to SSL key file" );
    addClientOption( "ssl-cert", po::value< std::string >()->value_name( "<path>" ),
        "Specifies path to SSL certificate file" );
    addClientOption( "ssl-ca", po::value< std::string >()->value_name( "<path>" ),
        "Specifies path to SSL CA file" );

    /// skale
    addClientOption( "aa", po::value< string >()->value_name( "<yes/no/always>" ),
        "Auto-auth; automatic answer to all authentication questions" );

    addClientOption( "skale", "Use the Skale net" );

    addClientOption( "config", po::value< string >()->value_name( "<file>" ),
        "Configure specialised blockchain using given JSON information\n" );
    addClientOption( "main-net-url", po::value< string >()->value_name( "<url>" ),
        "Configure IMA verification algorithms to use specified Main Net url\n" );
    addClientOption( "ipc", "Enable IPC server (default: on)" );
    addClientOption( "ipcpath", po::value< string >()->value_name( "<path>" ),
        "Set .ipc socket path (default: data directory)" );
    addClientOption( "no-ipc", "Disable IPC server" );

    addClientOption( "http-port", po::value< string >()->value_name( "<port>" ),
        "Run web3 HTTP(IPv4) server(s) on specified port(and next set of ports if --acceptors "
        "> 1)" );
    addClientOption( "https-port", po::value< string >()->value_name( "<port>" ),
        "Run web3 HTTPS(IPv4) server(s) on specified port(and next set of ports if "
        "--acceptors > 1)" );
    addClientOption( "ws-port", po::value< string >()->value_name( "<port>" ),
        "Run web3 WS(IPv4) server on specified port(and next set of ports if --acceptors > 1)" );
    addClientOption( "wss-port", po::value< string >()->value_name( "<port>" ),
        "Run web3 WSS(IPv4) server(s) on specified port(and next set of ports if --acceptors > "
        "1)" );

    addClientOption( "http-port6", po::value< string >()->value_name( "<port>" ),
        "Run web3 HTTP(IPv6) server(s) on specified port(and next set of ports if --acceptors "
        "> 1)" );
    addClientOption( "https-port6", po::value< string >()->value_name( "<port>" ),
        "Run web3 HTTPS(IPv6) server(s) on specified port(and next set of ports if "
        "--acceptors > 1)" );
    addClientOption( "ws-port6", po::value< string >()->value_name( "<port>" ),
        "Run web3 WS(IPv6) server on specified port(and next set of ports if --acceptors > 1)" );
    addClientOption( "wss-port6", po::value< string >()->value_name( "<port>" ),
        "Run web3 WSS(IPv6) server(s) on specified port(and next set of ports if --acceptors > "
        "1)" );

    addClientOption( "info-http-port", po::value< string >()->value_name( "<port>" ),
        "Run informational web3 HTTP(IPv4) server(s) on specified port(and next set of ports "
        "if --info-acceptors > 1)" );
    addClientOption( "info-https-port", po::value< string >()->value_name( "<port>" ),
        "Run informational web3 HTTPS(IPv4) server(s) on specified port(and next set of ports "
        "if --info-acceptors > 1)" );
    addClientOption( "info-ws-port", po::value< string >()->value_name( "<port>" ),
        "Run informational web3 WS(IPv4) server on specified port(and next set of ports if "
        "--info-acceptors > 1)" );
    addClientOption( "info-wss-port", po::value< string >()->value_name( "<port>" ),
        "Run informational web3 WSS(IPv4) server(s) on specified port(and next set of ports if "
        "--info-acceptors > 1)" );

    addClientOption( "info-http-port6", po::value< string >()->value_name( "<port>" ),
        "Run informational web3 HTTP(IPv6) server(s) on specified port(and next set of ports "
        "if --info-acceptors > 1)" );
    addClientOption( "info-https-port6", po::value< string >()->value_name( "<port>" ),
        "Run informational web3 HTTPS(IPv6) server(s) on specified port(and next set of ports "
        "if --info-acceptors > 1)" );
    addClientOption( "info-ws-port6", po::value< string >()->value_name( "<port>" ),
        "Run informational web3 WS(IPv6) server on specified port(and next set of ports if "
        "--info-info-acceptors > 1)" );
    addClientOption( "info-wss-port6", po::value< string >()->value_name( "<port>" ),
        "Run informational web3 WSS(IPv6) server(s) on specified port(and next set of ports if "
        "--info-acceptors > 1)" );

    addClientOption( "no-snapshot-majority", po::value< string >()->value_name( "<url>" ), "" );

    addClientOption( "network-idle-timeout", po::value< long >()->value_name( "<timeout>" ),
        "Idle wait timeout for JSON RPC calls in milliseconds" );

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
        "Web socket debug logging mode(\"none\", \"basic\", \"detailed\"; default is \"none\")" );
    addClientOption( "max-connections", po::value< size_t >()->value_name( "<count>" ),
        "Max number of RPC connections(such as web3) summary for all protocols(0 is default and "
        "means unlimited)" );
    addClientOption( "max-http-queues", po::value< size_t >()->value_name( "<count>" ),
        "Max number of handler queues for HTTP/S connections per endpoint server" );
    addClientOption(
        "async-http-transfer-mode", "Use asynchronous HTTP(S) query handling, default mode" );
    addClientOption( "sync-http-transfer-mode", "Use synchronous HTTP(S) query handling" );

    addClientOption( "pg-threads", po::value< int32_t >()->value_name( "<count>" ),
        "Proxygen threads, zero means use CPU thread count" );
    addClientOption( "pg-threads-limit", po::value< int32_t >()->value_name( "<count>" ),
        "Limit number of proxygen threads, zero means no limit" );
    addClientOption( "pg-trace", "Log low level proxygen information" );

    addClientOption( "expose-all-debug-info", "Expose extra detailed debug info into log output" );

    addClientOption( "acceptors", po::value< size_t >()->value_name( "<count>" ),
        "Number of parallel RPC connection(such as web3) acceptor threads per protocol(1 is "
        "default and minimal)" );
    addClientOption( "info-acceptors", po::value< size_t >()->value_name( "<count>" ),
        "Number of informational parallel RPC connection(such as web3) acceptor threads per "
        "protocol(1 is default and minimal)" );
    addClientOption( "web3-trace", "Log HTTP/HTTPS/WS/WSS requests and responses" );
    addClientOption(
        "special-rpc-trace", "Log admin, miner, personal, and debug requests and responses" );
    addClientOption( "enable-personal-apis", "Enables personal JSON RPC APIs" );
    addClientOption( "enable-admin-apis", "Enables admi JSON RPC APIs" );
    addClientOption( "enable-debug-behavior-apis",
        "Enables debug set of JSON RPC APIs which are changing app behavior" );
    addClientOption(
        "enable-performance-tracker-apis", "Enables JSON RPC APIs for performance data recording" );

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

    addClientOption( "sgx-url", po::value< string >()->value_name( "<url>" ), "SGX server url" );

    // skale - snapshot download command
    addClientOption( "download-snapshot", po::value< string >()->value_name( "<url>" ),
        "Download snapshot from other skaled node specified by web3/json-rpc url" );
    // addClientOption( "download-target", po::value< string >()->value_name( "<port>" ),
    //    "Path of file to save downloaded snapshot to" );
    addClientOption( "start-timestamp", po::value< time_t >()->value_name( "<seconds>" ),
        "Start at specified timestamp (since epoch) - usually after downloading a snapshot" );

    LoggingOptions loggingOptions;
    po::options_description loggingProgramOptions(
        createLoggingProgramOptions( c_lineWidth, loggingOptions ) );

    po::options_description generalOptions( "GENERAL OPTIONS", c_lineWidth );
    auto addGeneralOption = generalOptions.add_options();
    addGeneralOption( "db-path,d", po::value< string >()->value_name( "<path>" ),
        ( "Load database from path (default: " + getDataDir().string() + ")" ).c_str() );
    addGeneralOption( "block-rotation-period", po::value< size_t >()->value_name( "<seconds>" ),
        "Block rotation period in seconds, zero to disable timer based block rotation." );
    addGeneralOption( "shared-space-path", po::value< string >()->value_name( "<path>" ),
        ( "Use shared space folder for temporary files (default: " + getDataDir().string() +
            "/diffs)" )
            .c_str() );
    addGeneralOption( "bls-key-file", po::value< string >()->value_name( "<file>" ),
        "Load BLS keys from file (default: none)" );
    addGeneralOption( "test-url", po::value< string >()->value_name( "<url>" ),
        "Perform test JSON RPC call to Ethereum client at sepcified URL and exit" );
    addGeneralOption( "test-json", po::value< string >()->value_name( "<JSON>" ),
        "Send specified JSON in test RPC call" );
    addGeneralOption( "test-ca", po::value< string >()->value_name( "<path>" ), "Test CA file" );
    addGeneralOption(
        "test-cert", po::value< string >()->value_name( "<path>" ), "Test certifcicate file" );
    addGeneralOption( "test-key", po::value< string >()->value_name( "<path>" ), "Test key file" );
    addGeneralOption( "colors", "Use ANSI colorized output and logging" );
    addGeneralOption( "no-colors", "Use output and logging without colors" );
    addGeneralOption( "log-value-size-limit",
        po::value< size_t >()->value_name( "<size in bytes>" ),
        "Log value size limit(zero means unlimited)" );
    addGeneralOption( "log-json-string-limit",
        po::value< size_t >()->value_name( "<number of chars>" ),
        "JSON string value length limit for logging, specify 0 for unlimited" );
    addGeneralOption( "log-tx-params-limit",
        po::value< size_t >()->value_name( "<number of chars>" ),
        "Transaction params length limit in eth_sendRawTransaction calls for logging, specify 0 "
        "for unlimited" );
    addGeneralOption( "dispatch-threads", po::value< size_t >()->value_name( "<count>" ),
        "Number of threads to run task dispatcher, default is CPU count * 2" );
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

    if ( vm.count( "network-idle-timeout" ) )
        skutils::rest::g_nClientConnectionTimeoutMS = vm["network-idle-timeout"].as< long >();

    if ( vm.count( "test-enable-crash-at" ) ) {
        std::string crash_at = vm["test-enable-crash-at"].as< string >();
        batched_io::test_enable_crash_at( crash_at );
    }

    if ( vm.count( "log-value-size-limit" ) ) {
        int n = vm["log-value-size-limit"].as< size_t >();
        cc::_max_value_size_ = ( n > 0 ) ? n : std::string::npos;
    }
    if ( vm.count( "log-json-string-limit" ) ) {
        int n = vm["log-json-string-limit"].as< size_t >();
        SkaleServerOverride::g_nMaxStringValueLengthForJsonLogs = n;
    }
    if ( vm.count( "log-tx-params-limit" ) ) {
        int n = vm["log-tx-params-limit"].as< size_t >();
        SkaleServerOverride::g_nMaxStringValueLengthForTransactionParams = n;
    }

    if ( vm.count( "test-url" ) ) {
        std::string strJSON, strURL = vm["test-url"].as< std::string >(), strPathCA, strPathCert,
                             strPathKey;
        if ( vm.count( "test-json" ) )
            strJSON = vm["test-json"].as< std::string >();
        if ( vm.count( "test-ca" ) )
            strPathCA = vm["test-ca"].as< std::string >();
        if ( vm.count( "test-cert" ) )
            strPathCert = vm["test-cert"].as< std::string >();
        if ( vm.count( "test-key" ) )
            strPathKey = vm["test-key"].as< std::string >();
        skutils::url u;
        try {
            u = skutils::url( strURL );
            std::cout << ( cc::debug( "Using URL" ) + cc::debug( "................" ) +
                           cc::u( u.str() ) + "\n" );
        } catch ( const std::exception& ex ) {
            std::cout << ( cc::fatal( "ERROR:" ) + cc::error( " Failed to parse test URL: " ) +
                           cc::warn( ex.what() ) + "\n" );
            return EX_TEMPFAIL;
        } catch ( ... ) {
            std::cout << ( cc::fatal( "ERROR:" ) + cc::error( " Failed to parse test URL: " ) +
                           cc::warn( "unknown exception" ) + "\n" );
            return EX_TEMPFAIL;
        }
        nlohmann::json joIn, joOut;
        try {
            if ( !strJSON.empty() ) {
                joIn = nlohmann::json::parse( strJSON );
                std::cout << ( cc::debug( "Input JSON is" ) + cc::debug( "............" ) +
                               cc::j( joIn ) + "\n" );
            } else
                std::cout << ( cc::error( "NOTICE:" ) +
                               cc::warn( " No valid JSON specified for test call" ) + "\n" );
        } catch ( const std::exception& ex ) {
            std::cout << ( cc::fatal( "ERROR:" ) +
                           cc::error( " Failed to parse specified test JSON: " ) +
                           cc::warn( ex.what() ) + "\n" );
            return EX_TEMPFAIL;
        } catch ( ... ) {
            std::cout << ( cc::fatal( "ERROR:" ) +
                           cc::error( " Failed to parse specified test JSON: " ) +
                           cc::warn( "unknown exception" ) + "\n" );
            return EX_TEMPFAIL;
        }
        skutils::http::SSL_client_options optsSSL;
        if ( !strPathCA.empty() ) {
            optsSSL.ca_file = skutils::tools::trim_copy( strPathCA );
            std::cout << ( cc::debug( "Using CA file " ) + cc::debug( "..........." ) +
                           cc::p( strPathCA ) + "\n" );
        }
        if ( !strPathCert.empty() ) {
            optsSSL.client_cert = skutils::tools::trim_copy( strPathCert );
            std::cout << ( cc::debug( "Using CERT file " ) + cc::debug( "........." ) +
                           cc::p( strPathCert ) + "\n" );
        }
        if ( !strPathKey.empty() ) {
            optsSSL.client_key = skutils::tools::trim_copy( strPathKey );
            std::cout << ( cc::debug( "Using KEY file " ) + cc::debug( ".........." ) +
                           cc::p( strPathKey ) + "\n" );
        }
        try {
            skutils::rest::client cli( skutils::rest::g_nClientConnectionTimeoutMS );
            cli.optsSSL_ = optsSSL;
            cli.open( u );
            const bool isAutoGenJsonID = true;
            const skutils::rest::e_data_fetch_strategy edfs =
                skutils::rest::e_data_fetch_strategy::edfs_default;
            const std::chrono::milliseconds wait_step = std::chrono::milliseconds( 20 );
            const size_t cntSteps = 1000;
            const bool isReturnErrorResponse = true;
            skutils::rest::data_t d =
                cli.call( joIn, isAutoGenJsonID, edfs, wait_step, cntSteps, isReturnErrorResponse );
            if ( !d.err_s_.empty() )
                throw std::runtime_error( "REST call error: " + d.err_s_ );
            if ( d.empty() )
                throw std::runtime_error( "EMPTY answer received" );
            std::cout << ( cc::debug( "Raw received data is" ) + cc::debug( "....." ) +
                           cc::normal( d.s_ ) + "\n" );
            joOut = nlohmann::json::parse( d.s_ );
            std::cout << ( cc::debug( "Output JSON is" ) + cc::debug( "..........." ) +
                           cc::j( joOut ) + "\n" );
        } catch ( const std::exception& ex ) {
            std::cout << ( cc::fatal( "ERROR:" ) + cc::error( " JSON RPC call failed: " ) +
                           cc::warn( ex.what() ) + "\n" );
            return EX_TEMPFAIL;
        } catch ( ... ) {
            std::cout << ( cc::fatal( "ERROR:" ) + cc::error( " JSON RPC call failed: " ) +
                           cc::warn( "unknown exception" ) + "\n" );
            return EX_TEMPFAIL;
        }
        return 0;
    }

    std::cout << cc::bright( "skaled " ) << cc::sunny( Version ) << "\n"
              << cc::bright( "client " ) << clientVersionColorized() << "\n";
    std::cout.flush();
    version();

    pid_t this_process_pid = getpid();
    std::cout << cc::debug( "This process " ) << cc::info( "PID" ) << cc::debug( "=" )
              << cc::size10( size_t( this_process_pid ) ) << "\n";
    std::cout.flush();

    setupLogging( loggingOptions );

    const size_t nCpuCount = skutils::tools::cpu_count();
    size_t nDispatchThreads = nCpuCount * 2;
    if ( vm.count( "dispatch-threads" ) ) {
        size_t n = vm["dispatch-threads"].as< size_t >();
        const size_t nMin = 4;
        if ( n < nMin )
            n = nMin;
        nDispatchThreads = n;
    }
    clog( VerbosityInfo, "main" ) << cc::debug( "Using " ) << cc::size10( nDispatchThreads )
                                  << cc::debug( " threads in task dispatcher" ) << std::endl;
    skutils::dispatch::default_domain( nDispatchThreads );
    // skutils::dispatch::default_domain( 48 );

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
                throw std::runtime_error( "Bad config file path" );
            configJSON = contentsString( configPath.string() );
            if ( configJSON.empty() )
                throw std::runtime_error( "Config file probably not found" );
            chainParams = chainParams.loadConfig( configJSON, configPath );
            chainConfigIsSet = true;
            // TODO avoid double-parse
            joConfig = nlohmann::json::parse( configJSON );
            chainConfigParsed = true;
            dev::eth::g_configAccesssor.reset(
                new skutils::json_config_file_accessor( configPath.string() ) );
            dev::db::DBFactory::setReopenPeriodMs( chainParams.sChain.levelDBReopenIntervalMs );
        } catch ( const char* str ) {
            clog( VerbosityError, "main" ) << "Error: " << str << ": " << configPath;
            return EX_USAGE;
        } catch ( const json_spirit::Error_position& err ) {
            clog( VerbosityError, "main" ) << "error in parsing config json:";
            clog( VerbosityError, "main" ) << configJSON;
            clog( VerbosityError, "main" ) << err.reason_ << " line " << err.line_;
            return EX_CONFIG;
        } catch ( const std::exception& ex ) {
            clog( VerbosityError, "main" ) << "provided configuration is incorrect";
            clog( VerbosityError, "main" ) << configJSON;
            clog( VerbosityError, "main" ) << nested_exception_what( ex );
            return EX_CONFIG;
        } catch ( ... ) {
            clog( VerbosityError, "main" ) << "provided configuration is incorrect";
            clog( VerbosityError, "main" ) << configJSON;
            return EX_CONFIG;
        }
    }

    std::shared_ptr< StatusAndControl > statusAndControl = std::make_shared< StatusAndControlFile >(
        boost::filesystem::path( configPath ).remove_filename() );
    // for now, leave previous values in file (for case of crash)

    if ( vm.count( "main-net-url" ) ) {
        if ( !g_configAccesssor ) {
            clog( VerbosityError, "main" )
                << "config=<path> should be specified before --main-net-url=<url>\n";
            return EX_SOFTWARE;
        }
        skutils::json_config_file_accessor::g_strImaMainNetURL =
            skutils::tools::trim_copy( vm["main-net-url"].as< string >() );
        if ( !g_configAccesssor->validateImaMainNetURL() ) {
            clog( VerbosityError, "main" )
                << "bad --main-net-url=<url> parameter value: "
                << skutils::json_config_file_accessor::g_strImaMainNetURL << "\n";
            return EX_SOFTWARE;
        }
        clog( VerbosityDebug, "main" )
            << cc::notice( "Main Net URL" ) + cc::debug( " is: " )
            << cc::u( skutils::json_config_file_accessor::g_strImaMainNetURL );
    }

    if ( !chainConfigIsSet )
        // default to skale if not already set with `--config`
        chainParams = ChainParams( genesisInfo( eth::Network::Skale ) );

    if ( chainConfigParsed ) {
        try {
            size_t n = joConfig["skaleConfig"]["nodeInfo"]["log-value-size-limit"].get< size_t >();
            cc::_max_value_size_ = ( n > 0 ) ? n : std::string::npos;
        } catch ( ... ) {
        }
        try {
            size_t n = joConfig["skaleConfig"]["nodeInfo"]["log-json-string-limit"].get< size_t >();
            SkaleServerOverride::g_nMaxStringValueLengthForJsonLogs = n;
        } catch ( ... ) {
        }
        try {
            size_t n = joConfig["skaleConfig"]["nodeInfo"]["log-tx-params-limit"].get< size_t >();
            SkaleServerOverride::g_nMaxStringValueLengthForTransactionParams = n;
        } catch ( ... ) {
        }
    }

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
    clog( VerbosityDebug, "main" ) << cc::notice( "IPC server" ) + cc::debug( " is: " )
                                   << ( is_ipc ? cc::success( "on" ) : cc::error( "off" ) );

    // First, get "httpRpcPort", "httpsRpcPort", "wsRpcPort", "wssRpcPort" ... from config.json
    // Second, get them from command line parameters (higher priority source)
    if ( chainConfigParsed ) {
        auto fnExtractPort = [&]( const char* strConfigVarName, const char* strCommandLineKey,
                                 const char* strDescription ) -> int {
            int nPort = -1;
            try {
                if ( joConfig["skaleConfig"]["nodeInfo"].count( strConfigVarName ) )
                    nPort = joConfig["skaleConfig"]["nodeInfo"][strConfigVarName].get< int >();
            } catch ( ... ) {
            }
            if ( !( 0 <= nPort && nPort <= 65535 ) )
                nPort = -1;
            else
                clog( VerbosityDebug, "main" )
                    << cc::debug( "Got " )
                    << cc::notice( strDescription ) + cc::debug( " from configuration JSON: " )
                    << cc::num10( nPort );
            if ( vm.count( strCommandLineKey ) ) {
                std::string strPort = vm[strCommandLineKey].as< string >();
                if ( !strPort.empty() ) {
                    nPort = atoi( strPort.c_str() );
                    if ( !( 0 <= nPort && nPort <= 65535 ) )
                        nPort = -1;
                    else
                        clog( VerbosityDebug, "main" )
                            << cc::debug( "Got " )
                            << cc::notice( strDescription ) + cc::debug( " from command line: " )
                            << cc::num10( nPort );
                }
            }
            return nPort;
        };
        nExplicitPortHTTP4std = fnExtractPort( "httpRpcPort", "http-port", "HTTP/4/std port" );
        nExplicitPortHTTP4nfo =
            fnExtractPort( "infoHttpRpcPort", "info-http-port", "HTTP/4/nfo port" );
        nExplicitPortHTTP6std = fnExtractPort( "httpRpcPort6", "http-port6", "HTTP/6/std port" );
        nExplicitPortHTTP6nfo =
            fnExtractPort( "infoHttpRpcPort6", "info-http-port6", "HTTP/6/nfo port" );
        nExplicitPortHTTPS4std = fnExtractPort( "httpsRpcPort", "https-port", "HTTPS/4/std port" );
        nExplicitPortHTTPS4nfo =
            fnExtractPort( "infoHttpsRpcPort", "info-https-port", "HTTPS/4/nfo port" );
        nExplicitPortHTTPS6std =
            fnExtractPort( "httpsRpcPort6", "https-port6", "HTTPS/6/std port" );
        nExplicitPortHTTPS6nfo =
            fnExtractPort( "infoHttpsRpcPort6", "info-https-port6", "HTTPS/6/nfo port" );
        nExplicitPortWS4std = fnExtractPort( "wsRpcPort", "ws-port", "WS/4/std port" );
        nExplicitPortWS4nfo = fnExtractPort( "infoWsRpcPort", "info-ws-port", "WS/4/nfo port" );
        nExplicitPortWS6std = fnExtractPort( "wsRpcPort6", "ws-port6", "WS/6/std port" );
        nExplicitPortWS6nfo = fnExtractPort( "infoWsRpcPort6", "info-ws-port6", "WS/6/nfo port" );
        nExplicitPortWSS4std = fnExtractPort( "wssRpcPort", "wss-port", "WSS/4/std port" );
        nExplicitPortWSS4nfo = fnExtractPort( "infoWssRpcPort", "info-wss-port", "WSS/4/nfo port" );
        nExplicitPortWSS6std = fnExtractPort( "wssRpcPort6", "wss-port6", "WSS/6/std port" );
        nExplicitPortWSS6nfo =
            fnExtractPort( "infoWssRpcPort6", "info-wss-port6", "WSS/6/nfo port" );
    }  // if ( chainConfigParsed )

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
    clog( VerbosityDebug, "main" )
        << cc::info( "JSON RPC" ) << cc::debug( " trace logging mode is " )
        << cc::flag_ed( bTraceJsonRpcCalls );

    // First, get "special-rpc-trace" from config.json
    // Second, get it from command line parameter (higher priority source)
    if ( chainConfigParsed ) {
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "special-rpc-trace" ) )
                bTraceJsonRpcSpecialCalls =
                    joConfig["skaleConfig"]["nodeInfo"]["special-rpc-trace"].get< bool >();
        } catch ( ... ) {
        }
    }
    if ( vm.count( "special-rpc-trace" ) )
        bTraceJsonRpcSpecialCalls = true;
    clog( VerbosityDebug, "main" )
        << cc::info( "Special JSON RPC" ) << cc::debug( " trace logging mode is " )
        << cc::flag_ed( bTraceJsonRpcSpecialCalls );

    // First, get "enable-personal-apis", "enable-admin-apis", "enable-debug-behavior-apis",
    // "enable-performance-tracker-apis" from config.json Second, get it from command line parameter
    // (higher priority source)
    if ( chainConfigParsed ) {
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "enable-personal-apis" ) )
                bEnabledAPIs_personal =
                    joConfig["skaleConfig"]["nodeInfo"]["enable-personal-apis"].get< bool >();
        } catch ( ... ) {
        }
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "enable-admin-apis" ) )
                bEnabledAPIs_admin =
                    joConfig["skaleConfig"]["nodeInfo"]["enable-admin-apis"].get< bool >();
        } catch ( ... ) {
        }
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "enable-debug-behavior-apis" ) )
                bEnabledAPIs_debug =
                    joConfig["skaleConfig"]["nodeInfo"]["enable-debug-behavior-apis"].get< bool >();
        } catch ( ... ) {
        }
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "enable-performance-tracker-apis" ) )
                bEnabledAPIs_performanceTracker =
                    joConfig["skaleConfig"]["nodeInfo"]["enable-performance-tracker-apis"]
                        .get< bool >();
        } catch ( ... ) {
        }
    }
    if ( vm.count( "enable-personal-apis" ) )
        bEnabledAPIs_personal = true;
    if ( vm.count( "enable-admin-apis" ) )
        bEnabledAPIs_admin = true;
    if ( vm.count( "enable-debug-behavior-apis" ) )
        bEnabledAPIs_debug = true;
    if ( vm.count( "enable-performance-tracker-apis" ) )
        bEnabledAPIs_performanceTracker = true;
    clog( VerbosityWarning, "main" )
        << cc::warn( "Important notice: " ) << cc::debug( "Programmatic " )
        << cc::info( "enable-personal-apis" ) << cc::debug( " mode is " )
        << cc::flag_ed( bEnabledAPIs_personal );
    clog( VerbosityWarning, "main" )
        << cc::warn( "Important notice: " ) << cc::debug( "Programmatic " )
        << cc::info( "enable-admin-apis" ) << cc::debug( " mode is " )
        << cc::flag_ed( bEnabledAPIs_admin );
    clog( VerbosityWarning, "main" )
        << cc::warn( "Important notice: " ) << cc::debug( "Programmatic " )
        << cc::info( "enable-debug-behavior-apis" ) << cc::debug( " mode is " )
        << cc::flag_ed( bEnabledAPIs_debug );
    clog( VerbosityWarning, "main" )
        << cc::warn( "Important notice: " ) << cc::debug( "Programmatic " )
        << cc::info( "enable-performance-tracker-apis" ) << cc::debug( " mode is " )
        << cc::flag_ed( bEnabledAPIs_performanceTracker );

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
    clog( VerbosityWarning, "main" )
        << cc::warn( "Important notice: " ) << cc::debug( "Programmatic " )
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
    clog( VerbosityWarning, "main" )
        << cc::warn( "Important notice: " ) << cc::debug( "Programmatic " )
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
    clog( VerbosityDebug, "main" )
        << cc::notice( "IPC path" ) + cc::debug( " is: " ) << cc::p( strPathIPC );
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

    UnsafeRegion::init( getDataDir() );
    if ( UnsafeRegion::isActive() ) {
        clog( VerbosityError, "main" ) << "FATAL "
                                       << "Previous skaled shutdown was too hard, need to repair!";
        return int( ExitHandler::ec_state_root_mismatch );
    }  // if bad exit

    size_t clockDbRotationPeriodInSeconds = 0;
    if ( chainConfigParsed ) {
        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "block-rotation-period" ) )
                clockDbRotationPeriodInSeconds =
                    joConfig["skaleConfig"]["nodeInfo"]["block-rotation-period"].get< size_t >();
        } catch ( ... ) {
            clockDbRotationPeriodInSeconds = 0;
        }
    }
    if ( vm.count( "block-rotation-period" ) )
        clockDbRotationPeriodInSeconds = vm["block-rotation-period"].as< size_t >();
    if ( clockDbRotationPeriodInSeconds > 0 )
        clog( VerbosityInfo, "main" )
            << cc::debug( "Timer-based " ) + cc::notice( "Block Rotation" ) +
                   cc::debug( " period is: " )
            << cc::size10( clockDbRotationPeriodInSeconds );


    ///////////////// CACHE PARAMS ///////////////
    extern chrono::system_clock::duration c_collectionDuration;
    extern unsigned c_collectionQueueSize;
    extern unsigned c_maxCacheSize;
    extern unsigned c_minCacheSize;

    unsigned c_transactionQueueSize = 100000;
    unsigned c_futureTransactionQueueSize = 16000;
    unsigned c_transactionQueueSizeBytes = 12322916;
    unsigned c_futureTransactionQueueSizeBytes = 24645833;

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
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "futureTransactionQueueSize" ) )
                c_futureTransactionQueueSize =
                    joConfig["skaleConfig"]["nodeInfo"]["futureTransactionQueueSize"]
                        .get< unsigned >();
        } catch ( ... ) {
        }

        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "transactionQueueLimitBytes" ) )
                c_transactionQueueSizeBytes =
                    joConfig["skaleConfig"]["nodeInfo"]["transactionQueueLimitBytes"]
                        .get< unsigned >();
        } catch ( ... ) {
        }

        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "futureTransactionQueueLimitBytes" ) )
                c_futureTransactionQueueSizeBytes =
                    joConfig["skaleConfig"]["nodeInfo"]["futureTransactionQueueLimitBytes"]
                        .get< unsigned >();
        } catch ( ... ) {
        }

        try {
            if ( joConfig["skaleConfig"]["nodeInfo"].count( "maxOpenLeveldbFiles" ) )
                dev::db::c_maxOpenLeveldbFiles =
                    joConfig["skaleConfig"]["nodeInfo"]["maxOpenLeveldbFiles"].get< unsigned >();
        } catch ( ... ) {
        }

        if ( vm.count( "log-value-size-limit" ) ) {
            int n = vm["log-value-size-limit"].as< size_t >();
            cc::_max_value_size_ = ( n > 0 ) ? n : std::string::npos;
        }
        if ( vm.count( "log-json-string-limit" ) ) {
            int n = vm["log-json-string-limit"].as< size_t >();
            SkaleServerOverride::g_nMaxStringValueLengthForJsonLogs = n;
        }
        if ( vm.count( "log-tx-params-limit" ) ) {
            int n = vm["log-tx-params-limit"].as< size_t >();
            SkaleServerOverride::g_nMaxStringValueLengthForTransactionParams = n;
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
    if ( vm.count( "password" ) )
        passwordsToNote.push_back( vm["password"].as< string >() );
    if ( vm.count( "master" ) ) {
        masterPassword = vm["master"].as< string >();
        masterSet = true;
    }
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

    if ( vm.count( "sgx-url" ) ) {
        std::string strURL = vm["sgx-url"].as< string >();
        // chainParams.nodeInfo.sgxServerUrl = strURL;
        skutils::url u( strURL );
        u.user_info( "" );
        u.user_name_and_password( "", "", true );
        u.path( "" );
        u.fragment( "" );
        u.set_query();
        strURL = u.str();
        chainParams.nodeInfo.sgxServerUrl = strURL;
    }

    std::shared_ptr< SharedSpace > sharedSpace;
    if ( vm.count( "shared-space-path" ) ) {
        try {
            fs::create_directory( vm["shared-space-path"].as< string >() );
        } catch ( const fs::filesystem_error& ex ) {
        }

        sharedSpace.reset( new SharedSpace( vm["shared-space-path"].as< string >() ) );
    }

    bool downloadSnapshotFlag = false;
    std::shared_ptr< SnapshotManager > snapshotManager;

    if ( vm.count( "download-snapshot" ) ) {
        downloadSnapshotFlag = true;
    }

    std::string urlToDownloadSnapshotFrom = "";
    if ( vm.count( "no-snapshot-majority" ) ) {
        downloadSnapshotFlag = true;
        urlToDownloadSnapshotFrom = vm["no-snapshot-majority"].as< string >();
        clog( VerbosityInfo, "main" )
            << "Manually set url to download snapshot from: " << urlToDownloadSnapshotFrom;
    }

    if ( chainParams.sChain.snapshotIntervalSec > 0 || downloadSnapshotFlag ) {
        std::vector< std::string > coreVolumes = { BlockChain::getChainDirName( chainParams ),
            "filestorage", "prices_" + chainParams.nodeInfo.id.str() + ".db",
            "blocks_" + chainParams.nodeInfo.id.str() + ".db" };
        snapshotManager.reset( new SnapshotManager(
            chainParams, getDataDir(), sharedSpace ? sharedSpace->getPath() : "" ) );
    }

    if ( downloadSnapshotFlag ) {
        statusAndControl->setExitState( StatusAndControl::StartAgain, true );
        statusAndControl->setExitState( StatusAndControl::StartFromSnapshot, true );
        statusAndControl->setSubsystemRunning( StatusAndControl::SnapshotDownloader, true );

        std::unique_ptr< std::lock_guard< SharedSpace > > sharedSpace_lock;
        if ( sharedSpace )
            sharedSpace_lock.reset( new std::lock_guard< SharedSpace >( *sharedSpace ) );

        try {
            downloadAndProccessSnapshot(
                snapshotManager, chainParams, urlToDownloadSnapshotFrom, true );

            // if we dont have 0 snapshot yet
            try {
                snapshotManager->isSnapshotHashPresent( 0 );
            } catch ( SnapshotManager::SnapshotAbsent& ex ) {
                // sleep before send skale_getSnapshot again - will receive error
                clog( VerbosityInfo, "main" )
                    << std::string( "Will sleep for " )
                    << chainParams.sChain.snapshotDownloadInactiveTimeout +
                           dev::rpc::Skale::snapshotDownloadFragmentMonitorThreadTimeout()
                    << std::string( " seconds before downloading 0 snapshot" );
                sleep( chainParams.sChain.snapshotDownloadInactiveTimeout +
                       dev::rpc::Skale::snapshotDownloadFragmentMonitorThreadTimeout() );

                downloadAndProccessSnapshot(
                    snapshotManager, chainParams, urlToDownloadSnapshotFrom, false );
            }

        } catch ( std::exception& ) {
            std::throw_with_nested( std::runtime_error(
                std::string( " Fatal error in downloadAndProccessSnapshot! Will exit " ) ) );
        }

    }  // if --download-snapshot

    // download 0 snapshot if needed
    if ( chainParams.nodeInfo.syncNode ) {
        auto bc = BlockChain( chainParams, getDataDir() );
        if ( bc.number() == 0 ) {
            if ( chainParams.nodeInfo.syncFromCatchup && !downloadSnapshotFlag ) {
                statusAndControl->setExitState( StatusAndControl::StartAgain, true );
                statusAndControl->setExitState( StatusAndControl::StartFromSnapshot, true );
                statusAndControl->setSubsystemRunning( StatusAndControl::SnapshotDownloader, true );

                try {
                    downloadAndProccessSnapshot(
                        snapshotManager, chainParams, urlToDownloadSnapshotFrom, false );
                    snapshotManager->restoreSnapshot( 0 );
                } catch ( SnapshotManager::SnapshotAbsent& ) {
                    clog( VerbosityWarning, "main" ) << "Snapshot for 0 block is not found";
                }
            }
        }
    }

    statusAndControl->setSubsystemRunning( StatusAndControl::SnapshotDownloader, false );

    statusAndControl->setExitState( StatusAndControl::StartAgain, true );
    statusAndControl->setExitState( StatusAndControl::StartFromSnapshot, false );

    // it was needed for snapshot downloading
    if ( chainParams.sChain.snapshotIntervalSec <= 0 ) {
        snapshotManager = nullptr;
    }

    time_t startTimestamp = 0;
    if ( vm.count( "start-timestamp" ) ) {
        startTimestamp = vm["start-timestamp"].as< time_t >();
    }

    if ( time( NULL ) < startTimestamp ) {
        statusAndControl->setSubsystemRunning( StatusAndControl::WaitingForTimestamp, true );
        std::cout << "\nWill start at localtime " << ctime( &startTimestamp ) << std::endl;
        do
            sleep( 1 );
        while ( time( NULL ) < startTimestamp );
        statusAndControl->setSubsystemRunning( StatusAndControl::WaitingForTimestamp, false );
    }

    if ( loggingOptions.verbosity > 0 )
        clog( VerbosityInfo, "main" ) << cc::attention( "skaled, a C++ Skale client" ) << "\n";

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
    auto caps = set< string >{ "eth" };

    //    dev::WebThreeDirect web3( WebThreeDirect::composeClientVersion( "skaled" ), getDataDir(),
    //    "",
    //        chainParams, withExisting, nodeMode == NodeMode::Full ? caps : set< string >(), false
    //        );

    std::shared_ptr< GasPricer > gasPricer;

    auto rotationFlagDirPath = configPath.parent_path();
    auto instanceMonitor = make_shared< InstanceMonitor >( rotationFlagDirPath, statusAndControl );
    SkaleDebugInterface debugInterface;

    if ( getDataDir().size() )
        Defaults::setDBPath( getDataDir() );
    if ( nodeMode == NodeMode::Full && caps.count( "eth" ) ) {
        Ethash::init();
        NoProof::init();

        if ( chainParams.sealEngineName == Ethash::name() ) {
            g_client.reset( new eth::EthashClient( chainParams, ( int ) chainParams.networkID,
                shared_ptr< GasPricer >(), snapshotManager, instanceMonitor, getDataDir(),
                withExisting,
                TransactionQueue::Limits{ c_transactionQueueSize, c_futureTransactionQueueSize,
                    c_transactionQueueSizeBytes, c_futureTransactionQueueSizeBytes } ) );
        } else if ( chainParams.sealEngineName == NoProof::name() ) {
            g_client.reset( new eth::Client( chainParams, ( int ) chainParams.networkID,
                shared_ptr< GasPricer >(), snapshotManager, instanceMonitor, getDataDir(),
                withExisting,
                TransactionQueue::Limits{ c_transactionQueueSize, c_futureTransactionQueueSize,
                    c_transactionQueueSizeBytes, c_futureTransactionQueueSizeBytes } ) );
        } else
            BOOST_THROW_EXCEPTION( ChainParamsInvalid() << errinfo_comment(
                                       "Unknown seal engine: " + chainParams.sealEngineName ) );

        g_client->dbRotationPeriod(
            ( ( clock_t )( clockDbRotationPeriodInSeconds ) ) * CLOCKS_PER_SEC );

        // XXX nested lambdas and strlen hacks..
        auto client_debug_handler = g_client->getDebugHandler();
        debugInterface.add_handler( [client_debug_handler]( const std::string& arg ) -> string {
            if ( arg.find( "Client " ) == 0 )
                return client_debug_handler( arg.substr( 7 ) );
            else
                return "";
        } );
        g_client->setAuthor( chainParams.sChain.blockAuthor );

        DefaultConsensusFactory cons_fact( *g_client );
        setenv( "DATA_DIR", getDataDir().c_str(), 0 );

        std::shared_ptr< SkaleHost > skaleHost = std::make_shared< SkaleHost >( *g_client,
            &cons_fact, instanceMonitor, skutils::json_config_file_accessor::g_strImaMainNetURL,
            !chainParams.nodeInfo.syncNode );
        dev::eth::g_skaleHost = skaleHost;

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

        skale_get_buildinfo();
        g_client->setExtraData( dev::bytes{ 's', 'k', 'a', 'l', 'e' } );

        // this must be last! (or client will be mining blocks before this!)
        g_client->startWorking();
        statusAndControl->setSubsystemRunning( StatusAndControl::Blockchain, true );
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

    if ( nodeMode == NodeMode::Full ) {
        g_client->setSealer( m.minerType() );
        if ( networkID != NoNetworkID )
            g_client->setNetworkId( networkID );
    }

    clog( VerbosityInfo, "main" ) << "Mining Beneficiary: " << g_client->author();

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
            clog( VerbosityError, "main" ) << "Bad "
                                           << "--aa"
                                           << " option: " << strAA << "\n";
            return EX_USAGE;
        }
        clog( VerbosityDebug, "main" )
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
                _t.userReadable(
                    isProxy,
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
                { "yes", "n", "N", "no", "NO", "always" } );
            if ( r == "always" )
                allowedDestinations.insert( _t.to );
            return r == "yes" || r == "always";
        };
    if ( chainParams.nodeInfo.ip.empty() ) {
        clog( VerbosityWarning, "main" )
            << cc::info( "IPv4" )
            << cc::warn( " bind address is not set, will not start RPC on this protocol" );
        nExplicitPortHTTP4std = nExplicitPortHTTPS4std = nExplicitPortHTTP4nfo =
            nExplicitPortHTTPS4nfo = nExplicitPortWS4std = nExplicitPortWSS4std =
                nExplicitPortWS4nfo = nExplicitPortWSS4nfo = -1;
    }
    if ( chainParams.nodeInfo.ip6.empty() ) {
        clog( VerbosityWarning, "main" )
            << cc::info( "IPv6" )
            << cc::warn( " bind address is not set, will not start RPC on this protocol" );
        nExplicitPortHTTP6std = nExplicitPortHTTPS6std = nExplicitPortHTTP6nfo =
            nExplicitPortHTTPS6nfo = nExplicitPortWS6std = nExplicitPortWSS6std =
                nExplicitPortWS6nfo = nExplicitPortWSS6nfo = -1;
    }
    if ( is_ipc || nExplicitPortHTTP4std > 0 || nExplicitPortHTTPS4std > 0 ||
         nExplicitPortHTTP6std > 0 || nExplicitPortHTTPS6std > 0 || nExplicitPortHTTP4nfo > 0 ||
         nExplicitPortHTTPS4nfo > 0 || nExplicitPortHTTP6nfo > 0 || nExplicitPortHTTPS6nfo > 0 ||
         nExplicitPortWS4std > 0 || nExplicitPortWSS4std > 0 || nExplicitPortWS6std > 0 ||
         nExplicitPortWSS6std > 0 || nExplicitPortWS4nfo > 0 || nExplicitPortWSS4nfo > 0 ||
         nExplicitPortWS6nfo > 0 || nExplicitPortWSS6nfo > 0 ) {
        using FullServer = ModularServer< rpc::EthFace,
            rpc::SkaleFace,   /// skale
            rpc::SkaleStats,  /// skaleStats
            rpc::NetFace, rpc::Web3Face, rpc::PersonalFace, rpc::AdminEthFace,
            // SKALE rpc::AdminNetFace,
            rpc::DebugFace, rpc::SkalePerformanceTracker, rpc::TracingFace, rpc::TestFace >;

        sessionManager.reset( new rpc::SessionManager() );
        accountHolder.reset( new SimpleAccountHolder(
            [&]() { return g_client.get(); }, getAccountPassword, keyManager, authenticator ) );

        std::string argv_string;
        {  // block
            ostringstream ss;
            for ( int i = 1; i < argc; ++i )
                ss << argv[i] << " ";
            argv_string = ss.str();
        }  // block

        if ( chainConfigParsed ) {
            try {
                isExposeAllDebugInfo =
                    joConfig["skaleConfig"]["nodeInfo"]["expose-all-debug-info"].get< bool >();
            } catch ( ... ) {
            }
        }
        if ( vm.count( "expose-all-debug-info" ) )
            isExposeAllDebugInfo = true;


        auto pNetFace = new rpc::Net( chainParams );
        auto pWeb3Face = new rpc::Web3( clientVersion() );
        auto pEthFace = new rpc::Eth( configPath.string(), *g_client, *accountHolder.get() );
        auto pSkaleFace = new rpc::Skale( *g_client, sharedSpace );
        auto pSkaleStatsFace = new rpc::SkaleStats( configPath.string(), *g_client, chainParams );
        pSkaleStatsFace->isExposeAllDebugInfo_ = isExposeAllDebugInfo;
        auto pPersonalFace = bEnabledAPIs_personal ?
                                 new rpc::Personal( keyManager, *accountHolder, *g_client ) :
                                 nullptr;
        auto pAdminEthFace = bEnabledAPIs_admin ? new rpc::AdminEth( *g_client, *gasPricer.get(),
                                                      keyManager, *sessionManager.get() ) :
                                                  nullptr;
        auto pDebugFace = bEnabledAPIs_debug ?
                              new rpc::Debug( *g_client, &debugInterface, argv_string ) :
                              nullptr;

#ifdef HISTORIC_STATE
        // tracing interface is always enabled for the historic state nodes
        auto pTracingFace = new rpc::Tracing( *g_client, argv_string );
#else
        // tracing interface is only enabled for the historic state nodes
        auto pTracingFace = nullptr;
#endif


        auto pPerformanceTrackerFace = bEnabledAPIs_performanceTracker ?
                                           new rpc::SkalePerformanceTracker( configPath.string() ) :
                                           nullptr;

        g_jsonrpcIpcServer.reset( new FullServer( pEthFace, pSkaleFace, pSkaleStatsFace, pNetFace,
            pWeb3Face, pPersonalFace, pAdminEthFace, pDebugFace, pPerformanceTrackerFace,
            pTracingFace, nullptr ) );

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

        auto fnCheckPort = [&]( int& nPort, const char* strCommandLineKey ) -> bool {
            if ( nPort <= 0 || nPort >= 65536 ) {
                clog( VerbosityError, "main" )
                    << cc::error( "WARNING:" ) << cc::warn( " No valid port value provided with " )
                    << cc::info( std::string( "--" ) + strCommandLineKey ) << cc::warn( "=" )
                    << cc::info( "number" );
                return false;
            }
            return true;
        };
        if ( !fnCheckPort( nExplicitPortHTTP4std, "http-port" ) ) {
            // return EX_USAGE;
        }
        if ( !fnCheckPort( nExplicitPortHTTP4nfo, "info-http-port" ) ) {
            // return EX_USAGE;
        }
        if ( !fnCheckPort( nExplicitPortHTTP6std, "http-port6" ) ) {
            // return EX_USAGE;
        }
        if ( !fnCheckPort( nExplicitPortHTTP6nfo, "info-http-port6" ) ) {
            // return EX_USAGE;
        }
        if ( !fnCheckPort( nExplicitPortHTTPS4std, "https-port" ) ) {
            // return EX_USAGE;
        }
        if ( !fnCheckPort( nExplicitPortHTTPS4nfo, "info-https-port" ) ) {
            // return EX_USAGE;
        }
        if ( !fnCheckPort( nExplicitPortHTTPS6std, "https-port6" ) ) {
            // return EX_USAGE;
        }
        if ( !fnCheckPort( nExplicitPortHTTPS6nfo, "info-https-port6" ) ) {
            // return EX_USAGE;
        }
        if ( !fnCheckPort( nExplicitPortWS4std, "ws-port" ) ) {
            // return EX_USAGE;
        }
        if ( !fnCheckPort( nExplicitPortWS4nfo, "info-ws-port" ) ) {
            // return EX_USAGE;
        }
        if ( !fnCheckPort( nExplicitPortWS6std, "ws-port6" ) ) {
            // return EX_USAGE;
        }
        if ( !fnCheckPort( nExplicitPortWS6nfo, "info-ws-port6" ) ) {
            // return EX_USAGE;
        }
        if ( !fnCheckPort( nExplicitPortWSS4std, "wss-port" ) ) {
            // return EX_USAGE;
        }
        if ( !fnCheckPort( nExplicitPortWSS4nfo, "info-wss-port" ) ) {
            // return EX_USAGE;
        }
        if ( !fnCheckPort( nExplicitPortWSS6std, "wss-port6" ) ) {
            // return EX_USAGE;
        }
        if ( !fnCheckPort( nExplicitPortWSS6nfo, "info-wss-port6" ) ) {
            // return EX_USAGE;
        }
        if ( nExplicitPortHTTP4std > 0 || nExplicitPortHTTPS4std > 0 || nExplicitPortHTTP6std > 0 ||
             nExplicitPortHTTPS6std > 0 || nExplicitPortHTTP4nfo > 0 ||
             nExplicitPortHTTPS4nfo > 0 || nExplicitPortHTTP6nfo > 0 ||
             nExplicitPortHTTPS6nfo > 0 || nExplicitPortWS4std > 0 || nExplicitPortWSS4std > 0 ||
             nExplicitPortWS6std > 0 || nExplicitPortWSS6std > 0 || nExplicitPortWS4nfo > 0 ||
             nExplicitPortWSS4nfo > 0 || nExplicitPortWS6nfo > 0 || nExplicitPortWSS6nfo > 0 ) {
            clog( VerbosityDebug, "main" )
                << cc::debug( "...." ) << cc::attention( "RPC params" ) << cc::debug( ":" );
            //
            auto fnPrintPort = [&]( const int& nPort, const char* strDescription ) -> void {
                static const size_t nAlign = 35;
                size_t nDescLen = strnlen( strDescription, 1024 );
                std::string strDots;
                for ( ; ( strDots.size() + nDescLen ) < nAlign; )
                    strDots += ".";
                clog( VerbosityDebug, "main" )
                    << cc::debug( "...." ) << cc::info( strDescription ) << cc::debug( strDots )
                    << " " << ( ( nPort >= 0 ) ? cc::num10( nPort ) : cc::error( "off" ) );
            };
            fnPrintPort( nExplicitPortHTTP4std, "HTTP/4/std port" );
            fnPrintPort( nExplicitPortHTTP4nfo, "HTTP/4/nfo port" );
            fnPrintPort( nExplicitPortHTTP6std, "HTTP/6/std port" );
            fnPrintPort( nExplicitPortHTTP6nfo, "HTTP/6/nfo port" );
            fnPrintPort( nExplicitPortHTTPS4std, "HTTPS/4/std port" );
            fnPrintPort( nExplicitPortHTTPS4nfo, "HTTPS/4/nfo port" );
            fnPrintPort( nExplicitPortHTTPS6std, "HTTPS/6/std port" );
            fnPrintPort( nExplicitPortHTTPS6nfo, "HTTPS/6/nfo port" );
            fnPrintPort( nExplicitPortWS4std, "WS/4/std port" );
            fnPrintPort( nExplicitPortWS4nfo, "WS/4/nfo port" );
            fnPrintPort( nExplicitPortWS6std, "WS/6/std port" );
            fnPrintPort( nExplicitPortWS6nfo, "WS/6/nfo port" );
            fnPrintPort( nExplicitPortWSS4std, "WSS/4/std port" );
            fnPrintPort( nExplicitPortWSS4nfo, "WSS/4/nfo port" );
            fnPrintPort( nExplicitPortWSS6std, "WSS/6/std port" );
            fnPrintPort( nExplicitPortWSS6nfo, "WSS/6/nfo port" );
            //
            std::string strPathSslKey, strPathSslCert, strPathSslCA;
            bool bHaveSSL = false;
            if ( ( nExplicitPortHTTPS4std > 0 || nExplicitPortHTTPS6std > 0 ||
                     nExplicitPortHTTPS4nfo > 0 || nExplicitPortHTTPS6nfo > 0 ||
                     nExplicitPortWSS4std > 0 || nExplicitPortWSS6std > 0 ||
                     nExplicitPortWSS4nfo > 0 || nExplicitPortWSS6nfo > 0 ) &&
                 vm.count( "ssl-key" ) > 0 && vm.count( "ssl-cert" ) > 0 ) {
                strPathSslKey = vm["ssl-key"].as< std::string >();
                strPathSslCert = vm["ssl-cert"].as< std::string >();
                if ( ( !strPathSslKey.empty() ) && ( !strPathSslCert.empty() ) )
                    bHaveSSL = true;
                if ( vm.count( "ssl-ca" ) > 0 )
                    strPathSslCA = vm["ssl-ca"].as< std::string >();
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
            clog( VerbosityDebug, "main" )
                << cc::debug( "...." ) << cc::info( "Performance timeline tracker" )
                << cc::debug( "............. " )
                << ( pTracker->is_enabled() ? cc::size10( pTracker->get_safe_max_item_count() ) :
                                              cc::error( "off" ) );

            if ( !bHaveSSL )
                nExplicitPortHTTPS4std = nExplicitPortHTTPS6std = nExplicitPortHTTPS4nfo =
                    nExplicitPortHTTPS6nfo = nExplicitPortWSS4std = nExplicitPortWSS6std =
                        nExplicitPortWSS4nfo = nExplicitPortWSS6nfo = -1;
            if ( bHaveSSL ) {
                clog( VerbosityDebug, "main" )
                    << cc::debug( "...." ) << cc::info( "SSL key is" )
                    << cc::debug( "............................... " ) << cc::p( strPathSslKey );
                clog( VerbosityDebug, "main" )
                    << cc::debug( "...." ) + cc::info( "SSL certificate is" )
                    << cc::debug( "....................... " ) << cc::p( strPathSslCert );
                clog( VerbosityDebug, "main" )
                    << cc::debug( "...." ) + cc::info( "SSL CA is" )
                    << cc::debug( "................................ " ) << cc::p( strPathSslCA );
            }
            //
            //
            size_t maxConnections = 0,
                   max_http_handler_queues = __SKUTILS_HTTP_DEFAULT_MAX_PARALLEL_QUEUES_COUNT__,
                   cntServersStd = 1, cntServersNfo = 0, cntInBatch = 128;
            bool is_async_http_transfer_mode = true;
            int32_t pg_threads = 0;
            int32_t pg_threads_limit = 0;

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

            if ( chainConfigParsed ) {
                try {
                    pg_threads = joConfig["skaleConfig"]["nodeInfo"]["pg-threads"].get< int32_t >();
                    if ( pg_threads < 0 )
                        pg_threads = 0;
                } catch ( ... ) {
                    pg_threads = 0;
                }
                try {
                    pg_threads_limit =
                        joConfig["skaleConfig"]["nodeInfo"]["pg-threads-limit"].get< int32_t >();
                    if ( pg_threads_limit < 0 )
                        pg_threads_limit = 0;
                } catch ( ... ) {
                    pg_threads_limit = 0;
                }
                try {
                    bool is_pg_trace =
                        joConfig["skaleConfig"]["nodeInfo"]["pg-trace"].get< bool >();
                    skutils::http_pg::pg_logging_set( is_pg_trace );
                } catch ( ... ) {
                }
            }
            if ( vm.count( "pg-threads" ) )
                pg_threads = vm["pg-threads"].as< int32_t >();
            if ( vm.count( "pg-threads-limit" ) )
                pg_threads_limit = vm["pg-threads-limit"].as< int32_t >();
            if ( vm.count( "pg-trace" ) )
                skutils::http_pg::pg_logging_set( true );

            // First, get "acceptors"/"info-acceptors" true/false from config.json
            // Second, get it from command line parameter (higher priority source)
            if ( chainConfigParsed ) {
                try {
                    cntServersStd =
                        joConfig["skaleConfig"]["nodeInfo"]["acceptors"].get< size_t >();
                } catch ( ... ) {
                    cntServersStd = 1;
                }
                try {
                    cntServersNfo =
                        joConfig["skaleConfig"]["nodeInfo"]["info-acceptors"].get< size_t >();
                } catch ( ... ) {
                    cntServersNfo = 0;
                }
            }
            if ( vm.count( "acceptors" ) )
                cntServersStd = vm["acceptors"].as< size_t >();
            if ( cntServersStd < 1 )
                cntServersStd = 1;
            if ( vm.count( "info-acceptors" ) )
                cntServersNfo = vm["info-acceptors"].as< size_t >();

            // First, get "acceptors"/"info-acceptors" true/false from config.json
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

            clog( VerbosityDebug, "main" )
                << cc::debug( "...." ) + cc::info( "WS mode" )
                << cc::debug( ".................................. " )
                << skutils::ws::nlws::srvmode2str( skutils::ws::nlws::g_default_srvmode );
            clog( VerbosityDebug, "main" )
                << cc::debug( "...." ) + cc::info( "WS logging" )
                << cc::debug( "............................... " )
                << cc::info( skutils::ws::wsll2str( skutils::ws::g_eWSLL ) );
            clog( VerbosityDebug, "main" )
                << cc::debug( "...." ) + cc::info( "Max RPC connections" )
                << cc::debug( "...................... " )
                << ( ( maxConnections > 0 ) ? cc::size10( maxConnections ) :
                                              cc::error( "disabled" ) );
            clog( VerbosityDebug, "main" )
                << cc::debug( "...." ) + cc::info( "Max HTTP queues" )
                << cc::debug( ".......................... " )
                << ( ( max_http_handler_queues > 0 ) ? cc::size10( max_http_handler_queues ) :
                                                       cc::notice( "default" ) );
            clog( VerbosityDebug, "main" ) << cc::debug( "...." ) + cc::info( "Asynchronous HTTP" )
                                           << cc::debug( "........................ " )
                                           << cc::yn( is_async_http_transfer_mode );
            clog( VerbosityDebug, "main" )
                << cc::debug( "...." ) + cc::info( "Proxygen threads" )
                << cc::debug( "......................... " ) << cc::num10( pg_threads );
            clog( VerbosityDebug, "main" )
                << cc::debug( "...." ) + cc::info( "Proxygen threads limit" )
                << cc::debug( "................... " ) << cc::num10( pg_threads_limit );

            //
            clog( VerbosityDebug, "main" )
                << cc::debug( "...." ) + cc::info( "Max count in batch JSON RPC request" )
                << cc::debug( "...... " ) << cc::size10( cntInBatch );
            clog( VerbosityDebug, "main" )
                << cc::debug( "...." ) + cc::info( "Parallel RPC connection acceptors" )
                << cc::debug( "........ " ) << cc::size10( cntServersStd );
            clog( VerbosityDebug, "main" )
                << cc::debug( "...." ) + cc::info( "Parallel informational RPC acceptors" )
                << cc::debug( "..... " ) << cc::size10( cntServersNfo );
            SkaleServerOverride::fn_binary_snapshot_download_t fn_binary_snapshot_download =
                [=]( const nlohmann::json& joRequest ) -> std::vector< uint8_t > {
                return pSkaleFace->impl_skale_downloadSnapshotFragmentBinary( joRequest );
            };

            //
            SkaleServerOverride::opts_t serverOpts;
            inject_rapidjson_handlers( serverOpts, pEthFace );
            serverOpts.fn_binary_snapshot_download_ = fn_binary_snapshot_download;
            serverOpts.netOpts_.bindOptsStandard_.cntServers_ = cntServersStd;
            serverOpts.netOpts_.bindOptsStandard_.strAddrHTTP4_ = chainParams.nodeInfo.ip;
            serverOpts.netOpts_.bindOptsStandard_.nBasePortHTTP4_ = nExplicitPortHTTP4std;
            serverOpts.netOpts_.bindOptsStandard_.strAddrHTTP6_ = chainParams.nodeInfo.ip6;
            serverOpts.netOpts_.bindOptsStandard_.nBasePortHTTP6_ = nExplicitPortHTTP6std;
            serverOpts.netOpts_.bindOptsStandard_.strAddrHTTPS4_ = chainParams.nodeInfo.ip;
            serverOpts.netOpts_.bindOptsStandard_.nBasePortHTTPS4_ = nExplicitPortHTTPS4std;
            serverOpts.netOpts_.bindOptsStandard_.strAddrHTTPS6_ = chainParams.nodeInfo.ip6;
            serverOpts.netOpts_.bindOptsStandard_.nBasePortHTTPS6_ = nExplicitPortHTTPS6std;
            serverOpts.netOpts_.bindOptsStandard_.strAddrWS4_ = chainParams.nodeInfo.ip;
            serverOpts.netOpts_.bindOptsStandard_.nBasePortWS4_ = nExplicitPortWS4std;
            serverOpts.netOpts_.bindOptsStandard_.strAddrWS6_ = chainParams.nodeInfo.ip6;
            serverOpts.netOpts_.bindOptsStandard_.nBasePortWS6_ = nExplicitPortWS6std;
            serverOpts.netOpts_.bindOptsStandard_.strAddrWSS4_ = chainParams.nodeInfo.ip;
            serverOpts.netOpts_.bindOptsStandard_.nBasePortWSS4_ = nExplicitPortWSS4std;
            serverOpts.netOpts_.bindOptsStandard_.strAddrWSS6_ = chainParams.nodeInfo.ip6;
            serverOpts.netOpts_.bindOptsStandard_.nBasePortWSS6_ = nExplicitPortWSS6std;

            serverOpts.netOpts_.bindOptsInformational_.cntServers_ = cntServersNfo;
            serverOpts.netOpts_.bindOptsInformational_.strAddrHTTP4_ = chainParams.nodeInfo.ip;
            serverOpts.netOpts_.bindOptsInformational_.nBasePortHTTP4_ = nExplicitPortHTTP4nfo;
            serverOpts.netOpts_.bindOptsInformational_.strAddrHTTP6_ = chainParams.nodeInfo.ip6;
            serverOpts.netOpts_.bindOptsInformational_.nBasePortHTTP6_ = nExplicitPortHTTP6nfo;
            serverOpts.netOpts_.bindOptsInformational_.strAddrHTTPS4_ = chainParams.nodeInfo.ip;
            serverOpts.netOpts_.bindOptsInformational_.nBasePortHTTPS4_ = nExplicitPortHTTPS4nfo;
            serverOpts.netOpts_.bindOptsInformational_.strAddrHTTPS6_ = chainParams.nodeInfo.ip6;
            serverOpts.netOpts_.bindOptsInformational_.nBasePortHTTPS6_ = nExplicitPortHTTPS6nfo;

            serverOpts.netOpts_.bindOptsInformational_.strAddrWS4_ = chainParams.nodeInfo.ip;
            serverOpts.netOpts_.bindOptsInformational_.nBasePortWS4_ = nExplicitPortWS4nfo;
            serverOpts.netOpts_.bindOptsInformational_.strAddrWS6_ = chainParams.nodeInfo.ip6;
            serverOpts.netOpts_.bindOptsInformational_.nBasePortWS6_ = nExplicitPortWS6nfo;
            serverOpts.netOpts_.bindOptsInformational_.strAddrWSS4_ = chainParams.nodeInfo.ip;
            serverOpts.netOpts_.bindOptsInformational_.nBasePortWSS4_ = nExplicitPortWSS4nfo;
            serverOpts.netOpts_.bindOptsInformational_.strAddrWSS6_ = chainParams.nodeInfo.ip6;
            serverOpts.netOpts_.bindOptsInformational_.nBasePortWSS6_ = nExplicitPortWSS6nfo;

            serverOpts.netOpts_.strPathSslKey_ = strPathSslKey;
            serverOpts.netOpts_.strPathSslCert_ = strPathSslCert;
            serverOpts.netOpts_.strPathSslCA_ = strPathSslCA;
            serverOpts.lfExecutionDurationMaxForPerformanceWarning_ =
                lfExecutionDurationMaxForPerformanceWarning;
            try {
                static const char* g_arrVarNamesToTryEthERC20[] = {
                    "EthERC20",
                    "ethERC20Address",
                };
                for ( size_t idxVar = 0; idxVar < sizeof( g_arrVarNamesToTryEthERC20 ) /
                                                      sizeof( g_arrVarNamesToTryEthERC20[0] );
                      ++idxVar ) {
                    const char* strVarName = g_arrVarNamesToTryEthERC20[idxVar];
                    serverOpts.strEthErc20Address_ =
                        joConfig["skaleConfig"]["contractSettings"]["IMA"][strVarName]
                            .get< std::string >();
                    serverOpts.strEthErc20Address_ =
                        skutils::tools::trim_copy( serverOpts.strEthErc20Address_ );
                    if ( !serverOpts.strEthErc20Address_.empty() )
                        break;
                }
                if ( serverOpts.strEthErc20Address_.empty() )
                    throw std::runtime_error( "\"ethERC20Address\" was not found in config JSON" );
                clog( VerbosityDebug, "main" ) << ( cc::debug( "\"ethERC20Address\" is" ) + " " +
                                                    cc::info( serverOpts.strEthErc20Address_ ) );
            } catch ( ... ) {
                serverOpts.strEthErc20Address_ = "0xd3cdbc1b727b2ed91b8ad21333841d2e96f255af";
                clog( VerbosityError, "main" )
                    << ( cc::error( "WARNING:" ) + " " +
                           cc::warn(
                               "\"ethERC20Address\" was not found in config JSON, assuming" ) +
                           " " + cc::info( serverOpts.strEthErc20Address_ ) );
            }
            auto skale_server_connector =
                new SkaleServerOverride( chainParams, g_client.get(), serverOpts );
            //
            // unddos
            if ( joConfig.count( "unddos" ) > 0 ) {
                nlohmann::json joUnDdosSettings = joConfig["unddos"];
                skale_server_connector->unddos_.load_settings_from_json( joUnDdosSettings );
            } else
                skale_server_connector->unddos_.get_settings();  // auto-init
            //
            clog( VerbosityDebug, "main" )
                << cc::attention( "UN-DDOS" ) + cc::debug( " is using configuration" )
                << cc::j( skale_server_connector->unddos_.get_settings_json() );
            skale_server_connector->max_http_handler_queues_ = max_http_handler_queues;
            skale_server_connector->is_async_http_transfer_mode_ = is_async_http_transfer_mode;
            skale_server_connector->maxCountInBatchJsonRpcRequest_ = cntInBatch;
            skale_server_connector->pg_threads_ = pg_threads;
            skale_server_connector->pg_threads_limit_ = pg_threads_limit;
            //
            pSkaleStatsFace->setProvider( skale_server_connector );
            skale_server_connector->setConsumer( pSkaleStatsFace );
            //
            skale_server_connector->opts_.isTraceCalls_ = bTraceJsonRpcCalls;
            skale_server_connector->opts_.isTraceSpecialCalls_ = bTraceJsonRpcSpecialCalls;

            skale_server_connector->max_connection_set( maxConnections );
            g_jsonrpcIpcServer->addConnector( skale_server_connector );
            if ( !skale_server_connector->StartListening() ) {  // TODO Will it delete itself?
                clog( VerbosityError, "main" )
                    << ( cc::fatal( "FATAL:" ) + " " +
                           cc::error( "Failed to start JSON RPC, will exit..." ) );
                return EX_IOERR;
            }
            int nStatHTTP4std = skale_server_connector->getServerPortStatusProxygenHTTP(
                4, e_server_mode_t::esm_standard );
            int nStatHTTP4nfo = skale_server_connector->getServerPortStatusProxygenHTTP(
                4, e_server_mode_t::esm_informational );
            int nStatHTTP6std = skale_server_connector->getServerPortStatusProxygenHTTP(
                6, e_server_mode_t::esm_standard );
            int nStatHTTP6nfo = skale_server_connector->getServerPortStatusProxygenHTTP(
                6, e_server_mode_t::esm_informational );
            int nStatHTTPS4std = skale_server_connector->getServerPortStatusProxygenHTTPS(
                4, e_server_mode_t::esm_standard );
            int nStatHTTPS4nfo = skale_server_connector->getServerPortStatusProxygenHTTPS(
                4, e_server_mode_t::esm_informational );
            int nStatHTTPS6std = skale_server_connector->getServerPortStatusProxygenHTTPS(
                6, e_server_mode_t::esm_standard );
            int nStatHTTPS6nfo = skale_server_connector->getServerPortStatusProxygenHTTPS(
                6, e_server_mode_t::esm_informational );
            int nStatWS4std =
                skale_server_connector->getServerPortStatusWS( 4, e_server_mode_t::esm_standard );
            int nStatWS4nfo = skale_server_connector->getServerPortStatusWS(
                4, e_server_mode_t::esm_informational );
            int nStatWS6std =
                skale_server_connector->getServerPortStatusWS( 6, e_server_mode_t::esm_standard );
            int nStatWS6nfo = skale_server_connector->getServerPortStatusWS(
                6, e_server_mode_t::esm_informational );
            int nStatWSS4std =
                skale_server_connector->getServerPortStatusWSS( 4, e_server_mode_t::esm_standard );
            int nStatWSS4nfo = skale_server_connector->getServerPortStatusWSS(
                4, e_server_mode_t::esm_informational );
            int nStatWSS6std =
                skale_server_connector->getServerPortStatusWSS( 6, e_server_mode_t::esm_standard );
            int nStatWSS6nfo = skale_server_connector->getServerPortStatusWSS(
                6, e_server_mode_t::esm_informational );
            static const size_t g_cntWaitAttempts = 30;
            static const std::chrono::milliseconds g_waitAttempt = std::chrono::milliseconds( 100 );
            if ( nExplicitPortHTTP4std > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatHTTP4std < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !ExitHandler::shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityDebug, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "HTTP/4/std" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatHTTP4std = skale_server_connector->getServerPortStatusProxygenHTTP(
                        4, e_server_mode_t::esm_standard );
                }
            }
            if ( nExplicitPortHTTP4nfo > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatHTTP4nfo < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !ExitHandler::shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityDebug, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "HTTP/4/nfo" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatHTTP4nfo = skale_server_connector->getServerPortStatusProxygenHTTP(
                        4, e_server_mode_t::esm_informational );
                }
            }
            if ( nExplicitPortHTTP6std > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatHTTP6std < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !ExitHandler::shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityDebug, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "HTTP/6/std" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatHTTP6std = skale_server_connector->getServerPortStatusProxygenHTTP(
                        6, e_server_mode_t::esm_standard );
                }
            }
            if ( nExplicitPortHTTP6nfo > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatHTTP6nfo < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !ExitHandler::shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityDebug, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "HTTP/6/nfo" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatHTTP6nfo = skale_server_connector->getServerPortStatusProxygenHTTP(
                        6, e_server_mode_t::esm_informational );
                }
            }
            if ( nExplicitPortHTTPS4std > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatHTTPS4std < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !ExitHandler::shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityDebug, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "HTTPS/4/std" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatHTTPS4std = skale_server_connector->getServerPortStatusProxygenHTTPS(
                        4, e_server_mode_t::esm_standard );
                }
            }
            if ( nExplicitPortHTTPS4nfo > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatHTTPS4nfo < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !ExitHandler::shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityDebug, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "HTTPS/4/nfo" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatHTTPS4nfo = skale_server_connector->getServerPortStatusProxygenHTTPS(
                        4, e_server_mode_t::esm_informational );
                }
            }
            if ( nExplicitPortHTTPS6std > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatHTTPS6std < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !ExitHandler::shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityDebug, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "HTTPS/6/std" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatHTTPS6std = skale_server_connector->getServerPortStatusProxygenHTTPS(
                        6, e_server_mode_t::esm_standard );
                }
            }
            if ( nExplicitPortHTTPS6nfo > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatHTTPS6nfo < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !ExitHandler::shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityDebug, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "HTTPS/6/nfo" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatHTTPS6nfo = skale_server_connector->getServerPortStatusProxygenHTTPS(
                        6, e_server_mode_t::esm_informational );
                }
            }
            if ( nExplicitPortWS4std > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatWS4std < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !ExitHandler::shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityDebug, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "WS/4/std" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatWS4std = skale_server_connector->getServerPortStatusWS(
                        4, e_server_mode_t::esm_standard );
                }
            }
            if ( nExplicitPortWS4nfo > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatWS4nfo < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !ExitHandler::shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityDebug, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "WS/4/nfo" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatWS4nfo = skale_server_connector->getServerPortStatusWS(
                        4, e_server_mode_t::esm_informational );
                }
            }
            if ( nExplicitPortWS6std > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatWS6std < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !ExitHandler::shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityDebug, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "WS/6/std" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatWS6std = skale_server_connector->getServerPortStatusWS(
                        6, e_server_mode_t::esm_standard );
                }
            }
            if ( nExplicitPortWS6nfo > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatWS6nfo < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !ExitHandler::shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityDebug, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "WS/6/nfo" )
                            << cc::debug( " start... " );
                    std::this_thread::sleep_for( g_waitAttempt );
                    nStatWS6nfo = skale_server_connector->getServerPortStatusWS(
                        6, e_server_mode_t::esm_informational );
                }
            }
            if ( nExplicitPortWSS4std > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatWSS4std < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !ExitHandler::shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityDebug, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "WSS/4/std" )
                            << cc::debug( " start... " );
                    nStatWSS4std = skale_server_connector->getServerPortStatusWSS(
                        4, e_server_mode_t::esm_standard );
                }
            }
            if ( nExplicitPortWSS4nfo > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatWSS4nfo < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !ExitHandler::shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityDebug, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "WSS/4/nfo" )
                            << cc::debug( " start... " );
                    nStatWSS4nfo = skale_server_connector->getServerPortStatusWSS(
                        4, e_server_mode_t::esm_informational );
                }
            }
            if ( nExplicitPortWSS6std > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatWSS6std < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !ExitHandler::shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityDebug, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "WSS/6/std" )
                            << cc::debug( " start... " );
                    nStatWSS6std = skale_server_connector->getServerPortStatusWSS(
                        6, e_server_mode_t::esm_standard );
                }
            }
            if ( nExplicitPortWSS6nfo > 0 ) {
                for ( size_t idxWaitAttempt = 0;
                      nStatWSS6nfo < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                      ( !ExitHandler::shouldExit() );
                      ++idxWaitAttempt ) {
                    if ( idxWaitAttempt == 0 )
                        clog( VerbosityDebug, "main" )
                            << cc::debug( "Waiting for " ) + cc::info( "WSS/6/nfo" )
                            << cc::debug( " start... " );
                    nStatWSS6nfo = skale_server_connector->getServerPortStatusWSS(
                        6, e_server_mode_t::esm_informational );
                }
            }
            clog( VerbosityDebug, "main" )
                << cc::debug( "...." ) << cc::attention( "RPC status" ) << cc::debug( ":" );
            auto fnPrintStatus = []( const int& nPort, const int& nStat,
                                     const char* strDescription ) -> void {
                static const size_t nAlign = 35;
                size_t nDescLen = strnlen( strDescription, 1024 );
                std::string strDots;
                for ( ; ( strDots.size() + nDescLen ) < nAlign; )
                    strDots += ".";
                clog( VerbosityDebug, "main" )
                    << cc::debug( "...." ) << cc::info( strDescription ) << cc::debug( strDots )
                    << ( ( nStat >= 0 ) ? ( ( nPort > 0 ) ? cc::num10( nStat ) :
                                                            cc::warn( "still starting..." ) ) :
                                          cc::error( "off" ) );
            };
            fnPrintStatus( nExplicitPortHTTP4std, nStatHTTP4std, "HTTP/4std" );
            fnPrintStatus( nExplicitPortHTTP4nfo, nStatHTTP4nfo, "HTTP/4nfo" );
            fnPrintStatus( nExplicitPortHTTP6std, nStatHTTP6std, "HTTP/6std" );
            fnPrintStatus( nExplicitPortHTTP6nfo, nStatHTTP6nfo, "HTTP/6nfo" );
            fnPrintStatus( nExplicitPortHTTPS4std, nStatHTTPS4std, "HTTPS/4std" );
            fnPrintStatus( nExplicitPortHTTPS4nfo, nStatHTTPS4nfo, "HTTPS/4nfo" );
            fnPrintStatus( nExplicitPortHTTPS6std, nStatHTTPS6std, "HTTPS/6std" );
            fnPrintStatus( nExplicitPortHTTPS6nfo, nStatHTTPS6nfo, "HTTPS/6nfo" );
            fnPrintStatus( nExplicitPortWS4std, nStatWS4std, "WS/4std" );
            fnPrintStatus( nExplicitPortWS4nfo, nStatWS4nfo, "WS/4nfo" );
            fnPrintStatus( nExplicitPortWS6std, nStatWS6std, "WS/6std" );
            fnPrintStatus( nExplicitPortWS6nfo, nStatWS6nfo, "WS/6nfo" );
            fnPrintStatus( nExplicitPortWSS4std, nStatWS4std, "WSS/4std" );
            fnPrintStatus( nExplicitPortWSS4nfo, nStatWS4nfo, "WSS/4nfo" );
            fnPrintStatus( nExplicitPortWSS6std, nStatWS6std, "WSS/6std" );
            fnPrintStatus( nExplicitPortWSS6nfo, nStatWS6nfo, "WSS/6nfo" );
        }  // if ( nExplicitPort ......

        statusAndControl->setSubsystemRunning( StatusAndControl::Rpc, true );

        if ( strJsonAdminSessionKey.empty() )
            strJsonAdminSessionKey =
                sessionManager->newSession( rpc::SessionPermissions{ { rpc::Privilege::Admin } } );
        else
            sessionManager->addSession(
                strJsonAdminSessionKey, rpc::SessionPermissions{ { rpc::Privilege::Admin } } );

        clog( VerbosityInfo, "main" )
            << cc::bright( "JSONRPC Admin Session Key: " ) << cc::sunny( strJsonAdminSessionKey );
    }  // if ( is_ipc || nExplicitPort...

    if ( bEnabledShutdownViaWeb3 ) {
        clog( VerbosityWarning, "main" )
            << cc::warn( "Enabling programmatic shutdown via Web3..." );
        dev::rpc::Skale::enableWeb3Shutdown( true );
        dev::rpc::Skale::onShutdownInvoke(
            []() { ExitHandler::exitHandler( -1, ExitHandler::ec_web3_request ); } );
        clog( VerbosityWarning, "main" )
            << cc::warn( "Done, programmatic shutdown via Web3 is enabled" );
    } else {
        clog( VerbosityDebug, "main" )
            << cc::debug( "Disabling programmatic shutdown via Web3..." );
        dev::rpc::Skale::enableWeb3Shutdown( false );
        clog( VerbosityDebug, "main" )
            << cc::debug( "Done, programmatic shutdown via Web3 is disabled" );
    }

    if ( g_client ) {
        unsigned int n = g_client->blockChain().details().number;
        unsigned int mining = 0;
        while ( !ExitHandler::shouldExit() )
            stopSealingAfterXBlocks( g_client.get(), n, mining );
    } else {
        while ( !ExitHandler::shouldExit() )
            this_thread::sleep_for( chrono::milliseconds( 1000 ) );
    }

    if ( statusAndControl ) {
        statusAndControl->setExitState( StatusAndControl::StartAgain,
            ( ExitHandler::requestedExitCode() != ExitHandler::ec_success ) );
        statusAndControl->setExitState( StatusAndControl::StartFromSnapshot,
            ( ExitHandler::requestedExitCode() == ExitHandler::ec_state_root_mismatch ) );
        statusAndControl->setExitState( StatusAndControl::ClearDataDir,
            ( ExitHandler::requestedExitCode() == ExitHandler::ec_state_root_mismatch ) );
    }  // if

    if ( g_jsonrpcIpcServer.get() ) {
        g_jsonrpcIpcServer->StopListening();
        g_jsonrpcIpcServer.reset( nullptr );
        statusAndControl->setSubsystemRunning( StatusAndControl::Rpc, false );
    }
    if ( g_client ) {
        g_client->stopWorking();
        statusAndControl->setSubsystemRunning( StatusAndControl::Blockchain, false );
        g_client.reset( nullptr );
    }

    std::cerr << localeconv()->decimal_point << std::endl;

    std::string basename = "profile" + chainParams.nodeInfo.id.str();
    MicroProfileDumpFileImmediately(
        ( basename + ".html" ).c_str(), ( basename + ".csv" ).c_str(), nullptr );
    MicroProfileShutdown();

    //    clog( VerbosityDebug, "main" ) << cc::debug( "Stopping task dispatcher..." );
    //    skutils::dispatch::shutdown();
    //    clog( VerbosityDebug, "main" ) << cc::debug( "Done, task dispatcher stopped" );
    ExitHandler::exit_code_t ec = ExitHandler::requestedExitCode();
    if ( ec != ExitHandler::ec_success ) {
        clog( VerbosityError, "main" ) << cc::error( "Exiting main with code " )
                                       << cc::num10( int( ec ) ) << cc::error( "...\n" );
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
