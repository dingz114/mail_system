#include "db_manager.h"
#include <cstdio>
#include <sstream>
#include <iostream>
#include <cstring>

DbManager::DbManager(DbConnection* conn) : conn_(conn) {}

std::string DbManager::last_error() const {
    return conn_ ? conn_->get_last_error() : "No database connection";
}

// ========== Helper functions ==========

static std::string sql_str(const std::string& s) {
    // This is a simple wrapper — the caller should use conn_->escape_string for values
    return s;
}

static std::string sql_int(int n) {
    return std::to_string(n);
}

static std::string sql_bool(bool b) {
    return b ? "1" : "0";
}

static std::string sql_quote(DbConnection* conn, const std::string& s) {
    return "'" + conn->escape_string(s) + "'";
}

// 只替换非法 UTF-8 字节；数据库文本列已迁移为 utf8mb4，合法 emoji 等
// 4 字节字符应原样保存。
static bool is_utf8_continuation(unsigned char b) {
    return (b & 0xC0) == 0x80;  // 10xxxxxx
}

static std::string safe_utf8(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        size_t len = 1;
        if ((c & 0x80) == 0)        len = 1;     // 0xxxxxxx
        else if ((c & 0xE0) == 0xC0) len = 2;    // 110xxxxx
        else if ((c & 0xF0) == 0xE0) len = 3;    // 1110xxxx
        else if ((c & 0xF8) == 0xF0) len = 4;    // 11110xxx
        else { result += '?'; ++i; continue; }    // invalid lead byte

        // 边界检查：确保不越界读
        if (i + len > s.size()) {
            result += '?';
            ++i;
            continue;
        }

        // 验证续字节格式 (10xxxxxx)
        bool valid = true;
        for (size_t j = 1; j < len; ++j) {
            if (!is_utf8_continuation(static_cast<unsigned char>(s[i + j]))) {
                valid = false;
                break;
            }
        }

        if (valid) {
            result.append(s, i, len);
        } else {
            result += '?';
        }
        i += len;
    }
    return result;
}

// ========== Accounts CRUD ==========

int DbManager::add_account(const Account& acc) {
    std::ostringstream sql;
    sql << "INSERT INTO accounts (account_name, email_address, username, password, "
        << "smtp_server, smtp_port, smtp_ssl, pop3_server, pop3_port, pop3_ssl, leave_on_server) "
        << "VALUES ("
        << sql_quote(conn_, acc.account_name) << ", "
        << sql_quote(conn_, acc.email_address) << ", "
        << sql_quote(conn_, acc.username) << ", "
        << sql_quote(conn_, acc.password) << ", "
        << sql_quote(conn_, acc.smtp_server) << ", "
        << sql_int(acc.smtp_port) << ", "
        << sql_bool(acc.smtp_ssl) << ", "
        << sql_quote(conn_, acc.pop3_server) << ", "
        << sql_int(acc.pop3_port) << ", "
        << sql_bool(acc.pop3_ssl) << ", "
        << sql_bool(acc.leave_on_server) << ")";

    if (!conn_->query(sql.str())) {
        return -1;
    }
    return (int)conn_->last_insert_id();
}

bool DbManager::update_account(const Account& acc) {
    std::ostringstream sql;
    sql << "UPDATE accounts SET "
        << "account_name=" << sql_quote(conn_, acc.account_name) << ", "
        << "email_address=" << sql_quote(conn_, acc.email_address) << ", "
        << "username=" << sql_quote(conn_, acc.username) << ", "
        << "password=" << sql_quote(conn_, acc.password) << ", "
        << "smtp_server=" << sql_quote(conn_, acc.smtp_server) << ", "
        << "smtp_port=" << sql_int(acc.smtp_port) << ", "
        << "smtp_ssl=" << sql_bool(acc.smtp_ssl) << ", "
        << "pop3_server=" << sql_quote(conn_, acc.pop3_server) << ", "
        << "pop3_port=" << sql_int(acc.pop3_port) << ", "
        << "pop3_ssl=" << sql_bool(acc.pop3_ssl) << ", "
        << "leave_on_server=" << sql_bool(acc.leave_on_server) << " "
        << "WHERE id=" << sql_int(acc.id);
    return conn_->query(sql.str());
}

bool DbManager::delete_account(int id) {
    std::string sql = "DELETE FROM accounts WHERE id=" + sql_int(id);
    return conn_->query(sql);
}

std::vector<Account> DbManager::get_all_accounts() {
    std::vector<Account> accounts;
    std::string sql = "SELECT * FROM accounts ORDER BY id";
    if (!conn_->query(sql)) return accounts;

    MYSQL_RES* result = conn_->store_result();
    if (!result) return accounts;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        accounts.push_back(row_to_account(row));
    }
    mysql_free_result(result);
    return accounts;
}

Account DbManager::get_account(int id) {
    Account acc;
    std::string sql = "SELECT * FROM accounts WHERE id=" + sql_int(id);
    if (!conn_->query(sql)) return acc;

    MYSQL_RES* result = conn_->store_result();
    if (!result) return acc;

    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
        acc = row_to_account(row);
    }
    mysql_free_result(result);
    return acc;
}

Account DbManager::get_account_by_email(const std::string& email) {
    Account acc;
    std::string sql = "SELECT * FROM accounts WHERE email_address=" + sql_quote(conn_, email);
    if (!conn_->query(sql)) return acc;

    MYSQL_RES* result = conn_->store_result();
    if (!result) return acc;

    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
        acc = row_to_account(row);
    }
    mysql_free_result(result);
    return acc;
}

Account DbManager::row_to_account(char** row) {
    Account acc;
    int col = 0;
    acc.id = row[col] ? std::stoi(row[col]) : 0; ++col;
    acc.account_name = row[col] ? row[col] : ""; ++col;
    acc.email_address = row[col] ? row[col] : ""; ++col;
    acc.username = row[col] ? row[col] : ""; ++col;
    acc.password = row[col] ? row[col] : ""; ++col;
    acc.smtp_server = row[col] ? row[col] : ""; ++col;
    acc.smtp_port = row[col] ? std::stoi(row[col]) : 465; ++col;
    acc.smtp_ssl = row[col] ? (std::stoi(row[col]) != 0) : true; ++col;
    acc.pop3_server = row[col] ? row[col] : ""; ++col;
    acc.pop3_port = row[col] ? std::stoi(row[col]) : 995; ++col;
    acc.pop3_ssl = row[col] ? (std::stoi(row[col]) != 0) : true; ++col;
    acc.leave_on_server = row[col] ? (std::stoi(row[col]) != 0) : true; ++col;
    return acc;
}

// ========== Emails CRUD ==========

int DbManager::save_email(const Email& email) {
    std::ostringstream sql;
    sql << "INSERT INTO emails (account_id, folder, message_id, pop3_uid, "
        << "sender_name, sender_addr, recipients_to, recipients_cc, recipients_bcc, "
        << "subject, body_plain, body_html, is_read, is_deleted, is_flagged, "
        << "has_attachments, received_date) VALUES ("
        << sql_int(email.account_id) << ", "
        << sql_quote(conn_, email.folder) << ", "
        << sql_quote(conn_, email.message_id) << ", "
        << sql_quote(conn_, email.pop3_uid) << ", "
        << sql_quote(conn_, safe_utf8(email.sender_name)) << ", "
        << sql_quote(conn_, safe_utf8(email.sender_addr)) << ", "
        << sql_quote(conn_, safe_utf8(Email::join_recipients(email.to))) << ", "
        << sql_quote(conn_, safe_utf8(Email::join_recipients(email.cc))) << ", "
        << sql_quote(conn_, safe_utf8(Email::join_recipients(email.bcc))) << ", "
        << sql_quote(conn_, safe_utf8(email.subject)) << ", "
        << sql_quote(conn_, safe_utf8(email.body_plain)) << ", "
        << sql_quote(conn_, safe_utf8(email.body_html)) << ", "
        << sql_bool(email.is_read) << ", "
        << sql_bool(email.is_deleted) << ", "
        << sql_bool(email.is_flagged) << ", "
        << sql_bool(email.has_attachments) << ", "
        << sql_quote(conn_, email.received_date.empty() ? "1970-01-01 00:00:00" : email.received_date) << ")";

    if (!conn_->query(sql.str())) {
        return -1;
    }
    return (int)conn_->last_insert_id();
}

bool DbManager::update_email(const Email& email) {
    std::ostringstream sql;
    sql << "UPDATE emails SET "
        << "folder=" << sql_quote(conn_, email.folder) << ", "
        << "is_read=" << sql_bool(email.is_read) << ", "
        << "is_deleted=" << sql_bool(email.is_deleted) << ", "
        << "is_flagged=" << sql_bool(email.is_flagged) << ", "
        << "has_attachments=" << sql_bool(email.has_attachments) << " "
        << "WHERE id=" << sql_int(email.id);
    return conn_->query(sql.str());
}

bool DbManager::mark_read(int email_id) {
    std::string sql = "UPDATE emails SET is_read=1 WHERE id=" + sql_int(email_id);
    return conn_->query(sql);
}

bool DbManager::mark_unread(int email_id) {
    std::string sql = "UPDATE emails SET is_read=0 WHERE id=" + sql_int(email_id);
    return conn_->query(sql);
}

bool DbManager::mark_deleted(int email_id) {
    std::string sql = "UPDATE emails SET is_deleted=1 WHERE id=" + sql_int(email_id);
    return conn_->query(sql);
}

bool DbManager::mark_undeleted(int email_id) {
    std::string sql = "UPDATE emails SET is_deleted=0 WHERE id=" + sql_int(email_id);
    return conn_->query(sql);
}

bool DbManager::delete_permanently(int email_id) {
    // 先删附件记录和同步状态（外键依赖），再删邮件
    std::string del_att = "DELETE FROM attachments WHERE email_id=" + sql_int(email_id);
    conn_->query(del_att);  // 忽略失败（可能无附件）
    std::string del_sync = "DELETE FROM sync_state WHERE email_id=" + sql_int(email_id);
    conn_->query(del_sync);
    std::string sql = "DELETE FROM emails WHERE id=" + sql_int(email_id);
    return conn_->query(sql);
}

bool DbManager::mark_flagged(int email_id, bool flagged) {
    std::string sql = "UPDATE emails SET is_flagged=" + sql_bool(flagged) + " WHERE id=" + sql_int(email_id);
    return conn_->query(sql);
}

bool DbManager::move_to_folder(int email_id, const std::string& folder) {
    std::string sql = "UPDATE emails SET folder=" + sql_quote(conn_, folder) + " WHERE id=" + sql_int(email_id);
    return conn_->query(sql);
}

std::vector<Email> DbManager::get_emails(int account_id, const std::string& folder,
                                         int limit, int offset) {
    std::vector<Email> emails;
    std::ostringstream sql;
    sql << "SELECT * FROM emails WHERE account_id=" << sql_int(account_id)
        << " AND folder=" << sql_quote(conn_, folder)
        << " AND is_deleted=0 ORDER BY received_date DESC"
        << " LIMIT " << limit << " OFFSET " << offset;

    if (!conn_->query(sql.str())) return emails;

    MYSQL_RES* result = conn_->store_result();
    if (!result) return emails;

    // 先收集所有 email_id，用于批量查附件
    std::vector<int> email_ids;
    std::vector<Email> temp;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Email email = row_to_email(row);
        email_ids.push_back(email.id);
        temp.push_back(email);
    }
    mysql_free_result(result);

    // 批量加载附件（1 次 SQL 替代 N 次）
    auto att_map = get_attachments_batch(email_ids);
    for (auto& email : temp) {
        auto it = att_map.find(email.id);
        if (it != att_map.end()) {
            email.attachments = it->second;
        }
        email.has_attachments = !email.attachments.empty();
        emails.push_back(email);
    }
    return emails;
}

std::vector<Email> DbManager::get_deleted_emails(int account_id,
                                                int limit, int offset) {
    std::vector<Email> emails;
    std::ostringstream sql;
    sql << "SELECT * FROM emails WHERE account_id=" << sql_int(account_id)
        << " AND is_deleted=1 ORDER BY updated_at DESC"
        << " LIMIT " << limit << " OFFSET " << offset;

    if (!conn_->query(sql.str())) return emails;
    MYSQL_RES* result = conn_->store_result();
    if (!result) return emails;

    std::vector<int> email_ids;
    std::vector<Email> temp;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Email email = row_to_email(row);
        email_ids.push_back(email.id);
        temp.push_back(email);
    }
    mysql_free_result(result);

    auto att_map = get_attachments_batch(email_ids);
    for (auto& email : temp) {
        auto it = att_map.find(email.id);
        if (it != att_map.end()) {
            email.attachments = it->second;
        }
        email.has_attachments = !email.attachments.empty();
        emails.push_back(email);
    }
    return emails;
}

Email DbManager::get_email(int email_id) {
    Email email;
    std::string sql = "SELECT * FROM emails WHERE id=" + sql_int(email_id);

    if (!conn_->query(sql)) return email;

    MYSQL_RES* result = conn_->store_result();
    if (!result) return email;

    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
        email = row_to_email(row);
        email.attachments = get_attachments(email.id);
        email.has_attachments = !email.attachments.empty();
    }
    mysql_free_result(result);
    return email;
}

int DbManager::get_unread_count(int account_id, const std::string& folder) {
    std::ostringstream sql;
    sql << "SELECT COUNT(*) FROM emails WHERE account_id=" << sql_int(account_id)
        << " AND folder=" << sql_quote(conn_, folder)
        << " AND is_read=0 AND is_deleted=0";

    if (!conn_->query(sql.str())) return 0;

    MYSQL_RES* result = conn_->store_result();
    if (!result) return 0;

    MYSQL_ROW row = mysql_fetch_row(result);
    int count = (row && row[0]) ? std::stoi(row[0]) : 0;
    mysql_free_result(result);
    return count;
}

int DbManager::get_total_count(int account_id, const std::string& folder) {
    std::ostringstream sql;
    sql << "SELECT COUNT(*) FROM emails WHERE account_id=" << sql_int(account_id)
        << " AND folder=" << sql_quote(conn_, folder)
        << " AND is_deleted=0";

    if (!conn_->query(sql.str())) return 0;

    MYSQL_RES* result = conn_->store_result();
    if (!result) return 0;

    MYSQL_ROW row = mysql_fetch_row(result);
    int count = (row && row[0]) ? std::stoi(row[0]) : 0;
    mysql_free_result(result);
    return count;
}

int DbManager::get_email_count(int account_id, const std::string& folder) {
    std::ostringstream sql;
    sql << "SELECT COUNT(*) FROM emails WHERE account_id=" << sql_int(account_id)
        << " AND folder=" << sql_quote(conn_, folder)
        << " AND is_deleted=0";

    if (!conn_->query(sql.str())) return 0;
    MYSQL_RES* result = conn_->store_result();
    if (!result) return 0;
    MYSQL_ROW row = mysql_fetch_row(result);
    int count = (row && row[0]) ? std::stoi(row[0]) : 0;
    mysql_free_result(result);
    return count;
}

int DbManager::get_deleted_count(int account_id) {
    std::ostringstream sql;
    sql << "SELECT COUNT(*) FROM emails WHERE account_id=" << sql_int(account_id)
        << " AND is_deleted=1";

    if (!conn_->query(sql.str())) return 0;
    MYSQL_RES* result = conn_->store_result();
    if (!result) return 0;
    MYSQL_ROW row = mysql_fetch_row(result);
    int count = (row && row[0]) ? std::stoi(row[0]) : 0;
    mysql_free_result(result);
    return count;
}

Email DbManager::row_to_email(char** row) {
    Email email;
    int col = 0;
    email.id = row[col] ? std::stoi(row[col]) : 0; ++col;
    email.account_id = row[col] ? std::stoi(row[col]) : 0; ++col;
    email.folder = row[col] ? row[col] : "inbox"; ++col;
    email.message_id = row[col] ? row[col] : ""; ++col;
    email.pop3_uid = row[col] ? row[col] : ""; ++col;
    email.sender_name = row[col] ? row[col] : ""; ++col;
    email.sender_addr = row[col] ? row[col] : ""; ++col;
    email.to = Email::split_recipients(row[col] ? row[col] : ""); ++col;
    email.cc = Email::split_recipients(row[col] ? row[col] : ""); ++col;
    email.bcc = Email::split_recipients(row[col] ? row[col] : ""); ++col;
    email.subject = row[col] ? row[col] : ""; ++col;
    email.body_plain = row[col] ? row[col] : ""; ++col;
    email.body_html = row[col] ? row[col] : ""; ++col;
    email.is_read = row[col] ? (std::stoi(row[col]) != 0) : false; ++col;
    email.is_deleted = row[col] ? (std::stoi(row[col]) != 0) : false; ++col;
    email.is_flagged = row[col] ? (std::stoi(row[col]) != 0) : false; ++col;
    email.has_attachments = row[col] ? (std::stoi(row[col]) != 0) : false; ++col;
    email.received_date = row[col] ? row[col] : ""; ++col;
    return email;
}

// ========== Attachments ==========

int DbManager::add_attachment(int email_id, const Attachment& att) {
    std::ostringstream sql;
    sql << "INSERT INTO attachments (email_id, file_name, file_path, mime_type, file_size, content_id) VALUES ("
        << sql_int(email_id) << ", "
        << sql_quote(conn_, att.file_name) << ", "
        << sql_quote(conn_, att.file_path) << ", "
        << sql_quote(conn_, att.mime_type) << ", "
        << att.file_size << ", "
        << sql_quote(conn_, att.content_id) << ")";

    if (!conn_->query(sql.str())) {
        return -1;
    }
    return (int)conn_->last_insert_id();
}

bool DbManager::update_attachment_path(int attachment_id, const std::string& path) {
    std::string sql = "UPDATE attachments SET file_path=" + sql_quote(conn_, path) +
                      " WHERE id=" + sql_int(attachment_id);
    return conn_->query(sql);
}

std::vector<Attachment> DbManager::get_attachments(int email_id) {
    std::vector<Attachment> attachments;
    std::string sql = "SELECT * FROM attachments WHERE email_id=" + sql_int(email_id);

    if (!conn_->query(sql)) return attachments;

    MYSQL_RES* result = conn_->store_result();
    if (!result) return attachments;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        attachments.push_back(row_to_attachment(row));
    }
    mysql_free_result(result);
    return attachments;
}

std::unordered_map<int, std::vector<Attachment>>
DbManager::get_attachments_batch(const std::vector<int>& email_ids) {
    std::unordered_map<int, std::vector<Attachment>> result;
    if (email_ids.empty()) return result;

    std::ostringstream sql;
    sql << "SELECT * FROM attachments WHERE email_id IN (";
    for (size_t i = 0; i < email_ids.size(); ++i) {
        if (i > 0) sql << ", ";
        sql << email_ids[i];
    }
    sql << ")";

    if (!conn_->query(sql.str())) return result;
    MYSQL_RES* mysql_result = conn_->store_result();
    if (!mysql_result) return result;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(mysql_result))) {
        Attachment att = row_to_attachment(row);
        // email_id 是 row 的第 2 列（index 1）
        int email_id = row[1] ? std::stoi(row[1]) : 0;
        result[email_id].push_back(att);
    }
    mysql_free_result(mysql_result);
    return result;
}

Attachment DbManager::row_to_attachment(char** row) {
    Attachment att;
    int col = 0;
    att.id = row[col] ? std::stoi(row[col]) : -1; ++col;
    // skip email_id
    ++col;
    att.file_name = row[col] ? row[col] : ""; ++col;
    att.file_path = row[col] ? row[col] : ""; ++col;
    att.mime_type = row[col] ? row[col] : ""; ++col;
    att.file_size = row[col] ? std::stoll(row[col]) : 0; ++col;
    att.content_id = row[col] ? row[col] : ""; ++col;
    return att;
}

// ========== Sync State ==========

bool DbManager::is_uid_downloaded(int account_id, const std::string& uid) {
    std::ostringstream sql;
    sql << "SELECT COUNT(*) FROM sync_state WHERE account_id=" << sql_int(account_id)
        << " AND pop3_uid=" << sql_quote(conn_, uid);

    if (!conn_->query(sql.str())) return false;

    MYSQL_RES* result = conn_->store_result();
    if (!result) return false;

    MYSQL_ROW row = mysql_fetch_row(result);
    int count = (row && row[0]) ? std::stoi(row[0]) : 0;
    mysql_free_result(result);
    return count > 0;
}

std::unordered_set<std::string> DbManager::get_downloaded_uids(int account_id) {
    std::unordered_set<std::string> uids;
    std::string sql = "SELECT pop3_uid FROM sync_state WHERE account_id=" + sql_int(account_id);
    if (!conn_->query(sql)) return uids;

    MYSQL_RES* result = conn_->store_result();
    if (!result) return uids;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        if (row[0] && row[0][0] != '\0') {
            uids.insert(row[0]);
        }
    }
    mysql_free_result(result);
    return uids;
}

int DbManager::get_email_id_by_uid(int account_id, const std::string& uid) {
    if (uid.empty()) return -1;

    std::ostringstream sql;
    sql << "SELECT email_id FROM sync_state WHERE account_id=" << sql_int(account_id)
        << " AND pop3_uid=" << sql_quote(conn_, uid)
        << " LIMIT 1";

    if (!conn_->query(sql.str())) return -1;

    MYSQL_RES* result = conn_->store_result();
    if (!result) return -1;

    MYSQL_ROW row = mysql_fetch_row(result);
    int email_id = (row && row[0]) ? std::stoi(row[0]) : -1;
    mysql_free_result(result);
    return email_id;
}

bool DbManager::mark_uid_downloaded(int account_id, const std::string& uid, int email_id) {
    std::ostringstream sql;
    sql << "INSERT INTO sync_state (account_id, pop3_uid, email_id) VALUES ("
        << sql_int(account_id) << ", "
        << sql_quote(conn_, uid) << ", "
        << sql_int(email_id) << ")";
    return conn_->query(sql.str());
}

// ========== Contacts ==========

bool DbManager::add_contact(int account_id, const std::string& name, const std::string& email_addr) {
    std::ostringstream sql;
    sql << "INSERT INTO contacts (account_id, display_name, email_address) VALUES ("
        << sql_int(account_id) << ", "
        << sql_quote(conn_, name) << ", "
        << sql_quote(conn_, email_addr) << ") "
        << "ON DUPLICATE KEY UPDATE display_name=" << sql_quote(conn_, name);
    return conn_->query(sql.str());
}

bool DbManager::delete_contact(int contact_id) {
    std::string sql = "DELETE FROM contacts WHERE id=" + sql_int(contact_id);
    return conn_->query(sql);
}

std::vector<std::pair<int, std::pair<std::string, std::string>>> DbManager::get_contacts(int account_id) {
    std::vector<std::pair<int, std::pair<std::string, std::string>>> contacts;
    std::string sql = "SELECT id, display_name, email_address FROM contacts WHERE account_id="
                    + sql_int(account_id) + " ORDER BY display_name";

    if (!conn_->query(sql)) return contacts;

    MYSQL_RES* result = conn_->store_result();
    if (!result) return contacts;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        int id = row[0] ? std::stoi(row[0]) : 0;
        std::string name = row[1] ? row[1] : "";
        std::string email = row[2] ? row[2] : "";
        contacts.push_back({id, {name, email}});
    }
    mysql_free_result(result);
    return contacts;
}
