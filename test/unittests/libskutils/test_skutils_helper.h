#if ( !defined __TEST_SKUTILS_HELPER_H )
#define __TEST_SKUTILS_HELPER_H 1

#include <skutils/async_work.h>
#include <skutils/atomic_shared_ptr.h>
#include <skutils/console_colors.h>
#include <skutils/dispatch.h>
#include <skutils/http.h>
#include <skutils/mail.h>
#include <skutils/multifunction.h>
#include <skutils/multithreading.h>
#include <skutils/network.h>
#include <skutils/stats.h>
#include <skutils/thread_pool.h>
#include <skutils/url.h>
#include <skutils/utils.h>
#include <skutils/ws.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace skutils {
namespace test {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern void test_log_output( const std::string& strMessage );
extern std::string test_log_prefix_reformat(
    bool isInsertTime, const std::string& strPrefix, const std::string& strMessage );
extern std::string test_log_caption(
    const char* strPrefix = nullptr, const char* strSuffix = nullptr );
extern std::string test_log_caption(
    const std::string& strPrefix, const char* strSuffix = nullptr );

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern void test_log_e( const std::string& strMessage );
extern void test_log_ew( const std::string& strMessage );
extern void test_log_ee( const std::string& strMessage );
extern void test_log_ef( const std::string& strMessage );
extern void test_log_s( const std::string& strMessage );
extern void test_log_ss( const std::string& strMessage );
extern void test_log_sw( const std::string& strMessage );
extern void test_log_se( const std::string& strMessage );
extern void test_log_sf( const std::string& strMessage );
extern void test_log_c( const std::string& strMessage );
extern void test_log_c( const std::string& strClient, const std::string& strMessage );
extern void test_log_cs( const std::string& strMessage );
extern void test_log_cs( const std::string& strClient, const std::string& strMessage );
extern void test_log_cw( const std::string& strMessage );
extern void test_log_cw( const std::string& strClient, const std::string& strMessage );
extern void test_log_ce( const std::string& strMessage );
extern void test_log_ce( const std::string& strClient, const std::string& strMessage );
extern void test_log_cf( const std::string& strMessage );
extern void test_log_cf( const std::string& strClient, const std::string& strMessage );
extern void test_log_p( const std::string& strMessage );
extern void test_log_p( const std::string& strClient, const std::string& strMessage );
extern void test_log_ps( const std::string& strMessage );
extern void test_log_ps( const std::string& strClient, const std::string& strMessage );
extern void test_log_pw( const std::string& strMessage );
extern void test_log_pw( const std::string& strClient, const std::string& strMessage );
extern void test_log_pe( const std::string& strMessage );
extern void test_log_pe( const std::string& strClient, const std::string& strMessage );
extern void test_log_pf( const std::string& strMessage );
extern void test_log_pf( const std::string& strClient, const std::string& strMessage );

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern std::string generate_new_call_id( const char* strPrefix = nullptr );
extern std::string generate_new_call_id( const std::string& strPrefix );
extern void ensure_call_id_present( nlohmann::json& joMsg, const char* strCustomPrefix = nullptr );
extern nlohmann::json ensure_call_id_present_copy(
    const nlohmann::json& joMsg, const char* strCustomPrefix = nullptr );

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class test_ssl_cert_and_key_holder {
public:
    std::string strFilePathKey_;
    std::string strFilePathCert_;
    test_ssl_cert_and_key_holder();
    ~test_ssl_cert_and_key_holder();
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class test_ssl_cert_and_key_provider {
public:
    static test_ssl_cert_and_key_holder& helper_ssl_info();
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class test_server : public test_ssl_cert_and_key_provider {
    std::atomic_bool thread_is_running_ = false;

public:
    const std::string strScheme_;    // protocol name, lover case
    const std::string strSchemeUC_;  // protocol name, upper case
    const int nListenPort_;
    test_server( const char* strScheme, int nListenPort );
    virtual ~test_server();
    virtual bool isSSL() const = 0;
    virtual void stop() = 0;
    virtual void run() = 0;
    void run_parallel();
    void wait_parallel();
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class test_server_ws_base;

class test_ws_peer : public skutils::ws::peer {
public:
    std::string strPeerQueueID_;
    test_ws_peer( skutils::ws::server& srv, const skutils::ws::hdl_t& hdl );
    ~test_ws_peer() override;
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
    test_server_ws_base& get_test_server();
    const test_server_ws_base& get_test_server() const {
        return const_cast< test_ws_peer* >( this )->get_test_server();
    }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class test_server_ws_base : public test_server, public skutils::ws::server {
protected:
    std::atomic_bool ws_server_thread_is_running_ = false;
    std::atomic_bool bStopFlag_ = false;

public:
    test_server_ws_base( const char* strScheme, int nListenPort );
    ~test_server_ws_base() override;
    void stop() override;
    void run() override;
    void onLogMessage(
        skutils::ws::e_ws_log_message_type_t eWSLMT, const std::string& msg ) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class test_server_ws : public test_server_ws_base {
public:
    test_server_ws( int nListenPort );
    ~test_server_ws() override;
    bool isSSL() const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class test_server_wss : public test_server_ws_base {
public:
    test_server_wss( int nListenPort );
    virtual ~test_server_wss();
    bool isSSL() const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class test_server_http_base : public test_server {
    std::shared_ptr< skutils::http::server > pServer_;  // pointer to skutils::http::server or
                                                        // skutils::SSL_server
public:
    test_server_http_base( const char* strScheme, int nListenPort );
    virtual ~test_server_http_base();
    void stop() override;
    void run() override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class test_server_http : public test_server_http_base {
public:
    test_server_http( int nListenPort );
    virtual ~test_server_http();
    bool isSSL() const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class test_server_https : public test_server_http_base {
public:
    test_server_https( int nListenPort );
    ~test_server_https() override;
    bool isSSL() const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class test_client : public test_ssl_cert_and_key_provider {
public:
    const std::string strClientName_;
    int nTargetPort_;
    const std::string strScheme_;
    const std::string strSchemeUC_;
    test_client( const char* strClientName, int nTargetPort, const char* strScheme );
    virtual ~test_client();
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

class test_client_ws_base : public test_client, public skutils::ws::client::client {
    std::atomic_bool bHaveAnswer_;
    std::string strLastMessage_;

public:
    volatile size_t cntClose_ = 0, cntFail_ = 0;
    int nLocalCloseCode_ = 0;
    std::string strLocalCloseCode_;
    std::string strCloseReason_;
    test_client_ws_base( const char* strClientName, int nTargetPort, const char* strScheme_,
        const size_t nConnectAttempts );
    virtual ~test_client_ws_base();
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

class test_client_ws : public test_client_ws_base {
public:
    test_client_ws( const char* strClientName, int nTargetPort, const size_t nConnectAttempts );
    virtual ~test_client_ws();
    bool isSSL() const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class test_client_wss : public test_client_ws_base {
public:
    test_client_wss( const char* strClientName, int nTargetPort, const size_t nConnectAttempts );
    virtual ~test_client_wss();
    bool isSSL() const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class test_client_http_base : public test_client {
public:
    std::shared_ptr< skutils::http::client > pClient_;  // skutils::http::client or
                                                        // skutils::SSL_client
    test_client_http_base( const char* strClientName, int nTargetPort, const char* strScheme_,
        const size_t nConnectAttempts );
    virtual ~test_client_http_base();
    void stop() override;
    void run() override;
    bool sendMessage( const char* msg ) override;  // text only
    bool sendMessage( const nlohmann::json& joMsg ) override;
    bool call( const std::string& strMsg, std::string& strAnswer );
    bool call( const nlohmann::json& joMsg, nlohmann::json& joAnswer ) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class test_client_http : public test_client_http_base {
public:
    test_client_http( const char* strClientName, int nTargetPort, const size_t nConnectAttempts );
    virtual ~test_client_http();
    bool isSSL() const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class test_client_https : public test_client_http_base {
public:
    test_client_https( const char* strClientName, int nTargetPort, const size_t nConnectAttempts );
    virtual ~test_client_https();
    bool isSSL() const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::function< void() > fn_with_test_environment_t;
extern void with_test_environment( fn_with_test_environment_t fn );

typedef std::function< void( skutils::thread_pool& pool ) > fn_with_thread_pool_t;
extern void with_thread_pool( fn_with_thread_pool_t fn,
    size_t cntThreads = 0,  // 0 means used actual CPU thread count, i.e. result of
                            // skutils::tools::cpu_count()
    size_t nCallQueueLimit = 1024 * 100 );

typedef std::function< void( test_server& refServer ) > fn_with_test_server_t;
extern void with_test_server(
    fn_with_test_server_t fn, const std::string& strServerUrlScheme, const int nSocketListenPort );

typedef std::function< void( test_client& refClient ) > fn_with_test_client_t;
extern void with_test_client( fn_with_test_client_t fn, const std::string& strTestClientName,
    const std::string& strServerUrlScheme, const int nSocketListenPort,
    bool runClientInOtherThread = false,  // typically, this is never needed
    const size_t nConnectAttempts = 10 );
extern void with_test_clients( fn_with_test_client_t fn,
    const std::vector< std::string >& vecTestClientNames, const std::string& strServerUrlScheme,
    const int nSocketListenPort, const size_t nConnectAttempts = 10 );

typedef std::function< void( test_server& refServer, test_client& refClient ) >
    fn_with_test_client_server_t;
extern void with_test_client_server( fn_with_test_client_server_t fn,
    const std::string& strTestClientName, const std::string& strServerUrlScheme,
    const int nSocketListenPort, bool runClientInOtherThread = false,
    const size_t nConnectAttempts = 10 );

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern void test_print_header_name( const char* s );

extern int g_nDefaultPort;

extern std::vector< std::string > g_vecTestClientNamesA;
extern std::vector< std::string > g_vecTestClientNamesB;

extern void test_protocol_server_startup( const char* strProto, int nPort );
extern void test_protocol_single_call( const char* strProto, int nPort );
extern void test_protocol_serial_calls(
    const char* strProto, int nPort, const std::vector< std::string >& vecClientNames );
extern void test_protocol_parallel_calls(
    const char* strProto, int nPort, const std::vector< std::string >& vecClientNames );

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace test
};  // namespace skutils

#endif  /// (!defined __TEST_SKUTILS_HELPER_H)
