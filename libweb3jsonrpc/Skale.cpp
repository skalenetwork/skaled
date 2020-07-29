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
#include <libweb3jsonrpc/JsonHelper.h>

#include <skutils/console_colors.h>
#include <skutils/eth_utils.h>

#include <boost/algorithm/string.hpp>

//#include <nlohmann/json.hpp>
#include <json.hpp>

//#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>

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

Skale::Skale( Client& _client ) : m_client( _client ) {}

volatile bool Skale::g_bShutdownViaWeb3Enabled = false;
volatile bool Skale::g_bNodeInstanceShouldShutdown = false;
Skale::list_fn_on_shutdown_t Skale::g_list_fn_on_shutdown;

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
        std::cout << "\nINSTANCE SHUTDOWN ATTEMPT WHEN DISABLED\n\n";
        return toJS( "disabled" );
    }
    if ( g_bNodeInstanceShouldShutdown ) {
        std::cout << "\nSECONDARY INSTANCE SHUTDOWN EVENT\n\n";
        return toJS( "in progress(secondary attempt)" );
    }
    g_bNodeInstanceShouldShutdown = true;
    std::cout << "\nINSTANCE SHUTDOWN EVENT\n\n";
    for ( auto& fn : g_list_fn_on_shutdown ) {
        if ( !fn )
            continue;
        try {
            fn();
        } catch ( std::exception& ex ) {
            std::string s = ex.what();
            if ( s.empty() )
                s = "no description";
            std::cout << "Exception in shutdown event handler: " << s << "\n";
        } catch ( ... ) {
            std::cout << "Unknown exception in shutdown event handler\n";
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
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );  // TODO test!
    }
}

size_t g_nMaxChunckSize = 1024 * 1024;

//
// call example:
// curl http://127.0.0.1:7000 -X POST --data
// '{"jsonrpc":"2.0","method":"skale_getSnapshot","params":{ "blockNumber": "latest",  "autoCreate":
// false },"id":73}'
//
nlohmann::json Skale::impl_skale_getSnapshot( const nlohmann::json& joRequest, Client& client ) {
    // std::cout << cc::attention( "------------ " ) << cc::info( "skale_getSnapshot" ) <<
    // cc::normal( " call with " ) << cc::j( joRequest ) << "\n";

    nlohmann::json joResponse = nlohmann::json::object();

    // exit if too early
    if ( currentSnapshotBlockNumber >= 0 &&
         time( NULL ) - currentSnapshotTime <= SNAPSHOT_DOWNLOAD_TIMEOUT ) {
        joResponse["error"] =
            "snapshot info request received too early, no snapshot available yet, please try later "
            "or request earlier block number";
        joResponse["timeValid"] = currentSnapshotTime + SNAPSHOT_DOWNLOAD_TIMEOUT;
        return joResponse;
    }

    if ( currentSnapshotBlockNumber >= 0 )
        fs::remove( currentSnapshotPath );

    // TODO check
    unsigned blockNumber = joRequest["blockNumber"].get< unsigned >();
    currentSnapshotPath = client.createSnapshotFile( blockNumber );
    currentSnapshotTime = time( NULL );
    currentSnapshotBlockNumber = blockNumber;

    //
    //
    size_t sizeOfFile = fs::file_size( currentSnapshotPath );
    //
    //
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
    // size_t sizeOfFile = fs::file_size( fp );
    //
    //
    std::ifstream f;
    f.open( fp.native(), std::ios::in | std::ios::binary );
    if ( !f.is_open() )
        throw std::runtime_error( "failed to open snapshot file" );
    size_t i;
    std::vector< uint8_t > buffer;
    for ( i = 0; i < sizeOfChunk; ++i )
        buffer.push_back( ( uint8_t )( 0 ) );
    f.seekg( idxFrom );
    f.read( ( char* ) buffer.data(), sizeOfChunk );
    f.close();
    return buffer;
}
std::vector< uint8_t > Skale::impl_skale_downloadSnapshotFragmentBinary(
    const nlohmann::json& joRequest ) {
    //    unsigned blockNumber = joRequest["blockNumber"].get< unsigned >();
    //    ... ...
    fs::path fp = currentSnapshotPath;
    //
    size_t idxFrom = joRequest["from"].get< size_t >();
    size_t sizeOfChunk = joRequest["size"].get< size_t >();
    size_t sizeOfFile = fs::file_size( fp );
    if ( idxFrom >= sizeOfFile )
        sizeOfChunk = 0;
    if ( ( idxFrom + sizeOfChunk ) > sizeOfFile )
        sizeOfChunk = sizeOfFile - idxFrom;
    if ( sizeOfChunk > g_nMaxChunckSize )
        sizeOfChunk = g_nMaxChunckSize;
    std::vector< uint8_t > buffer =
        Skale::ll_impl_skale_downloadSnapshotFragment( fp, idxFrom, sizeOfChunk );
    return buffer;
}
nlohmann::json Skale::impl_skale_downloadSnapshotFragmentJSON( const nlohmann::json& joRequest ) {
    //    unsigned blockNumber = joRequest["blockNumber"].get< unsigned >();
    //    ... ...
    fs::path fp = currentSnapshotPath;
    //
    size_t idxFrom = joRequest["from"].get< size_t >();
    size_t sizeOfChunk = joRequest["size"].get< size_t >();
    size_t sizeOfFile = fs::file_size( fp );
    if ( idxFrom >= sizeOfFile )
        sizeOfChunk = 0;
    if ( ( idxFrom + sizeOfChunk ) > sizeOfFile )
        sizeOfChunk = sizeOfFile - idxFrom;
    if ( sizeOfChunk > g_nMaxChunckSize )
        sizeOfChunk = g_nMaxChunckSize;
    std::vector< uint8_t > buffer =
        Skale::ll_impl_skale_downloadSnapshotFragment( fp, idxFrom, sizeOfChunk );
    std::string strBase64 = skutils::tools::base64::encode( buffer.data(), sizeOfChunk );
    nlohmann::json joResponse = nlohmann::json::object();
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
    if ( chainParams.nodeInfo.keyShareName.empty() || chainParams.nodeInfo.sgxServerUrl.empty() )
        throw jsonrpc::JsonRpcException( "Snapshot signing is not enabled" );

    try {
        dev::h256 snapshot_hash = this->m_client.getSnapshotHash( blockNumber );

        nlohmann::json joCall = nlohmann::json::object();
        joCall["jsonrpc"] = "2.0";
        joCall["method"] = "blsSignMessageHash";
        nlohmann::json obj = nlohmann::json::object();

        obj["keyShareName"] = chainParams.nodeInfo.keyShareName;
        obj["messageHash"] = snapshot_hash.hex();
        obj["n"] = chainParams.sChain.nodes.size();
        obj["t"] = chainParams.sChain.t;

        auto it = std::find_if( chainParams.sChain.nodes.begin(), chainParams.sChain.nodes.end(),
            [chainParams]( const dev::eth::sChainNode& schain_node ) {
                return schain_node.id == chainParams.nodeInfo.id;
            } );
        assert( it != chainParams.sChain.nodes.end() );
        dev::eth::sChainNode schain_node = *it;

        obj["signerIndex"] = schain_node.sChainIndex.convert_to< int >();
        joCall["params"] = obj;

        std::string sgxServerURL = chainParams.nodeInfo.sgxServerUrl;

        const std::string sgx_cert_path = "/skale_node_data/sgx_certs/";
        const std::string sgx_cert_filename = "sgx.crt";
        const std::string sgx_key_filename = "sgx.key";

        skutils::http::SSL_client_options ssl_options;
        ssl_options.client_cert = sgx_cert_path + sgx_cert_filename;
        ssl_options.client_key = sgx_cert_path + sgx_key_filename;

        skutils::rest::client cli;
        cli.optsSSL = ssl_options;
        bool fl = cli.open( sgxServerURL );
        if ( !fl ) {
            std::cerr << cc::fatal( "FATAL:" )
                      << cc::error( " Exception while trying to connect to sgx server: " )
                      << cc::warn( "connection refused" ) << std::endl;
        }

        skutils::rest::data_t d;
        while ( true ) {
            bool is_continue = true;
            std::cout << cc::ws_tx( ">>> SGX call >>>" ) << " " << cc::j( joCall ) << std::endl;
            d = cli.call( joCall );
            if ( d.ei_.et_ != skutils::http::common_network_exception::error_type::et_no_error ) {
                if ( d.ei_.et_ == skutils::http::common_network_exception::error_type::et_unknown ||
                     d.ei_.et_ == skutils::http::common_network_exception::error_type::et_fatal ) {
                    std::cerr << cc::error( "ERROR:" )
                              << cc::error( " Exception while trying to connect to sgx server: " )
                              << cc::error( " error with connection: " )
                              << cc::info( " retrying... " ) << std::endl;
                } else {
                    is_continue = false;
                    std::cerr << cc::error( "ERROR:" )
                              << cc::error( " Exception while trying to connect to sgx server: " )
                              << cc::error( " error with ssl certificates " )
                              << cc::error( d.ei_.strError_ ) << std::endl;
                }
            }

            if ( !is_continue ) {
                break;
            }
        }

        if ( d.empty() ) {
            static const char g_strErrMsg[] = "SGX Server call to blsSignMessageHash failed";
            std::cout << cc::error( "!!! SGX call error !!!" ) << " " << cc::error( g_strErrMsg )
                      << std::endl;
            throw std::runtime_error( g_strErrMsg );
        }

        nlohmann::json joResponse = nlohmann::json::parse( d.s_ )["result"];
        std::cout << cc::ws_rx( "<<< SGX call <<<" ) << " " << cc::j( joResponse ) << std::endl;
        if ( joResponse["status"] != 0 ) {
            throw std::runtime_error(
                "SGX Server call to blsSignMessageHash returned non-zero status" );
        }
        std::string signature_with_helper = joResponse["signatureShare"].get< std::string >();

        std::vector< std::string > splited_string;
        splited_string = boost::split(
            splited_string, signature_with_helper, []( char c ) { return c == ':'; } );

        nlohmann::json joSignature = nlohmann::json::object();

        joSignature["X"] = splited_string[0];
        joSignature["Y"] = splited_string[1];
        joSignature["helper"] = splited_string[3];
        joSignature["hash"] = snapshot_hash.hex();
        joSignature["signerIndex"] = obj["signerIndex"];

        std::string strSignature = joSignature.dump();
        Json::Value response;
        Json::Reader().parse( strSignature, response );
        return response;
    } catch ( Exception const& ) {
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
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
        //
        //
        if ( block_number == unsigned( -1 ) ) {
            // this means "latest"
            skutils::rest::client cli;
            if ( !cli.open( strURLWeb3 ) ) {
                if ( pStrErrorDescription )
                    ( *pStrErrorDescription ) = "REST failed to connect to server(1)";
                std::cout << cc::fatal( "FATAL:" ) << " "
                          << cc::error( "REST failed to connect to server(1)" ) << "\n";
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
                std::cout << cc::fatal( "FATAL:" ) << " "
                          << cc::error( "Failed to get latest bockNumber" ) << "\n";
                return false;
            }
            // TODO catch?
            block_number = dev::eth::jsToBlockNumber(
                nlohmann::json::parse( d.s_ )["result"].get< std::string >() );
        }
        //
        //
        skutils::rest::client cli;
        if ( !cli.open( strURLWeb3 ) ) {
            if ( pStrErrorDescription )
                ( *pStrErrorDescription ) = "REST failed to connect to server(2)";
            std::cout << cc::fatal( "FATAL:" ) << " "
                      << cc::error( "REST failed to connect to server(2)" ) << "\n";
            return false;
        }

        nlohmann::json joIn = nlohmann::json::object();
        joIn["jsonrpc"] = "2.0";
        joIn["method"] = "skale_getSnapshot";
        nlohmann::json joParams = nlohmann::json::object();
        joParams["autoCreate"] = false;
        joParams["blockNumber"] = block_number;
        joIn["params"] = joParams;
        skutils::rest::data_t d = cli.call( joIn );
        if ( d.empty() ) {
            if ( pStrErrorDescription )
                ( *pStrErrorDescription ) = "REST call failed";
            std::cout << cc::fatal( "FATAL:" ) << " " << cc::error( "REST call failed" ) << "\n";
            return false;
        }
        // std::cout << cc::success( "REST call success" ) << "\n" << cc::j( d.s_ ) << "\n";
        nlohmann::json joAnswer = nlohmann::json::parse( d.s_ );
        // std::cout << cc::normal( "Got answer(1) " ) << cc::j( joAnswer ) << std::endl;
        nlohmann::json joSnapshotInfo = joAnswer["result"];
        if ( joSnapshotInfo.count( "error" ) > 0 ) {
            std::string s;
            s += "skale_getSnapshot error: ";
            s += joSnapshotInfo["error"].get< std::string >();
            if ( pStrErrorDescription )
                ( *pStrErrorDescription ) = s;
            std::cout << cc::fatal( "FATAL:" ) << " " << cc::error( s ) << "\n";
            return false;
        }
        size_t sizeOfFile = joSnapshotInfo["dataSize"].get< size_t >();
        size_t maxAllowedChunkSize = joSnapshotInfo["maxAllowedChunkSize"].get< size_t >();
        size_t idxChunk, cntChunks = sizeOfFile / maxAllowedChunkSize +
                                     ( ( ( sizeOfFile % maxAllowedChunkSize ) > 0 ) ? 1 : 0 );
        //
        //
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
            joParams["blockNumber"] = "latest";
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
                std::cout << cc::fatal( "FATAL:" ) << " "
                          << cc::error( "REST call failed(fragment downloader)" ) << "\n";
                return false;
            }
            std::vector< uint8_t > buffer;
            if ( isBinaryDownload )
                buffer.insert( buffer.end(), d.s_.begin(), d.s_.end() );
            else {
                // std::cout << cc::success( "REST call success(fragment downloader)" ) << "\n" <<
                nlohmann::json joAnswer = nlohmann::json::parse( d.s_ );
                // std::cout << cc::normal( "Got answer(2) " ) << cc::j( joAnswer ) << std::endl;
                // cc::j( d.s_ ) << "\n";
                nlohmann::json joFragment = joAnswer["result"];
                if ( joFragment.count( "error" ) > 0 ) {
                    std::string s;
                    s += "skale_downloadSnapshotFragment error: ";
                    s += joFragment["error"].get< std::string >();
                    if ( pStrErrorDescription )
                        ( *pStrErrorDescription ) = s;
                    std::cout << cc::fatal( "FATAL:" ) << " " << cc::error( s ) << "\n";
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
