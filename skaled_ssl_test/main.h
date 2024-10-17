#if ( !defined __MAIN_H )
#define __MAIN_H 1

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include <skutils/async_work.h>
#include <skutils/atomic_shared_ptr.h>
#include <skutils/command_line_parser.h>
#include <skutils/console_colors.h>
#include <skutils/dispatch.h>
#include <skutils/http.h>
#include <skutils/mail.h>
#include <skutils/multifunction.h>
#include <skutils/multithreading.h>
#include <skutils/network.h>
#include <skutils/rest_call.h>
#include <skutils/thread_pool.h>
#include <skutils/unddos.h>
#include <skutils/url.h>
#include <skutils/utils.h>
#include <skutils/ws.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern std::string generate_new_call_id( const char* strPrefix = nullptr );
extern std::string generate_new_call_id( const std::string& strPrefix );
extern void ensure_call_id_present( nlohmann::json& joMsg, const char* strCustomPrefix = nullptr );
extern nlohmann::json ensure_call_id_present_copy(
    const nlohmann::json& joMsg, const char* strCustomPrefix = nullptr );

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class helper_ssl_cert_and_key_holder {
    bool need_remove_files_ : 1;

public:
    std::string strFilePathKey_;
    std::string strFilePathCert_;
    static std::string g_strFilePathKey;
    static std::string g_strFilePathCert;
    helper_ssl_cert_and_key_holder();
    ~helper_ssl_cert_and_key_holder();

private:
    void auto_init();
    void auto_done();
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class helper_ssl_cert_and_key_provider {
public:
    static helper_ssl_cert_and_key_holder& helper_ssl_info();
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class helper_server : public helper_ssl_cert_and_key_provider {
    std::atomic_bool thread_is_running_ = false;

public:
    const std::string strScheme_;    // protocol name, lover case
    const std::string strSchemeUC_;  // protocol name, upper case
    const int nListenPort_;
    const std::string strBindAddressServer_;
    helper_server(
        const char* strScheme, int nListenPort, const std::string& strBindAddressServer );
    virtual ~helper_server();
    virtual bool isSSL() const = 0;
    virtual void stop() = 0;
    virtual void run() = 0;
    void run_parallel();
    void wait_parallel();
    static void stat_check_port_availability_to_start_listen(
        int ipVer, const char* strAddr, int nPort, const char* strScheme );
    void check_can_listen();
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class helper_server_ws_base;

class helper_ws_peer : public skutils::ws::peer {
public:
    std::string strPeerQueueID_;
    helper_ws_peer( skutils::ws::server& srv, const skutils::ws::hdl_t& hdl );
    ~helper_ws_peer() override;
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
    helper_server_ws_base& get_helper_server();
    const helper_server_ws_base& get_helper_server() const {
        return const_cast< helper_ws_peer* >( this )->get_helper_server();
    }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class helper_server_ws_base : public helper_server, public skutils::ws::server {
protected:
    std::atomic_bool ws_server_thread_is_running_ = false;
    std::atomic_bool bStopFlag_ = false;

public:
    helper_server_ws_base(
        const char* strScheme, int nListenPort, const std::string& strBindAddressServer );
    ~helper_server_ws_base() override;
    void stop() override;
    void run() override;
    void onLogMessage(
        skutils::ws::e_ws_log_message_type_t eWSLMT, const std::string& msg ) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class helper_server_ws : public helper_server_ws_base {
public:
    helper_server_ws( int nListenPort, const std::string& strBindAddressServer );
    ~helper_server_ws() override;
    bool isSSL() const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class helper_server_wss : public helper_server_ws_base {
public:
    helper_server_wss( int nListenPort, const std::string& strBindAddressServer );
    virtual ~helper_server_wss();
    bool isSSL() const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class helper_server_http_base : public helper_server {
    std::shared_ptr< skutils::http::server > pServer_;  // pointer to skutils::http::server or
                                                        // skutils::SSL_server
public:
    helper_server_http_base( const char* strScheme, int nListenPort,
        const std::string& strBindAddressServer, bool is_async_http_transfer_mode = true );
    virtual ~helper_server_http_base();
    void stop() override;
    void run() override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class helper_server_http : public helper_server_http_base {
public:
    helper_server_http( int nListenPort, const std::string& strBindAddressServer,
        bool is_async_http_transfer_mode = true );
    virtual ~helper_server_http();
    bool isSSL() const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class helper_server_https : public helper_server_http_base {
public:
    helper_server_https( int nListenPort, const std::string& strBindAddressServer,
        bool is_async_http_transfer_mode = true );
    ~helper_server_https() override;
    bool isSSL() const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class helper_client : public helper_ssl_cert_and_key_provider {
public:
    const std::string strClientName_;
    int nTargetPort_;
    const std::string strScheme_;
    const std::string strSchemeUC_;
    const std::string strBindAddressClient_;
    helper_client( const char* strClientName, int nTargetPort, const char* strScheme, const std::string& strBindAddressClient );
    virtual ~helper_client();
    virtual bool isSSL() const = 0;
    virtual void stop() = 0;
    virtual void run() = 0;
    virtual bool sendMessage( const char* msg ) = 0;  // text only
    virtual bool sendMessage( const nlohmann::json& joMsg ) = 0;
    virtual bool call( const nlohmann::json& joMsg, nlohmann::json& joAnswer ) = 0;
    nlohmann::json call( const nlohmann::json& joMsg );
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class helper_client_ws_base : public helper_client, public skutils::ws::client::client {
    std::atomic_bool bHaveAnswer_;
    std::string strLastMessage_;

public:
    volatile size_t cntClose_ = 0, cntFail_ = 0;
    int nLocalCloseCode_ = 0;
    std::string strLocalCloseCode_;
    std::string strCloseReason_;
    helper_client_ws_base( const char* strClientName, int nTargetPort, const char* strScheme_,
                           const std::string& strBindAddressClient, const size_t nConnectAttempts );
    virtual ~helper_client_ws_base();
    void stop() override;
    void run() override;
    void onLogMessage(
        skutils::ws::e_ws_log_message_type_t eWSLMT, const std::string& msg ) override;
    bool sendMessage(
        const std::string& msg, skutils::ws::opcv eOpCode = skutils::ws::opcv::text ) override;
    bool sendMessage( const char* msg ) override;  // text only
    bool sendMessage( const nlohmann::json& joMsg ) override;
    void onMessage( skutils::ws::hdl_t /*hdl*/, skutils::ws::opcv /*eOpCode*/,
        const std::string& msg ) override;
    std::string waitAnswer();
    void waitClose();
    void waitFail();
    void waitCloseOrFail();
    std::string exchange(
        const std::string& msg, skutils::ws::opcv eOpCode = skutils::ws::opcv::text );
    nlohmann::json exchange( const nlohmann::json& joMsg );
    bool call( const nlohmann::json& joMsg, nlohmann::json& joAnswer ) override;
    bool call( const nlohmann::json& joMsg );
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class helper_client_ws : public helper_client_ws_base {
public:
    helper_client_ws( const char* strClientName, int nTargetPort, const std::string& strBindAddressClient, const size_t nConnectAttempts );
    virtual ~helper_client_ws();
    bool isSSL() const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class helper_client_wss : public helper_client_ws_base {
public:
    helper_client_wss( const char* strClientName, int nTargetPort, const std::string& strBindAddressClient, const size_t nConnectAttempts );
    virtual ~helper_client_wss();
    bool isSSL() const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class helper_client_http_base : public helper_client {
public:
    std::shared_ptr< skutils::http::client > pClient_;  // skutils::http::client or
                                                        // skutils::SSL_client
    helper_client_http_base( const char* strClientName, int nTargetPort, const char* strScheme_,
                             const std::string& strBindAddressClient, const size_t nConnectAttempts );
    virtual ~helper_client_http_base();
    void stop() override;
    void run() override;
    bool sendMessage( const char* msg ) override;  // text only
    bool sendMessage( const nlohmann::json& joMsg ) override;
    bool call( const std::string& strMsg, std::string& strAnswer );
    bool call( const nlohmann::json& joMsg, nlohmann::json& joAnswer ) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class helper_client_http : public helper_client_http_base {
public:
    helper_client_http( const char* strClientName, int nTargetPort, const std::string& strBindAddressClient, const size_t nConnectAttempts );
    virtual ~helper_client_http();
    bool isSSL() const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class helper_client_https : public helper_client_http_base {
public:
    helper_client_https(
        const char* strClientName, int nTargetPort, const std::string& strBindAddressClient, const size_t nConnectAttempts );
    virtual ~helper_client_https();
    bool isSSL() const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::function< void( skutils::thread_pool& pool ) > fn_with_thread_pool_t;
extern void with_thread_pool( fn_with_thread_pool_t fn,
    size_t cntThreads = 0,  // 0 means used actual CPU thread count, i.e. result of
                            // skutils::tools::cpu_count()
    size_t nCallQueueLimit = 1024 * 100 );

typedef std::function< void() > fn_with_busy_tcp_port_worker_t;
typedef std::function< bool( const std::string& strErrorDescription ) >
    fn_with_busy_tcp_port_error_t;  // returns true if errror should be ignored
extern void with_busy_tcp_port( fn_with_busy_tcp_port_worker_t fnWorker,
                                fn_with_busy_tcp_port_error_t fnErrorHandler,
                                const std::string& strBindAddressServer,
                                const int nSocketListenPort, bool isIPv4 = true,
                                bool isIPv6 = true, bool is_reuse_address = true,
                                bool is_reuse_port = false );

typedef std::function< void( helper_server& refServer ) > fn_with_server_t;
extern void with_server( fn_with_server_t fn, const std::string& strServerUrlScheme,
    const std::string& strBindAddressServer, const int nSocketListenPort );

typedef std::function< void( helper_client& refClient ) > fn_with_client_t;
extern void with_client( fn_with_client_t fn, const std::string& strClientName,
    const std::string& strServerUrlScheme, const std::string& strBindAddressClient,
    const int nSocketListenPort,
    bool runClientInOtherThread = false,  // typically, this is never needed
    const size_t nConnectAttempts = 10 );
extern void with_clients( fn_with_client_t fn, const std::vector< std::string >& vecClientNames,
    const std::string& strServerUrlScheme, const std::string& strBindAddressClient,
    const int nSocketListenPort, const size_t nConnectAttempts = 10 );

typedef std::function< void( helper_server& refServer, helper_client& refClient ) >
    fn_with_client_server_t;
extern void with_client_server( fn_with_client_server_t fn, const std::string& strClientName,
    const std::string& strServerUrlScheme, const std::string& strBindAddressServer,
    const std::string& strBindAddressClient, const int nSocketListenPort,
    bool runClientInOtherThread = false, const size_t nConnectAttempts = 10 );

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern void helper_protocol_busy_port(
    const char* strProtocol, const char* strBindAddressServer, int nPort );

extern void helper_protocol_rest_call( const char* strProtocol, const char* strBindAddressServer,
    const char* strBindAddressClient, int nPort, bool isAutoExitOnSuccess );

extern void helper_protocol_echo_server( const char* strProtocol, const char* strBindAddressServer, int nPort );

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif  /// (!defined __MAIN_H)
