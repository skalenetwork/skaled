#if ( !defined SKUTILS_MAIL_H )
#define SKUTILS_MAIL_H 1

#include <assert.h>
#include <openssl/ssl.h>
#include <stdint.h>
#include <string.h>
#include <vector>

#if ( defined WIN32 )
#include <time.h>
#include <winsock2.h>
#pragma comment( lib, "ws2_32.lib" )
#if _MSC_VER < 1400
#define snprintf _snprintf
#else
#define snprintf sprintf_s
#endif
#else  /// (defined WIN32)
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#endif  /// else from (defined WIN32)

namespace skutils {
namespace mail {
// based on https://www.codeproject.com/Articles/98355/SMTP-Client-with-SSL-TLS

#if ( defined WIN32 )
#else   /// (defined WIN32)
typedef uint16_t WORD;
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct hostent* LPHOSTENT;
typedef struct servent* LPSERVENT;
typedef struct in_addr* LPIN_ADDR;
typedef struct sockaddr* LPSOCKADDR;
#endif  /// else from (defined WIN32)

const char BOUNDARY_TEXT[] = "__MESSAGE__ID__54yg6f6h6y456345";

enum smtp_x_priority {
    XPRIORITY_HIGH = 2,
    XPRIORITY_NORMAL = 3,
    XPRIORITY_LOW = 4
};  /// enum smtp_x_priority
smtp_x_priority str_2_smtp_x_priority( const std::string& s );
std::string smtp_x_priority_2_str( smtp_x_priority sxp );

class smtp_error_info {
public:
    enum smtp_error {
        CSMTP_NO_ERROR = 0,
        WSA_STARTUP = 100,  // WSAGetLastError()
        WSA_VER,
        WSA_SEND,
        WSA_RECV,
        WSA_CONNECT,
        WSA_GETHOSTBY_NAME_ADDR,
        WSA_INVALID_SOCKET,
        WSA_HOSTNAME,
        WSA_IOCTLSOCKET,
        WSA_SELECT,
        BAD_IPV4_ADDR,
        UNDEF_MSG_HEADER = 200,
        UNDEF_MAIL_FROM,
        UNDEF_SUBJECT,
        UNDEF_RECIPIENTS,
        UNDEF_LOGIN,
        UNDEF_PASSWORD,
        BAD_LOGIN_PASSWORD,
        BAD_DIGEST_RESPONSE,
        BAD_SERVER_NAME,
        UNDEF_RECIPIENT_MAIL,
        COMMAND_MAIL_FROM = 300,
        COMMAND_EHLO,
        COMMAND_AUTH_PLAIN,
        COMMAND_AUTH_LOGIN,
        COMMAND_AUTH_CRAMMD5,
        COMMAND_AUTH_DIGESTMD5,
        COMMAND_DIGESTMD5,
        COMMAND_DATA,
        COMMAND_QUIT,
        COMMAND_RCPT_TO,
        MSG_BODY_ERROR,
        CONNECTION_CLOSED = 400,  // by server
        SERVER_NOT_READY,         // remote server
        SERVER_NOT_RESPONDING,
        SELECT_TIMEOUT,
        FILE_NOT_EXIST,
        MSG_TOO_BIG,
        BAD_LOGIN_PASS,
        UNDEF_XYZ_RESPONSE,
        LACK_OF_MEMORY,
        TIME_ERROR,
        RECVBUF_IS_EMPTY,
        SENDBUF_IS_EMPTY,
        OUT_OF_MSG_RANGE,
        COMMAND_EHLO_STARTTLS,
        SSL_PROBLEM,
        COMMAND_DATABLOCK,
        STARTTLS_NOT_SUPPORTED,
        LOGIN_NOT_SUPPORTED
    };  /// enum smtp_error
    smtp_error_info( smtp_error err_ ) : ErrorCode( err_ ) {}
    smtp_error GetErrorNum() const { return ErrorCode; }
    std::string GetErrorText() const;

private:
    smtp_error ErrorCode;
};  /// class smtp_error_info

enum SMTP_COMMAND {
    command_INIT,
    command_EHLO,
    command_AUTHPLAIN,
    command_AUTHLOGIN,
    command_AUTHCRAMMD5,
    command_AUTHDIGESTMD5,
    command_DIGESTMD5,
    command_USER,
    command_PASSWORD,
    command_MAILFROM,
    command_RCPTTO,
    command_DATA,
    command_DATABLOCK,
    command_DATAEND,
    command_QUIT,
    command_STARTTLS
};  /// enum SMTP_COMMAND

enum smtp_security_type {  // TLS/SSL extension
    NO_SECURITY,
    USE_TLS,
    USE_SSL,
    DO_NOT_SET
};  /// enum smtp_security_type
smtp_security_type str_2_smtp_security_type( const std::string& s );
std::string smtp_security_type_2_str( smtp_security_type sst );

typedef struct tag_command_entry {
    SMTP_COMMAND command;
    int send_timeout;      // 0 means no send is required
    int recv_timeout;      // 0 means no recv is required
    int valid_reply_code;  // 0 means no recv is required, so no reply code
    smtp_error_info::smtp_error error;
} command_entry;

class client {
public:
    client( bool bCareAboutOpenSSLInitAndShutdown = false );
    virtual ~client();
    void addRecipient( const char* email, const char* name = nullptr );
    void addBCCRecipient( const char* email, const char* name = nullptr );
    void addCCRecipient( const char* email, const char* name = nullptr );
    void addAttachment( const char* path );
    void addMsgLine( const char* text );
    void addMsgLines( const char* text );
    void clearMessage();
    bool connectRemoteServer( const char* strServer, const unsigned short anPort = 0,
        smtp_security_type securityType = DO_NOT_SET, bool b_authenticate = true,
        const char* login = nullptr, const char* password = nullptr );
    void disconnectRemoteServer();
    void delRecipients();
    void delvecBvecCCRecipients__();
    void delvecCCRecipients_();
    void delvecAttachments_();
    void delMsgLines();
    void delMsgLine( unsigned int line );
    void modMsgLine( unsigned int line, const char* text );
    unsigned int getBCCRecipientCount() const;
    unsigned int getCCRecipientCount() const;
    unsigned int getRecipientCount() const;
    const char* getLocalHostIP() const;
    const char* getLocalHostName();
    const char* getMsgLineText( unsigned int line ) const;
    unsigned int getMsgLines() const;
    const char* getReplyTo() const;
    const char* getMailFrom() const;
    const char* getSenderName() const;
    const char* getSubject() const;
    const char* getXMailer() const;
    smtp_x_priority getXPriority() const;
    void send();
    void setCharSet( const char* sCharSet );
    void setLocalHostName( const char* sLocalHostName );
    void setSubject( const char* );
    void setSenderName( const char* );
    void setSenderMail( const char* );
    void setReplyTo( const char* );
    void setReadReceipt( bool requestReceipt = true );
    void setXMailer( const char* );
    void setLogin( const char* );
    void setPassword( const char* );
    void setXPriority( smtp_x_priority );
    void setSMTPServer(
        const char* server, const unsigned short port = 0, bool b_authenticate = true );

private:
    std::string strLocalHostName_;
    std::string strMailFrom_;
    std::string strNameFrom_;
    std::string strSubject_;
    std::string strCharSet_;
    std::string strXMailer_;
    std::string strReplyTo_;
    bool bReadReceipt_;
    std::string strIPAddress_;
    std::string strLogin_;
    std::string strPassword_;
    std::string strSMTPSrvName_;
    unsigned short nSMTPServerPort_;
    bool bAuthenticate_;
    smtp_x_priority eXPriority_;
    char* pSendBuf_;
    char* pRecvBuf_;
    //
    SOCKET socket_handle_;
    bool bIsConnected_;
    //
    struct Recipient {
        std::string name_;
        std::string mail_;
    };

    std::vector< Recipient > vecRecipients_;
    std::vector< Recipient > vecCCRecipients_;
    std::vector< Recipient > vecBvecCCRecipients__;
    std::vector< std::string > vecAttachments_;
    std::vector< std::string > msgBody_;
    void receiveData( command_entry* pEntry );
    void sendData( command_entry* pEntry );
    void formatHeader( char* );
    int smtpXYZdigits();
    void sayHello();
    void sayQuit();
    // TLS/SSL extension
public:
    smtp_security_type getSecurityType() const { return security_type_; }
    void setSecurityType( smtp_security_type st ) { security_type_ = st; }
    bool bIsHTML_;

private:
    smtp_security_type security_type_;
    SSL_CTX* ssl_ctx_;
    SSL* ssl_handle_;
    bool bCareAboutOpenSSLInitAndShutdown_;
    void receiveResponse( command_entry* pEntry );
    void initOpenSSL();
    void connectOpenSSL();
    void cleanupOpenSSL();
    void receiveData_SSL( SSL* ssl, command_entry* pEntry );
    void sendData_SSL( SSL* ssl, command_entry* pEntry );
    void startTls();
};  /// class client

};  // namespace mail
};  // namespace skutils

#endif  /// (!defined SKUTILS_MAIL_H)
