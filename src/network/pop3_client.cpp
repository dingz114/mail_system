#include "pop3_client.h"
#include <sstream>
#include <iostream>
#include <map>
#include <chrono>

Pop3Client::Pop3Client()
    : last_success_(false), connected_(false),
      progress_cb_(nullptr) {}

Pop3Client::~Pop3Client() {
    quit();
}

std::vector<Email> Pop3Client::receive_emails(const std::string& server, int port, bool use_ssl,
                                               const std::string& username, const std::string& password,
                                               int* new_count,
                                               const std::unordered_set<std::string>* skip_uids) {
    std::vector<Email> emails;

    if (!connect(server, port, use_ssl)) {
        last_error_ = "Connect failed";
        return emails;
    }

    // Read greeting
    recv_line();
    if (!last_success_) {
        last_error_ = "Server greeting error: " + last_response_;
        return emails;
    }

    if (!user(username)) {
        last_error_ = "USER failed: " + last_response_;
        return emails;
    }

    if (!pass(password)) {
        last_error_ = "PASS failed: " + last_response_;
        return emails;
    }

    int count = 0, total_size = 0;
    if (!stat(&count, &total_size)) {
        last_error_ = "STAT failed: " + last_response_;
        return emails;
    }

    if (count == 0) {
        if (new_count) *new_count = 0;
        quit();
        return emails;
    }

    // Get UIDL for dedup
    std::vector<std::pair<int, std::string>> uid_list = uidl();
    if (!last_success_) {
        // UIDL 失败不致命，清除错误标记继续
        last_error_.clear();
        last_success_ = true;
    }

    // Build UID map for easy lookup
    std::map<int, std::string> uid_map;
    for (const auto& p : uid_list) {
        uid_map[p.first] = p.second;
    }

    int fetch_total = count;
    if (skip_uids && !skip_uids->empty() && !uid_map.empty()) {
        fetch_total = 0;
        for (int i = 1; i <= count; ++i) {
            auto it = uid_map.find(i);
            if (it == uid_map.end() || !skip_uids->count(it->second)) {
                ++fetch_total;
            }
        }
    }

    int fetched = 0;
    for (int i = 1; i <= count; ++i) {
        auto uid_it = uid_map.find(i);
        if (skip_uids && uid_it != uid_map.end() && skip_uids->count(uid_it->second)) {
            continue;
        }

        if (progress_cb_) {
            progress_cb_(++fetched, fetch_total);
        }

        std::string raw = retr(i);
        if (!last_success_) {
            if (last_error_.empty()) {
                last_error_ = "Failed to retrieve message " + std::to_string(i);
            }
            continue;  // Skip failed retrievals
        }

        Email email;
        email.folder = "inbox";
        email.body_plain = raw;  // Raw email will be parsed by MIME decoder
        if (uid_map.count(i)) {
            email.pop3_uid = uid_map[i];
        }
        emails.push_back(email);
    }

    if (new_count) *new_count = (int)emails.size();

    quit();
    return emails;
}

bool Pop3Client::connect(const std::string& server, int port, bool use_ssl) {
    if (use_ssl) {
        if (!socket_.connect(server, port)) {
            last_error_ = "SSL connect failed: " + socket_.get_last_error();
            last_success_ = false;
            return false;
        }
    } else {
        if (!socket_.connect_plain(server, port)) {
            last_error_ = "TCP connect failed: " + socket_.get_last_error();
            last_success_ = false;
            return false;
        }
    }
    connected_ = true;
    last_success_ = true;
    return true;
}

bool Pop3Client::user(const std::string& username) {
    std::string cmd = "USER " + username + "\r\n";
    if (socket_.is_ssl_connected()) {
        socket_.send(cmd.c_str(), (int)cmd.size());
    } else {
        socket_.tcp().send(cmd.c_str(), (int)cmd.size());
    }
    recv_line();
    return last_success_;
}

bool Pop3Client::pass(const std::string& password) {
    std::string cmd = "PASS " + password + "\r\n";
    if (socket_.is_ssl_connected()) {
        socket_.send(cmd.c_str(), (int)cmd.size());
    } else {
        socket_.tcp().send(cmd.c_str(), (int)cmd.size());
    }
    recv_line();
    return last_success_;
}

bool Pop3Client::stat(int* out_count, int* out_total_size) {
    std::string cmd = "STAT\r\n";
    if (socket_.is_ssl_connected()) {
        socket_.send(cmd.c_str(), (int)cmd.size());
    } else {
        socket_.tcp().send(cmd.c_str(), (int)cmd.size());
    }
    recv_line();
    if (!last_success_) return false;

    // Parse: +OK <count> <total_size>
    std::istringstream iss(last_response_);
    std::string ok, count_str, size_str;
    iss >> ok >> count_str >> size_str;
    if (out_count) *out_count = std::stoi(count_str);
    if (out_total_size) *out_total_size = std::stoi(size_str);
    return true;
}

bool Pop3Client::list(std::vector<std::pair<int, int>>& messages) {
    std::string cmd = "LIST\r\n";
    if (socket_.is_ssl_connected()) {
        socket_.send(cmd.c_str(), (int)cmd.size());
    } else {
        socket_.tcp().send(cmd.c_str(), (int)cmd.size());
    }

    std::string response = recv_multiline();
    if (!last_success_) return false;

    std::istringstream iss(response);
    std::string line;
    // Skip the +OK line
    std::getline(iss, line);
    while (std::getline(iss, line)) {
        if (line == "." || line == "\r" || line.empty()) break;
        std::istringstream liss(line);
        int num, size;
        if (liss >> num >> size) {
            messages.push_back({num, size});
        }
    }
    return true;
}

std::string Pop3Client::retr(int msg_number) {
    std::string cmd = "RETR " + std::to_string(msg_number) + "\r\n";
    if (socket_.is_ssl_connected()) {
        socket_.send(cmd.c_str(), (int)cmd.size());
    } else {
        socket_.tcp().send(cmd.c_str(), (int)cmd.size());
    }

    return recv_multiline();
}

bool Pop3Client::dele(int msg_number) {
    std::string cmd = "DELE " + std::to_string(msg_number) + "\r\n";
    if (socket_.is_ssl_connected()) {
        socket_.send(cmd.c_str(), (int)cmd.size());
    } else {
        socket_.tcp().send(cmd.c_str(), (int)cmd.size());
    }
    recv_line();
    return last_success_;
}

std::vector<std::pair<int, std::string>> Pop3Client::uidl() {
    std::vector<std::pair<int, std::string>> result;
    std::string cmd = "UIDL\r\n";
    if (socket_.is_ssl_connected()) {
        socket_.send(cmd.c_str(), (int)cmd.size());
    } else {
        socket_.tcp().send(cmd.c_str(), (int)cmd.size());
    }

    std::string response = recv_multiline();
    if (!last_success_) return result;

    std::istringstream iss(response);
    std::string line;
    std::getline(iss, line);  // Skip +OK
    while (std::getline(iss, line)) {
        if (line == "." || line == "\r" || line.empty()) break;
        // Remove trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream liss(line);
        int num;
        std::string uid;
        if (liss >> num >> uid) {
            result.push_back({num, uid});
        }
    }
    return result;
}

bool Pop3Client::capa(std::string& capabilities) {
    std::string cmd = "CAPA\r\n";
    if (socket_.is_ssl_connected()) {
        socket_.send(cmd.c_str(), (int)cmd.size());
    } else {
        socket_.tcp().send(cmd.c_str(), (int)cmd.size());
    }
    capabilities = recv_multiline();
    return last_success_;
}

bool Pop3Client::stls() {
    std::string cmd = "STLS\r\n";
    socket_.tcp().send(cmd.c_str(), (int)cmd.size());
    recv_line();
    if (!last_success_) return false;

    return socket_.ssl_handshake();
}

void Pop3Client::quit() {
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

std::string Pop3Client::recv_line() {
    std::string line;
    if (socket_.is_ssl_connected()) {
        line = socket_.recv_line(30);
    } else {
        line = socket_.tcp().recv_line(30);
    }
    last_response_ = line;
    last_success_ = check_ok(line);
    return line;
}

std::string Pop3Client::recv_multiline() {
    // POP3 multi-line response ends with "\r\n.\r\n". Large attachments can
    // legitimately take several minutes; treat timeout as "no data for a
    // while", not as a fixed total transfer duration.
    std::string data;
    char buffer[65536];
    auto started_at = std::chrono::steady_clock::now();
    auto last_data_at = std::chrono::steady_clock::now();
    constexpr int kReadTimeoutSec = 300;
    constexpr int kMaxTransferMinutes = 60;

    while (true) {
        int bytes = socket_.is_ssl_connected()
            ? socket_.recv(buffer, sizeof(buffer), kReadTimeoutSec)
            : socket_.tcp().recv(buffer, sizeof(buffer), kReadTimeoutSec);

        if (bytes > 0) {
            data.append(buffer, bytes);
            last_data_at = std::chrono::steady_clock::now();
            if (data.find("\r\n.\r\n") != std::string::npos ||
                data.find("\n.\n") != std::string::npos) {
                break;
            }
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        auto idle = std::chrono::duration_cast<std::chrono::seconds>(now - last_data_at).count();
        auto total = std::chrono::duration_cast<std::chrono::minutes>(now - started_at).count();
        if (idle >= kReadTimeoutSec || total >= kMaxTransferMinutes) {
            break;
        }
    }

    last_response_ = data;
    const bool complete = data.find("\r\n.\r\n") != std::string::npos ||
                          data.find("\n.\n") != std::string::npos;
    last_success_ = check_ok(data) && complete;
    if (!last_success_ && check_ok(data)) {
        last_error_ = "POP3 response ended before message terminator; attachment may be incomplete";
    }
    return data;
}

bool Pop3Client::check_ok(const std::string& response) {
    return response.size() >= 3 && response[0] == '+' && response[1] == 'O' && response[2] == 'K';
}
