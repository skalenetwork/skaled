#include <skutils/rest_call.h>
#include <skutils/utils.h>

#include <cstdlib>

namespace skutils {
namespace rest {

client::client() {}

client::client( const skutils::url& u ) {
    open( u );
}

client::client( const std::string& url_str ) {
    open( url_str );
}

client::client( const char* url_str ) {
    open( std::string( url_str ? url_str : "" ) );
}

client::~client() {
    close();
}

bool client::open( const skutils::url& u ) {
    try {
        std::string strScheme = skutils::tools::to_lower( skutils::tools::trim_copy( u.scheme() ) );
        if ( strScheme.empty() )
            return false;
        //
        std::string strHost = skutils::tools::to_lower( skutils::tools::trim_copy( u.host() ) );
        if ( strHost.empty() )
            return false;
        //
        std::string strPort = skutils::tools::to_lower( skutils::tools::trim_copy( u.port() ) );
        if ( strPort.empty() )
            strPort = "80";
        int nPort = std::atoi( strPort.c_str() );
        //
        if ( strScheme == "http" ) {
            close();
            ch_.reset( new skutils::http::client( strHost.c_str(), nPort ) );
        } else if ( strScheme == "https" ) {
            close();
            ch_.reset( new skutils::http::SSL_client( strHost.c_str(), nPort ) );
        } else if ( strScheme == "ws" || strScheme == "wss" ) {
            cw_.reset( new skutils::ws::client );
            if ( !cw_->open( u.str() ) ) {
                close();
                return false;
            }
        } else
            return false;
        return true;
    } catch ( ... ) {
    }
    close();
    return false;
}

bool client::open( const std::string& url_str ) {
    skutils::url u( url_str );
    return open( u );
}

bool client::open( const char* url_str ) {
    return open( std::string( url_str ? url_str : "" ) );
}

void client::close() {
    try {
        if ( ch_ ) {
            ch_.reset();
        }
        if ( cw_ ) {
            cw_->close();
            cw_.reset();
        }
    } catch ( ... ) {
    }
}

std::string client::get_connected_url_scheme() const {
    if ( ch_ ) {
        if ( ch_->is_valid() ) {
            if ( ch_->is_ssl() )
                return "https";
            return "http";
        }
    } else if ( cw_ ) {
        if ( cw_->isConnected() ) {
            if ( cw_->is_ssl() )
                return "wss";
            return "ws";
        }
    }
    return "";  // not connected
}
bool client::is_open() const {
    std::string s = get_connected_url_scheme();
    if ( s.empty() )
        return false;
    return true;
}

bool client::call( const std::string& strJsonIn, std::string& strOut ) {
    strOut.clear();
    if ( ch_ ) {
        if ( ch_->is_valid() ) {
            std::shared_ptr< skutils::http::response > resp =
                ch_->Post( "/", strJsonIn, "application/json" );
            if ( !resp )
                return false;
            if ( resp->status_ != 200 )
                return false;
            strOut = resp->body_;
            return true;
        }
    } else if ( cw_ ) {
        if ( cw_->isConnected() ) {
        }
    }
    return false;
}

};  // namespace rest
};  // namespace skutils
