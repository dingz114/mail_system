#ifndef MIME_DECODER_H
#define MIME_DECODER_H

#include "email.h"
#include <string>
#include <vector>
#include <map>

class MimeDecoder {
public:
    // Parse a raw email string into an Email object.
    // If save_attachments_dir is non-empty, attachment bodies are saved as files
    // under that directory and Attachment::file_path / file_size are populated.
    Email decode(const std::string& raw_email, int account_id,
                 const std::string& save_attachments_dir = "") const;

    // Decode RFC 2047 encoded words: =?charset?encoding?text?=
    std::string decode_rfc2047(const std::string& encoded) const;

private:
    // Parse headers from the header section
    void parse_headers(const std::string& header_text, Email& email) const;

    // Parse the body according to Content-Type
    void parse_body(const std::string& body_text, Email& email,
                    const std::string& content_type = "",
                    const std::string& save_dir = "") const;

    // Parse a multipart body
    void parse_multipart(const std::string& body, const std::string& boundary,
                         Email& email, const std::string& save_dir = "") const;

    // Decode Content-Transfer-Encoding: base64, quoted-printable, 7bit, 8bit
    std::string decode_transfer_encoding(const std::string& text, const std::string& encoding) const;

    // Convert charset to UTF-8
    std::string convert_to_utf8(const std::string& text, const std::string& charset) const;

    // Parse a Content-Type header value to get the main type and parameters
    void parse_content_type(const std::string& header_value,
                            std::string& media_type,
                            std::map<std::string, std::string>& params) const;

    // Extract email address and optional display name from a "From" value
    void parse_address(const std::string& value, std::string& name, std::string& addr) const;

    // Decode Quoted-Printable encoding
    std::string decode_quoted_printable(const std::string& text) const;

    // Parse date string
    std::string parse_date(const std::string& date_str) const;
};

#endif // MIME_DECODER_H
