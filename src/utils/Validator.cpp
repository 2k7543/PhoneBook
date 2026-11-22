#include "utils/Validator.h"

#include <algorithm>
#include <cctype>
#include <regex>

// Centralizes trimming, normalization, and validation rules used by services.
std::string Validator::trim(const std::string& value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    return first < last ? std::string(first, last) : std::string();
}

// Normalizes common input fields and validates email, phone, password, and name rules.
std::string Validator::toLower(const std::string& value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

bool Validator::isValidEmail(const std::string& email) {
    static const std::regex pattern(
        R"(^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$)");
    return std::regex_match(email, pattern);
}

bool Validator::isValidPhoneNumber(const std::string& phone) {
    static const std::regex pattern(R"(^\+?[0-9]{9,15}$)");
    return std::regex_match(phone, pattern);
}

bool Validator::isValidUsername(const std::string& username) {
    static const std::regex pattern(R"(^[A-Za-z0-9][A-Za-z0-9._-]{1,29}$)");
    return std::regex_match(username, pattern);
}

bool Validator::isValidPassword(const std::string& password) {
    return password.size() >= 8 &&
        std::any_of(password.begin(), password.end(), [](unsigned char ch) {
            return std::isalpha(ch) != 0;
        }) &&
        std::any_of(password.begin(), password.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        });
}

bool Validator::isValidFullName(const std::string& fullname) {
    const std::string normalized = trim(fullname);
    return !normalized.empty() && normalized.size() <= 200;
}

bool Validator::isEmpty(const std::string& value) {
    return trim(value).empty();
}
