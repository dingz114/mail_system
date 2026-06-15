#include "db_connection.h"
#include <iostream>
#include <vector>

DbConnection::DbConnection() : conn_(nullptr), connected_(false) {}

DbConnection::~DbConnection() {
    close();
}

bool DbConnection::connect(const std::string& host, int port,
                            const std::string& user, const std::string& password,
                            const std::string& database) {
    conn_ = mysql_init(nullptr);
    if (!conn_) {
        return false;
    }

    // Set connection options
    unsigned int timeout = 10;
    mysql_options(conn_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    if (!mysql_real_connect(conn_, host.c_str(), user.c_str(), password.c_str(),
                            database.c_str(), port, nullptr, 0)) {
        return false;
    }

    // Set charset to utf8mb4
    mysql_set_character_set(conn_, "utf8mb4");

    connected_ = true;
    return true;
}

bool DbConnection::query(const std::string& sql) {
    if (!connected_) return false;

    if (mysql_query(conn_, sql.c_str()) != 0) {
        return false;
    }
    return true;
}

MYSQL_RES* DbConnection::store_result() {
    if (!connected_) return nullptr;
    return mysql_store_result(conn_);
}

long long DbConnection::affected_rows() {
    if (!connected_) return 0;
    return mysql_affected_rows(conn_);
}

long long DbConnection::last_insert_id() {
    if (!connected_) return 0;
    return mysql_insert_id(conn_);
}

std::string DbConnection::escape_string(const std::string& str) {
    if (!connected_ || str.empty()) return str;

    std::vector<char> buf(str.size() * 2 + 1);
    unsigned long len = mysql_real_escape_string(conn_, buf.data(), str.c_str(),
                                                  static_cast<unsigned long>(str.size()));
    return std::string(buf.data(), len);
}

std::string DbConnection::get_last_error() const {
    if (conn_) {
        return std::string(mysql_error(conn_));
    }
    return "No connection";
}

void DbConnection::close() {
    if (conn_) {
        mysql_close(conn_);
        conn_ = nullptr;
    }
    connected_ = false;
}
