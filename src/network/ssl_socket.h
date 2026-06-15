#ifndef SSL_SOCKET_H
#define SSL_SOCKET_H

#include "tcp_socket.h"
#include <string>

#ifdef _WIN32
    #include <winsock2.h>
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>

class SslSocket {
public:
    SslSocket();
    ~SslSocket();

    // Disable copy
    SslSocket(const SslSocket&) = delete;
    SslSocket& operator=(const SslSocket&) = delete;

    // Connect with implicit SSL (ports 465, 995):
    // 1. TCP connect -> 2. SSL handshake immediately
    bool connect(const std::string& host, int port);

    // Connect TCP only, no SSL yet (for STARTTLS/STLS mode)
    bool connect_plain(const std::string& host, int port);

    // Perform SSL handshake on an already-connected TCP socket (STARTTLS/STLS)
    bool ssl_handshake();

    // Send data over SSL
    int send(const char* data, int len);

    // Receive data over SSL
    int recv(char* buffer, int buf_size, int timeout_sec = 30);

    // Receive until terminator string
    std::string recv_until(const std::string& terminator, int timeout_sec = 30);

    // Receive a line
    std::string recv_line(int timeout_sec = 30);

    // Close SSL and TCP
    void close();

    // Check connection state
    bool is_connected() const { return tcp_.is_connected(); }
    bool is_ssl_connected() const { return ssl_connected_; }

    // Access underlying TCP socket
    TcpSocket& tcp() { return tcp_; }

    // Get last error
    std::string get_last_error() const;

    // Static: initialize OpenSSL (call once at startup)
    static bool init_openssl();
    static void cleanup_openssl();

    // Set whether to verify server certificates
    void set_verify_cert(bool verify) { verify_cert_ = verify; }

private:
    TcpSocket tcp_;
    SSL_CTX*  ctx_;
    SSL*      ssl_;
    bool      ssl_connected_;
    bool      verify_cert_;
};

#endif // SSL_SOCKET_H
