#ifndef SMTP_CLIENT_H
#define SMTP_CLIENT_H

#include "ssl_socket.h"
#include "../core/email.h"
#include <string>
#include <vector>

class SmtpClient {
public:
    SmtpClient();
    ~SmtpClient();

    // Full send: connects, authenticates, sends email, disconnects
    // Returns true on success, false on failure. Check get_last_error() for details.
    bool send_email(const Email& email,
                    const std::string& smtp_server, int port, bool use_ssl,
                    const std::string& username, const std::string& password);

    // Low-level commands (for granular control)
    bool connect(const std::string& server, int port, bool use_ssl);
    bool ehlo();
    bool starttls();                   // For port 587 explicit TLS
    bool auth_login(const std::string& username, const std::string& password);
    bool mail_from(const std::string& sender);
    bool rcpt_to(const std::string& recipient);
    bool data_begin();                 // Send DATA command, wait for 354
    bool data_send(const std::string& mime_message); // Send MIME content
    bool data_end();                   // Send \r\n.\r\n and wait for 250
    void quit();

    // Status
    std::string get_last_response() const { return last_response_; }
    std::string get_last_error() const { return last_error_; }
    int get_last_code() const { return last_code_; }

private:
    // Read SMTP response (may be multi-line — lines with '-' continue, last line has ' ')
    std::string recv_response();
    bool check_response(int expected_code);

    SslSocket socket_;
    std::string last_response_;
    std::string last_error_;
    int last_code_;
    bool connected_;
};

#endif // SMTP_CLIENT_H
