#ifndef TCP_SOCKET_H
#define TCP_SOCKET_H

#include <string>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socklen_t = int;
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #define SOCKET int
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
    #define closesocket close
#endif

class TcpSocket {
public:
    TcpSocket();
    ~TcpSocket();

    // Disable copy
    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    // Create socket
    bool create();

    // Connect to host:port (performs DNS resolution)
    bool connect(const std::string& host, int port);

    // Send data
    int send(const char* data, int len);

    // Receive data with timeout (seconds), returns bytes read or -1 on error
    int recv(char* buffer, int buf_size, int timeout_sec = 30);

    // Receive until a terminator string is found
    std::string recv_until(const std::string& terminator, int timeout_sec = 30);

    // Receive a line (ends with \r\n)
    std::string recv_line(int timeout_sec = 30);

    // Receive all available data
    std::string recv_all(int timeout_sec = 30, int max_size = 104857600);

    // Close the socket
    void close();

    // Check if connected
    bool is_connected() const { return connected_; }

    // Get the raw socket handle (for SSL wrapping)
    SOCKET get_handle() const { return sock_; }

    // Set the raw socket handle (for SSL STARTTLS)
    void set_handle(SOCKET s) { sock_ = s; connected_ = (s != INVALID_SOCKET); }

    // Get last error
    int get_last_error() const { return last_error_; }
    std::string get_last_error_string() const;

    // Static: initialize Winsock (call once at startup)
    static bool init_winsock();
    static void cleanup_winsock();

private:
    SOCKET sock_;
    bool   connected_;
    int    last_error_;
};

#endif // TCP_SOCKET_H
