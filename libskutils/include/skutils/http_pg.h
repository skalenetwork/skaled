#if ( !defined SKUTILS_HTTP_PG_H )
#define SKUTILS_HTTP_PG_H 1

#include <atomic>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#pragma GCC diagnostic ignored "-Waddress"
#pragma GCC diagnostic ignored "-Wnonnull-compare"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wattributes"

#include <folly/Memory.h>
#include <proxygen/httpserver/RequestHandler.h>

#include <folly/io/async/EventBaseManager.h>
//#include <folly/portability/GFlags.h>
#include <folly/portability/Unistd.h>
#include <proxygen/httpserver/HTTPServer.h>
#include <proxygen/httpserver/RequestHandlerFactory.h>

#pragma GCC diagnostic pop

//#include <nlohmann/json.hpp>
#include <json.hpp>

#include <skutils/http.h>

namespace proxygen {
class ResponseHandler;
}

namespace skutils {
namespace http_pg {

class server_side_request_handler;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class request_sink {
    std::atomic_uint64_t reqCount_{ 0 };

public:
    request_sink();
    virtual ~request_sink();
    virtual void OnRecordRequestCountIncrement();
    virtual uint64_t getRequestCount();
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class request_site : public proxygen::RequestHandler {
    request_sink& sink_;
    std::unique_ptr< folly::IOBuf > body_;
    server_side_request_handler* pSSRQ_;
    static std::atomic_uint64_t g_instance_counter;
    uint64_t nInstanceNumber_;
    std::string strLogPrefix_;
    size_t nBodyPartNumber_ = 0;
    std::string strBody_;

public:
    std::string strHttpMethod_, strOrigin_, strPath_, strDstAddress_;
    int ipVer_ = -1, nDstPort_ = 0;

    explicit request_site( request_sink& a_sink, server_side_request_handler* pSSRQ );
    ~request_site() override;

    void onRequest( std::unique_ptr< proxygen::HTTPMessage > headers ) noexcept override;
    void onBody( std::unique_ptr< folly::IOBuf > body ) noexcept override;
    void onEOM() noexcept override;
    void onUpgrade( proxygen::UpgradeProtocol proto ) noexcept override;
    void requestComplete() noexcept override;
    void onError( proxygen::ProxygenError err ) noexcept override;
};  /// class request_site


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class request_site_factory : public proxygen::RequestHandlerFactory {
    folly::ThreadLocalPtr< request_sink > sink_;
    server_side_request_handler* pSSRQ_ = nullptr;

public:
    request_site_factory( server_side_request_handler* pSSRQ );
    ~request_site_factory() override;
    void onServerStart( folly::EventBase* /*evb*/ ) noexcept override;
    void onServerStop() noexcept override;
    proxygen::RequestHandler* onRequest(
        proxygen::RequestHandler*, proxygen::HTTPMessage* ) noexcept override;
};  /// class request_site_factory

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class server_side_request_handler {
public:
    server_side_request_handler();
    virtual ~server_side_request_handler();
    static nlohmann::json json_from_error_text(
        const char* strErrorDescription, const nlohmann::json& joID );
    static std::string answer_from_error_text(
        const char* strErrorDescription, const nlohmann::json& joID );
    virtual skutils::result_of_http_request onRequest( const nlohmann::json& joIn,
        const std::string& strOrigin, int ipVer, const std::string& strDstAddress,
        int nDstPort ) = 0;
};  /// class server_side_request_handler

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class server : public server_side_request_handler {
    std::thread thread_;
    std::unique_ptr< proxygen::HTTPServer > server_;
    pg_on_request_handler_t h_;
    pg_accumulate_entries entries_;
    int32_t threads_ = 0;
    int32_t threads_limit_ = 0;

    std::string strLogPrefix_;

public:
    server( pg_on_request_handler_t h, const pg_accumulate_entries& entries, int32_t threads = 0,
        int32_t threads_limit = 0 );
    ~server() override;
    bool start();
    void stop();
    skutils::result_of_http_request onRequest( const nlohmann::json& joIn,
        const std::string& strOrigin, int ipVer, const std::string& strDstAddress,
        int nDstPort ) override;
};  /// class server

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace http_pg
};  // namespace skutils

#endif  /// SKUTILS_HTTP_PG_H
