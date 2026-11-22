#pragma once

#include <string>

// Refactor note: email values are modeled consistently for contact validation
// and CSV import/export.
enum class EmailLabel { Personal, Work, Other };

struct ContactEmail {
    int id = 0;
    std::string email;
    EmailLabel label = EmailLabel::Personal;

    bool isValid() const;
};

std::string toString(EmailLabel label);
EmailLabel emailLabelFromString(const std::string& value);
