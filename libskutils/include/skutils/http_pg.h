#if ( !defined SKUTILS_HTTP_PG_H )
#define SKUTILS_HTTP_PG_H 1

#include <atomic>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#pragma GCC diagnostic ignored "-Waddress"
#pragma GCC diagnostic ignored "-Wnonnull-compare"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wattributes"

#include <proxygen/httpserver/RequestHandler.h>

#include <folly/io/async/EventBaseManager.h>
#include <proxygen/httpserver/HTTPServer.h>
#include <proxygen/httpserver/RequestHandlerFactory.h>

#pragma GCC diagnostic pop

#include <json.hpp>

#include <skutils/http.h>

namespace proxygen {
class ResponseHandler;
}

namespace skutils {
namespace http_pg {

class server_side_request_handler;


class request_sink {
    std::atomic_uint64_t m_reqCount{ 0 };

public:
    request_sink();
    virtual ~request_sink();
    virtual void OnRecordRequestCountIncrement();
    virtual uint64_t getRequestCount();
};


class request_site : public proxygen::RequestHandler {
    request_sink& m_sink;
    std::unique_ptr< folly::IOBuf > m_body;
    server_side_request_handler* m_SSRQ;
    static std::atomic_uint64_t g_instanceCounter;
    uint64_t m_instanceNumber;
    std::string m_strLogPrefix;
    size_t m_bodyPartNumber = 0;
    std::string m_strBody;

public:
    std::string m_httpMethod, m_origin, m_path, m_dstAddress_;

    int m_ipVer = -1, m_dstPort = 0;

    explicit request_site( request_sink& _aSink, server_side_request_handler* _SSRQ );

    ~request_site() override;

    void onRequest( std::unique_ptr< proxygen::HTTPMessage > _headers ) noexcept override;
    void onBody( std::unique_ptr< folly::IOBuf > _body ) noexcept override;
    void onEOM() noexcept override;
    void onUpgrade( proxygen::UpgradeProtocol _proto ) noexcept override;
    void requestComplete() noexcept override;
    void onError( proxygen::ProxygenError _err ) noexcept override;
};  /// class request_site



class request_site_factory : public proxygen::RequestHandlerFactory {
    folly::ThreadLocalPtr< request_sink > m_sink;
    server_side_request_handler* m_SSRQ = nullptr;

public:
    request_site_factory( server_side_request_handler* _SSRQ );
    ~request_site_factory() override;
    void onServerStart( folly::EventBase* /*evb*/ ) noexcept override;
    void onServerStop() noexcept override;
    proxygen::RequestHandler* onRequest(
        proxygen::RequestHandler*, proxygen::HTTPMessage* ) noexcept override;
};  /// class request_site_factory


class server_side_request_handler {
public:
    server_side_request_handler();
    virtual ~server_side_request_handler();
    static nlohmann::json json_from_error_text(
        const char* _errorDescription, const nlohmann::json& _joID );
    static std::string answer_from_error_text(
        const char* _errorDescription, const nlohmann::json& _joID );
    virtual skutils::result_of_http_request onRequest( const nlohmann::json& _joIn,
        const std::string& _origin, int _ipVer, const std::string& _dstAddress,
        int _dstPort ) = 0;
};  /// class server_side_request_handler


class server : public server_side_request_handler {
    std::thread m_thread;
    std::unique_ptr< proxygen::HTTPServer > m_server;
    pg_on_request_handler_t m_h;
    pg_accumulate_entries m_entries;
    int32_t m_threads = 0;
    int32_t m_threads_limit = 0;

    std::string m_logPrefix;

public:
    server( pg_on_request_handler_t _h, const pg_accumulate_entries& _entries, int32_t _threads = 0,
        int32_t _threadsLimit = 0 );
    ~server() override;
    bool start();
    void stop();
    skutils::result_of_http_request onRequest( const nlohmann::json& _joIn,
        const std::string& _origin, int _ipVer, const std::string& _dstAddress,
        int _dstPort ) override;
};  /// class server


};  // namespace http_pg
};  // namespace skutils

#endif  /// SKUTILS_HTTP_PG_H
