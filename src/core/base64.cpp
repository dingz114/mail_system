#include "base64.h"

const std::string Base64::base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

bool Base64::is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string Base64::encode(const std::string& input) {
    return encode(reinterpret_cast<const unsigned char*>(input.data()), input.size());
}

std::string Base64::encode(const unsigned char* data, size_t len) {
    std::string result;
    result.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        unsigned char b1 = data[i];
        unsigned char b2 = (i + 1 < len) ? data[i + 1] : 0;
        unsigned char b3 = (i + 2 < len) ? data[i + 2] : 0;

        result += base64_chars[(b1 >> 2) & 0x3F];
        result += base64_chars[((b1 & 0x03) << 4) | ((b2 >> 4) & 0x0F)];
        result += (i + 1 < len) ? base64_chars[((b2 & 0x0F) << 2) | ((b3 >> 6) & 0x03)] : '=';
        result += (i + 2 < len) ? base64_chars[b3 & 0x3F] : '=';
    }

    return result;
}

std::string Base64::decode(const std::string& input) {
    std::string result;
    result.reserve((input.size() / 4) * 3);

    int val = 0;
    int valb = -8;
    for (unsigned char c : input) {
        if (c == '=') break;
        if (!is_base64(c)) continue;

        val = (val << 6) + static_cast<int>(base64_chars.find(c));
        valb += 6;
        if (valb >= 0) {
            result.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    return result;
}
