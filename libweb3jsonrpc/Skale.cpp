/*
    Copyright (C) 2018-present, SKALE Labs

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
 * @file Skale.cpp
 * @author Bogdan Bliznyuk
 * @date 2018
 */

#include "Skale.h"

#include <libskale/SkaleClient.h>

#include <libethereum/SkaleHost.h>

#include "JsonHelper.h"
#include <libethcore/Common.h>
#include <libethcore/CommonJS.h>

#include <jsonrpccpp/common/exception.h>
#include <libethereum/SchainPatch.h>
#include <libweb3jsonrpc/JsonHelper.h>

#include <skutils/console_colors.h>
#include <skutils/eth_utils.h>

#include <boost/algorithm/string.hpp>

#include <json.hpp>

#include <jsonrpccpp/client/connectors/httpclient.h>

#include <libconsensus/exceptions/InvalidStateException.h>
#include <skutils/rest_call.h>
#include <skutils/utils.h>

#include <exception>
#include <fstream>
#include <iostream>
#include <vector>

#include <cstdlib>

using namespace dev::eth;

namespace dev {
namespace rpc {

std::string exceptionToErrorMessage();

volatile bool Skale::g_bShutdownViaWeb3Enabled = false;
volatile bool Skale::g_bNodeInstanceShouldShutdown = false;
Skale::list_fn_on_shutdown_t Skale::g_list_fn_on_shutdown;
const uint64_t Skale::SNAPSHOT_DOWNLOAD_MONITOR_THREAD_SLEEP_MS = 10;

Skale::Skale( Client& _client, std::shared_ptr< SharedSpace > _sharedSpace )
    : m_client( _client ), m_shared_space( _sharedSpace ) {}

Skale::~Skale() {
    threadExitRequested = true;
    if ( snapshotDownloadFragmentMonitorThread != nullptr &&
         snapshotDownloadFragmentMonitorThread->joinable() ) {
        clog( VerbosityInfo, "Skale" ) << "Joining downloadSnapshotFragmentMonitorThread";
        snapshotDownloadFragmentMonitorThread->join();
    }
}

bool Skale::isWeb3ShutdownEnabled() {
    return g_bShutdownViaWeb3Enabled;
}
void Skale::enableWeb3Shutdown( bool bEnable /*= true*/ ) {
    if ( ( g_bShutdownViaWeb3Enabled && bEnable ) ||
         ( ( !g_bShutdownViaWeb3Enabled ) && ( !bEnable ) ) )
        return;
    g_bShutdownViaWeb3Enabled = bEnable;
    if ( !g_bShutdownViaWeb3Enabled )
        g_list_fn_on_shutdown.clear();
}

bool Skale::isShutdownNeeded() {
    return g_bNodeInstanceShouldShutdown;
}
void Skale::onShutdownInvoke( fn_on_shutdown_t fn ) {
    if ( !fn )
        return;
    g_list_fn_on_shutdown.push_back( fn );
}

std::string Skale::skale_shutdownInstance() {
    if ( !g_bShutdownViaWeb3Enabled ) {
        cwarn << "\nINSTANCE SHUTDOWN ATTEMPT WHEN DISABLED\n\n";
        return toJS( "disabled" );
    }
    if ( g_bNodeInstanceShouldShutdown ) {
        cnote << "\nSECONDARY INSTANCE SHUTDOWN EVENT\n\n";
        return toJS( "in progress(secondary attempt)" );
    }
    g_bNodeInstanceShouldShutdown = true;
    cnote << "\nINSTANCE SHUTDOWN EVENT\n\n";
    for ( auto& fn : g_list_fn_on_shutdown ) {
        if ( !fn )
            continue;
        try {
            fn();
        } catch ( std::exception& ex ) {
            std::string s = ex.what();
            if ( s.empty() )
                s = "no description";
            cerror << "Exception in shutdown event handler: " << s;
        } catch ( ... ) {
            cerror << "Unknown exception in shutdown event handler";
        }
    }  // for( auto & fn : g_list_fn_on_shutdown )
    g_list_fn_on_shutdown.clear();
    return toJS( "will shutdown" );
}

std::string Skale::skale_protocolVersion() {
    return toJS( "0.2" );
}

std::string Skale::skale_receiveTransaction( std::string const& _rlp ) {
    try {
        return toJS( m_client.skaleHost()->receiveTransaction( _rlp ) );
    } catch ( Exception const& ) {
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );  // TODO test
    }
}

size_t g_nMaxChunckSize = 100 * 1024 * 1024;

//
// call example:
// curl http://127.0.0.1:7000 -X POST --data
// '{"jsonrpc":"2.0","method":"skale_getSnapshot","params":{ "blockNumber": "latest" },"id":73}'
//
nlohmann::json Skale::impl_skale_getSnapshot( const nlohmann::json& joRequest, Client& client ) {
    std::lock_guard< std::mutex > lock( m_snapshot_mutex );
    nlohmann::json joResponse = nlohmann::json::object();

    // TODO check
    unsigned blockNumber = joRequest["blockNumber"].get< unsigned >();
    if ( blockNumber != 0 && blockNumber != m_client.getLatestSnapshotBlockNumer() ) {
        joResponse["error"] = "Invalid snapshot block number requested - it might be deleted.";
        return joResponse;
    }

    // exit if too early
    if ( currentSnapshotBlockNumber >= 0 ) {
        joResponse["error"] =
            "snapshot info request received too early, no snapshot available yet, please try later "
            "or request earlier block number";
        joResponse["timeValid"] =
            currentSnapshotTime + m_client.chainParams().sChain.snapshotDownloadTimeout;
        return joResponse;
    }

    if ( currentSnapshotBlockNumber >= 0 ) {
        fs::remove( currentSnapshotPath );
        currentSnapshotBlockNumber = -1;
        if ( m_shared_space )
            m_shared_space->unlock();
    }

    // exit if shared space unavailable
    if ( m_shared_space && !m_shared_space->try_lock() ) {
        joResponse["error"] = "snapshot serialization space is occupied, please try again later";
        joResponse["timeValid"] =
            time( NULL ) + m_client.chainParams().sChain.snapshotDownloadTimeout;
        return joResponse;
    }

    if ( snapshotDownloadFragmentMonitorThread != nullptr &&
         snapshotDownloadFragmentMonitorThread->joinable() ) {
        snapshotDownloadFragmentMonitorThread->join();
    }

    try {
        currentSnapshotPath = client.createSnapshotFile( blockNumber );
    } catch ( ... ) {
        if ( m_shared_space )
            m_shared_space->unlock();
        throw;
    }
    currentSnapshotTime = time( NULL );
    currentSnapshotBlockNumber = blockNumber;

    if ( snapshotDownloadFragmentMonitorThread == nullptr ||
         !snapshotDownloadFragmentMonitorThread->joinable() ) {
        snapshotDownloadFragmentMonitorThread.reset( new std::thread( [this]() {
            while ( ( time( NULL ) - lastSnapshotDownloadFragmentTime <
                            m_client.chainParams().sChain.snapshotDownloadInactiveTimeout ||
                        time( NULL ) - currentSnapshotTime <
                            m_client.chainParams().sChain.snapshotDownloadInactiveTimeout ) &&
                    ( time( NULL ) - currentSnapshotTime <
                            m_client.chainParams().sChain.snapshotDownloadTimeout ||
                        m_client.chainParams().nodeInfo.archiveMode ) ) {
                if ( threadExitRequested )
                    break;
                sleep( SNAPSHOT_DOWNLOAD_MONITOR_THREAD_SLEEP_MS );
            }

            clog( VerbosityInfo, "skale_downloadSnapshotFragmentMonitorThread" )
                << "Unlocking shared space.";

            std::lock_guard< std::mutex > lock( m_snapshot_mutex );
            if ( currentSnapshotBlockNumber >= 0 ) {
                try {
                    fs::remove( currentSnapshotPath );
                    clog( VerbosityInfo, "skale_downloadSnapshotFragmentMonitorThread" )
                        << "Deleted snapshot file.";
                } catch ( ... ) {
                }
                currentSnapshotBlockNumber = -1;
                if ( m_shared_space )
                    m_shared_space->unlock();
            }
        } ) );
    }

    size_t sizeOfFile = fs::file_size( currentSnapshotPath );

    joResponse["dataSize"] = sizeOfFile;
    joResponse["maxAllowedChunkSize"] = g_nMaxChunckSize;
    return joResponse;
}

Json::Value Skale::skale_getSnapshot( const Json::Value& request ) {
    try {
        Json::FastWriter fastWriter;
        std::string strRequest = fastWriter.write( request );
        nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        nlohmann::json joResponse = impl_skale_getSnapshot( joRequest, m_client );
        std::string strResponse = joResponse.dump();
        Json::Value response;
        Json::Reader().parse( strResponse, response );
        return response;
    } catch ( Exception const& ) {
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    }
}

//
// call example:
// curl http://127.0.0.1:7000 -X POST --data
// '{"jsonrpc":"2.0","method":"skale_downloadSnapshotFragment","params":{ "blockNumber": "latest",
// "from": 0, "size": 1024, "isBinary": true },"id":73}'
//
std::vector< uint8_t > Skale::ll_impl_skale_downloadSnapshotFragment(
    const fs::path& fp, size_t idxFrom, size_t sizeOfChunk ) {
    std::ifstream f;
    f.open( fp.native(), std::ios::in | std::ios::binary );
    if ( !f.is_open() )
        throw std::runtime_error( "failed to open snapshot file" );
    std::vector< uint8_t > buffer( sizeOfChunk );
    f.seekg( idxFrom );
    f.read( ( char* ) buffer.data(), sizeOfChunk );
    f.close();
    return buffer;
}
std::vector< uint8_t > Skale::impl_skale_downloadSnapshotFragmentBinary(
    const nlohmann::json& joRequest ) {
    std::lock_guard< std::mutex > lock( m_snapshot_mutex );

    lastSnapshotDownloadFragmentTime = time( NULL );

    if ( currentSnapshotBlockNumber < 0 ) {
        return std::vector< uint8_t >();
    }

    fs::path fp = currentSnapshotPath;

    size_t idxFrom = joRequest["from"].get< size_t >();
    size_t sizeOfChunk = joRequest["size"].get< size_t >();
    size_t sizeOfFile = fs::file_size( fp );
    if ( idxFrom >= sizeOfFile )
        sizeOfChunk = 0;
    else if ( ( idxFrom + sizeOfChunk ) > sizeOfFile )
        sizeOfChunk = sizeOfFile - idxFrom;
    if ( sizeOfChunk > g_nMaxChunckSize )
        sizeOfChunk = g_nMaxChunckSize;
    std::vector< uint8_t > buffer =
        Skale::ll_impl_skale_downloadSnapshotFragment( fp, idxFrom, sizeOfChunk );
    return buffer;
}
nlohmann::json Skale::impl_skale_downloadSnapshotFragmentJSON( const nlohmann::json& joRequest ) {
    std::lock_guard< std::mutex > lock( m_snapshot_mutex );

    lastSnapshotDownloadFragmentTime = time( NULL );
    nlohmann::json joResponse = nlohmann::json::object();

    if ( currentSnapshotBlockNumber < 0 )
        return "there's no current snapshot, or snapshot expired; please call skale_getSnapshot() "
               "first";

    fs::path fp = currentSnapshotPath;

    size_t idxFrom = joRequest["from"].get< size_t >();
    size_t sizeOfChunk = joRequest["size"].get< size_t >();
    size_t sizeOfFile = fs::file_size( fp );
    if ( idxFrom >= sizeOfFile )
        sizeOfChunk = 0;
    else if ( ( idxFrom + sizeOfChunk ) > sizeOfFile )
        sizeOfChunk = sizeOfFile - idxFrom;
    if ( sizeOfChunk > g_nMaxChunckSize )
        sizeOfChunk = g_nMaxChunckSize;
    std::vector< uint8_t > buffer =
        Skale::ll_impl_skale_downloadSnapshotFragment( fp, idxFrom, sizeOfChunk );
    std::string strBase64 = skutils::tools::base64::encode( buffer.data(), sizeOfChunk );

    if ( sizeOfChunk + idxFrom == sizeOfFile )
        clog( VerbosityInfo, "skale_downloadSnapshotFragment" )
            << "Sent all chunks for " << currentSnapshotPath.string();

    joResponse["size"] = sizeOfChunk;
    joResponse["data"] = strBase64;
    return joResponse;
}

Json::Value Skale::skale_downloadSnapshotFragment( const Json::Value& request ) {
    try {
        Json::FastWriter fastWriter;
        std::string strRequest = fastWriter.write( request );
        nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        nlohmann::json joResponse = impl_skale_downloadSnapshotFragmentJSON( joRequest );
        std::string strResponse = joResponse.dump();
        Json::Value response;
        Json::Reader().parse( strResponse, response );
        return response;
    } catch ( Exception const& ) {
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    }
}

std::string Skale::skale_getLatestBlockNumber() {
    return std::to_string( this->m_client.number() );
}

std::string Skale::skale_getLatestSnapshotBlockNumber() {
    int64_t response = this->m_client.getLatestSnapshotBlockNumer();
    return response > 0 ? std::to_string( response ) : "earliest";
}

Json::Value Skale::skale_getSnapshotSignature( unsigned blockNumber ) {
    dev::eth::ChainParams chainParams = this->m_client.chainParams();
    if ( !chainParams.nodeInfo.syncNode && ( chainParams.nodeInfo.keyShareName.empty() ||
                                               chainParams.nodeInfo.sgxServerUrl.empty() ) )
        throw jsonrpc::JsonRpcException( "Snapshot signing is not enabled" );

    if ( blockNumber != 0 && blockNumber != this->m_client.getLatestSnapshotBlockNumer() ) {
        throw jsonrpc::JsonRpcException(
            "Invalid snapshot block number requested - it might be deleted." );
    }

    try {
        dev::h256 snapshotHash = this->m_client.getSnapshotHash( blockNumber );
        if ( !snapshotHash )
            throw std::runtime_error(
                "Requested hash of block " + to_string( blockNumber ) + " is absent" );

        nlohmann::json joSignature = nlohmann::json::object();
        if ( !chainParams.nodeInfo.syncNode ) {
            std::string sgxServerURL = chainParams.nodeInfo.sgxServerUrl;
            skutils::url u( sgxServerURL );

            nlohmann::json joCall = nlohmann::json::object();
            joCall["jsonrpc"] = "2.0";
            joCall["method"] = "blsSignMessageHash";
            if ( u.scheme() == "zmq" )
                joCall["type"] = "BLSSignReq";
            nlohmann::json obj = nlohmann::json::object();

            obj["keyShareName"] = chainParams.nodeInfo.keyShareName;
            obj["messageHash"] = snapshotHash.hex();
            obj["n"] = chainParams.sChain.nodes.size();
            obj["t"] = chainParams.sChain.t;

            auto it =
                std::find_if( chainParams.sChain.nodes.begin(), chainParams.sChain.nodes.end(),
                    [chainParams]( const dev::eth::sChainNode& schain_node ) {
                        return schain_node.id == chainParams.nodeInfo.id;
                    } );
            assert( it != chainParams.sChain.nodes.end() );
            dev::eth::sChainNode schain_node = *it;

            joCall["params"] = obj;

            // TODO deduplicate with SkaleHost
            std::string sgx_cert_path =
                getenv( "SGX_CERT_FOLDER" ) ? getenv( "SGX_CERT_FOLDER" ) : "";
            if ( sgx_cert_path.empty() )
                sgx_cert_path = "/skale_node_data/sgx_certs/";
            else if ( sgx_cert_path[sgx_cert_path.length() - 1] != '/' )
                sgx_cert_path += '/';

            const char* sgx_cert_filename = getenv( "SGX_CERT_FILE" );
            if ( sgx_cert_filename == nullptr )
                sgx_cert_filename = "sgx.crt";

            const char* sgx_key_filename = getenv( "SGX_KEY_FILE" );
            if ( sgx_key_filename == nullptr )
                sgx_key_filename = "sgx.key";

            skutils::http::SSL_client_options ssl_options;
            ssl_options.client_cert = sgx_cert_path + sgx_cert_filename;
            ssl_options.client_key = sgx_cert_path + sgx_key_filename;

            skutils::rest::client cli( skutils::rest::g_nClientConnectionTimeoutMS );
            cli.optsSSL_ = ssl_options;
            bool fl = cli.open( sgxServerURL );
            if ( !fl ) {
                clog( VerbosityError, "skale_getSnapshotSignature" )
                    << "FATAL:"
                    << " Exception while trying to connect to sgx server: "
                    << "connection refused";
            }

            skutils::rest::data_t d;
            while ( true ) {
                clog( VerbosityInfo, "skale_getSnapshotSignature" ) << ">>> SGX call >>>"
                                                                    << " " << joCall;
                d = cli.call( joCall );
                if ( d.ei_.et_ !=
                     skutils::http::common_network_exception::error_type::et_no_error ) {
                    if ( d.ei_.et_ ==
                             skutils::http::common_network_exception::error_type::et_unknown ||
                         d.ei_.et_ ==
                             skutils::http::common_network_exception::error_type::et_fatal ) {
                        clog( VerbosityError, "skale_getSnapshotSignature" )
                            << "ERROR:"
                            << " Exception while trying to connect to sgx server: "
                            << " error with connection: "
                            << " retrying... ";
                    } else {
                        clog( VerbosityError, "skale_getSnapshotSignature" )
                            << "ERROR:"
                            << " Exception while trying to connect to sgx server: "
                            << " error with ssl certificates " << d.ei_.strError_;
                    }
                } else {
                    break;
                }
            }

            if ( d.empty() ) {
                static const char g_strErrMsg[] = "SGX Server call to blsSignMessageHash failed";
                clog( VerbosityError, "skale_getSnapshotSignature" ) << "!!! SGX call error !!!"
                                                                     << " " << g_strErrMsg;
                throw std::runtime_error( g_strErrMsg );
            }

            nlohmann::json joAnswer = nlohmann::json::parse( d.s_ );
            nlohmann::json joResponse =
                ( joAnswer.count( "result" ) > 0 ) ? joAnswer["result"] : joAnswer;
            clog( VerbosityInfo, "skale_getSnapshotSignature" ) << "<<< SGX call <<<"
                                                                << " " << joResponse;
            if ( joResponse["status"] != 0 ) {
                throw std::runtime_error(
                    "SGX Server call to blsSignMessageHash returned non-zero status" );
            }
            std::string signature_with_helper = joResponse["signatureShare"].get< std::string >();

            std::vector< std::string > splidString;
            splidString = boost::split(
                splidString, signature_with_helper, []( char c ) { return c == ':'; } );

            joSignature["X"] = splidString.at( 0 );
            joSignature["Y"] = splidString.at( 1 );
            joSignature["helper"] = splidString.at( 3 );
        } else {
            joSignature["X"] = "1";
            joSignature["Y"] = "2";
            joSignature["helper"] = "1";
        }

        joSignature["hash"] = snapshotHash.hex();

        std::string strSignature = joSignature.dump();
        Json::Value response;
        Json::Reader().parse( strSignature, response );
        return response;
    } catch ( Exception const& ) {
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    }
}

Json::Value Skale::skale_getDBUsage() {
    nlohmann::json joDBUsageInfo = nlohmann::json::object();

    nlohmann::json joSkaledDBUsage = nlohmann::json::object();

    auto blocksDbUsage = m_client.getBlocksDbUsage();
    auto stateDbUsage = m_client.getStateDbUsage();
#ifdef HISTORIC_STATE
    auto historicStateDbUsage = m_client.getHistoricStateDbUsage();
    auto historicRootsDbUsage = m_client.getHistoricRootsDbUsage();
    joSkaledDBUsage["historic_state.db_disk_usage"] = historicStateDbUsage;
    joSkaledDBUsage["historic_roots.db_disk_usage"] = historicRootsDbUsage;
#endif  // HISTORIC_STATE

    joSkaledDBUsage["blocks.db_disk_usage"] = blocksDbUsage.first;
    joSkaledDBUsage["pieceUsageBytes"] = blocksDbUsage.second;
    joSkaledDBUsage["state.db_disk_usage"] = stateDbUsage.first;
    joSkaledDBUsage["contractStorageUsed"] = stateDbUsage.second;

    joDBUsageInfo["skaledDBUsage"] = joSkaledDBUsage;

    nlohmann::json joConsensusDBUsage = nlohmann::json::object();
    auto consensusDbUsage = m_client.skaleHost()->getConsensusDbUsage();
    for ( const auto& [key, val] : consensusDbUsage ) {
        joConsensusDBUsage[key] = val;
    }

    joDBUsageInfo["consensusDBUsage"] = joConsensusDBUsage;

    std::string strResponse = joDBUsageInfo.dump();
    Json::Value response;
    Json::Reader().parse( strResponse, response );
    return response;
}

std::string Skale::oracle_submitRequest( std::string& request ) {
    try {
        if ( m_client.chainParams().nodeInfo.syncNode )
            throw std::runtime_error( "Oracle is disabled on this instance" );
        std::string receipt;
        std::string errorMessage;

        clog( VerbosityDebug, "Oracle request:" ) << request;

        uint64_t status = this->m_client.submitOracleRequest( request, receipt, errorMessage );
        if ( status != ORACLE_SUCCESS ) {
            throw jsonrpc::JsonRpcException( status, errorMessage );
        }
        return receipt;
    } catch ( jsonrpc::JsonRpcException const& e ) {
        throw e;
    } catch ( const std::exception& e ) {
        throw jsonrpc::JsonRpcException( ORACLE_INTERNAL_SERVER_ERROR, e.what() );
    }
}

std::string Skale::oracle_checkResult( std::string& receipt ) {
    try {
        if ( m_client.chainParams().nodeInfo.syncNode )
            throw std::runtime_error( "Oracle is disabled on this instance" );
        std::string result;
        // this function is guaranteed not to throw exceptions
        uint64_t status = m_client.checkOracleResult( receipt, result );
        switch ( status ) {
        case ORACLE_SUCCESS:
            break;
        case ORACLE_RESULT_NOT_READY:
            throw jsonrpc::JsonRpcException( status, "Oracle result is not ready" );
        default:
            throw jsonrpc::JsonRpcException(
                status, skutils::tools::format( "Oracle request failed with status %zu", status ) );
        }
        clog( VerbosityDebug, "Oracle result:" ) << result;
        return result;
    } catch ( jsonrpc::JsonRpcException const& e ) {
        throw e;
    } catch ( const std::exception& e ) {
        throw jsonrpc::JsonRpcException( ORACLE_INTERNAL_SERVER_ERROR, e.what() );
    }
}

namespace snapshot {

bool download( const std::string& strURLWeb3, unsigned& block_number, const fs::path& saveTo,
    fn_progress_t onProgress, bool isBinaryDownload, std::string* pStrErrorDescription ) {
    if ( pStrErrorDescription )
        pStrErrorDescription->clear();
    std::ofstream f;
    try {
        boost::filesystem::remove( saveTo );

        if ( block_number == unsigned( -1 ) ) {
            // this means "latest"
            skutils::rest::client cli( skutils::rest::g_nClientConnectionTimeoutMS );
            if ( !cli.open( strURLWeb3 ) ) {
                if ( pStrErrorDescription )
                    ( *pStrErrorDescription ) = "REST failed to connect to server(1)";
                clog( VerbosityError, "download snapshot" )
                    << "FATAL:"
                    << " "
                    << "REST failed to connect to server(1)";
                return false;
            }

            nlohmann::json joIn = nlohmann::json::object();
            joIn["jsonrpc"] = "2.0";
            joIn["method"] = "skale_getLatestSnapshotBlockNumber";
            joIn["params"] = nlohmann::json::object();
            skutils::rest::data_t d = cli.call( joIn );
            if ( d.empty() ) {
                if ( pStrErrorDescription )
                    ( *pStrErrorDescription ) = "Failed to get latest bockNumber";
                clog( VerbosityError, "download snapshot" ) << "FATAL:"
                                                            << " "
                                                            << "Failed to get latest bockNumber";
                return false;
            }
            // TODO catch?
            nlohmann::json joAnswer = nlohmann::json::parse( d.s_ );
            block_number = dev::eth::jsToBlockNumber( joAnswer["result"].get< std::string >() );
        }

        skutils::rest::client cli( skutils::rest::g_nClientConnectionTimeoutMS );
        if ( !cli.open( strURLWeb3 ) ) {
            if ( pStrErrorDescription )
                ( *pStrErrorDescription ) = "REST failed to connect to server(2)";
            clog( VerbosityError, "download snapshot" ) << "FATAL:"
                                                        << " "
                                                        << "REST failed to connect to server(2)";
            return false;
        }

        nlohmann::json joIn = nlohmann::json::object();
        joIn["jsonrpc"] = "2.0";
        joIn["method"] = "skale_getSnapshot";
        nlohmann::json joParams = nlohmann::json::object();
        joParams["blockNumber"] = block_number;
        joIn["params"] = joParams;
        skutils::rest::data_t d = cli.call( joIn );
        if ( !d.err_s_.empty() ) {
            if ( pStrErrorDescription )
                ( *pStrErrorDescription ) = "REST call failed: " + d.err_s_;
            clog( VerbosityError, "download snapshot" ) << "FATAL:"
                                                        << " "
                                                        << "REST call failed: " << d.err_s_;
            return false;
        }
        if ( d.empty() ) {
            if ( pStrErrorDescription )
                ( *pStrErrorDescription ) = "REST call failed";
            clog( VerbosityError, "download snapshot" ) << "FATAL:"
                                                        << " "
                                                        << "REST call failed";
            return false;
        }
        nlohmann::json joAnswer = nlohmann::json::parse( d.s_ );
        nlohmann::json joSnapshotInfo = joAnswer["result"];
        if ( joSnapshotInfo.count( "error" ) > 0 ) {
            std::string s;
            s += "skale_getSnapshot error: ";
            s += joSnapshotInfo["error"].get< std::string >();
            if ( joSnapshotInfo.count( "timeValid" ) > 0 ) {
                std::time_t timeStamp = joSnapshotInfo["timeValid"].get< time_t >();
                s += "; Invalid time to download snapshot. Valid time is ";
                s += std::string( std::asctime( std::gmtime( &timeStamp ) ) );
            }
            if ( pStrErrorDescription )
                ( *pStrErrorDescription ) = s;
            clog( VerbosityError, "download snapshot" ) << "FATAL:"
                                                        << " " << s;
            return false;
        }
        size_t sizeOfFile = joSnapshotInfo["dataSize"].get< size_t >();
        size_t maxAllowedChunkSize = joSnapshotInfo["maxAllowedChunkSize"].get< size_t >();
        size_t idxChunk, cntChunks = sizeOfFile / maxAllowedChunkSize +
                                     ( ( ( sizeOfFile % maxAllowedChunkSize ) > 0 ) ? 1 : 0 );

        f.open( saveTo.native(), std::ios::out | std::ios::binary );
        if ( !f.is_open() ) {
            std::string s;
            s += "failed to open snapshot file \"";
            s += saveTo.native();
            s += "\"";
            if ( pStrErrorDescription )
                ( *pStrErrorDescription ) = s;
            throw std::runtime_error( s );
        }
        for ( idxChunk = 0; idxChunk < cntChunks; ++idxChunk ) {
            nlohmann::json joIn = nlohmann::json::object();
            joIn["jsonrpc"] = "2.0";
            joIn["method"] = "skale_downloadSnapshotFragment";
            nlohmann::json joParams = nlohmann::json::object();
            joParams["from"] = idxChunk * maxAllowedChunkSize;
            joParams["size"] = maxAllowedChunkSize;
            joParams["isBinary"] = isBinaryDownload;
            joIn["params"] = joParams;
            skutils::rest::data_t d = cli.call( joIn, true,
                isBinaryDownload ? skutils::rest::e_data_fetch_strategy::edfs_nearest_binary :
                                   skutils::rest::e_data_fetch_strategy::edfs_default );
            if ( d.empty() ) {
                if ( pStrErrorDescription )
                    ( *pStrErrorDescription ) = "REST call failed(fragment downloader)";
                clog( VerbosityError, "download snapshot" )
                    << "FATAL:"
                    << " "
                    << "REST call failed(fragment downloader)";
                return false;
            }
            std::vector< uint8_t > buffer;
            if ( isBinaryDownload )
                buffer.insert( buffer.end(), d.s_.begin(), d.s_.end() );
            else {
                nlohmann::json joAnswer = nlohmann::json::parse( d.s_ );
                nlohmann::json joFragment = joAnswer["result"];
                if ( joFragment.count( "error" ) > 0 ) {
                    std::string s;
                    s += "skale_downloadSnapshotFragment error: ";
                    s += joFragment["error"].get< std::string >();
                    if ( pStrErrorDescription )
                        ( *pStrErrorDescription ) = s;
                    clog( VerbosityError, "download snapshot" ) << "FATAL:"
                                                                << " " << s;
                    return false;
                }
                // size_t sizeArrived = joFragment["size"];
                std::string strBase64orBinary = joFragment["data"];

                buffer = skutils::tools::base64::decodeBin( strBase64orBinary );
            }
            f.write( ( char* ) buffer.data(), buffer.size() );
            bool bContinue = true;
            if ( onProgress )
                bContinue = onProgress( idxChunk, cntChunks );
            if ( !bContinue ) {
                if ( pStrErrorDescription )
                    ( *pStrErrorDescription ) = "fragment downloader stopped by callback";
                f.close();
                boost::filesystem::remove( saveTo );
                return false;
            }
        }  // for ( idxChunk = 0; idxChunk < cntChunks; ++idxChunk )
        f.close();
        return true;
    } catch ( const std::exception& ex ) {
        if ( pStrErrorDescription )
            ( *pStrErrorDescription ) = ex.what();
    } catch ( ... ) {
        if ( pStrErrorDescription )
            ( *pStrErrorDescription ) = "unknown exception";
        boost::filesystem::remove( saveTo );
    }
    return false;
}

};  // namespace snapshot

};  // namespace rpc
};  // namespace dev
