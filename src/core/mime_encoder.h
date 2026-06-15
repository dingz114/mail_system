#ifndef MIME_ENCODER_H
#define MIME_ENCODER_H

#include "email.h"
#include <string>

class MimeEncoder {
public:
    // Convert an Email object to a MIME-formatted string ready for SMTP DATA
    std::string encode(const Email& email) const;

private:
    // Encode a header value using RFC 2047 (only if it contains non-ASCII)
    std::string encode_header(const std::string& text) const;

    // Generate a unique MIME boundary string
    std::string generate_boundary() const;

    // Format a date string for the Date header
    std::string format_date() const;

    // Encode attachment data from file as Base64
    std::string encode_attachment(const Attachment& att) const;

    // Build a single MIME part (used for attachments)
    std::string build_attachment_part(const Attachment& att, const std::string& boundary) const;
};

#endif // MIME_ENCODER_H
