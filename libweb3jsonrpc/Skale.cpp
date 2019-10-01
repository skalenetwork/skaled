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
    size_t sizeOfFile = fs::file_size( currentSnapshotPath );
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

namespace snapshot {

bool download( const std::string& strURLWeb3, unsigned& block_number, const fs::path& saveTo,
    fn_progress_t onProgress, bool isBinaryDownload, int snapshotInterval ) {
    std::ofstream f;
    try {
        boost::filesystem::remove( saveTo );
        //
        //
        if ( block_number == unsigned( -1 ) ) {
            // this means "latest"
            skutils::rest::client cli;
            if ( !cli.open( strURLWeb3 ) ) {
                // std::cout << cc::fatal( "REST failed to connect to server" ) << "\n";
                return -1;
            }

            nlohmann::json joIn = nlohmann::json::object();
            joIn["jsonrpc"] = "2.0";
            joIn["method"] = "eth_blockNumber";
            joIn["params"] = nlohmann::json::object();
            skutils::rest::data_t d = cli.call( joIn );
            if ( d.empty() ) {
                std::cout << cc::fatal( "Snapshot download failed - cannot get bockNumber" )
                          << "\n";
                return -1;
            }
            // TODO catch?
            block_number = dev::eth::jsToBlockNumber(
                nlohmann::json::parse( d.s_ )["result"].get< std::string >() );
            block_number -= block_number % snapshotInterval;
        }
        //
        //
        skutils::rest::client cli;
        if ( !cli.open( strURLWeb3 ) ) {
            // std::cout << cc::fatal( "REST failed to connect to server" ) << "\n";
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
            // std::cout << cc::fatal( "REST call failed" ) << "\n";
            return false;
        }
        // std::cout << cc::success( "REST call success" ) << "\n" << cc::j( d.s_ ) << "\n";
        nlohmann::json joSnapshotInfo = nlohmann::json::parse( d.s_ )["result"];
        size_t sizeOfFile = joSnapshotInfo["dataSize"].get< size_t >();
        size_t maxAllowedChunkSize = joSnapshotInfo["maxAllowedChunkSize"].get< size_t >();
        size_t idxChunk, cntChunks = sizeOfFile / maxAllowedChunkSize +
                                     ( ( ( sizeOfFile % maxAllowedChunkSize ) > 0 ) ? 1 : 0 );
        //
        //
        f.open( saveTo.native(), std::ios::out | std::ios::binary );
        if ( !f.is_open() )
            throw std::runtime_error( "failed to open snapshot file" );
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
                // std::cout << cc::fatal( "REST call failed(fragment downloader)" ) << "\n";
                return false;
            }
            std::vector< uint8_t > buffer;
            if ( isBinaryDownload )
                buffer.insert( buffer.end(), d.s_.begin(), d.s_.end() );
            else {
                // std::cout << cc::success( "REST call success(fragment downloader)" ) << "\n" <<
                // cc::j( d.s_ ) << "\n";
                nlohmann::json joFragment = nlohmann::json::parse( d.s_ )["result"];
                // size_t sizeArrived = joFragment["size"];
                std::string strBase64orBinary = joFragment["data"];

                buffer = skutils::tools::base64::decodeBin( strBase64orBinary );
            }
            f.write( ( char* ) buffer.data(), buffer.size() );
            bool bContinue = true;
            if ( onProgress )
                bContinue = onProgress( idxChunk, cntChunks );
            if ( !bContinue ) {
                f.close();
                boost::filesystem::remove( saveTo );
                return false;
            }
        }  // for ( idxChunk = 0; idxChunk < cntChunks; ++idxChunk )
        f.close();
        return true;
    } catch ( ... ) {
        boost::filesystem::remove( saveTo );
    }
    return false;
}

};  // namespace snapshot

};  // namespace rpc
};  // namespace dev
