#ifndef POP3_CLIENT_H
#define POP3_CLIENT_H

#include "ssl_socket.h"
#include "../core/email.h"
#include <string>
#include <vector>
#include <utility>
#include <unordered_set>
#include <functional>

class Pop3Client {
public:
    Pop3Client();
    ~Pop3Client();

    // Full receive: connects, authenticates, fetches new emails, disconnects
    std::vector<Email> receive_emails(const std::string& server, int port, bool use_ssl,
                                       const std::string& username, const std::string& password,
                                       int* new_count = nullptr,
                                       const std::unordered_set<std::string>* skip_uids = nullptr);

    // Low-level commands
    bool connect(const std::string& server, int port, bool use_ssl);
    bool user(const std::string& username);
    bool pass(const std::string& password);
    bool stat(int* out_count, int* out_total_size);
    bool list(std::vector<std::pair<int, int>>& messages);  // <msg_num, size>
    std::string retr(int msg_number);                        // Returns raw email text
    bool dele(int msg_number);
    std::vector<std::pair<int, std::string>> uidl();         // <msg_num, uid>
    bool capa(std::string& capabilities);
    bool stls();                                             // STARTTLS for POP3
    void quit();

    // Read a single-line POP3 response (public for testing)
    std::string recv_line();

    // Status
    std::string get_last_response() const { return last_response_; }
    std::string get_last_error() const { return last_error_; }
    bool is_success() const { return last_success_; }

    // Set callback for progress reporting (msg_num, msg_count)
    using ProgressCallback = std::function<void(int current, int total)>;
    void set_progress_callback(ProgressCallback cb) {
        progress_cb_ = cb;
    }

private:
    // Read a multi-line POP3 response (terminated by \r\n.\r\n)
    std::string recv_multiline();

    // Check if response is +OK
    bool check_ok(const std::string& response);

    SslSocket socket_;
    std::string last_response_;
    std::string last_error_;
    bool last_success_;
    bool connected_;

    ProgressCallback progress_cb_;
};

#endif // POP3_CLIENT_H
