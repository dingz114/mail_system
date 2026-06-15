#ifndef DB_CONNECTION_H
#define DB_CONNECTION_H

#include <string>
#include <mysql.h>

class DbConnection {
public:
    DbConnection();
    ~DbConnection();

    // Disable copy
    DbConnection(const DbConnection&) = delete;
    DbConnection& operator=(const DbConnection&) = delete;

    // Connect to MySQL server
    bool connect(const std::string& host, int port,
                 const std::string& user, const std::string& password,
                 const std::string& database);

    // Execute a query (INSERT, UPDATE, DELETE, etc.)
    bool query(const std::string& sql);

    // Execute a query and get the result set (SELECT)
    MYSQL_RES* store_result();

    // Get number of affected rows
    long long affected_rows();

    // Get last insert ID
    long long last_insert_id();

    // Escape a string for safe SQL embedding
    std::string escape_string(const std::string& str);

    // Get last error
    std::string get_last_error() const;

    // Check connection status
    bool is_connected() const { return connected_; }

    // Close connection
    void close();

    // Get the raw MySQL handle (for advanced operations)
    MYSQL* get_handle() { return conn_; }

private:
    MYSQL* conn_;
    bool   connected_;
};

#endif // DB_CONNECTION_H
