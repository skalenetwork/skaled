#include "sys/random.h"
#include <sys/syscall.h>
#include <sys/types.h>
#include <fstream>
#include <regex>
#include <streambuf>

#include <skutils/rest_call.h>
#include <skutils/utils.h>

#include <stdlib.h>

#include <cstdlib>
#include <thread>

#include <skutils/utils.h>

#include <assert.h>

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

namespace skutils {
namespace rest {

data_t::data_t() {}
data_t::data_t( const data_t& d ) {
    assign( d );
}
data_t::~data_t() {
    clear();
}
data_t& data_t::operator=( const data_t& d ) {
    assign( d );
    return ( *this );
}

bool data_t::is_json() const {
    return ( strcasecmp( content_type_.c_str(), "application/json" ) == 0 ||
               strcasecmp( content_type_.c_str(), "text/json" ) == 0 ) ?
               true :
               false;
}
bool data_t::is_binary() const {
    return ( strcasecmp( content_type_.c_str(), "application/octet-stream" ) == 0 ) ? true : false;
}

bool data_t::empty() const {
    return content_type_.empty();
}
void data_t::clear() {
    content_type_.clear();
    s_.clear();
}
void data_t::assign( const data_t& d ) {
    s_ = d.s_;
    err_s_ = d.err_s_;
    content_type_ = d.content_type_;
    ei_ = d.ei_;
}

nlohmann::json data_t::extract_json() const {
    nlohmann::json jo = nlohmann::json::object();
    try {
        if ( !is_json() )
            return jo;
        jo = nlohmann::json::parse( s_ );
    } catch ( ... ) {
    }
    return jo;
}

std::string data_t::extract_json_id() const {
    std::string id;
    try {
        nlohmann::json jo = extract_json();
        if ( jo.count( "id" ) > 0 ) {
            nlohmann::json wid = jo[id];
            if ( wid.is_string() ) {
                id = wid.get< std::string >();
            } else if ( wid.is_number_unsigned() ) {
                unsigned n = wid.get< unsigned >();
                id = std::to_string( n );
            } else if ( wid.is_number() ) {
                int n = wid.get< int >();
                id = std::to_string( n );
            }
        }
    } catch ( ... ) {
    }
    return id;
}

sz_cli::sz_cli() {
    close();
}

sz_cli::sz_cli( const skutils::url& u, const skutils::http::SSL_client_options& optsSSL )
    : u_( u ), optsSSL_( optsSSL ) {
    if ( is_sign() ) {
        try {
            cert_ = stat_f2s( optsSSL.client_cert );
            if ( cert_.empty() )
                throw std::runtime_error( "loaded data is empty" );
        } catch ( std::exception& e ) {
            throw std::runtime_error( "sz_cli: could not read client certificate file \"" +
                                      optsSSL.client_cert + "\", exception is: " + e.what() );
        }
        try {
            key_ = stat_f2s( optsSSL.client_key );
            if ( key_.empty() )
                throw std::runtime_error( "loaded data is empty" );
        } catch ( std::exception& e ) {
            throw std::runtime_error( "sz_cli: could not client key read file \"" +
                                      optsSSL.client_key + "\", exception is: " + e.what() );
        }
        BIO* bo = BIO_new( BIO_s_mem() );
        if ( !bo )
            throw std::runtime_error( "sz_cli: BIO_new() failed(1)" );
        BIO_write( bo, key_.c_str(), key_.size() );
        PEM_read_bio_PrivateKey( bo, &pKeyPrivate_, 0, 0 );
        BIO_free( bo );
        if ( !pKeyPrivate_ )
            throw std::runtime_error( "sz_cli: key creation failed" );
        std::tie( pKeyPublic_, x509Cert_ ) = stat_cert_2_public_key( cert_ );
    }
    u2_ = u_;
    u2_.scheme( "tcp" );
}

sz_cli::~sz_cli() {
    close();
}


bool sz_cli::isConnected() const {
    return isConnected_;
}

void sz_cli::close() {
    std::lock_guard< std::recursive_mutex > lock( mtx_ );
    if ( !isConnected() )
        return;
    zmq_ctx_.shutdown();
    pClientSocket_->close();
    pClientSocket_.reset();
    isConnected_ = false;
}

void sz_cli::reconnect() {
    close();
    std::lock_guard< std::recursive_mutex > lock( mtx_ );
    uint64_t randNumber1 = rand(), randNumber2 = rand(), randNumber3 = rand(), randNumber4 = rand();
    std::string identity = std::to_string( randNumber3 ) + ":" + std::to_string( randNumber4 ) +
                           ":" + std::to_string( randNumber1 ) + ":" +
                           std::to_string( randNumber2 );
    pClientSocket_ = std::make_shared< zmq::socket_t >( zmq_ctx_, ZMQ_DEALER );
    pClientSocket_->setsockopt( ZMQ_IDENTITY, identity.c_str(), identity.size() + 1 );
    // configure socket to not wait at close time
    static const int ZMQ_TIMEOUT = 1000;
    int timeout = ZMQ_TIMEOUT;
    pClientSocket_->setsockopt( ZMQ_SNDTIMEO, &timeout, sizeof( int ) );
    pClientSocket_->setsockopt( ZMQ_RCVTIMEO, &timeout, sizeof( int ) );
    int linger = 0;
    pClientSocket_->setsockopt( ZMQ_LINGER, &linger, sizeof( linger ) );
    int val = 15000;
    pClientSocket_->setsockopt( ZMQ_HEARTBEAT_IVL, &val, sizeof( val ) );
    val = 3000;
    pClientSocket_->setsockopt( ZMQ_HEARTBEAT_TIMEOUT, &val, sizeof( val ) );
    val = 60000;
    pClientSocket_->setsockopt( ZMQ_HEARTBEAT_TTL, &val, sizeof( val ) );
    pClientSocket_->connect( u2_.str() );
    isConnected_ = true;
}

std::string sz_cli::stat_f2s( const std::string& strFileName ) {
    std::ifstream ifs( strFileName );
    ifs.exceptions( ifs.failbit | ifs.badbit | ifs.eofbit );
    std::string s = std::string(
        ( std::istreambuf_iterator< char >( ifs ) ), std::istreambuf_iterator< char >() );
    return s;
}

std::pair< EVP_PKEY*, X509* > sz_cli::stat_cert_2_public_key( const std::string& strCertificate ) {
    if ( strCertificate.empty() )
        throw std::runtime_error( "sz_cli: certicicate must not be empty to read public key" );
    BIO* bo = BIO_new( BIO_s_mem() );
    if ( !bo )
        throw std::runtime_error( "sz_cli: BIO_new() failed(2)" );
    if ( !( BIO_write( bo, strCertificate.c_str(), strCertificate.size() ) > 0 ) )
        throw std::runtime_error( "sz_cli: BIO_write() failed(2)" );
    X509* cert = PEM_read_bio_X509( bo, nullptr, 0, 0 );
    if ( !cert )
        throw std::runtime_error( "sz_cli: PEM_read_bio_X509() failed(2)" );
    auto key = X509_get_pubkey( cert );
    BIO_free( bo );
    if ( !key )
        throw std::runtime_error( "sz_cli: X509_get_pubkey() failed(2)" );
    return {key, cert};
}

static void stat_append_msgSig( std::string& src, const std::string& sig ) {
    assert( src.length() > 2 );
    assert( src[src.length() - 1] == '}' );
    std::string w = ",\"msgSig\":\"";
    w += sig;
    w += "\"";
    src.insert( src.length() - 1, w );
}

static void stat_params_trick( nlohmann::json& jo ) {
    if ( jo.count( "params" ) > 0 ) {
        nlohmann::json& joParams = jo["params"];
        for ( auto& parm : joParams.items() )
            jo[parm.key()] = parm.value();
    }
}

nlohmann::json sz_cli::stat_sendMessage( nlohmann::json& joRequest, bool bExceptionOnTimeout,
    size_t cntAttempts, size_t timeoutMilliseconds ) {
    if ( is_sign() )
        joRequest["cert"] = cert_;  // skutils::tools::replace_all_copy( cert_, "\n", "" );
    stat_params_trick( joRequest );
    std::string reqStr = joRequest.dump();
    std::string strOrig = reqStr;
    if ( is_sign() ) {
        auto sig = stat_sign( pKeyPrivate_, reqStr );
        // joRequest["msgSig"] = sig;
        stat_append_msgSig( reqStr, sig );
    }
    // reqStr = joRequest.dump();
    auto resultStr =
        stat_sendMessageZMQ( reqStr, bExceptionOnTimeout, cntAttempts, timeoutMilliseconds );
    try {
        nlohmann::json joAnswer = nlohmann::json::parse( resultStr );
        return joAnswer;
    } catch ( std::exception& e ) {
        throw std::runtime_error(
            std::string( "sz_cli: ZMQ message sending failed, exception is: " ) + e.what() );
    } catch ( ... ) {
        throw std::runtime_error( "sz_cli: ZMQ message sending failed, unknown exception" );
    }
}  // namespace rest

std::string sz_cli::stat_sendMessageZMQ( std::string& jvRequest, bool bExceptionOnTimeout,
    size_t cntAttempts, size_t timeoutMilliseconds ) {
    reconnect();
    size_t idxAttempt = 0;
    std::stringstream request;
    s_send( *pClientSocket_, jvRequest );
    ++idxAttempt;
    while ( true ) {
        zmq::pollitem_t items[] = {{static_cast< void* >( *pClientSocket_ ), 0, ZMQ_POLLIN, 0}};
        zmq::poll( &items[0], 1, timeoutMilliseconds );
        if ( items[0].revents & ZMQ_POLLIN ) {
            std::string reply = s_recv( *pClientSocket_ );
            return reply;
        } else {
            if ( bExceptionOnTimeout && ( idxAttempt >= cntAttempts ) )
                throw std::runtime_error( "sz_cli: no response from sgx server" );
            reconnect();
            s_send( *pClientSocket_, jvRequest );  // send again
            ++idxAttempt;
        }
    }
}

std::string sz_cli::stat_sign( EVP_PKEY* pKey, const std::string& s ) {
    assert( pKey );
    assert( !s.empty() );
    static std::regex r( "\\s+" );
    auto msgToSign = std::regex_replace( s, r, "" );
    EVP_MD_CTX* mdctx = NULL;
    unsigned char* signature = NULL;
    size_t slen = 0;
    mdctx = EVP_MD_CTX_create();
    assert( mdctx );
    auto rv1 = EVP_DigestSignInit( mdctx, NULL, EVP_sha256(), NULL, pKey );
    if ( rv1 != 1 )
        throw std::runtime_error( "sz_cli::stat_sign() failure 1" );
    auto rv2 = EVP_DigestSignUpdate( mdctx, msgToSign.c_str(), msgToSign.size() );
    if ( rv2 != 1 )
        throw std::runtime_error( "sz_cli::stat_sign() failure 2" );
    auto rv3 = EVP_DigestSignFinal( mdctx, NULL, &slen );
    if ( rv3 != 1 )
        throw std::runtime_error( "sz_cli::stat_sign() failure 3" );
    signature = ( unsigned char* ) OPENSSL_malloc( sizeof( unsigned char ) * slen );
    if ( !signature )
        throw std::runtime_error( "sz_cli::stat_sign() failure 5" );
    auto rv4 = EVP_DigestSignFinal( mdctx, signature, &slen );
    if ( rv4 != 1 )
        throw std::runtime_error( "sz_cli::stat_sign() failure 4" );
    auto hexSig = stat_a2h( signature, slen );
    std::string hexStringSig( hexSig.begin(), hexSig.end() );
    // cleanup
    if ( signature )
        OPENSSL_free( signature );
    if ( mdctx )
        EVP_MD_CTX_destroy( mdctx );
    return hexStringSig;
}

std::string sz_cli::stat_a2h( const uint8_t* ptr, size_t cnt ) {
    char hex[2 * cnt];
    static char hexval[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    for ( size_t i = 0; i < cnt; ++i ) {
        hex[i * 2] = hexval[( ( ptr[i] >> 4 ) & 0xF )];
        hex[( i * 2 ) + 1] = hexval[( ptr[i] ) & 0x0F];
    }
    std::string result( ( char* ) hex, 2 * cnt );
    return result;
}

bool sz_cli::is_sign() const {
    if ( optsSSL_.client_cert.empty() )
        return false;
    if ( optsSSL_.client_key.empty() )
        return false;
    return true;
}

bool sz_cli::is_ssl() const {
    return false;
}

bool sz_cli::sendMessage( const std::string& strMessage, std::string& strAnswer ) {
    // if ( !isConnected() ) {
    //     reconnect();
    //     if ( !isConnected() )
    //         return false;
    // }
    nlohmann::json joMessage = nlohmann::json::parse( strMessage );
    nlohmann::json joAnswer =
        stat_sendMessage( joMessage, true, cntAttemptsToSendMessage_, timeoutMilliseconds_ );
    strAnswer = joAnswer.dump();
    return true;
}


client::client() {
    async_remove_all();
}

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

std::string client::u_path() const {
    std::string s = u_.path();
    if ( s.empty() )
        s = "/";
    return s;
}
std::string client::u_path_and_args() const {
    std::string s = u_path() + u_.str_query();
    return s;
}

bool client::open( const skutils::url& u, std::chrono::milliseconds wait_step, size_t cntSteps ) {
    try {
        u_ = u;
        std::string strScheme = skutils::tools::to_lower( skutils::tools::trim_copy( u.scheme() ) );
        if ( strScheme.empty() )
            return false;
        //
        std::string strHost = skutils::tools::to_lower( skutils::tools::trim_copy( u.host() ) );
        if ( strHost.empty() )
            return false;
        //
        std::string strPort = skutils::tools::to_lower( skutils::tools::trim_copy( u.port() ) );
        if ( strPort.empty() ) {
            if ( strScheme == "https" || strScheme == "wss" )
                strPort = "443";
            else
                strPort = "80";
        }
        int nPort = std::atoi( strPort.c_str() );
        //
        if ( strScheme == "http" ) {
            close();
#if (defined __SKUTIS_REST_USE_CURL_FOR_HTTP)
            ch_.reset( new skutils::http_curl::client( u, __SKUTILS_HTTP_CLIENT_CONNECT_TIMEOUT_MILLISECONDS__, &optsSSL_ ) );
            ch_->isVerboseInsideCURL_ = isVerboseInsideNetworkLayer_;
#else  // (defined __SKUTIS_REST_USE_CURL_FOR_HTTP)
            ch_.reset( new skutils::http::client( -1, strHost.c_str(), nPort, __SKUTILS_HTTP_CLIENT_CONNECT_TIMEOUT_MILLISECONDS__, nullptr ) );
#endif // else from (defined __SKUTIS_REST_USE_CURL_FOR_HTTP)
        } else if ( strScheme == "https" ) {
            close();
#if (defined __SKUTIS_REST_USE_CURL_FOR_HTTP)
            ch_.reset( new skutils::http_curl::client( u, __SKUTILS_HTTP_CLIENT_CONNECT_TIMEOUT_MILLISECONDS__, &optsSSL_ ) );
            ch_->isVerboseInsideCURL_ = isVerboseInsideNetworkLayer_;
#else  // (defined __SKUTIS_REST_USE_CURL_FOR_HTTP)
            ch_.reset( new skutils::http::SSL_client( -1, strHost.c_str(), nPort,
                __SKUTILS_HTTP_CLIENT_CONNECT_TIMEOUT_MILLISECONDS__, &optsSSL_ ) );
#endif // else from (defined __SKUTIS_REST_USE_CURL_FOR_HTTP)
        } else if ( strScheme == "ws" || strScheme == "wss" ) {
            close();
            cw_.reset( new skutils::ws::client );
            cw_->onMessage_ = [&]( skutils::ws::basic_participant&, skutils::ws::hdl_t,
                                  skutils::ws::opcv eOpCode, const std::string& s ) -> void {
                data_t d;
                d.s_ = s;
                d.content_type_ = ( eOpCode == skutils::ws::opcv::binary ) ?
                                      "application/octet-stream" :
                                      "application/json" /*g_str_default_content_type*/;
                handle_data_arrived( d );
            };
            if ( !cw_->open( u.str() ) ) {
                close();
                return false;
            }
            for ( size_t i = 0; ( !cw_->isConnected() ) && i < cntSteps; ++i ) {
                std::this_thread::sleep_for( wait_step );
            }
            if ( !cw_->isConnected() ) {
                close();
                return false;
            }
        } else if ( strScheme == "zmq" || strScheme == "zmqs" || strScheme == "z" ||
                    strScheme == "zs" ) {
            close();
            cz_.reset( new sz_cli( u, optsSSL_ ) );
        } else
            return false;
        return true;
    } catch ( ... ) {
    }
    close();
    return false;
}

bool client::open(
    const std::string& url_str, std::chrono::milliseconds wait_step, size_t cntSteps ) {
    skutils::url u( url_str );
    return open( u, wait_step, cntSteps );
}

bool client::open( const char* url_str, std::chrono::milliseconds wait_step, size_t cntSteps ) {
    return open( std::string( url_str ? url_str : "" ), wait_step, cntSteps );
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
        if ( cz_ ) {
            cz_->close();
            cz_.reset();
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
    } else if ( cz_ ) {
        if ( cz_->isConnected() ) {
            if ( cz_->is_ssl() )
                return "zmqs";
            return "zmq";
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

bool client::handle_data_arrived( const data_t& d ) {
    if ( !d.err_s_.empty() )
        return false;
    if ( d.empty() )
        return false;
    await_t a;
    {  // block
        lock_type lock( mtxData_ );
        // try to find async handler
        std::string strCallID = d.extract_json_id();
        if ( !strCallID.empty() )
            a = async_get( strCallID );
        // push it, if not found
        if ( a.strCallID.empty() )
            lstData_.push_back( d );
        else
            async_remove_by_call_id( strCallID );
    }  // block
    if ( !a.strCallID.empty() ) {
        if ( a.onData )
            a.onData( a.joIn, d );
    }
    return true;
}

data_t client::fetch_data_with_strategy( e_data_fetch_strategy edfs, const std::string id ) {
    data_t d;
    lock_type lock( mtxData_ );
    if ( lstData_.empty() )
        return d;
    switch ( edfs ) {
    case e_data_fetch_strategy::edfs_by_equal_json_id: {
        data_list_t::iterator itWalk = lstData_.begin(), itEnd = lstData_.end();
        for ( ; itWalk != itEnd; ++itWalk ) {
            const data_t& walk = ( *itWalk );
            if ( walk.is_json() ) {
                try {
                    std::string wid = walk.extract_json_id();
                    if ( wid == id ) {
                        d = walk;
                        lstData_.erase( itWalk );
                        break;
                    }
                } catch ( ... ) {
                }
            }
        }  // for ( ; itWalk != itEnd; ++itWalk )
    } break;
    case e_data_fetch_strategy::edfs_nearest_arrived:
        d = lstData_.front();
        lstData_.pop_front();
        break;
    case e_data_fetch_strategy::edfs_nearest_binary: {
        data_list_t::iterator itWalk = lstData_.begin(), itEnd = lstData_.end();
        for ( ; itWalk != itEnd; ++itWalk ) {
            const data_t& walk = ( *itWalk );
            if ( walk.is_binary() ) {
                d = walk;
                lstData_.erase( itWalk );
                break;
            }
        }  // for ( ; itWalk != itEnd; ++itWalk )
    } break;
    case e_data_fetch_strategy::edfs_nearest_json: {
        data_list_t::iterator itWalk = lstData_.begin(), itEnd = lstData_.end();
        for ( ; itWalk != itEnd; ++itWalk ) {
            const data_t& walk = ( *itWalk );
            if ( walk.is_json() ) {
                d = walk;
                lstData_.erase( itWalk );
                break;
            }
        }  // for ( ; itWalk != itEnd; ++itWalk )
    } break;
    case e_data_fetch_strategy::edfs_nearest_text: {
        data_list_t::iterator itWalk = lstData_.begin(), itEnd = lstData_.end();
        for ( ; itWalk != itEnd; ++itWalk ) {
            const data_t& walk = ( *itWalk );
            if ( !walk.is_binary() ) {
                d = walk;
                lstData_.erase( itWalk );
                break;
            }
        }  // for ( ; itWalk != itEnd; ++itWalk )
    } break;
    }  // switch( edfs )
    return d;
}

const char client::g_str_default_content_type[] = "application/json";

std::string client::stat_extract_short_content_type_string( const std::string& s ) {
    std::string h = skutils::tools::to_lower( skutils::tools::trim_copy( s ) );
    size_t pos = h.find( ';' );
    if ( pos != std::string::npos && pos > 0 )
        h = skutils::tools::trim_copy( h.substr( 0, pos ) );
    return h;
}

uint64_t client::stat_get_random_number( uint64_t const& min, uint64_t const& max ) {
    return ( ( ( uint64_t )( unsigned int ) rand() << 32 ) + ( uint64_t )( unsigned int ) rand() ) %
               ( max - min ) +
           min;
}
uint64_t client::stat_get_random_number() {
    return stat_get_random_number( 1, RAND_MAX );
}

bool client::stat_auto_gen_json_id( nlohmann::json& jo ) {
    if ( !jo.is_object() )
        return false;
    if ( jo.count( "id" ) > 0 )
        return false;
    static mutex_type g_mtx;
    lock_type lock( g_mtx );
    static volatile uint64_t g_id = stat_get_random_number();
    ++g_id;
    if ( g_id == 0 )
        ++g_id;
    jo["id"] = g_id;
    return true;  // "id" was generated and set
}

data_t client::call( const nlohmann::json& joIn, bool isAutoGenJsonID, e_data_fetch_strategy edfs,
    std::chrono::milliseconds wait_step, size_t cntSteps, bool isReturnErrorResponse ) {
    nlohmann::json jo = joIn;
    if ( isAutoGenJsonID )
        stat_auto_gen_json_id( jo );
    std::string strJsonIn = jo.dump();
    if ( ch_ ) {
        if ( ch_->is_valid() ) {
            data_t d;
#if (defined __SKUTIS_REST_USE_CURL_FOR_HTTP)
            std::string strOutData, strOutContentType;
            skutils::http::common_network_exception::error_info ei;
            bool ret = ch_->query(
               strJsonIn.c_str(), "application/json",
               strOutData, strOutContentType,
               ei
               );
            d.ei_ = ei;
            if( ( ! ret ) || strOutData.empty() ) {
                d.err_s_ = ei.strError_.empty() ? "call failed" : ei.strError_;
                return d;  // data_t();
            }
            d.s_ = strOutData;
            std::string h;
            if( ! strOutContentType.empty() )
                h = stat_extract_short_content_type_string( strOutContentType );
            d.content_type_ = ( !h.empty() ) ? h : g_str_default_content_type;
#else  // (defined __SKUTIS_REST_USE_CURL_FOR_HTTP)
            const std::string strHttpQueryPath = u_path_and_args();
            std::shared_ptr< skutils::http::response > resp = ch_->Post(
                strHttpQueryPath.c_str(), strJsonIn, "application/json", isReturnErrorResponse );
            d.ei_ = ch_->eiLast_;
            if ( !resp )
                return d;  // data_t();
            if ( !resp->send_status_ ) {
                d.err_s_ = resp->body_;
                return d;  // data_t();
            }
            if ( resp->status_ != 200 ) {
                d.err_s_ = resp->body_;
                return d;  // data_t();
            }
            d.s_ = resp->body_;
            std::string h;
            if ( resp->has_header( "Content-Type" ) )
                h = stat_extract_short_content_type_string( resp->get_header_value( "Content-Type" ) );
            d.content_type_ = ( !h.empty() ) ? h : g_str_default_content_type;
#endif // else from (defined __SKUTIS_REST_USE_CURL_FOR_HTTP)
            handle_data_arrived( d );
        }
    } else if ( cw_ ) {
        if ( cw_->isConnected() ) {
            if ( !cw_->sendMessage( strJsonIn ) )
                return data_t();
            for ( size_t i = 0; ( cw_->isConnected() ) && i < cntSteps; ++i ) {
                data_t d = fetch_data_with_strategy( edfs );
                if ( !d.empty() )
                    return d;
                std::this_thread::sleep_for( wait_step );
            }
        }
    } else if ( cz_ ) {
        data_t d;
        d.content_type_ = "application/json";
        std::string strAnswer;
        if ( !cz_->sendMessage( strJsonIn, strAnswer ) )  // auto-connect
            return data_t();
        d.s_ = strAnswer;
        handle_data_arrived( d );
    }
    data_t d = fetch_data_with_strategy( edfs );
    if ( ( !d.err_s_.empty() ) || d.empty() ) {
        d.ei_.et_ = skutils::http::common_network_exception::error_type::et_unknown;
        d.ei_.ec_ = errno;
        d.ei_.strError_ = "WS(S) data transfer error";
        if ( !d.err_s_.empty() ) {
            d.ei_.strError_ += ": ";
            d.ei_.strError_ += d.err_s_;
        }
    }
    return d;
}
data_t client::call( const std::string& strJsonIn, bool isAutoGenJsonID, e_data_fetch_strategy edfs,
    std::chrono::milliseconds wait_step, size_t cntSteps, bool isReturnErrorResponse ) {
    try {
        nlohmann::json jo = nlohmann::json::parse( strJsonIn );
        return call( jo, isAutoGenJsonID, edfs, wait_step, cntSteps, isReturnErrorResponse );
    } catch ( ... ) {
    }
    return data_t();
}

skutils::dispatch::queue_id_t client::async_get_dispatch_queue_id(
    const std::string& strCallID ) const {
    return skutils::dispatch::generate_id(
        ( void* ) this, ( !strCallID.empty() ) ? strCallID.c_str() : "noID" );
}
await_t client::async_get( const std::string& strCallID ) const {
    if ( strCallID.empty() )
        return await_t();
    lock_type lock( mtxData_ );
    map_await_t::const_iterator itFind = map_await_.find( strCallID ), itEnd = map_await_.cend();
    if ( itFind == itEnd )
        return await_t();
    return itFind->second;
}
void client::async_add( await_t& a ) {
    auto strCallID = a.strCallID;
    if ( strCallID.empty() )
        throw std::runtime_error( "rest async call must have non-empty \"id\"" );
    lock_type lock( mtxData_ );
    map_await_t::const_iterator itFind = map_await_.find( a.strCallID ), itEnd = map_await_.cend();
    if ( itFind != itEnd )
        throw std::runtime_error( "rest async call must have unique \"id\"" );
    skutils::dispatch::job_t fnTimeout = [=]() -> void {
        await_t a2 = async_get( strCallID );
        async_remove_by_call_id( strCallID );
        if ( a2.onError )
            a2.onError( a2.joIn, "timeout" );
    };
    skutils::dispatch::once( async_get_dispatch_queue_id( strCallID ), fnTimeout,
        skutils::dispatch::duration_from_milliseconds( a.wait_timeout.count() ),
        &a.idTimeoutNotificationJob );
    map_await_[strCallID] = a;
}
void client::async_remove_impl( await_t& a ) {
    // no lock needed here
    if ( a.strCallID.empty() )
        return;
    if ( !a.idTimeoutNotificationJob.empty() ) {
        skutils::dispatch::stop( a.idTimeoutNotificationJob );
        a.idTimeoutNotificationJob.clear();
    }
    skutils::dispatch::remove( async_get_dispatch_queue_id( a.strCallID ) );
}
bool client::async_remove_by_call_id( const std::string& strCallID ) {
    lock_type lock( mtxData_ );
    map_await_t::iterator itFind = map_await_.find( strCallID ), itEnd = map_await_.end();
    if ( itFind == itEnd )
        return false;
    async_remove_impl( itFind->second );
    map_await_.erase( itFind );
    return true;
}
void client::async_remove_all() {
    lock_type lock( mtxData_ );
    map_await_t::iterator itWalk = map_await_.begin(), itEnd = map_await_.end();
    for ( ; itWalk != itEnd; ++itWalk )
        async_remove_impl( itWalk->second );
    map_await_.clear();
}

void client::async_call( const nlohmann::json& joIn, fn_async_call_data_handler_t onData,
    fn_async_call_error_handler_t onError, bool isAutoGenJsonID, e_data_fetch_strategy edfs,
    std::chrono::milliseconds wait_timeout ) {
    if ( !onData )
        throw std::runtime_error(
            "skutils::reset::client::async_call() cannot be used without onData() handler" );
    if ( !onError )
        throw std::runtime_error(
            "skutils::reset::client::async_call() cannot be used without onError() handler" );
    nlohmann::json jo = joIn;
    if ( isAutoGenJsonID )
        stat_auto_gen_json_id( jo );
    std::string strCallID = jo["id"].dump();
    std::string strJsonIn = jo.dump();
    if ( ch_ ) {
        if ( ch_->is_valid() ) {
            data_t d;
#if (defined __SKUTIS_REST_USE_CURL_FOR_HTTP)
            std::string strOutData, strOutContentType;
            skutils::http::common_network_exception::error_info ei;
            bool ret = ch_->query(
               strJsonIn.c_str(), "application/json",
               strOutData, strOutContentType,
               ei
               );
            d.ei_ = ei;
            if( ( ! ret ) || strOutData.empty() ) {
                d.err_s_ = ei.strError_.empty() ? "call failed" : ei.strError_;
                onError( jo, d.err_s_.c_str() );
                return;
            }
            d.s_ = strOutData;
            std::string h;
            if( ! strOutContentType.empty() )
                h = stat_extract_short_content_type_string( strOutContentType );
            d.content_type_ = ( !h.empty() ) ? h : g_str_default_content_type;
#else  // (defined __SKUTIS_REST_USE_CURL_FOR_HTTP)
            const std::string strHttpQueryPath = u_path_and_args();
            std::shared_ptr< skutils::http::response > resp =
                ch_->Post( strHttpQueryPath.c_str(), strJsonIn, "application/json" );
            if ( !resp ) {
                onError( jo, "empty responce" );
                return;
            }
            if ( resp->status_ != 200 ) {
                onError( jo, skutils::tools::format( "returned status %d from call to %s",
                                 int( resp->status_ ), u_.str().c_str() ) );
                return;
            }
            d.s_ = resp->body_;
            std::string h;
            if ( resp->has_header( "Content-Type" ) )
                h = stat_extract_short_content_type_string(
                    resp->get_header_value( "Content-Type" ) );
            d.content_type_ = ( !h.empty() ) ? h : g_str_default_content_type;
#endif  // else from (defined __SKUTIS_REST_USE_CURL_FOR_HTTP)
            handle_data_arrived( d );
            data_t dataOut = fetch_data_with_strategy( edfs );
            onData( jo, dataOut );
            return;
        };
        onError( jo, skutils::tools::format( "not connected to %s", u_.str().c_str() ) );
        return;
    }
    if ( cw_ ) {
        if ( cw_->isConnected() ) {
            await_t a;
            a.strCallID = strCallID;
            a.joIn = jo;
            a.wait_timeout = wait_timeout;
            // a.idTimeoutNotificationJob = ....
            a.onData = onData;
            a.onError = onError;
            async_add( a );
            if ( !cw_->sendMessage( strJsonIn ) ) {
                onError(
                    jo, skutils::tools::format( "data transfer error to %s", u_.str().c_str() ) );
                return;
            }
            // for ( size_t i = 0; ( cw_->isConnected() ) && i < cntSteps; ++i ) {
            //    data_t d = fetch_data_with_strategy( edfs );
            //    if ( !d.empty() )
            //        return d;
            //    std::this_thread::sleep_for( wait_step );
            //}
            // return;
            return;
        }
        onError( jo, skutils::tools::format( "not connected to %s", u_.str().c_str() ) );
        return;
    }
    if ( cz_ ) {
        if ( cz_->isConnected() ) {
            std::string strAnswer;
            if ( !cz_->sendMessage( strJsonIn, strAnswer ) ) {
                onError( jo, skutils::tools::format( "not connected to %s", u_.str().c_str() ) );
                return;
            }
            data_t d;
            d.content_type_ = "application/json";
            d.s_ = strAnswer;
            handle_data_arrived( d );
            return;
        }
        onError( jo, skutils::tools::format( "not connected to %s", u_.str().c_str() ) );
        return;
    }
    onError( jo, skutils::tools::format( "connection not initialized for %s", u_.str().c_str() ) );
}
void client::async_call( const std::string& strJsonIn, fn_async_call_data_handler_t onData,
    fn_async_call_error_handler_t onError, bool isAutoGenJsonID, e_data_fetch_strategy edfs,
    std::chrono::milliseconds wait_timeout ) {
    try {
        nlohmann::json jo = nlohmann::json::parse( strJsonIn );
        return async_call( jo, onData, onError, isAutoGenJsonID, edfs, wait_timeout );
    } catch ( ... ) {
    }
}

};  // namespace rest
};  // namespace skutils
