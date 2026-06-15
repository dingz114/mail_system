#ifndef BASE64_H
#define BASE64_H

#include <string>

class Base64 {
public:
    // Encode binary data to Base64 string
    static std::string encode(const std::string& input);
    static std::string encode(const unsigned char* data, size_t len);

    // Decode Base64 string to binary data
    static std::string decode(const std::string& input);

private:
    static const std::string base64_chars;
    static bool is_base64(unsigned char c);
};

#endif // BASE64_H
