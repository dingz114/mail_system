#include "email.h"
#include <sstream>

std::string Email::join_recipients(const std::vector<std::string>& list) {
    std::ostringstream oss;
    for (size_t i = 0; i < list.size(); ++i) {
        if (i > 0) oss << "; ";
        oss << list[i];
    }
    return oss.str();
}

std::vector<std::string> Email::split_recipients(const std::string& str) {
    std::vector<std::string> result;
    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, ';')) {
        // Trim whitespace
        size_t start = token.find_first_not_of(" \t\r\n");
        size_t end = token.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            result.push_back(token.substr(start, end - start + 1));
        } else if (!token.empty()) {
            result.push_back(token);
        }
    }
    return result;
}
