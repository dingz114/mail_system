#ifndef EMAIL_H
#define EMAIL_H

#include <string>
#include <vector>
#include <ctime>

struct Attachment {
    int         id = -1;
    std::string file_name;
    std::string file_path;      // Path on local filesystem
    std::string mime_type;
    long long   file_size = 0;
    std::string content_id;     // CID for inline attachments
};

struct Email {
    int         id = -1;        // Database ID, -1 if not saved
    int         account_id = -1;
    std::string folder;         // "inbox", "sent", "drafts", "trash"
    std::string message_id;
    std::string pop3_uid;

    // Headers
    std::string sender_name;
    std::string sender_addr;
    std::vector<std::string> to;
    std::vector<std::string> cc;
    std::vector<std::string> bcc;
    std::string subject;

    // Body
    std::string body_plain;
    std::string body_html;

    // Flags
    bool is_read = false;
    bool is_deleted = false;
    bool is_flagged = false;
    bool has_attachments = false;

    // Attachments
    std::vector<Attachment> attachments;

    // Dates
    std::string received_date;  // ISO 8601 format

    // Helper: format recipients as semicolon-separated string
    static std::string join_recipients(const std::vector<std::string>& list);
    static std::vector<std::string> split_recipients(const std::string& str);
};

struct Account {
    int         id = -1;
    std::string account_name;
    std::string email_address;
    std::string username;
    std::string password;
    std::string smtp_server;
    int         smtp_port = 465;
    bool        smtp_ssl = true;
    std::string pop3_server;
    int         pop3_port = 995;
    bool        pop3_ssl = true;
    bool        leave_on_server = true;
};

#endif // EMAIL_H
