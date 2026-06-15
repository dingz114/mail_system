#include "tcp_socket.h"
#include <iostream>
#include <cstring>
#include <chrono>

#ifdef _WIN32
static bool winsock_initialized = false;
#endif

bool TcpSocket::init_winsock() {
#ifdef _WIN32
    if (winsock_initialized) return true;
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return false;
    }
    winsock_initialized = true;
#endif
    return true;
}

void TcpSocket::cleanup_winsock() {
#ifdef _WIN32
    if (winsock_initialized) {
        WSACleanup();
        winsock_initialized = false;
    }
#endif
}

TcpSocket::TcpSocket() : sock_(INVALID_SOCKET), connected_(false), last_error_(0) {}

TcpSocket::~TcpSocket() {
    close();
}

bool TcpSocket::create() {
    sock_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_ == INVALID_SOCKET) {
#ifdef _WIN32
        last_error_ = WSAGetLastError();
#else
        last_error_ = errno;
#endif
        return false;
    }
    return true;
}

bool TcpSocket::connect(const std::string& host, int port) {
    if (!create()) return false;

    // Resolve hostname
    struct addrinfo hints, *result = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result) != 0) {
        last_error_ = -1;
        return false;
    }

    // Connect to the first resolved address
    int ret = ::connect(sock_, result->ai_addr, (int)result->ai_addrlen);
    freeaddrinfo(result);

    if (ret == SOCKET_ERROR) {
#ifdef _WIN32
        last_error_ = WSAGetLastError();
#else
        last_error_ = errno;
#endif
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        return false;
    }

    connected_ = true;
    return true;
}

int TcpSocket::send(const char* data, int len) {
    if (!connected_) return -1;
    int total = 0;
    while (total < len) {
        int sent = ::send(sock_, data + total, len - total, 0);
        if (sent == SOCKET_ERROR) {
#ifdef _WIN32
            last_error_ = WSAGetLastError();
#else
            last_error_ = errno;
#endif
            return -1;
        }
        total += sent;
    }
    return total;
}

int TcpSocket::recv(char* buffer, int buf_size, int timeout_sec) {
    if (!connected_) return -1;

    // Set timeout
#ifdef _WIN32
    DWORD timeout = timeout_sec * 1000;
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif

    int bytes = ::recv(sock_, buffer, buf_size - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
    } else if (bytes == SOCKET_ERROR) {
#ifdef _WIN32
        last_error_ = WSAGetLastError();
#else
        last_error_ = errno;
#endif
        return -1;
    }
    return bytes;
}

std::string TcpSocket::recv_until(const std::string& terminator, int timeout_sec) {
    std::string result;
    char buffer[4096];
    auto start_time = std::chrono::steady_clock::now();

    while (true) {
        // Check timeout
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        if (elapsed >= timeout_sec) break;

        int remaining = static_cast<int>(timeout_sec - elapsed);
        int bytes = recv(buffer, sizeof(buffer), remaining > 0 ? remaining : 1);
        if (bytes <= 0) break;

        result.append(buffer, bytes);

        // Check if terminator found
        if (result.size() >= terminator.size()) {
            if (result.compare(result.size() - terminator.size(), terminator.size(), terminator) == 0) {
                break;
            }
        }
    }
    return result;
}

std::string TcpSocket::recv_line(int timeout_sec) {
    return recv_until("\r\n", timeout_sec);
}

std::string TcpSocket::recv_all(int timeout_sec, int max_size) {
    std::string result;
    result.reserve(max_size);
    char buffer[65536];
    auto start_time = std::chrono::steady_clock::now();

    while ((int)result.size() < max_size) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        if (elapsed >= timeout_sec) break;

        int remaining = static_cast<int>(timeout_sec - elapsed);
        int bytes = recv(buffer, sizeof(buffer), remaining > 0 ? remaining : 1);
        if (bytes <= 0) break;

        result.append(buffer, bytes);
    }
    return result;
}

void TcpSocket::close() {
    if (sock_ != INVALID_SOCKET) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
    connected_ = false;
}

std::string TcpSocket::get_last_error_string() const {
#ifdef _WIN32
    char* s = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, last_error_, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&s, 0, nullptr);
    std::string result(s ? s : "Unknown error");
    if (s) LocalFree(s);
    return result;
#else
    return std::string(strerror(last_error_));
#endif
}
