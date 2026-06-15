#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include "db_connection.h"
#include "../core/email.h"
#include <string>
#include <vector>

class DbManager {
public:
    // DbManager takes a pointer to an existing DbConnection (does NOT own it)
    explicit DbManager(DbConnection* conn);

    // ========== Accounts ==========
    int  add_account(const Account& acc);       // Returns new account ID
    bool update_account(const Account& acc);
    bool delete_account(int id);
    std::vector<Account> get_all_accounts();
    Account get_account(int id);
    Account get_account_by_email(const std::string& email);

    // ========== Emails ==========
    int  save_email(const Email& email);         // Returns new email ID
    bool update_email(const Email& email);
    bool mark_read(int email_id);
    bool mark_unread(int email_id);
    bool mark_deleted(int email_id);
    bool mark_undeleted(int email_id);
    bool delete_permanently(int email_id);   // 彻底从数据库删除
    bool mark_flagged(int email_id, bool flagged);
    bool move_to_folder(int email_id, const std::string& folder);
    std::vector<Email> get_emails(int account_id, const std::string& folder);
    std::vector<Email> get_deleted_emails(int account_id);  // 废纸篓
    Email get_email(int email_id);
    int  get_unread_count(int account_id, const std::string& folder);
    int  get_total_count(int account_id, const std::string& folder);

    // ========== Attachments ==========
    int  add_attachment(int email_id, const Attachment& att);
    bool update_attachment_path(int attachment_id, const std::string& path);
    std::vector<Attachment> get_attachments(int email_id);

    // ========== Sync State ==========
    bool is_uid_downloaded(int account_id, const std::string& uid);
    bool mark_uid_downloaded(int account_id, const std::string& uid, int email_id);

    // ========== Contacts ==========
    bool add_contact(int account_id, const std::string& name, const std::string& email_addr);
    bool delete_contact(int contact_id);
    std::vector<std::pair<int, std::pair<std::string, std::string>>> get_contacts(int account_id);
    // Returns: vector of <id, <name, email>>

    std::string last_error() const;

private:
    DbConnection* conn_;

    // Helper: convert MYSQL_ROW to Account
    Account row_to_account(char** row);
    // Helper: convert MYSQL_ROW to Email
    Email row_to_email(char** row);
    // Helper: convert MYSQL_ROW to Attachment
    Attachment row_to_attachment(char** row);
};

#endif // DB_MANAGER_H
