#pragma once

#include <string>

// Refactor note: phone numbers now have one consistent validation and
// normalization API shared by contact models, CSV, and services.
enum class PhoneLabel { Mobile, Home, Work, Other };

struct PhoneNumber {
    int id = 0;
    std::string number;
    PhoneLabel label = PhoneLabel::Mobile;

    std::string normalize() const;
    bool isValid() const;
};

std::string toString(PhoneLabel label);
PhoneLabel phoneLabelFromString(const std::string& value);
