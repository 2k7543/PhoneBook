#include "models/ContactPhoneNumber.h"

#include "utils/Validator.h"

#include <cctype>

// Removes formatting characters while preserving a leading international plus.
std::string PhoneNumber::normalize() const {
    std::string normalized;
    for (char ch : number) {
        if (std::isdigit(static_cast<unsigned char>(ch)) ||
            (ch == '+' && normalized.empty())) {
            normalized.push_back(ch);
        }
    }
    return normalized;
}

// Applies the shared phone-number rule to the normalized value.
bool PhoneNumber::isValid() const {
    return Validator::isValidPhoneNumber(normalize());
}

// Maps phone labels between the model enum and their stored text values.
std::string toString(PhoneLabel label) {
    switch (label) {
    case PhoneLabel::Home: return "home";
    case PhoneLabel::Work: return "work";
    case PhoneLabel::Other: return "other";
    case PhoneLabel::Mobile: return "mobile";
    }
    return "mobile";
}

PhoneLabel phoneLabelFromString(const std::string& value) {
    if (value == "home") return PhoneLabel::Home;
    if (value == "work") return PhoneLabel::Work;
    if (value == "other") return PhoneLabel::Other;
    return PhoneLabel::Mobile;
}
