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
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

#include <skutils/console_colors.h>
#include <skutils/dispatch.h>
#include <skutils/http.h>
#include <skutils/stats.h>
#include <skutils/unddos.h>
#include <skutils/utils.h>
#include <skutils/ws.h>
#include <json.hpp>

#include <libdevcore/Log.h>
#include <libethereum/ChainParams.h>
#include <libethereum/Interface.h>
#include <libethereum/LogFilter.h>

#include <libweb3jsonrpc/SkaleStatsSite.h>

class SkaleStatsSubscriptionManager;
struct SkaleServerConnectionsTrackHelper;
class SkaleWsPeer;
class SkaleRelayWS;
class SkaleServerOverride;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum class e_server_mode_t { esm_standard, esm_informational };

extern const char* esm2str( e_server_mode_t esm );

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SkaleStatsSubscriptionManager {
public:
    typedef int64_t subscription_id_t;

protected:
    typedef skutils::multithreading::recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;
    mutex_type mtx_;

    std::atomic< subscription_id_t > next_subscription_;
    subscription_id_t nextSubscriptionID();

    struct subscription_data_t {
        subscription_id_t m_idSubscription = 0;
        skutils::retain_release_ptr< SkaleWsPeer > m_pPeer;
        size_t m_nIntervalMilliseconds = 0;
        skutils::dispatch::job_id_t m_idDispatchJob;
    };  /// struct subscription_data_t
    typedef std::map< subscription_id_t, subscription_data_t > map_subscriptions_t;
    map_subscriptions_t map_subscriptions_;

public:
    SkaleStatsSubscriptionManager();
    virtual ~SkaleStatsSubscriptionManager();
    bool subscribe(
        subscription_id_t& idSubscription, SkaleWsPeer* pPeer, size_t nIntervalMilliseconds );
    bool unsubscribe( const subscription_id_t& idSubscription );
    void unsubscribeAll();
    virtual SkaleServerOverride& getSSO() = 0;
};  // class SkaleStatsSubscriptionManager

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct SkaleServerConnectionsTrackHelper {
    SkaleServerOverride& m_sso;
    SkaleServerConnectionsTrackHelper( SkaleServerOverride& sso );
    ~SkaleServerConnectionsTrackHelper();
};  /// struct SkaleServerConnectionsTrackHelper

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SkaleWsPeer : public skutils::ws::peer {
public:
    std::atomic_size_t nTaskNumberInPeer_ = 0;
    const std::string m_strPeerQueueID;
    std::unique_ptr< SkaleServerConnectionsTrackHelper > m_pSSCTH;
    std::string m_strUnDdosOrigin;
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

public:
    bool handleRequestWithBinaryAnswer( e_server_mode_t esm, const nlohmann::json& joRequest );

    bool handleWebSocketSpecificRequest(
        e_server_mode_t esm, const nlohmann::json& joRequest, std::string& strResponse );
    bool handleWebSocketSpecificRequest(
        e_server_mode_t esm, const nlohmann::json& joRequest, nlohmann::json& joResponse );

protected:
    typedef void ( SkaleWsPeer::*rpc_method_t )(
        e_server_mode_t esm, const nlohmann::json& joRequest, nlohmann::json& joResponse );
    typedef std::map< std::string, rpc_method_t > ws_rpc_map_t;
    static const ws_rpc_map_t g_ws_rpc_map;

    void eth_subscribe(
        e_server_mode_t esm, const nlohmann::json& joRequest, nlohmann::json& joResponse );
    void eth_subscribe_logs(
        e_server_mode_t esm, const nlohmann::json& joRequest, nlohmann::json& joResponse );
    void eth_subscribe_newPendingTransactions(
        e_server_mode_t esm, const nlohmann::json& joRequest, nlohmann::json& joResponse );
    void eth_subscribe_newHeads( e_server_mode_t esm, const nlohmann::json& joRequest,
        nlohmann::json& joResponse, bool bIncludeTransactions );
    void eth_subscribe_skaleStats(
        e_server_mode_t esm, const nlohmann::json& joRequest, nlohmann::json& joResponse );
    void eth_unsubscribe(
        e_server_mode_t esm, const nlohmann::json& joRequest, nlohmann::json& joResponse );

public:
    friend class SkaleRelayWS;

private:
    void register_ws_conn_for_origin();
    void unregister_ws_conn_for_origin();
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
    std::atomic_bool m_isRunning = false;
    std::atomic_bool m_isInLoop = false;
    int ipVer_;
    std::string strBindAddr_, strInterfaceName_;
    std::string m_strScheme_;
    std::string m_strSchemeUC;
    int m_nPort = -1;
    SkaleServerOverride* m_pSO = nullptr;
    e_server_mode_t esm_;

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
    SkaleRelayWS( int ipVer, const char* strBindAddr, const char* strScheme,  // "ws" or "wss"
        int nPort, e_server_mode_t esm, int nServerIndex = -1 );
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

    std::string nfoGetScheme() const { return m_strScheme_; }
    std::string nfoGetSchemeUC() const { return m_strSchemeUC; }

    friend class SkaleWsPeer;
};  /// class SkaleRelayWS

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SkaleRelayHTTP : public SkaleServerHelper {
protected:
    SkaleServerOverride* m_pSO = nullptr;

public:
    int ipVer_;
    std::string strBindAddr_;
    int nPort_;
    const bool m_bHelperIsSSL : 1;
    std::shared_ptr< skutils::http::server > m_pServer;
    SkaleRelayHTTP( SkaleServerOverride* pSO, int ipVer, const char* strBindAddr, int nPort,
        const char* cert_path = nullptr, const char* private_key_path = nullptr,
        int nServerIndex = -1,
        size_t a_max_http_handler_queues = __SKUTILS_HTTP_DEFAULT_MAX_PARALLEL_QUEUES_COUNT__,
        bool is_async_http_transfer_mode = true );
    ~SkaleRelayHTTP() override;
    SkaleServerOverride* pso() { return m_pSO; }
    const SkaleServerOverride* pso() const { return m_pSO; }
    bool handleHttpSpecificRequest( const std::string& strOrigin, e_server_mode_t esm,
        const std::string& strRequest, std::string& strResponse );
    bool handleHttpSpecificRequest( const std::string& strOrigin, e_server_mode_t esm,
        const nlohmann::json& joRequest, nlohmann::json& joResponse );

protected:
    typedef void ( SkaleRelayHTTP::*rpc_method_t )( const std::string& strOrigin,
        e_server_mode_t esm, const nlohmann::json& joRequest, nlohmann::json& joResponse );
    typedef std::map< std::string, rpc_method_t > http_rpc_map_t;
    static const http_rpc_map_t g_http_rpc_map;
};  /// class SkaleRelayHTTP

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SkaleServerOverride : public jsonrpc::AbstractServerConnector,
                            public SkaleStatsSubscriptionManager,
                            public dev::rpc::SkaleStatsProviderImpl {
    std::atomic_size_t nTaskNumberCall_ = 0;
    dev::eth::ChainParams& chainParams_;
    mutable dev::eth::Interface* pEth_;

public:
    typedef std::function< std::vector< uint8_t >( const nlohmann::json& joRequest ) >
        fn_binary_snapshot_download_t;

    static const double g_lfDefaultExecutionDurationMaxForPerformanceWarning;  // in seconds,
                                                                               // default 1 second

    size_t maxCountInBatchJsonRpcRequest_ = 128;

    skutils::unddos::algorithm unddos_;

    struct net_bind_opts_t {
        size_t cntServers_ = 1;
        std::string strAddrHTTP4_;
        int nBasePortHTTP4_ = 0;
        std::string strAddrHTTP6_;
        int nBasePortHTTP6_ = 0;
        std::string strAddrHTTPS4_;
        int nBasePortHTTPS4_ = 0;
        std::string strAddrHTTPS6_;
        int nBasePortHTTPS6_ = 0;
        std::string strAddrWS4_;
        int nBasePortWS4_ = 0;
        std::string strAddrWS6_;
        int nBasePortWS6_ = 0;
        std::string strAddrWSS4_;
        int nBasePortWSS4_ = 0;
        std::string strAddrWSS6_;
        int nBasePortWSS6_ = 0;
        net_bind_opts_t() {}
        net_bind_opts_t( const net_bind_opts_t& other ) { assign( other ); }
        net_bind_opts_t& operator=( const net_bind_opts_t& other ) { return assign( other ); }
        net_bind_opts_t& assign( const net_bind_opts_t& other ) {
            cntServers_ = other.cntServers_;
            strAddrHTTP4_ = other.strAddrHTTP4_;
            nBasePortHTTP4_ = other.nBasePortHTTP4_;
            strAddrHTTP6_ = other.strAddrHTTP6_;
            nBasePortHTTP6_ = other.nBasePortHTTP6_;
            strAddrHTTPS4_ = other.strAddrHTTPS4_;
            nBasePortHTTPS4_ = other.nBasePortHTTPS4_;
            strAddrHTTPS6_ = other.strAddrHTTPS6_;
            nBasePortHTTPS6_ = other.nBasePortHTTPS6_;
            strAddrWS4_ = other.strAddrWS4_;
            nBasePortWS4_ = other.nBasePortWS4_;
            strAddrWS6_ = other.strAddrWS6_;
            nBasePortWS6_ = other.nBasePortWS6_;
            strAddrWSS4_ = other.strAddrWSS4_;
            nBasePortWSS4_ = other.nBasePortWSS4_;
            strAddrWSS6_ = other.strAddrWSS6_;
            nBasePortWSS6_ = other.nBasePortWSS6_;
            return ( *this );
        }
    };
    struct net_opts_t {
        net_bind_opts_t bindOptsStandard_;
        net_bind_opts_t bindOptsInformational_;
        std::string strPathSslKey_;
        std::string strPathSslCert_;
        std::atomic_size_t cntConnections_ = 0;
        std::atomic_size_t cntConnectionsMax_ = 0;  // 0 is unlimited
        net_opts_t() {}
        net_opts_t( const net_opts_t& other ) { assign( other ); }
        net_opts_t& operator=( const net_opts_t& other ) { return assign( other ); }
        net_opts_t& assign( const net_opts_t& other ) {
            bindOptsStandard_ = other.bindOptsStandard_;
            bindOptsInformational_ = other.bindOptsInformational_;
            strPathSslKey_ = other.strPathSslKey_;
            strPathSslCert_ = other.strPathSslCert_;
            cntConnections_ = size_t( other.cntConnections_ );
            cntConnectionsMax_ = size_t( other.cntConnectionsMax_ );
            return ( *this );
        }
    };
    struct opts_t {
        net_opts_t netOpts_;
        fn_binary_snapshot_download_t fn_binary_snapshot_download_;
        double lfExecutionDurationMaxForPerformanceWarning_ = 0;  // in seconds
        bool isTraceCalls_ = false;
        bool isTraceSpecialCalls_ = false;
        std::string strEthErc20Address_;
        opts_t() {}
        opts_t( const opts_t& other ) { assign( other ); }
        opts_t& operator=( const opts_t& other ) { return assign( other ); }
        opts_t& assign( const opts_t& other ) {
            netOpts_ = other.netOpts_;
            fn_binary_snapshot_download_ = other.fn_binary_snapshot_download_;
            lfExecutionDurationMaxForPerformanceWarning_ =
                other.lfExecutionDurationMaxForPerformanceWarning_;
            isTraceCalls_ = other.isTraceCalls_;
            strEthErc20Address_ = other.strEthErc20Address_;
            return ( *this );
        }
    };
    opts_t opts_;

    SkaleServerOverride(
        dev::eth::ChainParams& chainParams, dev::eth::Interface* pEth, const opts_t& opts );
    ~SkaleServerOverride() override;

    dev::eth::Interface* ethereum() const;
    dev::eth::ChainParams& chainParams();
    const dev::eth::ChainParams& chainParams() const;
    dev::Verbosity methodTraceVerbosity( const std::string& strMethod ) const;
    bool checkAdminOriginAllowed( const std::string& origin ) const;

private:
    bool implStartListening( std::shared_ptr< SkaleRelayHTTP >& pSrv, int ipVer,
        const std::string& strAddr, int nPort, const std::string& strPathSslKey,
        const std::string& strPathSslCert, int nServerIndex, e_server_mode_t esm,
        size_t a_max_http_handler_queues = __SKUTILS_HTTP_DEFAULT_MAX_PARALLEL_QUEUES_COUNT__,
        bool is_async_http_transfer_mode = true );
    bool implStartListening( std::shared_ptr< SkaleRelayWS >& pSrv, int ipVer,
        const std::string& strAddr, int nPort, const std::string& strPathSslKey,
        const std::string& strPathSslCert, int nServerIndex, e_server_mode_t esm );
    bool implStopListening(
        std::shared_ptr< SkaleRelayHTTP >& pSrv, int ipVer, bool bIsSSL, e_server_mode_t esm );
    bool implStopListening(
        std::shared_ptr< SkaleRelayWS >& pSrv, int ipVer, bool bIsSSL, e_server_mode_t esm );

public:
    size_t max_http_handler_queues_ = __SKUTILS_HTTP_DEFAULT_MAX_PARALLEL_QUEUES_COUNT__;
    bool is_async_http_transfer_mode_ = true;
    virtual bool StartListening( e_server_mode_t esm );
    virtual bool StartListening() override;
    virtual bool StopListening( e_server_mode_t esm );
    virtual bool StopListening() override;

    void SetUrlHandler( const std::string& url, jsonrpc::IClientConnectionHandler* handler );

    void logPerformanceWarning( double lfExecutionDuration, int ipVer, const char* strProtocol,
        int nServerIndex, e_server_mode_t esm, const char* strOrigin, const char* strMethod,
        nlohmann::json joID );
    void logTraceServerEvent( bool isError, int ipVer, const char* strProtocol, int nServerIndex,
        e_server_mode_t esm, const std::string& strMessage );
    void logTraceServerTraffic( bool isRX, dev::Verbosity verbosity, int ipVer,
        const char* strProtocol, int nServerIndex, e_server_mode_t esm, const char* strOrigin,
        const std::string& strPayload );

private:
    std::map< std::string, jsonrpc::IClientConnectionHandler* > urlhandler;
    jsonrpc::IClientConnectionHandler* GetHandler( const std::string& url );

public:
    std::atomic_bool m_bShutdownMode = false;

private:
    std::list< std::shared_ptr< SkaleRelayHTTP > > serversHTTP4std_, serversHTTP6std_,
        serversHTTPS4std_, serversHTTPS6std_, serversHTTP4nfo_, serversHTTP6nfo_, serversHTTPS4nfo_,
        serversHTTPS6nfo_;
    std::list< std::shared_ptr< SkaleRelayWS > > serversWS4std_, serversWS6std_, serversWSS4std_,
        serversWSS6std_, serversWS4nfo_, serversWS6nfo_, serversWSS4nfo_, serversWSS6nfo_;

public:
    // status API, returns running server port or -1 if server is not started
    int getServerPortStatusHTTP( int ipVer, e_server_mode_t esm ) const;
    int getServerPortStatusHTTPS( int ipVer, e_server_mode_t esm ) const;
    int getServerPortStatusWS( int ipVer, e_server_mode_t esm ) const;
    int getServerPortStatusWSS( int ipVer, e_server_mode_t esm ) const;

    bool is_connection_limit_overflow() const;
    void connection_counter_inc();
    void connection_counter_dec();
    size_t max_connection_get() const;
    void max_connection_set( size_t cntConnectionsMax );
    virtual void on_connection_overflow_peer_closed(
        int ipVer, const char* strProtocol, int nServerIndex, int nPort, e_server_mode_t esm );

    SkaleServerOverride& getSSO() override;       // abstract in SkaleStatsSubscriptionManager
    nlohmann::json provideSkaleStats() override;  // abstract from dev::rpc::SkaleStatsProviderImpl

protected:
    typedef void ( SkaleServerOverride::*informational_rpc_method_t )(
        const nlohmann::json& joRequest, nlohmann::json& joResponse );
    typedef std::map< std::string, informational_rpc_method_t > informational_rpc_map_t;
    static const informational_rpc_map_t g_informational_rpc_map;

public:
    bool handleInformationalRequest( const nlohmann::json& joRequest, nlohmann::json& joResponse );

protected:
    void informational_eth_getBalance(
        const nlohmann::json& joRequest, nlohmann::json& joResponse );

public:
    bool handleRequestWithBinaryAnswer(
        e_server_mode_t esm, const nlohmann::json& joRequest, std::vector< uint8_t >& buffer );
    bool handleAdminOriginFilter( const std::string& strMethod, const std::string& strOriginURL );

    bool isShutdownMode() const { return m_bShutdownMode; }

    bool handleProtocolSpecificRequest( SkaleServerHelper& sse, const std::string& strOrigin,
        const nlohmann::json& joRequest, nlohmann::json& joResponse );

protected:
    typedef void ( SkaleServerOverride::*rpc_method_t )( SkaleServerHelper& sse,
        const std::string& strOrigin, const nlohmann::json& joRequest, nlohmann::json& joResponse );
    typedef std::map< std::string, rpc_method_t > protocol_rpc_map_t;
    static const protocol_rpc_map_t g_protocol_rpc_map;

    void setSchainExitTime( SkaleServerHelper& sse, const std::string& strOrigin,
        const nlohmann::json& joRequest, nlohmann::json& joResponse );

    unsigned iwBlockStats_ = unsigned( -1 ), iwPendingTransactionStats_ = unsigned( -1 );
    mutex_type mtxStats_;
    skutils::stats::named_event_stats statsBlocks_, statsTransactions_, statsPendingTx_;
    nlohmann::json generateBlocksStats();

    friend class SkaleRelayWS;
    friend class SkaleWsPeer;
};  /// class SkaleServerOverride

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif  ///(!defined __HTTP_SERVER_OVERRIDE_H)
