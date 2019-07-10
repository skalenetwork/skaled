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
 * @file httpserveroverride.h
 * @author Dima Litvinov
 * @date 2018
 */

#ifndef HTTPSERVEROVERRIDE_H
#define HTTPSERVEROVERRIDE_H

#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>
#if defined( _WIN32 ) && !defined( __CYGWIN__ )
#include <ws2tcpip.h>
#if defined( _MSC_FULL_VER ) && !defined( _SSIZE_T_DEFINED )
#define _SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#endif  // !_SSIZE_T_DEFINED */
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <jsonrpccpp/server/abstractserverconnector.h>
#include <microhttpd.h>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

#include <skutils/console_colors.h>
#include <skutils/http.h>
#include <skutils/utils.h>
#include <skutils/ws.h>
#include <json.hpp>

#include <libdevcore/Log.h>
#include <libethereum/Interface.h>
#include <libethereum/LogFilter.h>

class SkaleServerConnectionsTrackHelper;
class SkaleWsPeer;
class SkaleRelayWS;
class SkaleServerOverride;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct SkaleServerConnectionsTrackHelper {
    SkaleServerOverride& m_sso;
    SkaleServerConnectionsTrackHelper( SkaleServerOverride& sso );
    ~SkaleServerConnectionsTrackHelper();
};  /// truct SkaleServerConnectionsTrackHelper

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SkaleWsPeer : public skutils::ws::peer {
public:
    const std::string m_strPeerQueueID;
    std::unique_ptr< SkaleServerConnectionsTrackHelper > m_pSSCTH;
    SkaleWsPeer( skutils::ws::server& srv, const skutils::ws::hdl_t& hdl );
    ~SkaleWsPeer() override;
    void onPeerRegister() override;
    void onPeerUnregister() override;  // peer will no longer receive onMessage after call to this
    void onMessage( const std::string& msg, skutils::ws::opcv eOpCode ) override;
    void onClose( const std::string& reason, int local_close_code,
        const std::string& local_close_code_as_str ) override;
    void onFail() override;
    void onLogMessage(
        skutils::ws::e_ws_log_message_type_t eWSLMT, const std::string& msg ) override;

    std::string desc( bool isColored = true ) const {
        return getShortPeerDescription( isColored, false, false );
    }
    SkaleRelayWS& getRelay();
    const SkaleRelayWS& getRelay() const { return const_cast< SkaleWsPeer* >( this )->getRelay(); }
    SkaleServerOverride* pso();
    const SkaleServerOverride* pso() const { return const_cast< SkaleWsPeer* >( this )->pso(); }
    dev::eth::Interface* ethereum() const;

protected:
    typedef std::set< unsigned > set_watche_ids_t;
    set_watche_ids_t setInstalledWatchesLogs_, setInstalledWatchesNewPendingTransactions_,
        setInstalledWatchesNewBlocks_;
    void uninstallAllWatches();

    bool handleWebSocketSpecificRequest(
        const nlohmann::json& joRequest, std::string& strResponse );
    bool handleWebSocketSpecificRequest(
        const nlohmann::json& joRequest, nlohmann::json& joResponse );

    typedef void ( SkaleWsPeer::*rpc_method_t )(
        const nlohmann::json& joRequest, nlohmann::json& joResponse );
    typedef std::map< std::string, rpc_method_t > rpc_map_t;
    static const rpc_map_t g_rpc_map;

    bool checkParamsPresent(
        const char* strMethodName, const nlohmann::json& joRequest, nlohmann::json& joResponse );
    bool checkParamsIsArray(
        const char* strMethodName, const nlohmann::json& joRequest, nlohmann::json& joResponse );

    void eth_subscribe( const nlohmann::json& joRequest, nlohmann::json& joResponse );
    void eth_subscribe_logs( const nlohmann::json& joRequest, nlohmann::json& joResponse );
    void eth_subscribe_newPendingTransactions(
        const nlohmann::json& joRequest, nlohmann::json& joResponse );
    void eth_subscribe_newHeads(
        const nlohmann::json& joRequest, nlohmann::json& joResponse, bool bIncludeTransactions );
    void eth_unsubscribe( const nlohmann::json& joRequest, nlohmann::json& joResponse );

public:
    friend class SkaleRelayWS;
};  /// class SkaleWsPeer

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SkaleServerHelper {
protected:
    int m_nServerIndex;

public:
    SkaleServerHelper( int nServerIndex = -1 ) : m_nServerIndex( nServerIndex ) {}
    virtual ~SkaleServerHelper() {}
    int serverIndex() const { return m_nServerIndex; }
};  /// class SkaleServerHelper

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SkaleRelayWS : public skutils::ws::server, public SkaleServerHelper {
protected:
    volatile bool m_isRunning = false;
    volatile bool m_isInLoop = false;
    std::string m_strScheme_;
    std::string m_strSchemeUC;
    int m_nPort = -1;
    SkaleServerOverride* m_pSO = nullptr;

public:
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;
    typedef skutils::retain_release_ptr< SkaleWsPeer > skale_peer_ptr_t;
    typedef std::map< std::string, skale_peer_ptr_t > map_skale_peers_t;  // maps m_strPeerQueueID
                                                                          // -> skale peer pointer

protected:
    mutable mutex_type m_mtxAllPeers;
    mutable map_skale_peers_t m_mapAllPeers;

public:
    SkaleRelayWS( const char* strScheme,  // "ws" or "wss"
        int nPort, int nServerIndex = -1 );
    ~SkaleRelayWS() override;
    void run( skutils::ws::fn_continue_status_flag_t fnContinueStatusFlag );
    bool isRunning() const { return m_isRunning; }
    bool isInLoop() const { return m_isInLoop; }
    void waitWhileInLoop();
    bool start( SkaleServerOverride* pSO );
    void stop();
    SkaleServerOverride* pso() { return m_pSO; }
    const SkaleServerOverride* pso() const { return m_pSO; }
    dev::eth::Interface* ethereum() const;
    mutex_type& mtxAllPeers() const { return m_mtxAllPeers; }
    friend class SkaleWsPeer;
};  /// class SkaleRelayWS

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SkaleRelayHTTP : public SkaleServerHelper {
public:
    std::shared_ptr< skutils::http::server > m_pServer;
    SkaleRelayHTTP( const char* cert_path = nullptr, const char* private_key_path = nullptr,
        int nServerIndex = -1 );
    ~SkaleRelayHTTP() override;
};  /// class SkaleRelayHTTP

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SkaleServerOverride : public jsonrpc::AbstractServerConnector {
    size_t m_cntServers;
    mutable dev::eth::Interface* pEth_;

public:
    SkaleServerOverride( size_t cntServers, dev::eth::Interface* pEth,
        const std::string& strAddrHTTP, int nBasePortHTTP, const std::string& strAddrHTTPS,
        int nBasePortHTTPS, const std::string& strAddrWS, int nBasePortWS,
        const std::string& strAddrWSS, int nBasePortWSS, const std::string& strPathSslKey,
        const std::string& strPathSslCert );
    ~SkaleServerOverride() override;

    dev::eth::Interface* ethereum() const;

private:
    bool implStartListening( std::shared_ptr< SkaleRelayHTTP >& pSrv, const std::string& strAddr,
        int nPort, const std::string& strPathSslKey, const std::string& strPathSslCert,
        int nServerIndex );
    bool implStartListening( std::shared_ptr< SkaleRelayWS >& pSrv, const std::string& strAddr,
        int nPort, const std::string& strPathSslKey, const std::string& strPathSslCert,
        int nServerIndex );
    bool implStopListening( std::shared_ptr< SkaleRelayHTTP >& pSrv, bool bIsSSL );
    bool implStopListening( std::shared_ptr< SkaleRelayWS >& pSrv, bool bIsSSL );

public:
    virtual bool StartListening() override;
    virtual bool StopListening() override;

    void SetUrlHandler( const std::string& url, jsonrpc::IClientConnectionHandler* handler );

private:
    void logTraceServerEvent(
        bool isError, const char* strProtocol, int nServerIndex, const std::string& strMessage );
    void logTraceServerTraffic( bool isRX, bool isError, const char* strProtocol, int nServerIndex,
        const char* strOrigin, const std::string& strPayload );
    const std::string m_strAddrHTTP;
    const int m_nBasePortHTTP;
    const std::string m_strAddrHTTPS;
    const int m_nBasePortHTTPS;
    const std::string m_strAddrWS;
    const int m_nBasePortWS;
    const std::string m_strAddrWSS;
    const int m_nBasePortWSS;

    std::map< std::string, jsonrpc::IClientConnectionHandler* > urlhandler;
    jsonrpc::IClientConnectionHandler* GetHandler( const std::string& url );

public:
    bool m_bTraceCalls;

private:
    std::list< std::shared_ptr< SkaleRelayHTTP > > m_serversHTTP, m_serversHTTPS;
    std::string m_strPathSslKey, m_strPathSslCert;
    std::list< std::shared_ptr< SkaleRelayWS > > m_serversWS, m_serversWSS;

    std::atomic_size_t m_cntConnections;
    std::atomic_size_t m_cntConnectionsMax;  // 0 is unlimited

public:
    // status API, returns running server port or -1 if server is not started
    int getServerPortStatusHTTP() const;
    int getServerPortStatusHTTPS() const;
    int getServerPortStatusWS() const;
    int getServerPortStatusWSS() const;

    bool is_connection_limit_overflow() const;
    void connection_counter_inc();
    void connection_counter_dec();
    size_t max_connection_get() const;
    void max_connection_set( size_t cntConnectionsMax );
    virtual void on_connection_overflow_peer_closed(
        const char* strProtocol, int nServerIndex, int nPort );

    friend class SkaleRelayWS;
    friend class SkaleWsPeer;
};  /// class SkaleServerOverride

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif  ///(!defined __HTTP_SERVER_OVERRIDE_H)
