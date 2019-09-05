#if ( !defined __SKUTILS_REST_CALLS_H )
#define __SKUTILS_REST_CALLS_H 1

#include <memory>
#include <string>

#include <skutils/url.h>

#include <skutils/http.h>
#include <skutils/ws.h>

namespace skutils {
namespace rest {

class client {
    skutils::url u_;
    std::unique_ptr< skutils::http::client > ch_;
    std::unique_ptr< skutils::ws::client > cw_;

public:
    client();
    client( const skutils::url& u );
    client( const std::string& url_str );
    client( const char* url_str );
    virtual ~client();

    const skutils::url& url() const { return u_; }

    bool open( const skutils::url& u );
    bool open( const std::string& url_str );
    bool open( const char* url_str );
    void close();
    std::string get_connected_url_scheme() const;
    bool is_open() const;

    bool call( const std::string& strJsonIn, std::string& strOut );

};  /// class client

};  // namespace rest
};  // namespace skutils

#endif  /// (!defined __SKUTILS_REST_CALLS_H)
