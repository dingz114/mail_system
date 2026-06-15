#include "mime_decoder.h"
#include "base64.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>

// 临时调试：CMake 中加 -DDEBUG_EMAIL_CONTENT 或在文件开头取消注释来启用
// #define DEBUG_EMAIL_CONTENT

#ifdef DEBUG_EMAIL_CONTENT
#define DEBUG_LOG(x) std::cerr << x
#else
#define DEBUG_LOG(x) ((void)0)
#endif

#ifdef _WIN32
    #include <windows.h>
#endif

namespace {

// 将附件内容写入文件，返回文件路径，失败返回空字符串
std::string save_attachment_file(const std::string& save_dir,
                                  const std::string& filename,
                                  const std::string& data) {
    if (save_dir.empty() || data.empty()) return "";

    // 生成唯一文件名：时间戳_序号_原始文件名
    static int seq = 0;
    char buf[64];
    auto now = std::time(nullptr);
    std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", std::localtime(&now));
    std::string safe_name = std::string(buf) + "_" + std::to_string(++seq) + "_" + filename;

    // 确保文件名安全：替换路径分隔符
    for (auto& ch : safe_name) {
        if (ch == '/' || ch == '\\' || ch == ':') ch = '_';
    }

    std::string full_path = save_dir;
    if (!full_path.empty() && full_path.back() != '/' && full_path.back() != '\\')
        full_path += '/';
    full_path += safe_name;

    std::ofstream out(full_path, std::ios::binary);
    if (!out) return "";
    out.write(data.data(), data.size());
    out.close();
    return full_path;
}

std::string trim_copy(const std::string& value) {
    size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool split_header_body(const std::string& text,
                       std::string& headers,
                       std::string& body) {
    size_t pos = text.find("\r\n\r\n");
    size_t sep_len = 4;
    if (pos == std::string::npos) {
        pos = text.find("\n\n");
        sep_len = 2;
    }
    if (pos == std::string::npos) {
        return false;
    }

    headers = text.substr(0, pos);
    body = text.substr(pos + sep_len);
    return true;
}

std::map<std::string, std::string> parse_unfolded_headers(const std::string& header_text) {
    std::map<std::string, std::string> headers;
    std::istringstream iss(header_text);
    std::string line;
    std::string current_name;
    std::string current_value;

    auto flush = [&]() {
        if (!current_name.empty()) {
            headers[lower_copy(current_name)] = trim_copy(current_value);
            current_name.clear();
            current_value.clear();
        }
    };

    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) {
            flush();
            continue;
        }

        if ((line[0] == ' ' || line[0] == '\t') && !current_name.empty()) {
            current_value += " " + trim_copy(line);
            continue;
        }

        flush();
        size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        current_name = line.substr(0, colon);
        current_value = line.substr(colon + 1);
    }
    flush();

    return headers;
}

std::string clean_pop3_payload(const std::string& raw) {
    std::string text = raw;

    if (text.rfind("+OK", 0) == 0) {
        size_t first_crlf = text.find("\r\n");
        size_t first_lf = text.find('\n');
        if (first_crlf != std::string::npos) {
            text.erase(0, first_crlf + 2);
        } else if (first_lf != std::string::npos) {
            text.erase(0, first_lf + 1);
        }
    }

    auto remove_suffix = [&](const std::string& suffix) {
        if (text.size() >= suffix.size() &&
            text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0) {
            text.erase(text.size() - suffix.size());
            return true;
        }
        return false;
    };

    remove_suffix("\r\n.\r\n") || remove_suffix("\n.\n") ||
        remove_suffix("\r\n.") || remove_suffix("\n.");

    std::istringstream iss(text);
    std::ostringstream oss;
    std::string line;
    bool first = true;
    while (std::getline(iss, line)) {
        if (!first) oss << '\n';
        first = false;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("..", 0) == 0) {
            line.erase(0, 1);
        }
        oss << line;
    }

    return oss.str();
}

} // namespace

Email MimeDecoder::decode(const std::string& raw_email, int account_id,
                         const std::string& save_attachments_dir) const {
    Email email;
    email.account_id = account_id;
    email.folder = "inbox";

    if (raw_email.empty()) return email;

    const std::string message = clean_pop3_payload(raw_email);
    DEBUG_LOG("[MimeDecoder] cleaned message size=" << message.size()
              << "\n=== FULL MESSAGE ===\n" << message << "\n=== END ===\n");

    std::string header_text;
    std::string body_text;
    if (!split_header_body(message, header_text, body_text)) {
        email.body_plain = message;
        return email;
    }

    parse_headers(header_text, email);

    std::map<std::string, std::string> top_headers = parse_unfolded_headers(header_text);
    std::string media_type;
    std::map<std::string, std::string> params;
    parse_content_type(top_headers.count("content-type")
                           ? top_headers["content-type"]
                           : "text/plain; charset=\"utf-8\"",
                       media_type, params);

    DEBUG_LOG("[MimeDecoder] top-level Content-Type: " << media_type
              << " has_boundary=" << (params.count("boundary") ? "yes" : "no")
              << " body_size=" << body_text.size()
              << " has_attach=" << email.has_attachments
              << " attach_count=" << email.attachments.size() << std::endl);

    if (media_type.find("multipart/") == 0 && params.count("boundary")) {
        DEBUG_LOG("[MimeDecoder] boundary=" << params["boundary"] << std::endl);
        parse_multipart(body_text, params["boundary"], email, save_attachments_dir);
    } else {
        parse_body(body_text, email, header_text, save_attachments_dir);
    }

    return email;
}

void MimeDecoder::parse_headers(const std::string& header_text, Email& email) const {
    std::istringstream iss(header_text);
    std::string line;
    std::string current_header;
    std::string current_value;

    while (std::getline(iss, line)) {
        // Remove \r at end
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.empty()) {
            // Save current header
            if (!current_header.empty()) {
                std::string lower = current_header;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

                if (lower == "from") {
                    parse_address(decode_rfc2047(current_value), email.sender_name, email.sender_addr);
                } else if (lower == "to") {
                    email.to.push_back(decode_rfc2047(current_value));
                } else if (lower == "cc") {
                    email.cc.push_back(decode_rfc2047(current_value));
                } else if (lower == "subject") {
                    email.subject = decode_rfc2047(current_value);
                } else if (lower == "date") {
                    email.received_date = parse_date(current_value);
                } else if (lower == "message-id") {
                    email.message_id = current_value;
                }
                current_header.clear();
                current_value.clear();
            }
            continue;
        }

        // Check for continuation line (starts with space or tab)
        if ((line[0] == ' ' || line[0] == '\t') && !current_header.empty()) {
            // Unfold: append to current value
            current_value += " " + line.substr(1);
            continue;
        }

        // Save previous header
        if (!current_header.empty()) {
            std::string lower = current_header;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

            if (lower == "from") {
                parse_address(decode_rfc2047(current_value), email.sender_name, email.sender_addr);
            } else if (lower == "to") {
                email.to.push_back(decode_rfc2047(current_value));
            } else if (lower == "cc") {
                email.cc.push_back(decode_rfc2047(current_value));
            } else if (lower == "subject") {
                email.subject = decode_rfc2047(current_value);
            } else if (lower == "date") {
                email.received_date = parse_date(current_value);
            } else if (lower == "message-id") {
                email.message_id = current_value;
            }
        }

        // Parse new header
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            current_header = line.substr(0, colon);
            current_value = line.substr(colon + 1);
            // Trim leading whitespace from value
            size_t start = current_value.find_first_not_of(" \t");
            if (start != std::string::npos) {
                current_value = current_value.substr(start);
            }
        }
    }

    // Don't forget the last header
    if (!current_header.empty()) {
        std::string lower = current_header;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "from") {
            parse_address(decode_rfc2047(current_value), email.sender_name, email.sender_addr);
        } else if (lower == "subject") {
            email.subject = decode_rfc2047(current_value);
        } else if (lower == "date") {
            email.received_date = parse_date(current_value);
        }
    }
}

void MimeDecoder::parse_body(const std::string& body_text, Email& email,
                            const std::string& body_header_hint,
                            const std::string& save_dir) const {
    std::map<std::string, std::string> params;
    std::string media_type = "text/plain";

    // Check if body_text starts with headers (for multipart sub-parts)
    std::string part_headers;
    std::string part_body = body_text;

    if (!body_header_hint.empty()) {
        part_headers = body_header_hint;
    } else {
        split_header_body(body_text, part_headers, part_body);
    }

    std::string transfer_encoding = "7bit";
    std::string charset = "utf-8";
    if (!part_headers.empty()) {
        auto headers = parse_unfolded_headers(part_headers);
        if (headers.count("content-transfer-encoding")) {
            transfer_encoding = lower_copy(headers["content-transfer-encoding"]);
        }
        if (headers.count("content-type")) {
            parse_content_type(headers["content-type"], media_type, params);
        }
        if (params.count("charset")) {
            charset = params["charset"];
        }
    }

    std::string decoded = decode_transfer_encoding(part_body, transfer_encoding);
    decoded = convert_to_utf8(decoded, charset);

    if (media_type == "text/html") {
        email.body_html = decoded;
    } else {
        email.body_plain = decoded;
    }
}

void MimeDecoder::parse_multipart(const std::string& body, const std::string& boundary,
                                 Email& email, const std::string& save_dir) const {
    // Split body by boundary
    std::string delimiter = "--" + boundary;
    size_t pos = 0;
    DEBUG_LOG("[MimeDecoder] parse_multipart boundary=" << boundary
              << " body_size=" << body.size() << std::endl);

    while (true) {
        size_t next = body.find(delimiter, pos);
        if (next == std::string::npos) break;

        // Check if it's the end boundary (--boundary--)
        if (next + delimiter.size() + 2 <= body.size()) {
            if (body.substr(next + delimiter.size(), 2) == "--") {
                break;  // End of multipart
            }
        }

        // Move past the delimiter line. Real mail normally uses CRLF, while
        // some POP3 paths and tests may already be normalized to LF.
        size_t part_start = body.find("\r\n", next);
        size_t line_break_len = 2;
        if (part_start == std::string::npos) {
            part_start = body.find('\n', next);
            line_break_len = 1;
        }
        if (part_start == std::string::npos) break;
        part_start += line_break_len;

        // Find the next boundary to determine part end
        size_t part_end = body.find(delimiter, part_start);
        if (part_end == std::string::npos) {
            part_end = body.size();
        } else {
            // Go back before the line break before the delimiter.
            if (part_end >= 2 && body.substr(part_end - 2, 2) == "\r\n") {
                part_end -= 2;
            } else if (part_end >= 1 && body[part_end - 1] == '\n') {
                part_end -= 1;
            }
        }

        std::string part = body.substr(part_start, part_end - part_start);
        pos = part_end;

        // Parse this part's headers
        std::string part_headers;
        std::string part_body;
        if (!split_header_body(part, part_headers, part_body)) continue;

        // Parse headers for this part
        std::string media_type = "text/plain";
        std::map<std::string, std::string> params;
        std::string transfer_encoding = "7bit";
        std::string content_disposition;
        std::string attachment_filename;
        std::string content_id;

        auto headers = parse_unfolded_headers(part_headers);
        if (headers.count("content-type")) {
            parse_content_type(headers["content-type"], media_type, params);
        }
        if (headers.count("content-transfer-encoding")) {
            transfer_encoding = lower_copy(headers["content-transfer-encoding"]);
        }
        if (headers.count("content-disposition")) {
            content_disposition = headers["content-disposition"];
                // Extract filename
                size_t fname = content_disposition.find("filename=");
                if (fname != std::string::npos) {
                    size_t fstart = fname + 9;
                    if (fstart < content_disposition.size()) {
                        char quote = content_disposition[fstart];
                        if (quote == '"') {
                            size_t fend = content_disposition.find('"', fstart + 1);
                            if (fend != std::string::npos) {
                                attachment_filename = content_disposition.substr(fstart + 1, fend - fstart - 1);
                            }
                        } else {
                            attachment_filename = content_disposition.substr(fstart);
                            size_t semi = attachment_filename.find(';');
                            if (semi != std::string::npos) attachment_filename = attachment_filename.substr(0, semi);
                        }
                        attachment_filename = decode_rfc2047(attachment_filename);
                    }
                }
                // Extract name from Content-Type if not in Content-Disposition
                if (attachment_filename.empty() && params.count("name")) {
                    attachment_filename = decode_rfc2047(params["name"]);
                }
        }
        if (headers.count("content-id")) {
            content_id = headers["content-id"];
        }

        // Decode body
        std::string decoded_body = decode_transfer_encoding(part_body, transfer_encoding);
        std::string charset = params.count("charset") ? params["charset"] : "utf-8";
        decoded_body = convert_to_utf8(decoded_body, charset);

        // Determine what to do with this part
        std::string lower_disposition = content_disposition;
        std::transform(lower_disposition.begin(), lower_disposition.end(),
                     lower_disposition.begin(), ::tolower);

        bool is_attachment = (lower_disposition.find("attachment") != std::string::npos);

        if (is_attachment || media_type.find("image/") == 0 ||
            media_type.find("application/") == 0 ||
            media_type.find("audio/") == 0 || media_type.find("video/") == 0) {
            DEBUG_LOG("[MimeDecoder] part: media_type=" << media_type
                      << " disposition=" << content_disposition
                      << " transfer_enc=" << transfer_encoding
                      << " is_attachment=" << is_attachment
                      << " filename=" << attachment_filename
                      << " raw_body_size=" << part_body.size()
                      << " decoded_size=" << decoded_body.size()
                      << " first50=" << part_body.substr(0, 50) << std::endl);
        }

        if (is_attachment) {
            Attachment att;
            att.file_name = attachment_filename;
            att.file_path = save_attachment_file(save_dir, attachment_filename, decoded_body);
            att.file_size = decoded_body.size();
            att.mime_type = media_type;
            att.content_id = content_id;
            email.attachments.push_back(att);
            email.has_attachments = true;
        } else if (media_type == "text/plain") {
            if (email.body_plain.empty()) {
                email.body_plain = decoded_body;
            }
        } else if (media_type == "text/html") {
            email.body_html = decoded_body;
        } else if (media_type.find("multipart/") == 0 && params.count("boundary")) {
            // Nested multipart
            parse_multipart(part_body, params["boundary"], email, save_dir);
        } else if (media_type.find("image/") == 0 || media_type.find("application/") == 0 ||
                   media_type.find("audio/") == 0 || media_type.find("video/") == 0) {
            // Inline media — save and treat as attachment
            Attachment att;
            att.file_name = attachment_filename.empty() ? "attachment" : attachment_filename;
            att.file_path = save_attachment_file(save_dir, att.file_name, decoded_body);
            att.file_size = decoded_body.size();
            att.mime_type = media_type;
            att.content_id = content_id;
            email.attachments.push_back(att);
            email.has_attachments = true;
        } else {
            // Default: use as body if plain text seems suitable
            if (email.body_plain.empty()) {
                email.body_plain += decoded_body;
            }
        }
    }
}

std::string MimeDecoder::decode_rfc2047(const std::string& encoded) const {
    std::string result;
    size_t pos = 0;

    while (pos < encoded.size()) {
        size_t start = encoded.find("=?", pos);
        if (start == std::string::npos) {
            result += encoded.substr(pos);
            break;
        }

        // Copy text before encoded word
        result += encoded.substr(pos, start - pos);

        // Find the end
        size_t end = encoded.find("?=", start + 2);
        if (end == std::string::npos) {
            result += encoded.substr(start);
            break;
        }

        // Parse: =?charset?encoding?text?=
        std::string word = encoded.substr(start + 2, end - start - 2);
        size_t q1 = word.find('?');
        size_t q2 = word.find('?', q1 + 1);

        if (q1 != std::string::npos && q2 != std::string::npos) {
            std::string charset = word.substr(0, q1);
            std::string encoding = word.substr(q1 + 1, q2 - q1 - 1);
            std::string text = word.substr(q2 + 1);

            std::string decoded;
            if (encoding == "B" || encoding == "b") {
                decoded = Base64::decode(text);
            } else if (encoding == "Q" || encoding == "q") {
                decoded = decode_quoted_printable(text);
                // Replace underscores with spaces (Q encoding)
                std::replace(decoded.begin(), decoded.end(), '_', ' ');
            } else {
                decoded = text;
            }

            // Convert charset
            decoded = convert_to_utf8(decoded, charset);
            result += decoded;
        } else {
            result += encoded.substr(start, end - start + 2);
        }

        pos = end + 2;
    }

    return result;
}

std::string MimeDecoder::decode_transfer_encoding(const std::string& text, const std::string& encoding) const {
    if (encoding == "base64") {
        // Remove line breaks and whitespace
        std::string clean;
        for (char c : text) {
            if (c != '\r' && c != '\n' && c != ' ' && c != '\t') {
                clean += c;
            }
        }
        return Base64::decode(clean);
    } else if (encoding == "quoted-printable") {
        return decode_quoted_printable(text);
    } else {
        // 7bit, 8bit, binary — return as-is
        return text;
    }
}

std::string MimeDecoder::decode_quoted_printable(const std::string& text) const {
    std::string result;
    result.reserve(text.size());

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '=' && i + 2 < text.size()) {
            // Check if it's a hex sequence
            char c1 = text[i + 1];
            char c2 = text[i + 2];
            if (std::isxdigit(static_cast<unsigned char>(c1)) &&
                std::isxdigit(static_cast<unsigned char>(c2))) {
                int hi = std::isdigit(static_cast<unsigned char>(c1)) ? (c1 - '0') : (std::toupper(c1) - 'A' + 10);
                int lo = std::isdigit(static_cast<unsigned char>(c2)) ? (c2 - '0') : (std::toupper(c2) - 'A' + 10);
                result += static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            } else if (c1 == '\r' && c2 == '\n') {
                // Soft line break — skip
                i += 2;
                continue;
            }
        }
        result += text[i];
    }

    return result;
}

std::string MimeDecoder::convert_to_utf8(const std::string& text, const std::string& charset) const {
    if (charset.empty()) return text;

    std::string lower = charset;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "utf-8" || lower == "utf8" || lower == "us-ascii") {
        return text;
    }

#ifdef _WIN32
    // Use Windows API for charset conversion to UTF-8
    int from_cp = CP_UTF8;
    if (lower == "gbk" || lower == "gb2312" || lower == "gb18030") {
        from_cp = 936;  // CP936 = GBK
    } else if (lower == "big5" || lower == "big-5") {
        from_cp = 950;
    } else if (lower == "shift_jis" || lower == "shift-jis" || lower == "sjis") {
        from_cp = 932;
    } else if (lower == "euc-kr" || lower == "euckr") {
        from_cp = 949;
    } else if (lower == "iso-8859-1" || lower == "latin1") {
        from_cp = 28591;
    } else if (lower == "windows-1252" || lower == "cp1252") {
        from_cp = 1252;
    } else if (lower == "koi8-r") {
        from_cp = 20866;
    } else {
        return text;  // Unknown charset, return as-is
    }

    int wlen = MultiByteToWideChar(from_cp, 0, text.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return text;

    std::vector<wchar_t> wbuf(wlen);
    MultiByteToWideChar(from_cp, 0, text.c_str(), -1, wbuf.data(), wlen);

    int ulen = WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), -1, nullptr, 0, nullptr, nullptr);
    if (ulen <= 0) return text;

    std::vector<char> ubuf(ulen);
    WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), -1, ubuf.data(), ulen, nullptr, nullptr);

    return std::string(ubuf.data());
#else
    return text;
#endif
}

void MimeDecoder::parse_content_type(const std::string& header_value,
                                      std::string& media_type,
                                      std::map<std::string, std::string>& params) const {
    // Parse: "text/plain; charset=\"utf-8\"; name=\"file.txt\""
    size_t semi = header_value.find(';');
    if (semi != std::string::npos) {
        media_type = header_value.substr(0, semi);
        // Trim
        size_t end = media_type.find_last_not_of(" \t");
        if (end != std::string::npos) media_type = media_type.substr(0, end + 1);

        // Parse parameters
        std::string param_str = header_value.substr(semi + 1);
        std::istringstream pss(param_str);
        std::string param;
        while (std::getline(pss, param, ';')) {
            size_t eq = param.find('=');
            if (eq != std::string::npos) {
                std::string key = param.substr(0, eq);
                std::string value = param.substr(eq + 1);
                // Trim key
                size_t ks = key.find_first_not_of(" \t");
                size_t ke = key.find_last_not_of(" \t");
                if (ks != std::string::npos && ke != std::string::npos) {
                    key = key.substr(ks, ke - ks + 1);
                    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                }
                // Trim value and remove quotes
                size_t vs = value.find_first_not_of(" \t\"");
                size_t ve = value.find_last_not_of(" \t\"");
                if (vs != std::string::npos && ve != std::string::npos) {
                    value = value.substr(vs, ve - vs + 1);
                }
                params[key] = value;
            }
        }
    } else {
        media_type = header_value;
        size_t end = media_type.find_last_not_of(" \t");
        if (end != std::string::npos) media_type = media_type.substr(0, end + 1);
    }

    std::transform(media_type.begin(), media_type.end(), media_type.begin(), ::tolower);
}

void MimeDecoder::parse_address(const std::string& value, std::string& name, std::string& addr) const {
    // Parse: "Display Name <user@domain.com>" or just "user@domain.com"
    size_t lt = value.find('<');
    size_t gt = value.find('>');

    if (lt != std::string::npos && gt != std::string::npos && gt > lt) {
        name = value.substr(0, lt);
        // Trim name
        size_t start = name.find_first_not_of(" \t\"");
        size_t end = name.find_last_not_of(" \t\"");
        if (start != std::string::npos && end != std::string::npos) {
            name = name.substr(start, end - start + 1);
        } else {
            name.clear();
        }
        addr = value.substr(lt + 1, gt - lt - 1);
    } else {
        name.clear();
        addr = value;
        // Trim
        size_t start = addr.find_first_not_of(" \t");
        size_t end = addr.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            addr = addr.substr(start, end - start + 1);
        }
    }
}

std::string MimeDecoder::parse_date(const std::string& date_str) const {
    // MySQL DATETIME only accepts "YYYY-MM-DD HH:MM:SS". Real mail Date
    // headers are usually RFC 5322, e.g. "Mon, 15 Jun 2026 10:20:05 +0800".
    if (date_str.empty()) {
        auto now = std::time(nullptr);
        char buf[64];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        return std::string(buf);
    }

    size_t s = date_str.find_first_not_of(" \t\r\n");
    size_t e = date_str.find_last_not_of(" \t\r\n");
    if (s == std::string::npos || e == std::string::npos) {
        auto now = std::time(nullptr);
        char buf[64];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        return std::string(buf);
    }

    std::string value = date_str.substr(s, e - s + 1);

    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (std::sscanf(value.c_str(), "%d-%d-%d %d:%d:%d",
                    &year, &month, &day, &hour, &minute, &second) == 6) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                      year, month, day, hour, minute, second);
        return std::string(buf);
    }

    size_t comma = value.find(',');
    if (comma != std::string::npos) {
        value = value.substr(comma + 1);
        size_t start = value.find_first_not_of(" \t");
        if (start != std::string::npos) value = value.substr(start);
    }

    char month_name[8] = {0};
    if (std::sscanf(value.c_str(), "%d %7s %d %d:%d:%d",
                    &day, month_name, &year, &hour, &minute, &second) == 6) {
        std::string mon = month_name;
        std::transform(mon.begin(), mon.end(), mon.begin(), ::tolower);
        static const std::map<std::string, int> months = {
            {"jan", 1}, {"feb", 2}, {"mar", 3}, {"apr", 4},
            {"may", 5}, {"jun", 6}, {"jul", 7}, {"aug", 8},
            {"sep", 9}, {"oct", 10}, {"nov", 11}, {"dec", 12}
        };
        auto it = months.find(mon.substr(0, 3));
        if (it != months.end()) {
            month = it->second;
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                          year, month, day, hour, minute, second);
            return std::string(buf);
        }
    }

    auto now = std::time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return std::string(buf);
}
