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
 * @file Skale.h
 * @author Bogdan Bliznyuk
 * @date 2018
 */

#ifndef CPP_ETHEREUM_SKALE_H
#define CPP_ETHEREUM_SKALE_H

#include <stdint.h>

#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server.h>
#include <libdevcore/SharedSpace.h>
#include <libethereum/Client.h>
#include <libweb3jsonrpc/SkaleFace.h>
#include <functional>
#include <iosfwd>
#include <libconsensus/thirdparty/json.hpp>
#include <list>
#include <memory>
#include <mutex>
#include <string>

#include <boost/filesystem/path.hpp>

class SkaleHost;

namespace dev {

namespace rpc {

namespace fs = boost::filesystem;

/**
 * @brief Skale JSON-RPC api implementation
 */
class Skale : public dev::rpc::SkaleFace {
public:
    explicit Skale(
        dev::eth::Client& _client, std::shared_ptr< SharedSpace > _sharedSpace = nullptr );
    virtual ~Skale();

    virtual RPCModules implementedModules() const override {
        return RPCModules{ RPCModule{ "skale", "0.1" } };
    }


    std::string skale_protocolVersion() override;
    std::string skale_receiveTransaction( std::string const& _rlp ) override;
    std::string skale_shutdownInstance() noexcept( false ) override;
    Json::Value skale_getSnapshot( const Json::Value& request ) override;
    Json::Value skale_downloadSnapshotFragment( const Json::Value& request ) override;
    Json::Value skale_getSnapshotSignature( unsigned blockNumber ) override;
    std::string skale_getLatestSnapshotBlockNumber() override;
    std::string skale_getLatestBlockNumber() override;
    Json::Value skale_getDBUsage() override;

    std::string oracle_submitRequest( std::string& request ) override;
    std::string oracle_checkResult( std::string& receipt ) override;

    static bool isWeb3ShutdownEnabled();
    static void enableWeb3Shutdown( bool bEnable = true );
    static bool isShutdownNeeded();

    typedef std::function< void() > fn_on_shutdown_t;
    static void onShutdownInvoke( fn_on_shutdown_t fn );

    static uint64_t snapshotDownloadFragmentMonitorThreadTimeout() {
        return SNAPSHOT_DOWNLOAD_MONITOR_THREAD_SLEEP_MS;
    }

public:
    nlohmann::json impl_skale_getSnapshot(
        const nlohmann::json& joRequest, dev::eth::Client& client );
    std::vector< uint8_t > ll_impl_skale_downloadSnapshotFragment(
        const fs::path& fp, size_t idxFrom, size_t sizeOfChunk );
    std::vector< uint8_t > impl_skale_downloadSnapshotFragmentBinary(
        const nlohmann::json& joRequest );
    nlohmann::json impl_skale_downloadSnapshotFragmentJSON( const nlohmann::json& joRequest );

private:
    static volatile bool g_bShutdownViaWeb3Enabled;
    static volatile bool g_bNodeInstanceShouldShutdown;
    typedef std::list< fn_on_shutdown_t > list_fn_on_shutdown_t;
    static list_fn_on_shutdown_t g_list_fn_on_shutdown;

    static const uint64_t SNAPSHOT_DOWNLOAD_MONITOR_THREAD_SLEEP_MS;

    dev::eth::Client& m_client;
    std::shared_ptr< SharedSpace > m_shared_space;
    int currentSnapshotBlockNumber = -1;
    fs::path currentSnapshotPath;
    std::atomic< time_t > currentSnapshotTime = 0;
    std::atomic< time_t > lastSnapshotDownloadFragmentTime = 0;
    std::unique_ptr< std::thread > snapshotDownloadFragmentMonitorThread;
    std::atomic_bool threadExitRequested = false;
    mutable std::mutex m_snapshot_mutex;
};

namespace snapshot {

typedef std::function< bool( size_t idxChunck, size_t cntChunks ) > fn_progress_t;  // returns false
                                                                                    // to cancel
                                                                                    // download

extern bool download( const std::string& strURLWeb3, unsigned& block_number, const fs::path& saveTo,
    fn_progress_t onProgress, bool isBinaryDownload = true,
    std::string* pStrErrorDescription = nullptr );

};  // namespace snapshot

extern size_t g_nMaxChunckSize;

};  // namespace rpc
};  // namespace dev

#endif  // CPP_ETHEREUM_SKALE_H
