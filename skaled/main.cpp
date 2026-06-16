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
#include <filesystem>

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
#ifdef HISTORIC_STATE
#include <libweb3jsonrpc/Tracing.h>
#else
#include <libweb3jsonrpc/TracingFace.h>
#endif
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
    static Logger loggerInfo{ createLogger( VerbosityInfo, "main - version" ) };
    const auto* buildinfo = skale_get_buildinfo();
    std::string pv = buildinfo->project_version, ver, commit;
    auto pos = pv.find( "+" );
    if ( pos != std::string::npos ) {
        commit = pv.c_str() + pos;
        boost::replace_all( commit, "+commit.", "" );
        ver = pv.substr( 0, pos );
    } else
        ver = pv;
    BOOST_LOG( loggerInfo ) << "Skaled ............................" << ver << "\n";
    if ( !commit.empty() )
        BOOST_LOG( loggerInfo ) << "Commit ............................." << commit << "\n";
    BOOST_LOG( loggerInfo ) << "Skale network protocol version ...." << dev::eth::c_protocolVersion
                            << "." << c_minorProtocolVersion << "\n";
    BOOST_LOG( loggerInfo ) << "Client database version ..........." << dev::eth::c_databaseVersion
                            << "\n";
    BOOST_LOG( loggerInfo ) << "Build ............................." << buildinfo->system_name
                            << "/" << buildinfo->build_type << "\n";
    BOOST_LOG( loggerInfo ).flush();
}

static std::string clientVersion() {
    const auto* buildinfo = skale_get_buildinfo();
    return std::string( "skaled/" ) + buildinfo->project_version + "/" + buildinfo->system_name +
           "/" + buildinfo->compiler_id + buildinfo->compiler_version + "/" + buildinfo->build_type;
}

static std::string clientVersionColorized() {
    const auto* buildinfo = skale_get_buildinfo();
    return std::string( "skaled/" ) + buildinfo->project_version + "/" + buildinfo->system_name +
           "/" + buildinfo->compiler_id + buildinfo->compiler_version + "/" + buildinfo->build_type;
}

static std::string flag_ed( bool flagBool_ ) {
    return flagBool_ ? "enabled" : "disabled";
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

#ifdef FAIR
    // HACK: this should be called from every active thread
    // that has ever entered consensus
    // in reality this is the only place this function is executed
    if ( _c->skaleHost()->isConsesusUpdateHappened() )
        _c->skaleHost()->handleConsensusUpdate();
#endif

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

#ifdef FAIR
uint64_t fetchLatestBlockTimestamp( const std::string& url ) {
    static Logger loggerInfo{ createLogger( VerbosityInfo, "fetchLatestBlockTimestamp" ) };

    skutils::rest::client cli( skutils::rest::g_nClientConnectionTimeoutMS );
    if ( !cli.open( url ) ) {
        throw std::runtime_error( "REST failed to connect to server: " + url );
    }

    // Create JSON-RPC request
    nlohmann::json request = nlohmann::json::object();
    request["jsonrpc"] = "2.0";
    request["method"] = "eth_getBlockByNumber";
    request["params"] = nlohmann::json::array( { "latest", false } );

    BOOST_LOG( loggerInfo ) << "Sending eth_getBlockByNumber request to " << url;
    skutils::rest::data_t response = cli.call( request );

    // Response validation
    if ( !response.err_s_.empty() ) {
        throw std::runtime_error( "RPC call error for " + url + ": " + response.err_s_ );
    }

    if ( response.empty() ) {
        throw std::runtime_error( "Empty response from " + url );
    }

    nlohmann::json responseData;
    try {
        responseData = nlohmann::json::parse( response.s_ );
    } catch ( const nlohmann::json::parse_error& ex ) {
        throw std::runtime_error( "JSON parse error from " + url + ": " + ex.what() );
    }

    // Check for JSON-RPC error
    if ( responseData.contains( "error" ) ) {
        auto error = responseData["error"];
        std::string error_msg = "JSON-RPC error from " + url + ": ";
        if ( error.contains( "message" ) ) {
            error_msg += error["message"].get< std::string >();
        }
        if ( error.contains( "code" ) ) {
            error_msg += " (code: " + std::to_string( error["code"].get< int >() ) + ")";
        }
        throw std::runtime_error( error_msg );
    }

    // Validate response data
    if ( !responseData.contains( "result" ) ) {
        throw std::runtime_error( "Missing 'result' field in response from " + url );
    }

    auto result = responseData["result"];
    if ( result.is_null() ) {
        throw std::runtime_error( "Block result is null from " + url );
    }

    if ( !result.contains( "timestamp" ) ) {
        throw std::runtime_error( "Missing 'timestamp' field in block data from " + url );
    }

    std::string timestampStringRep = result["timestamp"].get< std::string >();

    // Converting timestamp to uint64_t
    uint64_t timestamp;
    try {
        timestamp = jsToInt( timestampStringRep );
    } catch ( const std::exception& ex ) {
        throw std::runtime_error( "Failed to parse timestamp '" + timestampStringRep + "' from " +
                                  url + ": " + ex.what() );
    }

    BOOST_LOG( loggerInfo ) << "Successfully fetched timestamp " << timestamp << " from " << url;
    return timestamp;
}
#endif

void downloadSnapshot( unsigned block_number, std::shared_ptr< SnapshotManager >& snapshotManager,
    const std::string& strURLWeb3, const ChainParams& chainParams ) {
    static Logger loggerInfo{ createLogger( VerbosityInfo, "downloadSnapshot" ) };
    static Logger loggerError{ createLogger( VerbosityError, "downloadSnapshot" ) };

    fs::path saveTo;
    try {
        BOOST_LOG( loggerInfo ) << "Will download snapshot from " << strURLWeb3;

        try {
            bool isBinaryDownload = true;
            std::string strErrorDescription;
            saveTo = snapshotManager->getDiffPath( block_number );
            bool bOK = dev::rpc::snapshot::download(
                strURLWeb3, block_number, saveTo,
                [&]( size_t idxChunck, size_t cntChunks ) -> bool {
                    BOOST_LOG( loggerInfo )
                        << "... download progress ... " << idxChunck << " of " << cntChunks << "\r";
                    return true;  // continue download
                },
                isBinaryDownload, &strErrorDescription );
            BOOST_LOG( loggerInfo )
                << "                                                  \r";  // clear
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
        BOOST_LOG( loggerInfo ) << "Snapshot download success for block "
                                << to_string( block_number );
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
                BOOST_LOG( loggerError ) << "Snapshot downloaded without " + prefix + " db";
                return;
            }

            fs::rename( db_path,
                db_path.parent_path() / ( prefix + chainParams.getSelfNodeId().str() + ".db" ) );
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
    if ( chainParams.getNodeGroups().size() > 1 ) {
        if ( ( uint64_t ) time( NULL ) >=
             chainParams.getNodeGroupByIndex( chainParams.getNodeGroups().size() - 2 ).finishTs ) {
            isRotationtrigger = false;
        }
    } else {
        isRotationtrigger = false;
    }
    if ( isRotationtrigger ) {
        arrayCommonPublicKey =
            chainParams.getNodeGroupByIndex( chainParams.getNodeGroups().size() - 2 ).blsPublicKey;
    } else {
        arrayCommonPublicKey =
            chainParams.getNodeGroupByIndex( chainParams.getNodeGroups().size() - 1 ).blsPublicKey;
    }

    return arrayCommonPublicKey;
}

unsigned getBlockToDownladSnapshot( const std::string& nodeUrl ) {
    static Logger loggerInfo{ createLogger( VerbosityInfo, "getBlockToDownladSnapshot" ) };

    BOOST_LOG( loggerInfo ) << "Asking node " << ' ' << nodeUrl
                            << " for latest snapshot block number.";

    unsigned blockNumber = getLatestSnapshotBlockNumber( nodeUrl );
    BOOST_LOG( loggerInfo ) << std::string( "Latest Snapshot Block Number is: " ) << blockNumber
                            << " (from " << nodeUrl << ")";

    return blockNumber;
}

std::pair< std::vector< std::string >, std::pair< dev::h256, libBLS::algebra::G1Point > >
voteForSnapshotHash(
    std::unique_ptr< SnapshotHashAgent >& snapshotHashAgent, unsigned blockNumber ) {
    static Logger loggerInfo{ createLogger( VerbosityInfo, "voteForSnapshotHash" ) };

    std::pair< dev::h256, libBLS::algebra::G1Point > votedHash;
    std::vector< std::string > listUrlsToDownload;
    try {
        listUrlsToDownload = snapshotHashAgent->getNodesToDownloadSnapshotFrom( blockNumber );
        BOOST_LOG( loggerInfo ) << "Got urls to download snapshot from "
                                << listUrlsToDownload.size() << " nodes ";

        if ( listUrlsToDownload.size() == 0 )
            return { listUrlsToDownload, votedHash };

        votedHash = snapshotHashAgent->getVotedHash();

        return { listUrlsToDownload, votedHash };
    } catch ( std::exception& ex ) {
        std::throw_with_nested(
            std::runtime_error( "Exception while collecting snapshot hash from other skaleds " ) );
    }
}

bool checkLocalSnapshot( std::shared_ptr< SnapshotManager >& snapshotManager, unsigned blockNumber,
    const dev::h256& votedHash ) {
    static Logger loggerInfo{ createLogger( VerbosityInfo, "checkLocalSnapshot" ) };
    static Logger loggerWarning{ createLogger( VerbosityWarning, "checkLocalSnapshot" ) };

    try {
        if ( snapshotManager->checkSnapshotFolderAndSnapshotHash( blockNumber ) ) {
            BOOST_LOG( loggerInfo )
                << "Snapshot for block " << blockNumber << " already present locally";

            dev::h256 calculated_hash = snapshotManager->getSnapshotHash( blockNumber );

            if ( calculated_hash == votedHash ) {
                BOOST_LOG( loggerInfo ) << std::string( "Will delete all snapshots except " )
                                        << std::to_string( blockNumber );
                snapshotManager->cleanupButKeepSnapshot( blockNumber );
                snapshotManager->restoreSnapshot( blockNumber );
                BOOST_LOG( loggerInfo )
                    << "Snapshot restore success for block " << std::to_string( blockNumber );
                return true;
            } else {
                BOOST_LOG( loggerWarning )
                    << "Snapshot is present locally but its hash is different";
            }
        }  // if present
    } catch ( const std::exception& ex ) {
        // usually snapshot absent exception
        BOOST_LOG( loggerInfo ) << dev::nested_exception_what( ex );
    }

    return false;
}

bool tryDownloadSnapshot( std::shared_ptr< SnapshotManager >& snapshotManager,
    const ChainParams& chainParams, const std::vector< std::string >& listUrlsToDownload,
    const std::pair< dev::h256, libBLS::algebra::G1Point >& votedHash, unsigned blockNumber,
    bool isRegularSnapshot ) {
    static Logger loggerInfo{ createLogger( VerbosityInfo, "tryDownloadSnapshot" ) };
    static Logger loggerWarning{ createLogger( VerbosityWarning, "tryDownloadSnapshot" ) };

    BOOST_LOG( loggerInfo ) << "Will cleanup data dir and snapshots dir if needed";
    if ( isRegularSnapshot )
        snapshotManager->cleanup();

    bool successfulDownload = false;

    size_t n_found = listUrlsToDownload.size();

    size_t shift = rand() % n_found;

    for ( size_t cnt = 0; cnt < n_found && !successfulDownload; ++cnt )
        try {
            size_t i = ( shift + cnt ) % n_found;

            std::string urlToDownloadSnapshot;
            urlToDownloadSnapshot = listUrlsToDownload[i];

            downloadSnapshot( blockNumber, snapshotManager, urlToDownloadSnapshot, chainParams );

            try {
                snapshotManager->computeSnapshotHash( blockNumber
#ifndef FAIR
                    ,
                    true
#endif
                );
            } catch ( const std::exception& ) {
                std::throw_with_nested( std::runtime_error(
                    std::string( "FATAL:" ) +
                    std::string( " Exception while computing snapshot hash " ) ) );
            }

            dev::h256 calculated_hash = snapshotManager->getSnapshotHash( blockNumber );

            if ( calculated_hash == votedHash.first ) {
                successfulDownload = true;
                if ( isRegularSnapshot ) {
                    snapshotManager->restoreSnapshot( blockNumber );
                    BOOST_LOG( loggerInfo )
                        << "Snapshot restore success for block " << to_string( blockNumber );
                }
                return successfulDownload;
            } else {
                BOOST_LOG( loggerWarning )
                    << "tryDownloadSnapshot"
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
            BOOST_LOG( loggerWarning ) << dev::nested_exception_what( ex );
        }  // for download url
    return false;
}

bool downloadSnapshotFromUrl( std::shared_ptr< SnapshotManager >& snapshotManager,
    const ChainParams& chainParams, const std::array< std::string, 4 >& arrayCommonPublicKey,
    const std::string& urlToDownloadSnapshotFrom, bool isRegularSnapshot,
    bool forceDownload = false ) {
    static Logger loggerWarning{ createLogger( VerbosityWarning, "downloadSnapshotFromUrl" ) };
    static Logger loggerInfo{ createLogger( VerbosityInfo, "downloadSnapshotFromUrl" ) };

    unsigned blockNumber = 0;
    if ( isRegularSnapshot )
        blockNumber = getBlockToDownladSnapshot( urlToDownloadSnapshotFrom );

    std::unique_ptr< SnapshotHashAgent > snapshotHashAgent;
    if ( forceDownload )
        snapshotHashAgent.reset(
            new SnapshotHashAgent( chainParams, arrayCommonPublicKey, urlToDownloadSnapshotFrom ) );
    else
        snapshotHashAgent.reset( new SnapshotHashAgent( chainParams, arrayCommonPublicKey ) );

    std::pair< dev::h256, libBLS::algebra::G1Point > votedHash;
    std::vector< std::string > listUrlsToDownload;
    std::tie( listUrlsToDownload, votedHash ) =
        voteForSnapshotHash( snapshotHashAgent, blockNumber );

    if ( listUrlsToDownload.empty() ) {
        if ( !isRegularSnapshot )
            return true;
        BOOST_LOG( loggerWarning )
            << "No nodes to download from - will skip " << urlToDownloadSnapshotFrom;
        return false;
    }

    bool successfulDownload = checkLocalSnapshot( snapshotManager, blockNumber, votedHash.first );
    if ( successfulDownload )
        return successfulDownload;

    successfulDownload = tryDownloadSnapshot( snapshotManager, chainParams, listUrlsToDownload,
        votedHash, blockNumber, isRegularSnapshot );

    if ( successfulDownload ) {
        BOOST_LOG( loggerInfo ) << "Snapshot download success for block "
                                << std::to_string( blockNumber );
    }
    return successfulDownload;
}

#ifdef FAIR
uint64_t fetchLatestBlockTimestampFromNodes( const std::vector< sChainNode >& nodes ) {
    static Logger loggerWarning{ createLogger(
        VerbosityWarning, "fetchLatestBlockTimestampFromNodes" ) };
    static Logger loggerInfo{ createLogger( VerbosityInfo, "fetchLatestBlockTimestampFromNodes" ) };

    uint64_t timestamp = 0;
    // Trying to get latest block timestamp from each node until we succeed
    for ( auto& node : nodes ) {
        std::string nodeUrl = std::string( "http://" ) + std::string( node.ip ) +
                              std::string( ":" ) + ( node.port + 3 ).convert_to< std::string >();
        BOOST_LOG( loggerInfo ) << "Trying to fetch latest block timestamp from " << nodeUrl;
        try {
            timestamp = fetchLatestBlockTimestamp( nodeUrl );
        } catch ( ... ) {
            BOOST_LOG( loggerWarning ) << "Could not fetch latest block timestamp from " << nodeUrl;
        }

        if ( timestamp > 0 ) {
            BOOST_LOG( loggerInfo ) << "Successfully fetched latest block timestamp  " << timestamp
                                    << " from " << nodeUrl;
            break;
        }
    }
    if ( !timestamp ) {
        throw std::runtime_error( "Could not fetch current block timestamp from provided nodes " );
    }
    return timestamp;
}
#endif

void downloadAndProccessSnapshot( std::shared_ptr< SnapshotManager >& snapshotManager,
    const ChainParams& chainParams, const std::string& urlToDownloadSnapshotFrom,
    bool isRegularSnapshot ) {
    static Logger loggerWarning{ createLogger( VerbosityWarning, "downloadAndProccessSnapshot" ) };

    std::array< std::string, 4 > arrayCommonPublicKey =
        getBLSPublicKeyToVerifySnapshot( chainParams );

    bool successfulDownload = false;

    if ( !urlToDownloadSnapshotFrom.empty() )
        successfulDownload = downloadSnapshotFromUrl( snapshotManager, chainParams,
            arrayCommonPublicKey, urlToDownloadSnapshotFrom, isRegularSnapshot, true );
    else {
        for ( size_t idx = 0; idx < chainParams.getNodesCount() && !successfulDownload; ++idx )
            try {
                if ( chainParams.getSelfNodeId() == chainParams.getNodeByIndex( idx ).id )
                    continue;

                std::string nodeUrl =
                    std::string( "http://" ) + std::string( chainParams.getNodeByIndex( idx ).ip ) +
                    std::string( ":" ) +
                    ( chainParams.getNodeByIndex( idx ).port + 3 ).convert_to< std::string >();

                successfulDownload = downloadSnapshotFromUrl( snapshotManager, chainParams,
                    arrayCommonPublicKey, nodeUrl, isRegularSnapshot );
            } catch ( std::exception& ex ) {
                BOOST_LOG( loggerWarning ) << "Exception while trying to set up snapshot: "
                                           << dev::nested_exception_what( ex );
            }  // for blockNumber_url
    }

    if ( !successfulDownload ) {
        throw std::runtime_error( "FATAL: tried to download snapshot from everywhere!" );
    }
}

void doSnapshotDownload( const std::shared_ptr< ChainParams >& chainParams,
    std::shared_ptr< StatusAndControl >& statusAndControl,
    const std::string& urlToDownloadSnapshotFrom,
    std::shared_ptr< SnapshotManager >& snapshotManager,
    std::shared_ptr< SharedSpace >& sharedSpace, bool zeroSnapshotOnly = false ) {
    static Logger loggerInfo{ createLogger( VerbosityInfo, "doSnapshotDownload" ) };
    static Logger loggerWarning{ createLogger( VerbosityWarning, "doSnapshotDownload" ) };
#ifdef FAIR
    // To process correct signatures we fetch current block timestamp
    // from one of the nodes and temporarily changing current group

    CurrentGroup latestGroup = chainParams->getNewestGroup();
    uint64_t fetchedCurrentBlockTimestamp = fetchLatestBlockTimestampFromNodes( latestGroup.nodes );
    chainParams->updateCurrentGroupIfNeeded( fetchedCurrentBlockTimestamp );
#endif

    statusAndControl->setExitState( StatusAndControl::StartAgain, true );
    statusAndControl->setExitState( StatusAndControl::StartFromSnapshot, true );
    statusAndControl->setSubsystemRunning( StatusAndControl::SnapshotDownloader, true );

    if ( !zeroSnapshotOnly ) {
        std::unique_ptr< std::lock_guard< SharedSpace > > sharedSpace_lock;
        if ( sharedSpace )
            sharedSpace_lock.reset( new std::lock_guard< SharedSpace >( *sharedSpace ) );

        try {
            downloadAndProccessSnapshot(
                snapshotManager, *chainParams, urlToDownloadSnapshotFrom, true );

        } catch ( std::exception& e ) {
            std::throw_with_nested( std::runtime_error(
                std::string( "Fatal error in downloadAndProccessSnapshot: " ) + e.what() ) );
        }
    }
    // if we dont have 0 snapshot yet
    try {
        snapshotManager->checkSnapshotFolderAndSnapshotHash( 0 );
    } catch ( SnapshotManager::SnapshotAbsent& ex ) {
        // sleep before send skale_getSnapshot again - will receive error
        BOOST_LOG( loggerInfo )
            << std::string( "Will sleep for " )
            << chainParams->getSnapshotDownloadInactiveTimeout() +
                   dev::rpc::Skale::snapshotDownloadFragmentMonitorThreadTimeout()
            << std::string( " seconds before downloading 0 snapshot" );
        sleep( chainParams->getSnapshotDownloadInactiveTimeout() +
               dev::rpc::Skale::snapshotDownloadFragmentMonitorThreadTimeout() );
#ifdef FAIR
        // Fetch timestamp and update group again
        // since the group may change during previous requests

        fetchedCurrentBlockTimestamp = fetchLatestBlockTimestampFromNodes( latestGroup.nodes );
        chainParams->updateCurrentGroupIfNeeded( fetchedCurrentBlockTimestamp );
#endif
        downloadAndProccessSnapshot(
            snapshotManager, *chainParams, urlToDownloadSnapshotFrom, false );
        if ( zeroSnapshotOnly ) {
            // Restoring here since we do not restore it during latest snapshot download
            snapshotManager->restoreSnapshot( 0 );
        }
    } catch ( std::exception& ) {
        std::throw_with_nested( std::runtime_error( std::string(
            " Fatal error in downloadAndProccessSnapshot for zero block! Will exit " ) ) );
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

int main( int argc, char** argv ) {
    /// Loggers
    Logger loggerDebug{ createLogger( VerbosityDebug, "main" ) };
    Logger loggerInfo{ createLogger( VerbosityInfo, "main" ) };
    Logger loggerWarning{ createLogger( VerbosityWarning, "main" ) };
    Logger loggerError{ createLogger( VerbosityError, "main" ) };

    try {
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

        // init cryptographic parameters
        libBLS::init();

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

        string strJsonAdminSessionKey;
        std::shared_ptr< ChainParams > chainParams = std::make_shared< ChainParams >();
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
            "Run web3 WS(IPv4) server on specified port(and next set of ports if --acceptors > "
            "1)" );
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
            "Run web3 WS(IPv6) server on specified port(and next set of ports if --acceptors > "
            "1)" );
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
            "Specifies time margin in floating point format, in seconds, for displaying "
            "performance "
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
        addClientOption( "ws-mode", po::value< string >()->value_name( "<mode>" ),
            str_ws_mode_description.c_str() );
        addClientOption( "ws-log", po::value< string >()->value_name( "<mode>" ),
            "Web socket debug logging mode(\"none\", \"basic\", \"detailed\"; default is "
            "\"none\")" );
        addClientOption( "max-connections", po::value< size_t >()->value_name( "<count>" ),
            "Max number of RPC connections(such as web3) summary for all protocols(0 is default "
            "and "
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

        addClientOption(
            "expose-all-debug-info", "Expose extra detailed debug info into log output" );

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
        addClientOption( "enable-performance-tracker-apis",
            "Enables JSON RPC APIs for performance data recording" );

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
        addNetworkingOption( "upnp", po::value< string >()->value_name( "<on/off>" ),
            "Use UPnP for NAT (default: on)" );
#endif

        addClientOption(
            "sgx-url", po::value< string >()->value_name( "<url>" ), "SGX server url" );

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
        addGeneralOption(
            "test-ca", po::value< string >()->value_name( "<path>" ), "Test CA file" );
        addGeneralOption(
            "test-cert", po::value< string >()->value_name( "<path>" ), "Test certifcicate file" );
        addGeneralOption(
            "test-key", po::value< string >()->value_name( "<path>" ), "Test key file" );
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
            "Transaction params length limit in eth_sendRawTransaction calls for logging, specify "
            "0 "
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
            BOOST_LOG( loggerError ) << e.what();
            return EX_USAGE;
        }
        for ( size_t i = 0; i < unrecognisedOptions.size(); ++i )
            if ( !m.interpretOption( i, unrecognisedOptions ) ) {
                BOOST_LOG( loggerError ) << "Invalid argument: " << unrecognisedOptions[i];
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
            BOOST_LOG( loggerInfo ) << "NAME:\n"
                                    << "   skaled " << Version << '\n'
                                    << "USAGE:\n"
                                    << "   skaled [options]";
            BOOST_LOG( loggerInfo ) << clientDefaultMode << clientTransacting << clientNetworking;
            BOOST_LOG( loggerInfo ) << vmOptions << loggingProgramOptions << generalOptions;
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
            std::string strJSON, strURL = vm["test-url"].as< std::string >(), strPathCA,
                                 strPathCert, strPathKey;
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
                BOOST_LOG( loggerDebug ) << "Using URL ................" + u.str();
            } catch ( const std::exception& ex ) {
                BOOST_LOG( loggerError )
                    << "ERROR: Failed to parse test URL: " + std::string( ex.what() );
                return EX_TEMPFAIL;
            } catch ( ... ) {
                BOOST_LOG( loggerError ) << "ERROR: Failed to parse test URL: unknown exception";
                return EX_TEMPFAIL;
            }
            nlohmann::json joIn, joOut;
            try {
                if ( !strJSON.empty() ) {
                    joIn = nlohmann::json::parse( strJSON );
                    BOOST_LOG( loggerDebug ) << "Input JSON is ............" + joIn.dump();
                } else
                    BOOST_LOG( loggerWarning ) << "NOTICE: No valid JSON specified for test call";
            } catch ( const std::exception& ex ) {
                BOOST_LOG( loggerError )
                    << "ERROR: Failed to parse specified test JSON: " + std::string( ex.what() );
                return EX_TEMPFAIL;
            } catch ( ... ) {
                BOOST_LOG( loggerError )
                    << "ERROR: Failed to parse specified test JSON: unknown exception";
                return EX_TEMPFAIL;
            }
            skutils::http::SSL_client_options optsSSL;
            if ( !strPathCA.empty() ) {
                optsSSL.ca_file = skutils::tools::trim_copy( strPathCA );
                BOOST_LOG( loggerDebug ) << "Using CA file ..........." + strPathCA;
            }
            if ( !strPathCert.empty() ) {
                optsSSL.client_cert = skutils::tools::trim_copy( strPathCert );
                BOOST_LOG( loggerDebug ) << "Using CERT file ........." + strPathCert;
            }
            if ( !strPathKey.empty() ) {
                optsSSL.client_key = skutils::tools::trim_copy( strPathKey );
                BOOST_LOG( loggerDebug ) << "Using KEY file .........." + strPathKey;
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
                skutils::rest::data_t d = cli.call(
                    joIn, isAutoGenJsonID, edfs, wait_step, cntSteps, isReturnErrorResponse );
                if ( !d.err_s_.empty() )
                    throw std::runtime_error( "REST call error: " + d.err_s_ );
                if ( d.empty() )
                    throw std::runtime_error( "EMPTY answer received" );
                BOOST_LOG( loggerDebug ) << "Raw received data is ....." + d.s_;
                joOut = nlohmann::json::parse( d.s_ );
                BOOST_LOG( loggerDebug ) << "Output JSON is ..........." + joOut.dump();
            } catch ( const std::exception& ex ) {
                BOOST_LOG( loggerError )
                    << "ERROR: JSON RPC call failed: " + std::string( ex.what() );
                return EX_TEMPFAIL;
            } catch ( ... ) {
                BOOST_LOG( loggerError ) << "ERROR: JSON RPC call failed: unknown exception";
                return EX_TEMPFAIL;
            }
            return 0;
        }

        BOOST_LOG( loggerInfo ) << "skaled " << Version << "\n"
                                << "client " << clientVersionColorized();
        BOOST_LOG( loggerInfo ).flush();
        version();

        pid_t this_process_pid = getpid();
        BOOST_LOG( loggerDebug ) << "This process PID = " << size_t( this_process_pid );
        BOOST_LOG( loggerDebug ).flush();

        setupLogging( loggingOptions );

        // we do not really use these threads anymore
        // so setting default value to 1
        size_t nDispatchThreads = 1;
        if ( vm.count( "dispatch-threads" ) ) {
            size_t n = vm["dispatch-threads"].as< size_t >();
            const size_t nMin = 4;
            if ( n < nMin )
                n = nMin;
            nDispatchThreads = n;
        }
        BOOST_LOG( loggerInfo ) << "Using " << std::to_string( nDispatchThreads )
                                << " threads in task dispatcher";
        skutils::dispatch::default_domain( nDispatchThreads );

        bool chainConfigIsSet = false, chainConfigParsed = false;
        static nlohmann::json joConfig;

        if ( vm.count( "import-presale" ) )
            presaleImports.push_back( vm["import-presale"].as< string >() );
        if ( vm.count( "admin" ) )
            strJsonAdminSessionKey = vm["admin"].as< string >();

        if ( vm.count( "skale" ) ) {
            chainParams.reset( new ChainParams( genesisInfo( eth::Network::Skale ) ) );
            chainConfigIsSet = true;
        }

        if ( vm.count( "config" ) ) {
            try {
                configPath = vm["config"].as< string >();
                BOOST_LOG( loggerInfo ) << "main: Using config file:" << configPath;
                if ( !fs::is_regular_file( configPath.string() ) )
                    throw std::runtime_error( "Bad config file path" );
                configJSON = contentsString( configPath.string() );
                if ( configJSON.empty() )
                    throw std::runtime_error( "Config file probably not found" );
                chainParams->loadConfig( configJSON, configPath );
                chainConfigIsSet = true;
                // TODO avoid double-parse
                joConfig = nlohmann::json::parse( configJSON );
                chainConfigParsed = true;
                dev::eth::g_configAccesssor.reset(
                    new skutils::json_config_file_accessor( configPath.string() ) );
                dev::db::DBFactory::setReopenPeriodMs( chainParams->getLevelDbReopenIntervalMs() );
            } catch ( const char* str ) {
                BOOST_LOG( loggerError ) << "Error: " << str << ": " << configPath;
                return EX_USAGE;
            } catch ( const json_spirit::Error_position& err ) {
                BOOST_LOG( loggerError ) << "error in parsing config json:";
                BOOST_LOG( loggerError ) << configJSON;
                BOOST_LOG( loggerError ) << err.reason_ << " line " << err.line_;
                return EX_CONFIG;
            } catch ( const std::exception& ex ) {
                BOOST_LOG( loggerError ) << "provided configuration is incorrect";
                BOOST_LOG( loggerError ) << configJSON;
                BOOST_LOG( loggerError ) << nested_exception_what( ex );
                return EX_CONFIG;
            } catch ( ... ) {
                BOOST_LOG( loggerError ) << "provided configuration is incorrect";
                BOOST_LOG( loggerError ) << configJSON;
                return EX_CONFIG;
            }
        }

#ifndef FAIR
        // for now, leave previous values in file (for case of crash)

        if ( vm.count( "main-net-url" ) ) {
            if ( !g_configAccesssor ) {
                BOOST_LOG( loggerError )
                    << "config=<path> should be specified before --main-net-url=<url>";
                return EX_SOFTWARE;
            }
            skutils::json_config_file_accessor::g_strImaMainNetURL =
                skutils::tools::trim_copy( vm["main-net-url"].as< string >() );
            if ( !g_configAccesssor->validateImaMainNetURL() ) {
                BOOST_LOG( loggerError ) << "bad --main-net-url=<url> parameter value: "
                                         << skutils::json_config_file_accessor::g_strImaMainNetURL;
                return EX_SOFTWARE;
            }
            BOOST_LOG( loggerDebug )
                << "Main Net URL is: " << skutils::json_config_file_accessor::g_strImaMainNetURL;
        }
#endif

        if ( !chainConfigIsSet )
            // default to skale if not already set with `--config`
            chainParams.reset( new ChainParams( genesisInfo( eth::Network::Skale ) ) );

        if ( chainConfigParsed ) {
            try {
                size_t n =
                    joConfig["skaleConfig"]["nodeInfo"]["log-value-size-limit"].get< size_t >();
                cc::_max_value_size_ = ( n > 0 ) ? n : std::string::npos;
            } catch ( ... ) {
            }
            try {
                size_t n =
                    joConfig["skaleConfig"]["nodeInfo"]["log-json-string-limit"].get< size_t >();
                SkaleServerOverride::g_nMaxStringValueLengthForJsonLogs = n;
            } catch ( ... ) {
            }
            try {
                size_t n =
                    joConfig["skaleConfig"]["nodeInfo"]["log-tx-params-limit"].get< size_t >();
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
        BOOST_LOG( loggerDebug ) << "IPC server is: " << ( is_ipc ? "on" : "off" );

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
                    BOOST_LOG( loggerDebug )
                        << "Got "
                        << std::string( strDescription ) + " from configuration JSON: " << nPort;
                if ( vm.count( strCommandLineKey ) ) {
                    std::string strPort = vm[strCommandLineKey].as< string >();
                    if ( !strPort.empty() ) {
                        nPort = atoi( strPort.c_str() );
                        if ( !( 0 <= nPort && nPort <= 65535 ) )
                            nPort = -1;
                        else
                            BOOST_LOG( loggerDebug )
                                << "Got "
                                << std::string( strDescription ) + " from command line: " << nPort;
                    }
                }
                return nPort;
            };
            nExplicitPortHTTP4std = fnExtractPort( "httpRpcPort", "http-port", "HTTP/4/std port" );
            nExplicitPortHTTP4nfo =
                fnExtractPort( "infoHttpRpcPort", "info-http-port", "HTTP/4/nfo port" );
            nExplicitPortHTTP6std =
                fnExtractPort( "httpRpcPort6", "http-port6", "HTTP/6/std port" );
            nExplicitPortHTTP6nfo =
                fnExtractPort( "infoHttpRpcPort6", "info-http-port6", "HTTP/6/nfo port" );
            nExplicitPortHTTPS4std =
                fnExtractPort( "httpsRpcPort", "https-port", "HTTPS/4/std port" );
            nExplicitPortHTTPS4nfo =
                fnExtractPort( "infoHttpsRpcPort", "info-https-port", "HTTPS/4/nfo port" );
            nExplicitPortHTTPS6std =
                fnExtractPort( "httpsRpcPort6", "https-port6", "HTTPS/6/std port" );
            nExplicitPortHTTPS6nfo =
                fnExtractPort( "infoHttpsRpcPort6", "info-https-port6", "HTTPS/6/nfo port" );
            nExplicitPortWS4std = fnExtractPort( "wsRpcPort", "ws-port", "WS/4/std port" );
            nExplicitPortWS4nfo = fnExtractPort( "infoWsRpcPort", "info-ws-port", "WS/4/nfo port" );
            nExplicitPortWS6std = fnExtractPort( "wsRpcPort6", "ws-port6", "WS/6/std port" );
            nExplicitPortWS6nfo =
                fnExtractPort( "infoWsRpcPort6", "info-ws-port6", "WS/6/nfo port" );
            nExplicitPortWSS4std = fnExtractPort( "wssRpcPort", "wss-port", "WSS/4/std port" );
            nExplicitPortWSS4nfo =
                fnExtractPort( "infoWssRpcPort", "info-wss-port", "WSS/4/nfo port" );
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
        BOOST_LOG( loggerDebug ) << "JSON RPC trace logging mode is "
                                 << flag_ed( bTraceJsonRpcCalls );

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
        BOOST_LOG( loggerDebug ) << "Special JSON RPC"
                                 << " trace logging mode is "
                                 << flag_ed( bTraceJsonRpcSpecialCalls );

        // First, get "enable-personal-apis", "enable-admin-apis", "enable-debug-behavior-apis",
        // "enable-performance-tracker-apis" from config.json Second, get it from command line
        // parameter (higher priority source)
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
                        joConfig["skaleConfig"]["nodeInfo"]["enable-debug-behavior-apis"]
                            .get< bool >();
            } catch ( ... ) {
            }
            try {
                if ( joConfig["skaleConfig"]["nodeInfo"].count(
                         "enable-performance-tracker-apis" ) )
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
        BOOST_LOG( loggerWarning ) << "Important notice: Programmatic enable-personal-apis mode is "
                                   << flag_ed( bEnabledAPIs_personal );
        BOOST_LOG( loggerWarning ) << "Important notice: Programmatic enable-admin-apis mode is "
                                   << flag_ed( bEnabledAPIs_admin );
        BOOST_LOG( loggerWarning )
            << "Important notice: Programmatic enable-debug-behavior-apis mode is "
            << flag_ed( bEnabledAPIs_debug );
        BOOST_LOG( loggerWarning )
            << "Important notice: Programmatic enable-performance-tracker-apis mode is "
            << flag_ed( bEnabledAPIs_performanceTracker );

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
        BOOST_LOG( loggerWarning ) << "Important notice: Programmatic unsafe-transactions mode is "
                                   << flag_ed( !alwaysConfirm );

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
        BOOST_LOG( loggerWarning ) << "Important notice: Programmatic web3-shutdown mode is "
                                   << flag_ed( bEnabledShutdownViaWeb3 );

        // First, get "ipcpath" from config.json
        // Second, get it from command line parameter (higher priority source)
        std::string strPathIPC;
        if ( chainConfigParsed ) {
            try {
                if ( joConfig["skaleConfig"]["nodeInfo"].count( "ipcpath" ) )
                    strPathIPC =
                        joConfig["skaleConfig"]["nodeInfo"]["ipcpath"].get< std::string >();
            } catch ( ... ) {
            }
        }

        BOOST_LOG( loggerDebug ) << "IPC path is: " << strPathIPC;
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
        BOOST_LOG( loggerInfo ) << "DB path is: " << strPathDB;

        if ( !strPathDB.empty() )
            setDataDir( strPathDB );

        UnsafeRegion::init( getDataDir() );
        if ( UnsafeRegion::isActive() ) {
            BOOST_LOG( loggerError )
                << "FATAL Previous skaled shutdown was too hard, need to repair!";
            return int( ExitHandler::ec_state_root_mismatch );
        }  // if bad exit

        size_t clockDbRotationPeriodInSeconds = 0;
        if ( chainConfigParsed ) {
            try {
                if ( joConfig["skaleConfig"]["nodeInfo"].count( "block-rotation-period" ) )
                    clockDbRotationPeriodInSeconds =
                        joConfig["skaleConfig"]["nodeInfo"]["block-rotation-period"]
                            .get< size_t >();
            } catch ( ... ) {
                clockDbRotationPeriodInSeconds = 0;
            }
        }
        if ( vm.count( "block-rotation-period" ) )
            clockDbRotationPeriodInSeconds = vm["block-rotation-period"].as< size_t >();
        if ( clockDbRotationPeriodInSeconds > 0 )
            BOOST_LOG( loggerInfo )
                << "Timer-based Block Rotation period is: " << clockDbRotationPeriodInSeconds;


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
                        joConfig["skaleConfig"]["nodeInfo"]["collectionQueueSize"]
                            .get< unsigned >();
            } catch ( ... ) {
            }

            try {
                if ( joConfig["skaleConfig"]["nodeInfo"].count( "collectionDuration" ) )
                    c_collectionDuration =
                        chrono::seconds( joConfig["skaleConfig"]["nodeInfo"]["collectionDuration"]
                                             .get< unsigned >() );
            } catch ( ... ) {
            }

            try {
                if ( joConfig["skaleConfig"]["nodeInfo"].count( "transactionQueueSize" ) )
                    c_transactionQueueSize =
                        joConfig["skaleConfig"]["nodeInfo"]["transactionQueueSize"]
                            .get< unsigned >();
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
                if ( joConfig["skaleConfig"]["nodeInfo"].count(
                         "futureTransactionQueueLimitBytes" ) )
                    c_futureTransactionQueueSizeBytes =
                        joConfig["skaleConfig"]["nodeInfo"]["futureTransactionQueueLimitBytes"]
                            .get< unsigned >();
            } catch ( ... ) {
            }

            try {
                if ( joConfig["skaleConfig"]["nodeInfo"].count( "maxOpenLeveldbFiles" ) )
                    dev::db::c_maxOpenLeveldbFiles =
                        joConfig["skaleConfig"]["nodeInfo"]["maxOpenLeveldbFiles"]
                            .get< unsigned >();
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
                BOOST_LOG( loggerError ) << "Bad "
                                         << "--network-id"
                                         << " option: " << vm["network-id"].as< string >();
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
            skutils::url u( strURL );
            u.user_info( "" );
            u.user_name_and_password( "", "", true );
            u.path( "" );
            u.fragment( "" );
            u.set_query();
            strURL = u.str();
            chainParams->setSgxServerUrl( strURL );
        }

        std::shared_ptr< StatusAndControl > statusAndControl =
            std::make_shared< StatusAndControlFile >(
                boost::filesystem::path( configPath ).remove_filename() );

        // Reset subsystem running status before initialization procedure started
        statusAndControl->setSubsystemRunning( StatusAndControl::SnapshotDownloader, false );
        statusAndControl->setSubsystemRunning( StatusAndControl::Blockchain, false );
        statusAndControl->setSubsystemRunning( StatusAndControl::Rpc, false );

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
            BOOST_LOG( loggerInfo )
                << "Manually set url to download snapshot from: " << urlToDownloadSnapshotFrom;
        }

        // Checking that data dir is empty before doing creating initial layout
        bool dataDirEmpty = isDataDirEmpty();
        if ( chainParams->getSnapshotIntervalSec() > 0 || downloadSnapshotFlag ) {
            std::vector< std::string > coreVolumes = { BlockChain::getChainDirName( *chainParams ),
#ifndef FAIR
                "filestorage",
#endif
                "prices_" + chainParams->getSelfNodeId().str() + ".db",
                "blocks_" + chainParams->getSelfNodeId().str() + ".db" };
            snapshotManager.reset( new SnapshotManager(
                chainParams, getDataDir(), sharedSpace ? sharedSpace->getPath() : "" ) );
        }
        if ( downloadSnapshotFlag ) {
            if ( dataDirEmpty ) {
                doSnapshotDownload( chainParams, statusAndControl, urlToDownloadSnapshotFrom,
                    snapshotManager, sharedSpace, false );
            } else {
                BOOST_LOG( loggerInfo )
                    << "Skipping snapshot downloading since data directroy is not empty";
            }
        }  // if --download-snapshot

        // download 0 snapshot if needed
        if ( chainParams->isSyncFromCatchupEnabled() && dataDirEmpty && !downloadSnapshotFlag ) {
            // Syncing from catchup, so zeroSnapshotOnly = true
            doSnapshotDownload( chainParams, statusAndControl, urlToDownloadSnapshotFrom,
                snapshotManager, sharedSpace, true );
        }

#ifdef FAIR

        // Configuring current group if it is regular syncing from catchup.
        // Setting back correct group if it starting from snapshot mode.

        uint64_t latestBlockTs = BlockChain::getLatestBlockTimestamp( *chainParams, getDataDir() );
        BOOST_LOG( loggerInfo ) << "Latest block timestamp is: " << latestBlockTs;
        chainParams->updateCurrentGroupIfNeeded( latestBlockTs );
#endif

        statusAndControl->setSubsystemRunning( StatusAndControl::SnapshotDownloader, false );

        statusAndControl->setExitState( StatusAndControl::StartAgain, true );
        statusAndControl->setExitState( StatusAndControl::StartFromSnapshot, false );

        // it was needed for snapshot downloading
        if ( chainParams->getSnapshotIntervalSec() <= 0 ) {
            snapshotManager = nullptr;
        }

        time_t startTimestamp = 0;
        if ( vm.count( "start-timestamp" ) ) {
            startTimestamp = vm["start-timestamp"].as< time_t >();
        }

        if ( time( NULL ) < startTimestamp ) {
            statusAndControl->setSubsystemRunning( StatusAndControl::WaitingForTimestamp, true );
            BOOST_LOG( loggerInfo ) << "\nWill start at localtime " << ctime( &startTimestamp );
            do
                sleep( 1 );
            while ( time( NULL ) < startTimestamp );
            statusAndControl->setSubsystemRunning( StatusAndControl::WaitingForTimestamp, false );
        }

        if ( loggingOptions.verbosity > 0 )
            BOOST_LOG( loggerInfo ) << "skaled, a C++ Skale client";

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

        auto netPrefs = publicIP.empty() ?
                            NetworkPreferences( listenIP, listenPort, upnp ) :
                            NetworkPreferences( publicIP, listenIP, listenPort, upnp );
        netPrefs.discovery = false;
        netPrefs.pin = false;

        auto nodesState = contents( getDataDir() / fs::path( "network.rlp" ) );
        auto caps = set< string >{ "eth" };

        std::shared_ptr< GasPricer > gasPricer;

        auto rotationFlagDirPath = configPath.parent_path();
        auto instanceMonitor =
            make_shared< InstanceMonitor >( rotationFlagDirPath, statusAndControl );
        SkaleDebugInterface debugInterface;

        if ( getDataDir().size() )
            Defaults::setDBPath( getDataDir() );
        if ( nodeMode == NodeMode::Full && caps.count( "eth" ) ) {
            Ethash::init();
            NoProof::init();

            if ( chainParams->getSealEngineName() == Ethash::name() ) {
                g_client.reset( new eth::EthashClient( chainParams, chainParams->getNetworkId(),
                    shared_ptr< GasPricer >(), snapshotManager, instanceMonitor, getDataDir(),
                    withExisting,
                    TransactionQueue::Limits{ c_transactionQueueSize, c_futureTransactionQueueSize,
                        c_transactionQueueSizeBytes, c_futureTransactionQueueSizeBytes } ) );
            } else if ( chainParams->getSealEngineName() == NoProof::name() ) {
                g_client.reset( new eth::Client( chainParams, chainParams->getNetworkId(),
                    shared_ptr< GasPricer >(), snapshotManager, instanceMonitor, getDataDir(),
                    withExisting,
                    TransactionQueue::Limits{ c_transactionQueueSize, c_futureTransactionQueueSize,
                        c_transactionQueueSizeBytes, c_futureTransactionQueueSizeBytes } ) );
            } else
                BOOST_THROW_EXCEPTION(
                    ChainParamsInvalid() << errinfo_comment(
                        "Unknown seal engine: " + chainParams->getSealEngineName() ) );

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
            g_client->setAuthor( chainParams->getBlockAuthor() );

            DefaultConsensusFactory cons_fact( *g_client );
            setenv( "DATA_DIR", getDataDir().c_str(), 0 );

            std::shared_ptr< SkaleHost > skaleHost =
                std::make_shared< SkaleHost >( *g_client, &cons_fact, instanceMonitor,
#ifndef FAIR
                    skutils::json_config_file_accessor::g_strImaMainNetURL,
#endif
                    !chainParams->isSyncNode() );
            dev::eth::g_skaleHost = skaleHost;

            // XXX nested lambdas and strlen hacks..
            auto skaleHost_debug_handler = skaleHost->getDebugHandler();
            debugInterface.add_handler(
                [skaleHost_debug_handler]( const std::string& arg ) -> string {
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
                             << ( getDataDir( "ethereum" ) / fs::path( "keys.info" ) ).string()
                             << "\n";
                    }
                }
            } else {
                if ( masterSet )
                    keyManager.create( masterPassword );
                else
                    keyManager.create( std::string() );
            }
        } catch ( ... ) {
            BOOST_LOG( loggerError ) << "Error initializing key manager: "
                                     << boost::current_exception_diagnostic_information();
            return 1;
        }

        for ( auto const& presale : presaleImports )
            importPresale( keyManager, presale, [&]() {
                return getPassword( "Enter your wallet password for " + presale + ": " );
            } );

        for ( auto const& s : toImport ) {
            keyManager.import( s, "Imported key (UNSAFE)" );
        }

        if ( nodeMode == NodeMode::Full ) {
            g_client->setSealer( m.minerType() );
            if ( networkID != NoNetworkID )
                g_client->setNetworkId( networkID );
        }

        BOOST_LOG( loggerInfo ) << "Mining Beneficiary: " << g_client->author();

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
                BOOST_LOG( loggerError ) << "Bad "
                                         << "--aa"
                                         << " option: " << strAA;
                return EX_USAGE;
            }
            BOOST_LOG( loggerDebug ) << "Auto-answer mode is set to: " << strAA;
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
        if ( chainParams->getSelfNodeIp().empty() ) {
            BOOST_LOG( loggerWarning )
                << "IPv4"
                << " bind address is not set, will not start RPC on this protocol";
            nExplicitPortHTTP4std = nExplicitPortHTTPS4std = nExplicitPortHTTP4nfo =
                nExplicitPortHTTPS4nfo = nExplicitPortWS4std = nExplicitPortWSS4std =
                    nExplicitPortWS4nfo = nExplicitPortWSS4nfo = -1;
        }
        if ( chainParams->getSelfNodeIpV6().empty() ) {
            BOOST_LOG( loggerWarning )
                << "IPv6 bind address is not set, will not start RPC on this protocol";
            nExplicitPortHTTP6std = nExplicitPortHTTPS6std = nExplicitPortHTTP6nfo =
                nExplicitPortHTTPS6nfo = nExplicitPortWS6std = nExplicitPortWSS6std =
                    nExplicitPortWS6nfo = nExplicitPortWSS6nfo = -1;
        }
        if ( is_ipc || nExplicitPortHTTP4std > 0 || nExplicitPortHTTPS4std > 0 ||
             nExplicitPortHTTP6std > 0 || nExplicitPortHTTPS6std > 0 || nExplicitPortHTTP4nfo > 0 ||
             nExplicitPortHTTPS4nfo > 0 || nExplicitPortHTTP6nfo > 0 ||
             nExplicitPortHTTPS6nfo > 0 || nExplicitPortWS4std > 0 || nExplicitPortWSS4std > 0 ||
             nExplicitPortWS6std > 0 || nExplicitPortWSS6std > 0 || nExplicitPortWS4nfo > 0 ||
             nExplicitPortWSS4nfo > 0 || nExplicitPortWS6nfo > 0 || nExplicitPortWSS6nfo > 0 ) {
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
            auto pSkaleStatsFace = new rpc::SkaleStats( configPath.string(), *g_client );
            pSkaleStatsFace->isExposeAllDebugInfo_ = isExposeAllDebugInfo;
            auto pPersonalFace = bEnabledAPIs_personal ?
                                     new rpc::Personal( keyManager, *accountHolder, *g_client ) :
                                     nullptr;
            auto pAdminEthFace = bEnabledAPIs_admin ?
                                     new rpc::AdminEth( *g_client, *gasPricer.get(), keyManager,
                                         *sessionManager.get() ) :
                                     nullptr;

            auto pDebugFace = bEnabledAPIs_debug ?
                                  new rpc::Debug( *g_client, &debugInterface, argv_string ) :
                                  nullptr;
            SkaleDebugInterface::g_isEnabled = bEnabledAPIs_debug;

#ifdef HISTORIC_STATE
            // tracing interface is always enabled for the historic state nodes
            auto pTracingFace = new rpc::Tracing( *g_client, argv_string );
#else
            // tracing interface is only enabled for the historic state nodes
            auto pTracingFace = nullptr;
#endif


            auto pPerformanceTrackerFace =
                bEnabledAPIs_performanceTracker ?
                    new rpc::SkalePerformanceTracker( configPath.string() ) :
                    nullptr;

            g_jsonrpcIpcServer.reset( new FullServer( pEthFace, pSkaleFace, pSkaleStatsFace,
                pNetFace, pWeb3Face, pPersonalFace, pAdminEthFace, pDebugFace,
                pPerformanceTrackerFace, pTracingFace, nullptr ) );

            if ( is_ipc ) {
                try {
                    auto ipcConnector = new IpcServer( "geth" );
                    g_jsonrpcIpcServer->addConnector( ipcConnector );
                    if ( !ipcConnector->StartListening() ) {
                        BOOST_LOG( loggerError )
                            << "Cannot start listening for RPC requests on ipc port: "
                            << strerror( errno );
                        return EX_IOERR;
                    }  // error
                } catch ( const std::exception& ex ) {
                    BOOST_LOG( loggerError )
                        << "Cannot start listening for RPC requests on ipc port: " << ex.what();
                    return EX_IOERR;
                }  // catch
            }      // if ( is_ipc )

            auto fnCheckPort = [&]( int& nPort, const char* strCommandLineKey ) -> bool {
                if ( nPort <= 0 || nPort >= 65536 ) {
                    BOOST_LOG( loggerError ) << "WARNING: No valid port value provided with "
                                             << std::string( "--" ) + strCommandLineKey << "="
                                             << "number";
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
            if ( nExplicitPortHTTP4std > 0 || nExplicitPortHTTPS4std > 0 ||
                 nExplicitPortHTTP6std > 0 || nExplicitPortHTTPS6std > 0 ||
                 nExplicitPortHTTP4nfo > 0 || nExplicitPortHTTPS4nfo > 0 ||
                 nExplicitPortHTTP6nfo > 0 || nExplicitPortHTTPS6nfo > 0 ||
                 nExplicitPortWS4std > 0 || nExplicitPortWSS4std > 0 || nExplicitPortWS6std > 0 ||
                 nExplicitPortWSS6std > 0 || nExplicitPortWS4nfo > 0 || nExplicitPortWSS4nfo > 0 ||
                 nExplicitPortWS6nfo > 0 || nExplicitPortWSS6nfo > 0 ) {
                BOOST_LOG( loggerDebug ) << "....RPC params:";
                //
                auto fnPrintPort = [&]( const int& nPort, const char* strDescription ) -> void {
                    static const size_t nAlign = 35;
                    size_t nDescLen = strnlen( strDescription, 1024 );
                    std::string strDots;
                    for ( ; ( strDots.size() + nDescLen ) < nAlign; )
                        strDots += ".";
                    BOOST_LOG( loggerDebug )
                        << "...." << strDescription << strDots << " "
                        << ( ( nPort >= 0 ) ? std::to_string( nPort ) : "off" );
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
                BOOST_LOG( loggerDebug )
                    << "....Performance timeline tracker............. "
                    << ( pTracker->is_enabled() ?
                               std::to_string( pTracker->get_safe_max_item_count() ) :
                               "off" );

                if ( !bHaveSSL )
                    nExplicitPortHTTPS4std = nExplicitPortHTTPS6std = nExplicitPortHTTPS4nfo =
                        nExplicitPortHTTPS6nfo = nExplicitPortWSS4std = nExplicitPortWSS6std =
                            nExplicitPortWSS4nfo = nExplicitPortWSS6nfo = -1;
                if ( bHaveSSL ) {
                    BOOST_LOG( loggerDebug )
                        << "....SSL key is............................... " << strPathSslKey;
                    BOOST_LOG( loggerDebug )
                        << "....SSL certificate is....................... " << strPathSslCert;
                    BOOST_LOG( loggerDebug )
                        << "....SSL CA is................................ " << strPathSslCA;
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
                        max_http_handler_queues =
                            __SKUTILS_HTTP_DEFAULT_MAX_PARALLEL_QUEUES_COUNT__;
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
                        pg_threads =
                            joConfig["skaleConfig"]["nodeInfo"]["pg-threads"].get< int32_t >();
                        if ( pg_threads < 0 )
                            pg_threads = 0;
                    } catch ( ... ) {
                        pg_threads = 0;
                    }
                    try {
                        pg_threads_limit = joConfig["skaleConfig"]["nodeInfo"]["pg-threads-limit"]
                                               .get< int32_t >();
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
                        cntInBatch =
                            joConfig["skaleConfig"]["nodeInfo"]["max-batch"].get< size_t >();
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

                BOOST_LOG( loggerDebug )
                    << "....WS mode.................................. "
                    << skutils::ws::nlws::srvmode2str( skutils::ws::nlws::g_default_srvmode );
                BOOST_LOG( loggerDebug ) << "....WS logging............................... "
                                         << skutils::ws::wsll2str( skutils::ws::g_eWSLL );
                BOOST_LOG( loggerDebug )
                    << "....Max RPC connections...................... "
                    << ( ( maxConnections > 0 ) ? std::to_string( maxConnections ) : "disabled" );
                BOOST_LOG( loggerDebug ) << "....Max HTTP queues.......................... "
                                         << ( ( max_http_handler_queues > 0 ) ?
                                                    std::to_string( max_http_handler_queues ) :
                                                    "default" );
                BOOST_LOG( loggerDebug ) << "....Asynchronous HTTP........................ "
                                         << ( is_async_http_transfer_mode ? "yes" : "no" );
                BOOST_LOG( loggerDebug )
                    << "....Proxygen threads......................... " << pg_threads;
                BOOST_LOG( loggerDebug )
                    << "....Proxygen threads limit................... " << pg_threads_limit;

                //
                BOOST_LOG( loggerDebug )
                    << "....Max count in batch JSON RPC request...... " << cntInBatch;
                BOOST_LOG( loggerDebug )
                    << "....Parallel RPC connection acceptors........ " << cntServersStd;
                BOOST_LOG( loggerDebug )
                    << "....Parallel informational RPC acceptors..... " << cntServersNfo;
                SkaleServerOverride::fn_binary_snapshot_download_t fn_binary_snapshot_download =
                    [=]( const nlohmann::json& joRequest ) -> std::vector< uint8_t > {
                    return pSkaleFace->impl_skale_downloadSnapshotFragmentBinary( joRequest );
                };

                //
                SkaleServerOverride::opts_t serverOpts;
                inject_rapidjson_handlers( serverOpts, pEthFace );
                serverOpts.fn_binary_snapshot_download_ = fn_binary_snapshot_download;
                serverOpts.netOpts_.bindOptsStandard_.cntServers_ = cntServersStd;
                serverOpts.netOpts_.bindOptsStandard_.strAddrHTTP4_ = chainParams->getSelfNodeIp();
                serverOpts.netOpts_.bindOptsStandard_.nBasePortHTTP4_ = nExplicitPortHTTP4std;
                serverOpts.netOpts_.bindOptsStandard_.strAddrHTTP6_ =
                    chainParams->getSelfNodeIpV6();
                serverOpts.netOpts_.bindOptsStandard_.nBasePortHTTP6_ = nExplicitPortHTTP6std;
                serverOpts.netOpts_.bindOptsStandard_.strAddrHTTPS4_ = chainParams->getSelfNodeIp();
                serverOpts.netOpts_.bindOptsStandard_.nBasePortHTTPS4_ = nExplicitPortHTTPS4std;
                serverOpts.netOpts_.bindOptsStandard_.strAddrHTTPS6_ =
                    chainParams->getSelfNodeIpV6();
                serverOpts.netOpts_.bindOptsStandard_.nBasePortHTTPS6_ = nExplicitPortHTTPS6std;
                serverOpts.netOpts_.bindOptsStandard_.strAddrWS4_ = chainParams->getSelfNodeIp();
                serverOpts.netOpts_.bindOptsStandard_.nBasePortWS4_ = nExplicitPortWS4std;
                serverOpts.netOpts_.bindOptsStandard_.strAddrWS6_ = chainParams->getSelfNodeIpV6();
                serverOpts.netOpts_.bindOptsStandard_.nBasePortWS6_ = nExplicitPortWS6std;
                serverOpts.netOpts_.bindOptsStandard_.strAddrWSS4_ = chainParams->getSelfNodeIp();
                serverOpts.netOpts_.bindOptsStandard_.nBasePortWSS4_ = nExplicitPortWSS4std;
                serverOpts.netOpts_.bindOptsStandard_.strAddrWSS6_ = chainParams->getSelfNodeIpV6();
                serverOpts.netOpts_.bindOptsStandard_.nBasePortWSS6_ = nExplicitPortWSS6std;

                serverOpts.netOpts_.bindOptsInformational_.cntServers_ = cntServersNfo;
                serverOpts.netOpts_.bindOptsInformational_.strAddrHTTP4_ =
                    chainParams->getSelfNodeIp();
                serverOpts.netOpts_.bindOptsInformational_.nBasePortHTTP4_ = nExplicitPortHTTP4nfo;
                serverOpts.netOpts_.bindOptsInformational_.strAddrHTTP6_ =
                    chainParams->getSelfNodeIpV6();
                serverOpts.netOpts_.bindOptsInformational_.nBasePortHTTP6_ = nExplicitPortHTTP6nfo;
                serverOpts.netOpts_.bindOptsInformational_.strAddrHTTPS4_ =
                    chainParams->getSelfNodeIp();
                serverOpts.netOpts_.bindOptsInformational_.nBasePortHTTPS4_ =
                    nExplicitPortHTTPS4nfo;
                serverOpts.netOpts_.bindOptsInformational_.strAddrHTTPS6_ =
                    chainParams->getSelfNodeIpV6();
                serverOpts.netOpts_.bindOptsInformational_.nBasePortHTTPS6_ =
                    nExplicitPortHTTPS6nfo;

                serverOpts.netOpts_.bindOptsInformational_.strAddrWS4_ =
                    chainParams->getSelfNodeIp();
                serverOpts.netOpts_.bindOptsInformational_.nBasePortWS4_ = nExplicitPortWS4nfo;
                serverOpts.netOpts_.bindOptsInformational_.strAddrWS6_ =
                    chainParams->getSelfNodeIpV6();
                serverOpts.netOpts_.bindOptsInformational_.nBasePortWS6_ = nExplicitPortWS6nfo;
                serverOpts.netOpts_.bindOptsInformational_.strAddrWSS4_ =
                    chainParams->getSelfNodeIp();
                serverOpts.netOpts_.bindOptsInformational_.nBasePortWSS4_ = nExplicitPortWSS4nfo;
                serverOpts.netOpts_.bindOptsInformational_.strAddrWSS6_ =
                    chainParams->getSelfNodeIpV6();
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
                        throw std::runtime_error(
                            "\"ethERC20Address\" was not found in config JSON" );
                    BOOST_LOG( loggerDebug )
                        << "\"ethERC20Address\" is " + serverOpts.strEthErc20Address_;
                } catch ( ... ) {
                    serverOpts.strEthErc20Address_ = "0xd3cdbc1b727b2ed91b8ad21333841d2e96f255af";
                    BOOST_LOG( loggerWarning )
                        << "WARNING: \"ethERC20Address\" was not found in config JSON, assuming " +
                               serverOpts.strEthErc20Address_;
                }
                auto skale_server_connector =
                    new SkaleServerOverride( chainParams, g_client.get(), serverOpts );
                //
                // unddos
                if ( joConfig.count( "unddos" ) > 0 ) {
                    nlohmann::json joUnDdosSettings = joConfig["unddos"];
                    skale_server_connector->unddos_.load_settings_from_json( joUnDdosSettings );
                } else {
                    BOOST_LOG( loggerWarning ) << "No DDOS config found. DDOS Disabled";
                    skale_server_connector->unddos_.disable_ddos();  // auto-init
                }

                skale_server_connector->max_http_handler_queues_ = max_http_handler_queues;
                skale_server_connector->is_async_http_transfer_mode_ = is_async_http_transfer_mode;
                skale_server_connector->maxCountInBatchJsonRpcRequest_ = cntInBatch;
                skale_server_connector->pg_threads_ = pg_threads;
                skale_server_connector->pg_threads_limit_ = pg_threads_limit;

                if ( pg_threads > 0 ) {
                    BOOST_LOG( loggerInfo )
                        << "Count of threads in proxygen server: " << pg_threads;
                } else {
                    BOOST_LOG( loggerWarning )
                        << "Count of threads in proxygen server is not defined in config. "
                           "Using default value of 10 from the mainnet";
                    pg_threads = 10;
                    pg_threads_limit = 10;
                }

                //
                pSkaleStatsFace->setProvider( skale_server_connector );
                skale_server_connector->setConsumer( pSkaleStatsFace );
                //
                skale_server_connector->opts_.isTraceCalls_ = bTraceJsonRpcCalls;
                skale_server_connector->opts_.isTraceSpecialCalls_ = bTraceJsonRpcSpecialCalls;

                skale_server_connector->max_connection_set( maxConnections );
                g_jsonrpcIpcServer->addConnector( skale_server_connector );
                if ( !skale_server_connector->StartListening() ) {  // TODO Will it delete itself?
                    BOOST_LOG( loggerError ) << "FATAL: Failed to start JSON RPC, will exit...";
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
                int nStatWS4std = skale_server_connector->getServerPortStatusWS(
                    4, e_server_mode_t::esm_standard );
                int nStatWS4nfo = skale_server_connector->getServerPortStatusWS(
                    4, e_server_mode_t::esm_informational );
                int nStatWS6std = skale_server_connector->getServerPortStatusWS(
                    6, e_server_mode_t::esm_standard );
                int nStatWS6nfo = skale_server_connector->getServerPortStatusWS(
                    6, e_server_mode_t::esm_informational );
                int nStatWSS4std = skale_server_connector->getServerPortStatusWSS(
                    4, e_server_mode_t::esm_standard );
                int nStatWSS4nfo = skale_server_connector->getServerPortStatusWSS(
                    4, e_server_mode_t::esm_informational );
                int nStatWSS6std = skale_server_connector->getServerPortStatusWSS(
                    6, e_server_mode_t::esm_standard );
                int nStatWSS6nfo = skale_server_connector->getServerPortStatusWSS(
                    6, e_server_mode_t::esm_informational );
                static const size_t g_cntWaitAttempts = 30;
                static const std::chrono::milliseconds g_waitAttempt =
                    std::chrono::milliseconds( 100 );
                if ( nExplicitPortHTTP4std > 0 ) {
                    for ( size_t idxWaitAttempt = 0;
                          nStatHTTP4std < 0 && idxWaitAttempt < g_cntWaitAttempts &&
                          ( !ExitHandler::shouldExit() );
                          ++idxWaitAttempt ) {
                        if ( idxWaitAttempt == 0 )
                            BOOST_LOG( loggerDebug ) << "Waiting for HTTP/4/std start... ";
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
                            BOOST_LOG( loggerDebug ) << "Waiting for HTTP/4/nfo start... ";
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
                            BOOST_LOG( loggerDebug ) << "Waiting for HTTP/6/std start... ";
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
                            BOOST_LOG( loggerDebug ) << "Waiting for HTTP/6/nfo start... ";
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
                            BOOST_LOG( loggerDebug ) << "Waiting for HTTPS/4/std start... ";
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
                            BOOST_LOG( loggerDebug ) << "Waiting for HTTPS/4/nfo start... ";
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
                            BOOST_LOG( loggerDebug ) << "Waiting for HTTPS/6/std start... ";
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
                            BOOST_LOG( loggerDebug ) << "Waiting for HTTPS/6/nfo"
                                                     << " start... ";
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
                            BOOST_LOG( loggerDebug ) << "Waiting for WS/4/std start... ";
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
                            BOOST_LOG( loggerDebug ) << "Waiting for WS/4/nfo start... ";
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
                            BOOST_LOG( loggerDebug ) << "Waiting for WS/6/std start... ";
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
                            BOOST_LOG( loggerDebug ) << "Waiting for WS/6/nfo start... ";
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
                            BOOST_LOG( loggerDebug ) << "Waiting for WSS/4/std start... ";
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
                            BOOST_LOG( loggerDebug ) << "Waiting for WSS/4/nfo start... ";
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
                            BOOST_LOG( loggerDebug ) << "Waiting for WSS/6/std start... ";
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
                            BOOST_LOG( loggerDebug ) << "Waiting for WSS/6/nfo start... ";
                        nStatWSS6nfo = skale_server_connector->getServerPortStatusWSS(
                            6, e_server_mode_t::esm_informational );
                    }
                }
                BOOST_LOG( loggerDebug ) << "....RPC status:";
                auto fnPrintStatus = [&loggerDebug]( const int& nPort, const int& nStat,
                                         const char* strDescription ) -> void {
                    static const size_t nAlign = 35;
                    size_t nDescLen = strnlen( strDescription, 1024 );
                    std::string strDots;
                    for ( ; ( strDots.size() + nDescLen ) < nAlign; )
                        strDots += ".";
                    BOOST_LOG( loggerDebug )
                        << "...." << strDescription << strDots
                        << ( ( nStat >= 0 ) ? ( ( nPort > 0 ) ? std::to_string( nStat ) :
                                                                "still starting..." ) :
                                              "off" );
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
                strJsonAdminSessionKey = sessionManager->newSession(
                    rpc::SessionPermissions{ { rpc::Privilege::Admin } } );
            else
                sessionManager->addSession(
                    strJsonAdminSessionKey, rpc::SessionPermissions{ { rpc::Privilege::Admin } } );

        }  // if ( is_ipc || nExplicitPort...

        if ( bEnabledShutdownViaWeb3 ) {
            BOOST_LOG( loggerWarning ) << "Enabling programmatic shutdown via Web3...";
            dev::rpc::Skale::enableWeb3Shutdown( true );
            dev::rpc::Skale::onShutdownInvoke(
                []() { ExitHandler::exitHandler( -1, ExitHandler::ec_web3_request ); } );
            BOOST_LOG( loggerWarning ) << "Done, programmatic shutdown via Web3 is enabled";
        } else {
            BOOST_LOG( loggerDebug ) << "Disabling programmatic shutdown via Web3...";
            dev::rpc::Skale::enableWeb3Shutdown( false );
            BOOST_LOG( loggerDebug ) << "Done, programmatic shutdown via Web3 is disabled";
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

        BOOST_LOG( loggerError ) << localeconv()->decimal_point;

        std::string basename = "profile" + chainParams->getSelfNodeId().str();
        MicroProfileDumpFileImmediately(
            ( basename + ".html" ).c_str(), ( basename + ".csv" ).c_str(), nullptr );
        MicroProfileShutdown();

        ExitHandler::exit_code_t ec = ExitHandler::requestedExitCode();
        if ( ec != ExitHandler::ec_success ) {
            BOOST_LOG( loggerError ) << "Exiting main with code " << int( ec ) << "...";
        }
        return int( ec );
    } catch ( const Client::CreationException& ex ) {
        // cannot use loggerError - not in scope
        BOOST_LOG( loggerError ) << dev::nested_exception_what( ex );
        // TODO close microprofile!!
        g_client.reset( nullptr );
        return int( ExitHandler::ec_failure );
    } catch ( const SkaleHost::CreationException& ex ) {
        BOOST_LOG( loggerError ) << dev::nested_exception_what( ex );
        // TODO close microprofile!!
        g_client.reset( nullptr );
        return int( ExitHandler::ec_failure );
    } catch ( const std::exception& ex ) {
        BOOST_LOG( loggerError ) << "CRITICAL " << dev::nested_exception_what( ex );
        BOOST_LOG( loggerError ) << "\n" << skutils::signal::generate_stack_trace();
        g_client.reset( nullptr );
        return int( ExitHandler::ec_failure );
    } catch ( ... ) {
        BOOST_LOG( loggerError ) << "CRITICAL unknown error";
        BOOST_LOG( loggerError ) << "\n" << skutils::signal::generate_stack_trace();
        g_client.reset( nullptr );
        return int( ExitHandler::ec_failure );
    }
}
