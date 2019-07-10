#include <openssl/err.h>
#include <skutils/mail.h>
#include <skutils/utils.h>
#include <cassert>

#define TIME_IN_SEC 3 * 60  // how long client will wait for server response in non-blocking mode
#define BUFFER_SIZE 10240   // sendData and RecvData buffers sizes
#define MSG_SIZE_IN_MB 25   // the maximum size of the message with all attachments
#define COUNTER_VALUE 100   // how many times program will try to receive data

#if ( defined WIN32 )
#else  /// (defined WIN32)
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#ifndef HAVE__STRNICMP
#define HAVE__STRNICMP
#define _strnicmp strncasecmp
#endif
#define OutputDebugStringA( buf )
#endif  /// else from (defined WIN32)

namespace skutils {
namespace mail {

smtp_x_priority str_2_smtp_x_priority( const std::string& s ) {
    std::string x = skutils::tools::to_lower( skutils::tools::trim_copy( s ) );
    if ( x.empty() )
        return smtp_x_priority::XPRIORITY_NORMAL;
    char c0 = x[0];
    switch ( c0 ) {
    case 'h':
        return smtp_x_priority::XPRIORITY_HIGH;
    case 'l':
        return smtp_x_priority::XPRIORITY_LOW;
    }  // switch( c0 )
    return smtp_x_priority::XPRIORITY_NORMAL;
}
std::string smtp_x_priority_2_str( smtp_x_priority sxp ) {
    switch ( sxp ) {
    case smtp_x_priority::XPRIORITY_HIGH:
        return "high";
    case smtp_x_priority::XPRIORITY_LOW:
        return "low";
    default:
        return "normal";
    }  // switch( sxp )
}

smtp_security_type str_2_smtp_security_type( const std::string& s ) {
    std::string x = skutils::tools::to_lower( skutils::tools::trim_copy( s ) );
    if ( x.empty() )
        return smtp_security_type::DO_NOT_SET;
    char c0 = x[0];
    switch ( c0 ) {
    case 'z':
    case '0':
    case 'n':
        return smtp_security_type::NO_SECURITY;
    case 't':
        return smtp_security_type::USE_TLS;
    case 's':
        return smtp_security_type::USE_SSL;
    }  // switch( c0 )
    return smtp_security_type::DO_NOT_SET;
}
std::string smtp_security_type_2_str( smtp_security_type sst ) {
    switch ( sst ) {
    case smtp_security_type::NO_SECURITY:
        return "none";
    case smtp_security_type::USE_TLS:
        return "tls";
    case smtp_security_type::USE_SSL:
        return "ssl";
    default:
        return "unknown";
    }  // switch( sst )
}

command_entry command_list[] = {
    {command_INIT, 0, 5 * 60, 220, smtp_error_info::SERVER_NOT_RESPONDING},
    {command_EHLO, 5 * 60, 5 * 60, 250, smtp_error_info::COMMAND_EHLO},
    {command_AUTHPLAIN, 5 * 60, 5 * 60, 235, smtp_error_info::COMMAND_AUTH_PLAIN},
    {command_AUTHLOGIN, 5 * 60, 5 * 60, 334, smtp_error_info::COMMAND_AUTH_LOGIN},
    {command_AUTHCRAMMD5, 5 * 60, 5 * 60, 334, smtp_error_info::COMMAND_AUTH_CRAMMD5},
    {command_AUTHDIGESTMD5, 5 * 60, 5 * 60, 334, smtp_error_info::COMMAND_AUTH_DIGESTMD5},
    {command_DIGESTMD5, 5 * 60, 5 * 60, 335, smtp_error_info::COMMAND_DIGESTMD5},
    {command_USER, 5 * 60, 5 * 60, 334, smtp_error_info::UNDEF_XYZ_RESPONSE},
    {command_PASSWORD, 5 * 60, 5 * 60, 235, smtp_error_info::BAD_LOGIN_PASS},
    {command_MAILFROM, 5 * 60, 5 * 60, 250, smtp_error_info::COMMAND_MAIL_FROM},
    {command_RCPTTO, 5 * 60, 5 * 60, 250, smtp_error_info::COMMAND_RCPT_TO},
    {command_DATA, 5 * 60, 2 * 60, 354, smtp_error_info::COMMAND_DATA},
    {command_DATABLOCK, 3 * 60, 0, 0,
        smtp_error_info::COMMAND_DATABLOCK},  // Here the valid_reply_code is set to zero because
                                              // there are no replies when sending data blocks
    {command_DATAEND, 3 * 60, 10 * 60, 250, smtp_error_info::MSG_BODY_ERROR},
    {command_QUIT, 5 * 60, 5 * 60, 221, smtp_error_info::COMMAND_QUIT},
    {command_STARTTLS, 5 * 60, 5 * 60, 220, smtp_error_info::COMMAND_EHLO_STARTTLS}};
command_entry* findCommandEntry( SMTP_COMMAND command ) {
    command_entry* pEntry = nullptr;
    for ( size_t i = 0; i < sizeof( command_list ) / sizeof( command_list[0] ); ++i ) {
        if ( command_list[i].command == command ) {
            pEntry = &command_list[i];
            break;
        }
    }
    assert( pEntry != nullptr );
    return pEntry;
}
bool isKeywordSupported( const char* response, const char* keyword ) {  // a simple string match
    assert( response != nullptr && keyword != nullptr );
    if ( response == nullptr || keyword == nullptr )
        return false;
    int res_len = strlen( response );
    int key_len = strlen( keyword );
    if ( res_len < key_len )
        return false;
    int pos = 0;
    for ( ; pos < res_len - key_len + 1; ++pos ) {
        if ( _strnicmp( keyword, response + pos, key_len ) == 0 ) {
            if ( pos > 0 && ( response[pos - 1] == '-' || response[pos - 1] == ' ' ||
                                response[pos - 1] == '=' ) ) {
                if ( pos + key_len < res_len ) {
                    if ( response[pos + key_len] == ' ' || response[pos + key_len] == '=' )
                        return true;
                    else if ( pos + key_len + 1 < res_len ) {
                        if ( response[pos + key_len] == '\r' &&
                             response[pos + key_len + 1] == '\n' )
                            return true;
                    }
                }
            }
        }
    }
    return false;
}
unsigned char* charToUnsignedChar( const char* strIn ) {
    unsigned char* strOut = nullptr;
    unsigned long length, i;
    length = strlen( strIn );
    strOut = new unsigned char[length + 1];
    if ( !strOut )
        return nullptr;
    for ( i = 0; i < length; i++ )
        strOut[i] = ( unsigned char ) strIn[i];
    strOut[length] = '\0';
    return strOut;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

client::client( bool bCareAboutOpenSSLInitAndShutdown /*= false*/ ) {
    bCareAboutOpenSSLInitAndShutdown_ = bCareAboutOpenSSLInitAndShutdown;
    socket_handle_ = INVALID_SOCKET;
    bIsConnected_ = false;
    eXPriority_ = XPRIORITY_NORMAL;
    nSMTPServerPort_ = 0;
    bAuthenticate_ = true;
#if ( defined WIN32 )
    WSADATA wsaData;
    WORD wVer = MAKEWORD( 2, 2 );
    if ( WSAStartup( wVer, &wsaData ) != NO_ERROR )
        throw smtp_error_info( smtp_error_info::WSA_STARTUP );
    if ( LOBYTE( wsaData.wVersion ) != 2 || HIBYTE( wsaData.wVersion ) != 2 ) {
        WSACleanup();
        throw smtp_error_info( smtp_error_info::WSA_VER );
    }
#endif  /// #if (defined WIN32)
    char hostname[255];
    if ( gethostname( ( char* ) &hostname, 255 ) == SOCKET_ERROR )
        throw smtp_error_info( smtp_error_info::WSA_HOSTNAME );
    strLocalHostName_ = hostname;
    if ( ( pRecvBuf_ = new char[BUFFER_SIZE] ) == nullptr )
        throw smtp_error_info( smtp_error_info::LACK_OF_MEMORY );
    if ( ( pSendBuf_ = new char[BUFFER_SIZE] ) == nullptr )
        throw smtp_error_info( smtp_error_info::LACK_OF_MEMORY );
    security_type_ = NO_SECURITY;
    ssl_ctx_ = nullptr;
    ssl_handle_ = nullptr;
    bIsHTML_ = false;
    bReadReceipt_ = false;
    strCharSet_ = "US-ASCII";
}
client::~client() {
    if ( bIsConnected_ )
        disconnectRemoteServer();
    if ( pSendBuf_ ) {
        delete[] pSendBuf_;
        pSendBuf_ = nullptr;
    }
    if ( pRecvBuf_ ) {
        delete[] pRecvBuf_;
        pRecvBuf_ = nullptr;
    }
    cleanupOpenSSL();
#if ( defined WIN32 )
    WSACleanup();
#endif  /// (defined WIN32)
}

void client::addAttachment( const char* Path ) {
    assert( Path );
    vecAttachments_.insert( vecAttachments_.end(), Path );
}
void client::addRecipient( const char* email, const char* name ) {
    if ( !email )
        throw smtp_error_info( smtp_error_info::UNDEF_RECIPIENT_MAIL );
    Recipient recipient;
    recipient.mail_ = email;
    if ( name != nullptr )
        recipient.name_ = name;
    else
        recipient.name_.empty();
    vecRecipients_.insert( vecRecipients_.end(), recipient );
}
void client::addCCRecipient( const char* email, const char* name ) {
    if ( !email )
        throw smtp_error_info( smtp_error_info::UNDEF_RECIPIENT_MAIL );
    Recipient recipient;
    recipient.mail_ = email;
    if ( name != nullptr )
        recipient.name_ = name;
    else
        recipient.name_.empty();
    vecCCRecipients_.insert( vecCCRecipients_.end(), recipient );
}
void client::addBCCRecipient( const char* email, const char* name ) {
    if ( !email )
        throw smtp_error_info( smtp_error_info::UNDEF_RECIPIENT_MAIL );
    Recipient recipient;
    recipient.mail_ = email;
    if ( name != nullptr )
        recipient.name_ = name;
    else
        recipient.name_.empty();
    vecBvecCCRecipients__.insert( vecBvecCCRecipients__.end(), recipient );
}

void client::addMsgLine( const char* Text ) {
    if ( Text )
        msgBody_.insert( msgBody_.end(), Text );
}
void client::delMsgLine( unsigned int Line ) {
    if ( Line >= msgBody_.size() )
        throw smtp_error_info( smtp_error_info::OUT_OF_MSG_RANGE );
    msgBody_.erase( msgBody_.begin() + Line );
}

void client::addMsgLines( const char* Text ) {
    if ( !Text )
        return;
    std::vector< std::string > vecLines = skutils::tools::split2vec( Text, '\n' );
    size_t i, cnt = vecLines.size();
    for ( i = 0; i < cnt; ++i )
        addMsgLine( vecLines[i].c_str() );
}

void client::delRecipients() {
    vecRecipients_.clear();
}
void client::delvecBvecCCRecipients__() {
    vecBvecCCRecipients__.clear();
}
void client::delvecCCRecipients_() {
    vecCCRecipients_.clear();
}

void client::delMsgLines() {
    msgBody_.clear();
}
void client::delvecAttachments_() {
    vecAttachments_.clear();
}

void client::modMsgLine( unsigned int Line, const char* Text ) {
    if ( Text ) {
        if ( Line >= msgBody_.size() )
            throw smtp_error_info( smtp_error_info::OUT_OF_MSG_RANGE );
        msgBody_.at( Line ) = std::string( Text );
    }
}

void client::clearMessage() {
    delRecipients();
    delvecBvecCCRecipients__();
    delvecCCRecipients_();
    delvecAttachments_();
    delMsgLines();
}

void client::send() {
    unsigned int i, rcpt_count, res, FileId;
    char* FileBuf = nullptr;
    FILE* hFile = nullptr;
    unsigned long int FileSize, TotalSize, MsgPart;
    std::string FileName, EncodedFileName;
    std::string::size_type pos;
    // ***** CONNECTING TO SMTP SERVER *****
    // connecting to remote host if not already connected:
    if ( socket_handle_ == INVALID_SOCKET ) {
        if ( !connectRemoteServer(
                 strSMTPSrvName_.c_str(), nSMTPServerPort_, security_type_, bAuthenticate_ ) )
            throw smtp_error_info( smtp_error_info::WSA_INVALID_SOCKET );
    }
    try {
        if ( ( FileBuf = new char[55] ) == nullptr )
            throw smtp_error_info( smtp_error_info::LACK_OF_MEMORY );
        // check that any attachments specified can be opened
        TotalSize = 0;
        for ( FileId = 0; FileId < vecAttachments_.size(); FileId++ ) {
            // opening the file:
            hFile = fopen( vecAttachments_[FileId].c_str(), "rb" );
            if ( hFile == nullptr )
                throw smtp_error_info( smtp_error_info::FILE_NOT_EXIST );
            // checking file size:
            fseek( hFile, 0, SEEK_END );
            FileSize = ftell( hFile );
            TotalSize += FileSize;
            // sending the file:
            if ( TotalSize / 1024 > MSG_SIZE_IN_MB * 1024 )
                throw smtp_error_info( smtp_error_info::MSG_TOO_BIG );
            fclose( hFile );
            hFile = nullptr;
        }
        // ***** SENDING E-MAIL *****
        // MAIL <SP> FROM:<reverse-path> <CRLF>
        if ( !strMailFrom_.size() )
            throw smtp_error_info( smtp_error_info::UNDEF_MAIL_FROM );
        command_entry* pEntry = findCommandEntry( command_MAILFROM );
        snprintf( pSendBuf_, BUFFER_SIZE, "MAIL FROM:<%s>\r\n", strMailFrom_.c_str() );
        sendData( pEntry );
        receiveResponse( pEntry );
        // RCPT <SP> TO:<forward-path> <CRLF>
        if ( !( rcpt_count = vecRecipients_.size() ) )
            throw smtp_error_info( smtp_error_info::UNDEF_RECIPIENTS );
        pEntry = findCommandEntry( command_RCPTTO );
        for ( i = 0; i < vecRecipients_.size(); i++ ) {
            snprintf( pSendBuf_, BUFFER_SIZE, "RCPT TO:<%s>\r\n",
                ( vecRecipients_.at( i ).mail_ ).c_str() );
            sendData( pEntry );
            receiveResponse( pEntry );
        }
        for ( i = 0; i < vecCCRecipients_.size(); i++ ) {
            snprintf( pSendBuf_, BUFFER_SIZE, "RCPT TO:<%s>\r\n",
                ( vecCCRecipients_.at( i ).mail_ ).c_str() );
            sendData( pEntry );
            receiveResponse( pEntry );
        }
        for ( i = 0; i < vecBvecCCRecipients__.size(); i++ ) {
            snprintf( pSendBuf_, BUFFER_SIZE, "RCPT TO:<%s>\r\n",
                ( vecBvecCCRecipients__.at( i ).mail_ ).c_str() );
            sendData( pEntry );
            receiveResponse( pEntry );
        }
        pEntry = findCommandEntry( command_DATA );
        // DATA <CRLF>
        snprintf( pSendBuf_, BUFFER_SIZE, "DATA\r\n" );
        sendData( pEntry );
        receiveResponse( pEntry );
        pEntry = findCommandEntry( command_DATABLOCK );
        // send header(s)
        formatHeader( pSendBuf_ );
        sendData( pEntry );
        // send text message
        if ( getMsgLines() ) {
            for ( i = 0; i < getMsgLines(); i++ ) {
                snprintf( pSendBuf_, BUFFER_SIZE, "%s\r\n", getMsgLineText( i ) );
                sendData( pEntry );
            }
        } else {
            snprintf( pSendBuf_, BUFFER_SIZE, "%s\r\n", " " );
            sendData( pEntry );
        }
        // next goes attachments (if they are)
        for ( FileId = 0; FileId < vecAttachments_.size(); FileId++ ) {
#if ( defined WIN32 )
            pos = vecAttachments_[FileId].find_last_of( "/" );
#else
            pos = vecAttachments_[FileId].find_last_of( "\\" );
#endif
            if ( pos == std::string::npos )
                FileName = vecAttachments_[FileId];
            else
                FileName = vecAttachments_[FileId].substr( pos + 1 );

            // RFC 2047 - Use UTF-8 charset,base64 encode.
            EncodedFileName = "=?UTF-8?B?";
            EncodedFileName += skutils::tools::base64::encode(
                ( unsigned char* ) FileName.c_str(), FileName.size() );
            EncodedFileName += "?=";

            snprintf( pSendBuf_, BUFFER_SIZE, "--%s\r\n", BOUNDARY_TEXT );
            strcat( pSendBuf_, "Content-Type: application/x-msdownload; name=\"" );
            strcat( pSendBuf_, EncodedFileName.c_str() );
            strcat( pSendBuf_, "\"\r\n" );
            strcat( pSendBuf_, "Content-Transfer-Encoding: base64\r\n" );
            strcat( pSendBuf_, "Content-Disposition: attachment; filename=\"" );
            strcat( pSendBuf_, EncodedFileName.c_str() );
            strcat( pSendBuf_, "\"\r\n" );
            strcat( pSendBuf_, "\r\n" );

            sendData( pEntry );

            // opening the file:
            hFile = fopen( vecAttachments_[FileId].c_str(), "rb" );
            if ( hFile == nullptr )
                throw smtp_error_info( smtp_error_info::FILE_NOT_EXIST );

            // get file size:
            fseek( hFile, 0, SEEK_END );
            FileSize = ftell( hFile );
            fseek( hFile, 0, SEEK_SET );
            MsgPart = 0;
            for ( i = 0; i < FileSize / 54 + 1; i++ ) {
                res = fread( FileBuf, sizeof( char ), 54, hFile );
                MsgPart ? strcat( pSendBuf_,
                              skutils::tools::base64::encode(
                                  reinterpret_cast< const unsigned char* >( FileBuf ), res )
                                  .c_str() ) :
                          strcpy( pSendBuf_,
                              skutils::tools::base64::encode(
                                  reinterpret_cast< const unsigned char* >( FileBuf ), res )
                                  .c_str() );
                strcat( pSendBuf_, "\r\n" );
                MsgPart += res + 2;
                if ( MsgPart >= BUFFER_SIZE / 2 ) {  // sending part of the message
                    MsgPart = 0;
                    sendData( pEntry );  // FileBuf, FileName, fclose( hFile );
                }
            }
            if ( MsgPart )
                sendData( pEntry );  // FileBuf, FileName, fclose( hFile );
            fclose( hFile );
            hFile = nullptr;
        }
        delete[] FileBuf;
        FileBuf = nullptr;

        // sending last message block (if there is one or more attachments)
        if ( vecAttachments_.size() ) {
            snprintf( pSendBuf_, BUFFER_SIZE, "\r\n--%s--\r\n", BOUNDARY_TEXT );
            sendData( pEntry );
        }

        pEntry = findCommandEntry( command_DATAEND );
        // <CRLF> . <CRLF>
        snprintf( pSendBuf_, BUFFER_SIZE, "\r\n.\r\n" );
        sendData( pEntry );
        receiveResponse( pEntry );
    } catch ( const smtp_error_info& ) {
        if ( hFile )
            fclose( hFile );
        if ( FileBuf )
            delete[] FileBuf;
        disconnectRemoteServer();
        throw;
    }
}

bool client::connectRemoteServer( const char* strServer,
    const unsigned short anPort,      // = 0
    smtp_security_type securityType,  // = DO_NOT_SET
    bool b_authenticate,              // = true
    const char* login,                // = nullptr
    const char* password              // = nullptr
) {
    unsigned short nPort = 0;
    LPSERVENT lpServEnt;
    SOCKADDR_IN sockAddr;
    unsigned long ul = 1;
    fd_set fdwrite, fdexcept;
    timeval timeout;
    int res = 0;
    try {
        timeout.tv_sec = TIME_IN_SEC;
        timeout.tv_usec = 0;
        socket_handle_ = INVALID_SOCKET;
        if ( ( socket_handle_ = socket( PF_INET, SOCK_STREAM, 0 ) ) == INVALID_SOCKET )
            throw smtp_error_info( smtp_error_info::WSA_INVALID_SOCKET );
        if ( anPort != 0 )
            nPort = htons( anPort );
        else {
            lpServEnt = getservbyname( "mail", 0 );
            if ( lpServEnt == nullptr )
                nPort = htons( 25 );
            else
                nPort = lpServEnt->s_port;
        }
        sockAddr.sin_family = AF_INET;
        sockAddr.sin_port = nPort;
        if ( ( sockAddr.sin_addr.s_addr = inet_addr( strServer ) ) == INADDR_NONE ) {
            LPHOSTENT host;
            host = gethostbyname( strServer );
            if ( host )
                memcpy( &sockAddr.sin_addr, host->h_addr_list[0], host->h_length );
            else {
#if ( defined WIN32 )
                closesocket( socket_handle_ );
#else
                close( socket_handle_ );
#endif
                throw smtp_error_info( smtp_error_info::WSA_GETHOSTBY_NAME_ADDR );
            }
        }

        // start non-blocking mode for socket:
#if ( defined WIN32 )
        if ( ioctlsocket( socket_handle_, FIONBIO, ( unsigned long* ) &ul ) == SOCKET_ERROR )
#else
        if ( ioctl( socket_handle_, FIONBIO, ( unsigned long* ) &ul ) == SOCKET_ERROR )
#endif
        {
#if ( defined WIN32 )
            closesocket( socket_handle_ );
#else
            close( socket_handle_ );
#endif
            throw smtp_error_info( smtp_error_info::WSA_IOCTLSOCKET );
        }
        if ( connect( socket_handle_, ( LPSOCKADDR ) &sockAddr, sizeof( sockAddr ) ) ==
             SOCKET_ERROR ) {
#if ( defined WIN32 )
            if ( WSAGetLastError() != WSAEWOULDBLOCK )
#else
            if ( errno != EINPROGRESS )
#endif
            {
#if ( defined WIN32 )
                closesocket( socket_handle_ );
#else
                close( socket_handle_ );
#endif
                throw smtp_error_info( smtp_error_info::WSA_CONNECT );
            }
        } else
            return true;

        while ( true ) {
            FD_ZERO( &fdwrite );
            FD_ZERO( &fdexcept );

            FD_SET( socket_handle_, &fdwrite );
            FD_SET( socket_handle_, &fdexcept );

            if ( ( res = select( socket_handle_ + 1, nullptr, &fdwrite, &fdexcept, &timeout ) ) ==
                 SOCKET_ERROR ) {
#if ( defined WIN32 )
                closesocket( socket_handle_ );
#else
                close( socket_handle_ );
#endif
                throw smtp_error_info( smtp_error_info::WSA_SELECT );
            }
            if ( !res ) {
#if ( defined WIN32 )
                closesocket( socket_handle_ );
#else
                close( socket_handle_ );
#endif
                throw smtp_error_info( smtp_error_info::SELECT_TIMEOUT );
            }
            if ( res && FD_ISSET( socket_handle_, &fdwrite ) )
                break;
            if ( res && FD_ISSET( socket_handle_, &fdexcept ) ) {
#if ( defined WIN32 )
                closesocket( socket_handle_ );
#else
                close( socket_handle_ );
#endif
                throw smtp_error_info( smtp_error_info::WSA_SELECT );
            }
        }
        FD_CLR( socket_handle_, &fdwrite );
        FD_CLR( socket_handle_, &fdexcept );
        if ( securityType != DO_NOT_SET )
            setSecurityType( securityType );
        if ( getSecurityType() == USE_TLS || getSecurityType() == USE_SSL ) {
            initOpenSSL();
            if ( getSecurityType() == USE_SSL )
                connectOpenSSL();
        }
        command_entry* pEntry = findCommandEntry( command_INIT );
        receiveResponse( pEntry );
        sayHello();
        if ( getSecurityType() == USE_TLS ) {
            startTls();
            sayHello();
        }
        if ( b_authenticate && isKeywordSupported( pRecvBuf_, "AUTH" ) == true ) {
            if ( login )
                setLogin( login );
            if ( !strLogin_.size() )
                throw smtp_error_info( smtp_error_info::UNDEF_LOGIN );
            if ( password )
                setPassword( password );
            if ( !strPassword_.size() )
                throw smtp_error_info( smtp_error_info::UNDEF_PASSWORD );
            if ( isKeywordSupported( pRecvBuf_, "LOGIN" ) == true ) {
                pEntry = findCommandEntry( command_AUTHLOGIN );
                snprintf( pSendBuf_, BUFFER_SIZE, "AUTH LOGIN\r\n" );
                sendData( pEntry );
                receiveResponse( pEntry );

                // send login:
                std::string encoded_login = skutils::tools::base64::encode(
                    reinterpret_cast< const unsigned char* >( strLogin_.c_str() ),
                    strLogin_.size() );
                pEntry = findCommandEntry( command_USER );
                snprintf( pSendBuf_, BUFFER_SIZE, "%s\r\n", encoded_login.c_str() );
                sendData( pEntry );
                receiveResponse( pEntry );

                // send password:
                std::string encoded_password = skutils::tools::base64::encode(
                    reinterpret_cast< const unsigned char* >( strPassword_.c_str() ),
                    strPassword_.size() );
                pEntry = findCommandEntry( command_PASSWORD );
                snprintf( pSendBuf_, BUFFER_SIZE, "%s\r\n", encoded_password.c_str() );
                sendData( pEntry );
                receiveResponse( pEntry );
            } else if ( isKeywordSupported( pRecvBuf_, "PLAIN" ) == true ) {
                pEntry = findCommandEntry( command_AUTHPLAIN );
                snprintf( pSendBuf_, BUFFER_SIZE, "%s^%s^%s", strLogin_.c_str(), strLogin_.c_str(),
                    strPassword_.c_str() );
                unsigned int length = strlen( pSendBuf_ );
                unsigned char* ustrLogin = charToUnsignedChar( pSendBuf_ );
                for ( unsigned int i = 0; i < length; i++ ) {
                    if ( ustrLogin[i] == 94 )
                        ustrLogin[i] = 0;
                }
                std::string encoded_login = skutils::tools::base64::encode( ustrLogin, length );
                delete[] ustrLogin;
                snprintf( pSendBuf_, BUFFER_SIZE, "AUTH PLAIN %s\r\n", encoded_login.c_str() );
                sendData( pEntry );
                receiveResponse( pEntry );
            } else if ( isKeywordSupported( pRecvBuf_, "CRAM-MD5" ) == true ) {
                pEntry = findCommandEntry( command_AUTHCRAMMD5 );
                snprintf( pSendBuf_, BUFFER_SIZE, "AUTH CRAM-MD5\r\n" );
                sendData( pEntry );
                receiveResponse( pEntry );

                std::string encoded_challenge = pRecvBuf_;
                encoded_challenge = encoded_challenge.substr( 4 );
                std::string decoded_challenge = skutils::tools::base64::decode( encoded_challenge );

                /////////////////////////////////////////////////////////////////////
                // test data from RFC 2195
                // decoded_challenge = "<1896.697170952@postoffice.reston.mci.net>";
                // strLogin_ = "tim";
                // strPassword_ = "tanstaaftanstaaf";
                // MD5 should produce b913a602c7eda7a495b4e6e7334d3890
                // should encode as dGltIGI5MTNhNjAyYzdlZGE3YTQ5NWI0ZTZlNzMzNGQzODkw
                /////////////////////////////////////////////////////////////////////

                unsigned char* ustrChallenge = charToUnsignedChar( decoded_challenge.c_str() );
                unsigned char* ustrPassword = charToUnsignedChar( strPassword_.c_str() );
                if ( ( !ustrChallenge ) || ( !ustrPassword ) )
                    throw smtp_error_info( smtp_error_info::BAD_LOGIN_PASSWORD );

                // if ustrPassword is longer than 64 bytes reset it to
                // ustrPassword=MD5(ustrPassword)
                int passwordLength = strPassword_.size();
                if ( passwordLength > 64 ) {
                    skutils::tools::md5 md5password;
                    md5password.update( ustrPassword, passwordLength );
                    md5password.finalize();
                    ustrPassword = md5password.raw_digest();
                    passwordLength = 16;
                }

                // Storing ustrPassword in pads
                unsigned char ipad[65], opad[65];
                memset( ipad, 0, 64 );
                memset( opad, 0, 64 );
                memcpy( ipad, ustrPassword, passwordLength );
                memcpy( opad, ustrPassword, passwordLength );

                // XOR ustrPassword with ipad and opad values
                for ( int i = 0; i < 64; i++ ) {
                    ipad[i] ^= 0x36;
                    opad[i] ^= 0x5c;
                }

                // perform inner MD5
                skutils::tools::md5 md5pass1;
                md5pass1.update( ipad, 64 );
                md5pass1.update( ustrChallenge, decoded_challenge.size() );
                md5pass1.finalize();
                unsigned char* ustrResult = md5pass1.raw_digest();

                // perform outer MD5
                skutils::tools::md5 md5pass2;
                md5pass2.update( opad, 64 );
                md5pass2.update( ustrResult, 16 );
                md5pass2.finalize();
                decoded_challenge = md5pass2.hex_digest();

                delete[] ustrChallenge;
                delete[] ustrPassword;
                delete[] ustrResult;

                decoded_challenge = strLogin_ + " " + decoded_challenge;
                encoded_challenge = skutils::tools::base64::encode(
                    reinterpret_cast< const unsigned char* >( decoded_challenge.c_str() ),
                    decoded_challenge.size() );

                snprintf( pSendBuf_, BUFFER_SIZE, "%s\r\n", encoded_challenge.c_str() );
                pEntry = findCommandEntry( command_PASSWORD );
                sendData( pEntry );
                receiveResponse( pEntry );
            } else if ( isKeywordSupported( pRecvBuf_, "DIGEST-MD5" ) == true ) {
                pEntry = findCommandEntry( command_DIGESTMD5 );
                snprintf( pSendBuf_, BUFFER_SIZE, "AUTH DIGEST-MD5\r\n" );
                sendData( pEntry );
                receiveResponse( pEntry );

                std::string encoded_challenge = pRecvBuf_;
                encoded_challenge = encoded_challenge.substr( 4 );
                std::string decoded_challenge = skutils::tools::base64::decode( encoded_challenge );

                /////////////////////////////////////////////////////////////////////
                // Test data from RFC 2831
                // To test jump into authenticate and read this line and the ones down to next test
                // data section decoded_challenge =
                // "realm=\"elwood.innosoft.com\",nonce=\"OA6MG9tEQGm2hh\",qop=\"auth\",algorithm=md5-sess,charset=utf-8";
                /////////////////////////////////////////////////////////////////////

                // Get the nonce (manditory)
                int find = decoded_challenge.find( "nonce" );
                if ( find < 0 )
                    throw smtp_error_info( smtp_error_info::BAD_DIGEST_RESPONSE );
                std::string nonce = decoded_challenge.substr( find + 7 );
                find = nonce.find( "\"" );
                if ( find < 0 )
                    throw smtp_error_info( smtp_error_info::BAD_DIGEST_RESPONSE );
                nonce = nonce.substr( 0, find );

                // Get the realm (optional)
                std::string realm;
                find = decoded_challenge.find( "realm" );
                if ( find >= 0 ) {
                    realm = decoded_challenge.substr( find + 7 );
                    find = realm.find( "\"" );
                    if ( find < 0 )
                        throw smtp_error_info( smtp_error_info::BAD_DIGEST_RESPONSE );
                    realm = realm.substr( 0, find );
                }

                // Create a cnonce
                char cnonce[17], nc[9];
                snprintf( cnonce, 17, "%x", ( unsigned int ) time( nullptr ) );

                // Set nonce count
                snprintf( nc, 9, "%08d", 1 );

                // Set QOP
                std::string qop = "auth";

                // Get server address and set uri
                // Skip this step during test
#if ( defined WIN32 )
                int len;
#else
                socklen_t len;
#endif
                struct sockaddr_storage addr;
                len = sizeof addr;
                if ( !getpeername( socket_handle_, ( struct sockaddr* ) &addr, &len ) )
                    throw smtp_error_info( smtp_error_info::BAD_SERVER_NAME );

                struct sockaddr_in* s = ( struct sockaddr_in* ) &addr;
                std::string uri = inet_ntoa( s->sin_addr );
                uri = "smtp/" + uri;

                /////////////////////////////////////////////////////////////////////
                // test data from RFC 2831
                // strLogin_ = "chris";
                // strPassword_ = "secret";
                // snprintf(cnonce, 17, "OA6MHXh6VqTrRk" );
                // uri = "imap/elwood.innosoft.com";
                // Should form the response:
                //    charset=utf-8,username="chris",
                //    realm="elwood.innosoft.com",nonce="OA6MG9tEQGm2hh",nc=00000001,
                //    cnonce="OA6MHXh6VqTrRk",digest-uri="imap/elwood.innosoft.com",
                //    response=d388dad90d4bbd760a152321f2143af7,qop=auth
                // This encodes to:
                //    Y2hhcnNldD11dGYtOCx1c2VybmFtZT0iY2hyaXMiLHJlYWxtPSJlbHdvb2
                //    QuaW5ub3NvZnQuY29tIixub25jZT0iT0E2TUc5dEVRR20yaGgiLG5jPTAw
                //    MDAwMDAxLGNub25jZT0iT0E2TUhYaDZWcVRyUmsiLGRpZ2VzdC11cmk9Im
                //    ltYXAvZWx3b29kLmlubm9zb2Z0LmNvbSIscmVzcG9uc2U9ZDM4OGRhZDkw
                //    ZDRiYmQ3NjBhMTUyMzIxZjIxNDNhZjcscW9wPWF1dGg=
                /////////////////////////////////////////////////////////////////////

                // Calculate digest response
                unsigned char* ustrRealm = charToUnsignedChar( realm.c_str() );
                unsigned char* ustrUsername = charToUnsignedChar( strLogin_.c_str() );
                unsigned char* ustrPassword = charToUnsignedChar( strPassword_.c_str() );
                unsigned char* ustrNonce = charToUnsignedChar( nonce.c_str() );
                unsigned char* ustrCNonce = charToUnsignedChar( cnonce );
                unsigned char* ustrUri = charToUnsignedChar( uri.c_str() );
                unsigned char* ustrNc = charToUnsignedChar( nc );
                unsigned char* ustrQop = charToUnsignedChar( qop.c_str() );
                if ( !ustrRealm || !ustrUsername || !ustrPassword || !ustrNonce || !ustrCNonce ||
                     !ustrUri || !ustrNc || !ustrQop )
                    throw smtp_error_info( smtp_error_info::BAD_LOGIN_PASSWORD );

                skutils::tools::md5 md5a1a;
                md5a1a.update( ustrUsername, strLogin_.size() );
                md5a1a.update( ( unsigned char* ) ":", 1 );
                md5a1a.update( ustrRealm, realm.size() );
                md5a1a.update( ( unsigned char* ) ":", 1 );
                md5a1a.update( ustrPassword, strPassword_.size() );
                md5a1a.finalize();
                unsigned char* ua1 = md5a1a.raw_digest();

                skutils::tools::md5 md5a1b;
                md5a1b.update( ua1, 16 );
                md5a1b.update( ( unsigned char* ) ":", 1 );
                md5a1b.update( ustrNonce, nonce.size() );
                md5a1b.update( ( unsigned char* ) ":", 1 );
                md5a1b.update( ustrCNonce, strlen( cnonce ) );
                // authzid could be added here
                md5a1b.finalize();
                char* a1 = md5a1b.hex_digest();

                skutils::tools::md5 md5a2;
                md5a2.update( ( unsigned char* ) "AUTHENTICATE:", 13 );
                md5a2.update( ustrUri, uri.size() );
                // authint and authconf add an additional line here
                md5a2.finalize();
                char* a2 = md5a2.hex_digest();

                delete[] ua1;
                ua1 = charToUnsignedChar( a1 );
                unsigned char* ua2 = charToUnsignedChar( a2 );

                // compute KD
                skutils::tools::md5 md5;
                md5.update( ua1, 32 );
                md5.update( ( unsigned char* ) ":", 1 );
                md5.update( ustrNonce, nonce.size() );
                md5.update( ( unsigned char* ) ":", 1 );
                md5.update( ustrNc, strlen( nc ) );
                md5.update( ( unsigned char* ) ":", 1 );
                md5.update( ustrCNonce, strlen( cnonce ) );
                md5.update( ( unsigned char* ) ":", 1 );
                md5.update( ustrQop, qop.size() );
                md5.update( ( unsigned char* ) ":", 1 );
                md5.update( ua2, 32 );
                md5.finalize();
                decoded_challenge = md5.hex_digest();

                delete[] ustrRealm;
                delete[] ustrUsername;
                delete[] ustrPassword;
                delete[] ustrNonce;
                delete[] ustrCNonce;
                delete[] ustrUri;
                delete[] ustrNc;
                delete[] ustrQop;
                delete[] ua1;
                delete[] ua2;
                delete[] a1;
                delete[] a2;

                // send the response
                if ( strstr( pRecvBuf_, "charset" ) )
                    snprintf( pSendBuf_, BUFFER_SIZE, "charset=utf-8,username=\"%s\"",
                        strLogin_.c_str() );
                else
                    snprintf( pSendBuf_, BUFFER_SIZE, "username=\"%s\"", strLogin_.c_str() );
                if ( !realm.empty() ) {
                    snprintf( pRecvBuf_, BUFFER_SIZE, ",realm=\"%s\"", realm.c_str() );
                    strcat( pSendBuf_, pRecvBuf_ );
                }
                snprintf( pRecvBuf_, BUFFER_SIZE, ",nonce=\"%s\"", nonce.c_str() );
                strcat( pSendBuf_, pRecvBuf_ );
                snprintf( pRecvBuf_, BUFFER_SIZE, ",nc=%s", nc );
                strcat( pSendBuf_, pRecvBuf_ );
                snprintf( pRecvBuf_, BUFFER_SIZE, ",cnonce=\"%s\"", cnonce );
                strcat( pSendBuf_, pRecvBuf_ );
                snprintf( pRecvBuf_, BUFFER_SIZE, ",digest-uri=\"%s\"", uri.c_str() );
                strcat( pSendBuf_, pRecvBuf_ );
                snprintf( pRecvBuf_, BUFFER_SIZE, ",response=%s", decoded_challenge.c_str() );
                strcat( pSendBuf_, pRecvBuf_ );
                snprintf( pRecvBuf_, BUFFER_SIZE, ",qop=%s", qop.c_str() );
                strcat( pSendBuf_, pRecvBuf_ );
                unsigned char* ustrDigest = charToUnsignedChar( pSendBuf_ );
                encoded_challenge =
                    skutils::tools::base64::encode( ustrDigest, strlen( pSendBuf_ ) );
                delete[] ustrDigest;
                snprintf( pSendBuf_, BUFFER_SIZE, "%s\r\n", encoded_challenge.c_str() );
                pEntry = findCommandEntry( command_DIGESTMD5 );
                sendData( pEntry );
                receiveResponse( pEntry );

                // Send completion carraige return
                snprintf( pSendBuf_, BUFFER_SIZE, "\r\n" );
                pEntry = findCommandEntry( command_PASSWORD );
                sendData( pEntry );
                receiveResponse( pEntry );
            } else
                throw smtp_error_info( smtp_error_info::LOGIN_NOT_SUPPORTED );
        }
    } catch ( const smtp_error_info& ) {
        if ( pRecvBuf_[0] == '5' && pRecvBuf_[1] == '3' && pRecvBuf_[2] == '0' )
            bIsConnected_ = false;
        disconnectRemoteServer();
        throw;
        return false;
    }
    return true;
}

void client::disconnectRemoteServer() {
    if ( bIsConnected_ )
        sayQuit();
    if ( socket_handle_ ) {
#if ( defined WIN32 )
        closesocket( socket_handle_ );
#else
        close( socket_handle_ );
#endif
    }
    socket_handle_ = INVALID_SOCKET;
}

int client::smtpXYZdigits() {
    assert( pRecvBuf_ );
    if ( pRecvBuf_ == nullptr )
        return 0;
    return ( pRecvBuf_[0] - '0' ) * 100 + ( pRecvBuf_[1] - '0' ) * 10 + pRecvBuf_[2] - '0';
}

void client::formatHeader( char* header ) {
    char month[][4] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    size_t i;
    std::string to;
    std::string cc;
    std::string bcc;
    time_t rawtime;
    struct tm* timeinfo;

    // date/time check
    if ( time( &rawtime ) > 0 )
        timeinfo = localtime( &rawtime );
    else
        throw smtp_error_info( smtp_error_info::TIME_ERROR );

    // check for at least one recipient
    if ( vecRecipients_.size() ) {
        for ( i = 0; i < vecRecipients_.size(); i++ ) {
            if ( i > 0 )
                to.append( "," );
            to += vecRecipients_[i].name_;
            to.append( "<" );
            to += vecRecipients_[i].mail_;
            to.append( ">" );
        }
    } else
        throw smtp_error_info( smtp_error_info::UNDEF_RECIPIENTS );

    if ( vecCCRecipients_.size() ) {
        for ( i = 0; i < vecCCRecipients_.size(); i++ ) {
            if ( i > 0 )
                cc.append( "," );
            cc += vecCCRecipients_[i].name_;
            cc.append( "<" );
            cc += vecCCRecipients_[i].mail_;
            cc.append( ">" );
        }
    }

    // Date: <SP> <dd> <SP> <mon> <SP> <yy> <SP> <hh> ":" <mm> ":" <ss> <SP> <zone> <CRLF>
    snprintf( header, BUFFER_SIZE, "Date: %d %s %d %d:%d:%d\r\n", timeinfo->tm_mday,
        month[timeinfo->tm_mon], timeinfo->tm_year + 1900, timeinfo->tm_hour, timeinfo->tm_min,
        timeinfo->tm_sec );

    // From: <SP> <sender>  <SP> "<" <sender-email> ">" <CRLF>
    if ( !strMailFrom_.size() )
        throw smtp_error_info( smtp_error_info::UNDEF_MAIL_FROM );

    strcat( header, "From: " );
    if ( strNameFrom_.size() )
        strcat( header, strNameFrom_.c_str() );

    strcat( header, " <" );
    strcat( header, strMailFrom_.c_str() );
    strcat( header, ">\r\n" );

    // X-Mailer: <SP> <xmailer-app> <CRLF>
    if ( strXMailer_.size() ) {
        strcat( header, "X-Mailer: " );
        strcat( header, strXMailer_.c_str() );
        strcat( header, "\r\n" );
    }

    // Reply-To: <SP> <reverse-path> <CRLF>
    if ( strReplyTo_.size() ) {
        strcat( header, "Reply-To: " );
        strcat( header, strReplyTo_.c_str() );
        strcat( header, "\r\n" );
    }

    // Disposition-Notification-To: <SP> <reverse-path or sender-email> <CRLF>
    if ( bReadReceipt_ ) {
        strcat( header, "Disposition-Notification-To: " );
        if ( strReplyTo_.size() )
            strcat( header, strReplyTo_.c_str() );
        else
            strcat( header, strNameFrom_.c_str() );
        strcat( header, "\r\n" );
    }

    // X-Priority: <SP> <number> <CRLF>
    switch ( eXPriority_ ) {
    case XPRIORITY_HIGH:
        strcat( header, "X-Priority: 2 (High)\r\n" );
        break;
    case XPRIORITY_NORMAL:
        strcat( header, "X-Priority: 3 (Normal)\r\n" );
        break;
    case XPRIORITY_LOW:
        strcat( header, "X-Priority: 4 (Low)\r\n" );
        break;
    default:
        strcat( header, "X-Priority: 3 (Normal)\r\n" );
        break;
    }

    // To: <SP> <remote-user-mail> <CRLF>
    strcat( header, "To: " );
    strcat( header, to.c_str() );
    strcat( header, "\r\n" );

    // Cc: <SP> <remote-user-mail> <CRLF>
    if ( vecCCRecipients_.size() ) {
        strcat( header, "Cc: " );
        strcat( header, cc.c_str() );
        strcat( header, "\r\n" );
    }

    if ( vecBvecCCRecipients__.size() ) {
        strcat( header, "Bcc: " );
        strcat( header, bcc.c_str() );
        strcat( header, "\r\n" );
    }

    // Subject: <SP> <subject-text> <CRLF>
    if ( !strSubject_.size() )
        strcat( header, "Subject:  " );
    else {
        strcat( header, "Subject: " );
        strcat( header, strSubject_.c_str() );
    }
    strcat( header, "\r\n" );

    // MIME-Version: <SP> 1.0 <CRLF>
    strcat( header, "MIME-Version: 1.0\r\n" );
    if ( !vecAttachments_.size() ) {  // no attachments
        if ( bIsHTML_ )
            strcat( header, "Content-Type: text/html; charset=\"" );
        else
            strcat( header, "Content-type: text/plain; charset=\"" );
        strcat( header, strCharSet_.c_str() );
        strcat( header, "\"\r\n" );
        strcat( header, "Content-Transfer-Encoding: 7bit\r\n" );
        strcat( pSendBuf_, "\r\n" );
    } else {  // there is one or more attachments
        strcat( header, "Content-Type: multipart/mixed; boundary=\"" );
        strcat( header, BOUNDARY_TEXT );
        strcat( header, "\"\r\n" );
        strcat( header, "\r\n" );
        // first goes text message
        strcat( pSendBuf_, "--" );
        strcat( pSendBuf_, BOUNDARY_TEXT );
        strcat( pSendBuf_, "\r\n" );
        if ( bIsHTML_ )
            strcat( pSendBuf_, "Content-type: text/html; charset=" );
        else
            strcat( pSendBuf_, "Content-type: text/plain; charset=" );
        strcat( header, strCharSet_.c_str() );
        strcat( header, "\r\n" );
        strcat( pSendBuf_, "Content-Transfer-Encoding: 7bit\r\n" );
        strcat( pSendBuf_, "\r\n" );
    }

    // done
}

void client::receiveData( command_entry* pEntry ) {
    if ( ssl_handle_ != nullptr ) {
        receiveData_SSL( ssl_handle_, pEntry );
        return;
    }
    int res = 0;
    fd_set fdread;
    timeval time;

    time.tv_sec = pEntry->recv_timeout;
    time.tv_usec = 0;

    assert( pRecvBuf_ );

    if ( pRecvBuf_ == nullptr )
        throw smtp_error_info( smtp_error_info::RECVBUF_IS_EMPTY );

    FD_ZERO( &fdread );

    FD_SET( socket_handle_, &fdread );

    if ( ( res = select( socket_handle_ + 1, &fdread, nullptr, nullptr, &time ) ) ==
         SOCKET_ERROR ) {
        FD_CLR( socket_handle_, &fdread );
        throw smtp_error_info( smtp_error_info::WSA_SELECT );
    }

    if ( !res ) {
        // timeout
        FD_CLR( socket_handle_, &fdread );
        throw smtp_error_info( smtp_error_info::SERVER_NOT_RESPONDING );
    }

    if ( FD_ISSET( socket_handle_, &fdread ) ) {
        res = recv( socket_handle_, pRecvBuf_, BUFFER_SIZE, 0 );
        if ( res == SOCKET_ERROR ) {
            FD_CLR( socket_handle_, &fdread );
            throw smtp_error_info( smtp_error_info::WSA_RECV );
        }
    }
    FD_CLR( socket_handle_, &fdread );
    pRecvBuf_[res] = 0;
    if ( res == 0 )
        throw smtp_error_info( smtp_error_info::CONNECTION_CLOSED );
}

void client::sendData( command_entry* pEntry ) {
    if ( ssl_handle_ != nullptr ) {
        sendData_SSL( ssl_handle_, pEntry );
        return;
    }
    int idx = 0, nLeft = strlen( pSendBuf_ );
    fd_set fdwrite;
    timeval time;

    time.tv_sec = pEntry->send_timeout;
    time.tv_usec = 0;

    assert( pSendBuf_ );

    if ( pSendBuf_ == nullptr )
        throw smtp_error_info( smtp_error_info::SENDBUF_IS_EMPTY );

    while ( nLeft > 0 ) {
        FD_ZERO( &fdwrite );
        FD_SET( socket_handle_, &fdwrite );
        int res;
        if ( ( res = select( socket_handle_ + 1, nullptr, &fdwrite, nullptr, &time ) ) ==
             SOCKET_ERROR ) {
            FD_CLR( socket_handle_, &fdwrite );
            throw smtp_error_info( smtp_error_info::WSA_SELECT );
        }

        if ( !res ) {
            // timeout
            FD_CLR( socket_handle_, &fdwrite );
            throw smtp_error_info( smtp_error_info::SERVER_NOT_RESPONDING );
        }

        if ( res && FD_ISSET( socket_handle_, &fdwrite ) ) {
            res = ::send( socket_handle_, &pSendBuf_[idx], nLeft, 0 );
            if ( res == SOCKET_ERROR || res == 0 ) {
                FD_CLR( socket_handle_, &fdwrite );
                throw smtp_error_info( smtp_error_info::WSA_SEND );
            }
            nLeft -= res;
            idx += res;
        }
    }
    OutputDebugStringA( pSendBuf_ );
    FD_CLR( socket_handle_, &fdwrite );
}

const char* client::getLocalHostName() {
    return strLocalHostName_.c_str();
}
unsigned int client::getRecipientCount() const {
    return vecRecipients_.size();
}
unsigned int client::getBCCRecipientCount() const {
    return vecBvecCCRecipients__.size();
}
unsigned int client::getCCRecipientCount() const {
    return vecCCRecipients_.size();
}
const char* client::getReplyTo() const {
    return strReplyTo_.c_str();
}
const char* client::getMailFrom() const {
    return strMailFrom_.c_str();
}
const char* client::getSenderName() const {
    return strNameFrom_.c_str();
}
const char* client::getSubject() const {
    return strSubject_.c_str();
}
const char* client::getXMailer() const {
    return strXMailer_.c_str();
}
smtp_x_priority client::getXPriority() const {
    return eXPriority_;
}
const char* client::getMsgLineText( unsigned int Line ) const {
    if ( Line >= msgBody_.size() )
        throw smtp_error_info( smtp_error_info::OUT_OF_MSG_RANGE );
    return msgBody_.at( Line ).c_str();
}
unsigned int client::getMsgLines() const {
    return msgBody_.size();
}
void client::setCharSet( const char* sCharSet ) {
    strCharSet_ = sCharSet;
}
void client::setLocalHostName( const char* sLocalHostName ) {
    strLocalHostName_ = sLocalHostName;
}
void client::setXPriority( smtp_x_priority priority ) {
    eXPriority_ = priority;
}
void client::setReplyTo( const char* ReplyTo ) {
    strReplyTo_ = ReplyTo;
}
void client::setReadReceipt( bool requestReceipt /* = true */ ) {
    bReadReceipt_ = requestReceipt;
}
void client::setSenderMail( const char* email ) {
    strMailFrom_ = email;
}
void client::setSenderName( const char* name ) {
    strNameFrom_ = name;
}
void client::setSubject( const char* Subject ) {
    strSubject_ = Subject;
}
void client::setXMailer( const char* XMailer ) {
    strXMailer_ = XMailer;
}
void client::setLogin( const char* Login ) {
    strLogin_ = Login;
}
void client::setPassword( const char* Password ) {
    strPassword_ = Password;
}
void client::setSMTPServer(
    const char* SrvName, const unsigned short SrvPort, bool b_authenticate ) {
    nSMTPServerPort_ = SrvPort;
    strSMTPSrvName_ = SrvName;
    bAuthenticate_ = b_authenticate;
}

std::string smtp_error_info::GetErrorText() const {
    switch ( ErrorCode ) {
    case smtp_error_info::CSMTP_NO_ERROR:
        return "";
    case smtp_error_info::WSA_STARTUP:
        return "Unable to initialise winsock2";
    case smtp_error_info::WSA_VER:
        return "Wrong version of the winsock2";
    case smtp_error_info::WSA_SEND:
        return "Function send() failed";
    case smtp_error_info::WSA_RECV:
        return "Function recv() failed";
    case smtp_error_info::WSA_CONNECT:
        return "Function connect failed";
    case smtp_error_info::WSA_GETHOSTBY_NAME_ADDR:
        return "Unable to determine remote server";
    case smtp_error_info::WSA_INVALID_SOCKET:
        return "Invalid winsock2 socket";
    case smtp_error_info::WSA_HOSTNAME:
        return "Function hostname() failed";
    case smtp_error_info::WSA_IOCTLSOCKET:
        return "Function ioctlsocket() failed";
    case smtp_error_info::BAD_IPV4_ADDR:
        return "Improper IPv4 address";
    case smtp_error_info::UNDEF_MSG_HEADER:
        return "Undefined message header";
    case smtp_error_info::UNDEF_MAIL_FROM:
        return "Undefined mail sender";
    case smtp_error_info::UNDEF_SUBJECT:
        return "Undefined message subject";
    case smtp_error_info::UNDEF_RECIPIENTS:
        return "Undefined at least one reciepent";
    case smtp_error_info::UNDEF_RECIPIENT_MAIL:
        return "Undefined recipent mail";
    case smtp_error_info::UNDEF_LOGIN:
        return "Undefined user login";
    case smtp_error_info::UNDEF_PASSWORD:
        return "Undefined user password";
    case smtp_error_info::BAD_LOGIN_PASSWORD:
        return "Invalid user login or password";
    case smtp_error_info::BAD_DIGEST_RESPONSE:
        return "Server returned a bad digest MD5 response";
    case smtp_error_info::BAD_SERVER_NAME:
        return "Unable to determine server name for digest MD5 response";
    case smtp_error_info::COMMAND_MAIL_FROM:
        return "Server returned error after sending MAIL FROM";
    case smtp_error_info::COMMAND_EHLO:
        return "Server returned error after sending EHLO";
    case smtp_error_info::COMMAND_AUTH_PLAIN:
        return "Server returned error after sending AUTH PLAIN";
    case smtp_error_info::COMMAND_AUTH_LOGIN:
        return "Server returned error after sending AUTH LOGIN";
    case smtp_error_info::COMMAND_AUTH_CRAMMD5:
        return "Server returned error after sending AUTH CRAM-MD5";
    case smtp_error_info::COMMAND_AUTH_DIGESTMD5:
        return "Server returned error after sending AUTH DIGEST-MD5";
    case smtp_error_info::COMMAND_DIGESTMD5:
        return "Server returned error after sending MD5 DIGEST";
    case smtp_error_info::COMMAND_DATA:
        return "Server returned error after sending DATA";
    case smtp_error_info::COMMAND_QUIT:
        return "Server returned error after sending QUIT";
    case smtp_error_info::COMMAND_RCPT_TO:
        return "Server returned error after sending RCPT TO";
    case smtp_error_info::MSG_BODY_ERROR:
        return "Error in message body";
    case smtp_error_info::CONNECTION_CLOSED:
        return "Server has closed the connection";
    case smtp_error_info::SERVER_NOT_READY:
        return "Server is not ready";
    case smtp_error_info::SERVER_NOT_RESPONDING:
        return "Server not responding";
    case smtp_error_info::FILE_NOT_EXIST:
        return "Attachment file does not exist";
    case smtp_error_info::MSG_TOO_BIG:
        return "Message is too big";
    case smtp_error_info::BAD_LOGIN_PASS:
        return "Bad login or password";
    case smtp_error_info::UNDEF_XYZ_RESPONSE:
        return "Undefined xyz SMTP response";
    case smtp_error_info::LACK_OF_MEMORY:
        return "Lack of memory";
    case smtp_error_info::TIME_ERROR:
        return "time() error";
    case smtp_error_info::RECVBUF_IS_EMPTY:
        return "pRecvBuf_ is empty";
    case smtp_error_info::SENDBUF_IS_EMPTY:
        return "pSendBuf_ is empty";
    case smtp_error_info::OUT_OF_MSG_RANGE:
        return "Specified line number is out of message size";
    case smtp_error_info::COMMAND_EHLO_STARTTLS:
        return "Server returned error after sending STARTTLS";
    case smtp_error_info::SSL_PROBLEM:
        return "SSL problem";
    case smtp_error_info::COMMAND_DATABLOCK:
        return "Failed to send data block";
    case smtp_error_info::STARTTLS_NOT_SUPPORTED:
        return "The STARTTLS command is not supported by the server";
    case smtp_error_info::LOGIN_NOT_SUPPORTED:
        return "AUTH LOGIN is not supported by the server";
    default:
        return "Undefined error id";
    }
}

void client::sayHello() {
    command_entry* pEntry = findCommandEntry( command_EHLO );
    snprintf( pSendBuf_, BUFFER_SIZE, "EHLO %s\r\n",
        getLocalHostName() != nullptr ? strLocalHostName_.c_str() : "domain" );
    sendData( pEntry );
    receiveResponse( pEntry );
    bIsConnected_ = true;
}
void client::sayQuit() {
    // ***** CLOSING CONNECTION *****
    command_entry* pEntry = findCommandEntry( command_QUIT );
    // QUIT <CRLF>
    snprintf( pSendBuf_, BUFFER_SIZE, "QUIT\r\n" );
    bIsConnected_ = false;
    sendData( pEntry );
    receiveResponse( pEntry );
}

void client::startTls() {
    if ( isKeywordSupported( pRecvBuf_, "STARTTLS" ) == false )
        throw smtp_error_info( smtp_error_info::STARTTLS_NOT_SUPPORTED );
    command_entry* pEntry = findCommandEntry( command_STARTTLS );
    snprintf( pSendBuf_, BUFFER_SIZE, "STARTTLS\r\n" );
    sendData( pEntry );
    receiveResponse( pEntry );
    connectOpenSSL();
}
void client::receiveData_SSL( SSL* ssl, command_entry* pEntry ) {
    int res = 0;
    int offset = 0;
    fd_set fdread;
    fd_set fdwrite;
    timeval time;

    int read_blocked_on_write = 0;

    time.tv_sec = pEntry->recv_timeout;
    time.tv_usec = 0;

    assert( pRecvBuf_ );

    if ( pRecvBuf_ == nullptr )
        throw smtp_error_info( smtp_error_info::RECVBUF_IS_EMPTY );

    bool bFinish = false;
    while ( !bFinish ) {
        FD_ZERO( &fdread );
        FD_ZERO( &fdwrite );
        FD_SET( socket_handle_, &fdread );
        if ( read_blocked_on_write ) {
            FD_SET( socket_handle_, &fdwrite );
        }
        if ( ( res = select( socket_handle_ + 1, &fdread, &fdwrite, nullptr, &time ) ) ==
             SOCKET_ERROR ) {
            FD_ZERO( &fdread );
            FD_ZERO( &fdwrite );
            throw smtp_error_info( smtp_error_info::WSA_SELECT );
        }
        if ( !res ) {
            // timeout
            FD_ZERO( &fdread );
            FD_ZERO( &fdwrite );
            throw smtp_error_info( smtp_error_info::SERVER_NOT_RESPONDING );
        }
        if ( FD_ISSET( socket_handle_, &fdread ) ||
             ( read_blocked_on_write && FD_ISSET( socket_handle_, &fdwrite ) ) ) {
            while ( 1 ) {
                read_blocked_on_write = 0;
                const int buff_len = 1024;
                char buff[buff_len];
                res = SSL_read( ssl, buff, buff_len );
                int ssl_err = SSL_get_error( ssl, res );
                if ( ssl_err == SSL_ERROR_NONE ) {
                    if ( offset + res > BUFFER_SIZE - 1 ) {
                        FD_ZERO( &fdread );
                        FD_ZERO( &fdwrite );
                        throw smtp_error_info( smtp_error_info::LACK_OF_MEMORY );
                    }
                    memcpy( pRecvBuf_ + offset, buff, res );
                    offset += res;
                    if ( SSL_pending( ssl ) ) {
                        continue;
                    } else {
                        bFinish = true;
                        break;
                    }
                } else if ( ssl_err == SSL_ERROR_ZERO_RETURN ) {
                    bFinish = true;
                    break;
                } else if ( ssl_err == SSL_ERROR_WANT_READ ) {
                    break;
                } else if ( ssl_err == SSL_ERROR_WANT_WRITE ) {
                    // We get a WANT_WRITE if we're trying to rehandshake and we block on a write
                    // during that rehandshake. We need to wait on the socket to be writeable but
                    // reinitiate the read when it is
                    read_blocked_on_write = 1;
                    break;
                } else {
                    FD_ZERO( &fdread );
                    FD_ZERO( &fdwrite );
                    throw smtp_error_info( smtp_error_info::SSL_PROBLEM );
                }
            }
        }
    }
    FD_ZERO( &fdread );
    FD_ZERO( &fdwrite );
    pRecvBuf_[offset] = 0;
    if ( offset == 0 )
        throw smtp_error_info( smtp_error_info::CONNECTION_CLOSED );
}

void client::receiveResponse( command_entry* pEntry ) {
    std::string line;
    int reply_code = 0;
    bool bFinish = false;
    while ( !bFinish ) {
        receiveData( pEntry );
        line.append( pRecvBuf_ );
        size_t len = line.length();
        size_t begin = 0;
        size_t offset = 0;
        while ( 1 ) {  // loop for all lines
            while ( offset + 1 < len ) {
                if ( line[offset] == '\r' && line[offset + 1] == '\n' )
                    break;
                ++offset;
            }
            if ( offset + 1 < len ) {  // we found a line
                // see if this is the last line
                // the last line must match the pattern: XYZ<SP>*<CRLF> or XYZ<CRLF> where XYZ is a
                // string of 3 digits
                offset += 2;  // skip <CRLF>
                if ( offset - begin >= 5 ) {
                    if ( isdigit( line[begin] ) && isdigit( line[begin + 1] ) &&
                         isdigit( line[begin + 2] ) ) {
                        // this is the last line
                        if ( offset - begin == 5 || line[begin + 3] == ' ' ) {
                            reply_code = ( line[begin] - '0' ) * 100 +
                                         ( line[begin + 1] - '0' ) * 10 + line[begin + 2] - '0';
                            bFinish = true;
                            break;
                        }
                    }
                }
                begin = offset;  // try to find next line
            } else  // we haven't received the last line, so we need to receive more data
                break;
        }
    }
    snprintf( pRecvBuf_, BUFFER_SIZE, "%s", line.c_str() );
    OutputDebugStringA( pRecvBuf_ );
    if ( reply_code != pEntry->valid_reply_code )
        throw smtp_error_info( pEntry->error );
}

void client::sendData_SSL( SSL* ssl, command_entry* pEntry ) {
    int offset = 0, res, nLeft = strlen( pSendBuf_ );
    fd_set fdwrite;
    fd_set fdread;
    timeval time;
    int write_blocked_on_read = 0;
    time.tv_sec = pEntry->send_timeout;
    time.tv_usec = 0;
    assert( pSendBuf_ );
    if ( pSendBuf_ == nullptr )
        throw smtp_error_info( smtp_error_info::SENDBUF_IS_EMPTY );
    while ( nLeft > 0 ) {
        FD_ZERO( &fdwrite );
        FD_ZERO( &fdread );
        FD_SET( socket_handle_, &fdwrite );
        if ( write_blocked_on_read ) {
            FD_SET( socket_handle_, &fdread );
        }
        if ( ( res = select( socket_handle_ + 1, &fdread, &fdwrite, nullptr, &time ) ) ==
             SOCKET_ERROR ) {
            FD_ZERO( &fdwrite );
            FD_ZERO( &fdread );
            throw smtp_error_info( smtp_error_info::WSA_SELECT );
        }
        if ( !res ) {  // timeout
            FD_ZERO( &fdwrite );
            FD_ZERO( &fdread );
            throw smtp_error_info( smtp_error_info::SERVER_NOT_RESPONDING );
        }
        if ( FD_ISSET( socket_handle_, &fdwrite ) ||
             ( write_blocked_on_read && FD_ISSET( socket_handle_, &fdread ) ) ) {
            write_blocked_on_read = 0;
            res = SSL_write( ssl, pSendBuf_ + offset, nLeft );  // Try to write
            switch ( SSL_get_error( ssl, res ) ) {
            case SSL_ERROR_NONE:  // We wrote something
                nLeft -= res;
                offset += res;
                break;
            case SSL_ERROR_WANT_WRITE:  // We would have blocked
                break;
                // We get a WANT_READ if we're trying to rehandshake and we block on write during
                // the current connection. We need to wait on the socket to be readable but
                // reinitiate our write when it is
            case SSL_ERROR_WANT_READ:
                write_blocked_on_read = 1;
                break;
            default:  // Some other error
                FD_ZERO( &fdread );
                FD_ZERO( &fdwrite );
                throw smtp_error_info( smtp_error_info::SSL_PROBLEM );
            }
        }
    }
    OutputDebugStringA( pSendBuf_ );
    FD_ZERO( &fdwrite );
    FD_ZERO( &fdread );
}

void client::initOpenSSL() {
    if ( bCareAboutOpenSSLInitAndShutdown_ ) {
        SSL_library_init();
        SSL_load_error_strings();
    }
    ssl_ctx_ = SSL_CTX_new( SSLv23_client_method() );
    if ( ssl_ctx_ == nullptr )
        throw smtp_error_info( smtp_error_info::SSL_PROBLEM );
}
void client::connectOpenSSL() {
    if ( ssl_ctx_ == nullptr )
        throw smtp_error_info( smtp_error_info::SSL_PROBLEM );
    ssl_handle_ = SSL_new( ssl_ctx_ );
    if ( ssl_handle_ == nullptr )
        throw smtp_error_info( smtp_error_info::SSL_PROBLEM );
    SSL_set_fd( ssl_handle_, ( int ) socket_handle_ );
    SSL_set_mode( ssl_handle_, SSL_MODE_AUTO_RETRY );
    int res = 0;
    fd_set fdwrite;
    fd_set fdread;
    int write_blocked = 0;
    int read_blocked = 0;
    timeval time;
    time.tv_sec = TIME_IN_SEC;
    time.tv_usec = 0;
    while ( 1 ) {
        FD_ZERO( &fdwrite );
        FD_ZERO( &fdread );
        if ( write_blocked )
            FD_SET( socket_handle_, &fdwrite );
        if ( read_blocked )
            FD_SET( socket_handle_, &fdread );
        if ( write_blocked || read_blocked ) {
            write_blocked = 0;
            read_blocked = 0;
            if ( ( res = select( socket_handle_ + 1, &fdread, &fdwrite, nullptr, &time ) ) ==
                 SOCKET_ERROR ) {
                FD_ZERO( &fdwrite );
                FD_ZERO( &fdread );
                throw smtp_error_info( smtp_error_info::WSA_SELECT );
            }
            if ( !res ) {  // timeout
                FD_ZERO( &fdwrite );
                FD_ZERO( &fdread );
                throw smtp_error_info( smtp_error_info::SERVER_NOT_RESPONDING );
            }
        }
        res = SSL_connect( ssl_handle_ );
        switch ( SSL_get_error( ssl_handle_, res ) ) {
        case SSL_ERROR_NONE:
            FD_ZERO( &fdwrite );
            FD_ZERO( &fdread );
            return;
        case SSL_ERROR_WANT_WRITE:
            write_blocked = 1;
            break;
        case SSL_ERROR_WANT_READ:
            read_blocked = 1;
            break;
        default:
            FD_ZERO( &fdwrite );
            FD_ZERO( &fdread );
            throw smtp_error_info( smtp_error_info::SSL_PROBLEM );
        }
    }
}
void client::cleanupOpenSSL() {
    if ( ssl_handle_ != nullptr ) {
        SSL_shutdown( ssl_handle_ ); /* send SSL/TLS close_notify */
        SSL_free( ssl_handle_ );
        ssl_handle_ = nullptr;
    }
    if ( ssl_ctx_ != nullptr ) {
        SSL_CTX_free( ssl_ctx_ );
        ssl_ctx_ = nullptr;
    }
    if ( bCareAboutOpenSSLInitAndShutdown_ ) {
        // ERR_remove_state( 0 );
        ERR_free_strings();
        EVP_cleanup();
        CRYPTO_cleanup_all_ex_data();
    }
}

};  // namespace mail
};  // namespace skutils
