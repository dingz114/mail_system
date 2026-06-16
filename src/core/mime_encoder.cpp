#include "mime_encoder.h"
#include "base64.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <ctime>
#include <cstdlib>
#include <random>
#include <QFile>
#include <QFileInfo>
#include <QString>

#ifdef _WIN32
    #include <windows.h>
#endif

static std::string charset_convert(const std::string& text, const std::string& from_charset, const std::string& to_charset) {
#ifdef _WIN32
    // Use Windows API for charset conversion
    int from_cp = CP_UTF8;
    if (from_charset == "utf-8" || from_charset == "UTF-8") from_cp = CP_UTF8;
    else if (from_charset == "gbk" || from_charset == "GBK" || from_charset == "gb2312" || from_charset == "GB2312") from_cp = 936;
    else from_cp = CP_UTF8;

    int to_cp = CP_UTF8;
    if (to_charset == "utf-8" || to_charset == "UTF-8") to_cp = CP_UTF8;
    else if (to_charset == "gbk" || to_charset == "GBK" || to_charset == "gb2312" || to_charset == "GB2312") to_cp = 936;
    else to_cp = CP_UTF8;

    // Wide char intermediate
    int wlen = MultiByteToWideChar(from_cp, 0, text.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return text;
    std::vector<wchar_t> wbuf(wlen);
    MultiByteToWideChar(from_cp, 0, text.c_str(), -1, wbuf.data(), wlen);

    int mlen = WideCharToMultiByte(to_cp, 0, wbuf.data(), -1, nullptr, 0, nullptr, nullptr);
    if (mlen <= 0) return text;
    std::vector<char> mbuf(mlen);
    WideCharToMultiByte(to_cp, 0, wbuf.data(), -1, mbuf.data(), mlen, nullptr, nullptr);

    return std::string(mbuf.data());
#else
    return text;
#endif
}

std::string MimeEncoder::encode(const Email& email) const {
    std::ostringstream oss;

    // Date header
    oss << "Date: " << format_date() << "\r\n";

    // From
    if (!email.sender_name.empty()) {
        std::string encoded_name = encode_header(email.sender_name);
        oss << "From: " << encoded_name << " <" << email.sender_addr << ">\r\n";
    } else {
        oss << "From: " << email.sender_addr << "\r\n";
    }

    // To
    oss << "To: ";
    for (size_t i = 0; i < email.to.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << email.to[i];
    }
    oss << "\r\n";

    // CC
    if (!email.cc.empty()) {
        oss << "Cc: ";
        for (size_t i = 0; i < email.cc.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << email.cc[i];
        }
        oss << "\r\n";
    }

    // Subject
    oss << "Subject: " << encode_header(email.subject.empty() ? "(No Subject)" : email.subject) << "\r\n";

    // MIME-Version
    oss << "MIME-Version: 1.0\r\n";

    bool has_attachments = !email.attachments.empty();
    bool has_html = !email.body_html.empty();

    if (!has_attachments && !has_html) {
        // Simple plain text email
        oss << "Content-Type: text/plain; charset=\"utf-8\"\r\n";
        oss << "Content-Transfer-Encoding: base64\r\n";
        oss << "\r\n";
        oss << Base64::encode(email.body_plain);
    } else if (!has_attachments && has_html) {
        // multipart/alternative: plain + html
        std::string boundary = generate_boundary();
        oss << "Content-Type: multipart/alternative; boundary=\"" << boundary << "\"\r\n";
        oss << "\r\n";

        // Plain text part
        oss << "--" << boundary << "\r\n";
        oss << "Content-Type: text/plain; charset=\"utf-8\"\r\n";
        oss << "Content-Transfer-Encoding: base64\r\n";
        oss << "\r\n";
        oss << Base64::encode(email.body_plain.empty() ? "" : email.body_plain);
        oss << "\r\n";

        // HTML part
        oss << "--" << boundary << "\r\n";
        oss << "Content-Type: text/html; charset=\"utf-8\"\r\n";
        oss << "Content-Transfer-Encoding: base64\r\n";
        oss << "\r\n";
        oss << Base64::encode(email.body_html);
        oss << "\r\n";

        oss << "--" << boundary << "--\r\n";
    } else {
        // multipart/mixed with attachments (contains multipart/alternative for body)
        std::string mixed_boundary = generate_boundary();
        oss << "Content-Type: multipart/mixed; boundary=\"" << mixed_boundary << "\"\r\n";
        oss << "\r\n";

        // Body part(s)
        if (has_html) {
            std::string alt_boundary = generate_boundary();
            oss << "--" << mixed_boundary << "\r\n";
            oss << "Content-Type: multipart/alternative; boundary=\"" << alt_boundary << "\"\r\n";
            oss << "\r\n";

            // Plain
            oss << "--" << alt_boundary << "\r\n";
            oss << "Content-Type: text/plain; charset=\"utf-8\"\r\n";
            oss << "Content-Transfer-Encoding: base64\r\n";
            oss << "\r\n";
            oss << Base64::encode(email.body_plain.empty() ? "" : email.body_plain);
            oss << "\r\n";

            // HTML
            oss << "--" << alt_boundary << "\r\n";
            oss << "Content-Type: text/html; charset=\"utf-8\"\r\n";
            oss << "Content-Transfer-Encoding: base64\r\n";
            oss << "\r\n";
            oss << Base64::encode(email.body_html);
            oss << "\r\n";

            oss << "--" << alt_boundary << "--\r\n";
        } else {
            oss << "--" << mixed_boundary << "\r\n";
            oss << "Content-Type: text/plain; charset=\"utf-8\"\r\n";
            oss << "Content-Transfer-Encoding: base64\r\n";
            oss << "\r\n";
            oss << Base64::encode(email.body_plain);
            oss << "\r\n";
        }

        // Attachment parts
        for (const auto& att : email.attachments) {
            oss << build_attachment_part(att, mixed_boundary);
        }

        oss << "--" << mixed_boundary << "--\r\n";
    }

    return oss.str();
}

std::string MimeEncoder::encode_header(const std::string& text) const {
    // Check if encoding is needed (contains non-ASCII)
    bool needs_encode = false;
    for (unsigned char c : text) {
        if (c > 127) {
            needs_encode = true;
            break;
        }
    }

    if (!needs_encode) {
        // Also check for special chars that need quoting
        for (unsigned char c : text) {
            if (c < 32 || c == '"' || c == '\\') {
                needs_encode = true;
                break;
            }
        }
    }

    if (!needs_encode) {
        return text;
    }

    // RFC 2047: =?UTF-8?B?<base64>?=
    return "=?UTF-8?B?" + Base64::encode(text) + "?=";
}

std::string MimeEncoder::generate_boundary() const {
    // Generate a unique boundary
    static std::mt19937 rng(static_cast<unsigned>(std::time(nullptr) + rand()));
    std::uniform_int_distribution<int> dist(10000000, 99999999);
    std::ostringstream oss;
    oss << "----=_Part_" << dist(rng) << "_" << dist(rng) << "." << dist(rng);
    return oss.str();
}

std::string MimeEncoder::format_date() const {
    auto now = std::time(nullptr);
    std::tm tm_local;
    std::tm tm_utc;
#ifdef _WIN32
    localtime_s(&tm_local, &now);
    gmtime_s(&tm_utc, &now);
#else
    localtime_r(&now, &tm_local);
    gmtime_r(&now, &tm_utc);
#endif

    char buf[128];
    // RFC 2822 format: Thu, 15 Jun 2026 10:00:00 +0800
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S", &tm_local);

    std::tm tm_utc_as_local = tm_utc;
    const long tz_offset_minutes = static_cast<long>(
        std::difftime(std::mktime(&tm_local), std::mktime(&tm_utc_as_local)) / 60);

    char tz_buf[8];
    const char sign = tz_offset_minutes >= 0 ? '+' : '-';
    const long abs_offset = std::labs(tz_offset_minutes);
    std::snprintf(tz_buf, sizeof(tz_buf), " %c%02ld%02ld",
                  sign, abs_offset / 60, abs_offset % 60);

    std::string date_str(buf);
    date_str += tz_buf;

    return date_str;
}

std::string MimeEncoder::encode_attachment(const Attachment& att) const {
    // QFile handles Unicode Windows paths correctly; std::ifstream with a
    // narrow UTF-8 path may fail for Chinese filenames.
    QFile file(QString::fromStdString(att.file_path));
    if (!file.open(QIODevice::ReadOnly)) return "";

    QByteArray bytes = file.readAll();
    if (bytes.isEmpty() && file.size() > 0) return "";

    return Base64::encode(reinterpret_cast<const unsigned char*>(bytes.constData()),
                          static_cast<size_t>(bytes.size()));
}

std::string MimeEncoder::build_attachment_part(const Attachment& att, const std::string& boundary) const {
    std::ostringstream oss;
    oss << "--" << boundary << "\r\n";

    // Determine MIME type
    std::string mime_type = att.mime_type;
    if (mime_type.empty()) {
        mime_type = "application/octet-stream";
    }

    long long file_size = att.file_size;
    QString file_path = QString::fromStdString(att.file_path);
    QFileInfo file_info(file_path);
    if (file_info.exists() && file_info.isFile()) {
        file_size = file_info.size();
    }

    oss << "Content-Type: " << mime_type << "; name=\"" << encode_header(att.file_name) << "\"\r\n";
    oss << "Content-Transfer-Encoding: base64\r\n";
    oss << "Content-Disposition: attachment; filename=\"" << encode_header(att.file_name) << "\"";
    if (file_size > 0) {
        oss << "; size=" << file_size;
    }
    oss << "\r\n";
    if (!att.content_id.empty()) {
        oss << "Content-ID: <" << att.content_id << ">\r\n";
    }
    oss << "\r\n";

    std::string encoded = encode_attachment(att);
    // Wrap Base64 at 76 chars per line
    for (size_t i = 0; i < encoded.size(); i += 76) {
        oss << encoded.substr(i, 76) << "\r\n";
    }
    oss << "\r\n";

    return oss.str();
}
