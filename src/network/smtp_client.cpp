#include "smtp_client.h"
#include "../core/base64.h"
#include <sstream>
#include <iostream>

SmtpClient::SmtpClient() : last_code_(0), connected_(false) {}

SmtpClient::~SmtpClient() {
    quit();
}

bool SmtpClient::send_email(const Email& email,
                             const std::string& smtp_server, int port, bool use_ssl,
                             const std::string& username, const std::string& password) {
    if (!connect(smtp_server, port, use_ssl)) {
        return false;
    }

    // Read server greeting
    recv_response();
    if (last_code_ != 220) {
        last_error_ = "Unexpected greeting: " + last_response_;
        return false;
    }

    if (!ehlo()) {
        last_error_ = "EHLO failed: " + last_response_;
        return false;
    }

    // For port 587, try STARTTLS if not already SSL
    if (!use_ssl && port == 587) {
        // Check if STARTTLS is in the EHLO response
        if (last_response_.find("STARTTLS") != std::string::npos) {
            if (!starttls()) {
                last_error_ = "STARTTLS failed: " + last_response_;
                return false;
            }
            if (!ehlo()) {
                last_error_ = "EHLO after STARTTLS failed: " + last_response_;
                return false;
            }
        }
    }

    if (!auth_login(username, password)) {
        last_error_ = "AUTH LOGIN failed: " + last_response_;
        return false;
    }

    std::string sender = email.sender_addr;
    if (!email.sender_name.empty()) {
        sender = email.sender_name + " <" + email.sender_addr + ">";
    }
    if (!mail_from(email.sender_addr)) {
        last_error_ = "MAIL FROM failed: " + last_response_;
        return false;
    }

    // All recipients
    std::vector<std::string> all_recipients = email.to;
    all_recipients.insert(all_recipients.end(), email.cc.begin(), email.cc.end());
    all_recipients.insert(all_recipients.end(), email.bcc.begin(), email.bcc.end());

    for (const auto& recipient : all_recipients) {
        // Extract just the email address
        std::string addr = recipient;
        size_t lt = addr.find('<');
        size_t gt = addr.find('>');
        if (lt != std::string::npos && gt != std::string::npos && gt > lt) {
            addr = addr.substr(lt + 1, gt - lt - 1);
        }
        if (!rcpt_to(addr)) {
            last_error_ = "RCPT TO <" + addr + "> failed: " + last_response_;
            return false;
        }
    }

    if (!data_begin()) {
        last_error_ = "DATA failed: " + last_response_;
        return false;
    }

    if (!data_send(email.body_plain)) {
        last_error_ = "DATA send failed: " + last_response_;
        return false;
    }

    quit();
    return true;
}

bool SmtpClient::connect(const std::string& server, int port, bool use_ssl) {
    if (use_ssl) {
        if (!socket_.connect(server, port)) {
            last_error_ = "SSL connect failed: " + socket_.get_last_error();
            return false;
        }
    } else {
        if (!socket_.connect_plain(server, port)) {
            last_error_ = "TCP connect failed: " + socket_.get_last_error();
            return false;
        }
    }
    connected_ = true;
    return true;
}

bool SmtpClient::ehlo() {
    std::string cmd = "EHLO localhost\r\n";
    if (socket_.is_ssl_connected()) {
        socket_.send(cmd.c_str(), (int)cmd.size());
    } else {
        socket_.tcp().send(cmd.c_str(), (int)cmd.size());
    }
    recv_response();
    return last_code_ == 250;
}

bool SmtpClient::starttls() {
    std::string cmd = "STARTTLS\r\n";
    socket_.tcp().send(cmd.c_str(), (int)cmd.size());
    recv_response();
    if (last_code_ != 220) return false;

    // Upgrade to SSL
    return socket_.ssl_handshake();
}

bool SmtpClient::auth_login(const std::string& username, const std::string& password) {
    std::string cmd = "AUTH LOGIN\r\n";
    if (socket_.is_ssl_connected()) {
        socket_.send(cmd.c_str(), (int)cmd.size());
    } else {
        socket_.tcp().send(cmd.c_str(), (int)cmd.size());
    }
    recv_response();
    if (last_code_ != 334) return false;

    // Send Base64 username
    std::string encoded_user = Base64::encode(username) + "\r\n";
    if (socket_.is_ssl_connected()) {
        socket_.send(encoded_user.c_str(), (int)encoded_user.size());
    } else {
        socket_.tcp().send(encoded_user.c_str(), (int)encoded_user.size());
    }
    recv_response();
    if (last_code_ != 334) return false;

    // Send Base64 password
    std::string encoded_pass = Base64::encode(password) + "\r\n";
    if (socket_.is_ssl_connected()) {
        socket_.send(encoded_pass.c_str(), (int)encoded_pass.size());
    } else {
        socket_.tcp().send(encoded_pass.c_str(), (int)encoded_pass.size());
    }
    recv_response();
    return last_code_ == 235;
}

bool SmtpClient::mail_from(const std::string& sender) {
    std::string cmd = "MAIL FROM:<" + sender + ">\r\n";
    if (socket_.is_ssl_connected()) {
        socket_.send(cmd.c_str(), (int)cmd.size());
    } else {
        socket_.tcp().send(cmd.c_str(), (int)cmd.size());
    }
    recv_response();
    return last_code_ == 250;
}

bool SmtpClient::rcpt_to(const std::string& recipient) {
    std::string cmd = "RCPT TO:<" + recipient + ">\r\n";
    if (socket_.is_ssl_connected()) {
        socket_.send(cmd.c_str(), (int)cmd.size());
    } else {
        socket_.tcp().send(cmd.c_str(), (int)cmd.size());
    }
    recv_response();
    return last_code_ == 250;
}

bool SmtpClient::data_begin() {
    std::string cmd = "DATA\r\n";
    if (socket_.is_ssl_connected()) {
        socket_.send(cmd.c_str(), (int)cmd.size());
    } else {
        socket_.tcp().send(cmd.c_str(), (int)cmd.size());
    }
    recv_response();
    return last_code_ == 354;
}

bool SmtpClient::data_send(const std::string& mime_message) {
    // Dot-stuffing: any line starting with '.' gets an extra '.'
    std::istringstream iss(mime_message);
    std::ostringstream oss;
    std::string line;
    while (std::getline(iss, line)) {
        // Remove \r if present (getline removes \n)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty() && line[0] == '.') {
            oss << '.';  // Dot-stuffing
        }
        oss << line << "\r\n";
    }
    oss << "\r\n.\r\n";

    std::string data = oss.str();
    if (socket_.is_ssl_connected()) {
        socket_.send(data.c_str(), (int)data.size());
    } else {
        socket_.tcp().send(data.c_str(), (int)data.size());
    }

    recv_response();
    return last_code_ == 250;
}

bool SmtpClient::data_end() {
    // Already sent \r\n.\r\n in data_send
    return last_code_ == 250;
}

void SmtpClient::quit() {
    if (connected_) {
        std::string cmd = "QUIT\r\n";
        if (socket_.is_ssl_connected()) {
            socket_.send(cmd.c_str(), (int)cmd.size());
        } else {
            socket_.tcp().send(cmd.c_str(), (int)cmd.size());
        }
        socket_.close();
        connected_ = false;
    }
}

std::string SmtpClient::recv_response() {
    std::string result;
    char buffer[4096];

    while (true) {
        int bytes;
        if (socket_.is_ssl_connected()) {
            bytes = socket_.recv(buffer, sizeof(buffer), 30);
        } else {
            bytes = socket_.tcp().recv(buffer, sizeof(buffer), 30);
        }
        if (bytes <= 0) break;

        std::string chunk(buffer, bytes);
        result += chunk;

        // Check if this is the last line (4th char is space, not hyphen)
        // SMTP response format: "250-message\r\n250 message\r\n"
        if (result.size() >= 5) {
            // Look for the pattern: \r\n<3digits><space> or start<3digits><space>
            size_t spos = 0;
            size_t end_pos = result.size();
            // The last complete line determines completion
            size_t last_crlf = result.rfind("\r\n");
            if (last_crlf != std::string::npos && last_crlf + 2 < result.size()) {
                spos = last_crlf + 2;
            }
            if (spos < result.size() && result.size() >= spos + 4) {
                if (result[spos + 3] == ' ') {
                    // Found a line with space after code — this terminates the response
                    break;
                }
            }
            // Also handle single-line responses
            if (result.size() >= 4 && result[3] == ' ') {
                break;
            }
        }
    }

    // Parse status code
    if (result.size() >= 3) {
        last_code_ = (result[0] - '0') * 100 + (result[1] - '0') * 10 + (result[2] - '0');
    } else {
        last_code_ = 0;
    }

    last_response_ = result;
    return result;
}

bool SmtpClient::check_response(int expected_code) {
    return last_code_ == expected_code;
}
