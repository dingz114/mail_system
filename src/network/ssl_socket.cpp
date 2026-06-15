#include "ssl_socket.h"
#include <iostream>
#include <chrono>

static bool openssl_initialized = false;

bool SslSocket::init_openssl() {
    if (openssl_initialized) return true;
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    openssl_initialized = true;
    return true;
}

void SslSocket::cleanup_openssl() {
    if (openssl_initialized) {
        EVP_cleanup();
        ERR_free_strings();
        openssl_initialized = false;
    }
}

SslSocket::SslSocket()
    : ctx_(nullptr), ssl_(nullptr), ssl_connected_(false), verify_cert_(false) {}

SslSocket::~SslSocket() {
    close();
}

bool SslSocket::connect(const std::string& host, int port) {
    // Step 1: TCP connect
    if (!tcp_.connect(host, port)) {
        return false;
    }

    // Step 2: Create SSL context
    const SSL_METHOD* method = TLS_client_method();
    ctx_ = SSL_CTX_new(method);
    if (!ctx_) {
        return false;
    }

    // Certificate verification
    if (verify_cert_) {
        SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, nullptr);
        // Load default CA certificates
        SSL_CTX_set_default_verify_paths(ctx_);
    } else {
        SSL_CTX_set_verify(ctx_, SSL_VERIFY_NONE, nullptr);
    }

    // Step 3: Create SSL object and associate with socket
    ssl_ = SSL_new(ctx_);
    if (!ssl_) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        return false;
    }

    SSL_set_fd(ssl_, (int)tcp_.get_handle());

    // Step 4: SSL handshake
    if (SSL_connect(ssl_) != 1) {
        SSL_free(ssl_);
        ssl_ = nullptr;
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        return false;
    }

    ssl_connected_ = true;
    return true;
}

bool SslSocket::connect_plain(const std::string& host, int port) {
    return tcp_.connect(host, port);
}

bool SslSocket::ssl_handshake() {
    if (!tcp_.is_connected()) return false;

    const SSL_METHOD* method = TLS_client_method();
    ctx_ = SSL_CTX_new(method);
    if (!ctx_) return false;

    if (verify_cert_) {
        SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, nullptr);
        SSL_CTX_set_default_verify_paths(ctx_);
    } else {
        SSL_CTX_set_verify(ctx_, SSL_VERIFY_NONE, nullptr);
    }

    ssl_ = SSL_new(ctx_);
    if (!ssl_) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        return false;
    }

    SSL_set_fd(ssl_, (int)tcp_.get_handle());

    if (SSL_connect(ssl_) != 1) {
        SSL_free(ssl_);
        ssl_ = nullptr;
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        return false;
    }

    ssl_connected_ = true;
    return true;
}

int SslSocket::send(const char* data, int len) {
    if (!ssl_connected_ || !ssl_) return -1;

    int total = 0;
    while (total < len) {
        int sent = SSL_write(ssl_, data + total, len - total);
        if (sent <= 0) {
            int err = SSL_get_error(ssl_, sent);
            if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) {
                continue;
            }
            return -1;
        }
        total += sent;
    }
    return total;
}

int SslSocket::recv(char* buffer, int buf_size, int timeout_sec) {
    if (!ssl_connected_ || !ssl_) {
        return tcp_.recv(buffer, buf_size, timeout_sec);
    }

    SOCKET sock = tcp_.get_handle();

    // 1) Check if OpenSSL already has buffered decrypted data
    int pending = SSL_pending(ssl_);
    if (pending > 0) {
        int bytes = SSL_read(ssl_, buffer, (int)std::min((size_t)(buf_size - 1), (size_t)pending));
        if (bytes > 0) {
            buffer[bytes] = '\0';
            return bytes;
        }
    }

    // 2) Make socket non-blocking for select() + SSL_read combo
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);

    // 3) Wait for data with select(), then SSL_read
    auto start = std::chrono::steady_clock::now();
    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeout_sec) {
            // Restore blocking mode
            mode = 0;
            ioctlsocket(sock, FIONBIO, &mode);
            return -1;
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        struct timeval tv;
        tv.tv_sec = 1;  // poll every 1 second
        tv.tv_usec = 0;

        int sel = select((int)sock + 1, &fds, nullptr, nullptr, &tv);
        if (sel < 0) {
            mode = 0; ioctlsocket(sock, FIONBIO, &mode);
            return -1;
        }
        if (sel == 0) continue;  // timeout, loop and recheck total elapsed

        // Socket readable, try SSL_read
        int bytes = SSL_read(ssl_, buffer, buf_size - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            mode = 0; ioctlsocket(sock, FIONBIO, &mode);
            return bytes;
        }
        if (bytes == 0) {
            mode = 0; ioctlsocket(sock, FIONBIO, &mode);
            return 0;  // clean shutdown
        }
        // bytes < 0
        int err = SSL_get_error(ssl_, bytes);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            continue;  // retry
        }
        // real error
        mode = 0; ioctlsocket(sock, FIONBIO, &mode);
        return -1;
    }
}

std::string SslSocket::recv_until(const std::string& terminator, int timeout_sec) {
    std::string result;
    char buffer[4096];
    auto start_time = std::chrono::steady_clock::now();

    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        if (elapsed >= timeout_sec) break;

        int remaining = static_cast<int>(timeout_sec - elapsed);
        int bytes = recv(buffer, sizeof(buffer), remaining > 0 ? remaining : 1);
        if (bytes <= 0) break;

        result.append(buffer, bytes);

        if (result.size() >= terminator.size()) {
            if (result.compare(result.size() - terminator.size(), terminator.size(), terminator) == 0) {
                break;
            }
        }
    }
    return result;
}

std::string SslSocket::recv_line(int timeout_sec) {
    return recv_until("\r\n", timeout_sec);
}

void SslSocket::close() {
    if (ssl_) {
        SSL_shutdown(ssl_);
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
    if (ctx_) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
    ssl_connected_ = false;
    tcp_.close();
}

std::string SslSocket::get_last_error() const {
    unsigned long err = ERR_get_error();
    if (err != 0) {
        char buf[256];
        ERR_error_string_n(err, buf, sizeof(buf));
        return std::string(buf);
    }
    return tcp_.get_last_error_string();
}
